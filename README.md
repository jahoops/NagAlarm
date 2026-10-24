# NagAlarm

This workspace now supports both:

- ESP8266 reminder firmware
- ESP32 bench test and ESP-NOW communication test firmware

## Environments

In `platformio.ini`:

- `env:nodemcuv2` (ESP8266 reminder)
- `env:esp32dev-bench` (ESP32 Wi-Fi reliability test)
- `env:esp32dev-now-sender` (ESP32 ESP-NOW sender)
- `env:esp32dev-now-receiver` (ESP32 ESP-NOW receiver)

## Recommended 3-board ESP32 test flow

1. Plug one ESP32 board in.
2. Build and upload bench firmware:
  - `platformio run -e esp32dev-bench -t upload --upload-port COMx`
3. Monitor logs:
  - `platformio device monitor -p COMx -b 115200`
4. Run for 20-30 minutes and note:
  - reconnect count
  - RSSI stability
  - ESP-NOW init status
5. Repeat for all three boards.

After all three pass Wi-Fi stability checks:

1. Pick two boards for communication test.
2. Flash one with sender env and one with receiver env:
  - `platformio run -e esp32dev-now-sender -t upload --upload-port COMx`
  - `platformio run -e esp32dev-now-receiver -t upload --upload-port COMy`
3. Open monitors for both ports.
4. Verify sender shows ACKs and receiver shows ping receipts.

## ESP32 test firmware files

- `src/main_esp32_bench.cpp`
- `src/main_esp32_now_comm.cpp`

## ESP8266 reminder firmware

- `src/main.cpp`

Current reminder timing is set to 15 minutes in `COUNTDOWN_MS`.
