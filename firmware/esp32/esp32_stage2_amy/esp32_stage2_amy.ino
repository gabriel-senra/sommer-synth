// esp32_stage2_amy — Stage 2: run the real AMY synth engine on the ESP32.
//
// Plays a C major chord once per second through a MAX98357 I2S amp, using a
// real AMY patch. Also doubles as the polyphony benchmark for the hardware:
// walk the TUNE block below up until it breaks (see notes there).
//
// Board:   ESP32 Dev Module (classic ESP-WROOM-32)
// Library: AMY (installed at ~/Documents/Arduino/libraries/AMY)
// Wiring:  MAX98357  LRC->GPIO25   BCLK->GPIO26   DIN->GPIO27
//          Vin->5V   GND->GND      GAIN/SD: leave unconnected
//
// amy_start() spawns AMY's own render + I2S FreeRTOS tasks, so audio runs in
// the background and loop() is free to just send note events.

#include <AMY-Arduino.h>

// ── Wiring ───────────────────────────────────────────────────────────────────
#define PIN_LRC   25    // amp LRC
#define PIN_BCLK  26    // amp BCLK
#define PIN_DOUT  27    // amp DIN

// ── TUNE: find the ESP32's real polyphony ceiling ────────────────────────────
// Walk MAX_OSCS / MAX_VOICES up until one of these happens:
//   audio crackles or stutters -> CPU limit (can't render in time)
//   board reboots / Serial spews boot text -> RAM limit
// The last setting that stays clean is this chip's real budget.
#define MAX_VOICES      8    // total voice pool. try 8, 12, 16...
#define MAX_OSCS       48    // total oscillator pool. try 48, 64, 96...
#define SYNTH_VOICES    4    // voices given to synth 0 = notes holdable at once
#define PATCH          18    // 18=Xylophone 0=Juno brass 7=Piano 22=Pizzicato 12=Pluck

// ── Volume ───────────────────────────────────────────────────────────────────
// AMY's master volume runs 0..10 (NOT 0..1) and internally scales by 0.1, so
// the default of 1.0 is only ~10% of full scale. That headroom is deliberate:
// it stops many summed voices from clipping. Raise for more level; if it starts
// sounding harsh/flat-topped you've hit the clipper — back off.
#define VOLUME       10.0f    // try 1.0 (default/quiet) .. 10.0 (max)
#define VELOCITY     0.8f    // per-note loudness, 0..1

// ── Note helpers ─────────────────────────────────────────────────────────────
static void note_on(int midi_note, float vel) {
  amy_event e = amy_default_event();
  e.synth     = 0;
  e.midi_note = (float)midi_note;
  e.velocity  = vel;
  amy_add_event(&e);
}

static void note_off(int midi_note) {
  amy_event e = amy_default_event();
  e.synth     = 0;
  e.midi_note = (float)midi_note;
  e.velocity  = 0.0f;          // velocity 0 == note off
  amy_add_event(&e);
}

// Master volume — a global AMY event, not tied to any synth.
static void set_volume(float v) {
  amy_event e = amy_default_event();
  e.volume = v;
  amy_add_event(&e);
}

// ── Setup ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("Stage 2: AMY on ESP32");
  Serial.printf("budget: max_voices=%d max_oscs=%d synth_voices=%d patch=%d\n",
                MAX_VOICES, MAX_OSCS, SYNTH_VOICES, PATCH);

  amy_config_t c = amy_default_config();
  c.audio    = AMY_AUDIO_IS_I2S;
  c.i2s_lrc  = PIN_LRC;
  c.i2s_bclk = PIN_BCLK;
  c.i2s_dout = PIN_DOUT;
  c.i2s_mclk = -1;                  // MAX98357 makes its own clock; no MCLK

  c.features.default_synths = 0;
  c.features.startup_bleep  = 1;    // audible proof AMY itself booted
  c.max_voices = MAX_VOICES;
  c.max_oscs   = MAX_OSCS;

  amy_start(c);                     // starts AMY's background audio tasks
  Serial.println("amy_start() done — startup bleep should have played");

  set_volume(VOLUME);
  Serial.printf("master volume set to %.1f (of 10)\n", VOLUME);

  // Give synth 0 a patch (same idea as load_patch() on the desktop build).
  amy_event e = amy_default_event();
  e.synth        = 0;
  e.num_voices   = SYNTH_VOICES;
  e.patch_number = PATCH;
  amy_add_event(&e);
  Serial.printf("patch %d loaded on synth 0\n", PATCH);
  Serial.printf("free heap: %u bytes\n", (unsigned)ESP.getFreeHeap());
}

// ── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
  const int chord[3] = {60, 64, 67};   // C major

  for (int i = 0; i < 3; i++) note_on(chord[i], VELOCITY);
  Serial.println("chord on");
  delay(800);

  for (int i = 0; i < 3; i++) note_off(chord[i]);
  Serial.println("chord off");
  delay(700);
}
