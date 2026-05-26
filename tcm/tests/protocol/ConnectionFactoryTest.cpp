#include <gtest/gtest.h>

#include "protocol/ConnectionFactory.hpp"
#include "protocol/SerialConnection.hpp"
#include "protocol/SshConnection.hpp"
#include "protocol/TelnetConnection.hpp"

namespace tcm {

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------
static SessionConfig makeSshConfig() {
    SessionConfig cfg;
    cfg.name = "test-ssh";
    cfg.type = SessionType::SSH;
    SshConfig ssh;
    ssh.host     = "localhost";
    ssh.port     = 22;
    ssh.username = "user";
    ssh.password = "pass";
    cfg.ssh = ssh;
    return cfg;
}

static SessionConfig makeSerialConfig() {
    SessionConfig cfg;
    cfg.name = "test-serial";
    cfg.type = SessionType::Serial;
    SerialConfig serial;
    serial.device   = "/dev/ttyUSB0";
    serial.baudRate = 115200;
    cfg.serial = serial;
    return cfg;
}

static SessionConfig makeTelnetConfig() {
    SessionConfig cfg;
    cfg.name = "test-telnet";
    cfg.type = SessionType::Telnet;
    TelnetConfig telnet;
    telnet.host = "192.168.1.1";
    telnet.port = 23;
    cfg.telnet = telnet;
    return cfg;
}

// ---------------------------------------------------------------------------
// 1. Factory tạo SshConnection
// ---------------------------------------------------------------------------
TEST(ConnectionFactoryTest, CreatesSshConnection) {
    auto conn = ConnectionFactory::create(makeSshConfig());
    EXPECT_NE(nullptr, dynamic_cast<SshConnection*>(conn.get()));
}

// ---------------------------------------------------------------------------
// 2. Factory tạo SerialConnection
// ---------------------------------------------------------------------------
TEST(ConnectionFactoryTest, CreatesSerialConnection) {
    auto conn = ConnectionFactory::create(makeSerialConfig());
    EXPECT_NE(nullptr, dynamic_cast<SerialConnection*>(conn.get()));
}

// ---------------------------------------------------------------------------
// 3. Factory tạo TelnetConnection
// ---------------------------------------------------------------------------
TEST(ConnectionFactoryTest, CreatesTelnetConnection) {
    auto conn = ConnectionFactory::create(makeTelnetConfig());
    EXPECT_NE(nullptr, dynamic_cast<TelnetConnection*>(conn.get()));
}

// ---------------------------------------------------------------------------
// 4. Throw khi optional không được set
// ---------------------------------------------------------------------------
TEST(ConnectionFactoryTest, ThrowsWhenSshConfigMissing) {
    SessionConfig cfg;
    cfg.type = SessionType::SSH;
    // cfg.ssh not set
    EXPECT_THROW(ConnectionFactory::create(cfg), std::invalid_argument);
}

// ---------------------------------------------------------------------------
// 5. Round-trip SSH
// ---------------------------------------------------------------------------
TEST(ConnectionFactoryTest, SessionConfigRoundTripSsh) {
    auto orig = makeSshConfig();
    auto back = SessionConfig::fromJson(orig.toJson());

    EXPECT_EQ(orig.name, back.name);
    EXPECT_EQ(orig.type, back.type);
    ASSERT_TRUE(back.ssh.has_value());
    EXPECT_EQ(orig.ssh->host,     back.ssh->host);
    EXPECT_EQ(orig.ssh->port,     back.ssh->port);
    EXPECT_EQ(orig.ssh->username, back.ssh->username);
    EXPECT_EQ(orig.ssh->password, back.ssh->password);
}

// ---------------------------------------------------------------------------
// 6. Round-trip Serial
// ---------------------------------------------------------------------------
TEST(ConnectionFactoryTest, SessionConfigRoundTripSerial) {
    auto orig = makeSerialConfig();
    auto back = SessionConfig::fromJson(orig.toJson());

    EXPECT_EQ(orig.name, back.name);
    EXPECT_EQ(orig.type, back.type);
    ASSERT_TRUE(back.serial.has_value());
    EXPECT_EQ(orig.serial->device,      back.serial->device);
    EXPECT_EQ(orig.serial->baudRate,    back.serial->baudRate);
    EXPECT_EQ(orig.serial->parity,      back.serial->parity);
    EXPECT_EQ(orig.serial->stopBits,    back.serial->stopBits);
    EXPECT_EQ(orig.serial->flowControl, back.serial->flowControl);
}

// ---------------------------------------------------------------------------
// 7. Round-trip Telnet
// ---------------------------------------------------------------------------
TEST(ConnectionFactoryTest, SessionConfigRoundTripTelnet) {
    auto orig = makeTelnetConfig();
    auto back = SessionConfig::fromJson(orig.toJson());

    EXPECT_EQ(orig.name, back.name);
    EXPECT_EQ(orig.type, back.type);
    ASSERT_TRUE(back.telnet.has_value());
    EXPECT_EQ(orig.telnet->host,             back.telnet->host);
    EXPECT_EQ(orig.telnet->port,             back.telnet->port);
    EXPECT_EQ(orig.telnet->connectTimeoutMs, back.telnet->connectTimeoutMs);
    EXPECT_EQ(orig.telnet->suppressGoAhead,  back.telnet->suppressGoAhead);
}

// ---------------------------------------------------------------------------
// 8. Connection không null
// ---------------------------------------------------------------------------
TEST(ConnectionFactoryTest, CreatedConnectionIsNotNull) {
    EXPECT_NE(nullptr, ConnectionFactory::create(makeSshConfig()));
    EXPECT_NE(nullptr, ConnectionFactory::create(makeSerialConfig()));
    EXPECT_NE(nullptr, ConnectionFactory::create(makeTelnetConfig()));
}

} // namespace tcm
