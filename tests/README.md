# Verification

Run commands from the repository root. Host checks exercise production C++ functions/modules with fake filesystem, UART, clock, queue and battery boundaries. The pre-existing PDU JavaScript suite is a decoder mirror, not execution of the firmware binary.

## Host Suite

Requirements: Node.js, a C++17 compiler, and the ArduinoJson v6.21.5 single-header distribution. Node 24.12.0 and Zig 0.13.0 were used during this repair.

```bash
curl -fL https://github.com/bblanchon/ArduinoJson/releases/download/v6.21.5/ArduinoJson-v6.21.5.h -o /tmp/ArduinoJson-v6.21.5.h
ARDUINOJSON_HEADER=/tmp/ArduinoJson-v6.21.5.h CXX=zig CXX_ARGS=c++ node tests/run_host_tests.js
```

With GCC, use `CXX=g++` and omit `CXX_ARGS`. The storage test specifically needs a 32-bit target because the production ArduinoJson capacity heuristics assume ESP32-sized slots. The runner selects `-target x86-linux-musl` for Zig and `-m32` for other compilers; GCC therefore needs its 32-bit C++ libraries. `STORAGE_CXX_ARGS` can override those target flags. Do not interpret a native 64-bit JSON-capacity failure as an ESP32 storage result.

The suite covers durable admission before SIM deletion; pending-only capacity exhaustion; short writes, corrupted readback and failed renames; multipart ordering/collisions; AT detailed errors/interleaving/final responses; provider JSON contracts; immutable job configuration; retry restoration and failed status commits; report markers; strict IDs/UTF-8/UCS2 limits; network unknown state; PAP command order; HTTP limits; bootstrap recovery; secret update semantics; and Web configuration round trips. The runner discovers new `*.test.cpp` and `*.test.js` files automatically, except the separate browser suite.

## Browser Suite

This uses mock HTTP APIs and never contacts a real notification provider or device. It checks 1440x1000 desktop and 390x844 mobile viewports, credential Keep/Replace/Clear behavior using actual browser FormData, all-disabled toggles, zero-valued settings, six-channel asynchronous results and horizontal overflow. Screenshots are written under a temporary directory printed by the test.

```bash
npm install --prefix /tmp/sms-web-test --no-audit --no-fund playwright
/tmp/sms-web-test/node_modules/.bin/playwright install chromium
NODE_PATH=/tmp/sms-web-test/node_modules node tests/web_browser.test.js
```

Chromium requires the normal Linux browser shared libraries. Browser tests do not establish correctness of the C++ WebServer on a physical board.

## Firmware Build

The verified toolchain is Arduino CLI 1.3.0 with ESP32 core 3.3.0, ArduinoJson 6.21.5 and Adafruit NeoPixel 1.12.5. The build uses the sketch's existing 3 MiB application slots and 16 MiB flash partition table.

```bash
arduino-cli core install esp32:esp32@3.3.0 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib install 'ArduinoJson@6.21.5' 'Adafruit NeoPixel@1.12.5'
arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc,PartitionScheme=custom --build-path /tmp/sms-esp32-build --jobs 4 sms_forwarder_esp32s3_sim7670g
```

The custom-partition CLI size report may display 16 MiB as the maximum; the actual app slot is 3 MiB. Check the binary against the slot size, not that printed total. No upload command is run by these tests. Check the V1/V2 hardware notes and actual PSRAM/USB wiring before flashing.

## Target Checklist

- Receive a normal SMS and a batch/live split multipart SMS; interrupt power before/after storage commit and SIM deletion. Confirm pending work restores without silent loss; duplicates are possible.
- Fill all 50 pending records with WiFi unavailable. Confirm the next message remains on SIM and is eventually re-scanned after capacity becomes available.
- Inject or observe `+CMS ERROR`, `+CME ERROR`, missing/late `OK`, concurrent CMTI and delayed `+CMGS` completion. Confirm the UART owner recovers and Web tasks cannot steal replies.
- Exercise real public TLS, a private CA, wrong-host and untrusted certificates, invalid system time, oversized/chunked responses and slow DNS. Test both the default and minimum watchdog settings; SDK-internal DNS waits are not a verified end-to-end deadline.
- Verify APN credentials and PDP policy on the actual carrier, especially roaming registration. Confirm enabling/disabling data never forces a `CGATT=0` detach.
- Verify first-boot Web/AP access with the documented fixed defaults without relying on serial logs, preservation of custom credentials, exact-match migration of a previous generated password, and startup with unavailable NVS. Test physical Web password recovery and save-failure behavior. Keep HTTP management on a trusted network.
- Test low/disconnected/charging/full battery readings and sleep/wake on the actual hardware pin mapping.

Host tests and a successful firmware compile do not establish these hardware results. System notifications have RAM-only retries; complete per-channel delivery and exactly-once remote delivery are outside this repair's guarantees.