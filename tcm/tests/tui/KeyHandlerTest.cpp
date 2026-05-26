#include "tui/KeyHandler.hpp"
#include "core/EventBus.hpp"

#include <gtest/gtest.h>
#include <ncurses.h>
#include <vector>

using namespace tcm;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class KeyHandlerTest : public ::testing::Test {
protected:
    EventBus   bus;
    int        eventCount = 0;
    std::vector<KeyPressedEvent> receivedEvents;

    void SetUp() override {
        bus.subscribe<KeyPressedEvent>([this](const KeyPressedEvent& ev) {
            ++eventCount;
            receivedEvents.push_back(ev);
        });
    }
};

// ---------------------------------------------------------------------------
// 1. Byte 0x1D → publish KeyPressedEvent{isEscape=true}
// ---------------------------------------------------------------------------
TEST_F(KeyHandlerTest, CtrlBracketPublishesEscapeEvent) {
    KeyHandler kh(bus);
    uint8_t buf[] = {0x1D};
    kh.processRawInput(buf, 1);

    ASSERT_EQ(1, eventCount);
    EXPECT_EQ(0x1D, receivedEvents[0].key);
    EXPECT_TRUE(receivedEvents[0].isEscape);
}

// ---------------------------------------------------------------------------
// 2. Regular byte 'a' (0x61) → publish KeyPressedEvent{isEscape=false}
// ---------------------------------------------------------------------------
TEST_F(KeyHandlerTest, RegularBytePublishesNonEscapeEvent) {
    KeyHandler kh(bus);
    uint8_t buf[] = {0x61};  // 'a'
    kh.processRawInput(buf, 1);

    ASSERT_EQ(1, eventCount);
    EXPECT_EQ(0x61, receivedEvents[0].key);
    EXPECT_FALSE(receivedEvents[0].isEscape);
}

// ---------------------------------------------------------------------------
// 3. Multiple bytes: 'a', 0x1D, 'b' → 3 events published
// ---------------------------------------------------------------------------
TEST_F(KeyHandlerTest, MultipleBytesPublishMultipleEvents) {
    KeyHandler kh(bus);
    uint8_t buf[] = {'a', 0x1D, 'b'};
    kh.processRawInput(buf, 3);

    ASSERT_EQ(3, eventCount);

    EXPECT_EQ('a',  receivedEvents[0].key);
    EXPECT_FALSE(receivedEvents[0].isEscape);

    EXPECT_EQ(0x1D, receivedEvents[1].key);
    EXPECT_TRUE(receivedEvents[1].isEscape);

    EXPECT_EQ('b',  receivedEvents[2].key);
    EXPECT_FALSE(receivedEvents[2].isEscape);
}

// ---------------------------------------------------------------------------
// 4. processKey(KEY_UP) → publish KeyPressedEvent
// ---------------------------------------------------------------------------
TEST_F(KeyHandlerTest, ProcessKeyPublishesEvent) {
    KeyHandler kh(bus);
    kh.processKey(KEY_UP);  // ncurses KEY_UP

    ASSERT_EQ(1, eventCount);
    EXPECT_EQ(KEY_UP, receivedEvents[0].key);
    EXPECT_FALSE(receivedEvents[0].isEscape);
}

// ---------------------------------------------------------------------------
// 5. Empty input → no events
// ---------------------------------------------------------------------------
TEST_F(KeyHandlerTest, EmptyInputPublishesNoEvents) {
    KeyHandler kh(bus);
    kh.processRawInput(nullptr, 0);

    EXPECT_EQ(0, eventCount);
}
