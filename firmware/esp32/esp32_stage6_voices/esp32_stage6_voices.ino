// esp32_stage6_voices — Stage 6: a SECOND encoder dedicated to chord voicing.
//
// Encoder 1 (as before):  TURN = value, PUSH = cycle KEY / PATCH / VOLUME
// Encoder 2 (NEW):        TURN = voicing  SINGLE->TRIAD->7TH->9TH->11TH
//                         PUSH = bass note on/off
//
// Board:   ESP32 Dev Module (classic ESP-WROOM-32)
// Library: AMY (~/Documents/Arduino/libraries/AMY)
//
// ── Wiring ───────────────────────────────────────────────────────────────────
// DAC/amp (I2S):  D25->LCK/LRC  D26->BCK/BCLK  D27->DIN   (+ SCK->GND on the DAC)
// Buttons:        GPIO13, 14, 32, 33  -> other leg to GND
// Encoder 1:      A->18  C(middle)->GND  B->19  SW->23  SW->GND
// Encoder 2 NEW:  A->4   C(middle)->GND  B->16  SW->17  SW->GND

#include <AMY-Arduino.h>
extern "C" {
  #include "chords.h"
}

// ── Audio wiring ─────────────────────────────────────────────────────────────
#define PIN_LRC   25
#define PIN_BCLK  26
#define PIN_DOUT  27

// ── Engine budget (6/32 proved stable) ───────────────────────────────────────
#define MAX_VOICES      6
#define MAX_OSCS       32
// 5 voices * 6 oscs (xylophone) = 30, just under MAX_OSCS 32 — so up to 5 notes
// sound before AMY steals. Covers a 7th chord, or a 9th without bass.
// WARNING: if you scroll to a heavier 8-osc patch, 5*8=40 > 32 and it may go
// silent — drop this back to 3, or switch to a lean patch (workaround #2).
#define SYNTH_VOICES    5
#define VELOCITY     0.8f

// ── Live state ───────────────────────────────────────────────────────────────
static int   current_key    = 0;      // 0=C .. 11=B
static int   current_patch  = 18;     // 18=Xylophone
static float current_volume = 2.0f;   // AMY 0..10
static int   voice_level    = 2;      // 0..4  SINGLE/TRIAD/7TH/9TH/11TH
static int   bass_on        = 1;      // root an octave below

// voice_level indexes VOICE_LABELS = SINGLE,TRIAD,7TH,9TH,11TH.
// Capped at 9TH (index 3): 11TH stacks too many notes and overruns the voice/
// osc budget, which crashes+reboots the ESP32 (the "doot doot" bleep). Turning
// past either end simply does nothing (the clamp below leaves voice_level
// unchanged, so no re-print, no action).
#define VOICE_MIN  0    // SINGLE
#define VOICE_MAX  3    // 9TH
#define VOL_MIN   0.0f
#define VOL_MAX  10.0f
#define VOL_STEP  0.5f
#define OCTAVE_SHIFT  0
#define PIANO_BASE   48

typedef enum { MODE_KEY, MODE_PATCH, MODE_VOLUME, MODE_COUNT } EncMode;
static EncMode enc_mode = MODE_PATCH;
static const char *MODE_NAMES[MODE_COUNT] = { "KEY", "PATCH", "VOLUME" };

// ── Buttons ──────────────────────────────────────────────────────────────────
#define NUM_BUTTONS     4
#define DEBOUNCE_MS    25
const int BTN_PIN[NUM_BUTTONS]    = { 13, 14, 32, 33 };
const int BTN_DEGREE[NUM_BUTTONS] = {  0,  3,  4,  5 };   // I, IV, V, vi
static int      btn_pressed[NUM_BUTTONS];
static uint32_t btn_changed_ms[NUM_BUTTONS];
static int      held_notes[NUM_BUTTONS][MAX_CHORD_NOTES];
static int      held_count[NUM_BUTTONS];

// ── A reusable encoder (so we can have several) ──────────────────────────────
typedef struct {
  int      pinA, pinB, pinSW;
  uint8_t  prev;
  int8_t   accum;
  int      sw_state;
  uint32_t sw_ms;
} Encoder;

static Encoder enc1 = { 18, 19, 23, 0, 0, 0, 0 };   // KEY/PATCH/VOLUME
static Encoder enc2 = {  4, 16, 17, 0, 0, 0, 0 };   // VOICES / bass

static const int8_t ENC_TABLE[16] = {
   0, -1,  1,  0,   1,  0,  0, -1,
  -1,  0,  0,  1,   0,  1, -1,  0
};

static void enc_init(Encoder *e) {
  pinMode(e->pinA,  INPUT_PULLUP);
  pinMode(e->pinB,  INPUT_PULLUP);
  pinMode(e->pinSW, INPUT_PULLUP);
  e->prev = (digitalRead(e->pinA) << 1) | digitalRead(e->pinB);
}

// returns -1 / 0 / +1 per detent
static int enc_turn(Encoder *e) {
  uint8_t s   = (digitalRead(e->pinA) << 1) | digitalRead(e->pinB);
  uint8_t idx = ((e->prev << 2) | s) & 0x0F;
  int8_t  d   = ENC_TABLE[idx];
  e->prev     = s;
  if (d) {
    e->accum += d;
    if (e->accum >=  4) { e->accum = 0; return  1; }
    if (e->accum <= -4) { e->accum = 0; return -1; }
  }
  return 0;
}

// returns 1 on a fresh press (debounced)
static int enc_pushed(Encoder *e, uint32_t now) {
  int sw = (digitalRead(e->pinSW) == LOW);
  if (sw != e->sw_state && now - e->sw_ms >= DEBOUNCE_MS) {
    e->sw_state = sw;
    e->sw_ms    = now;
    if (sw) return 1;
  }
  return 0;
}

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
  int root_midi = PIANO_BASE + current_key + MAJOR_SCALE[degree] + OCTAVE_SHIFT * 12;
  int notes[MAX_CHORD_NOTES];
  int n = build_chord_notes(degree, root_midi, voice_level, bass_on, -1, notes);
  for (int i = 0; i < n; i++) {
    if (notes[i] < 0 || notes[i] > 127) continue;
    note_on(notes[i], VELOCITY);
    held_notes[b][i] = notes[i];
  }
  held_count[b] = n;

  char name[24];
  make_chord_name(current_key, degree, voice_level, -1, name, sizeof(name));
  Serial.printf("  %-4s %-10s (%d notes)\n", DEGREE_ROMAN[degree], name, n);
}

static void load_patch(int p) {
  release_all_chords();
  amy_event e = amy_default_event();
  e.synth = 0; e.num_voices = SYNTH_VOICES; e.patch_number = (uint16_t)p;
  amy_add_event(&e);
}

static void print_state(void) {
  Serial.printf("[%-6s] key=%-2s patch=%3d vol=%4.1f  voices=%-6s bass=%s\n",
                MODE_NAMES[enc_mode], KEY_NAMES[current_key], current_patch,
                current_volume, VOICE_LABELS[voice_level], bass_on ? "ON" : "off");
}

// ── Setup ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nStage 6: two encoders (KEY/PATCH/VOL + VOICES)");

  for (int b = 0; b < NUM_BUTTONS; b++) {
    pinMode(BTN_PIN[b], INPUT_PULLUP);
    btn_pressed[b] = 0; btn_changed_ms[b] = 0; held_count[b] = 0;
  }
  enc_init(&enc1);
  enc_init(&enc2);

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

  Serial.printf("free heap after amy_start: %u\n", (unsigned)ESP.getFreeHeap());
  set_volume(current_volume);
  load_patch(current_patch);
  Serial.println("enc1 push=KEY/PATCH/VOL   enc2 turn=voicing push=bass");
  print_state();
}

// ── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
  uint32_t now = millis();

  // ── Encoder 1: KEY / PATCH / VOLUME ──
  int s1 = enc_turn(&enc1);
  if (s1) {
    switch (enc_mode) {
      case MODE_KEY:
        current_key = (current_key + s1 + 12) % 12;
        break;
      case MODE_PATCH: {
        int p = current_patch + s1;
        if (p < 0) p = 0; if (p > 255) p = 255;
        if (p != current_patch) { current_patch = p; load_patch(current_patch); }
        break;
      }
      case MODE_VOLUME: {
        float v = current_volume + s1 * VOL_STEP;
        if (v < VOL_MIN) v = VOL_MIN; if (v > VOL_MAX) v = VOL_MAX;
        if (v != current_volume) { current_volume = v; set_volume(current_volume); }
        break;
      }
      default: break;
    }
    print_state();
  }
  if (enc_pushed(&enc1, now)) {
    enc_mode = (EncMode)((enc_mode + 1) % MODE_COUNT);
    print_state();
  }

  // ── Encoder 2: VOICES (turn) + BASS (push) ──
  int s2 = enc_turn(&enc2);
  if (s2) {
    int v = voice_level + s2;
    if (v < VOICE_MIN) v = VOICE_MIN;
    if (v > VOICE_MAX) v = VOICE_MAX;
    if (v != voice_level) { voice_level = v; print_state(); }
  }
  if (enc_pushed(&enc2, now)) {
    bass_on = !bass_on;
    print_state();
  }

  // ── Chord buttons ──
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
