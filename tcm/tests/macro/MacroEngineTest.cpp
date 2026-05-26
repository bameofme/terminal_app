#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "macro/MacroEngine.hpp"
#include "core/EventBus.hpp"

using namespace tcm;
using namespace testing;

// ---------------------------------------------------------------------------
// MockConnection
// ---------------------------------------------------------------------------
class MockConnection : public IConnection {
public:
    MOCK_METHOD(int,             connect,    (),                        (noexcept, override));
    MOCK_METHOD(int,             send,       (std::span<const uint8_t>),(noexcept, override));
    MOCK_METHOD(int,             recv,       (std::span<uint8_t>),      (noexcept, override));
    MOCK_METHOD(void,            disconnect, (),                        (noexcept, override));
    MOCK_METHOD(int,             getFd,      (),                        (const, noexcept, override));
    MOCK_METHOD(ConnectionState, getState,   (),                        (const, noexcept, override));
};

// ---------------------------------------------------------------------------
// Helper: build a 3-step recipe
// ---------------------------------------------------------------------------
static Recipe makeSimpleRecipe() {
    return Recipe::loadFromString(R"({
        "name": "test-recipe",
        "steps": [
            {"expect": "login:",    "send": "admin\n", "timeout_ms": 5000},
            {"expect": "Password:", "send": "pass\n",  "timeout_ms": 5000},
            {"expect": "#",         "send": "exit\n",  "timeout_ms": 5000}
        ]
    })");
}

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------
class MacroEngineTest : public ::testing::Test {
protected:
    MockConnection mockConn;
    EventBus       bus;
    MacroEngine    engine{&mockConn, bus, "session-1"};
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_F(MacroEngineTest, StartSetsStateRunning) {
    auto recipe = makeSimpleRecipe();
    engine.start(recipe);
    EXPECT_EQ(engine.getState(), MacroState::Running);
}

TEST_F(MacroEngineTest, FeedMatchingDataTriggersSendAndAdvance) {
    EXPECT_CALL(mockConn, send(_)).Times(1).WillOnce(Return(0));

    auto recipe = makeSimpleRecipe();
    engine.start(recipe);
    EXPECT_EQ(engine.currentStep(), 0);

    engine.feed("login:");
    EXPECT_EQ(engine.currentStep(), 1);
}

TEST_F(MacroEngineTest, FeedNonMatchingDataDoesNotAdvance) {
    EXPECT_CALL(mockConn, send(_)).Times(0);

    auto recipe = makeSimpleRecipe();
    engine.start(recipe);

    engine.feed("noise data");
    EXPECT_EQ(engine.currentStep(), 0);
    EXPECT_EQ(engine.getState(), MacroState::Running);
}

TEST_F(MacroEngineTest, FeedMatchingLastStepSetsDone) {
    EXPECT_CALL(mockConn, send(_)).Times(3).WillRepeatedly(Return(0));

    auto recipe = makeSimpleRecipe();
    engine.start(recipe);

    engine.feed("login:");
    engine.feed("Password:");
    engine.feed("#");

    EXPECT_EQ(engine.getState(), MacroState::Done);
}

TEST_F(MacroEngineTest, VariableSubstitution) {
    std::string sentData;
    EXPECT_CALL(mockConn, send(_))
        .WillRepeatedly([&sentData](std::span<const uint8_t> data) noexcept -> int {
            sentData.append(reinterpret_cast<const char*>(data.data()), data.size());
            return 0;
        });

    auto recipe = Recipe::loadFromString(R"({
        "name": "var-test",
        "steps": [
            {"expect": "login:",    "send": "user\n",       "timeout_ms": 5000},
            {"expect": "Password:", "send": "{password}\n", "timeout_ms": 5000}
        ],
        "variables": {"password": "default"}
    })");

    engine.start(recipe, {{"password", "supersecret"}});

    engine.feed("login:");
    sentData.clear();  // discard send for step 0
    engine.feed("Password:");

    EXPECT_EQ(sentData, "supersecret\n");
}

TEST_F(MacroEngineTest, StopSetsStateNotRunning) {
    auto recipe = makeSimpleRecipe();
    engine.start(recipe);
    EXPECT_EQ(engine.getState(), MacroState::Running);

    engine.stop();
    EXPECT_NE(engine.getState(), MacroState::Running);
}

TEST_F(MacroEngineTest, ResetSetsIdleAndZeroStep) {
    EXPECT_CALL(mockConn, send(_)).WillRepeatedly(Return(0));

    auto recipe = makeSimpleRecipe();
    engine.start(recipe);
    engine.feed("login:");
    EXPECT_EQ(engine.currentStep(), 1);

    engine.reset();
    EXPECT_EQ(engine.getState(), MacroState::Idle);
    EXPECT_EQ(engine.currentStep(), 0);
}

TEST_F(MacroEngineTest, TotalStepsReturnsCorrectCount) {
    auto recipe = makeSimpleRecipe();
    engine.start(recipe);
    EXPECT_EQ(engine.totalSteps(), 3);
}

TEST_F(MacroEngineTest, MacroTriggeredEventPublished) {
    bool triggered = false;
    bus.subscribe<MacroTriggeredEvent>([&triggered](const MacroTriggeredEvent& e) {
        triggered = (e.sessionId == "session-1");
    });

    auto recipe = makeSimpleRecipe();
    engine.start(recipe);
    EXPECT_TRUE(triggered);
}

TEST_F(MacroEngineTest, MacroDoneEventPublishedWhenComplete) {
    EXPECT_CALL(mockConn, send(_)).WillRepeatedly(Return(0));

    bool done = false;
    bus.subscribe<MacroDoneEvent>([&done](const MacroDoneEvent& e) {
        done = e.success;
    });

    auto recipe = makeSimpleRecipe();
    engine.start(recipe);
    engine.feed("login:");
    engine.feed("Password:");
    engine.feed("#");

    EXPECT_TRUE(done);
}
