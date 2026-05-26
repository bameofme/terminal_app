# Software Architecture Design (SAD)
## TCM — Terminal Connection Manager

**Version:** 1.0  
**Date:** 2025-05-23  
**Status:** Draft  

---

## 1. Architecture Overview

### 1.1 Architectural style

TCM theo kiến trúc **module-based trong single process** với event-driven communication giữa các modules. Không có IPC, không có daemon — một process duy nhất, loosely coupled bằng `EventBus`.

### 1.2 High-level component diagram

```mermaid
graph TB
    subgraph TCM Process
        TUI[TUI Layer\nncurses]
        CORE[Core Engine]
        PROTO[Protocol Adapters]
        LOG[Logger Module]
        MACRO[Macro Engine]
        CRED[Credential Store]
        EB[EventBus]

        TUI <-->|events| EB
        CORE <-->|events| EB
        PROTO -->|data| EB
        LOG <--|data events| EB
        MACRO <-->|trigger/send| EB
        CRED -->|secrets| CORE
    end

    subgraph OS
        PTY[PTY / openpty]
        EPOLL[epoll]
        FS[File System]
    end

    subgraph Remote Devices
        SSH_SRV[SSH Server]
        SERIAL[/dev/ttyUSBx]
        TELNET[Telnet Server]
    end

    PROTO -->|libssh2| SSH_SRV
    PROTO -->|termios| SERIAL
    PROTO -->|BSD socket| TELNET
    CORE --> PTY
    CORE --> EPOLL
    LOG --> FS
    CRED --> FS
```

### 1.3 Layer diagram

```mermaid
graph LR
    L1[TUI Layer\nncurses / SessionListView\nSwitcherOverlay]
    L2[Core Engine\nSessionManager / PtyProxy\nEpollLoop / EventBus]
    L3[Protocol Adapters\nSshConnection / SerialConnection\nTelnetConnection]
    L4[Support Modules\nAsciicastLogger / MacroEngine\nCredentialStore / RingBuffer]
    L5[OS / System\nPTY / epoll / termios\nBSD socket / File I/O]

    L1 --> L2
    L2 --> L3
    L2 --> L4
    L3 --> L5
    L4 --> L5
```

---

## 2. Module Design

### 2.1 Class diagram — Core

```mermaid
classDiagram
    class IConnection {
        <<interface>>
        +connect() int
        +send(data span) int
        +recv(buf span) int
        +disconnect() void
        +getFd() int
        +getState() ConnectionState
    }

    class ConnectionState {
        <<enumeration>>
        Disconnected
        Connecting
        Connected
        Error
    }

    class SshConnection {
        -m_config SshConfig
        -m_session LIBSSH2_SESSION*
        -m_channel LIBSSH2_CHANNEL*
        -m_sock int
        +connect() int
        +send(data span) int
        +recv(buf span) int
        +disconnect() void
        +getFd() int
    }

    class SerialConnection {
        -m_config SerialConfig
        -m_fd int
        -m_savedTermios termios
        +connect() int
        +send(data span) int
        +recv(buf span) int
        +disconnect() void
        +getFd() int
    }

    class TelnetConnection {
        -m_config TelnetConfig
        -m_sock int
        -m_parser IacParser
        +connect() int
        +send(data span) int
        +recv(buf span) int
        +disconnect() void
        +getFd() int
    }

    class Session {
        +id string
        +name string
        +type SessionType
        +conn unique_ptr~IConnection~
        +state SessionState
        +buffer RingBuffer
    }

    class SessionManager {
        -m_sessions vector~Session~
        -m_activeId optional~string~
        -m_bus EventBus&
        +add(session Session) string
        +remove(id string) bool
        +setActive(id string) bool
        +getActive() Session*
        +forEach(cb function) void
        +count() size_t
    }

    IConnection <|.. SshConnection
    IConnection <|.. SerialConnection
    IConnection <|.. TelnetConnection
    Session "1" *-- "1" IConnection
    SessionManager "1" *-- "N" Session
```

### 2.2 Class diagram — PTY Proxy & Event Loop

```mermaid
classDiagram
    class EpollLoop {
        -m_epollFd int
        -m_wakeFd int
        -m_handlers map~int, function~
        -m_running bool
        +addFd(fd, callback) void
        +removeFd(fd) void
        +run() void
        +stop() void
    }

    class PtyProxy {
        -m_masterFd int
        -m_slaveFd int
        -m_childPid pid_t
        -m_active bool
        -m_buffer RingBuffer&
        -m_bus EventBus&
        +spawn(argv) bool
        +write(data span) int
        +setActive(active bool) void
        +masterFd() int
        +kill() void
    }

    class RingBuffer {
        -m_buf array~uint8_t, 65536~
        -m_head size_t
        -m_tail size_t
        +write(data, len) size_t
        +read(buf, len) size_t
        +available() size_t
        +clear() void
        +capacity() size_t
    }

    class EventBus {
        -m_handlers map~type_index, vector~function~~
        -m_mutex shared_mutex
        +subscribe~E~(handler) Token
        +publish~E~(event) void
        +unsubscribe(token) void
    }

    PtyProxy "1" --> "1" RingBuffer : background buffer
    PtyProxy "1" --> "1" EventBus : publish events
    EpollLoop "1" --> "N" PtyProxy : monitor masterFd
    SessionManager "1" --> "N" PtyProxy : one per session
```

### 2.3 Class diagram — TUI

```mermaid
classDiagram
    class IRenderable {
        <<interface>>
        +render(renderer Renderer&) void
        +handleKey(key int) bool
    }

    class Renderer {
        -m_initialized bool
        +init() void
        +teardown() void
        +drawBox(x,y,w,h) void
        +drawText(x,y,str,color) void
        +drawStatusBar(text) void
        +drawTitleBar(text) void
        +getSize() pair~int,int~
        +refresh() void
    }

    class SessionListView {
        -m_sessions vector~SessionInfo~&
        -m_selected int
        -m_searchQuery string
        -m_bus EventBus&
        +render(renderer) void
        +handleKey(key) bool
        -filterSessions() vector~SessionInfo~
    }

    class SwitcherOverlay {
        -m_sessions vector~SessionInfo~&
        -m_visible bool
        -m_bus EventBus&
        +show() void
        +hide() void
        +render(renderer) void
        +handleKey(key) bool
    }

    class KeyHandler {
        -m_bus EventBus&
        +processRawInput(buf, len) void
        -detectEscapeSequence(byte) bool
    }

    IRenderable <|.. SessionListView
    IRenderable <|.. SwitcherOverlay
    SessionListView --> Renderer
    SwitcherOverlay --> Renderer
    KeyHandler --> EventBus
```

### 2.4 Class diagram — Logger & Macro

```mermaid
classDiagram
    class AsciicastLogger {
        -m_file FILE*
        -m_startTime time_point
        -m_sessionName string
        +start(name, width, height) void
        +logOutput(data span) void
        +logInput(data span) void
        +stop() void
        -writeEvent(elapsed, type, data) void
        -jsonEscape(str) string
    }

    class MacroEngine {
        -m_conn IConnection*
        -m_recipe Recipe
        -m_currentStep int
        -m_buffer string
        -m_state MacroState
        -m_bus EventBus&
        +start(variables map) void
        +feed(data span) void
        +stop() void
        -checkMatch() void
        -executeStep(step Step) void
        -interpolate(tmpl, vars) string
    }

    class Recipe {
        +name string
        +description string
        +steps vector~Step~
        +variables map~string,string~
        +load(path) Recipe$
        +loadFromString(json) Recipe$
    }

    class Step {
        +expect string
        +send string
        +timeoutMs int
        +onTimeout OnTimeoutAction
    }

    class MacroState {
        <<enumeration>>
        Idle
        Running
        Done
        Error
    }

    MacroEngine "1" --> "1" Recipe
    Recipe "1" *-- "N" Step
    MacroEngine --> MacroState
    MacroEngine --> IConnection
```

---

## 3. Sequence Diagrams

### 3.1 Connect session flow

```mermaid
sequenceDiagram
    actor User
    participant SLV as SessionListView
    participant SM as SessionManager
    participant CF as ConnectionFactory
    participant CONN as SshConnection
    participant PP as PtyProxy
    participant EL as EpollLoop
    participant EB as EventBus

    User->>SLV: Press Enter on session
    SLV->>SM: connect(sessionId)
    SM->>CF: create(sessionConfig)
    CF-->>SM: SshConnection instance
    SM->>CONN: connect()
    CONN->>CONN: socket() + libssh2 handshake
    CONN-->>SM: 0 (success)
    SM->>PP: spawn(["ssh", host])
    PP->>PP: fork() + openpty()
    PP-->>SM: masterFd
    SM->>EL: addFd(masterFd, onData)
    SM->>EB: publish(SessionConnectedEvent)
    EB-->>SLV: update status badge
    Note over User,SLV: Terminal switches to fullscreen PTY view
```

### 3.2 Session switch flow (Ctrl+])

```mermaid
sequenceDiagram
    actor User
    participant KH as KeyHandler
    participant EB as EventBus
    participant SO as SwitcherOverlay
    participant SM as SessionManager
    participant PP1 as PtyProxy(session1)
    participant PP2 as PtyProxy(session2)

    User->>KH: Press Ctrl+] (0x1D)
    KH->>KH: detectEscapeSequence(0x1D)
    KH->>EB: publish(KeyPressedEvent{isEscape:true})
    EB-->>SO: onEscapeKey()
    SO->>SO: show() — render overlay
    User->>SO: Press '2'
    SO->>EB: publish(SessionSwitchedEvent{from:1, to:2})
    EB-->>SM: onSessionSwitched(from:1, to:2)
    SM->>PP1: setActive(false)
    PP1->>PP1: route output → RingBuffer
    SM->>PP2: setActive(true)
    PP2->>PP2: flush RingBuffer → stdout
    PP2->>PP2: route output → stdout
    SO->>SO: hide()
    Note over User,PP2: User now sees session 2 fullscreen
```

### 3.3 Macro execution flow

```mermaid
sequenceDiagram
    actor User
    participant TUI as TUI
    participant ME as MacroEngine
    participant CONN as IConnection
    participant EB as EventBus
    participant AL as AsciicastLogger

    User->>TUI: Press 'm', select recipe
    TUI->>ME: start(recipe, variables)
    ME->>ME: currentStep = 0
    ME->>ME: startTimer(step[0].timeoutMs)

    loop For each step
        CONN-->>ME: feed(receivedData)
        ME->>ME: buffer += data
        ME->>ME: regex_match(buffer, step[i].expect)

        alt Match found
            ME->>CONN: send(step[i].send)
            ME->>AL: logInput(step[i].send)
            ME->>ME: advance to step[i+1]
        else Timeout
            alt on_timeout == abort
                ME->>EB: publish(MacroFailedEvent)
                ME->>ME: state = Error
            else on_timeout == skip
                ME->>ME: advance to step[i+1]
            end
        end
    end

    ME->>EB: publish(MacroDoneEvent)
    ME->>TUI: notify completion
```

### 3.4 Session logging flow

```mermaid
sequenceDiagram
    participant PP as PtyProxy
    participant EB as EventBus
    participant AL as AsciicastLogger
    participant FS as File System

    Note over PP,FS: Session connected, logging active

    PP->>EB: publish(DataReceivedEvent{sessionId, data})
    EB-->>AL: onDataReceived(event)
    AL->>AL: elapsed = now() - startTime
    AL->>AL: jsonEscape(data)
    AL->>FS: write([elapsed, "o", escaped_data]\n)

    Note over PP,FS: User types command

    PP->>EB: publish(DataReceivedEvent{type:input, data})
    EB-->>AL: onInputReceived(event)
    AL->>FS: write([elapsed, "i", escaped_data]\n)

    Note over PP,FS: Session disconnect

    PP->>EB: publish(SessionRemovedEvent)
    EB-->>AL: onSessionRemoved()
    AL->>FS: fflush() + fclose()
```

---

## 4. Data Flow

### 4.1 Keyboard input flow

```mermaid
flowchart LR
    KB[Keyboard] -->|raw bytes| STDIN[stdin]
    STDIN --> KH[KeyHandler]
    KH -->|0x1D detected| EB1[EventBus\nKeyPressedEvent]
    KH -->|normal key| PTY_W[PtyProxy.write]
    EB1 --> SO[SwitcherOverlay]
    PTY_W --> PTY_M[PTY master fd]
    PTY_M --> CHILD[Child process\nSSH/bash]
```

### 4.2 Output data flow (active session)

```mermaid
flowchart LR
    DEV[Remote Device] -->|protocol| CHILD[Child process]
    CHILD --> PTY_S[PTY slave fd]
    PTY_S --> PTY_M[PTY master fd]
    PTY_M -->|epoll event| EL[EpollLoop callback]
    EL --> PP[PtyProxy]
    PP -->|active=true| STDOUT[stdout / screen]
    PP -->|active=false| RB[RingBuffer]
    EL --> EB[EventBus\nDataReceivedEvent]
    EB --> AL[AsciicastLogger]
    EB --> ME[MacroEngine.feed]
```

### 4.3 Session data model

```mermaid
erDiagram
    SESSION {
        string id PK
        string name
        enum type
        string host_or_device
        int port_or_baud
        string username
        enum auth_type
        string tags
        string notes
    }

    SSH_CONFIG {
        string session_id FK
        int port
        string username
        enum auth_type
        string key_path
        bool enable_tunnel
        int tunnel_local_port
        string tunnel_remote_host
        int tunnel_remote_port
    }

    SERIAL_CONFIG {
        string session_id FK
        string device_path
        int baud_rate
        int data_bits
        enum parity
        enum stop_bits
        enum flow_control
    }

    TELNET_CONFIG {
        string session_id FK
        string host
        int port
        int connect_timeout_ms
    }

    SESSION ||--o| SSH_CONFIG : "has (if type=ssh)"
    SESSION ||--o| SERIAL_CONFIG : "has (if type=serial)"
    SESSION ||--o| TELNET_CONFIG : "has (if type=telnet)"
```

---

## 5. Concurrency Design

### 5.1 Thread model

```mermaid
graph TB
    subgraph Main Thread
        TUI_LOOP[TUI event loop\ngetch / render]
        EPOLL_LOOP[EpollLoop.run\nepoll_wait]
    end

    subgraph Per-session Thread
        KEEPALIVE[SSH keepalive timer]
    end

    subgraph Logger Thread
        LOG_WRITER[Async log writer\ncondition_variable wait]
    end

    subgraph Child Processes
        SSH_PROC[ssh child process]
        SHELL_PROC[bash / sh]
    end

    TUI_LOOP -->|read getch| KEYBOARD
    EPOLL_LOOP -->|read PTY fds| PTY_FDS[PTY master fds]
    EPOLL_LOOP -->|post to| LOG_QUEUE[Log queue\nmutex-protected]
    LOG_QUEUE --> LOG_WRITER
    KEEPALIVE -->|libssh2_keepalive_send| SSH_SOCK[SSH socket]
```

**Quy tắc:**
- Main thread: TUI rendering + epoll loop (single-threaded, không cần lock)
- Logger thread: async write từ queue, không block main thread
- Keepalive thread: 1 per SSH session, lightweight timer
- EventBus publish/subscribe: thread-safe với `shared_mutex`
- RingBuffer: single-producer (epoll thread) / single-consumer (main thread) → dùng atomic

### 5.2 Synchronization points

| Resource | Protection | Notes |
|---|---|---|
| `SessionManager::m_sessions` | `std::mutex` | Add/remove từ TUI thread |
| `EventBus::m_handlers` | `std::shared_mutex` | Read-heavy (publish) |
| `RingBuffer` | `std::atomic` head/tail | SPSC lock-free |
| Log queue | `std::mutex` + `condition_variable` | Async writer |
| `SwitcherOverlay::m_visible` | Main thread only | No lock needed |

---

## 6. Error Handling Strategy

### 6.1 Error taxonomy

```mermaid
graph TD
    ERR[Errors] --> CONN_ERR[Connection errors]
    ERR --> IO_ERR[I/O errors]
    ERR --> CONFIG_ERR[Config errors]
    ERR --> SYSTEM_ERR[System errors]

    CONN_ERR --> AUTH_FAIL[Auth failure\n→ show error, prompt retry]
    CONN_ERR --> TIMEOUT[Timeout\n→ mark session error, offer reconnect]
    CONN_ERR --> DISCONNECT[Unexpected disconnect\n→ detect, mark error, buffer preserved]

    IO_ERR --> LOG_FULL[Disk full\n→ disable logging, warn user]
    IO_ERR --> PTY_ERROR[PTY read error\n→ session cleanup]

    CONFIG_ERR --> INVALID_JSON[Invalid JSON\n→ show parse error, use defaults]
    CONFIG_ERR --> MISSING_FIELD[Missing required field\n→ show validation error]

    SYSTEM_ERR --> SIGCHLD[Child exit\n→ SIGCHLD handler, reap, mark idle]
    SYSTEM_ERR --> ENXIO[Serial unplug\n→ ENXIO, mark error]
```

### 6.2 Error propagation

- **Connection errors:** Adapter trả về error code → `SessionManager` nhận, publish `SessionErrorEvent` → TUI hiện badge `✗`
- **Fatal system errors:** Log to stderr + syslog, graceful shutdown
- **Non-fatal errors:** Log + notify user qua status bar message (tự clear sau 5 giây)

---

## 7. Configuration Schema

### 7.1 Main config: `~/.config/tcm/config.json`

```json
{
  "version": 1,
  "log_dir": "~/.local/share/tcm/logs",
  "max_log_size_mb": 50,
  "ssh_known_hosts": "~/.ssh/known_hosts",
  "ring_buffer_size_kb": 64,
  "keepalive_interval_s": 30,
  "theme": "default"
}
```

### 7.2 Sessions: `~/.config/tcm/sessions.json`

Xem SRS Section 5.2 cho schema đầy đủ.

---

## 8. Build Architecture

```mermaid
graph TB
    subgraph CMake targets
        LIB_CORE[tcm_core\nstatic lib]
        LIB_PROTO[tcm_protocol\nstatic lib]
        LIB_TUI[tcm_tui\nstatic lib]
        LIB_LOGGER[tcm_logger\nstatic lib]
        LIB_MACRO[tcm_macro\nstatic lib]
        BIN[tcm\nexecutable]
        TEST_CORE[test_core]
        TEST_PROTO[test_protocol]
        TEST_TUI[test_tui]
        TEST_MACRO[test_macro]
    end

    LIB_CORE --> BIN
    LIB_PROTO --> BIN
    LIB_TUI --> BIN
    LIB_LOGGER --> BIN
    LIB_MACRO --> BIN

    LIB_CORE --> TEST_CORE
    LIB_PROTO --> TEST_PROTO
    LIB_TUI --> TEST_TUI
    LIB_MACRO --> TEST_MACRO

    subgraph External deps
        LIBSSH2[libssh2]
        NCURSES[ncurses]
        JSON[nlohmann_json]
        GTEST[gtest + gmock]
        OPENSSL[libcrypto]
    end

    LIBSSH2 --> LIB_PROTO
    NCURSES --> LIB_TUI
    JSON --> LIB_CORE
    GTEST --> TEST_CORE
    GTEST --> TEST_PROTO
    OPENSSL --> LIB_MACRO
```

### 8.1 Build commands

```bash
# Debug build với sanitizers
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DTCM_SANITIZE=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure

# Release build
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j$(nproc)

# Static binary
cmake -B build-static -DCMAKE_BUILD_TYPE=Release -DTCM_STATIC=ON
cmake --build build-static -j$(nproc)
# Output: build-static/tcm (~4MB static binary)
```

---

## 9. Directory Structure (Complete)

```
tcm/
├── CMakeLists.txt
├── README.md
├── .clang-tidy
├── .clang-format
├── scripts/
│   ├── build.sh
│   └── install.sh
├── include/
│   ├── core/
│   │   ├── IConnection.hpp
│   │   ├── Session.hpp
│   │   ├── SessionManager.hpp
│   │   ├── EventBus.hpp
│   │   ├── PtyProxy.hpp
│   │   ├── EpollLoop.hpp
│   │   └── RingBuffer.hpp
│   ├── protocol/
│   │   ├── SshConnection.hpp
│   │   ├── SshConfig.hpp
│   │   ├── SerialConnection.hpp
│   │   ├── SerialConfig.hpp
│   │   ├── TelnetConnection.hpp
│   │   ├── TelnetConfig.hpp
│   │   └── IacParser.hpp
│   ├── tui/
│   │   ├── IRenderable.hpp
│   │   ├── Renderer.hpp
│   │   ├── SessionListView.hpp
│   │   ├── SwitcherOverlay.hpp
│   │   └── KeyHandler.hpp
│   ├── logger/
│   │   └── AsciicastLogger.hpp
│   ├── macro/
│   │   ├── MacroEngine.hpp
│   │   └── Recipe.hpp
│   └── credential/
│       └── CredentialStore.hpp
├── src/
│   ├── main.cpp
│   ├── core/   [*.cpp mirrors include/core/]
│   ├── protocol/
│   ├── tui/
│   ├── logger/
│   ├── macro/
│   └── credential/
├── tests/
│   ├── core/
│   ├── protocol/
│   ├── tui/
│   ├── logger/
│   └── macro/
├── config/
│   ├── sessions.json.example
│   └── config.json.example
├── recipes/
│   └── examples/
│       ├── ont-login.json
│       ├── router-login.json
│       └── switch-login.json
└── docs/
    ├── SRS-TCM.md          ← requirements
    ├── SAD-TCM.md          ← this document
    ├── phase-1-core-foundation.md
    ├── phase-2-protocol-adapters.md
    ├── phase-3-4-5-plans.md
    └── recipe-format.md
```
