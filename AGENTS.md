# Agent Guide

## Project Overview

This repository contains Arduino/C++ firmware for the Waveshare ESP32-S3-SIM7670G-4G board. The SIM7670G receives and sends SMS; outbound notification delivery uses ESP32 Wi-Fi, not cellular data. The Web console is embedded HTML, CSS, and JavaScript, not a separately built frontend application.

Read [README.md](README.md) for setup, [tests/README.md](tests/README.md) for verification, and the [reliability guide](sms_forwarder_esp32s3_sim7670g/docs/reliability_security.md) before changing persistence, delivery, credentials, or TLS.

## Repository Map

- [Main sketch](sms_forwarder_esp32s3_sim7670g/sms_forwarder_esp32s3_sim7670g.ino): initialization, main-loop coordination, and scheduled reports.
- [Firmware sources](sms_forwarder_esp32s3_sim7670g/src/):
  - `config_manager.h` / `config_manager.cpp`: configuration schema, defaults, validation, persistence, and recovery.
  - `sim7670g_manager.cpp` / `sms_handler.cpp`: modem state, UART/AT ownership, SMS parsing, and multipart assembly.
  - `sms_storage.cpp` / `retry_manager.cpp`: durable message state and delivery retries.
  - `notification_manager.cpp`: asynchronous jobs, immutable configuration snapshots, and provider delivery results.
  - `tls_client.h` / `http_policy.h` / `http_limits.h`: certificate verification, timeouts, and bounded HTTP reads.
  - `web_server.cpp`: Web/API handlers; `web_pages_full.h`: embedded UI and frontend translations.
  - `i18n.cpp` / `i18n.h`: translated backend logs and API messages.
  - `millis_utils.h`, `input_validation.h`, and `verified_file.h`: shared timing, validation, and persistence helpers.
- [Example filesystem data](sms_forwarder_esp32s3_sim7670g/data/): configuration examples and filesystem provisioning notes, not live device data.
- [Documentation](sms_forwarder_esp32s3_sim7670g/docs/): hardware, localization, modem, and reliability notes.
- [Tests](tests/): Node.js drivers, C++ host tests, fake hardware boundaries, and browser regressions.

## Build

Run commands from the repository root. The documented baseline is Arduino CLI 1.3.0, ESP32 core 3.3.0, ArduinoJson 6.21.5, and Adafruit NeoPixel 1.12.5. Follow the installation commands in [tests/README.md](tests/README.md). A successful build with a newer library does not by itself establish host-suite or runtime compatibility.

```bash
arduino-cli compile \
  --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc,PartitionScheme=custom \
  --build-path /tmp/sms-esp32-build \
  --jobs 4 \
  sms_forwarder_esp32s3_sim7670g
```

- The board uses 16 MiB flash, OPI PSRAM, and the sketch's custom partition table. Each application slot is **3 MiB**; do not use the CLI's possible 16 MiB maximum-size report as the application limit.
- The default pin mapping is for V1 hardware. Check the [V2 pin notes](sms_forwarder_esp32s3_sim7670g/docs/hardware_v2_pin_changes.md) before targeting newer boards.
- Compilation does not upload firmware. Distinguish application images from full flash/filesystem images.
- Uploading filesystem data can overwrite configuration and SMS history. Preserve existing device data before provisioning a filesystem image.

## Tests

Use the relevant existing test files while developing. The full host runner discovers `*.test.js` and `*.test.cpp` automatically, excluding the separate browser suite.

### Host tests

Prerequisites are Node.js, a C++17 compiler, and the ArduinoJson **6.21.5 single-header distribution**. Obtain the header as described in [tests/README.md](tests/README.md).

Example targeted HTTP/TLS test:

```bash
ARDUINOJSON_HEADER=/tmp/ArduinoJson-v6.21.5.h CXX=g++ node tests/notification_http.test.js
```

Full host suite:

```bash
ARDUINOJSON_HEADER=/tmp/ArduinoJson-v6.21.5.h CXX=g++ node tests/run_host_tests.js
```

The storage test requires a **32-bit target** to match the firmware's ArduinoJson capacity assumptions. GCC needs its 32-bit C++ libraries for `-m32`. With Zig, use `CXX=zig CXX_ARGS=c++`; the runner supplies `-target x86-linux-musl` for storage. `STORAGE_CXX_ARGS` overrides those target flags. Do not change production storage limits to accommodate a 64-bit-only host fixture.

Most host tests execute production C++ with fake filesystem, UART, clock, queue, or client boundaries. `pdu_decode.test.js` is a JavaScript decoder mirror, not execution of the firmware decoder.

### Browser tests

After installing Playwright and Chromium using the test guide's temporary dependency location:

```bash
NODE_PATH=/tmp/sms-web-test/node_modules node tests/web_browser.test.js
```

The suite uses mock APIs, checks desktop and mobile layouts, and does not contact a real device or notification provider. UI changes should also cover `web_config.test.js`; configuration/API changes should cover the corresponding persistence and handler tests.

### Hardware verification

Host tests, browser tests, and successful compilation do not establish physical-device correctness. Use the target checklist in [tests/README.md](tests/README.md) for modem ownership, power-loss recovery, battery behavior, and real HTTPS delivery. Report which checks actually ran and which still require a board.

## Behavioral Contracts

### SMS ownership and delivery

- Persist and verify SIM-backed SMS records before deleting their SIM slots. Pending deliveries must not be evicted to make room for history.
- SIM slot `0` is valid. Direct `+CMT` messages have no SIM slot and must not cause slot `0` to be deleted.
- Delivery is at-least-once, not exactly-once. At least one enabled channel must acknowledge success; preserve provider-specific response validation.
- Queued notification jobs retain immutable configuration snapshots. New settings apply to newly admitted work, not jobs already queued.
- Preserve serialized UART/AT ownership and BUSY deferral. Cellular registration, data attachment, and Wi-Fi Internet access are distinct states.

### Configuration and localization

- A new setting may require changes to the C++ schema, defaults, load/save/export paths, validation, Web handlers, form loading/submission, example JSON, and tests. Preserve compatibility with stored configurations and older API clients.
- Reject invalid configuration without partially applying it; preserve existing save-failure rollback and recovery behavior.
- Keep the secret-field Keep/Replace/Clear semantics. Configuration responses must not return saved passwords, tokens, or credential-bearing URLs.
- Maintain both English and Chinese UI/backend translations. Follow the [i18n guide](sms_forwarder_esp32s3_sim7670g/docs/i18n_readme.md), including matching format arguments in both languages.

### HTTPS and diagnostics

- Keep certificate-chain, hostname, and clock validation. Do not add an insecure fallback or replace the public root bundle with a short-lived leaf certificate.
- Private CA selection is scoped to the configured hostname; other hosts use the SDK public roots.
- TLS handshake timeout defaults to 5 seconds and accepts 1-60 whole seconds. TCP/read-idle limits remain 2 seconds; the HTTPS I/O budget is `max(10, handshakeSeconds + 5)` seconds.
- Keep watchdog validation consistent across notification and system settings: at least five seconds above the TLS handshake timeout. Account for queued jobs retaining their old timeout when lowering the watchdog.
- A shorter served chain needs server-side configuration, not firmware changes, when its root is already trusted. See the [short-chain findings](sms_forwarder_esp32s3_sim7670g/docs/reliability_security.md#shorter-trusted-chains-and-root-updates) for the tested X2 chain and exact SDK version.
- HTTP/TLS `-1` is a generic connection failure, not proof of TCP refusal or certificate-validation latency. Preserve bounded diagnostics and the Web log's 200-byte message limit; do not log SMS bodies, response bodies, or credentials.
- Preserve response-size limits and rollover-safe timing checks. Manual network diagnostics make two separate TLS connections and are not a measurement of one handshake.
