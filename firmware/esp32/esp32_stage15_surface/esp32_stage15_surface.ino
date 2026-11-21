// esp32_stage15_surface — a lean control surface: 16 buttons + 1 encoder read
// through THREE daisy-chained 74HC165 shift registers, on 3 GPIOs total.
//
//    8 note buttons    -> one octave of diatonic chords + the octave repeat
//    4 modify buttons  -> chord-quality overrides, held-to-apply
//    4 setting buttons -> Octave- / Octave+ / Patch- / Patch+  (momentary)
//    1 encoder         -> Volume (turn); push = mute / unmute
//
// Chords are MONO: a new note press cuts the previous chord — a 9TH voicing plus
// bass is already 6 notes, exactly the board's stable ceiling.
//
// Board:   ESP32 Dev Module (classic ESP-WROOM-32)
// Library: AMY (~/Documents/Arduino/libraries/AMY)
//
// ── Wiring: 3x 74HC165, all sharing LATCH + CLOCK ────────────────────────────
//   Every chip: pin 16 -> 3.3V (NOT 5V), pin 8 -> GND, pin 15 (CLK INH) -> GND,
//               pin 1 (SH/LD) -> GPIO18, pin 2 (CLK) -> GPIO19.
//   Chain:  A pin 9 (Q7) -> GPIO23        <- the ESP32 reads chip A
//           B pin 9 -> A pin 10 (SER)
//           C pin 9 -> B pin 10 (SER)
//           C pin 10 (SER) -> GND         <- end of chain
//   Every USED input: 10k pull-up to 3.3V. Every SPARE input: tie to 3.3V.
//   1 electrolytic across the + / - rails for bulk decoupling.
//
//   Bit index = chip*8 + Dn.  D0-D3 are pins 11,12,13,14; D4-D7 are pins 3,4,5,6.
//     Chip A  bits  0-7   Enc A/B/SW (D0-D2), Notes 1-5 (D3-D7)
//     Chip B  bits  8-15  Notes 6-8 (D0-D2), Modify 1-4 (D3-D6), Setting 1 (D7)
//     Chip C  bits 16-23  Settings 2-4 (D0-D2)   D3-D7 spare -> tie to 3.3V
//
//   Encoder: 3-pin side is A / C / B with C in the MIDDLE -> C to GND.
//            2-pin side is the switch -> one leg to a D input, other to GND.
//
// Prove the wiring with esp32_shiftreg_test (set NUM_CHIPS 3) BEFORE flashing this.

#include <AMY-Arduino.h>
extern "C" {
  #include "chords.h"
}

// ── Audio wiring (unchanged) ─────────────────────────────────────────────────
#define PIN_LRC   25
#define PIN_BCLK  26
#define PIN_DOUT  27

// ── Shift-register wiring ────────────────────────────────────────────────────
#define PIN_LATCH  18
#define PIN_CLOCK  19
#define PIN_DATA   23
#define NUM_CHIPS   3

// ── Input bit map (edit to match how you actually wire) ──────────────────────
#define ENC_A_BIT   0
#define ENC_B_BIT   1
#define ENC_SW_BIT  2

#define NUM_NOTES   8
const int NOTE_BIT[NUM_NOTES] = { 3, 4, 5, 6, 7,  8, 9, 10 };

#define NUM_MOD     4
const int MOD_BIT[NUM_MOD]    = { 11, 12, 13, 14 };
// Which OVR_LABELS override each modify button applies (0..6). Change freely:
//   0 +-3rd  1 dom7  2 dim  3 dom9  4 sus4  5 maj6  6 aug
const int MOD_OVR[NUM_MOD]     = { 0, 1, 4, 5 };

#define NUM_SET     4
const int SET_BIT[NUM_SET]    = { 15, 16, 17, 18 };
// Setting button actions, in order. See apply_setting().
enum { SET_OCT_DN, SET_OCT_UP, SET_PATCH_DN, SET_PATCH_UP };

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
static float muted_vol     = 0.0f;  // remembers level while muted
static int   is_muted      = 0;
static int   octave_shift  = 0;     // -2..+2
static int   voice_level   = 2;     // 0=SINGLE 1=TRIAD 2=7TH 3=9TH
static int   bass_on       = 1;
static int   current_ovr   = -1;    // -1 diatonic, else 0..6
#define MAX_VOICE_LEVEL 3           // bass + 9TH = 6 notes = the ceiling

// ── Debounced input state ────────────────────────────────────────────────────
static uint8_t  enc_prev;
static int8_t   enc_accum;
static int      enc_sw_state; static uint32_t enc_sw_ms;
static int      note_state[NUM_NOTES]; static uint32_t note_ms[NUM_NOTES];
static int      mod_state [NUM_MOD];   static uint32_t mod_ms [NUM_MOD];
static int      set_state [NUM_SET];   static uint32_t set_ms [NUM_SET];

// ── Sounding chord (mono) ────────────────────────────────────────────────────
static int sounding   = -1;
static int held_notes[MAX_CHORD_NOTES];
static int held_count = 0;

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

// 8 note buttons: degrees I..vii of the current octave, then I of the next.
static void chord_start(int b) {
  chord_stop();
  int degree = b % 7;
  int oct    = b / 7;
  int root   = PIANO_BASE + current_key + MAJOR_SCALE[degree]
             + (octave_shift + oct) * 12;

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
  Serial.printf("  %-4s %-10s oct%+d  %s\n", DEGREE_ROMAN[degree], name,
                octave_shift + oct, current_ovr >= 0 ? OVR_LABELS[current_ovr] : "");
}

// Re-voice whatever is sounding — used when a modifier changes mid-hold.
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
  Serial.printf("patch=%3d  key=%-2s  vol=%.1f%s  oct=%+d  voicing=%s\n",
                current_patch, KEY_NAMES[current_key], current_vol,
                is_muted ? " (MUTED)" : "", octave_shift, VOICE_LABELS[voice_level]);
}

// ── Shift-register read ──────────────────────────────────────────────────────
static uint8_t snap[NUM_CHIPS];

static void sr_read(uint8_t *buf) {
  digitalWrite(PIN_LATCH, LOW);
  delayMicroseconds(5);
  digitalWrite(PIN_LATCH, HIGH);
  for (int chip = 0; chip < NUM_CHIPS; chip++) {
    uint8_t b = 0;
    for (int i = 0; i < 8; i++) {
      if (digitalRead(PIN_DATA)) b |= (uint8_t)(1u << (7 - i));
      digitalWrite(PIN_CLOCK, HIGH);
      digitalWrite(PIN_CLOCK, LOW);
    }
    buf[chip] = b;
  }
}
static inline int bit_of(const uint8_t *buf, int idx) {
  return (buf[idx >> 3] >> (idx & 7)) & 1;
}
static inline int pressed(const uint8_t *buf, int idx) { return bit_of(buf, idx) == 0; }

// ── Encoder decode ───────────────────────────────────────────────────────────
static const int8_t ENC_TABLE[16] = {
   0, -1,  1,  0,   1,  0,  0, -1,  -1,  0,  0,  1,   0,  1, -1,  0
};
static int enc_step(const uint8_t *buf) {
  uint8_t s   = (uint8_t)((bit_of(buf, ENC_A_BIT) << 1) | bit_of(buf, ENC_B_BIT));
  uint8_t idx = (uint8_t)(((enc_prev << 2) | s) & 0x0F);
  int8_t  d   = ENC_TABLE[idx];
  enc_prev    = s;
  if (d) {
    enc_accum += d;
    if (enc_accum >=  4) { enc_accum = 0; return  1; }
    if (enc_accum <= -4) { enc_accum = 0; return -1; }
  }
  return 0;
}

// ── Setting buttons ──────────────────────────────────────────────────────────
static void apply_setting(int s) {
  switch (s) {
    case SET_OCT_DN: if (octave_shift > -2) { octave_shift--; chord_revoice(); } break;
    case SET_OCT_UP: if (octave_shift <  2) { octave_shift++; chord_revoice(); } break;
    case SET_PATCH_DN: if (current_patch >   0) { current_patch--; load_patch(current_patch); } break;
    case SET_PATCH_UP: if (current_patch < 255) { current_patch++; load_patch(current_patch); } break;
  }
}

// ── Setup ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nStage 15: 16 buttons + 1 encoder via 3x 74HC165");

  pinMode(PIN_LATCH, OUTPUT);
  pinMode(PIN_CLOCK, OUTPUT);
  pinMode(PIN_DATA,  INPUT);
  digitalWrite(PIN_LATCH, HIGH);
  digitalWrite(PIN_CLOCK, LOW);

  sr_read(snap);
  enc_prev  = (uint8_t)((bit_of(snap, ENC_A_BIT) << 1) | bit_of(snap, ENC_B_BIT));
  enc_accum = 0; enc_sw_state = 0; enc_sw_ms = 0;
  for (int i = 0; i < NUM_NOTES; i++) { note_state[i] = 0; note_ms[i] = 0; }
  for (int i = 0; i < NUM_MOD;   i++) { mod_state[i]  = 0; mod_ms[i]  = 0; }
  for (int i = 0; i < NUM_SET;   i++) { set_state[i]  = 0; set_ms[i]  = 0; }

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
  sr_read(snap);
  int changed = 0;

  // Volume encoder — turn adjusts level, push toggles mute.
  int step = enc_step(snap);
  if (step && !is_muted) {
    float v = current_vol + 0.5f * step;
    if (v < 0.0f) v = 0.0f; if (v > 10.0f) v = 10.0f;
    current_vol = v; set_volume(current_vol); changed = 1;
  }
  int esw = pressed(snap, ENC_SW_BIT);
  if (esw != enc_sw_state && now - enc_sw_ms >= DEBOUNCE_MS) {
    enc_sw_state = esw; enc_sw_ms = now;
    if (esw) {                                 // act on press
      is_muted = !is_muted;
      if (is_muted) { muted_vol = current_vol; set_volume(0.0f); }
      else          { set_volume(current_vol); }
      changed = 1;
    }
  }

  // Setting buttons — momentary, one step per press.
  for (int i = 0; i < NUM_SET; i++) {
    int raw = pressed(snap, SET_BIT[i]);
    if (raw != set_state[i] && now - set_ms[i] >= DEBOUNCE_MS) {
      set_state[i] = raw; set_ms[i] = now;
      if (raw) { apply_setting(i); changed = 1; }
    }
  }

  // Modifier buttons — held-to-apply, lowest-numbered held button wins.
  int want_ovr = -1;
  for (int i = 0; i < NUM_MOD; i++) {
    int raw = pressed(snap, MOD_BIT[i]);
    if (raw != mod_state[i] && now - mod_ms[i] >= DEBOUNCE_MS) {
      mod_state[i] = raw; mod_ms[i] = now;
    }
    if (mod_state[i] && want_ovr < 0) want_ovr = MOD_OVR[i];
  }
  if (want_ovr != current_ovr) { current_ovr = want_ovr; chord_revoice(); }

  if (changed) print_state();

  // Note buttons — mono: a new press cuts whatever was sounding.
  for (int i = 0; i < NUM_NOTES; i++) {
    int raw = pressed(snap, NOTE_BIT[i]);
    if (raw == note_state[i]) continue;
    if (now - note_ms[i] < DEBOUNCE_MS) continue;
    note_state[i] = raw;
    note_ms[i]    = now;
    if (raw)                chord_start(i);
    else if (sounding == i) chord_stop();
  }
}
