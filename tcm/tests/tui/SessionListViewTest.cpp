#include "tui/SessionListView.hpp"
#include "tui/Renderer.hpp"
#include "core/EventBus.hpp"

#include <gtest/gtest.h>

using namespace tcm;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class SessionListViewTest : public ::testing::Test {
protected:
    EventBus                 bus;
    std::vector<SessionInfo> sessions;

    SessionInfo make(const std::string& id, const std::string& name,
                     const std::string& host, const std::string& typeStr,
                     SessionState state = SessionState::Idle) {
        SessionInfo s;
        s.id      = id;
        s.name    = name;
        s.host    = host;
        s.typeStr = typeStr;
        s.state   = state;
        return s;
    }
};

// ---------------------------------------------------------------------------
// 1. render() with 0 sessions does not crash
// ---------------------------------------------------------------------------
TEST_F(SessionListViewTest, RenderEmptyDoesNotCrash) {
    SessionListView view(sessions, bus);
    Renderer        r;
    EXPECT_NO_THROW(view.render(r));
}

// ---------------------------------------------------------------------------
// 2. render() with 3 sessions does not crash
// ---------------------------------------------------------------------------
TEST_F(SessionListViewTest, RenderWithSessionsDoesNotCrash) {
    sessions.push_back(make("1", "server1",  "192.168.1.1",   "SSH"));
    sessions.push_back(make("2", "serial1",  "/dev/ttyUSB0",  "SER"));
    sessions.push_back(make("3", "router",   "192.168.1.254", "TEL", SessionState::Connected));
    SessionListView view(sessions, bus);
    Renderer        r;
    EXPECT_NO_THROW(view.render(r));
}

// ---------------------------------------------------------------------------
// 3. moveDown() increases selectedIndex
// ---------------------------------------------------------------------------
TEST_F(SessionListViewTest, MoveDownIncreasesIndex) {
    sessions.push_back(make("1", "s1", "h1", "SSH"));
    sessions.push_back(make("2", "s2", "h2", "SSH"));
    SessionListView view(sessions, bus);
    EXPECT_EQ(0, view.getSelectedIndex());
    view.moveDown();
    EXPECT_EQ(1, view.getSelectedIndex());
}

// ---------------------------------------------------------------------------
// 4. moveUp() decreases selectedIndex
// ---------------------------------------------------------------------------
TEST_F(SessionListViewTest, MoveUpDecreasesIndex) {
    sessions.push_back(make("1", "s1", "h1", "SSH"));
    sessions.push_back(make("2", "s2", "h2", "SSH"));
    SessionListView view(sessions, bus);
    view.moveDown();
    ASSERT_EQ(1, view.getSelectedIndex());
    view.moveUp();
    EXPECT_EQ(0, view.getSelectedIndex());
}

// ---------------------------------------------------------------------------
// 5. moveUp() clamps at index 0
// ---------------------------------------------------------------------------
TEST_F(SessionListViewTest, MoveUpClampsAtZero) {
    sessions.push_back(make("1", "s1", "h1", "SSH"));
    SessionListView view(sessions, bus);
    view.moveUp();
    EXPECT_EQ(0, view.getSelectedIndex());
}

// ---------------------------------------------------------------------------
// 6. moveDown() clamps at count-1
// ---------------------------------------------------------------------------
TEST_F(SessionListViewTest, MoveDownClampsAtEnd) {
    sessions.push_back(make("1", "s1", "h1", "SSH"));
    sessions.push_back(make("2", "s2", "h2", "SSH"));
    SessionListView view(sessions, bus);
    view.moveDown();
    view.moveDown();  // attempt to go past last element
    EXPECT_EQ(1, view.getSelectedIndex());
}

// ---------------------------------------------------------------------------
// 7. getSelectedId() returns correct id
// ---------------------------------------------------------------------------
TEST_F(SessionListViewTest, GetSelectedIdReturnsCorrectId) {
    sessions.push_back(make("abc123", "s1", "h1", "SSH"));
    SessionListView view(sessions, bus);
    EXPECT_EQ("abc123", view.getSelectedId());
}

// ---------------------------------------------------------------------------
// 8. getSelectedId() returns "" when empty
// ---------------------------------------------------------------------------
TEST_F(SessionListViewTest, GetSelectedIdEmptyWhenNoSessions) {
    SessionListView view(sessions, bus);
    EXPECT_EQ("", view.getSelectedId());
}

// ---------------------------------------------------------------------------
// 9. enterSearchMode() / exitSearchMode()
// ---------------------------------------------------------------------------
TEST_F(SessionListViewTest, SearchModeToggle) {
    SessionListView view(sessions, bus);
    EXPECT_FALSE(view.isSearchMode());
    view.enterSearchMode();
    EXPECT_TRUE(view.isSearchMode());
    view.exitSearchMode();
    EXPECT_FALSE(view.isSearchMode());
}

// ---------------------------------------------------------------------------
// 10. appendSearchChar() builds query correctly
// ---------------------------------------------------------------------------
TEST_F(SessionListViewTest, AppendSearchCharBuildsQuery) {
    SessionListView view(sessions, bus);
    view.enterSearchMode();
    view.appendSearchChar('a');
    view.appendSearchChar('b');
    view.appendSearchChar('c');
    EXPECT_EQ("abc", view.getSearchQuery());
}

// ---------------------------------------------------------------------------
// 11. getFilteredSessions() matches a single session
// ---------------------------------------------------------------------------
TEST_F(SessionListViewTest, FilteredSessionsMatchQuery) {
    sessions.push_back(make("1", "webserver", "192.168.1.1", "SSH"));
    sessions.push_back(make("2", "dbserver",  "192.168.1.2", "SSH"));
    sessions.push_back(make("3", "router",    "10.0.0.1",    "TEL"));
    SessionListView view(sessions, bus);
    view.enterSearchMode();
    view.appendSearchChar('w');
    view.appendSearchChar('e');
    view.appendSearchChar('b');
    auto filtered = view.getFilteredSessions();
    ASSERT_EQ(1u, filtered.size());
    EXPECT_EQ("1", filtered[0].id);
}

// ---------------------------------------------------------------------------
// 12. getFilteredSessions() with empty query returns all sessions
// ---------------------------------------------------------------------------
TEST_F(SessionListViewTest, FilteredSessionsEmptyQueryReturnsAll) {
    sessions.push_back(make("1", "s1", "h1", "SSH"));
    sessions.push_back(make("2", "s2", "h2", "SSH"));
    SessionListView view(sessions, bus);
    auto filtered = view.getFilteredSessions();
    EXPECT_EQ(2u, filtered.size());
}

// ---------------------------------------------------------------------------
// 13. handleKey('j') → selected increases
// ---------------------------------------------------------------------------
TEST_F(SessionListViewTest, HandleKeyJMovesDown) {
    sessions.push_back(make("1", "s1", "h1", "SSH"));
    sessions.push_back(make("2", "s2", "h2", "SSH"));
    SessionListView view(sessions, bus);
    EXPECT_TRUE(view.handleKey('j'));
    EXPECT_EQ(1, view.getSelectedIndex());
}

// ---------------------------------------------------------------------------
// 14. handleKey('k') → selected decreases
// ---------------------------------------------------------------------------
TEST_F(SessionListViewTest, HandleKeyKMovesUp) {
    sessions.push_back(make("1", "s1", "h1", "SSH"));
    sessions.push_back(make("2", "s2", "h2", "SSH"));
    SessionListView view(sessions, bus);
    view.moveDown();
    EXPECT_TRUE(view.handleKey('k'));
    EXPECT_EQ(0, view.getSelectedIndex());
}

// ---------------------------------------------------------------------------
// 15. handleKey('/') → enters search mode
// ---------------------------------------------------------------------------
TEST_F(SessionListViewTest, HandleKeySlashEntersSearchMode) {
    SessionListView view(sessions, bus);
    EXPECT_TRUE(view.handleKey('/'));
    EXPECT_TRUE(view.isSearchMode());
}
