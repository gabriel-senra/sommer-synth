// esp32_stage4_encoder — Stage 4: an encoder to change KEY and PATCH live.
//
// Builds on Stage 3 (4 buttons = I / IV / V / vi via chords.c + AMY) and adds
// one EC11 push-encoder:
//    TURN  -> change the current value
//    PUSH  -> switch which value you're editing (KEY <-> PATCH)
// This is the first piece of the paged-encoder UI planned for the hardware.
//
// Board:   ESP32 Dev Module (classic ESP-WROOM-32)
// Library: AMY (~/Documents/Arduino/libraries/AMY)
//
// ── Wiring ───────────────────────────────────────────────────────────────────
// MAX98357 amp (unchanged):  LRC->25  BCLK->26  DIN->27  Vin->5V  GND->GND
// Chord buttons (unchanged): GPIO13, 14, 32, 33  -> other leg to GND
//
// EC11 encoder (NEW) — it has 3 pins on one side, 2 on the other:
//   3-pin side:  A     -> GPIO18
//                C     -> GND        (the MIDDLE pin is common)
//                B     -> GPIO19
//   2-pin side:  SW    -> GPIO23     (either pin)
//                SW    -> GND        (the other pin)
// No resistors needed — we use the ESP32's internal pull-ups.

#include <AMY-Arduino.h>
extern "C" {
  #include "chords.h"
}

// ── Audio wiring ─────────────────────────────────────────────────────────────
#define PIN_LRC   25
#define PIN_BCLK  26
#define PIN_DOUT  27

// ── Encoder wiring ───────────────────────────────────────────────────────────
#define PIN_ENC_A  18
#define PIN_ENC_B  19
#define PIN_ENC_SW 23

// ── Engine budget (6/32 is what this board proved stable) ────────────────────
#define MAX_VOICES      6
#define MAX_OSCS       32
// Patches use ~6 oscs per voice, so SYNTH_VOICES * 6 must stay under MAX_OSCS.
// 3 voices leaves headroom for heavier patches you might scroll onto.
#define SYNTH_VOICES    3
#define VOLUME       3.0f
#define VELOCITY     0.8f

// ── Musical state (now LIVE, changed by the encoder) ─────────────────────────
static int current_key   = 0;    // 0=C .. 11=B
static int current_patch = 18;   // 18=Xylophone
#define VOICE_LEVEL     2
#define BASS_ON         1
#define OCTAVE_SHIFT    0
#define PIANO_BASE     48

// Which value the encoder edits. Push toggles.
typedef enum { MODE_KEY, MODE_PATCH, MODE_COUNT } EncMode;
static EncMode enc_mode = MODE_PATCH;
static const char *MODE_NAMES[MODE_COUNT] = { "KEY", "PATCH" };

// ── Buttons ──────────────────────────────────────────────────────────────────
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
// NOTE: don't name this all_notes_off() — AMY already declares one in amy.h.
static void release_all_chords(void) {
  for (int b = 0; b < NUM_BUTTONS; b++) chord_off(b);
}

static void chord_on(int b) {
  int degree    = BTN_DEGREE[b];
  int root_midi = PIANO_BASE + current_key + MAJOR_SCALE[degree] + OCTAVE_SHIFT * 12;

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

// Reload the patch on synth 0. Kill sounding notes first — the desktop
// load_patch() does the same, otherwise stale voices hang on the old timbre.
static void load_patch(int p) {
  release_all_chords();
  amy_event e = amy_default_event();
  e.synth = 0; e.num_voices = SYNTH_VOICES; e.patch_number = (uint16_t)p;
  amy_add_event(&e);
}

static void print_state(void) {
  Serial.printf("[%s]  key=%-2s  patch=%3d   (push=switch, turn=change)\n",
                MODE_NAMES[enc_mode], KEY_NAMES[current_key], current_patch);
}

// ── Encoder decode ───────────────────────────────────────────────────────────
// Standard quadrature state table. EC11s emit 4 transitions per detent, so we
// accumulate and only report a step every 4 — that gives one step per click.
static const int8_t ENC_TABLE[16] = {
   0, -1,  1,  0,
   1,  0,  0, -1,
  -1,  0,  0,  1,
   0,  1, -1,  0
};
static uint8_t enc_prev  = 0;
static int8_t  enc_accum = 0;

static int enc_read_step(void) {
  uint8_t s   = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  uint8_t idx = ((enc_prev << 2) | s) & 0x0F;
  int8_t  d   = ENC_TABLE[idx];
  enc_prev    = s;
  if (d) {
    enc_accum += d;
    if (enc_accum >=  4) { enc_accum = 0; return  1; }
    if (enc_accum <= -4) { enc_accum = 0; return -1; }
  }
  return 0;
}

static int      sw_pressed    = 0;
static uint32_t sw_changed_ms = 0;

// ── Setup ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nStage 4: encoder + chords");

  for (int b = 0; b < NUM_BUTTONS; b++) {
    pinMode(BTN_PIN[b], INPUT_PULLUP);
    btn_pressed[b] = 0; btn_changed_ms[b] = 0; held_count[b] = 0;
  }
  pinMode(PIN_ENC_A,  INPUT_PULLUP);
  pinMode(PIN_ENC_B,  INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);
  enc_prev = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);

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

  set_volume(VOLUME);
  load_patch(current_patch);
  print_state();
}

// ── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
  uint32_t now = millis();

  // Encoder turn
  int step = enc_read_step();
  if (step) {
    if (enc_mode == MODE_KEY) {
      current_key = (current_key + step + 12) % 12;     // wraps around
    } else {
      int p = current_patch + step;
      if (p < 0)   p = 0;
      if (p > 255) p = 255;
      if (p != current_patch) { current_patch = p; load_patch(current_patch); }
    }
    print_state();
  }

  // Encoder push -> switch what the knob edits
  int sw = (digitalRead(PIN_ENC_SW) == LOW);
  if (sw != sw_pressed && now - sw_changed_ms >= DEBOUNCE_MS) {
    sw_pressed    = sw;
    sw_changed_ms = now;
    if (sw) {                                  // act on press, not release
      enc_mode = (EncMode)((enc_mode + 1) % MODE_COUNT);
      print_state();
    }
  }

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
