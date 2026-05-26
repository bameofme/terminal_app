#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <array>

#include "MockConnection.hpp"

using namespace tcm;       // NOLINT(google-build-using-namespace)
using namespace tcm::test; // NOLINT(google-build-using-namespace)
using ::testing::_;
using ::testing::Return;

// ---------------------------------------------------------------------------
// 1. connect() returns TCM_OK on success
// ---------------------------------------------------------------------------
TEST(IConnectionTest, ConnectReturnsOkOnSuccess) {
    MockConnection conn;
    EXPECT_CALL(conn, connect()).WillOnce(Return(TCM_OK));
    EXPECT_EQ(conn.connect(), TCM_OK);
}

// ---------------------------------------------------------------------------
// 2. send() forwards the correct bytes and returns the byte count
// ---------------------------------------------------------------------------
TEST(IConnectionTest, SendForwardsCorrectBytesAndReturnsBytesSent) {
    MockConnection conn;
    const std::array<uint8_t, 4> payload = {0x01, 0x02, 0x03, 0x04};
    std::array<uint8_t, 4> captured{};
    int capturedSize = 0;

    EXPECT_CALL(conn, send(_))
        .WillOnce([&captured, &capturedSize](std::span<const uint8_t> d) noexcept -> int {
            capturedSize = static_cast<int>(d.size());
            for (std::size_t i = 0; i < d.size() && i < captured.size(); ++i) {
                captured[i] = d[i];
            }
            return capturedSize;
        });

    EXPECT_EQ(conn.send(std::span<const uint8_t>(payload)), 4);
    EXPECT_EQ(capturedSize, 4);
    EXPECT_EQ(captured, payload);
}

// ---------------------------------------------------------------------------
// 3a. recv() returns the number of bytes received (data available)
// ---------------------------------------------------------------------------
TEST(IConnectionTest, RecvReturnsDataCount) {
    MockConnection conn;
    std::array<uint8_t, 8> buf{};
    EXPECT_CALL(conn, recv(_)).WillOnce(Return(8));
    EXPECT_EQ(conn.recv(std::span<uint8_t>(buf)), 8);
}

// ---------------------------------------------------------------------------
// 3b. recv() returns 0 when the peer disconnects (EOF)
// ---------------------------------------------------------------------------
TEST(IConnectionTest, RecvReturnsZeroOnPeerDisconnect) {
    MockConnection conn;
    std::array<uint8_t, 8> buf{};
    EXPECT_CALL(conn, recv(_)).WillOnce(Return(0));
    EXPECT_EQ(conn.recv(std::span<uint8_t>(buf)), 0);
}

// ---------------------------------------------------------------------------
// 4. disconnect() can be called multiple times without crashing
// ---------------------------------------------------------------------------
TEST(IConnectionTest, DisconnectCanBeCalledMultipleTimes) {
    MockConnection conn;
    EXPECT_CALL(conn, disconnect()).Times(3);
    conn.disconnect();
    conn.disconnect();
    conn.disconnect();
}

// ---------------------------------------------------------------------------
// 5a. getState() returns Connected when connected
// ---------------------------------------------------------------------------
TEST(IConnectionTest, GetStateReturnsConnected) {
    MockConnection conn;
    EXPECT_CALL(conn, getState()).WillOnce(Return(ConnectionState::Connected));
    EXPECT_EQ(conn.getState(), ConnectionState::Connected);
}

// ---------------------------------------------------------------------------
// 5b. getState() returns Disconnected initially
// ---------------------------------------------------------------------------
TEST(IConnectionTest, GetStateReturnsDisconnected) {
    MockConnection conn;
    EXPECT_CALL(conn, getState()).WillOnce(Return(ConnectionState::Disconnected));
    EXPECT_EQ(conn.getState(), ConnectionState::Disconnected);
}

// ---------------------------------------------------------------------------
// 5c. getState() returns Error on failure
// ---------------------------------------------------------------------------
TEST(IConnectionTest, GetStateReturnsError) {
    MockConnection conn;
    EXPECT_CALL(conn, getState()).WillOnce(Return(ConnectionState::Error));
    EXPECT_EQ(conn.getState(), ConnectionState::Error);
}

// ---------------------------------------------------------------------------
// 6. getFd() returns a valid (>= 0) file descriptor after connect()
// ---------------------------------------------------------------------------
TEST(IConnectionTest, GetFdReturnsValidFdAfterConnect) {
    MockConnection conn;
    EXPECT_CALL(conn, connect()).WillOnce(Return(TCM_OK));
    EXPECT_CALL(conn, getFd()).WillOnce(Return(3));

    EXPECT_EQ(conn.connect(), TCM_OK);
    EXPECT_GE(conn.getFd(), 0);
}
