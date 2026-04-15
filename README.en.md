# SMS Forwarder - ESP32-S3 + SIM7670G

English | [中文](README.md)

SMS forwarder built for the Waveshare ESP32-S3-SIM7670G-4G module. It receives SMS and forwards them to multiple notification platforms, provides a web UI, and includes battery/network management.

Docs:
- [Operator Table Maintenance](sms_forwarder_esp32s3_sim7670g/docs/operator_readme.md)
- [PDU Decode Tests](sms_forwarder_esp32s3_sim7670g/docs/pdu_decode_tests.md)
- [i18n Maintenance Guide](sms_forwarder_esp32s3_sim7670g/docs/i18n_readme.md)

## Highlights
1. SMS receive/forward with PDU parsing and long-SMS reassembly across GSM 7-bit, UCS2, and 8-bit text.
2. Multi-channel push: Bark, ServerChan, DingTalk, Telegram, Feishu, custom webhook.
3. Embedded web UI for status, config, logs, SMS management, and debugging tools.
4. Battery management with MAX17048 monitoring and alerts.
5. Network diagnostics, roaming management, and auto-reconnect WiFi.
6. Bilingual UI and logs (Chinese/English) with a top-bar language switch.
7. Local PDU regression coverage now includes full SMS-DELIVER samples, 2/3/4-part concatenation, Simplified Chinese, Traditional Chinese, Japanese, Russian, Arabic, and emoji-mixed content.

## Hardware
1. Waveshare ESP32-S3-SIM7670G-4G module
2. Nano-SIM (4G supported)
3. 18650 battery (optional)
4. LTE antenna (recommended)

## Quick Start
1. Open `sms_forwarder_esp32s3_sim7670g.ino` in Arduino IDE 2.0+.
2. Select board `ESP32S3 Dev Module`.
3. Set Flash size to 16MB and select a partition scheme with SPIFFS.
4. Build and upload.
5. On first boot, connect to AP `SMS-Forwarder-Setup` (password `12345678`).
6. Visit `http://192.168.4.1` and configure WiFi and notification channels.
   Default Web auth username: `admin`
   Default Web auth password: `admin1234`

## Web UI
1. Dashboard: battery, SIM/network status, signal, memory, LED status.
2. Config: WiFi, notifications, battery alerts, network settings, SMS filters, system options.
3. SMS: list, forward, delete, send, and statistics.
4. Logs: view and clear device logs.
5. Debug: AT testing, WiFi/network diagnostics, LED tests.

## WiFi DNS
1. `Use Custom DNS`: apply user-defined DNS servers.
2. `Use Static IP`: optional; required only when you want to force static IP + DNS together.
3. If DNS still shows gateway DNS, check logs for `wifi_dns_after_force` and `wifi_dns_after_reconnect`.

## Language
1. The top bar provides Chinese/English toggle.
2. Default language follows the browser on first load.
3. Logs follow the UI language at the time they are generated.
4. Full i18n workflow (key naming, placeholders, testing) is documented in `sms_forwarder_esp32s3_sim7670g/docs/i18n_readme.md`.
5. This decoder update does not introduce new user-facing i18n keys. Existing log/UI translations still cover the new behavior; `Unknown` remains an internal sentinel and is localized at the display layer.

## Operator Table
See `sms_forwarder_esp32s3_sim7670g/docs/operator_readme.md` for adding/removing operators and maintaining MCC/MNC mappings.

## Recent Fixes (v2.5.0)
1. Fixed sender decode for alphanumeric SMS originators (for example, `giffgaff` no longer appears as garbled text).
2. Fixed GSM 7-bit long-SMS UDH fill-bit handling so concatenated English SMS no longer decode into `Ψ/£/¥/Γ`-style garbage.
3. Improved SMS body fallback between GSM 7-bit and UCS2 for multilingual content, including Traditional Chinese, Russian, Arabic, and emoji-containing text.
4. UCS2 decode now handles UTF-16 surrogate pairs correctly, so emoji and other supplementary-plane characters are preserved.
5. 8-bit payload decode now prefers readable Windows-1252/Latin-1 output instead of collapsing most bytes to `.`.
6. Local PDU tests were expanded to 23 cases with full SMS-DELIVER samples and 2/3/4-part concatenation coverage.
7. Reduced UI lag by lowering loop blocking delay and throttling status polling in the main loop.
8. Fixed custom DNS not taking effect due to variable shadowing in WiFi DNS apply flow.
