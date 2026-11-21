// esp32_stage5_volume — Stage 5: knob now cycles KEY / PATCH / VOLUME.
//
// Same as Stage 4, plus VOLUME as a third encoder mode. Push cycles through the
// three; turn changes whichever is selected.
//
//    PUSH -> KEY -> PATCH -> VOLUME -> KEY ...
//    TURN -> change the selected value
//
// Board:   ESP32 Dev Module (classic ESP-WROOM-32)
// Library: AMY (~/Documents/Arduino/libraries/AMY)
//
// ── Wiring (all unchanged from Stage 4) ──────────────────────────────────────
// MAX98357 #1: LRC->25  BCLK->26  DIN->27  Vin->5V  GND->GND  + speaker
// Buttons:     GPIO13, 14, 32, 33 -> other leg to GND
// EC11:        A->18   C(middle)->GND   B->19   SW->23   SW->GND
//
// ── SECOND SPEAKER (new, optional) ───────────────────────────────────────────
// Wire a SECOND MAX98357 in parallel with the first — it shares the same three
// I2S signal wires, no code change needed:
//    amp2 Vin  -> + rail        amp2 LRC  -> GPIO25   (same pin as amp1)
//    amp2 GND  -> - rail        amp2 BCLK -> GPIO26   (same pin as amp1)
//    amp2 SD/GAIN -> nothing    amp2 DIN  -> GPIO27   (same pin as amp1)
//    amp2 screw terminal -> your 2nd speaker
// With SD left floating both amps play the same mono mix. Do NOT instead hang a
// 2nd speaker off amp1's terminal unless you know the load stays >= 4 ohm.

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

// ── Engine budget (6/32 proved stable on this board) ─────────────────────────
#define MAX_VOICES      6
#define MAX_OSCS       32
#define SYNTH_VOICES    3    // *~6 oscs/voice must stay under MAX_OSCS
#define VELOCITY     0.8f

// ── Live state (all three now editable by the knob) ──────────────────────────
static int   current_key    = 0;      // 0=C .. 11=B
static int   current_patch  = 18;     // 18=Xylophone
// AMY volume is 0..10 (it scales by 0.1 internally). Starting LOW on purpose:
// chords sum, so a level that's clean on one note will clip on four. Turn it up
// by ear while HOLDING a chord — the point it gets fuzzy is your ceiling.
static float current_volume = 2.5f;

#define VOL_MIN      0.0f
#define VOL_MAX     10.0f
#define VOL_STEP     0.5f

#define VOICE_LEVEL     2
#define BASS_ON         1
#define OCTAVE_SHIFT    0
#define PIANO_BASE     48

// Which value the knob edits. Push cycles.
typedef enum { MODE_KEY, MODE_PATCH, MODE_VOLUME, MODE_COUNT } EncMode;
static EncMode enc_mode = MODE_VOLUME;     // start on volume — you'll want it
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

static void load_patch(int p) {
  release_all_chords();
  amy_event e = amy_default_event();
  e.synth = 0; e.num_voices = SYNTH_VOICES; e.patch_number = (uint16_t)p;
  amy_add_event(&e);
}

static void print_state(void) {
  Serial.printf("[%-6s]  key=%-2s  patch=%3d  vol=%4.1f\n",
                MODE_NAMES[enc_mode], KEY_NAMES[current_key],
                current_patch, current_volume);
}

// ── Encoder decode ───────────────────────────────────────────────────────────
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
  Serial.println("\nStage 5: knob = KEY / PATCH / VOLUME");

  for (int b = 0; b < NUM_BUTTONS; b++) {
    pinMode(BTN_PIN[b], INPUT_PULLUP);
    btn_pressed[b] = 0; btn_changed_ms[b] = 0; held_count[b] = 0;
  }
  pinMode(PIN_ENC_A,  INPUT_PULLUP);
  pinMode(PIN_ENC_B,  INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);
  enc_prev = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);

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
  Serial.println("push = switch KEY/PATCH/VOLUME, turn = change");
  print_state();
}

// ── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
  uint32_t now = millis();

  // Encoder turn
  int step = enc_read_step();
  if (step) {
    switch (enc_mode) {
      case MODE_KEY:
        current_key = (current_key + step + 12) % 12;      // wraps
        break;
      case MODE_PATCH: {
        int p = current_patch + step;
        if (p < 0)   p = 0;
        if (p > 255) p = 255;
        if (p != current_patch) { current_patch = p; load_patch(current_patch); }
        break;
      }
      case MODE_VOLUME: {
        float v = current_volume + step * VOL_STEP;
        if (v < VOL_MIN) v = VOL_MIN;
        if (v > VOL_MAX) v = VOL_MAX;
        if (v != current_volume) { current_volume = v; set_volume(current_volume); }
        break;
      }
      default: break;
    }
    print_state();
  }

  // Encoder push -> cycle what the knob edits
  int sw = (digitalRead(PIN_ENC_SW) == LOW);
  if (sw != sw_pressed && now - sw_changed_ms >= DEBOUNCE_MS) {
    sw_pressed    = sw;
    sw_changed_ms = now;
    if (sw) {                                   // act on press, not release
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
