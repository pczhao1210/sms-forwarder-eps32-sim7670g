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

The SMS regression tests also cover slot `0` notifications, actual `AT+CMGR=0`/`AT+CMGD=0` commands, delete retries and deduplication, CPMS/CMGR response ordering, zero- and one-based scan boundaries, malformed index rejection, durable admission and multipart fragments at slot `0`, and direct CMT messages that must never delete a SIM slot. Scans start at `0` and include the CPMS capacity as a compatibility probe for one-based stores, stopping once the reported message count is found.

`network_maintenance.test.js` executes the production network maintenance functions with a fake UART and clock. It checks one-second BUSY deferral, no extra snapshot queries on BUSY, preservation of cached state, interruption between attach/activate and between snapshot queries, current-policy retries, roaming transitions, genuine errors, and `millis()` wraparound. `sms_at_state.test.js` verifies the maintenance availability guard against active modem owners and reset state.

`notification_http.test.js` executes the production HTTP request, TLS setup and TLS reachability probe functions with fake clients. It checks the default 5-second handshake, exact success/failure boundaries at 1/5/10/30/60 seconds, actual requests lasting beyond the old 10-second budget, matching diagnostic settings, rejection of invalid/unsafe TLS settings, TCP timeout ambiguity, watchdog feeds, rollover, unset clock/private-CA failures, 4096-byte body and 12288-byte receive limits, unchanged 2-second read idle and plain-HTTP limits, invalid JSON, all six provider success rules, and log redaction. Both production log translations are formatted through a 256-byte buffer and checked against the Web console's 200-byte limit, including long TLS errors. No real requests are sent. It accepts `ARDUINOJSON_HEADER`; for a standalone run it defaults to the Arduino IDE library under `~/Arduino/libraries/ArduinoJson/src/ArduinoJson.h`.

`tls_config.test.js` exercises the production notification/system save handlers: omitted-field compatibility, integer/range validation, the five-second watchdog margin in both directions, unchanged configuration after rejection, save-failure rollback and blocking watchdog reductions while old notification jobs remain. `config_persistence.test.js` checks default/migrated/saved TLS values and recovery from invalid settings. `notification_snapshot.test.js` verifies that pending jobs retain their original timeout and that the pending-work guard is conservative on lock failure.

HTTP logs retain the `code=..., err=..., resp=...` format, but `resp` is diagnostic metadata, not response content: `provider`, `phase`, `transport` and `elapsed_ms`. Available TLS/read codes, byte counts and connection timeout settings follow in short `HTTP detail` entries for the same provider. A generic core `-1` still cannot reliably distinguish DNS, TCP, and handshake failures and is not accompanied by the misleading "connection refused" label. Connect/read timeouts stay at 2 seconds; TLS defaults to 5 seconds and is configurable from 1 to 60. HTTPS I/O budgets use `max(10, handshake seconds + 5)` seconds; plain HTTP remains at 10. Certificate validation is unchanged.

## Browser Suite

This uses mock HTTP APIs and never contacts a real notification provider or device. It checks 1440x1000 desktop and 390x844 mobile viewports, credential Keep/Replace/Clear behavior using actual browser FormData, all-disabled toggles, zero-valued settings, TLS timeout load/save/defaults, browser range/required validation, visible backend watchdog errors, six-channel asynchronous results and horizontal overflow. The fast `web_config.test.js` also simulates 400 half-second result polls to verify that long tests are not cut off at the former 90-second frontend limit. Screenshots are written under a temporary directory printed by the browser test.

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

This command preserves the previously verified build with OPI PSRAM and `USB CDC On Boot: Enabled` (`CDCOnBoot=cdc`); it is not a universal V1/V2 hardware preset. The [revision-specific flashing settings](../README.md#firmware-flashing-settings) use QSPI PSRAM for V1, OPI PSRAM for V2, and `USB CDC On Boot: Disabled` for both. Use `CDCOnBoot=default` instead of `CDCOnBoot=cdc` to match that USB selection, and select the PSRAM mode for the actual hardware. This firmware requires `PartitionScheme=custom` on both revisions because its storage uses SPIFFS, not the general V1 FATFS layout. The recorded build results below do not establish validation of V1 QSPI or the Disabled configuration. With CDC disabled, logs and physical Web password recovery use the UART0 console rather than native USB CDC.

The custom-partition CLI size report may display 16 MiB as the maximum; the actual app slot is 3 MiB. Check the binary against the slot size, not that printed total. No upload command is run by these tests. Check the V1/V2 hardware notes and actual PSRAM/USB wiring before flashing.

The BUSY/HTTP diagnostics repair was also compiled with the locally installed ESP32 core 3.3.10, ArduinoJson 7.4.3 and Adafruit NeoPixel 1.15.5 using Arduino CLI 1.3.0. This does not change the pinned baseline above.

The configurable-timeout repair passed the targeted configuration/API/HTTP/policy/snapshot host tests with ArduinoJson 6.21.5, the HTTP/TLS tests with ArduinoJson 7.4.3, and a firmware build with Arduino CLI 1.3.0, ESP32 core 3.3.11 and ArduinoJson 7.4.3. Its 1,639,936-byte application binary fits the 3 MiB app slot. This is not a claim of successful delivery from a physical device.

## Target Checklist

- Receive a normal SMS and a batch/live split multipart SMS; interrupt power before/after storage commit and SIM deletion. Confirm pending work restores without silent loss; duplicates are possible.
- Receive `+CMTI: "SM",0` and confirm `SMS_READ` logs index `0`, the message appears in the Web SMS list and is forwarded, and SIM deletion happens only after local persistence. Repeat with manual SMS checking and a multipart message containing slot `0`; direct `+CMT` reception must not delete an unrelated slot `0`.
- Fill all 50 pending records with WiFi unavailable. Confirm the next message remains on SIM and is eventually re-scanned after capacity becomes available.
- Inject or observe `+CMS ERROR`, `+CME ERROR`, missing/late `OK`, concurrent CMTI and delayed `+CMGS` completion. Confirm the UART owner recovers and Web tasks cannot steal replies.
- Exercise real public TLS, a private CA, wrong-host and untrusted certificates, invalid system time, oversized/chunked responses and slow DNS. Test both the default and minimum watchdog settings; SDK-internal DNS waits are not a verified end-to-end deadline.
- On a failing notification, verify `phase`, `elapsed_ms`, and adjacent detail entries containing `tls_code`, `read_code` and timeout settings. Confirm `handshake_timeout_ms=5000` and `request_timeout_ms=10000` with defaults, complete `received_bytes` values, and no "connection refused" label for generic `-1`. Set a 20-second handshake with a watchdog of at least 25 seconds, reboot, and verify that sender/diagnostics retain the setting and the I/O budget grows to 25 seconds. Test both boundaries and invalid values; old files without the field must load 5 seconds. Confirm no provider URL key, SMS text or response body is logged.
- Attempt conflicting TLS/watchdog saves and watchdog reductions while an old job is queued/in flight; confirm a visible error with no partial settings change. Confirm newly queued jobs use the new value and already queued jobs keep their snapshot. Exercise multi-channel tests beyond 90 seconds without a premature frontend failure.
- Verify APN credentials and PDP policy on the actual carrier, especially roaming registration. Confirm enabling/disabling data never forces a `CGATT=0` detach.
- Run network/data maintenance while CNMI polling, SMS reads/deletes and Web AT jobs are active. Confirm a deferred DEBUG message instead of a DATA failure, then confirm maintenance resumes after the modem is idle without waiting a full signal-check interval.
- Verify first-boot Web/AP access with the documented fixed defaults without relying on serial logs, preservation of custom credentials, exact-match migration of a previous generated password, and startup with unavailable NVS. Test physical Web password recovery and save-failure behavior. Keep HTTP management on a trusted network.
- Test low/disconnected/charging/full battery readings and sleep/wake on the actual hardware pin mapping.

Host tests and a successful firmware compile do not establish these hardware results. System notifications have RAM-only retries; complete per-channel delivery and exactly-once remote delivery are outside this repair's guarantees.