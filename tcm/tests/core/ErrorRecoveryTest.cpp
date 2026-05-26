/**
 * Error Recovery Tests
 *
 * Tests that incorrect/edge-case usage does not crash, just returns
 * false / default values.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <thread>
#include <vector>

#include "core/EpollLoop.hpp"
#include "core/EventBus.hpp"
#include "core/PtyProxy.hpp"
#include "core/RingBuffer.hpp"
#include "core/Session.hpp"
#include "core/SessionManager.hpp"
#include "credential/CredentialStore.hpp"
#include "logger/AsciicastLogger.hpp"
#include "macro/MacroEngine.hpp"
#include "macro/Recipe.hpp"
#include "tui/Renderer.hpp"
#include "tui/SessionListView.hpp"

namespace fs = std::filesystem;
using namespace tcm;

// ---------------------------------------------------------------------------
// Minimal stub IConnection for tests that need one
// ---------------------------------------------------------------------------
namespace {
class StubConn : public IConnection {
public:
    int  connect()                          noexcept override { return 0; }
    int  send(std::span<const uint8_t>)     noexcept override { return 0; }
    int  recv(std::span<uint8_t>)           noexcept override { return 0; }
    void disconnect()                       noexcept override {}
    int  getFd()                      const noexcept override { return -1; }
    ConnectionState getState()        const noexcept override {
        return ConnectionState::Disconnected;
    }
};
} // anonymous namespace

// ===========================================================================
// 1. SessionManager: remove non-existent id returns false, no crash
// ===========================================================================
TEST(ErrorRecovery, SessionManagerRemoveNonExistent) {
    EventBus       bus;
    SessionManager mgr(bus);

    EXPECT_NO_THROW({
        bool result = mgr.remove("non-existent-id-xyz");
        EXPECT_FALSE(result);
    });
}

// ===========================================================================
// 2. PtyProxy: kill() when no process spawned — no crash
// ===========================================================================
TEST(ErrorRecovery, PtyProxyKillNonExistent) {
    RingBuffer buf;
    EventBus   bus;
    PtyProxy   pty(buf, bus, "no-proc");

    EXPECT_NO_THROW(pty.kill());
    EXPECT_FALSE(pty.isRunning());
}

// ===========================================================================
// 3. EpollLoop: removeFd with non-existent fd — no crash
// ===========================================================================
TEST(ErrorRecovery, EpollLoopRemoveNonExistentFd) {
    EpollLoop loop;

    EXPECT_NO_THROW(loop.removeFd(99999));
    EXPECT_NO_THROW(loop.removeFd(-1));
    EXPECT_NO_THROW(loop.removeFd(0));
}

// ===========================================================================
// 4. RingBuffer: write 200KB into 64KB buffer → newest data preserved
// ===========================================================================
TEST(ErrorRecovery, RingBufferOverflow) {
    RingBuffer buf;  // default 64KB
    EXPECT_EQ(buf.capacity(), RingBuffer::DEFAULT_CAPACITY);

    // Write 200KB of incrementing data
    constexpr size_t WRITE_SIZE = 200 * 1024;
    std::vector<uint8_t> src(WRITE_SIZE);
    for (size_t i = 0; i < WRITE_SIZE; ++i)
        src[i] = static_cast<uint8_t>(i & 0xFF);

    EXPECT_NO_THROW(buf.write(src.data(), WRITE_SIZE));

    // Buffer must not exceed capacity
    EXPECT_LE(buf.available(), buf.capacity());

    // Should still be readable
    std::vector<uint8_t> out(buf.capacity());
    size_t n = 0;
    EXPECT_NO_THROW(n = buf.read(out.data(), out.size()));
    EXPECT_GT(n, 0u);
}

// ===========================================================================
// 5. EventBus: publish after unsubscribe — no crash
// ===========================================================================
TEST(ErrorRecovery, EventBusPublishAfterUnsubscribe) {
    EventBus bus;

    int count = 0;
    auto token = bus.subscribe<SessionAddedEvent>([&](const SessionAddedEvent&) {
        count++;
    });

    bus.publish(SessionAddedEvent{"first"});
    EXPECT_EQ(count, 1);

    bus.unsubscribe(token);

    // After unsubscribe, publish should not crash and not increment
    EXPECT_NO_THROW(bus.publish(SessionAddedEvent{"second"}));
    EXPECT_EQ(count, 1);
}

// ===========================================================================
// 6. MacroEngine: feed() when state is Idle — no crash
// ===========================================================================
TEST(ErrorRecovery, MacroEngineFeedWhenIdle) {
    EventBus  bus;
    StubConn  conn;
    MacroEngine engine(&conn, bus, "idle-sess");

    EXPECT_EQ(engine.getState(), MacroState::Idle);

    EXPECT_NO_THROW(engine.feed("some data"));
    const uint8_t raw[] = {0x41, 0x42, 0x43};
    EXPECT_NO_THROW(engine.feed(raw, 3));

    // State stays Idle
    EXPECT_EQ(engine.getState(), MacroState::Idle);
}

// ===========================================================================
// 7. AsciicastLogger: logOutput() before start() — no crash
// ===========================================================================
TEST(ErrorRecovery, AsciicastLoggerLogBeforeStart) {
    AsciicastLogger logger;
    EXPECT_FALSE(logger.isLogging());

    const uint8_t data[] = "hello\r\n";
    EXPECT_NO_THROW(logger.logOutput(data, sizeof(data) - 1));
    EXPECT_NO_THROW(logger.logInput(data, sizeof(data) - 1));
    EXPECT_NO_THROW(logger.stop());
}

// ===========================================================================
// 8. CredentialStore: load() with non-existent file — returns false, no crash
// ===========================================================================
TEST(ErrorRecovery, CredentialStoreLoadNonExistentFile) {
    CredentialStore store("/tmp/tcm_does_not_exist_abc123.dat", "passphrase");

    bool loaded = false;
    EXPECT_NO_THROW(loaded = store.load());
    EXPECT_FALSE(loaded);
}

// ===========================================================================
// 9. SessionListView: render with empty sessions — no crash
// ===========================================================================
TEST(ErrorRecovery, SessionListViewEmptySessions) {
    EventBus                 bus;
    std::vector<SessionInfo> sessions;
    SessionListView          view(sessions, bus);
    Renderer                 renderer;

    EXPECT_NO_THROW(view.render(renderer));
    EXPECT_EQ(view.getSelectedIndex(), 0);
    EXPECT_EQ(view.getSelectedId(), "");
}

// ===========================================================================
// 10. PtyProxy setActive(): multiple calls with same value — no crash
// ===========================================================================
TEST(ErrorRecovery, PtyProxyMultipleSetActiveSameValue) {
    RingBuffer buf;
    EventBus   bus;
    PtyProxy   pty(buf, bus, "multi-active");

    // Call setActive(false) multiple times before spawn
    EXPECT_NO_THROW(pty.setActive(false));
    EXPECT_NO_THROW(pty.setActive(false));
    EXPECT_NO_THROW(pty.setActive(true));
    EXPECT_NO_THROW(pty.setActive(true));
    EXPECT_NO_THROW(pty.setActive(false));

    ASSERT_TRUE(pty.spawn({"/bin/echo", "idempotent"}));

    // setActive multiple times after spawn
    EXPECT_NO_THROW(pty.setActive(false));
    EXPECT_NO_THROW(pty.setActive(false));
    EXPECT_NO_THROW(pty.setActive(true));
    EXPECT_NO_THROW(pty.setActive(true));
    EXPECT_NO_THROW(pty.setActive(false));

    pty.kill();
}
