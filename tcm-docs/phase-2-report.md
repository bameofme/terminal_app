# Phase 2 Report — Protocol Adapters

**Project:** TCM (Terminal Connection Manager)
**Phase:** 2 — Protocol Adapters
**Date:** 2026-05-25
**Status:** COMPLETE

---

## Build & Test Results

| Metric | Result |
|--------|--------|
| Build | **SUCCESS** |
| Total Tests | **119 / 119 PASSED** |
| Phase 1 (regression) | 63 / 63 PASSED |
| Phase 2 (new) | 56 / 56 PASSED |

---

## Modules Completed

### Task 2.1 — SshConnection

**Dependency:** libssh2 1.11.0

**Implementation highlights:**
- TCP non-blocking `connect()` với `select()` timeout
- libssh2 handshake không blocking
- Authentication: password auth + key file auth
- Channel: PTY request `"xterm-256color"` + shell launch
- Non-blocking `recv()` với `LIBSSH2_ERROR_EAGAIN` handling
- RAII cleanup idempotent — safe to call `disconnect()` nhiều lần

**Tests:** 10

---

### Task 2.2 — SerialConnection

**Implementation highlights:**
- POSIX `termios` API (`tcgetattr` / `tcsetattr`)
- `cfmakeraw()` để vào raw mode
- Baud rate map đầy đủ: 300 → 921600
- Config: Parity, StopBits, FlowControl
- `ENXIO` detection khi device bị unplug trong lúc đang kết nối
- Restore `termios` state nguyên bản khi `disconnect()`

**Tests:** 12

---

### Task 2.3 — TelnetConnection + IacParser

**IacParser state machine:**

```
Normal → GotIac → GotCommand → (done)
                              → InSubneg → ... → SE (done)
```

**Implementation highlights:**
- Xử lý đầy đủ `WILL` / `WONT` / `DO` / `DONT` negotiation
- Strip `SB ... SE` subnegotiation khỏi data stream
- Escape `0xFF` → `IAC IAC` khi gửi dữ liệu
- `m_prevWasIac` flag để xử lý chính xác `IAC SE` trong subneg
- `m_pending` buffer trong `recv()` để không mất data khi clean output lớn hơn buffer caller

**Tests:** 15 (8 IacParser + 7 TelnetConnection)

---

### Task 2.4 — ConnectionFactory + SessionConfig

**Implementation highlights:**
- `ConnectionFactory` dispatch theo `SessionType` (SSH / Serial / Telnet)
- `SessionConfig` với `toJson()` / `fromJson()` dùng nlohmann/json
- Round-trip serialization cho cả 3 protocol types
- Validation tại `fromJson()` — throw nếu thiếu field bắt buộc

**Tests:** 8 (4 Factory + 4 SessionConfig)

---

## Definition of Done

- [x] Tất cả tests pass (119 / 119)
- [x] SSH connect / disconnect sạch (RAII, idempotent)
- [x] Serial `termios` restore đầy đủ khi disconnect
- [x] Telnet IAC negotiation không gây vòng lặp vô hạn
- [x] Factory tạo đúng adapter từ mọi config type
- [x] `SessionConfig` JSON serialization round-trip đầy đủ
- [x] Không regress Phase 1 tests (63 / 63 vẫn pass)

---

## Issues & Giải Pháp

| Issue | Giải pháp |
|-------|-----------|
| SSH tests không có real server | Focus vào error paths và state management; mock `libssh2` error codes |
| `SerialConnection` với non-tty file descriptor | Trả về `TCM_ERR_CONNECT` thay vì crash; kiểm tra `isatty()` sau khi open |
| `IacParser` xử lý `IAC SE` trong subneg | Thêm flag `m_prevWasIac` để phân biệt `SE` đứng sau `IAC` với byte `SE` bình thường trong payload |
| `TelnetConnection::recv()` mất data khi clean output > `buf.size()` | Thêm `m_pending` buffer — clean data dư được giữ lại và trả về ở lần `recv()` tiếp theo |

---

## Test Summary by Module

| Module | Tests | Status |
|--------|------:|--------|
| Phase 1 — Core (EpollLoop, EventBus, RingBuffer, Session, SessionManager, PtyProxy, IConnection) | 63 | PASSED |
| SshConnection | 10 | PASSED |
| SerialConnection | 12 | PASSED |
| IacParser | 8 | PASSED |
| TelnetConnection | 7 | PASSED |
| ConnectionFactory | 4 | PASSED |
| SessionConfig | 15 | PASSED |
| **Total** | **119** | **PASSED** |

---

## Next Phase

Xem kế hoạch tại [phase-3-4-5-plans.md](phase-3-4-5-plans.md).
