#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP32)

#include <WiFi.h>
#include <esp_now.h>

#ifndef NOW_ROLE
#define NOW_ROLE 1
#endif

constexpr uint8_t ROLE_SENDER = 1;
constexpr uint8_t ROLE_RECEIVER = 2;

constexpr uint8_t LED_PIN = 2;
constexpr bool LED_ACTIVE_LOW = false;
constexpr uint16_t PROTO_MAGIC = 0x4E41; // "NA"
constexpr unsigned long SEND_INTERVAL_MS = 1000;
constexpr unsigned long STATUS_INTERVAL_MS = 2000;
constexpr unsigned long RX_LED_ON_MS = 120;

const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

struct __attribute__((packed)) NowPacket {
  uint16_t magic;
  uint8_t type;    // 1 = ping, 2 = ack
  uint32_t seq;
  uint32_t stamp;
};

uint32_t seqCounter = 0;
unsigned long lastSendMs = 0;
unsigned long lastStatusMs = 0;
unsigned long rxCount = 0;
unsigned long txOkCount = 0;
unsigned long txFailCount = 0;
unsigned long lastRxPulseMs = 0;
bool rxPulseActive = false;

void writeLed(bool on) {
  digitalWrite(LED_PIN, (LED_ACTIVE_LOW ? !on : on) ? HIGH : LOW);
}

void ensurePeer(const uint8_t* mac) {
  if (esp_now_is_peer_exist(mac)) {
    return;
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, mac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
}

void onDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    txOkCount++;
  } else {
    txFailCount++;
  }
}

void onDataRecv(const uint8_t* mac, const uint8_t* incomingData, int len) {
  if (len != (int)sizeof(NowPacket)) {
    return;
  }

  NowPacket packet;
  memcpy(&packet, incomingData, sizeof(packet));
  if (packet.magic != PROTO_MAGIC) {
    return;
  }

  rxCount++;
  lastRxPulseMs = millis();
  rxPulseActive = true;

  if (NOW_ROLE == ROLE_RECEIVER && packet.type == 1) {
    ensurePeer(mac);

    NowPacket ack = {};
    ack.magic = PROTO_MAGIC;
    ack.type = 2;
    ack.seq = packet.seq;
    ack.stamp = millis();

    esp_now_send(mac, reinterpret_cast<const uint8_t*>(&ack), sizeof(ack));
  } else if (NOW_ROLE == ROLE_SENDER && packet.type == 2) {
    const unsigned long now = millis();
    const unsigned long rtt = now - packet.stamp;
    Serial.printf("[ACK] seq=%lu approx-latency=%lu ms from %02X:%02X:%02X:%02X:%02X:%02X\n",
                  (unsigned long)packet.seq,
                  rtt,
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }
}

void sendPing(unsigned long now) {
  NowPacket ping = {};
  ping.magic = PROTO_MAGIC;
  ping.type = 1;
  ping.seq = ++seqCounter;
  ping.stamp = now;

  esp_now_send(BROADCAST_MAC, reinterpret_cast<const uint8_t*>(&ping), sizeof(ping));
}

void updateLed(unsigned long now) {
  if (rxPulseActive && (now - lastRxPulseMs) < RX_LED_ON_MS) {
    writeLed(true);
    return;
  }
  rxPulseActive = false;

  if (NOW_ROLE == ROLE_SENDER) {
    const bool blink = ((now / 400) % 2) == 0;
    writeLed(blink);
  } else {
    const bool blink = ((now / 900) % 2) == 0;
    writeLed(blink);
  }
}

void printStatus(unsigned long now) {
  if ((now - lastStatusMs) < STATUS_INTERVAL_MS) {
    return;
  }
  lastStatusMs = now;

  if (NOW_ROLE == ROLE_SENDER) {
    Serial.printf("[SENDER] sent=%lu rxAck=%lu txOK=%lu txFail=%lu\n",
                  (unsigned long)seqCounter,
                  rxCount,
                  txOkCount,
                  txFailCount);
  } else {
    Serial.printf("[RECEIVER] rxPing=%lu txOK=%lu txFail=%lu\n",
                  rxCount,
                  txOkCount,
                  txFailCount);
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(LED_PIN, OUTPUT);
  writeLed(false);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (true) {
      writeLed(true);
      delay(50);
      writeLed(false);
      delay(200);
    }
  }

  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);

  ensurePeer(BROADCAST_MAC);

  Serial.printf("ESP-NOW role: %s\n", (NOW_ROLE == ROLE_SENDER) ? "SENDER" : "RECEIVER");
  Serial.printf("Local MAC: %s\n", WiFi.macAddress().c_str());
}

void loop() {
  const unsigned long now = millis();

  if (NOW_ROLE == ROLE_SENDER && (now - lastSendMs) >= SEND_INTERVAL_MS) {
    lastSendMs = now;
    sendPing(now);
  }

  updateLed(now);
  printStatus(now);
}

#endif
