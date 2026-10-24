#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP8266)

// Pins (ESP8266 NodeMCU-style defaults; adjust as needed)
constexpr uint8_t BUTTON_PIN = D3;      // Active-low button to GND
constexpr uint8_t LED_PIN = LED_BUILTIN;
constexpr uint8_t BUZZER_PIN = D5;

// Logic levels
constexpr bool BUTTON_ACTIVE_LOW = true;
constexpr bool LED_ACTIVE_LOW = true;
constexpr bool BUZZER_ACTIVE_HIGH = true;

// Timings
constexpr unsigned long COUNTDOWN_MS = 15UL * 60UL * 1000UL;
constexpr unsigned long LONG_PRESS_MS = 2000;
constexpr unsigned long DEBOUNCE_MS = 30;
constexpr unsigned long LED_ARM_PERIOD_MS = 2000;
constexpr unsigned long LED_ARM_PULSE_MS = 80;
constexpr unsigned long LED_ALARM_BLINK_MS = 120;
constexpr unsigned long BUZZER_BEEP_PERIOD_MS = 500;
constexpr unsigned long BUZZER_BEEP_ON_MS = 120;
constexpr unsigned long ACK_PULSE_ON_MS = 90;
constexpr unsigned long ACK_PULSE_OFF_MS = 90;
constexpr uint8_t ACK_PULSE_COUNT = 3;

// Temporary hardware test: press D3 button -> buzzer on.
constexpr bool TEST_BUTTON_TO_BUZZER = false;

enum State {
  IDLE,
  ARMED,
  ALARMING
};

State state = IDLE;
unsigned long countdownDeadlineMs = 0;

// Debounce + press tracking
bool lastRawPressed = false;
bool stablePressed = false;
unsigned long lastRawChangeMs = 0;
unsigned long pressStartMs = 0;
bool longPressHandled = false;
bool ackActive = false;
unsigned long ackStartMs = 0;

void writeLed(bool on) {
  digitalWrite(LED_PIN, (LED_ACTIVE_LOW ? !on : on) ? HIGH : LOW);
}

void writeBuzzer(bool on) {
  digitalWrite(BUZZER_PIN, (BUZZER_ACTIVE_HIGH ? on : !on) ? HIGH : LOW);
}

void armFromNow(unsigned long now) {
  state = ARMED;
  countdownDeadlineMs = now + COUNTDOWN_MS;
}

void startAck(unsigned long now) {
  ackActive = true;
  ackStartMs = now;
}

void goIdle() {
  state = IDLE;
  countdownDeadlineMs = 0;
}

void onShortPress(unsigned long now) {
  const State previousState = state;
  if (state == IDLE || state == ARMED || state == ALARMING) {
    armFromNow(now);
    if (previousState == IDLE) {
      startAck(now);
    }
  }
}

void onLongPress(unsigned long now) {
  if (state != IDLE) {
    startAck(now);
  }
  goIdle();
}

void updateButton(unsigned long now) {
  const bool rawPressed = (digitalRead(BUTTON_PIN) == (BUTTON_ACTIVE_LOW ? LOW : HIGH));

  if (rawPressed != lastRawPressed) {
    lastRawPressed = rawPressed;
    lastRawChangeMs = now;
  }

  if ((now - lastRawChangeMs) < DEBOUNCE_MS) {
    return;
  }

  if (rawPressed != stablePressed) {
    stablePressed = rawPressed;
    if (stablePressed) {
      pressStartMs = now;
      longPressHandled = false;
    } else if (!longPressHandled) {
      onShortPress(now);
    }
  }

  if (stablePressed && !longPressHandled && (now - pressStartMs >= LONG_PRESS_MS)) {
    longPressHandled = true;
    onLongPress(now);
  }
}

void updateState(unsigned long now) {
  if (state == ARMED && now >= countdownDeadlineMs) {
    state = ALARMING;
  }
}

void updateOutputs(unsigned long now) {
  bool ledOn = false;
  bool buzzerOn = false;

  if (ackActive) {
    const unsigned long pulsePeriodMs = ACK_PULSE_ON_MS + ACK_PULSE_OFF_MS;
    const unsigned long ackDurationMs = pulsePeriodMs * ACK_PULSE_COUNT;
    const unsigned long elapsedMs = now - ackStartMs;

    if (elapsedMs < ackDurationMs) {
      const unsigned long inPulseMs = elapsedMs % pulsePeriodMs;
      const bool pulseOn = inPulseMs < ACK_PULSE_ON_MS;
      ledOn = pulseOn;
      buzzerOn = pulseOn;
    } else {
      ackActive = false;
    }
  }

  if (!ackActive) {
    if (state == ARMED) {
      ledOn = (now % LED_ARM_PERIOD_MS) < LED_ARM_PULSE_MS;
    } else if (state == ALARMING) {
      ledOn = ((now / LED_ALARM_BLINK_MS) % 2) == 0;
      buzzerOn = (now % BUZZER_BEEP_PERIOD_MS) < BUZZER_BEEP_ON_MS;
    }
  }

  writeLed(ledOn);
  writeBuzzer(buzzerOn);
}

void setup() {
  if (TEST_BUTTON_TO_BUZZER) {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
  } else {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
  }
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  writeLed(false);
  writeBuzzer(false);
}

void loop() {
  const unsigned long now = millis();

  if (TEST_BUTTON_TO_BUZZER) {
    const bool pressed = (digitalRead(BUTTON_PIN) == (BUTTON_ACTIVE_LOW ? LOW : HIGH));
    writeLed(false);
    writeBuzzer(pressed);
    return;
  }

  updateButton(now);
  updateState(now);
  updateOutputs(now);
}

#endif
