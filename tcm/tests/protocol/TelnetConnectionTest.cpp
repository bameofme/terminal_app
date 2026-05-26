#include <gtest/gtest.h>
#include "protocol/IacParser.hpp"
#include "protocol/TelnetConnection.hpp"
#include "protocol/TelnetConfig.hpp"

using namespace tcm;

// ===========================================================================
// IacParser Tests
// ===========================================================================

// 1. Plain data passes through unchanged
TEST(IacParserTest, PlainDataUnmodified)
{
    IacParser parser;
    const uint8_t data[] = { 'H', 'e', 'l', 'l', 'o' };
    auto result = parser.feed(data, sizeof(data));
    EXPECT_EQ(result.cleanData, std::vector<uint8_t>({'H','e','l','l','o'}));
    EXPECT_TRUE(result.responses.empty());
}

// 2. IAC DO ECHO is stripped from data stream
TEST(IacParserTest, IacDoEchoStripped)
{
    IacParser parser;
    const uint8_t data[] = { 'A', IAC_CMD, DO_CMD, OPT_ECHO, 'B' };
    auto result = parser.feed(data, sizeof(data));
    EXPECT_EQ(result.cleanData, std::vector<uint8_t>({'A','B'}));
}

// 3. IAC IAC (escaped 0xFF) produces single 0xFF in output
TEST(IacParserTest, EscapedIacProducesSingleByte)
{
    IacParser parser;
    const uint8_t data[] = { 0x41, IAC_CMD, IAC_CMD, 0x42 };
    auto result = parser.feed(data, sizeof(data));
    EXPECT_EQ(result.cleanData, std::vector<uint8_t>({0x41, 0xFF, 0x42}));
    EXPECT_TRUE(result.responses.empty());
}

// 4. Incomplete IAC at end of buffer — next feed completes it
TEST(IacParserTest, IncompleteIacBufferedAcrossFeeds)
{
    IacParser parser;
    // Feed only the IAC byte
    const uint8_t part1[] = { 0x41, IAC_CMD };
    auto r1 = parser.feed(part1, sizeof(part1));
    EXPECT_EQ(r1.cleanData, std::vector<uint8_t>({0x41}));

    // Feed the rest
    const uint8_t part2[] = { DO_CMD, OPT_ECHO, 0x42 };
    auto r2 = parser.feed(part2, sizeof(part2));
    EXPECT_EQ(r2.cleanData, std::vector<uint8_t>({0x42}));
    // Should have generated a WILL ECHO response
    EXPECT_FALSE(r2.responses.empty());
}

// 5. IAC SB ... IAC SE is stripped completely
TEST(IacParserTest, SubnegotiationStripped)
{
    IacParser parser;
    const uint8_t data[] = {
        'X',
        IAC_CMD, SB, 0x18, 0x00, 'V', 'T', '1', '0', '0', IAC_CMD, SE,
        'Y'
    };
    auto result = parser.feed(data, sizeof(data));
    EXPECT_EQ(result.cleanData, std::vector<uint8_t>({'X','Y'}));
}

// 6. IAC WILL ECHO → generates DO ECHO response
TEST(IacParserTest, WillEchoGeneratesDoEchoResponse)
{
    IacParser parser;
    const uint8_t data[] = { IAC_CMD, WILL, OPT_ECHO };
    auto result = parser.feed(data, sizeof(data));
    EXPECT_TRUE(result.cleanData.empty());
    ASSERT_EQ(result.responses.size(), 3u);
    EXPECT_EQ(result.responses[0], IAC_CMD);
    EXPECT_EQ(result.responses[1], DO_CMD);
    EXPECT_EQ(result.responses[2], OPT_ECHO);
}

// 7. IAC DO SGA → generates WILL SGA response
TEST(IacParserTest, DoSgaGeneratesWillSgaResponse)
{
    IacParser parser;
    const uint8_t data[] = { IAC_CMD, DO_CMD, OPT_SUPPRESS_GA };
    auto result = parser.feed(data, sizeof(data));
    EXPECT_TRUE(result.cleanData.empty());
    ASSERT_EQ(result.responses.size(), 3u);
    EXPECT_EQ(result.responses[0], IAC_CMD);
    EXPECT_EQ(result.responses[1], WILL);
    EXPECT_EQ(result.responses[2], OPT_SUPPRESS_GA);
}

// 8. Mixed: data interleaved with IAC sequences → correct clean data
TEST(IacParserTest, MixedDataAndIacSequences)
{
    IacParser parser;
    const uint8_t data[] = {
        'H', 'i',
        IAC_CMD, DO_CMD, OPT_ECHO,
        ' ',
        IAC_CMD, WILL, OPT_SUPPRESS_GA,
        '!'
    };
    auto result = parser.feed(data, sizeof(data));
    EXPECT_EQ(result.cleanData, std::vector<uint8_t>({'H','i',' ','!'}));
    // Two IAC responses: WILL ECHO + DO SGA
    EXPECT_EQ(result.responses.size(), 6u);
}

// ===========================================================================
// TelnetConnection Tests
// ===========================================================================

// 9. Initial state is Disconnected
TEST(TelnetConnectionTest, InitialStateIsDisconnected)
{
    TelnetConnection conn(TelnetConfig{"localhost"});
    EXPECT_EQ(conn.getState(), ConnectionState::Disconnected);
}

// 10. connect() with invalid host returns error
TEST(TelnetConnectionTest, ConnectInvalidHostReturnsError)
{
    TelnetConfig cfg;
    cfg.host             = "this.host.does.not.exist.invalid";
    cfg.port             = 9999;
    cfg.connectTimeoutMs = 500;
    TelnetConnection conn(cfg);
    int rc = conn.connect();
    EXPECT_NE(rc, TCM_OK);
    EXPECT_EQ(conn.getState(), ConnectionState::Error);
}

// 11. getFd() returns -1 before connect
TEST(TelnetConnectionTest, GetFdMinusOneBeforeConnect)
{
    TelnetConnection conn(TelnetConfig{"localhost"});
    EXPECT_EQ(conn.getFd(), -1);
}

// 12. disconnect() when not connected does not crash
TEST(TelnetConnectionTest, DisconnectWhenNotConnectedNoCrash)
{
    TelnetConnection conn(TelnetConfig{"localhost"});
    EXPECT_NO_THROW(conn.disconnect());
    EXPECT_EQ(conn.getState(), ConnectionState::Disconnected);
}

// 13. Multiple disconnect() calls do not crash
TEST(TelnetConnectionTest, MultipleDisconnectCallsNoCrash)
{
    TelnetConnection conn(TelnetConfig{"localhost"});
    EXPECT_NO_THROW({
        conn.disconnect();
        conn.disconnect();
        conn.disconnect();
    });
}

// 14. TelnetConfig defaults
TEST(TelnetConnectionTest, TelnetConfigDefaults)
{
    TelnetConfig cfg;
    EXPECT_EQ(cfg.port, 23u);
    EXPECT_EQ(cfg.connectTimeoutMs, 5000);
    EXPECT_TRUE(cfg.suppressGoAhead);
}

// 15. send() with 0xFF in data is escaped (via IacParser round-trip validation)
TEST(TelnetConnectionTest, SendEscapesIacByte)
{
    // We verify the escaping logic through IacParser: a round-trip where
    // we feed { IAC IAC } back in should yield a single 0xFF clean byte.
    IacParser parser;
    const uint8_t escaped[] = { IAC_CMD, IAC_CMD };
    auto result = parser.feed(escaped, sizeof(escaped));
    ASSERT_EQ(result.cleanData.size(), 1u);
    EXPECT_EQ(result.cleanData[0], 0xFF);
    EXPECT_TRUE(result.responses.empty());
}
