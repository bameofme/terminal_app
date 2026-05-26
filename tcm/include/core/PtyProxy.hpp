#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/RingBuffer.hpp"
#include "core/EventBus.hpp"

namespace tcm {

class PtyProxy {
public:
    PtyProxy(RingBuffer& buffer, EventBus& bus, const std::string& sessionId);
    ~PtyProxy();

    // Non-copyable, non-movable
    PtyProxy(const PtyProxy&) = delete;
    PtyProxy& operator=(const PtyProxy&) = delete;

    // Spawn child process. argv[0] = program path.
    // Returns true on success.
    bool spawn(const std::vector<std::string>& argv);

    // Write data to PTY master (forward to child), filtering 0x1D
    int write(const uint8_t* data, size_t len);

    // Set active: if true, route output to stdout; if false, route to RingBuffer
    void setActive(bool active);

    // Get PTY master fd (for EpollLoop)
    int masterFd() const;

    // Kill child process
    void kill();

    // True if child process is running
    bool isRunning() const;

    // Called by EpollLoop when master fd has data
    void onData();

private:
    int m_masterFd = -1;
    int m_slaveFd  = -1;
    mutable pid_t m_childPid = -1;
    bool m_active  = false;
    mutable bool m_running = false;
    std::string m_sessionId;
    RingBuffer& m_buffer;
    EventBus&   m_bus;

    // Publish KeyPressedEvent for 0x1D in input stream
    void processInputByte(uint8_t byte);
};

} // namespace tcm
