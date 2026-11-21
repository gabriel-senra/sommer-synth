// esp32_stage3_chords — Stage 3: a playable chord instrument.
//
// Four buttons play real chords, built by the SAME chords.c music-theory engine
// the desktop synth uses (it's SDL-free, so it compiles here untouched).
// Buttons are the classic I - IV - V - vi progression in the current key.
//
// Board:   ESP32 Dev Module (classic ESP-WROOM-32)
// Library: AMY (installed at ~/Documents/Arduino/libraries/AMY)
//
// Wiring — MAX98357 amp (unchanged from Stage 2):
//   LRC->GPIO25   BCLK->GPIO26   DIN->GPIO27   Vin->5V   GND->GND
//
// Wiring — buttons (NEW): each button has one leg to its GPIO, the other to GND.
// We use the ESP32's internal pull-ups, so a press reads LOW. No resistors.
//   button 1 -> GPIO13     button 3 -> GPIO32
//   button 2 -> GPIO14     button 4 -> GPIO33
// (Avoid GPIO12 for buttons: it's a boot strapping pin. GPIO34/35 have no
//  internal pull-up, so they can't be used this way either.)

#include <AMY-Arduino.h>
extern "C" {
  #include "chords.h"     // C engine; extern "C" stops C++ name mangling
}

// ── Audio wiring ─────────────────────────────────────────────────────────────
#define PIN_LRC   25
#define PIN_BCLK  26
#define PIN_DOUT  27

// ── Engine budget (your benchmarked numbers) ─────────────────────────────────
// Backed off from 8/48 to the 6/32 that Stage 2 first proved. chords.c adds its
// own tables, so the RAM that 8/48 barely fit before may not fit now. Raise
// again once it boots clean.
#define MAX_VOICES      6
#define MAX_OSCS       32
// CAREFUL: patch 18 uses ~6 oscs per voice, so SYNTH_VOICES * 6 must stay well
// under MAX_OSCS or the patch fails to allocate and you get SILENCE.
// 4 voices * 6 = 24 oscs — the combo Stage 2 proved works. Raise MAX_OSCS first
// if you want more voices (e.g. MAX_OSCS 96 lets you run SYNTH_VOICES 8+).
#define SYNTH_VOICES    4
#define PATCH          18    // 18=Xylophone 0=Juno brass 7=Piano 22=Pizzicato
#define VOLUME       8.0f    // AMY master, 0..10
#define VELOCITY     0.8f    // per-note, 0..1

// ── Musical config ───────────────────────────────────────────────────────────
#define CURRENT_KEY     0    // 0=C 1=C# 2=D ... 11=B
#define VOICE_LEVEL     2    // 0..4 — chord richness (triad -> extensions)
#define BASS_ON         1    // add a bass note below the chord
#define OCTAVE_SHIFT    0    // -2..+2
#define PIANO_BASE     48    // MIDI note for the root of the bottom octave

// ── Buttons ──────────────────────────────────────────────────────────────────
#define NUM_BUTTONS     4
#define DEBOUNCE_MS    25

const int BTN_PIN[NUM_BUTTONS]    = { 13, 14, 32, 33 };
const int BTN_DEGREE[NUM_BUTTONS] = {  0,  3,  4,  5 };   // I, IV, V, vi

static int      btn_pressed[NUM_BUTTONS];                 // debounced state
static uint32_t btn_changed_ms[NUM_BUTTONS];
static int      held_notes[NUM_BUTTONS][MAX_CHORD_NOTES]; // what each button sounded
static int      held_count[NUM_BUTTONS];

// ── AMY helpers ──────────────────────────────────────────────────────────────
static void note_on(int midi_note, float vel) {
  amy_event e = amy_default_event();
  e.synth = 0; e.midi_note = (float)midi_note; e.velocity = vel;
  amy_add_event(&e);
}
static void note_off(int midi_note) {
  amy_event e = amy_default_event();
  e.synth = 0; e.midi_note = (float)midi_note; e.velocity = 0.0f;
  amy_add_event(&e);
}
static void set_volume(float v) {
  amy_event e = amy_default_event();
  e.volume = v;
  amy_add_event(&e);
}

// ── Chord trigger ────────────────────────────────────────────────────────────
// Mirrors ck_root_midi() + trigger_chord() from the desktop synth.c.
static void chord_on(int b) {
  int degree    = BTN_DEGREE[b];
  int root_midi = PIANO_BASE + CURRENT_KEY + MAJOR_SCALE[degree] + OCTAVE_SHIFT * 12;

  int notes[MAX_CHORD_NOTES];
  int n = build_chord_notes(degree, root_midi, VOICE_LEVEL, BASS_ON, -1, notes);

  for (int i = 0; i < n; i++) {
    if (notes[i] < 0 || notes[i] > 127) continue;
    note_on(notes[i], VELOCITY);
    held_notes[b][i] = notes[i];
  }
  held_count[b] = n;

  char name[24];
  make_chord_name(CURRENT_KEY, degree, VOICE_LEVEL, -1, name, sizeof(name));
  Serial.printf("[%s] %s  (%d notes)\n", DEGREE_ROMAN[degree], name, n);
}

static void chord_off(int b) {
  for (int i = 0; i < held_count[b]; i++) note_off(held_notes[b][i]);
  held_count[b] = 0;
}

// ── Setup ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nStage 3: playable chords");

  for (int b = 0; b < NUM_BUTTONS; b++) {
    pinMode(BTN_PIN[b], INPUT_PULLUP);   // press == LOW
    btn_pressed[b]    = 0;
    btn_changed_ms[b] = 0;
    held_count[b]     = 0;
  }

  // Heap BEFORE AMY: this is the whole budget it has to allocate from. If AMY
  // can't get what max_oscs/max_voices need, malloc returns NULL, AMY writes to
  // it, and you get "Guru Meditation ... StoreProhibited". Watch this number.
  Serial.printf("free heap BEFORE amy_start: %u bytes\n", (unsigned)ESP.getFreeHeap());
  Serial.printf("asking for max_voices=%d max_oscs=%d\n", MAX_VOICES, MAX_OSCS);
  Serial.flush();

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

  Serial.printf("free heap AFTER amy_start: %u bytes\n", (unsigned)ESP.getFreeHeap());

  set_volume(VOLUME);

  amy_event e = amy_default_event();
  e.synth = 0; e.num_voices = SYNTH_VOICES; e.patch_number = PATCH;
  amy_add_event(&e);

  Serial.printf("key=%s  patch=%d  voice_level=%d  free heap=%u\n",
                KEY_NAMES[CURRENT_KEY], PATCH, VOICE_LEVEL,
                (unsigned)ESP.getFreeHeap());
  Serial.println("press a button — I / IV / V / vi");
}

// ── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
  uint32_t now = millis();

  for (int b = 0; b < NUM_BUTTONS; b++) {
    int raw = (digitalRead(BTN_PIN[b]) == LOW);        // pull-up: LOW == pressed
    if (raw == btn_pressed[b]) continue;               // no change
    if (now - btn_changed_ms[b] < DEBOUNCE_MS) continue;  // too soon; bounce

    btn_pressed[b]    = raw;
    btn_changed_ms[b] = now;
    if (raw) chord_on(b);
    else     chord_off(b);
  }
}
