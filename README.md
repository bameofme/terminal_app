# terminal_app

This repository contains **TCM (Terminal Connection Manager)** — a C++20 TUI application for managing SSH, Telnet and Serial sessions with macro automation.

## Repository Structure

```
terminal_app/
├── tcm/           # Source code, tests, scripts
└── tcm-docs/      # Architecture docs, phase reports, SRS/SAD
```

---

# TCM — Terminal Connection Manager

A terminal multiplexer for managing multiple network device sessions (SSH, Telnet, Serial) with macro automation support.

## Features

- **Multi-protocol**: SSH, Telnet, Serial (RS-232)
- **TUI interface**: ncurses-based session list with live switcher overlay
- **Macro/Recipe engine**: Automate login sequences with expect/send steps
- **Session logging**: Asciicast v2 format for replay
- **Credential store**: Encrypted password storage
- **Epoll-driven**: Non-blocking I/O for all connections

---

## Requirements

| Dependency | Version |
|------------|---------|
| GCC / Clang | C++20 capable |
| CMake | ≥ 3.20 |
| libssh2 | ≥ 1.9 |
| OpenSSL | ≥ 1.1 |
| ncurses | ≥ 6.0 |
| pkg-config | any |

### Install dependencies (Debian/Ubuntu)

```bash
sudo apt-get install -y \
    build-essential cmake pkg-config \
    libssh2-1-dev libssl-dev libncurses-dev
```

---

## Build

### Debug (default)

```bash
cd tcm
./scripts/build.sh
```

### Release

```bash
cd tcm
./scripts/build.sh --release
```

### With AddressSanitizer

```bash
cd tcm
./scripts/build.sh --asan
```

### Static binary

```bash
cd tcm
./scripts/build.sh --release --static
```

### Manual CMake

```bash
cd tcm
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

---

## Install

After building, run the install script:

```bash
cd tcm
./scripts/install.sh
```

This will:
1. Copy the binary to `/usr/local/bin/tcm`
2. Create `~/.config/tcm/` with a default `sessions.json`
3. Create `~/.local/share/tcm/logs/` for session logs
4. Copy example recipes to `~/.config/tcm/recipes/`

---

## Quick Start (5 minutes)

### 1. Launch TCM

```bash
tcm
```

### 2. Add a session

Press `a` in the session list to open the New Session form. Fill in:
- **Name**: `my-router`
- **Protocol**: `ssh`
- **Host**: `192.168.1.1`
- **Port**: `22`
- **Username**: `admin`

Press `Enter` to save.

### 3. Connect

Highlight the session and press `Enter` to connect.

### 4. Switch sessions

Press `Ctrl-b` to open the switcher overlay and select another session.

### 5. Run a recipe

While connected, press `m` and select a recipe from `~/.config/tcm/recipes/`.

---

## Key Bindings

### Session List View

| Key | Action |
|-----|--------|
| `j` / `↓` | Move down |
| `k` / `↑` | Move up |
| `Enter` | Connect to selected session |
| `a` | New session |
| `e` | Edit selected session |
| `d` | Delete selected session |
| `m` | Run macro/recipe |
| `q` | Quit |

### Connected Session

| Key | Action |
|-----|--------|
| `Ctrl-]` | Open session switcher overlay |
| `Ctrl-]` then `d` | Detach — return to session list (session stays connected) |
| `Ctrl-]` then `1-9` | Switch to connected session by number |
| `Ctrl-]` then `Esc` | Close overlay, stay in current session |

### Switcher Overlay

| Key | Action |
|-----|--------|
| `1`-`9` | Switch to session by number |
| `d` | Detach — go back to session list |
| `Esc` | Close overlay, stay in current session |
---

## Session Configuration

Sessions are stored in `~/.config/tcm/sessions.json`.

### SSH session

```json
{
  "name": "prod-router",
  "protocol": "ssh",
  "host": "10.0.0.1",
  "port": 22,
  "username": "admin",
  "auth_method": "password"
}
```

### Telnet session

```json
{
  "name": "old-switch",
  "protocol": "telnet",
  "host": "192.168.1.254",
  "port": 23
}
```

### Serial session

```json
{
  "name": "console-port",
  "protocol": "serial",
  "device": "/dev/ttyUSB0",
  "baud_rate": 9600,
  "data_bits": 8,
  "parity": "none",
  "stop_bits": 1
}
```

---

## Recipe Format

Recipes automate login sequences and command execution. See [tcm/docs/recipe-format.md](tcm/docs/recipe-format.md) for the full specification.

### Minimal example

```json
{
  "name": "quick-login",
  "description": "Login and check version",
  "steps": [
    { "expect": "(?i)login",    "send": "{username}\n", "timeout_ms": 10000, "on_timeout": "abort" },
    { "expect": "(?i)password", "send": "{password}\n", "timeout_ms": 5000,  "on_timeout": "abort" },
    { "expect": "[#$>]",        "send": "show version\n","timeout_ms": 3000, "on_timeout": "skip"  }
  ],
  "variables": {
    "username": "admin",
    "password": ""
  }
}
```

Example recipes are available in `tcm/recipes/examples/`.

---

## Configuration File Locations

| File | Purpose |
|------|---------|
| `~/.config/tcm/sessions.json` | Saved sessions |
| `~/.config/tcm/recipes/` | Recipe JSON files |
| `~/.local/share/tcm/logs/` | Asciicast session logs |

---

## Project Structure

```
tcm/
├── include/          # Public headers
│   ├── app/
│   ├── core/
│   ├── credential/
│   ├── logger/
│   ├── macro/
│   ├── protocol/
│   └── tui/
├── src/              # Implementation
├── tests/            # Unit tests (GoogleTest)
├── recipes/
│   └── examples/     # Example JSON recipes
├── docs/             # Documentation
├── scripts/
│   ├── build.sh      # Build helper
│   └── install.sh    # Install helper
└── CMakeLists.txt

tcm-docs/
├── SAD-TCM.md                  # Software Architecture Document
├── SRS-TCM.md                  # Software Requirements Specification
├── phase-1-core-foundation.md  # Phase 1 plan
├── phase-2-protocol-adapters.md
├── phase-3-4-5-plans.md
├── phase-1-report.md           # Phase reports
├── phase-2-report.md
├── phase-3-report.md
├── phase-4-report.md
├── phase-5-report.md
└── project-completion-report.md
```

---

## Running Tests

```bash
cd tcm
ctest --test-dir build --output-on-failure
```

Or via the build script (tests run automatically after every build):

```bash
cd tcm
./scripts/build.sh
```

---

## Contributing

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Build with ASan enabled during development: `cd tcm && ./scripts/build.sh --asan`
4. Ensure all tests pass before submitting
5. Open a pull request with a clear description

### Code style

- C++20, no exceptions in new code
- Follow existing patterns in `src/`
- Add unit tests for new functionality in `tests/`
- Use `clang-format` with the project style

---

## License

See `LICENSE` file in the repository root.


## Features

- **Multi-protocol**: SSH, Telnet, Serial (RS-232)
- **TUI interface**: ncurses-based session list with live switcher overlay
- **Macro/Recipe engine**: Automate login sequences with expect/send steps
- **Session logging**: Asciicast v2 format for replay
- **Credential store**: Encrypted password storage
- **Epoll-driven**: Non-blocking I/O for all connections

---

## Requirements

| Dependency | Version |
|------------|---------|
| GCC / Clang | C++20 capable |
| CMake | ≥ 3.20 |
| libssh2 | ≥ 1.9 |
| OpenSSL | ≥ 1.1 |
| ncurses | ≥ 6.0 |
| pkg-config | any |

### Install dependencies (Debian/Ubuntu)

```bash
sudo apt-get install -y \
    build-essential cmake pkg-config \
    libssh2-1-dev libssl-dev libncurses-dev
```

---

## Build

### Debug (default)

```bash
cd tcm
./scripts/build.sh
```

### Release

```bash
./scripts/build.sh --release
```

### With AddressSanitizer

```bash
./scripts/build.sh --asan
```

### Static binary

```bash
./scripts/build.sh --release --static
```

### Manual CMake

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

---

## Install

After building, run the install script from the project root:

```bash
cd tcm
./scripts/install.sh
```

This will:
1. Copy the binary to `/usr/local/bin/tcm`
2. Create `~/.config/tcm/` with a default `sessions.json`
3. Create `~/.local/share/tcm/logs/` for session logs
4. Copy example recipes to `~/.config/tcm/recipes/`

---

## Quick Start (5 minutes)

### 1. Launch TCM

```bash
tcm
```

### 2. Add a session

Press `n` in the session list to open the New Session form. Fill in:
- **Name**: `my-router`
- **Protocol**: `ssh`
- **Host**: `192.168.1.1`
- **Port**: `22`
- **Username**: `admin`

Press `Enter` to save.

### 3. Connect

Highlight the session and press `Enter` or `c` to connect.

### 4. Switch sessions

Press `Ctrl-b` to open the switcher overlay and select another session.

### 5. Run a recipe

While connected, press `m` and select a recipe from `~/.config/tcm/recipes/`.

---

## Key Bindings

### Session List View

| Key | Action |
|-----|--------|
| `j` / `↓` | Move down |
| `k` / `↑` | Move up |
| `Enter` / `c` | Connect to selected session |
| `n` | New session |
| `e` | Edit selected session |
| `d` | Delete selected session |
| `m` | Run macro/recipe |
| `q` | Quit |

### Connected Session

| Key | Action |
|-----|--------|
| `Ctrl-]` | Open session switcher overlay |
| `Ctrl-]` then `d` | Detach — return to session list (session stays connected) |
| `Ctrl-]` then `1-9` | Switch to connected session by number |
| `Ctrl-]` then `Esc` | Close overlay, stay in current session |

### Switcher Overlay

| Key | Action |
|-----|--------|
| `1`-`9` | Switch to session by number |
| `d` | Detach — go back to session list |
| `Esc` | Close overlay, stay in current session |
---

## Session Configuration

Sessions are stored in `~/.config/tcm/sessions.json`.

### SSH session

```json
{
  "name": "prod-router",
  "protocol": "ssh",
  "host": "10.0.0.1",
  "port": 22,
  "username": "admin",
  "auth_method": "password"
}
```

### Telnet session

```json
{
  "name": "old-switch",
  "protocol": "telnet",
  "host": "192.168.1.254",
  "port": 23
}
```

### Serial session

```json
{
  "name": "console-port",
  "protocol": "serial",
  "device": "/dev/ttyUSB0",
  "baud_rate": 9600,
  "data_bits": 8,
  "parity": "none",
  "stop_bits": 1
}
```

---

## Recipe Format

Recipes automate login sequences and command execution. See [docs/recipe-format.md](docs/recipe-format.md) for the full specification.

### Minimal example

```json
{
  "name": "quick-login",
  "description": "Login and check version",
  "steps": [
    { "expect": "(?i)login",    "send": "{username}\n", "timeout_ms": 10000, "on_timeout": "abort" },
    { "expect": "(?i)password", "send": "{password}\n", "timeout_ms": 5000,  "on_timeout": "abort" },
    { "expect": "[#$>]",        "send": "show version\n","timeout_ms": 3000, "on_timeout": "skip"  }
  ],
  "variables": {
    "username": "admin",
    "password": ""
  }
}
```

Example recipes are available in `recipes/examples/`.

---

## Configuration File Locations

| File | Purpose |
|------|---------|
| `~/.config/tcm/sessions.json` | Saved sessions |
| `~/.config/tcm/recipes/` | Recipe JSON files |
| `~/.local/share/tcm/logs/` | Asciicast session logs |

---

## Project Structure

```
tcm/
├── include/          # Public headers
│   ├── app/
│   ├── core/
│   ├── credential/
│   ├── logger/
│   ├── macro/
│   ├── protocol/
│   └── tui/
├── src/              # Implementation
├── tests/            # Unit tests (GoogleTest)
├── recipes/
│   └── examples/     # Example JSON recipes
├── docs/             # Documentation
├── scripts/
│   ├── build.sh      # Build helper
│   └── install.sh    # Install helper
└── CMakeLists.txt
```

---

## Running Tests

```bash
ctest --test-dir build --output-on-failure
```

Or via the build script (tests run automatically after every build):

```bash
./scripts/build.sh
```

---

## Contributing

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Build with ASan enabled during development: `./scripts/build.sh --asan`
4. Ensure all tests pass before submitting
5. Open a pull request with a clear description

### Code style

- C++20, no exceptions in new code
- Follow existing patterns in `src/`
- Add unit tests for new functionality in `tests/`
- Use `clang-format` with the project style

---

## License

See `LICENSE` file in the repository root.
