// esp32_stage13_shiftenc — Stage 13: FIVE encoders read through 74HC165s.
//
// Builds on Stage 4 (one encoder + 4 chord buttons + AMY) but frees the encoder
// GPIOs and reads 5 EC11 push-encoders through TWO daisy-chained 74HC165 shift
// registers, using only 3 pins for the whole chain. This is the input-expansion
// step toward the hardware product's 5-encoder control surface.
//
//   Enc0 TURN=PATCH   PUSH=(reset patch to 18)
//   Enc1 TURN=KEY     PUSH=(reset to C)
//   Enc2 TURN=VOLUME  PUSH=(reset to 3.0)
//   Enc3 TURN=OCTAVE  PUSH=(reset to 0)
//   Enc4 TURN=VOICES  PUSH=(reset to 3)
//   4 chord buttons (GPIO13/14/32/33) still play I / IV / V / vi.
//
// Board:   ESP32 Dev Module (classic ESP-WROOM-32)
// Library: AMY (~/Documents/Arduino/libraries/AMY)
//
// ── Wiring: 2x 74HC165 (both share LATCH + CLOCK) ────────────────────────────
//   pin 16 VCC -> 3.3V     pin 8 GND -> GND     pin 15 CLK INH -> GND
//   pin 1  SH/LD -> GPIO18 (LATCH)   pin 2 CLK -> GPIO19 (CLOCK)   both chips
//   chip A pin 9 (Q7) -> GPIO23 (DATA)
//   chip A pin 10 (SER) -> chip B pin 9 (Q7)      chip B pin 10 (SER) -> GND
//   Every used D input: 10k pull-up to 3.3V, then encoder pin -> D input.
//   (74HC165 has NO internal pull-ups. Encoder common pin C -> GND, SW -> GND.)
//
//   Default bit map (edit the arrays below to match how you actually wire):
//     Enc0 A=chipA D0  B=D1  SW=D2
//     Enc1 A=chipA D3  B=D4  SW=D5
//     Enc2 A=chipA D6  B=D7  SW=chipB D0   (straddles both chips; code doesn't care)
//     Enc3 A=chipB D1  B=D2  SW=D3
//     Enc4 A=chipB D4  B=D5  SW=D6
//   Bit index = chip*8 + Dn  ->  chip A = 0..7, chip B = 8..15.
//
// Prove the wiring with esp32_shiftreg_test FIRST.

#include <AMY-Arduino.h>
extern "C" {
  #include "chords.h"
}

// ── Audio wiring (unchanged) ─────────────────────────────────────────────────
#define PIN_LRC   25
#define PIN_BCLK  26
#define PIN_DOUT  27

// ── Shift-register wiring ────────────────────────────────────────────────────
#define PIN_LATCH  18   // SH/LD
#define PIN_CLOCK  19   // CLK
#define PIN_DATA   23   // Q7 of chip A
#define NUM_CHIPS   2

// ── Encoder bit map (index = chip*8 + Dn) ────────────────────────────────────
#define NUM_ENC     5
const int ENC_A [NUM_ENC] = { 0, 3, 6,  9, 12 };
const int ENC_B [NUM_ENC] = { 1, 4, 7, 10, 13 };
const int ENC_SW[NUM_ENC] = { 2, 5, 8, 11, 14 };

// ── Engine budget (6/32 is what this board proved stable) ────────────────────
#define MAX_VOICES      6
#define MAX_OSCS       32

// ── Live musical state ───────────────────────────────────────────────────────
static int   current_patch = 18;    // 18=Xylophone
static int   current_key   = 0;     // 0=C .. 11=B
static float current_vol   = 3.0f;
static int   current_oct   = 0;     // octave shift, -2..+2
static int   synth_voices  = 3;     // SYNTH_VOICES*~6oscs must stay under MAX_OSCS
#define VELOCITY     0.8f
#define VOICE_LEVEL  2
#define BASS_ON      1
#define PIANO_BASE  48

// ── Chord buttons (unchanged direct GPIO) ────────────────────────────────────
#define NUM_BUTTONS     4
#define DEBOUNCE_MS    25
const int BTN_PIN[NUM_BUTTONS]    = { 13, 14, 32, 33 };
const int BTN_DEGREE[NUM_BUTTONS] = {  0,  3,  4,  5 };   // I, IV, V, vi
static int      btn_pressed[NUM_BUTTONS];
static uint32_t btn_changed_ms[NUM_BUTTONS];
static int      held_notes[NUM_BUTTONS][MAX_CHORD_NOTES];
static int      held_count[NUM_BUTTONS];

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
static void chord_off(int b) {
  for (int i = 0; i < held_count[b]; i++) note_off(held_notes[b][i]);
  held_count[b] = 0;
}
static void release_all_chords(void) {
  for (int b = 0; b < NUM_BUTTONS; b++) chord_off(b);
}
static void chord_on(int b) {
  int degree    = BTN_DEGREE[b];
  int root_midi = PIANO_BASE + current_key + MAJOR_SCALE[degree] + current_oct * 12;
  int notes[MAX_CHORD_NOTES];
  int n = build_chord_notes(degree, root_midi, VOICE_LEVEL, BASS_ON, -1, notes);
  for (int i = 0; i < n; i++) {
    if (notes[i] < 0 || notes[i] > 127) continue;
    note_on(notes[i], VELOCITY);
    held_notes[b][i] = notes[i];
  }
  held_count[b] = n;
  char name[24];
  make_chord_name(current_key, degree, VOICE_LEVEL, -1, name, sizeof(name));
  Serial.printf("  %-4s %s\n", DEGREE_ROMAN[degree], name);
}

// Reload the patch on synth 0. Kill sounding notes first (stale voices hang).
static void load_patch(int p) {
  release_all_chords();
  amy_event e = amy_default_event();
  e.synth = 0; e.num_voices = synth_voices; e.patch_number = (uint16_t)p;
  amy_add_event(&e);
}

static void print_state(void) {
  Serial.printf("patch=%3d  key=%-2s  vol=%.1f  oct=%+d  voices=%d\n",
                current_patch, KEY_NAMES[current_key], current_vol,
                current_oct, synth_voices);
}

// ── Shift-register read ──────────────────────────────────────────────────────
static uint16_t sr_read(void) {
  digitalWrite(PIN_LATCH, LOW);
  delayMicroseconds(5);
  digitalWrite(PIN_LATCH, HIGH);
  uint16_t v = 0;
  for (int chip = 0; chip < NUM_CHIPS; chip++) {
    for (int i = 0; i < 8; i++) {
      int dn = 7 - i;
      if (digitalRead(PIN_DATA)) v |= (1u << (chip * 8 + dn));
      digitalWrite(PIN_CLOCK, HIGH);
      digitalWrite(PIN_CLOCK, LOW);
    }
  }
  return v;
}
static inline int bit_of(uint16_t v, int idx) { return (v >> idx) & 1; }

// ── Encoder decode: one quadrature state machine per encoder ─────────────────
// Same table as Stage 4, but the A/B levels come from the shift-register word
// instead of digitalRead(). EC11s emit 4 transitions per detent -> step per 4.
static const int8_t ENC_TABLE[16] = {
   0, -1,  1,  0,
   1,  0,  0, -1,
  -1,  0,  0,  1,
   0,  1, -1,  0
};
static uint8_t  enc_prev [NUM_ENC];
static int8_t   enc_accum[NUM_ENC];
static int      sw_pressed[NUM_ENC];
static uint32_t sw_changed_ms[NUM_ENC];

// Returns -1 / 0 / +1 detent step for encoder e given the current snapshot.
static int enc_step(int e, uint16_t v) {
  uint8_t s   = (bit_of(v, ENC_A[e]) << 1) | bit_of(v, ENC_B[e]);
  uint8_t idx = ((enc_prev[e] << 2) | s) & 0x0F;
  int8_t  d   = ENC_TABLE[idx];
  enc_prev[e] = s;
  if (d) {
    enc_accum[e] += d;
    if (enc_accum[e] >=  4) { enc_accum[e] = 0; return  1; }
    if (enc_accum[e] <= -4) { enc_accum[e] = 0; return -1; }
  }
  return 0;
}

// Apply a turn to whichever parameter encoder e controls.
static void apply_turn(int e, int step) {
  switch (e) {
    case 0: {
      int p = current_patch + step;
      if (p < 0) p = 0; if (p > 255) p = 255;
      if (p != current_patch) { current_patch = p; load_patch(current_patch); }
      break;
    }
    case 1: current_key = (current_key + step + 12) % 12; break;
    case 2: {
      float nv = current_vol + 0.5f * step;
      if (nv < 0.0f) nv = 0.0f; if (nv > 10.0f) nv = 10.0f;  // AMY vol is 0..10
      current_vol = nv; set_volume(current_vol);
      break;
    }
    case 3: {
      int o = current_oct + step;
      if (o < -2) o = -2; if (o > 2) o = 2;
      current_oct = o;
      break;
    }
    case 4: {
      int nvc = synth_voices + step;
      if (nvc < 1) nvc = 1; if (nvc > 5) nvc = 5;  // *~6 oscs must fit MAX_OSCS
      if (nvc != synth_voices) { synth_voices = nvc; load_patch(current_patch); }
      break;
    }
  }
}

// PUSH resets that encoder's parameter to a sensible default.
static void apply_push(int e) {
  switch (e) {
    case 0: current_patch = 18; load_patch(current_patch); break;
    case 1: current_key   = 0;  break;
    case 2: current_vol   = 3.0f; set_volume(current_vol); break;
    case 3: current_oct   = 0;  break;
    case 4: synth_voices  = 3;  load_patch(current_patch); break;
  }
}

// ── Setup ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nStage 13: 5 encoders via 2x 74HC165");

  pinMode(PIN_LATCH, OUTPUT);
  pinMode(PIN_CLOCK, OUTPUT);
  pinMode(PIN_DATA,  INPUT);
  digitalWrite(PIN_LATCH, HIGH);
  digitalWrite(PIN_CLOCK, LOW);

  for (int b = 0; b < NUM_BUTTONS; b++) {
    pinMode(BTN_PIN[b], INPUT_PULLUP);
    btn_pressed[b] = 0; btn_changed_ms[b] = 0; held_count[b] = 0;
  }

  // Seed each encoder's quadrature state from the resting snapshot.
  uint16_t v = sr_read();
  for (int e = 0; e < NUM_ENC; e++) {
    enc_prev[e]  = (bit_of(v, ENC_A[e]) << 1) | bit_of(v, ENC_B[e]);
    enc_accum[e] = 0;
    sw_pressed[e] = 0; sw_changed_ms[e] = 0;
  }

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
  uint16_t v = sr_read();   // one snapshot drives every encoder this pass

  int changed = 0;
  for (int e = 0; e < NUM_ENC; e++) {
    int step = enc_step(e, v);
    if (step) { apply_turn(e, step); changed = 1; }

    int sw = (bit_of(v, ENC_SW[e]) == 0);   // pulled up -> 0 when pressed
    if (sw != sw_pressed[e] && now - sw_changed_ms[e] >= DEBOUNCE_MS) {
      sw_pressed[e]    = sw;
      sw_changed_ms[e] = now;
      if (sw) { apply_push(e); changed = 1; }   // act on press
    }
  }
  if (changed) print_state();

  // Chord buttons
  for (int b = 0; b < NUM_BUTTONS; b++) {
    int raw = (digitalRead(BTN_PIN[b]) == LOW);
    if (raw == btn_pressed[b]) continue;
    if (now - btn_changed_ms[b] < DEBOUNCE_MS) continue;
    btn_pressed[b]    = raw;
    btn_changed_ms[b] = now;
    if (raw) chord_on(b);
    else     chord_off(b);
  }
}
