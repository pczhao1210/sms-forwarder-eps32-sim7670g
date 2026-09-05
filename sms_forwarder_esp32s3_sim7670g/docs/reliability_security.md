# Reliability And Security

## Message Ownership

- A SIM-backed message is admitted only after its local record has been completely written, flushed, reopened and verified. Filtered and invalid messages also require durable admission before SIM deletion.
- History is bounded to 50 records and a 48 KiB file budget, with space reserved for status growth. Only terminal records can be evicted. A store full of pending work rejects new admission and keeps the source on the SIM for a later scan.
- CMGR/live, batch scans and direct multipart arrivals share the assembly buffer. Conflicting parts are not combined. SIM indices remain until all parts are present and the assembled record is saved. The live fragment buffer is bounded to 128 fragments.
- SIM deletion is separate from delivery. A delete failure or power loss between storing and deletion can cause re-admission. Remote delivery followed by a lost acknowledgement can also be retried. The guarantee is at-least-once, not exactly-once.
- Direct `+CMT` delivery has no guaranteed SIM copy. Default `CNMI` setup uses SIM-backed `+CMTI`; use that mode when durable recovery matters. Buffer exhaustion or power loss can still lose a direct-only message.

## Notification Results

At least one enabled channel must acknowledge success. Bark, ServerChan, Telegram, DingTalk and Feishu validate their provider-specific JSON results, not just HTTP 2xx. A custom webhook retains transport-level 2xx semantics. All enabled channels are attempted, but per-channel durable retries are not implemented.

Each notification uses an immutable configuration snapshot. A saved configuration applies to newly admitted work; already queued jobs retain the settings they were admitted with. One SMS cannot be manually forwarded while its delivery or result finalization is active. Automatic SMS retries are limited to three attempts after the initial attempt, and restoration does not grant a fourth attempt. A failed local status commit retries the commit without repeating HTTP in the same running session.

System alerts and reports have bounded in-memory retries. Their jobs are not a flash-backed outbox. Daily reports catch up later on the same date; weekly reports catch up later on Monday. A date is marked sent only after delivery and a successful statistics save. A reboot can therefore retry a report whose remote delivery preceded its durable marker. Report counters remain cumulative.

`POST /api/test/notification` returns HTTP 202 with `{ "id": 1, "complete": false }`. Authenticated `GET /api/test/notification?id=1` returns `complete`, named channel `results`, `total`, and `success`. Only the latest test is retained; a second test is rejected while one is outstanding. The Web UI polls this endpoint without blocking UART processing.

## Credentials And Recovery

Initial Web and setup-AP passwords are independent 128-bit random values represented as 32 hex characters. They are generated before radio/ADC initialization and retained in NVS. Serial output is the physical bootstrap channel; passwords are not copied into the Web log buffer.

An existing custom Web password is preserved on upgrade. An empty password or the legacy `admin1234` is replaced with the device's bootstrap password and authentication is enabled. A configured disabled-auth setting with a custom password is preserved; enabling authentication is strongly recommended. Missing credentials while authentication is enabled fail closed.

Connect the USB serial console at 115200 baud and send this line to recover access:

```text
RESET WEB AUTH
```

Recovery sets the username to `admin`, enables authentication and restores the device bootstrap password. It reports the password only after saving successfully, and does not erase SMS or WiFi settings. Full flash/NVS erasure generates new passwords at the next boot. Filesystem-only replacement does not erase NVS.

The console still uses HTTP Basic authentication. Do not expose it through router port forwarding or an untrusted network. HTTPS verification for outbound notifications does not encrypt the management UI. SPIFFS/NVS are not encrypted by this firmware, and physical flash access can reveal stored secrets.

## Safe Configuration Updates

`GET /api/config` does not return saved WiFi passwords, provider keys/tokens/chat IDs, provider URLs/webhooks, or APN credentials. It supplies `hasPassword`, `hasKey`, `hasToken`, `hasChatId`, `hasUrl`, `hasWebhook`, `hasApnUser`, and `hasApnPass` flags on the relevant objects. Responses are marked `Cache-Control: no-store`.

Sensitive form fields use an action parameter named `<field>Action`. For example, `barkKeyAction` accepts `keep`, `replace`, or `clear`. `replace` requires a nonempty value in `barkKey`; `clear` removes the saved value. For older clients without an action parameter, a nonempty value replaces and an absent/empty value keeps the existing value. To clear intentionally, send the explicit action. The current UI exposes all three actions.

Notification toggle parameters are parsed by value. `true`, `1`, and `on` enable; `false`, `0`, and `off` disable. Missing toggles are false. All six unchecked channels therefore remain disabled even if their credentials are retained. An enabled Telegram configuration requires both token and chat ID.

## Verified HTTPS

HTTPS requests verify the certificate chain and hostname using the root certificate bundle in the pinned Arduino-ESP32 core. The system clock must be set by NTP or the modem; invalid time or a failed certificate check fails delivery without `setInsecure()` fallback. Explicit `http://` endpoints remain available for local integrations but are unencrypted. Automatic redirects are disabled, so configure the final endpoint URL.

For a private CA:

1. Provision a PEM CA certificate or PEM CA chain as `/private-ca.pem` in SPIFFS, up to 16 KiB. This is the CA certificate, not a private key.
2. Set `tls.privateCaHost` in the stored configuration or the Web form's Private CA hostname field to the exact DNS hostname. Do not include a scheme, path or port.
3. Use an `https://` URL with that hostname and a matching server certificate. Only that hostname uses the private CA; other hosts keep using the built-in public roots.
4. Clear the private CA hostname to return to public-root verification. A missing/invalid private CA file fails closed for its selected host.

When using an Arduino filesystem-image uploader, include the PEM file in the sketch's data directory during commissioning. Uploading a filesystem image can overwrite existing configuration and SMS history; do not upload the example data image over an in-service device without arranging preservation of its data.

Responses are limited to 4096 bytes of decoded body and 12288 bytes of incoming headers/framing/body. An exceeded limit is a failure, even for a custom webhook. I/O checks enforce a 10-second elapsed budget, with 2-second TCP/read and 3-second TLS-handshake timeouts. These are not a strict end-to-end bound on SDK DNS/internal blocking. Test slow/unavailable DNS and the minimum configured watchdog timeout on the target board. Raw response bodies and credential-bearing URLs are not logged by the notification sender.

## Network And Power

Saving network settings returns `restartRequired: true`. The SIM reset action requests serialized modem reinitialization, rather than claiming an immediate change while a transaction owns the UART. Data activation waits for registration/roaming policy and successful APN/authentication acknowledgements. An unknown query result is not treated as proof that data is off. The firmware does not issue `CGATT=0` to disable data because that can disrupt LTE SMS registration.

PDP credentials use PAP when an APN username is supplied. SIM767XX `AT+CGAUTH` write syntax places the password before the username: `AT+CGAUTH=1,1,"password","username"`. Empty credentials send `AT+CGAUTH=1,0`. Values are limited to 64 printable ASCII bytes and cannot contain quotes or backslashes. CHAP selection is not exposed. Reference: [SIM767XX AT Command Manual V1.01, section 5.2.10](https://files.waveshare.com/wiki/ESP32-S3-SIM7670G-4G/SIM767XX_Series_AT_Command_Manual_V1.01.pdf).

Critical battery protection is independent of notification preferences. It requires an available battery reading and does not force sleep while charging or reported fully charged. The actual thresholds, charging indication and wake behavior still require board-specific testing, especially on V2 pin mappings.

Outbound SMS accepts a positive-length numeric destination with an optional leading `+`, up to 20 digits, and valid UTF-8 content fitting 70 UTF-16 code units / 140 UCS2 bytes. Supplementary characters consume two units. No automatic multipart-send feature is provided.