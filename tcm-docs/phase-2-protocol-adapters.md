# Phase 2 — Protocol Adapters

**Project:** TCM — Terminal Connection Manager  
**Depends on:** Phase 1 complete (IConnection, SessionManager, PtyProxy)  
**Duration:** ~3 tuần

---

## Mục tiêu phase

Implement ba protocol adapters thật sự: SSH (libssh2), Serial (termios/POSIX), và Telnet (raw BSD socket). Mỗi adapter implement `IConnection` interface từ Phase 1. Sau phase này, TCM có thể connect thật đến ONT device qua cả ba giao thức.

---

## Cấu trúc thư mục

```
tcm/
├── include/
│   └── protocol/
│       ├── SshConnection.hpp
│       ├── SshConfig.hpp
│       ├── SerialConnection.hpp
│       ├── SerialConfig.hpp
│       ├── TelnetConnection.hpp
│       └── TelnetConfig.hpp
├── src/
│   └── protocol/
│       ├── SshConnection.cpp
│       ├── SerialConnection.cpp
│       └── TelnetConnection.cpp
└── tests/
    └── protocol/
        ├── CMakeLists.txt
        ├── SshConnectionTest.cpp
        ├── SerialConnectionTest.cpp
        └── TelnetConnectionTest.cpp
```

---

## Module 2.1 — SshConnection (libssh2)

### Mô tả
Wrap libssh2 để implement `IConnection`. Hỗ trợ auth bằng password và private key. Request PTY sau khi open channel để app phía kia chạy ở interactive mode. Hỗ trợ port forwarding để dùng SSH tunnel.

### Config struct

```cpp
struct SshConfig {
    std::string host;
    uint16_t    port         = 22;
    std::string username;
    std::string password;           // empty nếu dùng key
    std::string privateKeyPath;     // empty nếu dùng password
    std::string knownHostsPath;     // default: ~/.ssh/known_hosts
    int         connectTimeoutMs   = 10000;
    int         keepAliveIntervalS = 30;
    struct Tunnel {
        std::string localHost  = "127.0.0.1";
        uint16_t    localPort;
        std::string remoteHost;
        uint16_t    remotePort;
    };
    std::optional<Tunnel> tunnel;
};
```

### Tasks

#### Task 2.1.1 — Viết test cho SshConnection
- [ ] Test `connect()` với mock libssh2 session → return 0
- [ ] Test `connect()` với wrong password → return `TCM_ERR_AUTH`
- [ ] Test `connect()` timeout → return `TCM_ERR_TIMEOUT`
- [ ] Test `send()` gửi đúng bytes qua channel
- [ ] Test `recv()` đọc data từ channel
- [ ] Test `disconnect()` free session và socket
- [ ] Test auth bằng private key path hợp lệ
- [ ] Test `getState()` transitions: Disconnected → Connecting → Connected

> **Ghi chú testing:** Dùng mock server hoặc `libssh2` fake layer. Có thể dùng Docker container chạy OpenSSH để test integration.

**File:** `tests/protocol/SshConnectionTest.cpp`

#### Task 2.1.2 — Implement SshConnection
- [ ] Constructor nhận `SshConfig`
- [ ] `connect()`:
  - [ ] `socket()` + `connect()` TCP đến host:port
  - [ ] `libssh2_session_init()`
  - [ ] `libssh2_session_handshake()`
  - [ ] Verify known_hosts (hoặc skip nếu config cho phép)
  - [ ] Auth: `libssh2_userauth_password()` hoặc `libssh2_userauth_publickey_fromfile()`
  - [ ] `libssh2_channel_open_session()`
  - [ ] `libssh2_channel_request_pty("xterm-256color")`
  - [ ] `libssh2_channel_shell()`
- [ ] `send(data)` → `libssh2_channel_write()`
- [ ] `recv(buf)` → `libssh2_channel_read()` (non-blocking với `libssh2_session_set_blocking(0)`)
- [ ] `disconnect()` → `libssh2_channel_close()` + `libssh2_session_disconnect()` + `libssh2_session_free()` + `close(sock)`
- [ ] `getFd()` → trả về socket fd để epoll theo dõi

#### Task 2.1.3 — Implement SSH tunnel
- [ ] `libssh2_channel_direct_tcpip()` để mở tunnel
- [ ] Expose local port qua `std::thread` + `accept()` loop
- [ ] Forward data qua SSH channel

#### Task 2.1.4 — Implement keep-alive
- [ ] `std::thread` gửi `libssh2_keepalive_send()` mỗi 30 giây
- [ ] Detect disconnect khi keepalive fail → publish `SessionRemovedEvent`

#### Task 2.1.5 — Refactor
- [ ] RAII wrapper `SshSession` và `SshChannel` để tránh leak
- [ ] Known-hosts verification đúng chuẩn
- [ ] Non-blocking mode để epoll work correctly

### Định nghĩa hoàn thành
- [ ] Tests pass: `ctest -R SshConnection`
- [ ] Connect thật đến OpenSSH server (Docker): `ssh admin@localhost -p 2222`
- [ ] Gõ lệnh, nhận output đúng
- [ ] Disconnect sạch (không zombie process, không leak fd)
- [ ] Auth bằng cả password lẫn key file đều work
- [ ] Valgrind clean

---

## Module 2.2 — SerialConnection (termios)

### Mô tả
Open `/dev/ttyUSBx` hoặc `/dev/ttySx`, cấu hình baud rate / parity / flow control qua POSIX termios API, rồi implement `IConnection` với raw byte I/O. Use case chính: connect UART console của ONT device.

### Config struct

```cpp
struct SerialConfig {
    std::string device;          // "/dev/ttyUSB0"
    uint32_t    baudRate  = 115200;
    uint8_t     dataBits  = 8;   // 7 hoặc 8
    enum class Parity { None, Odd, Even } parity = Parity::None;
    enum class StopBits { One, Two } stopBits = StopBits::One;
    enum class FlowControl { None, Hardware, Software } flowControl = FlowControl::None;
    int         readTimeoutMs = 100;
};
```

### Baud rate supported
`300, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600`

### Tasks

#### Task 2.2.1 — Viết test cho SerialConnection
- [ ] Test `connect()` với valid device path → return 0
- [ ] Test `connect()` với device không tồn tại → return error
- [ ] Test baud rate được set đúng (`cfsetispeed`/`cfsetospeed`)
- [ ] Test `send()` + `recv()` round-trip qua `socat` loopback
- [ ] Test `disconnect()` restore terminal settings
- [ ] Test parity None/Odd/Even config đúng
- [ ] Test hardware flow control (RTS/CTS) config đúng

> **Testing trick:** Dùng `socat PTY,link=/tmp/ttyV0 PTY,link=/tmp/ttyV1` để tạo virtual serial pair.

**File:** `tests/protocol/SerialConnectionTest.cpp`

#### Task 2.2.2 — Implement SerialConnection
- [ ] `connect()`:
  - [ ] `open(device, O_RDWR | O_NOCTTY | O_NONBLOCK)`
  - [ ] `tcgetattr()` để lưu original settings (restore khi disconnect)
  - [ ] `cfmakeraw()` để disable all processing
  - [ ] Set baud: `cfsetispeed()` + `cfsetospeed()` với `B115200` constants
  - [ ] Set data bits: `c_cflag &= ~CSIZE; c_cflag |= CS8`
  - [ ] Set parity: `c_cflag &= ~PARENB` (None) hoặc set PARENB/PARODD
  - [ ] Set stop bits: `c_cflag &= ~CSTOPB` (1 bit)
  - [ ] Set flow control: `c_cflag &= ~CRTSCTS` (None) hoặc set CRTSCTS
  - [ ] `tcsetattr(fd, TCSANOW, &tty)`
- [ ] `send(data)` → `write(fd, data, len)` với retry trên EINTR
- [ ] `recv(buf)` → `read(fd, buf, len)` non-blocking
- [ ] `disconnect()` → `tcsetattr()` restore + `close(fd)`
- [ ] `getFd()` → trả về fd để epoll theo dõi

#### Task 2.2.3 — Helper: baud rate lookup
- [ ] Map `uint32_t → speed_t`: `{{9600, B9600}, {115200, B115200}, ...}`
- [ ] Return error nếu baud rate không supported

#### Task 2.2.4 — Refactor
- [ ] RAII `SerialPort` class restore settings trong destructor
- [ ] Detect device hotunplug (read trả về -1 với ENXIO)
- [ ] Publish `SessionRemovedEvent` khi device disconnect

### Định nghĩa hoàn thành
- [ ] Tests pass: `ctest -R SerialConnection`
- [ ] Connect thật đến `/dev/ttyUSB0` ở 115200 baud: nhận được shell prompt
- [ ] `socat` loopback test: send "hello", recv "hello" chính xác
- [ ] Disconnect restore terminal (không bị stuck ở raw mode)
- [ ] Valgrind clean

---

## Module 2.3 — TelnetConnection (raw socket)

### Mô tả
Implement Telnet protocol (RFC 854) qua raw BSD socket. Telnet có option negotiation dùng IAC byte sequences cần được parse và strip khỏi data stream trước khi forward lên layer trên.

### Config struct

```cpp
struct TelnetConfig {
    std::string host;
    uint16_t    port           = 23;
    int         connectTimeoutMs = 5000;
    bool        suppressGoAhead = true;   // negotiate SGA
};
```

### IAC option constants cần implement

```cpp
// Telnet commands
constexpr uint8_t IAC  = 0xFF;
constexpr uint8_t WILL = 0xFB;
constexpr uint8_t WONT = 0xFC;
constexpr uint8_t DO   = 0xFD;
constexpr uint8_t DONT = 0xFE;
constexpr uint8_t SB   = 0xFA;
constexpr uint8_t SE   = 0xF0;

// Options
constexpr uint8_t OPT_ECHO           = 0x01;
constexpr uint8_t OPT_SUPPRESS_GA    = 0x03;
constexpr uint8_t OPT_TERMINAL_TYPE  = 0x18;
constexpr uint8_t OPT_WINDOW_SIZE    = 0x1F;
```

### Tasks

#### Task 2.3.1 — Viết test cho IAC parser
- [ ] Test `parseIac()` strip `IAC DO ECHO` khỏi data stream
- [ ] Test data thường không bị modify
- [ ] Test `IAC IAC` (escaped 0xFF) → output single 0xFF
- [ ] Test incomplete IAC sequence → buffer và đợi data tiếp theo
- [ ] Test `IAC SB ... IAC SE` (subnegotiation) bị strip hoàn toàn

**File:** `tests/protocol/TelnetConnectionTest.cpp`

#### Task 2.3.2 — Viết test cho TelnetConnection
- [ ] Test `connect()` TCP đến host:port → return 0
- [ ] Test `connect()` host unreachable → return error
- [ ] Test `send()` gửi đúng bytes (không bị IAC encode sai)
- [ ] Test `recv()` trả về data đã strip IAC sequences
- [ ] Test auto-negotiate WILL SGA khi nhận DO SGA

#### Task 2.3.3 — Implement IacParser
- [ ] `IacParser` class với internal state machine
- [ ] States: `Normal`, `GotIac`, `GotCommand`, `InSubneg`
- [ ] `feed(rawData)` → return `{cleanData, pendingResponses}`
- [ ] `pendingResponses`: IAC responses cần gửi lại cho server

#### Task 2.3.4 — Implement TelnetConnection
- [ ] `connect()`:
  - [ ] `socket(AF_INET, SOCK_STREAM, 0)`
  - [ ] `connect()` với timeout dùng `select()`
  - [ ] Set `O_NONBLOCK` sau khi connect
- [ ] `send(data)` → escape 0xFF thành `IAC IAC` trước khi write
- [ ] `recv(buf)` → read → feed vào `IacParser` → return clean data
  - [ ] Nếu `IacParser` trả về responses → gửi ngay
- [ ] `disconnect()` → `close(sock)`
- [ ] `getFd()` → trả về socket fd

#### Task 2.3.5 — Auto-negotiate options
- [ ] Nhận `DO SUPPRESS_GA` → gửi `WILL SUPPRESS_GA`
- [ ] Nhận `DO TERMINAL_TYPE` → gửi `WILL TERMINAL_TYPE`, handle SB
- [ ] Nhận `DO WINDOW_SIZE` → gửi `WILL WINDOW_SIZE` + NAWS data
- [ ] Nhận `WILL ECHO` → gửi `DO ECHO`

#### Task 2.3.6 — Refactor
- [ ] Connect timeout qua `setsockopt(SO_RCVTIMEO)`
- [ ] Reconnect logic với exponential backoff
- [ ] IPv6 support (`AF_INET6`)

### Định nghĩa hoàn thành
- [ ] Tests pass: `ctest -R TelnetConnection`
- [ ] IAC parser: 100% test coverage, tất cả edge cases pass
- [ ] Connect thật đến Telnet server (dùng `inetd` hoặc `telnetd` trong Docker)
- [ ] Option negotiation work (không bị vòng lặp IAC)
- [ ] Valgrind clean

---

## Integration test cho Phase 2

### Task 2.4 — Protocol factory

```cpp
// ConnectionFactory tạo đúng adapter từ config
class ConnectionFactory {
public:
    static std::unique_ptr<IConnection> create(const SessionConfig& cfg);
};
```

- [ ] Test factory tạo `SshConnection` từ SSH config
- [ ] Test factory tạo `SerialConnection` từ Serial config
- [ ] Test factory tạo `TelnetConnection` từ Telnet config
- [ ] Test factory throw nếu config type không hợp lệ

### Task 2.5 — Smoke test với SessionManager

- [ ] Add SSH session vào SessionManager → connect → send command → recv output
- [ ] Add Serial session vào SessionManager → connect → send → recv
- [ ] Add Telnet session → connect → recv prompt → send credentials

---

## Definition of Done — Phase 2

### Functional requirements
- [ ] **Tất cả tests pass**: `ctest -R "Ssh|Serial|Telnet"` → 0 failures
- [ ] **SSH**: Connect thật, password auth + key auth, tunnel
- [ ] **Serial**: Connect `/dev/ttyUSB0` 115200-8N1, send/recv raw bytes
- [ ] **Telnet**: Connect, IAC negotiation không bị loop, clean data stream
- [ ] **Factory**: Tạo đúng adapter từ mọi config type

### Non-functional requirements
- [ ] **Valgrind**: 0 leaks cho mỗi adapter sau 5 phút connected
- [ ] **Reconnect**: Detect disconnect và publish event trong vòng 2 giây
- [ ] **Thread-safety**: Có thể gọi `send()` và `recv()` từ different threads

### Integration với Phase 1
- [ ] Mỗi adapter chạy đúng trong `PtyProxy` + `EpollLoop`
- [ ] Switch giữa SSH session và Serial session hoạt động
- [ ] Background session vẫn buffer data khi không active

### Real device test (nếu có hardware)
- [ ] SSH vào ONT Broadcom BCM6xxx: nhận shell prompt, chạy `show version`
- [ ] Serial vào ONT qua `/dev/ttyUSB0`: nhận login prompt, login thành công
- [ ] Telnet vào switch management port: option negotiation sạch
