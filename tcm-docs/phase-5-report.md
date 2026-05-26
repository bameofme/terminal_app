# Phase 5 Report — Integration, Packaging & Documentation

## Summary

| Metric | Result |
|--------|--------|
| Build | **SUCCESS** (Release mode) |
| Tests | **238/238 PASSED** |
| Binary | `build/tcm` — 528K |
| Smoke test | **5/5 PASSED** |

---

## Tasks Completed

### Task 5.1 — Main Wiring

Implemented the top-level `Application` class that ties all subsystems together.

**`AppConfig` struct fields:**
- `configPath` — path to `sessions.json`
- `logDir` — directory for asciicast log files
- `credPath` — path to encrypted credential store

**CLI arguments:**
| Flag | Description |
|------|-------------|
| `--version` | Print `TCM v1.0.0` and exit |
| `--config <path>` | Override default config path |
| `--log-dir <path>` | Override default log directory |

**`init()` sequence:**
1. Create required directories (`logDir`, parent of `credPath`)
2. Initialise `EventBus`
3. Initialise `SessionManager`
4. Initialise `EpollLoop`
5. Initialise `Renderer` (ncurses)
6. Initialise TUI modules (`SessionListView`, `SwitcherOverlay`, `SessionForm`, `KeyHandler`)
7. Initialise `CredentialStore`
8. Initialise `AsciicastLogger`

**`wireEvents()` — event subscriptions:**
| Event | Handler |
|-------|---------|
| `SessionSwitchedEvent` | Renderer switches active pane |
| `KeyPressedEvent` | Routed to `KeyHandler` |
| `DataReceivedEvent` | Written to active `RingBuffer`, triggers redraw |
| `MacroDoneEvent` | Updates TUI status bar |

**`run()` — main loop:**
- 60 fps ncurses render loop
- Calls `EpollLoop::poll()` for I/O events
- Dispatches pending `EventBus` callbacks

**Persistence:**
- `loadSessions()` — deserialises `sessions.json` on startup
- `saveSessions()` — serialises on exit / `SIGTERM`

**Signal handlers:**
| Signal | Action |
|--------|--------|
| `SIGTERM` | Graceful shutdown, save sessions |
| `SIGINT` | Same as `SIGTERM` |
| `SIGCHLD` | Reap zombie PTY children |
| `SIGWINCH` | Re-query terminal size, force redraw |

---

### Task 5.2 — E2E Tests & Error Recovery

#### Integration Tests — Core (4 tests)
| Test | Description |
|------|-------------|
| PTY + EventBus | PTY spawns shell, output posted via EventBus |
| RingBuffer + PtyProxy | PtyProxy writes fill RingBuffer correctly |
| SessionManager + EpollLoop | Sessions registered with epoll, I/O flows end-to-end |
| EventBus round-trip | Subscribe → publish → callback verified |

#### Integration Tests — Protocol (3 tests)
- SSH connection lifecycle through `ConnectionFactory`
- Serial port open/close through `ConnectionFactory`
- Telnet IAC negotiation through `TelnetConnection`

#### Integration Tests — Logger + Macro (3 tests)
- `AsciicastLogger` records output from live `PtyProxy`
- `MacroEngine` runs recipe against PTY output stream
- Combined logger + macro on same session

#### Error Recovery Tests (10 tests)
| # | Scenario | Expected behaviour |
|---|----------|--------------------|
| 1 | `SessionManager::remove()` with non-existent ID | Returns error, no crash |
| 2 | `PtyProxy::kill()` on already-dead process | No double-kill, no crash |
| 3 | `EpollLoop::removeFd()` with bad/closed fd | Silently ignores, no crash |
| 4 | `RingBuffer` overflow (write past capacity) | Oldest data evicted, no crash |
| 5 | `EventBus::post()` after unsubscribe | Event dropped, no crash |
| 6 | `MacroEngine::feed()` called while in `Idle` state | No state corruption |
| 7 | `AsciicastLogger::write()` before `start()` | Returns error, no crash |
| 8 | `CredentialStore::load()` on non-existent file | Returns empty store, no crash |
| 9 | Double `EpollLoop::start()` | Second call is no-op |
| 10 | `SessionManager::connect()` with null `IConnection` | Returns error, no crash |

#### Smoke Test Binary (`tcm_smoke_test`)
Standalone binary exercising the five most critical paths:
1. Binary launches and exits cleanly (`--version`)
2. `SessionManager` creates and removes a session
3. `RingBuffer` write/read round-trip
4. `EventBus` pub/sub round-trip
5. `CredentialStore` store/retrieve round-trip

---

### Task 5.3 — Packaging & Documentation

| Artifact | Location | Description |
|----------|----------|-------------|
| `README.md` | `tcm/README.md` | Install, quickstart, keybindings reference |
| Recipe format guide | `docs/recipe-format.md` | JSON schema, field descriptions, examples |
| Man page | `docs/tcm.1` | Full `groff` man page with all options and sub-commands |
| Example recipes | `recipes/examples/` | `ont-login.json`, `switch-login.json`, `router-login.json` |
| Install script | `scripts/install.sh` | Installs binary + man page to system prefix |
| Build script | `scripts/build.sh` | Wrapper with `--release`, `--asan`, `--static` flags |
| CMake option | `CMakeLists.txt` | `TCM_STATIC` option for fully static build |

---

## Phase 5 Definition of Done

- [x] All E2E tests pass — **238/238**
- [x] Full workflow verified: `./tcm --version` prints `TCM v1.0.0`
- [x] Error recovery: all 10 edge cases handled without crash
- [x] Binary size: **528K** (limit: 5 MB)
- [x] Startup banner: `TCM v1.0.0` printed to stderr
- [x] README sufficient to get started within 5 minutes
- [x] Man page documents all CLI options and signals
- [x] 3 example recipes are valid JSON (verified with `python3 -m json.tool`)
