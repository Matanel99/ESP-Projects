// ESP32 DevKit - Button + LED baseline

const int LED_PIN = 13;      // אם לא עובד אצלך, נגלה ונשנה
const int BTN_PIN = 14;     // כפתור ל-GND, משתמשים ב-INPUT_PULLUP

int lastBtn = HIGH;
unsigned long lastChangeMs = 0;
const unsigned long debounceMs = 30;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);

  digitalWrite(LED_PIN, LOW);
  Serial.println("Boot OK. Press the button...");
}

void loop() {
  int btn = digitalRead(BTN_PIN);

  // Debounce: מזהים שינוי יציב
  if (btn != lastBtn) {
    lastChangeMs = millis();
    lastBtn = btn;
  }

  if (millis() - lastChangeMs > debounceMs) {
    // כפתור לחוץ = LOW (כי PULLUP)
    static int stableBtn = HIGH;
    if (btn != stableBtn) {
      stableBtn = btn;

      if (stableBtn == LOW) {
        Serial.println("Button pressed");
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      } else {
        Serial.println("Button released");
      }
    }
  }
}
