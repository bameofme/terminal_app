#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "core/PtyProxy.hpp"
#include "core/RingBuffer.hpp"
#include "core/EventBus.hpp"

#include <unistd.h>
#include <sys/types.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

using namespace tcm;

// ──────────────────────────────────────────────────────────────────────────────
// Helper: poll cond() up to timeoutMs
// ──────────────────────────────────────────────────────────────────────────────
static bool waitFor(std::function<bool()> cond, int timeoutMs = 2000) {
    auto deadline = std::chrono::steady_clock::now()
                  + std::chrono::milliseconds(timeoutMs);
    while (!cond()) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return true;
}

// ──────────────────────────────────────────────────────────────────────────────
// Fixture
// ──────────────────────────────────────────────────────────────────────────────
class PtyProxyTest : public ::testing::Test {
protected:
    RingBuffer buffer;
    EventBus   bus;
};

// ──────────────────────────────────────────────────────────────────────────────
// Test 1: spawn() returns true, masterFd() is a valid fd
// ──────────────────────────────────────────────────────────────────────────────
TEST_F(PtyProxyTest, SpawnSucceeds) {
    PtyProxy proxy(buffer, bus, "sess-1");
    EXPECT_TRUE(proxy.spawn({"/bin/echo", "hello"}));
    EXPECT_GT(proxy.masterFd(), 0);
    proxy.kill();
}

// ──────────────────────────────────────────────────────────────────────────────
// Test 2: write() forwards data to child (cat echoes it back to masterFd)
// ──────────────────────────────────────────────────────────────────────────────
TEST_F(PtyProxyTest, WriteToChildAndReadBack) {
    PtyProxy proxy(buffer, bus, "sess-2");
    ASSERT_TRUE(proxy.spawn({"/bin/cat"}));

    const uint8_t msg[] = "hello\n";
    int written = proxy.write(msg, sizeof(msg) - 1);
    EXPECT_GE(written, 0);

    // Poll onData() until buffer accumulates data echoed by cat
    bool gotData = waitFor([&]() {
        proxy.onData();
        return buffer.available() > 0;
    });
    EXPECT_TRUE(gotData);
    EXPECT_GT(buffer.available(), 0u);

    proxy.kill();
}

// ──────────────────────────────────────────────────────────────────────────────
// Test 3: setActive(true) causes onData() to write child output to stdout
//         (no crash; DataReceivedEvent still published)
// ──────────────────────────────────────────────────────────────────────────────
TEST_F(PtyProxyTest, SetActiveTrueForwardsData) {
    PtyProxy proxy(buffer, bus, "sess-3");
    ASSERT_TRUE(proxy.spawn({"/bin/echo", "active-test"}));

    std::atomic<int> eventCount{0};
    auto token = bus.subscribe<DataReceivedEvent>(
        [&](const DataReceivedEvent& /*e*/) { eventCount.fetch_add(1); });

    proxy.setActive(true);

    // Poll onData() until at least one DataReceivedEvent is fired
    bool got = waitFor([&]() {
        proxy.onData();
        return eventCount.load() > 0;
    });
    EXPECT_TRUE(got);

    bus.unsubscribe(token);
    proxy.kill();
}

// ──────────────────────────────────────────────────────────────────────────────
// Test 4: setActive(false) — child output lands in RingBuffer
// ──────────────────────────────────────────────────────────────────────────────
TEST_F(PtyProxyTest, SetActiveInactiveRoutesToRingBuffer) {
    PtyProxy proxy(buffer, bus, "sess-4");
    ASSERT_TRUE(proxy.spawn({"/bin/echo", "buftest"}));

    proxy.setActive(false);

    bool gotData = waitFor([&]() {
        proxy.onData();
        return buffer.available() > 0;
    });
    EXPECT_TRUE(gotData);
    EXPECT_GT(buffer.available(), 0u);

    proxy.kill();
}

// ──────────────────────────────────────────────────────────────────────────────
// Test 5: kill() causes isRunning() to return false
// ──────────────────────────────────────────────────────────────────────────────
TEST_F(PtyProxyTest, KillStopsChild) {
    PtyProxy proxy(buffer, bus, "sess-5");
    ASSERT_TRUE(proxy.spawn({"/bin/cat"}));
    EXPECT_TRUE(proxy.isRunning());

    proxy.kill();
    EXPECT_FALSE(proxy.isRunning());
}

// ──────────────────────────────────────────────────────────────────────────────
// Test 6: 0x1D (Ctrl+]) in write() publishes KeyPressedEvent, not forwarded
// ──────────────────────────────────────────────────────────────────────────────
TEST_F(PtyProxyTest, Ctrl29InWritePublishesEventAndIsFiltered) {
    PtyProxy proxy(buffer, bus, "sess-6");
    ASSERT_TRUE(proxy.spawn({"/bin/cat"}));

    std::atomic<int> keyEvents{0};
    auto token = bus.subscribe<KeyPressedEvent>([&](const KeyPressedEvent& e) {
        if (e.key == 0x1D && e.isEscape) {
            keyEvents.fetch_add(1);
        }
    });

    // Write a 0x1D byte — should trigger the event, NOT be forwarded to cat
    const uint8_t data[] = {0x1D};
    int ret = proxy.write(data, 1);

    // publish is synchronous inside write(), so event fires before return
    EXPECT_EQ(keyEvents.load(), 1);

    // write() should return 0 (nothing forwarded) not an error
    EXPECT_EQ(ret, 0);

    bus.unsubscribe(token);
    proxy.kill();
}
