// esp32_shiftreg_test — diagnostic ONLY. No AMY, no chords, no sound.
//
// Proves that TWO daisy-chained 74HC165 shift registers are read correctly,
// BEFORE we trust them to decode encoders. Same idea as esp32_encoder_test:
// 10 seconds tells us whether a problem is wiring or software.
//
// It prints the full 16-bit snapshot, but ONLY when a bit changes.
//
// ── Wiring (both chips share LATCH + CLOCK) ──────────────────────────────────
//   74HC165 pin 16 (VCC)      -> 3.3V   (NOT 5V — its Q7 drives an ESP32 pin)
//   74HC165 pin 8  (GND)      -> GND
//   74HC165 pin 15 (CLK INH)  -> GND
//   74HC165 pin 1  (SH/LD)    -> GPIO18  (LATCH)  both chips
//   74HC165 pin 2  (CLK)      -> GPIO19  (CLOCK)  both chips
//   chip A  pin 9  (Q7)       -> GPIO23  (DATA)   <- MCU reads chip A
//   chip A  pin 10 (SER)      -> chip B pin 9 (Q7)   <- chip B feeds chip A
//   chip B  pin 10 (SER)      -> GND                 (end of chain)
//   Every used D input D0..D7 : 10k pull-up to 3.3V, then encoder pin -> D input
//     ('165 has NO internal pull-ups — this is the same gotcha as GPIO34/35.)
//
// WHAT YOU SHOULD SEE:
//   Resting (nothing pressed): every used bit = 1 (pulled up).
//   Turn any encoder  -> its two A/B bits flip 0<->1.
//   Press an encoder  -> its SW bit goes 1 -> 0, back to 1 on release.
//   Silence while turning == wiring (most likely a missing pull-up, the SER
//   chain wrong, or a D input not seated). Bits DO change but stage13 ignores
//   them == decode logic.
//
// Bit index = chip*8 + Dn, so bit 0 = chip A D0 ... bit 15 = chip B D7.

#define PIN_LATCH  18   // SH/LD
#define PIN_CLOCK  19   // CLK
#define PIN_DATA   23   // Q7 of chip A
// Set this to however many chips are actually in the chain right now. You can
// bring the surface up in batches: wire 1, test, raise it, wire the rest.
#define NUM_CHIPS   3

// Read the whole chain. Byte per chip; bit Dn of buf[chip] = raw level of Dn.
static void sr_read(uint8_t *buf) {
  digitalWrite(PIN_LATCH, LOW);       // snapshot all inputs in parallel
  delayMicroseconds(5);
  digitalWrite(PIN_LATCH, HIGH);      // switch to shift mode; Q7 now holds D7

  for (int chip = 0; chip < NUM_CHIPS; chip++) {
    uint8_t b = 0;
    for (int i = 0; i < 8; i++) {
      if (digitalRead(PIN_DATA)) b |= (uint8_t)(1u << (7 - i));  // D7 out first
      digitalWrite(PIN_CLOCK, HIGH);  // rising edge shifts the next bit to Q7
      digitalWrite(PIN_CLOCK, LOW);
    }
    buf[chip] = b;
  }
}

static uint8_t  last[NUM_CHIPS];
static uint32_t changes = 0;

static void print_bits(const uint8_t *buf) {
  for (int chip = 0; chip < NUM_CHIPS; chip++) {
    Serial.printf("%c[D7..D0]=", 'A' + chip);
    for (int i = 7; i >= 0; i--) Serial.print((buf[chip] >> i) & 1);
    Serial.print("  ");
  }
  Serial.printf(" (change #%lu)\n", (unsigned long)changes);
}

void setup() {
  Serial.begin(115200);
  delay(400);
  pinMode(PIN_LATCH, OUTPUT);
  pinMode(PIN_CLOCK, OUTPUT);
  pinMode(PIN_DATA,  INPUT);
  digitalWrite(PIN_LATCH, HIGH);
  digitalWrite(PIN_CLOCK, LOW);

  Serial.println();
  Serial.printf("=== 74HC165 x%d RAW TEST ===\n", NUM_CHIPS);
  Serial.println("Resting: all used bits = 1. Turn an encoder -> its A/B flip.");
  Serial.println("Press a button -> its bit -> 0. Silence == wiring.");
  sr_read(last);
  Serial.print("resting state: ");
  print_bits(last);
  Serial.println("---------------------------");
}

void loop() {
  uint8_t v[NUM_CHIPS];
  sr_read(v);
  if (memcmp(v, last, NUM_CHIPS) != 0) {
    changes++;
    print_bits(v);
    memcpy(last, v, NUM_CHIPS);
  }
}
