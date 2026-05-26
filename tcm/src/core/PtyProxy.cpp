#include "core/PtyProxy.hpp"

#include <fcntl.h>
#include <pty.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace tcm {

PtyProxy::PtyProxy(RingBuffer& buffer, EventBus& bus, const std::string& sessionId)
    : m_sessionId(sessionId), m_buffer(buffer), m_bus(bus) {}

PtyProxy::~PtyProxy() {
    if (m_running) {
        kill();
    }
    if (m_masterFd >= 0) {
        ::close(m_masterFd);
        m_masterFd = -1;
    }
}

bool PtyProxy::spawn(const std::vector<std::string>& argv) {
    if (argv.empty()) return false;

    if (openpty(&m_masterFd, &m_slaveFd, nullptr, nullptr, nullptr) < 0) {
        return false;
    }

    m_childPid = fork();
    if (m_childPid < 0) {
        ::close(m_masterFd);
        ::close(m_slaveFd);
        m_masterFd = m_slaveFd = -1;
        return false;
    }

    if (m_childPid == 0) {
        // ---- child process ----
        setsid();
        if (ioctl(m_slaveFd, TIOCSCTTY, 0) < 0) {
            _exit(1);
        }
        if (dup2(m_slaveFd, STDIN_FILENO)  < 0) _exit(1);
        if (dup2(m_slaveFd, STDOUT_FILENO) < 0) _exit(1);
        if (dup2(m_slaveFd, STDERR_FILENO) < 0) _exit(1);

        ::close(m_masterFd);
        if (m_slaveFd > STDERR_FILENO) {
            ::close(m_slaveFd);
        }

        std::vector<char*> args;
        args.reserve(argv.size() + 1);
        for (const auto& a : argv) {
            args.push_back(const_cast<char*>(a.c_str()));
        }
        args.push_back(nullptr);

        execvp(args[0], args.data());
        _exit(127);
    }

    // ---- parent process ----
    ::close(m_slaveFd);
    m_slaveFd = -1;

    // Set master to non-blocking
    int flags = fcntl(m_masterFd, F_GETFL, 0);
    if (flags >= 0) {
        (void)fcntl(m_masterFd, F_SETFL, flags | O_NONBLOCK);
    }

    m_running = true;
    return true;
}

void PtyProxy::onData() {
    if (m_masterFd < 0) return;

    uint8_t buf[4096];
    ssize_t n = ::read(m_masterFd, buf, sizeof(buf));

    if (n < 0) {
        if (errno == EIO) {
            // PTY slave closed — child has exited
            m_running = false;
            if (m_childPid > 0) {
                waitpid(m_childPid, nullptr, WNOHANG);
            }
            m_bus.publish(SessionDisconnectedEvent{m_sessionId});
        }
        // EAGAIN / EWOULDBLOCK: no data available — normal for non-blocking
        return;
    }
    if (n == 0) return;

    if (m_active) {
        if (::write(STDOUT_FILENO, buf, static_cast<size_t>(n)) < 0) { /* ignore stdout write error */ }
    }

    m_buffer.write(buf, static_cast<size_t>(n));

    std::vector<uint8_t> data(buf, buf + n);
    m_bus.publish(DataReceivedEvent{m_sessionId, data});
}

int PtyProxy::write(const uint8_t* data, size_t len) {
    if (m_masterFd < 0 || !m_running) return -1;

    std::vector<uint8_t> filtered;
    filtered.reserve(len);

    for (size_t i = 0; i < len; i++) {
        if (data[i] == 0x1D) {
            processInputByte(data[i]);
        } else {
            filtered.push_back(data[i]);
        }
    }

    if (filtered.empty()) return 0;

    ssize_t written = ::write(m_masterFd, filtered.data(), filtered.size());
    return static_cast<int>(written);
}

void PtyProxy::setActive(bool active) {
    if (active && !m_active) {
        // Flush buffered output to stdout before going active
        size_t avail = m_buffer.available();
        if (avail > 0) {
            std::vector<uint8_t> tmp(avail);
            m_buffer.read(tmp.data(), avail);
            if (::write(STDOUT_FILENO, tmp.data(), avail) < 0) { /* ignore flush write error */ }
        }
    }
    m_active = active;
}

int PtyProxy::masterFd() const {
    return m_masterFd;
}

void PtyProxy::kill() {
    if (m_childPid > 0) {
        ::kill(m_childPid, SIGTERM);
        // Reap child to avoid zombies
        int status = 0;
        (void)waitpid(m_childPid, &status, 0);
        m_childPid = -1;
    }
    m_running = false;
}

bool PtyProxy::isRunning() const {
    if (!m_running) return false;
    if (m_childPid <= 0) return false;

    int status = 0;
    pid_t result = waitpid(m_childPid, &status, WNOHANG);
    if (result == m_childPid) {
        // Child has exited — update cached state
        m_running  = false;
        m_childPid = -1;
        return false;
    }
    if (result < 0) {
        // Error (child already reaped elsewhere)
        m_running  = false;
        m_childPid = -1;
        return false;
    }
    return true;  // result == 0: still running
}

void PtyProxy::processInputByte(uint8_t byte) {
    if (byte == 0x1D) {
        m_bus.publish(KeyPressedEvent{0x1D, true});
    }
}

} // namespace tcm
