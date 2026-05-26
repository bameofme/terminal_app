#include "core/EpollLoop.hpp"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <cerrno>
#include <stdexcept>

namespace tcm {

EpollLoop::EpollLoop() {
    m_epollFd = epoll_create1(EPOLL_CLOEXEC);
    if (m_epollFd < 0) {
        throw std::runtime_error("epoll_create1 failed");
    }

    m_wakeFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (m_wakeFd < 0) {
        ::close(m_epollFd);
        throw std::runtime_error("eventfd failed");
    }

    epoll_event ev{};
    ev.events  = EPOLLIN;
    ev.data.fd = m_wakeFd;
    if (epoll_ctl(m_epollFd, EPOLL_CTL_ADD, m_wakeFd, &ev) < 0) {
        ::close(m_wakeFd);
        ::close(m_epollFd);
        throw std::runtime_error("epoll_ctl failed for wakeFd");
    }
}

EpollLoop::~EpollLoop() {
    if (m_running.load()) {
        stop();
    }
    if (m_thread.joinable()) {
        m_thread.join();
    }
    if (m_wakeFd >= 0) ::close(m_wakeFd);
    if (m_epollFd >= 0) ::close(m_epollFd);
}

void EpollLoop::addFd(int fd, std::function<void(int)> callback) {
    std::lock_guard<std::mutex> lock(m_handlersMutex);

    epoll_event ev{};
    ev.events  = EPOLLIN;
    ev.data.fd = fd;
    epoll_ctl(m_epollFd, EPOLL_CTL_ADD, fd, &ev);

    m_handlers[fd] = std::move(callback);
}

void EpollLoop::removeFd(int fd) {
    std::lock_guard<std::mutex> lock(m_handlersMutex);
    epoll_ctl(m_epollFd, EPOLL_CTL_DEL, fd, nullptr);
    m_handlers.erase(fd);
}

void EpollLoop::run() {
    m_running.store(true, std::memory_order_release);

    constexpr int MAX_EVENTS = 16;
    epoll_event events[MAX_EVENTS];

    while (true) {
        int n = epoll_wait(m_epollFd, events, MAX_EVENTS, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            if (fd == m_wakeFd) {
                // Drain the eventfd counter
                uint64_t val = 0;
                if (::read(m_wakeFd, &val, sizeof(val)) < 0) { /* ignore drain error */ }
                m_running.store(false, std::memory_order_release);
                return;
            }

            // Grab a copy of the callback while holding the lock
            std::function<void(int)> cb;
            {
                std::lock_guard<std::mutex> lock(m_handlersMutex);
                auto it = m_handlers.find(fd);
                if (it != m_handlers.end()) {
                    cb = it->second;
                }
            }
            if (cb) cb(fd);
        }
    }

    m_running.store(false, std::memory_order_release);
}

void EpollLoop::stop() {
    m_running.store(false, std::memory_order_release);
    uint64_t val = 1;
    if (::write(m_wakeFd, &val, sizeof(val)) < 0) { /* ignore wake signal error */ }
}

void EpollLoop::runAsync() {
    m_thread = std::thread([this]() { run(); });
}

void EpollLoop::join() {
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

} // namespace tcm
