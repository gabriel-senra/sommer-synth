// esp32_stage14_surface — the full control surface: 4 encoders + 21 buttons
// read through FIVE daisy-chained 74HC165 shift registers, on 3 GPIOs total.
//
//   14 note buttons  -> two octaves of diatonic chords (degree = i % 7)
//    7 modify buttons -> the OVR_LABELS chord-quality overrides, held-to-apply
//    4 encoders       -> Patch · Key · Volume · Voicing
//
// Chords are MONO: pressing a new note button cuts the previous chord. With a
// 6-voice budget that is a necessity, not a stylistic choice — a 9TH voicing
// plus bass is already 6 notes.
//
// Board:   ESP32 Dev Module (classic ESP-WROOM-32)
// Library: AMY (~/Documents/Arduino/libraries/AMY)
//
// ── Wiring: 5x 74HC165, all sharing LATCH + CLOCK ────────────────────────────
//   Every chip: pin 16 -> 3.3V (NOT 5V), pin 8 -> GND, pin 15 (CLK INH) -> GND,
//               pin 1 (SH/LD) -> GPIO18, pin 2 (CLK) -> GPIO19,
//               100nF cap across pins 16 and 8.
//   Chain:  A pin 9 (Q7) -> GPIO23        <- the ESP32 reads chip A
//           B pin 9 -> A pin 10 (SER)
//           C pin 9 -> B pin 10
//           D pin 9 -> C pin 10
//           E pin 9 -> D pin 10
//           E pin 10 (SER) -> GND         <- end of chain
//   Every USED input: 10k pull-up to 3.3V (the '165 has none internally).
//   Every SPARE input: tie straight to 3.3V — never leave a CMOS input floating.
//
//   Bit index = chip*8 + Dn.  D0-D3 are pins 11,12,13,14; D4-D7 are pins 3,4,5,6.
//     Chip A  bits  0-7   Enc0 (D0-D2)  Enc1 (D3-D5)   D6,D7 spare
//     Chip B  bits  8-15  Enc2 (D0-D2)  Enc3 (D3-D5)   D6,D7 spare
//     Chip C  bits 16-23  Note buttons 1-8   (D0-D7)
//     Chip D  bits 24-31  Note buttons 9-14  (D0-D5)   D6,D7 spare
//     Chip E  bits 32-39  Modify buttons 1-7 (D0-D6)   D7 spare
//
//   Encoder: 3-pin side is A / C / B with C in the MIDDLE -> C to GND.
//            2-pin side is the switch -> one leg to a D input, other to GND.
//
// Prove the wiring with esp32_shiftreg_test (set NUM_CHIPS 5) BEFORE flashing this.

#include <AMY-Arduino.h>
extern "C" {
  #include "chords.h"
}

// ── Audio wiring (unchanged) ─────────────────────────────────────────────────
#define PIN_LRC   25
#define PIN_BCLK  26
#define PIN_DOUT  27

// ── Shift-register wiring ────────────────────────────────────────────────────
#define PIN_LATCH  18   // SH/LD, all chips
#define PIN_CLOCK  19   // CLK,   all chips
#define PIN_DATA   23   // Q7 of chip A
#define NUM_CHIPS   5
#define NUM_BITS   (NUM_CHIPS * 8)

// ── Input bit map (edit to match how you actually wire) ──────────────────────
#define NUM_ENC     4
const int ENC_A [NUM_ENC] = { 0, 3,  8, 11 };
const int ENC_B [NUM_ENC] = { 1, 4,  9, 12 };
const int ENC_SW[NUM_ENC] = { 2, 5, 10, 13 };

#define NUM_NOTES  14
const int NOTE_BIT[NUM_NOTES] = { 16,17,18,19,20,21,22,23,  24,25,26,27,28,29 };

#define NUM_MOD     7
const int MOD_BIT[NUM_MOD]    = { 32,33,34,35,36,37,38 };

// ── Engine budget (6/32 is what this board proved stable) ────────────────────
#define MAX_VOICES      6
#define MAX_OSCS       32
#define SYNTH_VOICES    3
#define DEBOUNCE_MS    25
#define VELOCITY     0.8f
#define PIANO_BASE     48

// ── Live state ───────────────────────────────────────────────────────────────
static int   current_patch = 18;    // 18=Xylophone
static int   current_key   = 0;     // 0=C .. 11=B
static float current_vol   = 3.0f;  // AMY master volume is 0..10, not 0..1
static int   voice_level   = 2;     // 0=SINGLE 1=TRIAD 2=7TH 3=9TH — capped at 9TH
static int   bass_on       = 1;
static int   current_ovr   = -1;    // -1 = diatonic, else 0..6 into OVR_LABELS

// bass + (voice_level+2) notes must fit MAX_VOICES, so 9TH is the ceiling.
#define MAX_VOICE_LEVEL 3

// ── Debounced input state ────────────────────────────────────────────────────
static uint8_t  enc_prev [NUM_ENC];
static int8_t   enc_accum[NUM_ENC];
static int      sw_state [NUM_ENC];
static uint32_t sw_ms    [NUM_ENC];
static int      note_state[NUM_NOTES];
static uint32_t note_ms   [NUM_NOTES];
static int      mod_state [NUM_MOD];
static uint32_t mod_ms    [NUM_MOD];

// ── Sounding chord (mono) ────────────────────────────────────────────────────
static int sounding    = -1;                    // which note button is held
static int held_notes[MAX_CHORD_NOTES];
static int held_count  = 0;

// ── AMY helpers ──────────────────────────────────────────────────────────────
static void note_on(int n, float v) {
  amy_event e = amy_default_event();
  e.synth = 0; e.midi_note = (float)n; e.velocity = v;
  amy_add_event(&e);
}
static void note_off(int n) {
  amy_event e = amy_default_event();
  e.synth = 0; e.midi_note = (float)n; e.velocity = 0.0f;
  amy_add_event(&e);
}
static void set_volume(float v) {
  amy_event e = amy_default_event();
  e.volume = v;
  amy_add_event(&e);
}

// ── Chords ───────────────────────────────────────────────────────────────────
static void chord_stop(void) {
  for (int i = 0; i < held_count; i++) note_off(held_notes[i]);
  held_count = 0;
  sounding   = -1;
}

// Note button b: 14 buttons = two octaves of the 7 diatonic degrees.
// Always releases the sounding chord first — this is what makes the keybed mono.
static void chord_start(int b) {
  chord_stop();

  int degree = b % 7;
  int oct    = b / 7;
  int root   = PIANO_BASE + current_key + MAJOR_SCALE[degree] + oct * 12;

  int notes[MAX_CHORD_NOTES];
  int n = build_chord_notes(degree, root, voice_level, bass_on, current_ovr, notes);
  held_count = 0;
  for (int i = 0; i < n; i++) {
    if (notes[i] < 0 || notes[i] > 127) continue;
    note_on(notes[i], VELOCITY);
    held_notes[held_count++] = notes[i];
  }
  sounding = b;

  char name[24];
  make_chord_name(current_key, degree, voice_level, current_ovr, name, sizeof(name));
  Serial.printf("  %-4s %-10s oct%d  %s\n", DEGREE_ROMAN[degree], name, oct,
                current_ovr >= 0 ? OVR_LABELS[current_ovr] : "");
}

// Re-voice whatever is sounding — used when a modifier or voicing changes mid-hold.
static void chord_revoice(void) {
  if (sounding < 0) return;
  int b = sounding;
  chord_stop();
  chord_start(b);
}

static void load_patch(int p) {
  chord_stop();
  amy_event e = amy_default_event();
  e.synth = 0; e.num_voices = SYNTH_VOICES; e.patch_number = (uint16_t)p;
  amy_add_event(&e);
}

static void print_state(void) {
  Serial.printf("patch=%3d  key=%-2s  vol=%.1f  voicing=%-6s  bass=%s\n",
                current_patch, KEY_NAMES[current_key], current_vol,
                VOICE_LABELS[voice_level], bass_on ? "on" : "off");
}

// ── Shift-register read ──────────────────────────────────────────────────────
static uint8_t snap[NUM_CHIPS];

static void sr_read(uint8_t *buf) {
  digitalWrite(PIN_LATCH, LOW);        // snapshot every input in parallel
  delayMicroseconds(5);
  digitalWrite(PIN_LATCH, HIGH);       // shift mode; Q7 holds D7 of chip A
  for (int chip = 0; chip < NUM_CHIPS; chip++) {
    uint8_t b = 0;
    for (int i = 0; i < 8; i++) {
      if (digitalRead(PIN_DATA)) b |= (uint8_t)(1u << (7 - i));  // D7 first
      digitalWrite(PIN_CLOCK, HIGH);   // rising edge shifts the next bit out
      digitalWrite(PIN_CLOCK, LOW);
    }
    buf[chip] = b;
  }
}
static inline int bit_of(const uint8_t *buf, int idx) {
  return (buf[idx >> 3] >> (idx & 7)) & 1;
}
// Pulled up, switch to GND -> a pressed input reads 0.
static inline int pressed(const uint8_t *buf, int idx) { return bit_of(buf, idx) == 0; }

// ── Encoder decode: one quadrature state machine per encoder ─────────────────
static const int8_t ENC_TABLE[16] = {
   0, -1,  1,  0,
   1,  0,  0, -1,
  -1,  0,  0,  1,
   0,  1, -1,  0
};
// EC11s emit 4 transitions per detent, so accumulate and report one step per 4.
static int enc_step(int e, const uint8_t *buf) {
  uint8_t s   = (uint8_t)((bit_of(buf, ENC_A[e]) << 1) | bit_of(buf, ENC_B[e]));
  uint8_t idx = (uint8_t)(((enc_prev[e] << 2) | s) & 0x0F);
  int8_t  d   = ENC_TABLE[idx];
  enc_prev[e] = s;
  if (d) {
    enc_accum[e] += d;
    if (enc_accum[e] >=  4) { enc_accum[e] = 0; return  1; }
    if (enc_accum[e] <= -4) { enc_accum[e] = 0; return -1; }
  }
  return 0;
}

static void apply_turn(int e, int step) {
  switch (e) {
    case 0: {                                   // PATCH
      int p = current_patch + step;
      if (p < 0) p = 0; if (p > 255) p = 255;
      if (p != current_patch) { current_patch = p; load_patch(current_patch); }
      break;
    }
    case 1:                                     // KEY (wraps)
      current_key = (current_key + step + 12) % 12;
      chord_revoice();
      break;
    case 2: {                                   // VOLUME
      float v = current_vol + 0.5f * step;
      if (v < 0.0f) v = 0.0f; if (v > 10.0f) v = 10.0f;
      current_vol = v; set_volume(current_vol);
      break;
    }
    case 3: {                                   // VOICING
      int vl = voice_level + step;
      if (vl < 0) vl = 0; if (vl > MAX_VOICE_LEVEL) vl = MAX_VOICE_LEVEL;
      if (vl != voice_level) { voice_level = vl; chord_revoice(); }
      break;
    }
  }
}

static void apply_push(int e) {
  switch (e) {
    case 0: current_patch = 18;  load_patch(current_patch); break;
    case 1: current_key   = 0;   chord_revoice();           break;
    case 2: current_vol   = 3.0f; set_volume(current_vol);  break;
    case 3: bass_on = !bass_on;  chord_revoice();           break;
  }
}

// ── Setup ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nStage 14: 4 encoders + 21 buttons via 5x 74HC165");

  pinMode(PIN_LATCH, OUTPUT);
  pinMode(PIN_CLOCK, OUTPUT);
  pinMode(PIN_DATA,  INPUT);
  digitalWrite(PIN_LATCH, HIGH);
  digitalWrite(PIN_CLOCK, LOW);

  sr_read(snap);
  for (int e = 0; e < NUM_ENC; e++) {
    enc_prev[e]  = (uint8_t)((bit_of(snap, ENC_A[e]) << 1) | bit_of(snap, ENC_B[e]));
    enc_accum[e] = 0; sw_state[e] = 0; sw_ms[e] = 0;
  }
  for (int i = 0; i < NUM_NOTES; i++) { note_state[i] = 0; note_ms[i] = 0; }
  for (int i = 0; i < NUM_MOD;   i++) { mod_state[i]  = 0; mod_ms[i]  = 0; }

  Serial.printf("free heap BEFORE amy_start: %u\n", (unsigned)ESP.getFreeHeap());
  amy_config_t c = amy_default_config();
  c.audio    = AMY_AUDIO_IS_I2S;
  c.i2s_lrc  = PIN_LRC;
  c.i2s_bclk = PIN_BCLK;
  c.i2s_dout = PIN_DOUT;
  c.i2s_mclk = -1;
  c.features.default_synths = 0;
  c.features.startup_bleep  = 1;
  c.max_voices = MAX_VOICES;
  c.max_oscs   = MAX_OSCS;
  amy_start(c);
  Serial.printf("free heap AFTER amy_start:  %u\n", (unsigned)ESP.getFreeHeap());

  set_volume(current_vol);
  load_patch(current_patch);
  print_state();
}

// ── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
  uint32_t now = millis();
  sr_read(snap);                 // one snapshot drives every control this pass

  // Encoders
  int changed = 0;
  for (int e = 0; e < NUM_ENC; e++) {
    int step = enc_step(e, snap);
    if (step) { apply_turn(e, step); changed = 1; }

    int sw = pressed(snap, ENC_SW[e]);
    if (sw != sw_state[e] && now - sw_ms[e] >= DEBOUNCE_MS) {
      sw_state[e] = sw; sw_ms[e] = now;
      if (sw) { apply_push(e); changed = 1; }   // act on press, not release
    }
  }
  if (changed) print_state();

  // Modifier buttons — held-to-apply. Lowest-numbered held button wins, so
  // mashing two never leaves the chord in an ambiguous state.
  int want_ovr = -1;
  for (int i = 0; i < NUM_MOD; i++) {
    int raw = pressed(snap, MOD_BIT[i]);
    if (raw != mod_state[i] && now - mod_ms[i] >= DEBOUNCE_MS) {
      mod_state[i] = raw; mod_ms[i] = now;
    }
    if (mod_state[i] && want_ovr < 0) want_ovr = i;
  }
  if (want_ovr != current_ovr) {
    current_ovr = want_ovr;
    chord_revoice();             // re-voice mid-hold so modifiers feel live
  }

  // Note buttons — mono: a new press cuts whatever was sounding.
  for (int i = 0; i < NUM_NOTES; i++) {
    int raw = pressed(snap, NOTE_BIT[i]);
    if (raw == note_state[i]) continue;
    if (now - note_ms[i] < DEBOUNCE_MS) continue;
    note_state[i] = raw;
    note_ms[i]    = now;
    if (raw)                 chord_start(i);   // cuts the old chord for us
    else if (sounding == i)  chord_stop();     // only release if still ours
  }
}
