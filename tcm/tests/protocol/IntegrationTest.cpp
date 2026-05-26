/**
 * Integration Test: Protocol + SessionManager
 *
 * Tests:
 *   1. ConnectionFactory + SessionManager: Factory creates connection, add to SessionManager
 *   2. SessionConfig JSON round-trip: SessionConfig → toJson() → fromJson() → verify
 *   3. Multiple sessions: Add 3 sessions (SSH, Serial, Telnet), count=3, remove all
 */

#include <gtest/gtest.h>

#include "core/EventBus.hpp"
#include "core/Session.hpp"
#include "core/SessionManager.hpp"
#include "protocol/ConnectionFactory.hpp"
#include "protocol/SessionConfig.hpp"
#include "protocol/SerialConfig.hpp"
#include "protocol/SshConfig.hpp"
#include "protocol/TelnetConfig.hpp"

using namespace tcm;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static SessionConfig makeSshCfg(const std::string& name = "ssh-session") {
    SessionConfig cfg;
    cfg.name = name;
    cfg.type = SessionType::SSH;
    SshConfig ssh;
    ssh.host     = "192.168.1.100";
    ssh.port     = 22;
    ssh.username = "admin";
    ssh.password = "secret";
    cfg.ssh = ssh;
    return cfg;
}

static SessionConfig makeSerialCfg(const std::string& name = "serial-session") {
    SessionConfig cfg;
    cfg.name = name;
    cfg.type = SessionType::Serial;
    SerialConfig serial;
    serial.device   = "/dev/ttyUSB0";
    serial.baudRate = 9600;
    cfg.serial = serial;
    return cfg;
}

static SessionConfig makeTelnetCfg(const std::string& name = "telnet-session") {
    SessionConfig cfg;
    cfg.name = name;
    cfg.type = SessionType::Telnet;
    TelnetConfig telnet;
    telnet.host = "10.0.0.1";
    telnet.port = 23;
    cfg.telnet = telnet;
    return cfg;
}

// ---------------------------------------------------------------------------
// Test 1: ConnectionFactory creates connection and can be added to SessionManager
// ---------------------------------------------------------------------------
TEST(ProtocolIntegration, ConnectionFactoryAndSessionManager) {
    EventBus       bus;
    SessionManager mgr(bus);

    auto conn = ConnectionFactory::create(makeSshCfg());
    ASSERT_NE(conn, nullptr);

    Session s;
    s.id   = generateUUID();
    s.name = "factory-session";
    s.type = SessionType::SSH;
    s.conn = std::move(conn);

    auto id = mgr.add(std::move(s));
    EXPECT_FALSE(id.empty());
    EXPECT_EQ(mgr.count(), 1u);

    mgr.remove(id);
    EXPECT_EQ(mgr.count(), 0u);
}

// ---------------------------------------------------------------------------
// Test 2: SessionConfig JSON round-trip
// ---------------------------------------------------------------------------
TEST(ProtocolIntegration, SessionConfigJsonRoundTrip) {
    SessionConfig original = makeSshCfg("my-ssh");
    original.ssh->port     = 2222;
    original.ssh->username = "roundtrip-user";

    auto j          = original.toJson();
    auto restored   = SessionConfig::fromJson(j);

    EXPECT_EQ(restored.name, "my-ssh");
    EXPECT_EQ(restored.type, SessionType::SSH);
    ASSERT_TRUE(restored.ssh.has_value());
    EXPECT_EQ(restored.ssh->host,     "192.168.1.100");
    EXPECT_EQ(restored.ssh->port,     2222);
    EXPECT_EQ(restored.ssh->username, "roundtrip-user");
}

// ---------------------------------------------------------------------------
// Test 3: Multiple sessions — add SSH, Serial, Telnet; count=3; remove all
// ---------------------------------------------------------------------------
TEST(ProtocolIntegration, MultipleSessions) {
    EventBus       bus;
    SessionManager mgr(bus);

    // Add SSH session
    Session s1;
    s1.id   = generateUUID();
    s1.name = "ssh";
    s1.type = SessionType::SSH;
    s1.conn = ConnectionFactory::create(makeSshCfg());
    auto id1 = mgr.add(std::move(s1));

    // Add Serial session
    Session s2;
    s2.id   = generateUUID();
    s2.name = "serial";
    s2.type = SessionType::Serial;
    s2.conn = ConnectionFactory::create(makeSerialCfg());
    auto id2 = mgr.add(std::move(s2));

    // Add Telnet session
    Session s3;
    s3.id   = generateUUID();
    s3.name = "telnet";
    s3.type = SessionType::Telnet;
    s3.conn = ConnectionFactory::create(makeTelnetCfg());
    auto id3 = mgr.add(std::move(s3));

    EXPECT_EQ(mgr.count(), 3u);

    // Remove all
    EXPECT_TRUE(mgr.remove(id1));
    EXPECT_TRUE(mgr.remove(id2));
    EXPECT_TRUE(mgr.remove(id3));

    EXPECT_EQ(mgr.count(), 0u);
}
