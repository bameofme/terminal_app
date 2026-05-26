# Phase 1 — Core Foundation: Completion Report

**Project:** TCM (Terminal Connection Manager)
**Phase:** 1 — Core Foundation
**Status:** COMPLETED
**Date:** 2026-05-25

---

## Build & Test Results

| Metric | Result |
|---|---|
| Build status | SUCCESS |
| Compiler warnings | 0 |
| Compiler errors | 0 |
| Tests passed | 63 / 63 |
| Test execution time | ~0.66 seconds |

```
$ cmake --build build -j$(nproc) && ctest --test-dir build
...
100% tests passed, 0 tests failed out of 63
Total Test time (real) =   0.66 sec
```

---

## Modules Completed

### Task 1.0 — Build System

- Root `CMakeLists.txt` và toàn bộ subdirectory CMake files
- `FetchContent` dependencies:
  - GoogleTest v1.14.0
  - nlohmann/json v3.11.3
  - libssh2 1.11.0
  - OpenSSL 3.0.13
  - ncurses
- Compiler flags: `-Wall -Wextra -Werror`
- C++ standard: C++20

---

### Task 1.1 — IConnection Interface

**File:** `include/core/IConnection.hpp`

- Pure virtual interface với các method: `connect`, `send`, `recv`, `disconnect`, `getFd`, `getState`
- `ConnectionState` enum: `Disconnected`, `Connecting`, `Connected`, `Error`
- Error codes:

  | Constant | Value | Meaning |
  |---|---|---|
  | `TCM_OK` | 0 | Success |
  | `TCM_ERR_CONNECT` | -1 | Connection failed |
  | `TCM_ERR_TIMEOUT` | -2 | Timeout |
  | `TCM_ERR_AUTH` | -3 | Authentication error |
  | `TCM_ERR_IO` | -4 | I/O error |

- Dùng `std::span<const uint8_t>` cho buffer parameters
- `MockConnection` implement đầy đủ `MOCK_METHOD` cho GMock
- **Tests:** 9 / 9 passed

---

### Task 1.2 — Session & SessionManager

**Files:** `include/core/Session.hpp`, `src/core/Session.cpp`, `include/core/SessionManager.hpp`, `src/core/SessionManager.cpp`

- `Session` struct: `id` (UUID v4), `name`, `type`, `conn` (`unique_ptr<IConnection>`), `state`
- `SessionManager` API: `add`, `remove`, `setActive`, `getActive`, `forEach`, `count`
- UUID v4 generator dùng `std::mt19937_64`
- Tích hợp `EventBus` để publish `SessionAdded`, `SessionRemoved`, `SessionSwitched`
- **Tests:** 13 / 13 passed

---

### Task 1.3 — EventBus

**Files:** `include/core/EventBus.hpp`, `src/core/EventBus.cpp`

- Template implementation với type erasure dùng `std::type_index`
- Thread-safe: `std::shared_mutex` (readers/writer lock)
- Token-based unsubscribe (RAII subscription token)
- Event types defined:

  | Event | Trigger |
  |---|---|
  | `SessionAdded` | Session được thêm vào manager |
  | `SessionRemoved` | Session bị xóa |
  | `SessionSwitched` | Active session thay đổi |
  | `DataReceived` | Dữ liệu đến từ connection |
  | `KeyPressed` | Phím được nhấn trong TUI |
  | `MacroTriggered` | Macro được kích hoạt |
  | `Done` | Tác vụ async hoàn tất |
  | `Failed` | Tác vụ async thất bại |
  | `SessionConnected` | Connection thiết lập thành công |
  | `SessionDisconnected` | Connection bị đóng |

- **Tests:** 10 / 10 passed

---

### Task 1.4 — RingBuffer

**Files:** `include/core/RingBuffer.hpp`, `src/core/RingBuffer.cpp`

- Fixed-size circular buffer, capacity mặc định 64 KB
- Capacity luôn là lũy thừa của 2 — indexing dùng bitwise mask (`& (capacity - 1)`)
- Khi đầy: overwrite dữ liệu cũ nhất (oldest-first eviction)
- API: `write`, `read`, `peek`, `available`, `capacity`, `clear`
- **Tests:** 15 / 15 passed

---

### Task 1.5 — PtyProxy + EpollLoop

**Files:** `include/core/EpollLoop.hpp`, `src/core/EpollLoop.cpp`, `include/core/PtyProxy.hpp`, `src/core/PtyProxy.cpp`

#### EpollLoop

- `epoll_create1` + `eventfd` để wake loop từ thread khác
- Hỗ trợ monitor nhiều file descriptor đồng thời
- Callback-based event dispatch
- **Tests:** 5 / 5 passed

#### PtyProxy

- Spawn process con qua `openpty` + `fork` + `execvp`
- `SIGCHLD` handler để detect process exit
- Intercept `0x1D` (Ctrl+]) để toggle active/background routing
- Hỗ trợ routing dữ liệu: active session vs background buffer
- **Tests:** 6 / 6 passed

---

## Definition of Done — Checklist

- [x] Tất cả unit tests pass (63/63)
- [x] `MockConnection` có thể substitute cho real connections trong mọi test
- [x] `SessionManager` add/remove/switch N sessions chính xác
- [x] `EpollLoop` monitor multiple file descriptors đồng thời
- [x] `PtyProxy` spawn bash, gõ lệnh, nhận output đúng
- [x] Memory management: RAII objects, `unique_ptr`, không có raw owning pointers
- [x] Code quality: `-Wall -Wextra -Werror` với 0 warnings
- [x] TDD compliance: test được viết trước khi implement

---

## Issues & Resolutions

| Issue | Root Cause | Resolution |
|---|---|---|
| `std::span` compile error | `std::span` yêu cầu C++20, spec ban đầu chỉ định C++17 | Nâng C++ standard lên C++20 trong CMakeLists.txt |
| GMock không match `std::span` | GMock matcher không hỗ trợ so sánh `std::span` trực tiếp | Dùng `testing::_` wildcard matcher cho span parameters |
| `EpollLoop` deadlock | Gọi callback trong khi giữ lock dẫn đến potential deadlock | Copy callback list ra ngoài critical section trước khi invoke |
| `PtyProxy::isRunning()` const violation | `waitpid` cập nhật internal state nhưng method được khai báo `const` | Khai báo state member là `mutable` để cho phép cập nhật trong const method |
| EIO false positive | Khi slave PTY đóng, `read` trả về `EIO` — cần phân biệt với lỗi thực | Kiểm tra `errno == EIO` riêng biệt, treat như EOF thay vì error |

---

## Next Phase

Phase 2 — Protocol Adapters: implement `SshConnection`, `SerialConnection`, `LocalConnection` kế thừa `IConnection`. Chi tiết tại [phase-2-protocol-adapters.md](phase-2-protocol-adapters.md).
