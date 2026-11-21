// esp32_stage11_scale — Stage 11: 7 buttons = the full diatonic scale.
//
// Adds 3 buttons to reach all 7 scale degrees (I ii iii IV V vi vii).
// The 3 new buttons are on the ESP32's INPUT-ONLY pins (34/35/36), which have
// NO internal pull-up — so each needs an external 10k resistor to 3.3V, and the
// code reads them as plain INPUT (not INPUT_PULLUP).
//
// Board:   ESP32 Dev Module (classic ESP-WROOM-32)
// Libraries: AMY + U8g2
//
// ── Button wiring ────────────────────────────────────────────────────────────
// Buttons 1-4 (as before): GPIO 13, 14, 32, 33 -> other leg to GND (internal PU)
// Buttons 5-7 (NEW):        GPIO 34, 35, 36
//     each: pin --10k-- 3.3V   (pull-up)   AND   pin --button-- GND
// Other wiring unchanged: I2S 25/26/27 (+DAC SCK->GND), enc1 18/19/23,
//     enc2 4/16/17, OLED VCC->3V3 GND->GND SDA->D21 SCL->D22.

#include <AMY-Arduino.h>
#include <U8x8lib.h>
extern "C" {
  #include "chords.h"
}

// Type defined up top so Arduino's auto-prototypes can see it.
typedef struct {
  int pinA, pinB, pinSW; uint8_t prev; int8_t accum; int sw_state; uint32_t sw_ms;
} Encoder;

U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(U8X8_PIN_NONE);

#define PIN_LRC   25
#define PIN_BCLK  26
#define PIN_DOUT  27

#define MAX_VOICES      8
#define MAX_OSCS       32

// ── Sound list = waves + all patches ─────────────────────────────────────────
const int   WAVES[]      = { SINE, SAW_DOWN, PULSE, TRIANGLE };
const char *WAVE_NAMES[] = { "SINE", "SAW",  "PULSE","TRI"    };
const float WAVE_VEL[]   = { 0.85f, 0.45f,   0.50f,  0.80f    };
#define NUM_WAVES    4
#define NUM_PATCHES  256
#define NUM_SOUNDS   (NUM_WAVES + NUM_PATCHES)
#define WAVE_VOICES  7

static int sound_idx = 4;       // start on P000

static void sound_name(int idx, char *buf, int sz) {
  if (idx < NUM_WAVES) snprintf(buf, sz, "%s", WAVE_NAMES[idx]);
  else                 snprintf(buf, sz, "P%03d", idx - NUM_WAVES);
}
static float sound_vel(int idx) {
  return (idx < NUM_WAVES) ? WAVE_VEL[idx] : 0.80f;
}

// ── Live state ───────────────────────────────────────────────────────────────
static int   current_key    = 0;
static float current_volume = 3.0f;
static int   voice_level    = 1;      // TRIAD
static int   bass_on        = 1;
static char  cur_chord[10]  = "-";

#define VOICE_MIN  0
#define VOICE_MAX  3
#define VOL_MIN   0.0f
#define VOL_MAX  10.0f
#define VOL_STEP  0.5f
#define OCTAVE_SHIFT  0
#define PIANO_BASE   48

typedef enum { MODE_KEY, MODE_SOUND, MODE_VOLUME, MODE_COUNT } EncMode;
static EncMode enc_mode = MODE_SOUND;
static const char *MODE_NAMES[MODE_COUNT] = { "KEY", "SND", "VOL" };

static int ui_dirty = 1;

// ── Buttons: 7 = full scale (I ii iii IV V vi vii) ───────────────────────────
#define NUM_BUTTONS     7
#define DEBOUNCE_MS    25
const int BTN_PIN[NUM_BUTTONS]    = { 13, 14, 32, 33, 34, 35, 36 };
const int BTN_DEGREE[NUM_BUTTONS] = {  0,  1,  2,  3,  4,  5,  6 };
static int      btn_pressed[NUM_BUTTONS];
static uint32_t btn_changed_ms[NUM_BUTTONS];
static int      held_notes[NUM_BUTTONS][MAX_CHORD_NOTES];
static int      held_count[NUM_BUTTONS];

// Pins 34..39 are input-only with no internal pull-up -> external resistor +
// plain INPUT. Everything else can use the chip's internal pull-up.
static int btn_mode_for(int pin) { return (pin >= 34) ? INPUT : INPUT_PULLUP; }

// ── Encoders ─────────────────────────────────────────────────────────────────
static Encoder enc1 = { 18, 19, 23, 0, 0, 0, 0 };
static Encoder enc2 = {  4, 16, 17, 0, 0, 0, 0 };
static const int8_t ENC_TABLE[16] = { 0,-1,1,0, 1,0,0,-1, -1,0,0,1, 0,1,-1,0 };

static void enc_init(Encoder *e) {
  pinMode(e->pinA, INPUT_PULLUP); pinMode(e->pinB, INPUT_PULLUP);
  pinMode(e->pinSW, INPUT_PULLUP);
  e->prev = (digitalRead(e->pinA) << 1) | digitalRead(e->pinB);
}
static int enc_turn(Encoder *e) {
  uint8_t s = (digitalRead(e->pinA) << 1) | digitalRead(e->pinB);
  int8_t d = ENC_TABLE[((e->prev << 2) | s) & 0x0F];
  e->prev = s;
  if (d) { e->accum += d;
    if (e->accum >=  4) { e->accum = 0; return  1; }
    if (e->accum <= -4) { e->accum = 0; return -1; } }
  return 0;
}
static int enc_pushed(Encoder *e, uint32_t now) {
  int sw = (digitalRead(e->pinSW) == LOW);
  if (sw != e->sw_state && now - e->sw_ms >= DEBOUNCE_MS) {
    e->sw_state = sw; e->sw_ms = now;
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
  release_all_chords();
  int degree    = BTN_DEGREE[b];
  int root_midi = PIANO_BASE + current_key + MAJOR_SCALE[degree] + OCTAVE_SHIFT * 12;
  int notes[MAX_CHORD_NOTES];
  int n = build_chord_notes(degree, root_midi, voice_level, bass_on, -1, notes);
  float vel = sound_vel(sound_idx);
  for (int i = 0; i < n; i++) {
    if (notes[i] < 0 || notes[i] > 127) continue;
    note_on(notes[i], vel);
    held_notes[b][i] = notes[i];
  }
  held_count[b] = n;
  make_chord_name(current_key, degree, voice_level, -1, cur_chord, sizeof(cur_chord));
  ui_dirty = 1;
  Serial.printf("  %s\n", cur_chord);
}

static void load_sound(int idx) {
  release_all_chords();
  amy_event e = amy_default_event();
  e.synth = 0;
  if (idx < NUM_WAVES) {
    e.num_voices     = WAVE_VOICES;
    e.oscs_per_voice = 1;
    e.wave           = (uint16_t)WAVES[idx];
  } else {
    int p = idx - NUM_WAVES;
    e.num_voices   = (p < 128) ? 5 : 4;   // Juno 6-osc -> 5, DX7 8-osc -> 4
    e.patch_number = (uint16_t)p;
  }
  amy_add_event(&e);
}

// ── OLED ─────────────────────────────────────────────────────────────────────
static void draw_display(void) {
  char line[20], snd[8];
  sound_name(sound_idx, snd, sizeof(snd));
  snprintf(line, sizeof(line), "KEY %-2s   %-5s", KEY_NAMES[current_key], snd);
  u8x8.drawString(0, 0, line);

  char big[9];
  snprintf(big, sizeof(big), "%-8s", cur_chord);
  u8x8.draw2x2String(0, 2, big);

  snprintf(line, sizeof(line), "%-6s BASS %-3s", VOICE_LABELS[voice_level], bass_on ? "ON" : "off");
  u8x8.drawString(0, 5, line);

  snprintf(line, sizeof(line), "VOL %3.1f   [%s]", current_volume, MODE_NAMES[enc_mode]);
  u8x8.drawString(0, 7, line);
}

static void print_state(void) {
  ui_dirty = 1;
  char snd[8]; sound_name(sound_idx, snd, sizeof(snd));
  Serial.printf("[%-3s] key=%-2s sound=%-5s vol=%4.1f voices=%-6s bass=%s\n",
                MODE_NAMES[enc_mode], KEY_NAMES[current_key], snd,
                current_volume, VOICE_LABELS[voice_level], bass_on ? "ON" : "off");
}

// ── Setup ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nStage 11: 7-button scale");

  u8x8.begin();
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.clear();
  u8x8.drawString(0, 0, "chord-synth");

  for (int b = 0; b < NUM_BUTTONS; b++) {
    pinMode(BTN_PIN[b], btn_mode_for(BTN_PIN[b]));
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
  load_sound(sound_idx);

  u8x8.clear();
  draw_display();
  Serial.println("ready - 7 buttons = I ii iii IV V vi vii");
}

// ── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
  uint32_t now = millis();

  int s1 = enc_turn(&enc1);
  if (s1) {
    switch (enc_mode) {
      case MODE_KEY: current_key = (current_key + s1 + 12) % 12; break;
      case MODE_SOUND: {
        int x = sound_idx + s1;
        if (x < 0) x = 0; if (x > NUM_SOUNDS - 1) x = NUM_SOUNDS - 1;
        if (x != sound_idx) { sound_idx = x; load_sound(sound_idx); }
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
  if (enc_pushed(&enc1, now)) { enc_mode = (EncMode)((enc_mode + 1) % MODE_COUNT); print_state(); }

  int s2 = enc_turn(&enc2);
  if (s2) {
    int v = voice_level + s2;
    if (v < VOICE_MIN) v = VOICE_MIN; if (v > VOICE_MAX) v = VOICE_MAX;
    if (v != voice_level) { voice_level = v; print_state(); }
  }
  if (enc_pushed(&enc2, now)) { bass_on = !bass_on; print_state(); }

  // Chord buttons (mono). Pins 34/35/36 read via external pull-up; same LOW=press.
  for (int b = 0; b < NUM_BUTTONS; b++) {
    int raw = (digitalRead(BTN_PIN[b]) == LOW);
    if (raw == btn_pressed[b]) continue;
    if (now - btn_changed_ms[b] < DEBOUNCE_MS) continue;
    btn_pressed[b]    = raw;
    btn_changed_ms[b] = now;
    if (raw) {
      chord_on(b);
    } else {
      int was_sounding = held_count[b] > 0;
      chord_off(b);
      if (was_sounding) { strcpy(cur_chord, "-"); ui_dirty = 1; }
    }
  }

  if (ui_dirty) { draw_display(); ui_dirty = 0; }
}
