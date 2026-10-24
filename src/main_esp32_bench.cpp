#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP32)

#include <WiFi.h>
#include <esp_now.h>

// Update these before testing.
constexpr char WIFI_SSID[] = "YOUR_WIFI_SSID";
constexpr char WIFI_PASS[] = "YOUR_WIFI_PASSWORD";

// ESP32 dev-board defaults; change if needed.
constexpr uint8_t LED_PIN = 2;
constexpr bool LED_ACTIVE_LOW = false;

constexpr unsigned long RECONNECT_INTERVAL_MS = 5000;
constexpr unsigned long STATUS_PRINT_INTERVAL_MS = 2000;
constexpr unsigned long SCAN_INTERVAL_MS = 60000;
constexpr unsigned long HEARTBEAT_HALF_PERIOD_MS = 500;

unsigned long lastReconnectMs = 0;
unsigned long lastStatusPrintMs = 0;
unsigned long lastScanMs = 0;
unsigned long disconnectCount = 0;
unsigned long reconnectAttemptCount = 0;
wl_status_t lastStatus = WL_IDLE_STATUS;
bool espNowReady = false;

void writeLed(bool on) {
  digitalWrite(LED_PIN, (LED_ACTIVE_LOW ? !on : on) ? HIGH : LOW);
}

void connectWiFi() {
  reconnectAttemptCount++;
  Serial.printf("[WiFi] Connect attempt %lu to %s\n", reconnectAttemptCount, WIFI_SSID);
  WiFi.disconnect(true, true);
  delay(20);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void ensureEspNowInit() {
  if (espNowReady || WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (esp_now_init() == ESP_OK) {
    espNowReady = true;
    Serial.println("[ESP-NOW] init OK");
  } else {
    Serial.println("[ESP-NOW] init FAILED");
  }
}

void printStatus(unsigned long now) {
  if ((now - lastStatusPrintMs) < STATUS_PRINT_INTERVAL_MS) {
    return;
  }
  lastStatusPrintMs = now;

  const wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    Serial.printf("[WiFi] CONNECTED  RSSI=%d dBm  IP=%s  disconnects=%lu\n",
                  WiFi.RSSI(),
                  WiFi.localIP().toString().c_str(),
                  disconnectCount);
  } else {
    Serial.printf("[WiFi] status=%d  disconnects=%lu\n", (int)status, disconnectCount);
  }
}

void maybeScan(unsigned long now) {
  if ((now - lastScanMs) < SCAN_INTERVAL_MS || WiFi.status() != WL_CONNECTED) {
    return;
  }
  lastScanMs = now;

  Serial.println("[WiFi] scanning nearby APs...");
  const int found = WiFi.scanNetworks(false, true);
  Serial.printf("[WiFi] scan done: %d AP(s)\n", found);
  WiFi.scanDelete();
}

void updateLed(unsigned long now) {
  if (WiFi.status() == WL_CONNECTED) {
    writeLed(true);
    return;
  }

  const bool heartbeatOn = ((now / HEARTBEAT_HALF_PERIOD_MS) % 2) == 0;
  writeLed(heartbeatOn);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(LED_PIN, OUTPUT);
  writeLed(false);

  WiFi.mode(WIFI_STA);
  connectWiFi();

  Serial.println("ESP32 Wi-Fi bench test running");
  Serial.println("- LED solid on when connected");
  Serial.println("- LED heartbeat blink when disconnected");
  Serial.println("- Serial logs RSSI, reconnects, scans, and ESP-NOW init status");
}

void loop() {
  const unsigned long now = millis();

  const wl_status_t status = WiFi.status();
  if (status != lastStatus) {
    if (status != WL_CONNECTED) {
      disconnectCount++;
      espNowReady = false;
      Serial.printf("[WiFi] link lost (status=%d)\n", (int)status);
    } else {
      Serial.println("[WiFi] link up");
    }
    lastStatus = status;
  }

  if (status != WL_CONNECTED && (now - lastReconnectMs) >= RECONNECT_INTERVAL_MS) {
    lastReconnectMs = now;
    connectWiFi();
  }

  ensureEspNowInit();
  printStatus(now);
  maybeScan(now);
  updateLed(now);
}

#endif
