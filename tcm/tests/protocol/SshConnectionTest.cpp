#include <gtest/gtest.h>

#include "protocol/SshConfig.hpp"
#include "protocol/SshConnection.hpp"

namespace tcm {

// ---------------------------------------------------------------------------
// SshConfig — default values
// ---------------------------------------------------------------------------

TEST(SshConfigTest, DefaultPort)
{
    SshConfig cfg;
    EXPECT_EQ(cfg.port, 22u);
}

TEST(SshConfigTest, DefaultConnectTimeout)
{
    SshConfig cfg;
    EXPECT_EQ(cfg.connectTimeoutMs, 10000);
}

TEST(SshConfigTest, DefaultKeepalive)
{
    SshConfig cfg;
    EXPECT_EQ(cfg.keepAliveIntervalS, 30);
}

TEST(SshConfigTest, DefaultStrictHostCheckingIsFalse)
{
    SshConfig cfg;
    EXPECT_FALSE(cfg.strictHostChecking);
}

TEST(SshConfigTest, DefaultTunnelIsEmpty)
{
    SshConfig cfg;
    EXPECT_FALSE(cfg.tunnel.has_value());
}

TEST(SshConfigTest, TunnelDefaultLocalHost)
{
    SshConfig::Tunnel t;
    EXPECT_EQ(t.localHost, "127.0.0.1");
    EXPECT_EQ(t.localPort, 0u);
    EXPECT_EQ(t.remotePort, 0u);
}

// ---------------------------------------------------------------------------
// SshConnection — state management & lifecycle
// ---------------------------------------------------------------------------

TEST(SshConnectionTest, InitialStateIsDisconnected)
{
    SshConfig cfg;
    cfg.host = "127.0.0.1";
    SshConnection conn(cfg);
    EXPECT_EQ(conn.getState(), ConnectionState::Disconnected);
}

TEST(SshConnectionTest, GetFdBeforeConnectReturnsMinusOne)
{
    SshConfig cfg;
    cfg.host = "127.0.0.1";
    SshConnection conn(cfg);
    EXPECT_EQ(conn.getFd(), -1);
}

TEST(SshConnectionTest, ConstructAndDestroyDoesNotCrash)
{
    SshConfig cfg;
    cfg.host = "127.0.0.1";
    {
        SshConnection conn(cfg);
        (void)conn;
    }
}

TEST(SshConnectionTest, DisconnectWhenNotConnectedDoesNotCrash)
{
    SshConfig cfg;
    cfg.host = "127.0.0.1";
    SshConnection conn(cfg);
    conn.disconnect();
    EXPECT_EQ(conn.getState(), ConnectionState::Disconnected);
}

TEST(SshConnectionTest, MultipleDisconnectsDoNotCrash)
{
    SshConfig cfg;
    cfg.host = "127.0.0.1";
    SshConnection conn(cfg);
    conn.disconnect();
    conn.disconnect();
    conn.disconnect();
    EXPECT_EQ(conn.getState(), ConnectionState::Disconnected);
}

// ---------------------------------------------------------------------------
// SshConnection — error paths
// ---------------------------------------------------------------------------

TEST(SshConnectionTest, ConnectInvalidHostReturnsError)
{
    SshConfig cfg;
    cfg.host             = "this.host.does.not.exist.invalid";
    cfg.port             = 22;
    cfg.connectTimeoutMs = 1000;
    SshConnection conn(cfg);
    EXPECT_LT(conn.connect(), 0);
}

TEST(SshConnectionTest, ConnectRefusedPortReturnsError)
{
    SshConfig cfg;
    cfg.host             = "127.0.0.1";
    cfg.port             = 1;   // nothing listens here → ECONNREFUSED
    cfg.connectTimeoutMs = 2000;
    SshConnection conn(cfg);
    EXPECT_LT(conn.connect(), 0);
}

TEST(SshConnectionTest, StateIsNotConnectedAfterFailedConnect)
{
    SshConfig cfg;
    cfg.host             = "127.0.0.1";
    cfg.port             = 1;
    cfg.connectTimeoutMs = 2000;
    SshConnection conn(cfg);
    (void)conn.connect();
    EXPECT_NE(conn.getState(), ConnectionState::Connected);
}

TEST(SshConnectionTest, FdIsMinusOneAfterFailedConnect)
{
    SshConfig cfg;
    cfg.host             = "127.0.0.1";
    cfg.port             = 1;
    cfg.connectTimeoutMs = 2000;
    SshConnection conn(cfg);
    (void)conn.connect();
    EXPECT_EQ(conn.getFd(), -1);
}

TEST(SshConnectionTest, DisconnectAfterFailedConnectDoesNotCrash)
{
    SshConfig cfg;
    cfg.host             = "127.0.0.1";
    cfg.port             = 1;
    cfg.connectTimeoutMs = 2000;
    SshConnection conn(cfg);
    (void)conn.connect();
    conn.disconnect();
    EXPECT_EQ(conn.getState(), ConnectionState::Disconnected);
}

} // namespace tcm
