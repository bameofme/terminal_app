#include <gtest/gtest.h>

#include "core/EpollLoop.hpp"

#include <sys/types.h>
#include <unistd.h>

// Intentional read/write return value discards in test helpers
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"

#include <atomic>
#include <chrono>
#include <cstring>
#include <functional>
#include <thread>

using namespace tcm;

// ──────────────────────────────────────────────────────────────────────────────
// Helper: busy-wait up to timeoutMs for cond() to become true
// ──────────────────────────────────────────────────────────────────────────────
static bool waitFor(std::function<bool()> cond, int timeoutMs = 1000) {
    auto deadline = std::chrono::steady_clock::now()
                  + std::chrono::milliseconds(timeoutMs);
    while (!cond()) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return true;
}

// ──────────────────────────────────────────────────────────────────────────────
// Test 1: Callback is invoked when pipe write end receives data
// ──────────────────────────────────────────────────────────────────────────────
TEST(EpollLoopTest, CallbackCalledWhenFdHasData) {
    EpollLoop loop;

    int pipefd[2];
    ASSERT_EQ(pipe(pipefd), 0);

    std::atomic<int> callCount{0};
    loop.addFd(pipefd[0], [&](int fd) {
        char buf[16] = {};
        (void)read(fd, buf, sizeof(buf));
        callCount.fetch_add(1);
    });

    loop.runAsync();

    const char* msg = "hello";
    (void)::write(pipefd[1], msg, std::strlen(msg));

    bool fired = waitFor([&]() { return callCount.load() > 0; });

    loop.stop();
    loop.join();

    ::close(pipefd[0]);
    ::close(pipefd[1]);

    EXPECT_TRUE(fired);
    EXPECT_GT(callCount.load(), 0);
}

// ──────────────────────────────────────────────────────────────────────────────
// Test 2: Callback is NOT invoked after removeFd()
// ──────────────────────────────────────────────────────────────────────────────
TEST(EpollLoopTest, CallbackNotCalledAfterRemoveFd) {
    EpollLoop loop;

    int pipefd[2];
    ASSERT_EQ(pipe(pipefd), 0);

    std::atomic<int> callCount{0};
    loop.addFd(pipefd[0], [&](int fd) {
        char buf[16] = {};
        (void)read(fd, buf, sizeof(buf));
        callCount.fetch_add(1);
    });

    loop.removeFd(pipefd[0]);
    loop.runAsync();

    (void)::write(pipefd[1], "test", 4);

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    loop.stop();
    loop.join();

    ::close(pipefd[0]);
    ::close(pipefd[1]);

    EXPECT_EQ(callCount.load(), 0);
}

// ──────────────────────────────────────────────────────────────────────────────
// Test 3: stop() terminates the loop promptly (no blocking)
// ──────────────────────────────────────────────────────────────────────────────
TEST(EpollLoopTest, StopTerminatesLoopPromptly) {
    EpollLoop loop;
    loop.runAsync();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto t0 = std::chrono::steady_clock::now();
    loop.stop();
    loop.join();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();

    EXPECT_LT(elapsed, 500);
}

// ──────────────────────────────────────────────────────────────────────────────
// Test 4: runAsync() runs the loop in a separate thread
// ──────────────────────────────────────────────────────────────────────────────
TEST(EpollLoopTest, RunAsyncDoesNotBlockCallerThread) {
    EpollLoop loop;

    int pipefd[2];
    ASSERT_EQ(pipe(pipefd), 0);

    std::atomic<bool> callbackCalled{false};
    loop.addFd(pipefd[0], [&](int fd) {
        char buf[4] = {};
        (void)read(fd, buf, sizeof(buf));
        callbackCalled = true;
    });

    loop.runAsync();
    // Caller thread continues immediately
    (void)::write(pipefd[1], "x", 1);

    bool fired = waitFor([&]() { return callbackCalled.load(); });

    loop.stop();
    loop.join();

    ::close(pipefd[0]);
    ::close(pipefd[1]);

    EXPECT_TRUE(fired);
}

// ──────────────────────────────────────────────────────────────────────────────
// Test 5: Multiple fds are monitored simultaneously
// ──────────────────────────────────────────────────────────────────────────────
TEST(EpollLoopTest, MultipleFdsMonitoredSimultaneously) {
    EpollLoop loop;

    int pipe1[2], pipe2[2];
    ASSERT_EQ(pipe(pipe1), 0);
    ASSERT_EQ(pipe(pipe2), 0);

    std::atomic<int> count1{0}, count2{0};

    loop.addFd(pipe1[0], [&](int fd) {
        char buf[4] = {};
        (void)read(fd, buf, sizeof(buf));
        count1.fetch_add(1);
    });
    loop.addFd(pipe2[0], [&](int fd) {
        char buf[4] = {};
        (void)read(fd, buf, sizeof(buf));
        count2.fetch_add(1);
    });

    loop.runAsync();

    (void)::write(pipe1[1], "a", 1);
    (void)::write(pipe2[1], "b", 1);

    bool both = waitFor([&]() {
        return count1.load() > 0 && count2.load() > 0;
    });

    loop.stop();
    loop.join();

    ::close(pipe1[0]); ::close(pipe1[1]);
    ::close(pipe2[0]); ::close(pipe2[1]);

    EXPECT_TRUE(both);
    EXPECT_GT(count1.load(), 0);
    EXPECT_GT(count2.load(), 0);
}

#pragma GCC diagnostic pop
