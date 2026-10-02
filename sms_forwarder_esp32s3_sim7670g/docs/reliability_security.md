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

The Web console defaults to username `admin` and password `admin1234`. The setup AP `SMS-Forwarder-Setup` uses password `12345678`. These fixed defaults do not require serial output, random generation or successful NVS access, and do not change on reboot.

Existing custom Web credentials are preserved on upgrade. For compatibility with the previous random-password implementation, the old NVS `sms-bootstrap/web` entry is read without writing to it. A saved Web password is restored to `admin1234` only when it exactly matches that old generated value; an unavailable NVS record does not block startup or overwrite an unrecognized saved password. Empty Web passwords also receive the default, and repaired credentials enable authentication. An explicitly disabled-auth configuration with complete, non-migrated credentials is preserved. Missing credentials while authentication is enabled fail closed.

Serial access is optional for normal setup. If a custom Web password is forgotten, connect the USB serial console at 115200 baud and send this line to recover access:

```text
RESET WEB AUTH
```

Recovery sets the username to `admin`, enables authentication and restores password `admin1234`. It acknowledges the reset only after saving successfully, and does not erase SMS, WiFi settings or other configuration. It does not change the setup AP password. Existing custom passwords are not printed in startup messages or returned by the configuration API.

The default credentials are public and the console uses HTTP Basic authentication. Use it only on a trusted network and do not expose it through router port forwarding. HTTPS verification for outbound notifications does not encrypt the management UI. SPIFFS/NVS are not encrypted by this firmware, and physical flash access can reveal stored secrets.

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

### Configurable Handshake Timeout

The notification form's **HTTPS TLS handshake timeout (s)** accepts whole seconds from **1 to 60**, default **5**. It controls only the TLS handshake, not DNS, TCP connection setup or response reads. The stored/exported field is `tls.handshakeTimeoutSeconds`; the notification form/API parameter is `tlsHandshakeTimeoutSeconds`. A missing field in an older configuration loads as 5 seconds; an older API client omitting the parameter preserves the saved value. Invalid types, out-of-range values and unsafe TLS/watchdog combinations are rejected rather than silently truncated.

The watchdog timeout must be at least `handshakeTimeoutSeconds + 5`. For example, a 30-second handshake needs a watchdog timeout of at least 35 seconds; 60 seconds needs 65. Increase the watchdog in System settings first. With the default 30-second watchdog, the maximum allowed handshake is 25 seconds. Neither the watchdog nor the handshake setting is silently relaxed. Lowering the watchdog is also rejected while notifications are pending/running, because those jobs retain their original configuration snapshots. Changes to the handshake setting apply to newly queued jobs.

TCP connection and read-idle limits remain 2 seconds. HTTPS I/O checks use a budget of `max(10, handshakeTimeoutSeconds + 5)` seconds, so a longer configured handshake is not cut off by the old 10-second budget. Plain HTTP retains its 10-second I/O budget. Responses remain limited to 4096 decoded bytes and 12288 incoming headers/framing/body bytes; crossing either limit is a failure, even for a custom webhook. Notifications and network diagnostics use the same TLS setting and feed the watchdog before/after blocking requests.

These are not strict end-to-end deadlines on SDK DNS/internal blocking; the five-second watchdog margin is not a guarantee against slow/unavailable DNS. Test those conditions and the minimum watchdog setting on the actual board. Manual network diagnostics run synchronously and make two separate HTTPS connections (reachability probe, then HTTP request), so long settings can delay main-loop services during diagnostics. The notification test button now waits for the backend job's completion or an explicit connection/status error instead of declaring failure after 90 seconds. Refreshing the page stops that frontend wait, not the queued delivery.

HTTP `-1` is a generic connection failure, even though the Arduino core calls it "connection refused". It does not prove that the server refused TCP. Failures at about 3 seconds on older firmware can be the previous handshake limit, but DNS/TCP failures can look similar. Connection failures log `connect_timeout_ms` and, for HTTPS, the effective `handshake_timeout_ms` and `request_timeout_ms`, along with available `tls_code` and `received_bytes`. The main error retains `code`, `err`, `provider`, `phase`, `transport` and `elapsed_ms`; additional `HTTP detail` entries identify the provider and stay below the Web log's 200-byte message limit. Read all adjacent entries for that provider. Raw response bodies and credential-bearing URLs are not logged.

Notification HTTP traffic uses the ESP32 Wi-Fi connection, not the SIM7670G data context. SIM `READY`, registration and signal AT replies do not verify the push path. If failures persist, run Web network diagnostics against the configured service's root URL (without the device key), check the device's Wi-Fi/DNS and system time, and retain the complete HTTP detail entries. A successful desktop request is useful evidence about the server, but does not verify the device's network or certificate bundle. Do not disable certificate verification to work around connection failures.

### Shorter Trusted Chains And Root Updates

The 2026-10-02 investigation inspected the actual certificate bundle in the locally installed ESP32 core **3.3.11** used for the new build: 150 trust anchors, including **ISRG Root X1 and ISRG Root X2**, but not **Root YE**. The embedded X2 public key exactly matches the official current certificate; its SHA-256 SPKI fingerprint is `762195c225586ee6c0237456e2107dc54f1efc21f61a792ebd515913cce68332`.

For the tested Let's Encrypt YE2 certificate, both a live TLS 1.2 connection trusting only X2 and offline hostname/chain validation of the shorter chain succeeded:

```text
Server sends: leaf -> YE2 -> Root YE (cross-signed by X2)
Device trusts: ISRG Root X2
```

This removes the final X2 certificate cross-signed by X1 from the served chain. It establishes trust compatibility, not a measured ESP32 speedup. A verifier may already stop at a known trust anchor, so fewer transmitted certificates do not necessarily mean the same reduction in signature checks. The shortened chain has not been deployed to the live server by this repair.

[Let's Encrypt's official certificate page](https://letsencrypt.org/certificates/) lists this X2 alternate chain and the new Root YE. At the time of checking, YE was listed as an upcoming root, not yet included in root-program trust stores. No additional trust anchor needs to be compiled into this firmware for the tested chain: the existing X2 anchor can validate it. Leaf/intermediate renewal on the server does not require embedding those certificates in the device. Keep using the SDK's maintained full public-root bundle; do not replace it with a pinned leaf certificate or automatically add a newly published root. These findings concern the tested chain and exact core bundle, not an audit of every public CA.

If Caddy obtains the certificate, merge this setting into the **existing ACME issuer configuration** that manages it, retaining the existing DNS challenge and other issuer options:

```caddyfile
tls {
    issuer acme {
        preferred_chains {
            root_common_name "ISRG Root X2"
        }
    }
}
```

See [Caddy's ACME issuer documentation](https://caddyserver.com/docs/caddyfile/directives/tls#acme). Prefer the explicit X2 root over unconditional `smallest`, which can select a chain ending at a root the device does not trust. For a shared wildcard certificate, adjust the policy that actually obtains that certificate, not merely an unrelated hostname's site block. Chain selection takes place during certificate acquisition/renewal; changing configuration does not itself rewrite a certificate already in storage. Validate the Caddy configuration, follow the server's normal renewal/reload process without deleting its certificate storage, and inspect the newly served chain with `openssl s_client -showcerts` before testing the device. Older clients that only trust X1 may need the longer compatibility chain.

## Network And Power

Saving network settings returns `restartRequired: true`. The SIM reset action requests serialized modem reinitialization, rather than claiming an immediate change while a transaction owns the UART. Data activation waits for registration/roaming policy and successful APN/authentication acknowledgements. An unknown query result is not treated as proof that data is off. The firmware does not issue `CGATT=0` to disable data because that can disrupt LTE SMS registration.

PDP credentials use PAP when an APN username is supplied. SIM767XX `AT+CGAUTH` write syntax places the password before the username: `AT+CGAUTH=1,1,"password","username"`. Empty credentials send `AT+CGAUTH=1,0`. Values are limited to 64 printable ASCII bytes and cannot contain quotes or backslashes. CHAP selection is not exposed. Reference: [SIM767XX AT Command Manual V1.01, section 5.2.10](https://files.waveshare.com/wiki/ESP32-S3-SIM7670G-4G/SIM767XX_Series_AT_Command_Manual_V1.01.pdf).

Critical battery protection is independent of notification preferences. It requires an available battery reading and does not force sleep while charging or reported fully charged. The actual thresholds, charging indication and wake behavior still require board-specific testing, especially on V2 pin mappings.

Outbound SMS accepts a positive-length numeric destination with an optional leading `+`, up to 20 digits, and valid UTF-8 content fitting 70 UTF-16 code units / 140 UCS2 bytes. Supplementary characters consume two units. No automatic multipart-send feature is provided.