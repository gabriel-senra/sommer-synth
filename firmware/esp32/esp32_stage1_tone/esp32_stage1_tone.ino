// esp32_stage1_tone — Stage 1: prove audio works.
//
// Plays a steady 440 Hz test tone out of a MAX98357 I2S amp + speaker.
// If you hear this cleanly, your toolchain, wiring, and audio path are all good
// and we can move on to running AMY (the real synth engine) on top.
//
// Board:  ESP32 Dev Module (the classic ESP-WROOM-32, NOT S3)
// Library: uses ESP_I2S, which ships with Arduino-ESP32 core 3.x (nothing to install)
//
// Wiring — MAX98357 amp:
//   VIN   -> 5V (VIN)        GND   -> GND
//   LRC   -> GPIO25          BCLK  -> GPIO26          DIN -> GPIO27
//   GAIN  -> (leave open)    SD    -> (leave open)
//   Speaker + / -  -> your 8 ohm speaker (do NOT ground either leg)

#include <ESP_I2S.h>
#include <math.h>

// ---- Pins to the MAX98357 ----
#define PIN_BCLK 26   // bit clock  -> BCLK
#define PIN_LRC  25   // word clock -> LRC
#define PIN_DOUT 27   // data       -> DIN

I2SClass I2S;

const int   SAMPLE_RATE = 44100;
const float TONE_HZ     = 440.0f;   // A4
float       phase       = 0.0f;
const float phaseInc    = 2.0f * PI * TONE_HZ / SAMPLE_RATE;

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("Stage 1: I2S test tone");

  I2S.setPins(PIN_BCLK, PIN_LRC, PIN_DOUT);   // (bclk, ws, dout)
  if (!I2S.begin(I2S_MODE_STD, SAMPLE_RATE,
                 I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    Serial.println("I2S begin FAILED — check wiring/pins.");
    while (true) delay(100);
  }
  Serial.println("I2S up. You should hear 440 Hz now.");
}

void loop() {
  // Fill a small stereo buffer with a sine wave and push it to the amp.
  const int FRAMES = 256;
  int16_t buf[FRAMES * 2];                 // interleaved L,R,L,R...
  for (int i = 0; i < FRAMES; i++) {
    int16_t s = (int16_t)(sinf(phase) * 8000.0f);  // ~25% volume, gentle
    phase += phaseInc;
    if (phase > 2.0f * PI) phase -= 2.0f * PI;
    buf[2 * i]     = s;   // left
    buf[2 * i + 1] = s;   // right
  }
  // write() blocks until the DMA has room, which paces this loop for us.
  I2S.write((uint8_t *)buf, sizeof(buf));
}
