# Phase 3 — TUI Layer

**Project:** TCM — Terminal Connection Manager  
**Depends on:** Phase 1 + Phase 2 complete  
**Duration:** ~2 tuần

---

## Mục tiêu phase

Implement toàn bộ giao diện người dùng dùng ncurses: session list khi khởi động, PTY rendering khi đang connected, và switcher overlay khi nhấn `Ctrl+]`. Sau phase này, TCM có thể dùng được end-to-end từ màn hình terminal.

---

## Cấu trúc thư mục

```
tcm/
├── include/
│   └── tui/
│       ├── IRenderable.hpp
│       ├── Renderer.hpp
│       ├── SessionListView.hpp
│       ├── SwitcherOverlay.hpp
│       └── KeyHandler.hpp
├── src/
│   └── tui/
│       ├── Renderer.cpp
│       ├── SessionListView.cpp
│       ├── SwitcherOverlay.cpp
│       └── KeyHandler.cpp
└── tests/
    └── tui/
        ├── RendererTest.cpp
        ├── SessionListViewTest.cpp
        └── SwitcherOverlayTest.cpp
```

---

## Layout màn hình

### Session list (không có session active)
```
┌──────────────────────────────────────────────────────────┐
│  TCM v1.0  —  Terminal Connection Manager                │  ← title bar (1 line)
├──────────────────────────────────────────────────────────┤
│                                                          │
│  [SSH]    router-lab-01      192.168.1.1     ● Connected │
│  [SSH]    ont-broadcom-01    10.0.0.5        ○ Idle      │
│  [SER]    /dev/ttyUSB0       115200          ○ Idle      │
│  [TEL]    switch-mgmt        172.16.0.1      ○ Idle      │
│                                                          │
│                                                          │
├──────────────────────────────────────────────────────────┤
│  [Enter] connect  [a] add  [d] delete  [/] search  [q] quit │  ← status bar
└──────────────────────────────────────────────────────────┘
```

### Switcher overlay (Ctrl+] khi đang trong session)
```
                    ┌──────────────────────────┐
                    │  Switch session           │
                    ├──────────────────────────┤
                    │  1  router-lab-01    ●   │
                    │  2  ont-broadcom-01  ●   │  ← active session
                    │  3  /dev/ttyUSB0    ○   │
                    │  4  [new session]        │
                    ├──────────────────────────┤
                    │  1-9: switch  Esc: cancel │
                    └──────────────────────────┘
```

---

## Module 3.1 — Renderer (ncurses wrapper)

### Tasks

#### Task 3.1.1 — Viết test cho Renderer
- [ ] Test `init()` không throw
- [ ] Test `drawBox(x, y, w, h)` không crash khi coords valid
- [ ] Test `drawBox()` throw/return error khi out of bounds
- [ ] Test `drawText(x, y, str)` truncate nếu quá width
- [ ] Test `teardown()` restore terminal settings
- [ ] Test `getSize()` trả về đúng cols/rows của terminal

> **Ghi chú:** TUI tests dùng `TERM=xterm-256color` trong test environment, hoặc mock ncurses functions.

#### Task 3.1.2 — Implement Renderer
- [ ] Constructor: `initscr()`, `start_color()`, `noecho()`, `cbreak()`, `keypad(stdscr, TRUE)`, `curs_set(0)`
- [ ] RAII destructor: `endwin()`
- [ ] Color pairs: `init_pair(1, COLOR_GREEN, COLOR_BLACK)` v.v.
- [ ] `drawBox(win, x, y, w, h)` → `box()` với border chars
- [ ] `drawText(win, x, y, str, colorPair)` → `mvwaddstr()`
- [ ] `drawStatusBar(text)` → bottom line với reverse color
- [ ] `drawTitleBar(text)` → top line
- [ ] `refresh()` → `wrefresh()`
- [ ] `getSize()` → `getmaxyx(stdscr, rows, cols)`
- [ ] `IRenderable` interface: `render(Renderer&)` pure virtual

#### Task 3.1.3 — SIGWINCH handling
- [ ] Install `SIGWINCH` handler → set flag
- [ ] Main loop check flag → `endwin()` + `refresh()` + re-render
- [ ] Re-send `TIOCSWINSZ` ke tất cả active PTYs

### Định nghĩa hoàn thành
- [ ] Tests pass: `ctest -R Renderer`
- [ ] Terminal không bị "broken" sau khi Renderer teardown
- [ ] Resize window → TUI adapt đúng

---

## Module 3.2 — SessionListView

### Tasks

#### Task 3.2.1 — Viết test cho SessionListView
- [ ] Test render với 0 sessions → hiện "No sessions. Press [a] to add."
- [ ] Test render với 3 sessions → 3 rows đúng format
- [ ] Test `moveDown()` / `moveUp()` thay đổi selected index
- [ ] Test selected index không vượt quá bounds (0 và count-1)
- [ ] Test `getSelected()` trả về đúng session id
- [ ] Test search filter `/` → chỉ hiện sessions match

#### Task 3.2.2 — Implement SessionListView
- [ ] `render(Renderer&)` → vẽ list với title bar + status bar
- [ ] Mỗi row: `[TYPE]  name  host/device  status_indicator`
- [ ] Highlight selected row với `A_REVERSE`
- [ ] Status indicator: `●` (COLOR_GREEN) connected, `○` (COLOR_WHITE) idle, `✗` (COLOR_RED) error
- [ ] `handleKey(int key)` → j/k/arrow/Enter/a/d/q

#### Task 3.2.3 — Search/filter
- [ ] Nhấn `/` → vào search mode, bottom line hiện search prompt
- [ ] Gõ text → filter sessions theo name hoặc host
- [ ] `Esc` → exit search, hiện lại tất cả
- [ ] Enter trong search → connect session đang highlighted

### Định nghĩa hoàn thành
- [ ] Tests pass: `ctest -R SessionListView`
- [ ] Navigate list bằng j/k và arrow keys
- [ ] Search filter hoạt động real-time

---

## Module 3.3 — SwitcherOverlay

### Tasks

#### Task 3.3.1 — Viết test cho SwitcherOverlay
- [ ] Test overlay hiện centered trên màn hình
- [ ] Test nhấn `1`-`9` → publish `SessionSwitchedEvent` đúng session
- [ ] Test `Esc` → dismiss overlay, không switch
- [ ] Test nhấn số vượt quá số sessions → ignore
- [ ] Test overlay chỉ hiện sessions đang connected

#### Task 3.3.2 — Implement SwitcherOverlay
- [ ] `show()` → render overlay popup ở giữa màn hình
- [ ] `hide()` → clear popup area, restore terminal content
- [ ] `handleKey(int key)` → `'1'`-`'9'` map đến session index
- [ ] Hiện tối đa 9 sessions (1-9), nếu nhiều hơn scroll
- [ ] `Esc` → `hide()` không action
- [ ] Subscribe `KeyPressedEvent{isEscape: true}` từ EventBus

#### Task 3.3.3 — Intercept Ctrl+] trong PtyProxy
- [ ] `PtyProxy` detect byte `0x1D` trong stdin stream
- [ ] Publish `KeyPressedEvent{key: 0x1D, isEscape: true}`
- [ ] Không forward `0x1D` vào PTY slave

### Định nghĩa hoàn thành
- [ ] Tests pass: `ctest -R SwitcherOverlay`
- [ ] `Ctrl+]` → overlay xuất hiện ngay lập tức (< 16ms)
- [ ] Nhấn số → switch session, overlay hide
- [ ] `Esc` → dismiss, vẫn ở session cũ

---

## Module 3.4 — Add/Edit session form

### Tasks
- [ ] Form nhập: name, type (SSH/Serial/Telnet), host/device, port/baud, username
- [ ] Validate input trước khi save
- [ ] Save vào `sessions.json`
- [ ] Cancel → không lưu, về session list

---

## Definition of Done — Phase 3

- [ ] **Tests pass**: `ctest -R "Renderer|SessionList|Switcher"`
- [ ] **Full flow**: Mở TCM → thấy session list → chọn SSH session → connect → gõ lệnh → `Ctrl+]` → switch sang Serial session → về SSH bằng `Ctrl+]`
- [ ] **Resize**: Terminal resize không crash, layout adapt
- [ ] **No artifacts**: `endwin()` sau khi quit restore terminal hoàn toàn (kiểm tra bằng `reset`)

---

---

# Phase 4 — Logger & Macro Engine

**Depends on:** Phase 1 + Phase 2 + Phase 3 complete  
**Duration:** ~2 tuần

---

## Mục tiêu phase

Implement asciicast v2 session logging và expect-like macro engine. Sau phase này, mọi session được ghi lại và có thể replay; người dùng có thể viết JSON recipe để automate login/command sequences.

---

## Cấu trúc thư mục

```
tcm/
├── include/
│   ├── logger/
│   │   └── AsciicastLogger.hpp
│   ├── macro/
│   │   ├── MacroEngine.hpp
│   │   └── Recipe.hpp
│   └── credential/
│       └── CredentialStore.hpp
├── src/
│   ├── logger/
│   │   └── AsciicastLogger.cpp
│   ├── macro/
│   │   ├── MacroEngine.cpp
│   │   └── Recipe.cpp
│   └── credential/
│       └── CredentialStore.cpp
└── tests/
    ├── logger/
    │   └── AsciicastLoggerTest.cpp
    └── macro/
        ├── MacroEngineTest.cpp
        └── RecipeTest.cpp
```

---

## Module 4.1 — AsciicastLogger

### asciicast v2 format

```jsonc
// Header (line 1)
{"version": 2, "width": 220, "height": 50, "timestamp": 1716422400, "title": "router-lab-01"}

// Events (line 2+)
[0.0,    "o", "\r\nBusyBox v1.36.0\r\n"]
[1.234,  "o", "# "]
[3.891,  "i", "show version\n"]
[4.100,  "o", "ZTE F670L firmware 3.2.1\r\n"]
```

- `"o"` = output (device → screen)
- `"i"` = input (user keystrokes)

### Tasks

#### Task 4.1.1 — Viết test cho AsciicastLogger
- [ ] Test `start()` tạo file với header hợp lệ
- [ ] Test `logOutput(data)` append event với timestamp đúng định dạng
- [ ] Test `logInput(data)` append event type "i"
- [ ] Test timestamp tăng dần (không bao giờ giảm)
- [ ] Test `stop()` flush và close file
- [ ] Test file rotation khi > maxSizeBytes
- [ ] Test file được đặt tên theo format: `{session_name}_{timestamp}.cast`
- [ ] Test parse file output là valid JSON trên mỗi line

#### Task 4.1.2 — Implement AsciicastLogger
- [ ] `start(sessionName, termWidth, termHeight)` → open file, write header JSON
- [ ] `logOutput(data)` → append `[elapsed, "o", escaped_data]`
- [ ] `logInput(data)` → append `[elapsed, "i", escaped_data]`
- [ ] Elapsed time dùng `std::chrono::steady_clock` từ lúc `start()`
- [ ] JSON escape: `\\`, `\"`, `\r`, `\n`, `\t` và non-ASCII
- [ ] `stop()` → `fflush()` + `fclose()`

#### Task 4.1.3 — File rotation
- [ ] Check file size sau mỗi N writes (N=100)
- [ ] Nếu size > max (default 50MB) → close, open file mới với suffix `-part2`
- [ ] Config: `logDir`, `maxFileSizeBytes`

#### Task 4.1.4 — Refactor
- [ ] Async write: queue events, dedicated writer thread
- [ ] `std::condition_variable` để wake writer thread
- [ ] Graceful shutdown: drain queue trước khi exit

### Định nghĩa hoàn thành
- [ ] Tests pass: `ctest -R AsciicastLogger`
- [ ] File output valid: `asciinema play session.cast` replay đúng
- [ ] Timestamp precision: microsecond (3 decimal places)
- [ ] Không mất data khi app crash (fflush thường xuyên)

---

## Module 4.2 — Recipe & MacroEngine

### Recipe format (JSON)

```json
{
  "name": "ont-login",
  "description": "Login vào ONT và chạy show version",
  "steps": [
    {
      "expect":     "login:",
      "send":       "admin\n",
      "timeout_ms": 5000,
      "on_timeout": "abort"
    },
    {
      "expect":     "Password:",
      "send":       "{password}\n",
      "timeout_ms": 3000
    },
    {
      "expect":     "#",
      "send":       "show version\n",
      "timeout_ms": 2000
    },
    {
      "expect":     "#",
      "send":       "",
      "timeout_ms": 1000,
      "on_match":   "done"
    }
  ],
  "variables": {
    "password": ""
  }
}
```

### Tasks

#### Task 4.2.1 — Viết test cho Recipe parser
- [ ] Test parse valid JSON → `Recipe` struct đúng fields
- [ ] Test parse missing required field → return error
- [ ] Test `{variable}` substitution trong send string
- [ ] Test steps array ordered đúng

#### Task 4.2.2 — Viết test cho MacroEngine (core logic)
- [ ] Test `feed("login:")` → trigger step 1, return `send("admin\n")`
- [ ] Test `feed("Password:")` sau step 1 → trigger step 2
- [ ] Test `feed(noise_data)` không match → không trigger
- [ ] Test timeout (mock clock) → on_timeout abort/skip
- [ ] Test `{variable}` replaced trước khi send
- [ ] Test run to completion → state `Done`
- [ ] Test `reset()` → về step 0

#### Task 4.2.3 — Implement Recipe
- [ ] `Recipe::load(filePath)` → parse JSON với nlohmann/json
- [ ] `Recipe::loadFromString(json)` → parse từ string (dễ test hơn)
- [ ] Struct: `name`, `description`, `steps[]`, `variables{}`
- [ ] `Step` struct: `expect` (regex string), `send`, `timeout_ms`, `on_timeout` enum

#### Task 4.2.4 — Implement MacroEngine
- [ ] `MacroEngine(IConnection*, Recipe)` constructor
- [ ] `start(variables)` → begin executing recipe, set step = 0
- [ ] `feed(data)` → append vào internal buffer, check regex match
- [ ] `std::regex` match trên buffer
- [ ] Nếu match: `conn->send(step.send)`, advance step
- [ ] Nếu không match trong `timeout_ms`: execute `on_timeout`
- [ ] `on_timeout = "abort"` → state Error, publish `MacroFailedEvent`
- [ ] `on_timeout = "skip"` → advance step
- [ ] Khi tất cả steps done → state Done, publish `MacroDoneEvent`
- [ ] `stop()` → cancel recipe, stop timer

#### Task 4.2.5 — Timer implementation
- [ ] Per-step countdown timer dùng `std::chrono`
- [ ] Check trong `feed()`: nếu elapsed > timeout → trigger on_timeout
- [ ] Reset timer khi advance to next step

#### Task 4.2.6 — Refactor
- [ ] Variable interpolation: `{var}` → replace từ variables map
- [ ] Conditional branching: `on_match: "goto:step_name"`
- [ ] Named steps thay vì index-based

### Định nghĩa hoàn thành
- [ ] Tests pass: `ctest -R "Recipe|MacroEngine"`
- [ ] Chạy `ont-login.json` recipe trên real ONT: login thành công, `show version` output nhận được
- [ ] Timeout behavior đúng (abort vs skip)
- [ ] Variable substitution work

---

## Module 4.3 — CredentialStore

### Tasks

#### Task 4.3.1 — Viết test
- [ ] Test `store(id, secret)` → retrieve đúng secret
- [ ] Test `retrieve(unknown_id)` → return `std::nullopt`
- [ ] Test `remove(id)` → retrieve trả về nullopt
- [ ] Test credential không lưu plaintext trong file

#### Task 4.3.2 — Implement CredentialStore
- [ ] Backend 1: `libsecret` (GNOME keyring) nếu available
- [ ] Backend 2: Fallback encrypt với AES-256 + user passphrase (dùng OpenSSL)
- [ ] `store(sessionId, secret)` → save encrypted
- [ ] `retrieve(sessionId)` → decrypt và return
- [ ] `remove(sessionId)` → delete entry

### Định nghĩa hoàn thành
- [ ] Tests pass: `ctest -R CredentialStore`
- [ ] Credential file không chứa plaintext khi mở bằng hex editor
- [ ] Works trên Ubuntu (libsecret) và headless server (file backend)

---

## Definition of Done — Phase 4

- [ ] **Tests pass**: `ctest -R "Asciicast|Recipe|MacroEngine|Credential"`
- [ ] **Logging**: Mỗi session tạo ra `.cast` file, `asciinema play` replay được
- [ ] **Macro**: Recipe `ont-login.json` automate đăng nhập ONT thành công
- [ ] **Credentials**: Password lưu encrypted, không prompt mỗi lần connect

---

---

# Phase 5 — Integration & Polish

**Depends on:** Phase 1–4 complete  
**Duration:** ~2 tuần

---

## Mục tiêu phase

Wire tất cả modules lại thành một binary hoạt động hoàn chỉnh. Viết integration tests end-to-end. Xử lý edge cases, error recovery, và packaging để distribute.

---

## Module 5.1 — Main wiring (main.cpp)

### Tasks

#### Task 5.1.1 — Application startup
- [ ] Parse CLI args: `tcm [--config path] [--log-dir path] [--version]`
- [ ] Load `sessions.json` từ `~/.config/tcm/sessions.json`
- [ ] Init EventBus (singleton hoặc dependency injection)
- [ ] Init Renderer (ncurses)
- [ ] Init SessionManager
- [ ] Init AsciicastLogger (per-session)
- [ ] Show SessionListView

#### Task 5.1.2 — Event wiring
- [ ] `SessionSwitchedEvent` → PtyProxy.setActive() + Renderer re-render
- [ ] `DataReceivedEvent` → AsciicastLogger.logOutput()
- [ ] `KeyPressedEvent{escape}` → SwitcherOverlay.show()
- [ ] `MacroDoneEvent` → notify user qua status bar
- [ ] `SessionRemovedEvent` → cleanup + update list

#### Task 5.1.3 — Signal handling
- [ ] `SIGTERM` / `SIGINT` → graceful shutdown: disconnect all, flush logs, endwin
- [ ] `SIGCHLD` → reap zombie child processes từ PtyProxy
- [ ] `SIGWINCH` → resize handler

#### Task 5.1.4 — Config load/save
- [ ] Đọc `sessions.json`: array of session configs
- [ ] Write khi user add/delete session
- [ ] Schema validation với nlohmann/json

---

## Module 5.2 — End-to-end tests

### Tasks

#### Task 5.2.1 — E2E test: SSH session lifecycle
- [ ] Spawn Docker OpenSSH server
- [ ] TCM connect SSH session
- [ ] Send `echo hello`
- [ ] Assert recv `hello\n`
- [ ] Assert `session.cast` file có events
- [ ] Disconnect → assert session state Idle

#### Task 5.2.2 — E2E test: session switching
- [ ] Open 3 SSH sessions (3 Docker containers)
- [ ] Switch S1 → S2 → S3 → S1 liên tục 50 lần
- [ ] Assert không có data từ session này lẫn sang session khác
- [ ] Assert buffer background sessions có output đúng

#### Task 5.2.3 — E2E test: serial loopback
- [ ] Tạo virtual serial pair: `socat PTY,link=/tmp/v0 PTY,link=/tmp/v1`
- [ ] TCM connect `/tmp/v0`
- [ ] Send data
- [ ] Read từ `/tmp/v1` → assert same data

#### Task 5.2.4 — E2E test: macro recipe
- [ ] Spawn mock login server (Python script print "login: " rồi đợi)
- [ ] Load `ont-login.json` recipe
- [ ] Run macro
- [ ] Assert tất cả steps complete trong timeout

#### Task 5.2.5 — Stress test
- [ ] 10 sessions đồng thời, mỗi session gửi 1KB/s liên tục
- [ ] Run 10 phút
- [ ] Assert: 0 data drops, 0 crashes, valgrind clean

---

## Module 5.3 — Error recovery

### Tasks
- [ ] SSH disconnect giữa chừng → hiện error badge, offer reconnect (nhấn `r`)
- [ ] Serial device unplug → detect ENXIO, mark session error
- [ ] Log file full disk → disable logging, hiện warning, không crash
- [ ] PTY child crash → SIGCHLD → cleanup, không zombie

---

## Module 5.4 — Packaging

### Tasks

#### Task 5.4.1 — Static binary
- [ ] CMake option `TCM_STATIC=ON` → link libssh2, ncurses, openssl statically
- [ ] Test binary chạy trên Ubuntu 22.04, 24.04, Debian 12
- [ ] File size < 5MB

#### Task 5.4.2 — Install script
- [ ] `install.sh`: copy binary → `/usr/local/bin/tcm`
- [ ] Create `~/.config/tcm/` với default `sessions.json`
- [ ] Create `~/.local/share/tcm/logs/`

#### Task 5.4.3 — Documentation
- [ ] `README.md`: install, quick start, key bindings
- [ ] `man tcm` man page
- [ ] `docs/recipe-format.md`: recipe JSON spec
- [ ] `recipes/examples/`: 3 example recipes (ONT login, switch login, router login)

---

## Definition of Done — Phase 5

### Functional
- [ ] **Tất cả E2E tests pass** trong CI
- [ ] **Full workflow**: Install → add session → connect SSH → `Ctrl+]` switch → run macro → view log
- [ ] **Error recovery**: Unplug serial → không crash, reconnect được

### Non-functional
- [ ] **Binary size** < 5MB static
- [ ] **Startup time** < 200ms từ launch đến session list
- [ ] **Memory** < 20MB RSS với 5 connected sessions
- [ ] **Valgrind** clean cho full E2E test run

### Documentation
- [ ] README đủ để người mới dùng trong 5 phút
- [ ] Man page có đầy đủ options và key bindings
- [ ] Ít nhất 2 example recipes trong `recipes/examples/`

---

## Global Definition of Done — Toàn bộ project

Project TCM được coi là **production-ready** khi:

1. **Tất cả phases hoàn thành** (Phase 1–5 DoD đều đạt)
2. **Test coverage** > 80% toàn bộ codebase (`gcov` report)
3. **Zero known bugs** trong backlog critical/high priority
4. **Static analysis** pass: `clang-tidy` với `.clang-tidy` config
5. **Real device validation**:
   - SSH vào ít nhất 2 loại ONT khác nhau
   - Serial console vào ít nhất 1 ONT
   - Macro recipe login thành công không intervention
6. **Một người khác** (không phải tác giả) có thể cài và dùng chỉ qua README
