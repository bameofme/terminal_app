/**
 * Integration Test: SessionManager + PtyProxy + EventBus
 *
 * Tests:
 *   1. PTY spawn + EventBus: Spawn /bin/echo, verify DataReceivedEvent published
 *   2. RingBuffer + PtyProxy: setActive(false), data goes to RingBuffer
 *   3. SessionManager + EpollLoop: Add session with MockConnection, verify count
 *   4. EventBus round-trip: publish event, subscriber receives correct data
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "core/EventBus.hpp"
#include "core/EpollLoop.hpp"
#include "core/PtyProxy.hpp"
#include "core/RingBuffer.hpp"
#include "core/Session.hpp"
#include "core/SessionManager.hpp"
#include "MockConnection.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

using namespace tcm;
using namespace tcm::test;
using ::testing::Return;

// ---------------------------------------------------------------------------
// Helper
// ---------------------------------------------------------------------------
static bool waitFor(std::function<bool()> cond, int timeoutMs = 2000) {
    auto deadline = std::chrono::steady_clock::now()
                  + std::chrono::milliseconds(timeoutMs);
    while (!cond()) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return true;
}

// ---------------------------------------------------------------------------
// Test 1: PTY spawn + EventBus — DataReceivedEvent published when child writes
// ---------------------------------------------------------------------------
TEST(CoreIntegration, PtySpawnPublishesDataReceivedEvent) {
    EventBus   bus;
    RingBuffer buf;
    PtyProxy   pty(buf, bus, "int-sess-1");

    std::atomic<int> eventCount{0};
    auto token = bus.subscribe<DataReceivedEvent>([&](const DataReceivedEvent& e) {
        if (e.sessionId == "int-sess-1") {
            eventCount.fetch_add(1, std::memory_order_relaxed);
        }
    });

    ASSERT_TRUE(pty.spawn({"/bin/echo", "hello"}));

    // Poll onData() until we receive an event (max 2s)
    bool got = waitFor([&]() {
        pty.onData();
        return eventCount.load() > 0;
    });

    EXPECT_TRUE(got) << "DataReceivedEvent was not received within timeout";

    bus.unsubscribe(token);
    pty.kill();
}

// ---------------------------------------------------------------------------
// Test 2: RingBuffer + PtyProxy — setActive(false) routes data to RingBuffer
// ---------------------------------------------------------------------------
TEST(CoreIntegration, PtyInactiveRoutesDataToRingBuffer) {
    EventBus   bus;
    RingBuffer buf;
    PtyProxy   pty(buf, bus, "int-sess-2");

    pty.setActive(false);  // inactive → data goes to RingBuffer
    ASSERT_TRUE(pty.spawn({"/bin/cat"}));

    const uint8_t msg[] = "ringbuf-test\n";
    pty.write(msg, sizeof(msg) - 1);

    bool gotData = waitFor([&]() {
        pty.onData();
        return buf.available() > 0;
    });

    EXPECT_TRUE(gotData);
    EXPECT_GT(buf.available(), 0u);
    pty.kill();
}

// ---------------------------------------------------------------------------
// Test 3: SessionManager + EpollLoop — add session, verify count and EpollLoop
// ---------------------------------------------------------------------------
TEST(CoreIntegration, SessionManagerWithEpollLoop) {
    EventBus       bus;
    SessionManager mgr(bus);
    EpollLoop      loop;

    // Add a session with a MockConnection that returns a dummy fd
    auto mock = std::make_unique<MockConnection>();
    EXPECT_CALL(*mock, getState()).WillRepeatedly(Return(ConnectionState::Disconnected));
    EXPECT_CALL(*mock, disconnect()).Times(::testing::AnyNumber());
    EXPECT_CALL(*mock, getFd()).WillRepeatedly(Return(-1));

    Session s;
    s.id   = generateUUID();
    s.name = "mock-session";
    s.type = SessionType::SSH;
    s.conn = std::move(mock);

    auto id = mgr.add(std::move(s));
    EXPECT_EQ(mgr.count(), 1u);
    EXPECT_FALSE(id.empty());

    // EpollLoop: remove a non-existent fd should not crash
    loop.removeFd(999);

    mgr.remove(id);
    EXPECT_EQ(mgr.count(), 0u);
}

// ---------------------------------------------------------------------------
// Test 4: EventBus round-trip — publish event, subscriber gets correct data
// ---------------------------------------------------------------------------
TEST(CoreIntegration, EventBusRoundTrip) {
    EventBus bus;

    std::string receivedId;
    std::vector<uint8_t> receivedData;

    auto token = bus.subscribe<DataReceivedEvent>([&](const DataReceivedEvent& e) {
        receivedId   = e.sessionId;
        receivedData = e.data;
    });

    DataReceivedEvent ev;
    ev.sessionId = "round-trip-session";
    ev.data      = {0x41, 0x42, 0x43};  // "ABC"
    bus.publish(ev);

    EXPECT_EQ(receivedId, "round-trip-session");
    ASSERT_EQ(receivedData.size(), 3u);
    EXPECT_EQ(receivedData[0], 0x41);
    EXPECT_EQ(receivedData[1], 0x42);
    EXPECT_EQ(receivedData[2], 0x43);

    bus.unsubscribe(token);
}
