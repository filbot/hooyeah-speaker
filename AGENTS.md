# AGENTS.md

> Operating rules for Codex when authoring or editing Arduino sketches for **ESP8266 Dev Module** in the **Arduino IDE**.  
> Goal: produce clean, reliable, memory-aware code that follows modern best practices while matching the default Arduino style.

---

## 0) Prime Directives

1. **Make it run, then make it robust, then make it tidy.** Code must compile for `ESP8266 Boards` → `Generic ESP8266 Module` in Arduino IDE with **no warnings** at default settings.
2. **Prefer non-blocking logic.** Use `millis()`-based timing and cooperative multitasking patterns. Avoid long `delay()`; if a tiny pause is required, prefer `delay(0)` or `yield()` on ESP8266 to feed the watchdog.
3. **Be memory-safe.** Avoid heap churn and fragmentation (especially via repeated dynamic `String` concatenations). Favor `F()` macro, `PROGMEM`, pre-allocated buffers, and `String::reserve()` when needed.
4. **Fail loudly in dev, fail safe in prod.** Provide clear serial logs and guard rails (bounds checks, `nullptr` checks). Never crash or block forever.
5. **Document intent.** Top-of-file banner: purpose, hardware, wiring, libraries, and version. Each function: 1–2 line doc comment describing behavior and constraints.
6. **Small, testable units.** Extract hardware access, Wi-Fi, and app logic into small functions or light structs/classes; keep `setup()` and `loop()` short and readable.

---

## 1) Target Environment & Defaults

- **MCU:** ESP8266 (e.g., NodeMCU / Wemos D1 mini)
- **IDE:** Arduino IDE (default formatting; opening brace on same line; 2-space indents; spaces, not tabs)
- **C++ level:** Use modern C++ where safe on ESP8266 core (e.g., `constexpr`, `enum class`, RAII for small helpers).
- **Filesystem:** Prefer **LittleFS** (SPIFFS is legacy).
- **Serial:** Default to `Serial.begin(115200);` with initial small boot delay (`delay(50)`), then `Serial.setTimeout(…. )` if reading.

---

## 2) Project Layout (Arduino IDE–friendly)

A single `.ino` is acceptable, but structure within it:

```text
SketchName.ino
  ├─ Banner comment (what/why/wiring)
  ├─ Includes
  ├─ Compile-time config (constexpr, #define)
  ├─ Types & small structs
  ├─ Globals (minimize; mark volatile where needed)
  ├─ Forward declarations
  ├─ setup()
  ├─ loop()
  ├─ Section: WiFi helpers
  ├─ Section: I/O helpers
  └─ Section: App logic (state machine, timers)
```

If multiple files are used, keep Arduino IDE happy by placing `.h/.cpp` alongside the `.ino` and ensure prototypes are explicit.

---

## 3) Coding Style Rules (Arduino default feel)

- **Braces:** same line: `if (x) { … }`
- **Indent:** 2 spaces
- **Names:** `UPPER_SNAKE_CASE` for macros/pin constants; `lowerCamelCase` for functions/variables; `PascalCase` for types.
- **Constants:** prefer `constexpr` over `#define` when possible.
- **Comments:** concise line comments; section headers with `// ===== Title =====`.
- **Includes ordering:** std/Arduino headers, then 3rd-party, then local.

---

## 4) ESP8266-Specific Gotchas (build in by default)

- **Watchdog:** long work must be chunked; call `yield()` in loops that may exceed ~50ms.
- **ISRs:** mark as `ICACHE_RAM_ATTR` (or `IRAM_ATTR` depending on core) and **do not** use `delay()`, dynamic memory, or Serial inside ISRs. Set a `volatile` flag and handle in `loop()`.
- **Wi-Fi events:** prefer event handlers (`WiFi.onStationModeConnected`, etc., or `WiFi.onEvent(...)` for newer cores) over polling.
- **TLS/HTTP:** if using HTTPS, set time (NTP) before TLS; reuse clients; avoid re-allocating large buffers per request.
- **GPIO:** mind boot-strapping pins (GPIO0, GPIO2, GPIO15). Default safe outputs and avoid pulling boot pins to wrong levels at reset.
- **Flash writes:** throttle config writes; batch and commit. Do **not** write in tight loops.

---

## 5) Non-Blocking Timing Pattern (required)

- Use a small scheduler or simple timers with `millis()`.
- Never busy-wait. No `while (...) {}` loops that starve the CPU; if unavoidable, include `yield()` inside.

Example helper:

```cpp
struct RepeatingTimer {
  uint32_t intervalMs;
  uint32_t lastMs = 0;
  bool ready() {
    const uint32_t now = millis();
    if (now - lastMs >= intervalMs) { lastMs = now; return true; }
    return false;
  }
};
```

---

## 6) Strings, Buffers, and PROGMEM

- Prefer `const __FlashStringHelper*` with `F("literal")` in `Serial.print/println`.
- For recurring JSON/build strings, `String s; s.reserve(256);`
- Use fixed-size `char` buffers with `snprintf()` where practical; always bounds-check lengths.
- Put large read-only data in `PROGMEM`.

---

## 7) Wi-Fi & Networking Checklist

**Baseline behavior Codex must implement unless explicitly excluded:**

- `WiFi.mode(WIFI_STA); WiFi.persistent(false); WiFi.setAutoReconnect(true);`
- Exponential backoff for reconnect attempts.
- Visual and serial feedback for states (connecting, connected, IP).
- NTP sync before any TLS (e.g., `configTime(gmtOffset, daylightOffset, "pool.ntp.org");`), then verify time.
- Reuse `WiFiClient` / `WiFiClientSecure` objects; set timeouts.
- Wrap requests with short, retryable functions; never block the main loop for long.
- If using **ArduinoOTA**, gate behind `#ifdef ENABLE_OTA` and call `ArduinoOTA.handle()` frequently.

---

## 8) File Storage & Config

- Use **LittleFS**; ensure `LittleFS.begin()` and graceful handling if it fails.
- Keep a `config.json` with schema validation on load; write only on changes.
- Provide a `printConfig()` function for debugging.

---

## 9) Logging & Diagnostics

- `#define LOG_LEVEL` (0=off, 1=error, 2=info, 3=debug).
- Macros:
  ```cpp
  #define LOGE(...) do { if (LOG_LEVEL>=1) { Serial.printf("[E] " __VA_ARGS__); } } while(0)
  #define LOGI(...) do { if (LOG_LEVEL>=2) { Serial.printf("[I] " __VA_ARGS__); } } while(0)
  #define LOGD(...) do { if (LOG_LEVEL>=3) { Serial.printf("[D] " __VA_ARGS__); } } while(0)
  ```
- First boot line should print sketch name, version, and build date/time.

---

## 10) Pin Handling & Safety

- Define pins as `constexpr uint8_t`.
- Immediately set pin modes in `setup()`, and set safe default states before enabling outputs.
- Debounce inputs (time or state machine). For mechanical buttons, require minimum stable time (e.g., 25–50ms).
- For PWM/analogWrite ranges, confirm ESP8266 semantics (`analogWriteRange`, `analogWriteFreq`) before usage.

---

## 11) State Machines Over Flags (preferred)

- Represent app behavior with an `enum class AppState { Boot, ConnectingWiFi, Ready, Error };`
- Central `tick()` function progresses the state with timers and events; keep transitions explicit.

---

## 12) Error Handling Policy

- Validate all assumptions (array bounds, pointer checks, return codes).
- For recoverable errors, log and retry with backoff.
- For non-recoverable, move to `Error` state and surface info over Serial (and LED blink pattern if available).

---

## 13) Security & Secrets

- Do **not** hardcode production secrets in code that will be shared.
- Support `#include "secrets.h"` ignored by VCS.
- If tokens are stored on flash, prefer short TTL tokens and explicit “factory reset” to clear.

---

## 14) Minimal Skeleton Codex Should Start From

Codex must adapt/remove parts not needed, but use this as a baseline pattern and style.

```cpp
/*
  Project: MyEsp8266Thing
  Hardware: ESP8266 Dev Module (NodeMCU/Wemos)
  Libraries: ESP8266WiFi, ArduinoOTA (optional), LittleFS (optional)
  Wiring: (document pins here)
  Notes: Non-blocking, watchdog friendly, memory aware.
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <time.h>

#ifdef ENABLE_FS
  #include <LittleFS.h>
#endif

// ===== Compile-time Config =====
constexpr char WIFI_SSID[]     = "YOUR_WIFI";
constexpr char WIFI_PASSWORD[] = "YOUR_PASS";
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
constexpr uint32_t WIFI_RETRY_MIN_MS       = 1000;
constexpr uint32_t WIFI_RETRY_MAX_MS       = 15000;

constexpr uint8_t LED_PIN = LED_BUILTIN;
constexpr bool LED_ACTIVE_LOW = true;

enum class AppState { Boot, ConnectingWiFi, Ready, Error };
AppState appState = AppState::Boot;

// ===== Utilities =====
inline void ledSet(bool on) {
  digitalWrite(LED_PIN, (LED_ACTIVE_LOW ? !on : on));
}

void printBanner() {
  Serial.println();
  Serial.println(F("=== MyEsp8266Thing ==="));
  Serial.printf("Build: %s %s\r\n", __DATE__, __TIME__);
  Serial.printf("Core: %s\r\n", ESP.getCoreVersion().c_str());
}

// ===== WiFi =====
uint32_t lastWifiAttemptMs = 0;
uint32_t wifiBackoffMs = WIFI_RETRY_MIN_MS;

void beginWifiAttempt() {
  lastWifiAttemptMs = millis();
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("WiFi: connecting to %s...\r\n", WIFI_SSID);
}

bool wifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void tickWifi() {
  const uint32_t now = millis();
  if (wifiConnected()) return;

  if (now - lastWifiAttemptMs >= wifiBackoffMs) {
    beginWifiAttempt();
    // Exponential backoff with cap
    wifiBackoffMs = min<uint32_t>(wifiBackoffMs * 2u, WIFI_RETRY_MAX_MS);
  }
}

// ===== NTP (needed before TLS) =====
bool timeReady() {
  time_t t = time(nullptr);
  return (t > 1700000000); // ~2023-11-14
}

void ensureTimeSync() {
  if (timeReady()) return;
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

// ===== setup/loop =====
void setup() {
  pinMode(LED_PIN, OUTPUT);
  ledSet(false);

  Serial.begin(115200);
  delay(50);
  printBanner();

#ifdef ENABLE_FS
  if (!LittleFS.begin()) {
    Serial.println(F("FS: mount failed"));
  } else {
    Serial.println(F("FS: mounted"));
  }
#endif

  beginWifiAttempt();
  appState = AppState::ConnectingWiFi;
}

void loop() {
  // Feed watchdog in busy sections
  yield();

  switch (appState) {
    case AppState::ConnectingWiFi: {
      tickWifi();
      if (wifiConnected()) {
        Serial.printf("WiFi: connected, IP=%s\r\n", WiFi.localIP().toString().c_str());
        ensureTimeSync();
        appState = AppState::Ready;
        wifiBackoffMs = WIFI_RETRY_MIN_MS;
      }
      break;
    }
    case AppState::Ready: {
      // Example heartbeat without blocking
      static uint32_t lastBlink = 0;
      const uint32_t now = millis();
      if (now - lastBlink >= 1000) {
        static bool on = false;
        on = !on;
        ledSet(on);
        lastBlink = now;
        Serial.println(F("tick"));
      }

      // If WiFi drops, go back to Connecting state
      if (!wifiConnected()) {
        appState = AppState::ConnectingWiFi;
      }
      break;
    }
    case AppState::Boot:
    case AppState::Error:
    default:
      // Keep yielding; optionally blink error code
      break;
  }
}
```

---

## 15) Testing & Verification Steps (Definition of Done)

Codex must ensure the following before presenting the sketch as “final”:

- [ ] Compiles for **Generic ESP8266 Module** with **no warnings**.
- [ ] No dynamic `String` growth in tight loops; large strings pre-reserved or avoided.
- [ ] No blocking network calls that stall `loop()`; long operations chunked with `yield()`.
- [ ] Watchdog never triggers under expected workloads.
- [ ] GPIO boot pins respected; outputs set to safe defaults on startup.
- [ ] Wi-Fi reconnects automatically with bounded backoff; IP logged on connect.
- [ ] If HTTPS is used, NTP sync occurs before TLS; time checked.
- [ ] Optional features (OTA, FS) are feature-flagged and compile out cleanly.
- [ ] Top banner includes project name, version, build date/time, and library notes.
- [ ] Inline comments explain any non-obvious math, timing, or hardware constraints.

---

## 16) Performance & Power Notes

- Prefer lower CPU load: avoid floating point in hot paths; precompute constants.
- Throttle logs at runtime (e.g., print once per second max).
- Sleep modes are optional; if implemented, clearly document wake sources and reconnection strategy.

---

## 17) When in Doubt

- Choose **clarity over cleverness**.
- Add a tiny helper function instead of duplicating a tricky line twice.
- Write the comment you wish you had when debugging at 2 a.m.

---

**End of AGENTS.md**
