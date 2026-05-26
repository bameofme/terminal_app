# Phase 1 — Core Foundation

**Project:** TCM — Terminal Connection Manager  
**Language:** C++17 | **Build:** CMake 3.20+ | **Test:** Google Test + Google Mock  
**Methodology:** Test-Driven Development (Red → Green → Refactor)  
**Duration:** ~3 tuần

---

## Mục tiêu phase

Xây dựng skeleton toàn bộ hệ thống: abstract interface cho connections, session lifecycle management, event bus nội bộ, và PTY proxy loop với epoll. Sau phase này, app có thể chạy được dù chưa có protocol thật — mọi thứ dùng mock.

---

## Cấu trúc thư mục cần tạo

```
tcm/
├── CMakeLists.txt
├── include/
│   └── core/
│       ├── IConnection.hpp
│       ├── Session.hpp
│       ├── SessionManager.hpp
│       ├── EventBus.hpp
│       ├── PtyProxy.hpp
│       ├── EpollLoop.hpp
│       └── RingBuffer.hpp
├── src/
│   └── core/
│       ├── Session.cpp
│       ├── SessionManager.cpp
│       ├── EventBus.cpp
│       ├── PtyProxy.cpp
│       └── EpollLoop.cpp
└── tests/
    └── core/
        ├── CMakeLists.txt
        ├── IConnectionTest.cpp
        ├── SessionManagerTest.cpp
        ├── EventBusTest.cpp
        ├── PtyProxyTest.cpp
        └── RingBufferTest.cpp
```

---

## Module 1.1 — IConnection interface

### Mô tả
Abstract base class định nghĩa contract cho tất cả protocol adapters (SSH, Serial, Telnet). Đây là interface duy nhất mà `SessionManager` và `MacroEngine` biết đến.

### Tasks

#### Task 1.1.1 — Viết test cho IConnection contract
- [ ] Test `connect()` trả về `0` khi thành công, negative khi lỗi
- [ ] Test `send()` forward đúng bytes, trả về số bytes đã gửi
- [ ] Test `recv()` trả về data nhận được, trả về `0` khi disconnect
- [ ] Test `disconnect()` có thể gọi nhiều lần mà không crash
- [ ] Test `getState()` trả về đúng `ConnectionState` enum
- [ ] Test `getFd()` trả về valid file descriptor sau `connect()`

**File:** `tests/core/IConnectionTest.cpp`

```cpp
// Ví dụ test đầu tiên cần viết
TEST(MockConnectionTest, ConnectReturnsZeroOnSuccess) {
    MockConnection conn;
    EXPECT_CALL(conn, connect()).WillOnce(Return(0));
    EXPECT_EQ(conn.connect(), 0);
}

TEST(MockConnectionTest, SendForwardsCorrectBytes) {
    MockConnection conn;
    std::vector<uint8_t> data = {'h', 'e', 'l', 'l', 'o'};
    EXPECT_CALL(conn, send(testing::_)).WillOnce(Return(5));
    EXPECT_EQ(conn.send(std::span<const uint8_t>(data)), 5);
}
```

#### Task 1.1.2 — Implement IConnection
- [ ] Tạo `IConnection.hpp` với pure virtual methods
- [ ] Định nghĩa `ConnectionState` enum: `Disconnected`, `Connecting`, `Connected`, `Error`
- [ ] Định nghĩa error codes: `TCM_OK=0`, `TCM_ERR_CONNECT=-1`, `TCM_ERR_TIMEOUT=-2`, `TCM_ERR_AUTH=-3`
- [ ] Tạo `MockConnection` trong `tests/core/` dùng `MOCK_METHOD`
- [ ] Virtual destructor với `= default`

#### Task 1.1.3 — Refactor
- [ ] Dùng `std::span<const uint8_t>` cho buffer params (zero-copy)
- [ ] Mark methods `noexcept` đúng chỗ
- [ ] Thêm `[[nodiscard]]` cho `connect()`, `send()`, `recv()`

### Định nghĩa hoàn thành (Definition of Done)
- [ ] Tất cả tests pass: `ctest --test-dir build -R IConnection`
- [ ] `MockConnection` compile và mock được mọi method
- [ ] Không có warning với `-Wall -Wextra`
- [ ] Code review: interface đủ clean để implement SSH/Serial/Telnet

---

## Module 1.2 — Session & SessionManager

### Mô tả
`Session` là data struct gom thông tin một connection (id, name, type, connection object, state). `SessionManager` quản lý vòng đời của nhiều sessions: thêm, xóa, switch foreground.

### Tasks

#### Task 1.2.1 — Viết test cho Session struct
- [ ] Test `Session` khởi tạo đúng với id, name, type
- [ ] Test `Session::id` là unique UUID string
- [ ] Test default state là `SessionState::Idle`

**File:** `tests/core/SessionManagerTest.cpp`

#### Task 1.2.2 — Viết test cho SessionManager
- [ ] Test `add()` tăng `count()` lên 1
- [ ] Test `remove(id)` giảm `count()` và cleanup connection
- [ ] Test `setActive(id)` thay đổi `getActiveId()`
- [ ] Test `getActive()` trả về `nullptr` khi không có session
- [ ] Test `setActive()` với id không tồn tại trả về `false`
- [ ] Test `forEach()` iterate đúng số sessions
- [ ] Test remove session đang active → active chuyển về `nullopt`

#### Task 1.2.3 — Implement Session + SessionManager
- [ ] `Session` struct: `id` (string), `name` (string), `type` (enum: SSH/Serial/Telnet), `conn` (unique_ptr<IConnection>), `state` (enum: Idle/Connecting/Connected/Error)
- [ ] `SessionManager` class với `std::vector<Session>` và `std::optional<std::string> m_activeId`
- [ ] `add(Session)` → push và return id
- [ ] `remove(id)` → erase và disconnect
- [ ] `setActive(id)` → update m_activeId, return bool
- [ ] `getActive()` → return `Session*` hoặc `nullptr`
- [ ] `forEach(callback)` → iterate all sessions

#### Task 1.2.4 — Refactor
- [ ] Generate UUID với `<random>` + hex formatting
- [ ] Notify `EventBus` khi session added/removed/switched
- [ ] Thread-safe với `std::mutex` nếu cần

### Định nghĩa hoàn thành
- [ ] Tất cả tests pass: `ctest --test-dir build -R SessionManager`
- [ ] `remove()` không leak memory (valgrind clean)
- [ ] `setActive()` với invalid id không crash, return false
- [ ] EventBus notifications hoạt động (kiểm tra qua mock observer)

---

## Module 1.3 — EventBus

### Mô tả
Loosely-coupled event system để các modules communicate mà không cần biết nhau. Events chạy synchronously trong cùng thread.

### Events cần định nghĩa

```cpp
struct SessionAddedEvent    { std::string sessionId; };
struct SessionRemovedEvent  { std::string sessionId; };
struct SessionSwitchedEvent { std::string fromId; std::string toId; };
struct DataReceivedEvent    { std::string sessionId; std::vector<uint8_t> data; };
struct KeyPressedEvent      { int key; bool isEscape; };
struct MacroTriggeredEvent  { std::string recipeName; std::string sessionId; };
```

### Tasks

#### Task 1.3.1 — Viết test cho EventBus
- [ ] Test `subscribe<E>()` nhận đúng event type
- [ ] Test `publish<E>()` gọi tất cả handlers đã subscribe
- [ ] Test multiple subscribers cùng event type đều được gọi
- [ ] Test `unsubscribe()` handler không còn được gọi
- [ ] Test publish event không có subscriber → không crash
- [ ] Test handler không gọi sau khi EventBus bị destroy

#### Task 1.3.2 — Implement EventBus
- [ ] Template class `EventBus` với `std::unordered_map<type_index, vector<function>>`
- [ ] `subscribe<EventType>(handler)` → return subscription token
- [ ] `publish<EventType>(event)` → call tất cả handlers
- [ ] `unsubscribe(token)` → remove handler
- [ ] Token là wrapper quanh integer id

#### Task 1.3.3 — Refactor
- [ ] Thread-safe publish với `std::shared_mutex` (read-many write-once)
- [ ] Dùng `std::weak_ptr` pattern để tránh dangling callbacks
- [ ] Không throw exception từ `publish()`

### Định nghĩa hoàn thành
- [ ] Tất cả tests pass: `ctest --test-dir build -R EventBus`
- [ ] Unsubscribe sau publish không crash
- [ ] Memory leak free (valgrind)
- [ ] Template instantiation compile sạch với mọi event type

---

## Module 1.4 — RingBuffer

### Mô tả
Per-session circular buffer để lưu output của background sessions. Khi buffer đầy, data cũ bị overwrite (newest data wins).

### Tasks

#### Task 1.4.1 — Viết test cho RingBuffer
- [ ] Test `write()` + `read()` round-trip đúng data
- [ ] Test `write()` khi buffer đầy → overwrite data cũ
- [ ] Test `available()` trả về đúng số bytes có thể đọc
- [ ] Test `clear()` reset buffer về trạng thái rỗng
- [ ] Test concurrent write/read (nếu thread-safe)
- [ ] Test boundary: write đúng capacity bytes

#### Task 1.4.2 — Implement RingBuffer
- [ ] `RingBuffer` class với fixed-size array (default 65536 bytes = 64KB)
- [ ] `write(data, len)` → overwrite khi đầy
- [ ] `read(buf, len)` → return bytes thực sự đọc
- [ ] `available()` → bytes có thể đọc
- [ ] `capacity()` → total size
- [ ] `clear()` → reset head/tail

#### Task 1.4.3 — Refactor
- [ ] Power-of-2 size để dùng bitwise mask thay vì modulo
- [ ] `std::atomic` cho head/tail nếu single-producer/single-consumer

### Định nghĩa hoàn thành
- [ ] Tất cả tests pass: `ctest --test-dir build -R RingBuffer`
- [ ] Overflow behavior đúng (overwrite oldest)
- [ ] No UB với address sanitizer (`-fsanitize=address`)

---

## Module 1.5 — PtyProxy + EpollLoop

### Mô tả
Đây là trái tim của TCM. `EpollLoop` theo dõi N file descriptors song song. `PtyProxy` wrap logic `openpty()` + `fork()` + `execvp()`, và kết nối output vào `RingBuffer` của từng session.

### Tasks

#### Task 1.5.1 — Viết test cho EpollLoop
- [ ] Test `addFd()` → fd được theo dõi
- [ ] Test `removeFd()` → fd không còn được theo dõi
- [ ] Test `wait()` trả về đúng fds có data
- [ ] Test `stop()` terminate loop sạch sẽ
- [ ] Test với pipe() thay vì PTY để dễ test

#### Task 1.5.2 — Viết test cho PtyProxy
- [ ] Test `spawn()` tạo PTY master/slave pair
- [ ] Test `write()` gửi data vào PTY slave
- [ ] Test `setActive()` forward output slave → stdout
- [ ] Test `setBackground()` forward output slave → RingBuffer
- [ ] Dùng `/bin/cat` làm child process trong test

#### Task 1.5.3 — Implement EpollLoop
- [ ] `EpollLoop` class với `epoll_create1()`
- [ ] `addFd(fd, callback)` → `epoll_ctl(EPOLL_CTL_ADD)`
- [ ] `removeFd(fd)` → `epoll_ctl(EPOLL_CTL_DEL)`
- [ ] `run()` → event loop với `epoll_wait()`
- [ ] `stop()` → write vào eventfd để wake up loop
- [ ] Chạy trong dedicated thread

#### Task 1.5.4 — Implement PtyProxy
- [ ] `spawn(argv[])` → `fork()` + `openpty()` + `execvp()`
- [ ] `masterFd()` → file descriptor của PTY master
- [ ] `setActive(bool)` → switch routing: stdout vs RingBuffer
- [ ] `write(data, len)` → ghi vào PTY master
- [ ] `kill()` → SIGTERM child process
- [ ] SIGCHLD handler để detect child exit

#### Task 1.5.5 — Intercept escape sequence
- [ ] Detect `Ctrl+]` (0x1D) trong input stream
- [ ] Publish `KeyPressedEvent{isEscape: true}` lên EventBus
- [ ] Không forward 0x1D vào PTY
- [ ] Mọi key khác forward bình thường

#### Task 1.5.6 — Refactor
- [ ] RAII wrapper cho PTY fd pair
- [ ] Graceful shutdown: drain buffer trước khi kill
- [ ] Resize PTY khi terminal resize (SIGWINCH → `ioctl(TIOCSWINSZ)`)

### Định nghĩa hoàn thành
- [ ] Tất cả tests pass: `ctest --test-dir build -R PtyProxy && ctest --test-dir build -R EpollLoop`
- [ ] Chạy `/bin/bash` qua PtyProxy, gõ lệnh được, output hiển thị đúng
- [ ] Switch active/background → output routing đúng
- [ ] `Ctrl+]` intercept thành công, không leak vào PTY
- [ ] Valgrind clean, no zombie processes

---

## Build system setup

### Tasks

#### Task 1.0.1 — CMakeLists.txt root
- [ ] Set C++17, `-Wall -Wextra -Werror`
- [ ] Fetch Google Test via `FetchContent`
- [ ] Fetch nlohmann/json via `FetchContent`
- [ ] Enable `ctest`
- [ ] Sanitizer build type: `cmake -DCMAKE_BUILD_TYPE=Asan`

#### Task 1.0.2 — CMakeLists.txt cho tests
- [ ] `add_subdirectory(tests/core)`
- [ ] Link `gtest_main` + `gmock`
- [ ] Mỗi test file là một executable riêng

#### Task 1.0.3 — CI script
- [ ] `scripts/build.sh`: configure → build → test
- [ ] Exit non-zero nếu bất kỳ test nào fail
- [ ] Run với `-fsanitize=address,undefined`

### Định nghĩa hoàn thành
- [ ] `cmake -B build && cmake --build build && ctest --test-dir build` chạy sạch
- [ ] Zero warnings với `-Wall -Wextra`
- [ ] Asan/UBSan pass

---

## Definition of Done — Phase 1

Phase 1 được coi là **hoàn thành** khi tất cả điều kiện sau đây đều đúng:

### Functional requirements
- [ ] **Tất cả unit tests pass**: `ctest --test-dir build` → 0 failures
- [ ] **MockConnection** có thể substitute cho bất kỳ real connection nào
- [ ] **SessionManager** có thể add/remove/switch N sessions không giới hạn
- [ ] **EpollLoop** theo dõi ít nhất 20 PTY fds đồng thời mà không drop data
- [ ] **PtyProxy** spawn `/bin/bash`, gõ lệnh, nhận output đúng

### Non-functional requirements
- [ ] **Memory**: Valgrind report 0 leaks khi run 10 phút với 5 sessions
- [ ] **Performance**: `epoll_wait` latency < 5ms cho 10 simultaneous PTYs
- [ ] **Code quality**: Không có warning với `-Wall -Wextra -Wno-unused-parameter`
- [ ] **Sanitizers**: AddressSanitizer + UBSanitizer pass toàn bộ test suite

### TDD compliance
- [ ] Mọi class đều có test file tương ứng
- [ ] Test coverage > 80% cho mọi file trong `src/core/`
- [ ] Không có code nào được merge nếu test đang fail

### Integration smoke test
```bash
# Chạy bằng tay sau khi build
./build/tcm_core_smoke_test
# Expected output:
# [OK] EventBus: 3 events published, 3 received
# [OK] SessionManager: 5 sessions, switch active, remove 2
# [OK] PtyProxy: spawn bash, send 'echo hello', recv 'hello\n'
# [OK] EpollLoop: 5 fds monitored, all data received
```

---

## Checklist nhanh cho developer

Trước khi mark một task là done:
- [ ] Test viết TRƯỚC khi implement (Red phase)
- [ ] Test fail với message có ý nghĩa (không phải compile error)
- [ ] Implement đủ để test pass (Green phase)
- [ ] Refactor code mà không break tests
- [ ] Commit với message: `feat(core): add SessionManager add/remove methods`
