# SMS Forwarder - ESP32-S3 + SIM7670G

English | [中文](README.zh.md)

> Important hardware note: this firmware uses the V1 pin mapping by default. If your ESP32-S3-SIM7670G-4G module was purchased or received in 2026 or later, check whether it is a V2 board before flashing. For V2 hardware, update the affected pins first, especially the MAX17048 I2C pins, using [Hardware V2 Pin Notes](sms_forwarder_esp32s3_sim7670g/docs/hardware_v2_pin_changes.md).

Firmware for the Waveshare ESP32-S3-SIM7670G-4G module. It receives SMS through the SIM7670G modem, stores recent messages locally, and forwards them through WiFi-based notification channels from a built-in web console.

## Overview

- Receive SMS in PDU mode and decode GSM 7-bit, UCS2, and 8-bit text.
- Reassemble long SMS fragments before storing or forwarding them.
- Forward messages to Bark, ServerChan, DingTalk, Telegram, Feishu, or a custom webhook.
- Manage WiFi, notification channels, SMS filters, logs, and diagnostics from the web UI.
- Monitor battery status with MAX17048 when the battery circuit is available.
- Provide bilingual UI and logs in English and Chinese.

## Documentation

| Topic | Link |
| --- | --- |
| Waveshare 2026/V2 hardware pin changes | [Hardware V2 Pin Notes](sms_forwarder_esp32s3_sim7670g/docs/hardware_v2_pin_changes.md) |
| PDU decoder test coverage | [PDU Decode Tests](sms_forwarder_esp32s3_sim7670g/docs/pdu_decode_tests.md) |
| Operator MCC/MNC table | [Operator Table Maintenance](sms_forwarder_esp32s3_sim7670g/docs/operator_readme.md) |
| UI/log translation maintenance | [i18n Maintenance Guide](sms_forwarder_esp32s3_sim7670g/docs/i18n_readme.md) |

## Hardware

- Waveshare ESP32-S3-SIM7670G-4G module
- Nano-SIM card with 4G service
- LTE antenna, recommended for reliable reception
- 18650 battery, optional

Important: Waveshare notes that modules received after 2026-01-01 should use the V2 examples. V2 boards change some peripheral pin assignments, especially the MAX17048 I2C pins used for battery monitoring. Check [Hardware V2 Pin Notes](sms_forwarder_esp32s3_sim7670g/docs/hardware_v2_pin_changes.md) before building for newer modules.

## Build And Flash

1. Open [sms_forwarder_esp32s3_sim7670g/sms_forwarder_esp32s3_sim7670g.ino](sms_forwarder_esp32s3_sim7670g/sms_forwarder_esp32s3_sim7670g.ino) in Arduino IDE 2.x.
2. Install the ESP32 board package and required libraries such as ArduinoJson.
3. Select `ESP32S3 Dev Module`.
4. Use 16 MB flash and a partition scheme with filesystem space.
5. Build and upload the firmware over USB.

## First Boot

1. Power on the device after flashing.
2. Connect to WiFi AP `SMS-Forwarder-Setup` with password `12345678`.
3. Open `http://192.168.4.1` in a browser.
4. Sign in with username `admin` and password `admin1234`.
5. Configure local WiFi and at least one notification channel.
6. Reboot or wait for the device to reconnect using the saved settings.

## Web Console

- Dashboard: battery, SIM registration, signal, memory, and device status.
- Config: WiFi, notification channels, battery alerts, network options, SMS filters, and system settings.
- SMS: message list, manual forwarding, deletion, sending, and statistics.
- Logs: recent runtime logs and clearing tools.
- Debug: AT command testing, WiFi/network diagnostics, and LED tests.

## Notes

- SMS receive/send uses the SIM7670G modem. Notification delivery uses ESP32 WiFi.
- Some network configuration AT commands may return `ERROR` or `+CME ERROR` with certain SIM cards, carriers, roaming states, or modem firmware. The firmware retries and skips these commands when needed; this normally does not block SMS forwarding.
- Use `networkConnected` to check cellular registration and `dataAttached` to check cellular data attachment.
- Custom DNS can be configured from the web UI. Static IP is only needed when you want to force static IP and DNS together.
- SMS records and logs are stored locally with bounded retention to protect flash and memory.

## Repository Layout

```text
.
|-- README.md
|-- README.zh.md
|-- sms_forwarder_esp32s3_sim7670g/
|   |-- sms_forwarder_esp32s3_sim7670g.ino
|   |-- data/
|   |-- docs/
|   `-- src/
`-- tests/
```

## Tests

Run the local PDU decoder tests from the repository root:

```bash
node tests/pdu_decode.test.js
g++ -std=c++11 -Wall -Wextra -pedantic tests/millis_utils.test.cpp -o /tmp/millis_utils_test
/tmp/millis_utils_test
```

Arduino compilation still requires a local Arduino IDE or `arduino-cli` environment with the ESP32 board package installed.