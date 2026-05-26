#include "tui/SwitcherOverlay.hpp"
#include "tui/Renderer.hpp"
#include "core/EventBus.hpp"

#include <gtest/gtest.h>
#include <ncurses.h>

using namespace tcm;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class SwitcherOverlayTest : public ::testing::Test {
protected:
    EventBus                 bus;
    std::vector<SessionInfo> sessions;

    SessionInfo make(const std::string& id, const std::string& name,
                     SessionState state = SessionState::Connected) {
        SessionInfo s;
        s.id      = id;
        s.name    = name;
        s.host    = "host";
        s.typeStr = "SSH";
        s.state   = state;
        return s;
    }
};

// ---------------------------------------------------------------------------
// 1. Overlay không visible ban đầu
// ---------------------------------------------------------------------------
TEST_F(SwitcherOverlayTest, NotVisibleByDefault) {
    SwitcherOverlay overlay(sessions, bus);
    EXPECT_FALSE(overlay.isVisible());
}

// ---------------------------------------------------------------------------
// 2. show() → isVisible() = true
// ---------------------------------------------------------------------------
TEST_F(SwitcherOverlayTest, ShowMakesVisible) {
    SwitcherOverlay overlay(sessions, bus);
    overlay.show();
    EXPECT_TRUE(overlay.isVisible());
}

// ---------------------------------------------------------------------------
// 3. hide() → isVisible() = false
// ---------------------------------------------------------------------------
TEST_F(SwitcherOverlayTest, HideMakesNotVisible) {
    SwitcherOverlay overlay(sessions, bus);
    overlay.show();
    overlay.hide();
    EXPECT_FALSE(overlay.isVisible());
}

// ---------------------------------------------------------------------------
// 4. handleKey(ESC) → hide overlay, return true
// ---------------------------------------------------------------------------
TEST_F(SwitcherOverlayTest, HandleKeyEscHidesOverlay) {
    SwitcherOverlay overlay(sessions, bus);
    overlay.show();
    ASSERT_TRUE(overlay.isVisible());

    bool consumed = overlay.handleKey(27);

    EXPECT_TRUE(consumed);
    EXPECT_FALSE(overlay.isVisible());
}

// ---------------------------------------------------------------------------
// 5. handleKey('1') với 1 connected session → publish SessionSwitchedEvent
// ---------------------------------------------------------------------------
TEST_F(SwitcherOverlayTest, HandleKey1WithOneConnectedSessionPublishesEvent) {
    sessions.push_back(make("session-42", "router-lab-01", SessionState::Connected));
    SwitcherOverlay overlay(sessions, bus);
    overlay.show();

    std::string switchedTo;
    bus.subscribe<SessionSwitchedEvent>([&](const SessionSwitchedEvent& ev) {
        switchedTo = ev.toId;
    });

    bool consumed = overlay.handleKey('1');

    EXPECT_TRUE(consumed);
    EXPECT_EQ("session-42", switchedTo);
    EXPECT_FALSE(overlay.isVisible());  // should hide after switch
}

// ---------------------------------------------------------------------------
// 6. handleKey('2') với chỉ 1 session → ignore, không publish event
// ---------------------------------------------------------------------------
TEST_F(SwitcherOverlayTest, HandleKey2WithOnlyOneSessionIgnores) {
    sessions.push_back(make("session-1", "only-session", SessionState::Connected));
    SwitcherOverlay overlay(sessions, bus);
    overlay.show();

    int eventCount = 0;
    bus.subscribe<SessionSwitchedEvent>([&](const SessionSwitchedEvent&) {
        ++eventCount;
    });

    bool consumed = overlay.handleKey('2');

    EXPECT_TRUE(consumed);       // key consumed
    EXPECT_EQ(0, eventCount);    // but no event published
}

// ---------------------------------------------------------------------------
// 7. handleKey('9') vượt quá số sessions → ignore
// ---------------------------------------------------------------------------
TEST_F(SwitcherOverlayTest, HandleKey9WithFewerSessionsIgnores) {
    sessions.push_back(make("s1", "session1", SessionState::Connected));
    sessions.push_back(make("s2", "session2", SessionState::Connected));
    SwitcherOverlay overlay(sessions, bus);
    overlay.show();

    int eventCount = 0;
    bus.subscribe<SessionSwitchedEvent>([&](const SessionSwitchedEvent&) {
        ++eventCount;
    });

    bool consumed = overlay.handleKey('9');

    EXPECT_TRUE(consumed);
    EXPECT_EQ(0, eventCount);
}

// ---------------------------------------------------------------------------
// 8. render() không crash khi không visible
// ---------------------------------------------------------------------------
TEST_F(SwitcherOverlayTest, RenderNotVisibleDoesNotCrash) {
    SwitcherOverlay overlay(sessions, bus);
    Renderer r;
    EXPECT_NO_THROW(overlay.render(r));
}

// ---------------------------------------------------------------------------
// 9. render() không crash khi visible
// ---------------------------------------------------------------------------
TEST_F(SwitcherOverlayTest, RenderVisibleDoesNotCrash) {
    sessions.push_back(make("s1", "router-lab-01", SessionState::Connected));
    SwitcherOverlay overlay(sessions, bus);
    overlay.show();
    Renderer r;
    EXPECT_NO_THROW(overlay.render(r));
}

// ---------------------------------------------------------------------------
// 10. getConnectedSessions chỉ trả về sessions Connected
//     Kiểm tra gián tiếp qua handleKey
// ---------------------------------------------------------------------------
TEST_F(SwitcherOverlayTest, OnlyConnectedSessionsAreSelectable) {
    // Mix: Idle, Connected, Error, Connected
    sessions.push_back(make("s1", "idle-session",    SessionState::Idle));
    sessions.push_back(make("s2", "connected-first", SessionState::Connected));
    sessions.push_back(make("s3", "error-session",   SessionState::Error));
    sessions.push_back(make("s4", "connected-second",SessionState::Connected));
    SwitcherOverlay overlay(sessions, bus);
    overlay.show();

    std::string switchedTo;
    bus.subscribe<SessionSwitchedEvent>([&](const SessionSwitchedEvent& ev) {
        switchedTo = ev.toId;
    });

    // '1' should select the first *connected* session (s2), not s1
    overlay.handleKey('1');
    EXPECT_EQ("s2", switchedTo);

    // Test second connected session
    switchedTo.clear();
    overlay.show();
    overlay.handleKey('2');
    EXPECT_EQ("s4", switchedTo);
}

// ---------------------------------------------------------------------------
// 11. handleKey avec non-digit → return false
// ---------------------------------------------------------------------------
TEST_F(SwitcherOverlayTest, HandleKeyNonDigitReturnsFalse) {
    SwitcherOverlay overlay(sessions, bus);
    overlay.show();

    EXPECT_FALSE(overlay.handleKey('a'));
    EXPECT_FALSE(overlay.handleKey(KEY_UP));  // ncurses up arrow
    EXPECT_FALSE(overlay.handleKey(0));
}
