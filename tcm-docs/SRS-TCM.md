# Software Requirements Specification (SRS)
## TCM — Terminal Connection Manager

**Version:** 1.0  
**Date:** 2025-05-23  
**Status:** Draft  

---

## 1. Introduction

### 1.1 Purpose

Tài liệu này mô tả đầy đủ các yêu cầu chức năng và phi chức năng của **TCM — Terminal Connection Manager**, một ứng dụng CLI/TUI chạy trên Linux để quản lý nhiều kết nối SSH, Serial console và Telnet đồng thời.

### 1.2 Scope

TCM là công cụ dành cho kỹ sư firmware/network cần thường xuyên kết nối đến nhiều thiết bị nhúng (ONT, router, switch, access point) thông qua các giao thức khác nhau. TCM thay thế workflow hiện tại dùng nhiều cửa sổ terminal riêng lẻ bằng một ứng dụng duy nhất có khả năng switch nhanh giữa các sessions.

### 1.3 Definitions

| Thuật ngữ | Định nghĩa |
|---|---|
| Session | Một kết nối đến một thiết bị cụ thể qua một giao thức cụ thể |
| Active session | Session đang được hiển thị fullscreen và nhận keyboard input |
| Background session | Session đang connected nhưng không phải foreground |
| PTY | Pseudo Terminal — software terminal emulation |
| Recipe | File JSON định nghĩa một automation script dạng expect |
| Macro | Một recipe đang được thực thi |
| Adapter | Class implement `IConnection` cho một giao thức cụ thể |

### 1.4 References

- RFC 854 — Telnet Protocol
- RFC 4251, 4252, 4253 — SSH Protocol
- asciicast v2 format specification
- POSIX termios specification
- libssh2 documentation

---

## 2. Overall Description

### 2.1 Product perspective

TCM là một standalone CLI application. Không có server-side component, không có network daemon, không require external services. Mọi state được lưu local trong `~/.config/tcm/`.

```
┌────────────────────────────────────────────────────────┐
│                     User Terminal                       │
│  ┌──────────────────────────────────────────────────┐  │
│  │                  TCM Process                      │  │
│  │  ┌──────────┐  ┌───────────┐  ┌──────────────┐  │  │
│  │  │ TUI Layer│  │   Core    │  │   Protocol   │  │  │
│  │  │(ncurses) │  │  Engine   │  │   Adapters   │  │  │
│  │  └──────────┘  └───────────┘  └──────────────┘  │  │
│  └──────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────┘
         ↕ SSH          ↕ /dev/ttyUSB0     ↕ TCP:23
    Remote device       ONT UART         Switch Telnet
```

### 2.2 User classes

**Primary user:** Kỹ sư firmware/embedded Linux, thành thạo terminal, quen dùng vim/tmux keybindings.

**Secondary user:** Kỹ sư network, cần connect nhiều thiết bị qua SSH/Telnet.

### 2.3 Operating environment

- OS: Linux (Ubuntu 20.04+, Debian 11+, bất kỳ distro nào có glibc 2.31+)
- Terminal: bất kỳ terminal emulator hỗ trợ xterm-256color
- Không require X11 hay GUI
- Không require root (trừ khi cần access `/dev/ttyUSBx` — giải quyết bằng group `dialout`)

---

## 3. Functional Requirements

### 3.1 Session Management

#### FR-SM-001: Add session
- **Description:** Người dùng có thể thêm session mới với type SSH, Serial, hoặc Telnet
- **Input:** name, type, host/device, port/baud, username, (optional) password/key
- **Output:** Session xuất hiện trong list với status Idle
- **Priority:** Critical

#### FR-SM-002: Delete session
- **Description:** Người dùng có thể xóa session. Nếu session đang connected, disconnect trước khi xóa
- **Input:** Session id
- **Output:** Session biến mất khỏi list, connection đóng sạch
- **Priority:** Critical

#### FR-SM-003: Connect session
- **Description:** Người dùng chọn session từ list và kết nối
- **Input:** Enter trên session được chọn
- **Output:** Màn hình chuyển sang fullscreen terminal của session, status → Connected
- **Priority:** Critical

#### FR-SM-004: Disconnect session
- **Description:** Người dùng disconnect session đang active (bằng cách exit shell hoặc lệnh disconnect trong session)
- **Output:** Màn hình quay về SessionList, status → Idle
- **Priority:** Critical

#### FR-SM-005: Persistent sessions
- **Description:** Session config được lưu vào `~/.config/tcm/sessions.json`, tự động load khi khởi động
- **Priority:** High

#### FR-SM-006: Session metadata
- **Description:** Mỗi session lưu: name, type, host/device, port, username, tags (tối đa 5 tags), notes
- **Priority:** Medium

### 3.2 Multi-session Switching

#### FR-SW-001: Background sessions
- **Description:** Khi switch sang session khác, session cũ vẫn connected ở background. Số sessions background không giới hạn
- **Priority:** Critical

#### FR-SW-002: Switch overlay
- **Description:** Nhấn `Ctrl+]` khi đang trong session → hiện switcher overlay. Overlay hiện tất cả sessions, đánh số 1-9. Nhấn số để switch
- **Priority:** Critical

#### FR-SW-003: Buffer background output
- **Description:** Output từ background sessions được buffer trong RAM (64KB per session). Khi switch về, buffer được hiển thị
- **Priority:** Critical

#### FR-SW-004: Switch từ session list
- **Description:** Từ SessionList, nhấn Enter trên session đã connected → switch về session đó (không reconnect)
- **Priority:** High

### 3.3 Protocol Support

#### FR-SSH-001: SSH connect
- **Description:** Connect đến SSH server với username/password hoặc private key
- **Input:** host, port (default 22), username, auth method
- **Priority:** Critical

#### FR-SSH-002: SSH key authentication
- **Description:** Hỗ trợ private key file (RSA, ED25519). Path đến key file được lưu trong session config
- **Priority:** Critical

#### FR-SSH-003: SSH tunnel
- **Description:** Hỗ trợ local port forwarding: `-L localPort:remoteHost:remotePort`
- **Priority:** Medium

#### FR-SSH-004: SSH keep-alive
- **Description:** Gửi keep-alive packet mỗi 30 giây để tránh session timeout trên firewall
- **Priority:** High

#### FR-SER-001: Serial connect
- **Description:** Open serial device `/dev/ttyUSBx` hoặc `/dev/ttySx` với baud rate, data bits, parity, stop bits, flow control configurable
- **Priority:** Critical

#### FR-SER-002: Baud rates
- **Description:** Hỗ trợ: 300, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600
- **Priority:** Critical

#### FR-TEL-001: Telnet connect
- **Description:** Connect TCP đến host:23 (configurable port), thực hiện IAC option negotiation cơ bản
- **Priority:** High

#### FR-TEL-002: IAC negotiation
- **Description:** Handle WILL/WONT/DO/DONT cho các options: ECHO, SUPPRESS-GO-AHEAD, TERMINAL-TYPE, WINDOW-SIZE. Strip IAC bytes khỏi data stream
- **Priority:** High

### 3.4 Session Logging

#### FR-LOG-001: Auto-logging
- **Description:** Mỗi session được tự động log vào file khi connect. Log directory: `~/.local/share/tcm/logs/`
- **Priority:** High

#### FR-LOG-002: asciicast v2 format
- **Description:** Log file theo format asciicast v2, có thể replay bằng `asciinema play`
- **Priority:** High

#### FR-LOG-003: File naming
- **Description:** File được đặt tên: `{session_name}_{YYYY-MM-DD_HHMMSS}.cast`
- **Priority:** Medium

#### FR-LOG-004: Log rotation
- **Description:** Khi file đạt 50MB, tự động tạo file mới với suffix `-part2`, `-part3`, v.v.
- **Priority:** Medium

### 3.5 Macro / Automation

#### FR-MAC-001: Recipe format
- **Description:** Recipe được định nghĩa trong file JSON với array of steps, mỗi step có: expect (regex), send (string), timeout_ms, on_timeout
- **Priority:** High

#### FR-MAC-002: Run recipe
- **Description:** Người dùng có thể run recipe trên session đang active. Recipe runs không interrupt user input
- **Priority:** High

#### FR-MAC-003: Variable substitution
- **Description:** Strings trong `send` field có thể dùng `{variable}` syntax. Variables được cung cấp lúc run recipe
- **Priority:** High

#### FR-MAC-004: Timeout handling
- **Description:** Nếu expect pattern không match trong `timeout_ms`, action `on_timeout` được thực thi: `abort` (dừng recipe) hoặc `skip` (bỏ qua step)
- **Priority:** High

### 3.6 Credential Management

#### FR-CRED-001: Store credentials
- **Description:** Password có thể được lưu encrypted, không cần nhập mỗi lần connect
- **Priority:** Medium

#### FR-CRED-002: Keyring backend
- **Description:** Dùng libsecret (GNOME keyring) nếu available. Fallback: AES-256 encrypted file với passphrase
- **Priority:** Medium

---

## 4. Non-Functional Requirements

### 4.1 Performance

| ID | Requirement | Target |
|---|---|---|
| NFR-PERF-001 | Startup time | < 200ms từ launch đến SessionList |
| NFR-PERF-002 | Switch latency | < 50ms từ keystroke đến new session active |
| NFR-PERF-003 | Input latency | < 16ms từ keystroke đến PTY |
| NFR-PERF-004 | Concurrent sessions | Hỗ trợ ≥ 20 sessions đồng thời |
| NFR-PERF-005 | Buffer throughput | ≥ 1MB/s per session không drop data |

### 4.2 Memory

| ID | Requirement | Target |
|---|---|---|
| NFR-MEM-001 | Base memory | < 10MB RSS khi idle (không có session) |
| NFR-MEM-002 | Per-session memory | < 2MB overhead mỗi session (excl. buffer) |
| NFR-MEM-003 | Buffer size | 64KB ring buffer per background session |
| NFR-MEM-004 | Memory leak | Zero leaks sau 24 giờ liên tục (Valgrind) |

### 4.3 Reliability

| ID | Requirement |
|---|---|
| NFR-REL-001 | SSH disconnect → detect trong 2s, mark session error, offer reconnect |
| NFR-REL-002 | Serial unplug → detect ENXIO, graceful error |
| NFR-REL-003 | Log disk full → disable logging, warn user, không crash |
| NFR-REL-004 | Signal SIGTERM → graceful shutdown: disconnect all, flush logs |
| NFR-REL-005 | Session crash (child process) → no zombie process |

### 4.4 Usability

| ID | Requirement |
|---|---|
| NFR-USE-001 | Keybindings phải nhất quán với vim (j/k navigation) |
| NFR-USE-002 | Mỗi màn hình phải hiện keybind hints ở status bar |
| NFR-USE-003 | Error messages phải human-readable, không raw errno |
| NFR-USE-004 | Terminal phải được restore hoàn toàn sau khi quit (`reset` không cần thiết) |

### 4.5 Portability

| ID | Requirement |
|---|---|
| NFR-PORT-001 | Build và chạy trên Ubuntu 20.04, 22.04, 24.04 |
| NFR-PORT-002 | Build và chạy trên Debian 11, 12 |
| NFR-PORT-003 | Build và chạy trên Alpine Linux (musl libc) |
| NFR-PORT-004 | Static binary option: single file, không cần install dependencies |

### 4.6 Security

| ID | Requirement |
|---|---|
| NFR-SEC-001 | Passwords không được log vào asciicast file |
| NFR-SEC-002 | Private key paths được lưu, private key content không bao giờ được copy |
| NFR-SEC-003 | SSH host key verification mặc định là bắt buộc |
| NFR-SEC-004 | Credentials trong storage phải encrypted (không plaintext) |

---

## 5. External Interface Requirements

### 5.1 User Interface

TCM là fullscreen TUI. Không có mouse support. Navigation bằng keyboard thuần túy.

**Global keybindings:**

| Key | Action |
|---|---|
| `j` / `↓` | Move selection down |
| `k` / `↑` | Move selection up |
| `Enter` | Connect / Select |
| `q` | Quit / Back |
| `Ctrl+]` | Show switcher overlay (khi trong session) |
| `a` | Add session |
| `d` | Delete session |
| `/` | Search sessions |
| `r` | Reconnect session (khi disconnected) |
| `m` | Run macro recipe |

### 5.2 File interfaces

#### Session config: `~/.config/tcm/sessions.json`

```json
{
  "version": 1,
  "sessions": [
    {
      "id": "uuid-v4",
      "name": "router-lab-01",
      "type": "ssh",
      "host": "192.168.1.1",
      "port": 22,
      "username": "admin",
      "auth": "password",
      "tags": ["lab", "router"],
      "notes": "Broadcom BCM6752"
    },
    {
      "id": "uuid-v4",
      "name": "ont-uart",
      "type": "serial",
      "device": "/dev/ttyUSB0",
      "baud_rate": 115200,
      "data_bits": 8,
      "parity": "none",
      "stop_bits": 1,
      "flow_control": "none"
    }
  ]
}
```

#### Recipe format: `~/.config/tcm/recipes/*.json`

```json
{
  "name": "string",
  "description": "string",
  "variables": { "key": "default_value" },
  "steps": [
    {
      "expect": "regex_string",
      "send": "string with {variables}",
      "timeout_ms": 5000,
      "on_timeout": "abort | skip"
    }
  ]
}
```

### 5.3 Dependencies (build-time)

| Library | Version | Purpose |
|---|---|---|
| libssh2 | ≥ 1.10 | SSH protocol |
| ncurses | ≥ 6.2 | TUI rendering |
| nlohmann/json | ≥ 3.11 | JSON parsing |
| Google Test | ≥ 1.14 | Unit testing |
| Google Mock | ≥ 1.14 | Mocking |
| OpenSSL (libcrypto) | ≥ 3.0 | Crypto for credential store |
| libsecret (optional) | ≥ 0.20 | GNOME keyring backend |

---

## 6. Constraints

- **Language:** C++17 (không dùng C++20 để đảm bảo compile trên Ubuntu 20.04)
- **Build system:** CMake ≥ 3.20
- **No GUI frameworks:** Không dùng Qt, GTK, hoặc bất kỳ GUI library nào
- **Single binary:** Output là một executable duy nhất
- **No systemd dependency:** TCM không cần systemd để chạy

---

## 7. Appendix — Use Cases

### UC-01: Kỹ sư debug ONT firmware qua UART

**Actor:** Firmware engineer  
**Precondition:** ONT connected qua USB-to-UART adapter  
**Flow:**
1. Mở TCM → thấy session list
2. Chọn session `ont-uart` (type Serial, /dev/ttyUSB0, 115200)
3. Enter → connect, thấy boot log hoặc shell prompt
4. Debug device qua UART console
5. Đồng thời `Ctrl+]` → switch sang SSH session đến lab server để check logs
6. Switch lại ONT session

**Success:** Không mất output từ cả hai sessions trong quá trình switch

### UC-02: Tự động login và lấy thông tin firmware

**Actor:** QA engineer  
**Flow:**
1. Viết recipe `get-fw-version.json` với steps: login, gõ `show version`, capture output
2. Connect SSH đến ONT
3. Nhấn `m` → chọn recipe → run
4. Xem output trên màn hình, kết quả cũng được log vào `.cast` file

### UC-03: Monitor nhiều ONT đồng thời

**Actor:** NOC engineer  
**Flow:**
1. Add 8 SSH sessions (8 ONTs khác nhau)
2. Connect tất cả (8 background sessions)
3. Switch liên tục qua `Ctrl+]` + số để check từng device
4. Phát hiện device có log bất thường qua buffer khi switch về
