// esp32_encoder_finder — works out which EC11 leg is COMMON, and proves the
// encoder is making electrical contact at all. No AMY, no sound.
//
// ── WIRE IT LIKE THIS (different from before!) ──
// Connect the THREE legs on the 3-pin edge of the encoder to these GPIOs.
// Do NOT connect GND to the encoder at all — this sketch supplies it.
//    leg 1 (one outer)  -> GPIO18
//    leg 2 (the middle) -> GPIO19
//    leg 3 (other outer)-> GPIO4
// Leave the 2-pin switch edge disconnected for now.
//
// ── HOW IT WORKS ──
// It rotates through the 3 legs. For each one it drives that leg LOW (pretending
// it's ground) and watches the other two with pull-ups. Turning the shaft should
// make the other two toggle ONLY when the real COMMON leg is the one driven low.
//
// ── WHAT TO DO ──
// Turn the shaft slowly and continuously for ~15 seconds while this runs.
// Then read the summary it prints. Whichever leg reports lots of activity is
// your COMMON pin -> that's the one that goes to GND in the real sketch.
//
// If NO leg reports any activity, the encoder isn't making contact at all
// (pins not seated / mounting tabs holding it up) -- not a wiring-order issue.

const int LEG[3]        = { 18, 19, 4 };
const char *LEG_NAME[3] = { "GPIO18 (outer 1)", "GPIO19 (MIDDLE)", "GPIO4  (outer 2)" };

static uint32_t activity[3] = { 0, 0, 0 };
static int      which       = 0;      // which leg we're currently driving low
static uint32_t phase_start = 0;
static int      lastX = -1, lastY = -1;

#define PHASE_MS 5000                 // test each leg for 5s

static void start_phase(int w) {
  which = w;
  // Drive the candidate common leg LOW; the other two become pulled-up inputs.
  for (int i = 0; i < 3; i++) {
    if (i == w) { pinMode(LEG[i], OUTPUT); digitalWrite(LEG[i], LOW); }
    else        { pinMode(LEG[i], INPUT_PULLUP); }
  }
  lastX = lastY = -1;
  phase_start = millis();
  Serial.printf("\n--- now testing: %s  as COMMON --- (keep turning!)\n", LEG_NAME[w]);
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\n=== EC11 COMMON-PIN FINDER ===");
  Serial.println("Wire the 3-pin edge to GPIO18, GPIO19, GPIO4. No GND wire.");
  Serial.println("TURN THE SHAFT CONTINUOUSLY for the next 15 seconds.");
  start_phase(0);
}

void loop() {
  // the two legs we're watching this phase
  int a = -1, b = -1;
  for (int i = 0; i < 3; i++) if (i != which) { if (a < 0) a = i; else b = i; }

  int x = digitalRead(LEG[a]);
  int y = digitalRead(LEG[b]);
  if (x != lastX || y != lastY) {
    if (lastX >= 0) activity[which]++;      // ignore the first read
    lastX = x; lastY = y;
  }

  if (millis() - phase_start > PHASE_MS) {
    Serial.printf("    %s -> %lu changes\n", LEG_NAME[which], (unsigned long)activity[which]);
    if (which == 2) {
      // Summary
      int best = 0;
      for (int i = 1; i < 3; i++) if (activity[i] > activity[best]) best = i;
      Serial.println("\n================ RESULT ================");
      for (int i = 0; i < 3; i++)
        Serial.printf("  %s : %lu changes\n", LEG_NAME[i], (unsigned long)activity[i]);
      if (activity[best] < 5) {
        Serial.println("  >> NO ACTIVITY ANYWHERE.");
        Serial.println("  >> The encoder isn't making contact. Check that the pins are");
        Serial.println("  >> fully seated -- bend/snip the metal mounting tabs, they hold");
        Serial.println("  >> the legs off the breadboard contacts.");
      } else {
        Serial.printf("  >> COMMON leg is: %s\n", LEG_NAME[best]);
        Serial.println("  >> Wire THAT leg to GND; the other two are A and B.");
      }
      Serial.println("========================================");
      Serial.println("(restarting test — keep turning if you want another pass)");
      for (int i = 0; i < 3; i++) activity[i] = 0;
      start_phase(0);
    } else {
      start_phase(which + 1);
    }
  }
}
