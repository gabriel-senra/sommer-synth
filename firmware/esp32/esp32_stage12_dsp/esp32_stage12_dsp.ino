// esp32_stage12_dsp — Stage 12: software DSP tuned for a small speaker.
//
// Adds a live 3-band EQ (AMY global EQ, bands @ 800Hz / 2.5kHz / 7kHz) you can
// switch by ear: push knob 1 to the EQ mode, turn to scroll presets
// (FLAT / SMALL / WARM / BRIGHT). "SMALL" cuts low-mid mud and boosts presence
// + air — the classic small-speaker curve. AMY already soft-clips the master
// bus, so that's our limiter.
//
// Everything else = Stage 10 (waves + Juno patches, rootless-over-bass voicing).
//
// Board:   ESP32 Dev Module (classic ESP-WROOM-32)
// Libraries: AMY + U8g2

#include <AMY-Arduino.h>
#include <U8x8lib.h>
extern "C" {
  #include "chords.h"
}

// Defined up here, before any function, so Arduino's auto-generated prototypes
// (inserted just above the first function) can see the Encoder type.
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
// SINE/TRI turned down: a full chord of these summed past full-scale and
// clipped into distortion. Now balanced with SAW/PULSE so chords stay clean.
const float WAVE_VEL[]   = { 0.45f, 0.45f,   0.50f,  0.50f    };
#define NUM_WAVES    4
#define NUM_PATCHES  128    // Juno only (0-127). DX7 (128-255) dropped — the
                            // 8-osc patches ran at zero margin and crashed on
                            // bass + big chords. Everything left is safe.
#define NUM_SOUNDS   (NUM_WAVES + NUM_PATCHES)
#define WAVE_VOICES  7
#define PATCH_VOICES 3          // 3 * 8 oscs (worst-case DX7) = 24 < MAX_OSCS

static int sound_idx = 4;       // start on P000
static int cur_voices = 5;      // voice count of the current sound (set in load_sound)

// ── DSP: 3-band EQ presets (values in dB @ 800Hz / 2.5kHz / 7kHz) ────────────
typedef struct { const char *name; float l, m, h; } EqPreset;
const EqPreset EQ_PRESETS[] = {
  { "FLAT",   0.0f, 0.0f,  0.0f },   // no EQ (A/B reference)
  { "SMALL", -4.0f, 4.0f,  3.0f },   // small speaker: cut mud, boost presence+air
  { "WARM",   2.0f, 0.0f, -2.0f },   // fuller, softer top
  { "BRITE", -2.0f, 2.0f,  6.0f },   // maximum clarity/sizzle
};
#define NUM_EQ ((int)(sizeof(EQ_PRESETS)/sizeof(EQ_PRESETS[0])))
static int eq_idx = 1;          // start on SMALL

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
static int   voice_level    = 1;      // start on TRIAD (3 notes fit 3 patch voices)
static int   bass_on        = 1;
static char  cur_chord[10]  = "-";

#define VOICE_MIN  0
#define VOICE_MAX  3
#define VOL_MIN   0.0f
#define VOL_MAX  10.0f
#define VOL_STEP  0.5f
#define OCTAVE_SHIFT  0
#define PIANO_BASE   48

typedef enum { MODE_KEY, MODE_SOUND, MODE_VOLUME, MODE_EQ, MODE_COUNT } EncMode;
static EncMode enc_mode = MODE_SOUND;
static const char *MODE_NAMES[MODE_COUNT] = { "KEY", "SND", "VOL", "EQ" };

static int ui_dirty = 1;

// ── Buttons ──────────────────────────────────────────────────────────────────
#define NUM_BUTTONS     4
#define DEBOUNCE_MS    25
const int BTN_PIN[NUM_BUTTONS]    = { 13, 14, 32, 33 };
const int BTN_DEGREE[NUM_BUTTONS] = {  0,  3,  4,  5 };
static int      btn_pressed[NUM_BUTTONS];
static uint32_t btn_changed_ms[NUM_BUTTONS];
static int      held_notes[NUM_BUTTONS][MAX_CHORD_NOTES];
static int      held_count[NUM_BUTTONS];

// ── Encoders (Encoder type is defined at the top of the file) ────────────────
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
static void set_eq(int idx) {
  const EqPreset *p = &EQ_PRESETS[idx];
  amy_event e = amy_default_event();
  e.eq_l = p->l; e.eq_m = p->m; e.eq_h = p->h;   // dB; AMY converts to gain
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
  // Notes are low->high: [bass] root 3rd 5th 7th 9th.
  //
  // 1) With bass on, the bass already sounds the root an octave down, so drop
  //    the root from the chord itself (no doubled "1") on any real chord
  //    (triad+). SINGLE keeps its root so it isn't left as a lone bass note.
  if (bass_on && voice_level >= 1 && n >= 2) {
    for (int i = 1; i < n - 1; i++) notes[i] = notes[i + 1];  // remove root (idx 1)
    n--;
  }
  // 2) Never send more notes than the sound has voices -> identical voicing
  //    every time, no voice-stealing lottery. On a 9th chord, sacrifice the 5TH
  //    first (implied by the root, least missed) so the 9th color survives;
  //    then trim the top if still over. After step 1 the 5th sits at index 2.
  if (n > cur_voices) {
    if (voice_level == 3) {
      for (int i = 2; i < n - 1; i++) notes[i] = notes[i + 1];  // remove 5th
      n--;
    }
    if (n > cur_voices) n = cur_voices;
  }
  float vel = sound_vel(sound_idx);
  int held = 0;
  for (int i = 0; i < n; i++) {
    if (notes[i] < 0 || notes[i] > 127) continue;
    note_on(notes[i], vel);
    held_notes[b][held++] = notes[i];
  }
  held_count[b] = held;
  make_chord_name(current_key, BTN_DEGREE[b], voice_level, -1, cur_chord, sizeof(cur_chord));
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
    cur_voices       = WAVE_VOICES;
  } else {
    int p = idx - NUM_WAVES;
    // Juno (6-osc) -> 5 voices = 30 oscs, safe. DX7 (8-osc) -> 3 voices = 24.
    int v          = (p < 128) ? 5 : 3;
    e.num_voices   = v;
    e.patch_number = (uint16_t)p;
    cur_voices     = v;
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

  if (enc_mode == MODE_EQ)
    snprintf(line, sizeof(line), "EQ %-6s  [EQ]", EQ_PRESETS[eq_idx].name);
  else
    snprintf(line, sizeof(line), "VOL %3.1f   [%s]", current_volume, MODE_NAMES[enc_mode]);
  u8x8.drawString(0, 7, line);
}

static void print_state(void) {
  ui_dirty = 1;
  char snd[8]; sound_name(sound_idx, snd, sizeof(snd));
  Serial.printf("[%-3s] key=%-2s sound=%-5s vol=%4.1f voices=%-6s bass=%s eq=%s\n",
                MODE_NAMES[enc_mode], KEY_NAMES[current_key], snd,
                current_volume, VOICE_LABELS[voice_level], bass_on ? "ON" : "off",
                EQ_PRESETS[eq_idx].name);
}

// ── Setup ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nStage 10: all 256 patches");

  u8x8.begin();
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.clear();
  u8x8.drawString(0, 0, "chord-synth");

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
  set_eq(eq_idx);
  load_sound(sound_idx);

  u8x8.clear();
  draw_display();
  Serial.println("ready — scroll SOUND to audition P000..P255");
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
      case MODE_EQ: {
        int x = eq_idx + s1;
        if (x < 0) x = 0; if (x > NUM_EQ - 1) x = NUM_EQ - 1;
        if (x != eq_idx) { eq_idx = x; set_eq(eq_idx); }
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
