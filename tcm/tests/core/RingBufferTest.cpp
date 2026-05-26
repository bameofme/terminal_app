#include <gtest/gtest.h>
#include "RingBuffer.hpp"

#include <vector>
#include <numeric>

using namespace tcm;

// 1. Basic write + read round-trip
TEST(RingBufferTest, WriteReadRoundTrip) {
    RingBuffer rb(16);
    const uint8_t data[] = {1, 2, 3, 4, 5};
    rb.write(data, 5);

    uint8_t out[5] = {};
    size_t n = rb.read(out, 5);
    EXPECT_EQ(n, 5u);
    EXPECT_EQ(std::vector<uint8_t>(out, out + 5),
              std::vector<uint8_t>(data, data + 5));
}

// 2. Overwrite oldest when full (newest wins)
TEST(RingBufferTest, OverwriteOldestWhenFull) {
    RingBuffer rb(4);
    // Write 4 bytes to fill
    const uint8_t first[] = {1, 2, 3, 4};
    rb.write(first, 4);
    // Write 2 more → overwrites oldest 2
    const uint8_t second[] = {5, 6};
    rb.write(second, 2);

    uint8_t out[4] = {};
    size_t n = rb.read(out, 4);
    EXPECT_EQ(n, 4u);
    // Expected: {3, 4, 5, 6}
    EXPECT_EQ(out[0], 3u);
    EXPECT_EQ(out[1], 4u);
    EXPECT_EQ(out[2], 5u);
    EXPECT_EQ(out[3], 6u);
}

// 3. available() returns correct count after write
TEST(RingBufferTest, AvailableAfterWrite) {
    RingBuffer rb(16);
    EXPECT_EQ(rb.available(), 0u);
    const uint8_t d[] = {10, 20, 30};
    rb.write(d, 3);
    EXPECT_EQ(rb.available(), 3u);
}

// 4. available() decreases after read
TEST(RingBufferTest, AvailableDecreasesAfterRead) {
    RingBuffer rb(16);
    const uint8_t d[] = {1, 2, 3, 4};
    rb.write(d, 4);
    uint8_t out[2];
    rb.read(out, 2);
    EXPECT_EQ(rb.available(), 2u);
}

// 5. clear() resets buffer to empty
TEST(RingBufferTest, ClearResetsBuffer) {
    RingBuffer rb(16);
    const uint8_t d[] = {1, 2, 3};
    rb.write(d, 3);
    rb.clear();
    EXPECT_TRUE(rb.empty());
    EXPECT_EQ(rb.available(), 0u);
}

// 6. capacity() returns correct value
TEST(RingBufferTest, CapacityReturnsCorrectValue) {
    RingBuffer rb(16);
    EXPECT_EQ(rb.capacity(), 16u);
    RingBuffer rb2(8);
    EXPECT_EQ(rb2.capacity(), 8u);
}

// 7. empty() behavior
TEST(RingBufferTest, EmptyFlagBehavior) {
    RingBuffer rb(16);
    EXPECT_TRUE(rb.empty());
    const uint8_t d[] = {42};
    rb.write(d, 1);
    EXPECT_FALSE(rb.empty());
    uint8_t out[1];
    rb.read(out, 1);
    EXPECT_TRUE(rb.empty());
}

// 8. read() from empty buffer returns 0
TEST(RingBufferTest, ReadFromEmptyReturnsZero) {
    RingBuffer rb(16);
    uint8_t out[4];
    EXPECT_EQ(rb.read(out, 4), 0u);
}

// 9. Write full capacity bytes then read back
TEST(RingBufferTest, WriteFullCapacityThenRead) {
    constexpr size_t CAP = 8;
    RingBuffer rb(CAP);
    uint8_t data[CAP];
    for (size_t i = 0; i < CAP; ++i) data[i] = static_cast<uint8_t>(i + 1);
    rb.write(data, CAP);
    EXPECT_EQ(rb.available(), CAP);

    uint8_t out[CAP] = {};
    size_t n = rb.read(out, CAP);
    EXPECT_EQ(n, CAP);
    for (size_t i = 0; i < CAP; ++i) EXPECT_EQ(out[i], data[i]);
}

// 10. Write more than capacity → only newest bytes remain
TEST(RingBufferTest, WriteExceedsCapacityKeepsNewest) {
    RingBuffer rb(4);
    const uint8_t data[] = {1, 2, 3, 4, 5, 6};
    rb.write(data, 6);

    // After writing 6 bytes into capacity-4 buffer, only {3,4,5,6} remain
    uint8_t out[4] = {};
    size_t n = rb.read(out, 4);
    EXPECT_EQ(n, 4u);
    EXPECT_EQ(out[0], 3u);
    EXPECT_EQ(out[1], 4u);
    EXPECT_EQ(out[2], 5u);
    EXPECT_EQ(out[3], 6u);
}

// 11. Boundary: write 1 byte, read 1 byte, repeat many times
TEST(RingBufferTest, OneByteBoundaryRepeat) {
    RingBuffer rb(4);
    for (uint8_t i = 0; i < 100; ++i) {
        rb.write(&i, 1);
        uint8_t out = 0;
        EXPECT_EQ(rb.read(&out, 1), 1u);
        EXPECT_EQ(out, i);
    }
    EXPECT_TRUE(rb.empty());
}

// 12. peek() does not consume data
TEST(RingBufferTest, PeekDoesNotConsumeData) {
    RingBuffer rb(16);
    const uint8_t data[] = {10, 20, 30};
    rb.write(data, 3);

    uint8_t peeked[3] = {};
    size_t n = rb.peek(peeked, 3);
    EXPECT_EQ(n, 3u);
    EXPECT_EQ(rb.available(), 3u);  // unchanged
    EXPECT_EQ(peeked[0], 10u);
    EXPECT_EQ(peeked[1], 20u);
    EXPECT_EQ(peeked[2], 30u);

    // Can still read the same data
    uint8_t out[3] = {};
    rb.read(out, 3);
    EXPECT_EQ(std::vector<uint8_t>(out, out + 3),
              std::vector<uint8_t>(peeked, peeked + 3));
}

// 13. Write wraps around (fill, read half, write again)
TEST(RingBufferTest, WriteWrapsAround) {
    RingBuffer rb(8);
    // Fill completely
    const uint8_t fill[] = {1, 2, 3, 4, 5, 6, 7, 8};
    rb.write(fill, 8);
    // Read half (advance head into middle)
    uint8_t discard[4];
    rb.read(discard, 4);
    EXPECT_EQ(rb.available(), 4u);

    // Write 4 more bytes — tail wraps
    const uint8_t more[] = {9, 10, 11, 12};
    rb.write(more, 4);
    EXPECT_EQ(rb.available(), 8u);

    uint8_t out[8] = {};
    rb.read(out, 8);
    const uint8_t expected[] = {5, 6, 7, 8, 9, 10, 11, 12};
    for (size_t i = 0; i < 8; ++i) EXPECT_EQ(out[i], expected[i]);
}

// 14. Write data larger than capacity → buffer contains newest bytes
TEST(RingBufferTest, WriteLargerThanCapacityKeepsNewestBytes) {
    constexpr size_t CAP = 4;
    RingBuffer rb(CAP);

    // Write 10 bytes, only last 4 should remain
    const uint8_t data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    size_t written = rb.write(data, 10);
    EXPECT_EQ(written, CAP);  // clamped to capacity
    EXPECT_EQ(rb.available(), CAP);

    uint8_t out[CAP] = {};
    rb.read(out, CAP);
    EXPECT_EQ(out[0], 7u);
    EXPECT_EQ(out[1], 8u);
    EXPECT_EQ(out[2], 9u);
    EXPECT_EQ(out[3], 10u);
}

// Bonus: non-power-of-2 capacity rounds up
TEST(RingBufferTest, NonPowerOf2CapacityRoundsUp) {
    RingBuffer rb(5);
    EXPECT_EQ(rb.capacity(), 8u);  // next power of 2 after 5
}
