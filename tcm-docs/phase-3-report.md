# Phase 3 Report — TUI Layer

**Project:** TCM (Terminal Connection Manager)
**Phase:** 3 — Terminal User Interface
**Status:** COMPLETED
**Date:** 2026-05-25

---

## Build & Test Results

| Metric | Result |
|--------|--------|
| Build | **SUCCESS** |
| Total Tests | **178 / 178 PASSED** |
| Phase 1 tests (carry-over) | 63 |
| Phase 2 tests (carry-over) | 56 |
| Phase 3 new tests | 59 |
| Regression | None |

---

## Modules Completed

### Task 3.1 — IRenderable + Renderer

**Files:** `include/tui/IRenderable.hpp`, `include/tui/Renderer.hpp`, `src/tui/Renderer.cpp`

- `IRenderable` interface defining the two-method contract: `render()` and `handleKey()`.
- `Renderer` wraps ncurses with a clean, headless-safe API.
- `ColorPair` enum: `Normal`, `Highlighted`, `StatusBar`, `TitleBar`, `Connected`, `Error`, `Idle`.
- Headless safety: `isatty()` check at construction; every draw call is guarded by `if (!m_initialized) return`.
- Public API: `init()`, `teardown()`, `drawBox()`, `drawText()`, `drawStatusBar()`, `drawTitleBar()`, `refresh()`, `clear()`, `getKey()`, `moveCursor()`.

**Tests:** 16

---

### Task 3.2 — SessionListView

**Files:** `include/tui/SessionListView.hpp`, `src/tui/SessionListView.cpp`

- `SessionInfo` struct carrying name, host, type, and connection status.
- `SessionListView` inherits `IRenderable`.
- Keyboard navigation: `j` / `k` and arrow keys (`KEY_DOWN` / `KEY_UP`).
- Search mode activated by `/`; filter applied in real-time against session name and host fields.
- Status indicators: `●` (connected), `○` (idle), `✗` (error).

**Tests:** 15

---

### Task 3.3 — SwitcherOverlay + KeyHandler

**Files:** `include/tui/SwitcherOverlay.hpp`, `src/tui/SwitcherOverlay.cpp`, `include/tui/KeyHandler.hpp`, `src/tui/KeyHandler.cpp`

- `SwitcherOverlay` renders a centered popup listing up to 9 connected sessions, selectable by digit keys `1`–`9`.
- `KeyHandler` detects raw byte `0x1D` (Ctrl+`]`) and publishes `KeyPressedEvent{isEscape=true}` on the `EventBus`.
- `SwitcherOverlay` subscribes to `EventBus` to show/hide in response to the event.

**Tests:** SwitcherOverlay — 11, KeyHandler — 5

---

### Task 3.4 — SessionForm

**Files:** `include/tui/SessionForm.hpp`, `src/tui/SessionForm.cpp`

- `FormField` struct (label, value, masked flag).
- `FormResult` enum: `Submitted`, `Cancelled`.
- `SessionForm` inherits `IRenderable`.
- Field sets per connection type:
  - **SSH:** Name, Host, Port, Username, Password (masked), Key Path
  - **Serial:** Name, Device, Baud Rate
  - **Telnet:** Name, Host, Port
- Tab key cycles through fields; connection type toggle cycles field layout.
- Input validation before submission.
- `loadConfig()` / `getConfig()` integration with `SessionConfig`.

**Tests:** 12

---

## Screen Layout

```
┌─────────────────────────────────────────────────────────────┐
│  TCM v1.0 — Terminal Connection Manager                     │  ← TitleBar
├─────────────────────────────────────────────────────────────┤
│  [SSH]  router-lab        192.168.1.1      ●               │
│  [TEL]  switch-core       10.0.0.1         ○               │  ← SessionListView
│  [SER]  /dev/ttyUSB0      115200           ✗               │
│  ...                                                        │
├─────────────────────────────────────────────────────────────┤
│  [Enter] connect  [a] add  [d] delete  [/] search  [q] quit │  ← StatusBar
└─────────────────────────────────────────────────────────────┘

        ┌─── Active Sessions ───┐
        │  1. router-lab        │   ← SwitcherOverlay (Ctrl+])
        │  2. switch-core       │
        │  [ESC] cancel         │
        └───────────────────────┘

        ┌─── New Connection ────┐
        │  Type: [SSH]          │
        │  Name: ____________   │   ← SessionForm
        │  Host: ____________   │
        │  Port: ____________   │
        │  [Tab] next  [Enter] OK  [Esc] cancel │
        └───────────────────────┘
```

---

## Definition of Done

- [x] All tests pass: **178 / 178**
- [x] Headless test environment — no physical terminal required
- [x] Session list navigation with `j`/`k` and arrow keys
- [x] Real-time search filter (`/` to activate)
- [x] `Ctrl+]` triggers `SwitcherOverlay` via `KeyPressedEvent`
- [x] Add/Edit form with per-type field layout and validation
- [x] Phase 1 and Phase 2 tests show zero regression

---

## Issues & Resolutions

| Issue | Resolution |
|-------|-----------|
| `KEY_UP` / `KEY_DOWN` / `KEY_BACKSPACE` macros unavailable in test files | Added `#include <ncurses.h>` to the affected test translation units |
| ncurses macro leakage into test assertions causing mismatched constants | Hardcoded numeric values (`258` = `KEY_DOWN`, `259` = `KEY_UP`, `343` = `KEY_BACKSPACE`) in test files to avoid macro contamination |
| `Renderer` crashing in CI (no TTY) | All draw methods guard with `if (!m_initialized) return`; `m_initialized` set only when `isatty(STDOUT_FILENO)` is true |

---

## Summary

Phase 3 delivers the complete TUI layer on top of the core and protocol foundations established in Phases 1 and 2. The four tasks — `Renderer`, `SessionListView`, `SwitcherOverlay`/`KeyHandler`, and `SessionForm` — provide a fully navigable, ncurses-based interface with headless test support. The codebase now contains **178 passing tests** with no regressions, and the UI components are ready for integration in Phase 4.
