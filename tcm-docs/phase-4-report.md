# Phase 4 Report: Logging, Macro Engine & Credential Store

**Project:** TCM (Terminal Connection Manager)
**Phase:** 4 — Logging, Automation & Security
**Date:** 2026-05-25
**Status:** COMPLETED

---

## Build & Test Summary

| Metric | Result |
|--------|--------|
| Build | **SUCCESS** |
| Total Tests | **218 / 218 PASSED** |
| Phase 1–3 (regression) | 178 / 178 |
| Phase 4 (new) | 40+ / 40+ |

All existing Phase 1, 2, and 3 tests continue to pass — no regressions introduced.

---

## Completed Tasks

### Task 4.1 — AsciicastLogger

**Module:** `logger/AsciicastLogger`

Implements session recording in the **asciicast v2** format, compatible with [asciinema](https://asciinema.org/).

**Features:**
- **File format:** one JSON object per line — header line followed by event lines.
- **Event types:** `logOutput()` writes type `"o"` events; `logInput()` writes type `"i"` events.
- **Timestamps:** recorded using `std::chrono::steady_clock` with 3 decimal places (millisecond precision).
- **JSON escaping:** all special characters are correctly escaped — `\`, `"`, `\r`, `\n`, `\t`, and control characters encoded as `\u00XX`.
- **File rotation:** when the current file reaches **50 MB**, a new part file is opened automatically (`part2`, `part3`, …), each with its own valid asciicast header.
- **File naming:** `{session_name}_{YYYYMMDD_HHMMSS}.cast`
- **Directory creation:** uses `std::filesystem` to create log directories automatically.

**Tests:** 10 tests — `AsciicastLoggerTest`

---

### Task 4.2 — Recipe & MacroEngine

**Modules:** `macro/Recipe`, `macro/MacroEngine`

Implements a **recipe-driven automation engine** for scripting terminal interactions.

**Recipe (`Recipe.hpp` / `Recipe.cpp`):**
- Parses recipe definitions from **JSON** using `nlohmann/json`.
- Defines `struct Step` with fields: `expect` (regex pattern), `send` (string to transmit), `timeout_ms`, and `on_timeout`.
- `OnTimeoutAction` enum: `Abort`, `Skip`, `Continue`.

**MacroEngine (`MacroEngine.hpp` / `MacroEngine.cpp`):**
- Implements a **state machine** with states: `Idle → Running → Done / Error`.
- `feed(data)` accumulates incoming data into a buffer across multiple calls; triggers a regex match against the current step's `expect` pattern.
- On match: sends the configured `send` string and advances to the next step.
- **Variable interpolation:** `{var}` placeholders in `send` strings are substituted at runtime.
- **Timeout detection:** steps that exceed `timeout_ms` trigger the `on_timeout` action.
- **Events emitted:** `MacroTriggeredEvent`, `MacroDoneEvent`, `MacroFailedEvent` via `EventBus`.

**Tests:** `RecipeTest` — 10 tests; `MacroEngineTest` — 11 tests

---

### Task 4.3 — CredentialStore

**Module:** `credential/CredentialStore`

Provides **encrypted-at-rest** storage for session credentials using modern authenticated encryption.

**Cryptographic design:**
- **Cipher:** AES-256-GCM (authenticated encryption with associated data).
- **Key derivation:** PBKDF2-SHA256 with **100,000 iterations** per NIST recommendations.
- **File format (binary):**

  ```
  [4 bytes: "TCMC" magic]
  [16 bytes: salt]
  [12 bytes: GCM nonce/IV]
  [16 bytes: GCM authentication tag]
  [N bytes: ciphertext]
  ```

- **Fresh randomness:** a new random salt and nonce are generated on every `save()` call.
- **Security guarantee:** plaintext credentials never appear in the file; wrong passphrase causes `load()` to return `false` (GCM tag mismatch).

**API:** `store()`, `retrieve()`, `remove()`, `contains()`, `listIds()`, `save()`, `load()`

**Tests:** 10 tests — `CredentialStoreTest`

---

## Definition of Done

- [x] **218 / 218 tests pass** (all phases)
- [x] `AsciicastLogger`: output files contain valid JSON-per-line asciicast v2 format
- [x] `MacroEngine`: recipe-based automation executes correctly end-to-end
- [x] `CredentialStore`: credentials encrypted with AES-256-GCM; wrong passphrase correctly rejected via authentication tag verification
- [x] No regressions in Phase 1, 2, or 3 test suites

---

## Issues & Resolutions

| Issue | Resolution |
|-------|-----------|
| **AsciicastLogger — file rotation header** | Stored `logDir`, `width`, and `height` in the logger instance so that each rotated part file is opened with a correct asciicast header (matching the original terminal dimensions). |
| **MacroEngine — partial data matching** | `feed()` appends incoming data to a persistent buffer; regex matching is performed against the accumulated buffer rather than each individual chunk, ensuring patterns are matched even when data arrives in multiple calls. |
| **CredentialStore — wrong passphrase handling** | An incorrect passphrase produces a derived key that fails GCM tag verification; `load()` catches the OpenSSL authentication failure and returns `false` cleanly without exposing partial plaintext. |

---

## Dependencies

| Library | Usage |
|---------|-------|
| OpenSSL (libssl, libcrypto) | AES-256-GCM encryption, PBKDF2 key derivation |
| nlohmann/json | Recipe JSON parsing |
| std::filesystem (C++17) | Log directory creation |
| std::chrono::steady_clock | High-resolution event timestamps |
| std::regex | MacroEngine expect-pattern matching |

---

## Next Phase

See [phase-3-4-5-plans.md](phase-3-4-5-plans.md) for the Phase 5 roadmap (TUI, session management, and integration).
