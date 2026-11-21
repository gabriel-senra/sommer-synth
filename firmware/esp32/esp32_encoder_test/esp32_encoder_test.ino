// esp32_encoder_test — diagnostic ONLY. No AMY, no chords, no sound.
//
// Prints the encoder's raw pin states, and only when something changes.
// This tells us in 10 seconds whether the problem is wiring or software.
//
// Wiring being tested:
//   EC11  A  -> GPIO18
//   EC11  C  -> GND          (the MIDDLE pin of the 3-pin side)
//   EC11  B  -> GPIO19
//   EC11  SW -> GPIO23
//   EC11  SW -> GND          (the other pin of the 2-pin side)
//
// WHAT YOU SHOULD SEE:
//   Turning the knob slowly  -> a burst of lines, A and B flipping 0/1
//   Pressing the knob        -> a line with SW=0  (releases back to SW=1)
//
// If turning prints NOTHING -> it's WIRING (most likely the middle pin C not
// actually on GND, or the encoder pins not seated in the breadboard).
// If turning DOES print but Stage 4 ignored it -> it's my decode logic.

#define PIN_ENC_A  18
#define PIN_ENC_B  19
#define PIN_ENC_SW 23

static int lastA = -1, lastB = -1, lastSW = -1;
static uint32_t changes = 0;

void setup() {
  Serial.begin(115200);
  delay(400);
  pinMode(PIN_ENC_A,  INPUT_PULLUP);
  pinMode(PIN_ENC_B,  INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);

  Serial.println();
  Serial.println("=== ENCODER RAW TEST ===");
  Serial.println("Turn the knob slowly. Then press it.");
  Serial.println("Expect: A/B flip 0<->1 as you turn.  SW=0 when pressed.");
  Serial.println("Silence while turning == wiring problem.");
  Serial.printf("resting state: A=%d B=%d SW=%d\n",
                digitalRead(PIN_ENC_A), digitalRead(PIN_ENC_B), digitalRead(PIN_ENC_SW));
  Serial.println("------------------------");
}

void loop() {
  int a  = digitalRead(PIN_ENC_A);
  int b  = digitalRead(PIN_ENC_B);
  int sw = digitalRead(PIN_ENC_SW);

  if (a != lastA || b != lastB || sw != lastSW) {
    changes++;
    Serial.printf("A=%d  B=%d  SW=%d      (change #%lu)\n",
                  a, b, sw, (unsigned long)changes);
    lastA = a; lastB = b; lastSW = sw;
  }
}
