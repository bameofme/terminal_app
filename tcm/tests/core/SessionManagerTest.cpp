#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "core/Session.hpp"
#include "core/SessionManager.hpp"
#include "MockConnection.hpp"

using namespace tcm;        // NOLINT(google-build-using-namespace)
using namespace tcm::test;  // NOLINT(google-build-using-namespace)
using ::testing::Return;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class SessionManagerTest : public ::testing::Test {
protected:
    EventBus       bus;
    SessionManager mgr{bus};

    static Session makeSession(const std::string& name,
                               SessionType type = SessionType::SSH) {
        Session s;
        s.id   = generateUUID();
        s.name = name;
        s.type = type;
        return s;
    }
};

// ---------------------------------------------------------------------------
// 1. Session fields are set correctly
// ---------------------------------------------------------------------------
TEST_F(SessionManagerTest, SessionHasCorrectFields) {
    Session s;
    s.id   = "test-id";
    s.name = "My Session";
    s.type = SessionType::Serial;

    EXPECT_EQ(s.id,   "test-id");
    EXPECT_EQ(s.name, "My Session");
    EXPECT_EQ(s.type, SessionType::Serial);
}

// ---------------------------------------------------------------------------
// 2. UUID is unique across two calls
// ---------------------------------------------------------------------------
TEST_F(SessionManagerTest, SessionIdIsUnique) {
    const std::string id1 = generateUUID();
    const std::string id2 = generateUUID();
    EXPECT_NE(id1, id2);
}

// ---------------------------------------------------------------------------
// 3. Default session state is Idle
// ---------------------------------------------------------------------------
TEST_F(SessionManagerTest, DefaultStateIsIdle) {
    Session s;
    EXPECT_EQ(s.state, SessionState::Idle);
}

// ---------------------------------------------------------------------------
// 4. add() increases count by 1
// ---------------------------------------------------------------------------
TEST_F(SessionManagerTest, AddIncreasesCount) {
    EXPECT_EQ(mgr.count(), 0u);
    mgr.add(makeSession("S1"));
    EXPECT_EQ(mgr.count(), 1u);
}

// ---------------------------------------------------------------------------
// 5. add() returns a non-empty id string
// ---------------------------------------------------------------------------
TEST_F(SessionManagerTest, AddReturnsNonEmptyId) {
    const std::string id = mgr.add(makeSession("S1"));
    EXPECT_FALSE(id.empty());
}

// ---------------------------------------------------------------------------
// 6. remove() decreases count by 1
// ---------------------------------------------------------------------------
TEST_F(SessionManagerTest, RemoveDecreasesCount) {
    const std::string id = mgr.add(makeSession("S1"));
    EXPECT_EQ(mgr.count(), 1u);
    EXPECT_TRUE(mgr.remove(id));
    EXPECT_EQ(mgr.count(), 0u);
}

// ---------------------------------------------------------------------------
// 7. remove() calls disconnect() when the session is Connected
// ---------------------------------------------------------------------------
TEST_F(SessionManagerTest, RemoveDisconnectsConnectedSession) {
    auto* mockPtr = new MockConnection();
    EXPECT_CALL(*mockPtr, getState())
        .WillOnce(Return(ConnectionState::Connected));
    EXPECT_CALL(*mockPtr, disconnect()).Times(1);

    Session s = makeSession("S1");
    s.conn    = std::unique_ptr<IConnection>(mockPtr);

    const std::string id = mgr.add(std::move(s));
    mgr.remove(id);
}

// ---------------------------------------------------------------------------
// 8. setActive() changes getActiveId()
// ---------------------------------------------------------------------------
TEST_F(SessionManagerTest, SetActiveChangesActiveId) {
    const std::string id = mgr.add(makeSession("S1"));
    EXPECT_TRUE(mgr.setActive(id));
    EXPECT_EQ(mgr.getActiveId(), id);
}

// ---------------------------------------------------------------------------
// 9. getActive() returns nullptr when no sessions exist
// ---------------------------------------------------------------------------
TEST_F(SessionManagerTest, GetActiveReturnsNullWhenEmpty) {
    EXPECT_EQ(mgr.getActive(), nullptr);
}

// ---------------------------------------------------------------------------
// 10. setActive() with a non-existent id returns false
// ---------------------------------------------------------------------------
TEST_F(SessionManagerTest, SetActiveWithNonExistentIdReturnsFalse) {
    EXPECT_FALSE(mgr.setActive("does-not-exist"));
}

// ---------------------------------------------------------------------------
// 11. forEach() iterates over all sessions
// ---------------------------------------------------------------------------
TEST_F(SessionManagerTest, ForEachIteratesAllSessions) {
    mgr.add(makeSession("S1"));
    mgr.add(makeSession("S2"));
    mgr.add(makeSession("S3"));

    int visits = 0;
    mgr.forEach([&visits](Session&) { ++visits; });
    EXPECT_EQ(visits, 3);
}

// ---------------------------------------------------------------------------
// 12. Removing the active session resets active id to ""
// ---------------------------------------------------------------------------
TEST_F(SessionManagerTest, RemoveActiveSessionResetsActiveId) {
    const std::string id = mgr.add(makeSession("S1"));
    mgr.setActive(id);
    ASSERT_EQ(mgr.getActiveId(), id);

    mgr.remove(id);
    EXPECT_EQ(mgr.getActiveId(), "");
}

// ---------------------------------------------------------------------------
// 13. findById() returns the correct session
// ---------------------------------------------------------------------------
TEST_F(SessionManagerTest, FindByIdReturnsCorrectSession) {
    mgr.add(makeSession("S1"));

    Session s2   = makeSession("S2");
    const std::string id2 = s2.id;
    mgr.add(std::move(s2));

    Session* found = mgr.findById(id2);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name, "S2");
}
