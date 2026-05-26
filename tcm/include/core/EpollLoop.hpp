#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace tcm {

class EpollLoop {
public:
    EpollLoop();
    ~EpollLoop();

    // Non-copyable, non-movable
    EpollLoop(const EpollLoop&) = delete;
    EpollLoop& operator=(const EpollLoop&) = delete;

    // Add fd to monitor. Callback called when fd has data to read.
    void addFd(int fd, std::function<void(int)> callback);

    // Remove fd from monitoring
    void removeFd(int fd);

    // Run event loop (blocks until stop() is called)
    void run();

    // Stop the event loop (thread-safe)
    void stop();

    // Run loop in dedicated thread
    void runAsync();

    // Wait for async loop to stop
    void join();

private:
    int m_epollFd = -1;
    int m_wakeFd = -1;  // eventfd to wake up epoll_wait
    std::unordered_map<int, std::function<void(int)>> m_handlers;
    std::mutex m_handlersMutex;
    std::atomic<bool> m_running{false};
    std::thread m_thread;
};

} // namespace tcm
