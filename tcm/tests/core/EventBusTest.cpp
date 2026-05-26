#include <gtest/gtest.h>

#include "core/EventBus.hpp"

using namespace tcm;  // NOLINT(google-build-using-namespace)

// ---------------------------------------------------------------------------
// 1. subscribe<E>() receives correct event type
// ---------------------------------------------------------------------------
TEST(EventBusTest, SubscribeReceivesCorrectEventType) {
    EventBus bus;
    std::string received;
    bus.subscribe<SessionAddedEvent>(
        [&](const SessionAddedEvent& e) { received = e.sessionId; });
    bus.publish(SessionAddedEvent{"sess-1"});
    EXPECT_EQ(received, "sess-1");
}

// ---------------------------------------------------------------------------
// 2. publish<E>() calls all subscribed handlers
// ---------------------------------------------------------------------------
TEST(EventBusTest, PublishCallsSubscribedHandler) {
    EventBus bus;
    int callCount = 0;
    bus.subscribe<SessionRemovedEvent>(
        [&](const SessionRemovedEvent&) { ++callCount; });
    bus.publish(SessionRemovedEvent{"id"});
    EXPECT_EQ(callCount, 1);
}

// ---------------------------------------------------------------------------
// 3. Multiple subscribers to same event type — all are called
// ---------------------------------------------------------------------------
TEST(EventBusTest, MultipleSubscribersSameEventAllCalled) {
    EventBus bus;
    int count1 = 0, count2 = 0;
    bus.subscribe<SessionSwitchedEvent>(
        [&](const SessionSwitchedEvent&) { ++count1; });
    bus.subscribe<SessionSwitchedEvent>(
        [&](const SessionSwitchedEvent&) { ++count2; });
    bus.publish(SessionSwitchedEvent{"a", "b"});
    EXPECT_EQ(count1, 1);
    EXPECT_EQ(count2, 1);
}

// ---------------------------------------------------------------------------
// 4. unsubscribe(token) — handler is no longer called
// ---------------------------------------------------------------------------
TEST(EventBusTest, UnsubscribeStopsHandler) {
    EventBus bus;
    int callCount = 0;
    EventBus::Token tok = bus.subscribe<SessionAddedEvent>(
        [&](const SessionAddedEvent&) { ++callCount; });
    bus.publish(SessionAddedEvent{"s1"});
    EXPECT_EQ(callCount, 1);
    bus.unsubscribe(tok);
    bus.publish(SessionAddedEvent{"s2"});
    EXPECT_EQ(callCount, 1);  // must still be 1
}

// ---------------------------------------------------------------------------
// 5. publish with no subscriber — no crash
// ---------------------------------------------------------------------------
TEST(EventBusTest, PublishWithNoSubscriberNoCrash) {
    EventBus bus;
    EXPECT_NO_THROW(bus.publish(SessionRemovedEvent{"orphan"}));
}

// ---------------------------------------------------------------------------
// 6. Different event types — only correct subscriber is called
// ---------------------------------------------------------------------------
TEST(EventBusTest, DifferentEventTypesOnlyCorrectSubscriberCalled) {
    EventBus bus;
    int addedCount = 0, removedCount = 0;
    bus.subscribe<SessionAddedEvent>(
        [&](const SessionAddedEvent&) { ++addedCount; });
    bus.subscribe<SessionRemovedEvent>(
        [&](const SessionRemovedEvent&) { ++removedCount; });
    bus.publish(SessionAddedEvent{"x"});
    EXPECT_EQ(addedCount, 1);
    EXPECT_EQ(removedCount, 0);
}

// ---------------------------------------------------------------------------
// 7. unsubscribe non-existent token — no crash
// ---------------------------------------------------------------------------
TEST(EventBusTest, UnsubscribeNonExistentTokenNoCrash) {
    EventBus bus;
    EXPECT_NO_THROW(bus.unsubscribe(99999));
}

// ---------------------------------------------------------------------------
// 8. SessionAddedEvent — subscriber receives correct sessionId
// ---------------------------------------------------------------------------
TEST(EventBusTest, SessionAddedEventCorrectSessionId) {
    EventBus bus;
    std::string got;
    bus.subscribe<SessionAddedEvent>(
        [&](const SessionAddedEvent& e) { got = e.sessionId; });
    bus.publish(SessionAddedEvent{"my-session-id"});
    EXPECT_EQ(got, "my-session-id");
}

// ---------------------------------------------------------------------------
// 9. DataReceivedEvent — subscriber receives correct data bytes
// ---------------------------------------------------------------------------
TEST(EventBusTest, DataReceivedEventCorrectData) {
    EventBus bus;
    std::vector<uint8_t> received;
    bus.subscribe<DataReceivedEvent>(
        [&](const DataReceivedEvent& e) { received = e.data; });
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03};
    bus.publish(DataReceivedEvent{"sess", payload});
    EXPECT_EQ(received, payload);
}

// ---------------------------------------------------------------------------
// 10. subscribe/publish/unsubscribe in complex order — no crash
// ---------------------------------------------------------------------------
TEST(EventBusTest, ComplexOrderNoCrash) {
    EventBus bus;
    int count = 0;
    auto t1 = bus.subscribe<KeyPressedEvent>(
        [&](const KeyPressedEvent&) { ++count; });
    auto t2 = bus.subscribe<KeyPressedEvent>(
        [&](const KeyPressedEvent&) { ++count; });
    bus.publish(KeyPressedEvent{10});
    EXPECT_EQ(count, 2);
    bus.unsubscribe(t1);
    bus.publish(KeyPressedEvent{11});
    EXPECT_EQ(count, 3);
    bus.unsubscribe(t2);
    bus.publish(KeyPressedEvent{12});
    EXPECT_EQ(count, 3);
    bus.unsubscribe(t1);  // double-unsubscribe — must not crash
    EXPECT_NO_THROW(bus.publish(MacroDoneEvent{"s", true}));
}
