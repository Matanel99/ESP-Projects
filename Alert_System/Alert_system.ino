#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// -------------------- Pin Mapping --------------------
const int LED_PIN  = 13;   // External LED
const int BTN_PIN  = 14;   // Button to GND (INPUT_PULLUP)
const int PIR_PIN  = 27;   // HC-SR501 OUT
const int TRIG_PIN = 26;   // HC-SR04 TRIG
const int ECHO_PIN = 25;   // HC-SR04 ECHO (through voltage divider)

// -------------------- OLED --------------------
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define SCREEN_ADDRESS  0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// -------------------- Button Debounce --------------------
int lastBtn = HIGH;
unsigned long lastChangeMs = 0;
const unsigned long debounceMs = 30;

// -------------------- Ultrasonic Settings --------------------
const unsigned long ECHO_TIMEOUT_US = 30000;  // 30ms timeout
const unsigned long ULTRA_PERIOD_MS = 200;    // measure every 200ms
const float THRESHOLD_CM = 30.0f;             // alert threshold

// -------------------- PIR / Motion --------------------
int pirPrev = LOW;                             // for rising edge debug (optional)
unsigned long lastMotionMs = 0;                // last time PIR was HIGH (or we saw motion)
const unsigned long MOTION_HOLD_MS = 1500;     // keep "motion relevant" for 1.5s

// -------------------- System State --------------------
enum SystemState {
  DISARMED,
  ARMED,
  ALERT
};

SystemState state = DISARMED;

// -------------------- Shared Runtime Data --------------------
float distanceCm = 999.0f;
unsigned long lastUltraMs = 0;

unsigned long lastBlinkMs = 0;
const unsigned long BLINK_PERIOD_MS = 200;

unsigned long lastTimeoutPrintMs = 0;
const unsigned long TIMEOUT_PRINT_PERIOD_MS = 1000;

unsigned long lastUIupdateMs = 0;
const unsigned long UI_UPDATE_MS = 500;  // (you raised it) reduce OLED load

bool motionRelevant = false; // this is what we will use for ALERT condition

// ============================================================
// Button: returns true exactly once per press (falling edge)
// ============================================================
bool checkButtonPressedEvent() {
  int btn = digitalRead(BTN_PIN);

  if (btn != lastBtn) {
    lastChangeMs = millis();
    lastBtn = btn;
  }

  if (millis() - lastChangeMs > debounceMs) {
    static int stableBtn = HIGH;

    if (btn != stableBtn) {
      stableBtn = btn;
      if (stableBtn == LOW) return true; // INPUT_PULLUP => pressed LOW
    }
  }
  return false;
}

// ============================================================
// PIR: returns true only on rising edge (debug / optional)
// ============================================================
bool pirMotionStartedEvent() {
  int pirNow = digitalRead(PIR_PIN);
  bool started = (pirNow == HIGH && pirPrev == LOW);
  pirPrev = pirNow;
  return started;
}

// ============================================================
// PIR: update "motionRelevant" window (this fixes your logic issue)
// - If PIR is HIGH now => motionRelevant true, refresh timer
// - If PIR is LOW for > MOTION_HOLD_MS => motionRelevant false
// ============================================================
void updateMotionRelevant() {
  int pirNow = digitalRead(PIR_PIN);

  if (pirNow == HIGH) {
    lastMotionMs = millis();
    motionRelevant = true;
  } else {
    if (motionRelevant && (millis() - lastMotionMs > MOTION_HOLD_MS)) {
      motionRelevant = false;
    }
  }
}

// ============================================================
// HC-SR04 trigger pulse
// ============================================================
void triggerPulse() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);
}

// ============================================================
// Measure ECHO HIGH time in microseconds, returns 0 on timeout
// Also waits for ECHO to be LOW before starting (clean start).
// ============================================================
unsigned long echoTimeUs() {
  unsigned long tClean = micros();
  while (digitalRead(ECHO_PIN) == HIGH) {
    if (micros() - tClean > 2000) break;
  }

  unsigned long t0 = micros();
  while (digitalRead(ECHO_PIN) == LOW) {
    if (micros() - t0 > ECHO_TIMEOUT_US) return 0;
  }

  unsigned long start = micros();
  while (digitalRead(ECHO_PIN) == HIGH) {
    if (micros() - start > ECHO_TIMEOUT_US) return 0;
  }

  return micros() - start;
}

// ============================================================
// Update distance periodically (non-blocking schedule)
// ============================================================
void updateDistance() {
  if (millis() - lastUltraMs < ULTRA_PERIOD_MS) return;
  lastUltraMs = millis();

  triggerPulse();
  unsigned long t_us = echoTimeUs();

  if (t_us == 0) {
    distanceCm = 999.0f;

    if (millis() - lastTimeoutPrintMs >= TIMEOUT_PRINT_PERIOD_MS) {
      lastTimeoutPrintMs = millis();
      Serial.println("Ultrasonic: timeout / out of range");
    }
  } else {
    distanceCm = t_us / 58.0f;
  }
}

// ============================================================
// LED behavior by state
// ============================================================
void updateLedByState() {
  if (state == DISARMED) {
    digitalWrite(LED_PIN, LOW);
    return;
  }

  if (state == ARMED) {
    digitalWrite(LED_PIN, HIGH);
    return;
  }

  // ALERT: blink without delay
  if (millis() - lastBlinkMs >= BLINK_PERIOD_MS) {
    lastBlinkMs = millis();
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
}

// ============================================================
// OLED UI update (rate-limited)
// ============================================================
void updateUI() {
  if (millis() - lastUIupdateMs < UI_UPDATE_MS) return;
  lastUIupdateMs = millis();

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  if (state == DISARMED) {
    display.setTextSize(2);
    display.print("DISARMED\n");
    display.setTextSize(1);
    display.print("Press to ARM\n");
    display.display();
    return;
  }

  if (state == ARMED) {
    display.setTextSize(2);
    display.print("ARMED\n");
  } else { // ALERT
    display.setTextSize(2);
    display.print("ALERT!!\n");
  }

  display.setTextSize(1);
  display.print("Dist: ");
  display.print(distanceCm, 1);
  display.println(" cm");

  display.print("Motion: ");
  display.println(motionRelevant ? "YES" : "no");

  display.display();
}

// ============================================================
// Setup
// ============================================================
void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);

  pinMode(PIR_PIN, INPUT);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  digitalWrite(TRIG_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;) {}
  }

  Serial.println("Boot OK. System ready.");
}

// ============================================================
// Main loop
// ============================================================
void loop() {
  // 1) Update inputs / sensors
  bool pressed = checkButtonPressedEvent();

  // (Optional) keep edge event for debug prints if you want:
  bool motionStarted = pirMotionStartedEvent(); // not used for ALERT decision anymore
  if (motionStarted) Serial.println("PIR: motion started (edge)");

  updateMotionRelevant();  // <-- IMPORTANT FIX
  updateDistance();

  // 2) State machine transitions
  switch (state) {
    case DISARMED:
      if (pressed) {
        state = ARMED;
        Serial.println("State -> ARMED");
      }
      break;

    case ARMED:
      if (pressed) {
        state = DISARMED;
        Serial.println("State -> DISARMED");
        break;
      }

      // IMPORTANT FIX:
      // Use motionRelevant (window / level-like behavior) instead of the one-shot edge
      if (motionRelevant && distanceCm < THRESHOLD_CM) {
        state = ALERT;
        Serial.print("State -> ALERT | distance_cm=");
        Serial.println(distanceCm, 1);
      }
      break;

    case ALERT:
      if (pressed) {
        state = DISARMED;
        Serial.println("State -> DISARMED");
      }
      break;
  }

  // 3) Outputs
  updateLedByState();
  updateUI();
}
