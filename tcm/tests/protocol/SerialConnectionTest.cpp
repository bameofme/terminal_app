#include <gtest/gtest.h>

#include "protocol/SerialConnection.hpp"
#include "protocol/SerialConfig.hpp"

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

using namespace tcm;

// ---------------------------------------------------------------------------
// SerialConfig default values
// ---------------------------------------------------------------------------

TEST(SerialConfigTest, DefaultValues)
{
    SerialConfig cfg;
    EXPECT_EQ(cfg.baudRate,     115200u);
    EXPECT_EQ(cfg.dataBits,     8u);
    EXPECT_EQ(cfg.parity,       SerialConfig::Parity::None);
    EXPECT_EQ(cfg.stopBits,     SerialConfig::StopBits::One);
    EXPECT_EQ(cfg.flowControl,  SerialConfig::FlowControl::None);
    EXPECT_EQ(cfg.readTimeoutMs, 100);
}

TEST(SerialConfigTest, DeviceDefaultEmpty)
{
    SerialConfig cfg;
    EXPECT_TRUE(cfg.device.empty());
}

// ---------------------------------------------------------------------------
// Initial state before connect
// ---------------------------------------------------------------------------

TEST(SerialConnectionTest, InitialStateIsDisconnected)
{
    SerialConfig cfg;
    cfg.device = "/dev/nonexistent";
    SerialConnection conn(cfg);
    EXPECT_EQ(conn.getState(), ConnectionState::Disconnected);
}

TEST(SerialConnectionTest, GetFdReturnsMinusOneBeforeConnect)
{
    SerialConfig cfg;
    cfg.device = "/dev/nonexistent";
    SerialConnection conn(cfg);
    EXPECT_EQ(conn.getFd(), -1);
}

// ---------------------------------------------------------------------------
// connect() error paths
// ---------------------------------------------------------------------------

TEST(SerialConnectionTest, ConnectNonexistentDeviceReturnsError)
{
    SerialConfig cfg;
    cfg.device = "/dev/nonexistent_serial_device_xyz";
    SerialConnection conn(cfg);
    int rc = conn.connect();
    EXPECT_LT(rc, 0);
    EXPECT_EQ(rc, TCM_ERR_CONNECT);
}

TEST(SerialConnectionTest, ConnectEmptyDevicePathReturnsError)
{
    SerialConfig cfg;
    // device is empty string — open("", ...) will fail
    SerialConnection conn(cfg);
    int rc = conn.connect();
    EXPECT_LT(rc, 0);
}

TEST(SerialConnectionTest, ConnectRegularFileReturnsError)
{
    // Create a temporary regular file; it is NOT a tty
    char tmpName[] = "/tmp/tcm_serial_test_XXXXXX";
    int tmpFd = mkstemp(tmpName);
    ASSERT_GE(tmpFd, 0);
    ::close(tmpFd);

    SerialConfig cfg;
    cfg.device = tmpName;
    SerialConnection conn(cfg);
    int rc = conn.connect();
    EXPECT_LT(rc, 0);
    EXPECT_EQ(rc, TCM_ERR_CONNECT);

    ::unlink(tmpName);
}

TEST(SerialConnectionTest, StateRemainsDisconnectedAfterFailedConnect)
{
    SerialConfig cfg;
    cfg.device = "/dev/nonexistent_device";
    SerialConnection conn(cfg);
    (void)conn.connect();
    EXPECT_EQ(conn.getState(), ConnectionState::Disconnected);
}

TEST(SerialConnectionTest, FdRemainsMinusOneAfterFailedConnect)
{
    SerialConfig cfg;
    cfg.device = "/dev/nonexistent_device";
    SerialConnection conn(cfg);
    (void)conn.connect();
    EXPECT_EQ(conn.getFd(), -1);
}

// ---------------------------------------------------------------------------
// disconnect() safety
// ---------------------------------------------------------------------------

TEST(SerialConnectionTest, DisconnectBeforeConnectDoesNotCrash)
{
    SerialConfig cfg;
    cfg.device = "/dev/nonexistent";
    SerialConnection conn(cfg);
    EXPECT_NO_FATAL_FAILURE(conn.disconnect());
}

TEST(SerialConnectionTest, MultipleDisconnectCallsDoNotCrash)
{
    SerialConfig cfg;
    cfg.device = "/dev/nonexistent";
    SerialConnection conn(cfg);
    EXPECT_NO_FATAL_FAILURE({
        conn.disconnect();
        conn.disconnect();
        conn.disconnect();
    });
}

// ---------------------------------------------------------------------------
// send() / recv() when not connected
// ---------------------------------------------------------------------------

TEST(SerialConnectionTest, SendWhenDisconnectedReturnsError)
{
    SerialConfig cfg;
    cfg.device = "/dev/nonexistent";
    SerialConnection conn(cfg);

    const uint8_t buf[] = { 0x01, 0x02 };
    int rc = conn.send(buf);
    EXPECT_LT(rc, 0);
}

TEST(SerialConnectionTest, RecvWhenDisconnectedReturnsError)
{
    SerialConfig cfg;
    cfg.device = "/dev/nonexistent";
    SerialConnection conn(cfg);

    uint8_t buf[16]{};
    int rc = conn.recv(buf);
    EXPECT_LT(rc, 0);
}

// ---------------------------------------------------------------------------
// Construct and destroy — no crash
// ---------------------------------------------------------------------------

TEST(SerialConnectionTest, ConstructDestroyNoCrash)
{
    SerialConfig cfg;
    cfg.device    = "/dev/ttyUSB0";
    cfg.baudRate  = 9600;
    cfg.dataBits  = 8;
    cfg.parity    = SerialConfig::Parity::None;
    cfg.stopBits  = SerialConfig::StopBits::One;
    cfg.flowControl = SerialConfig::FlowControl::None;
    EXPECT_NO_FATAL_FAILURE({
        SerialConnection conn(cfg);
    });
}

// ---------------------------------------------------------------------------
// Virtual serial pair via socat (skipped when socat absent)
// ---------------------------------------------------------------------------

class SocatSerialTest : public ::testing::Test {
protected:
    int  ptyMaster = -1;
    int  ptySlave  = -1;
    char slaveName[256]{};

    bool socatAvailable = false;

    void SetUp() override
    {
        // Check socat
        int rc = system("which socat > /dev/null 2>&1");
        socatAvailable = (rc == 0);
        if (!socatAvailable) return;

        // Open a PTY pair using posix_openpt
        ptyMaster = posix_openpt(O_RDWR | O_NOCTTY);
        if (ptyMaster < 0) return;
        if (grantpt(ptyMaster) != 0 || unlockpt(ptyMaster) != 0) {
            ::close(ptyMaster);
            ptyMaster = -1;
            return;
        }
        char* name = ptsname(ptyMaster);
        if (!name) {
            ::close(ptyMaster);
            ptyMaster = -1;
            return;
        }
        std::strncpy(slaveName, name, sizeof(slaveName) - 1);
        ptySlave = ::open(slaveName, O_RDWR | O_NOCTTY);
        if (ptySlave < 0) {
            ::close(ptyMaster);
            ptyMaster = -1;
        }
    }

    void TearDown() override
    {
        if (ptySlave  >= 0) { ::close(ptySlave);  ptySlave  = -1; }
        if (ptyMaster >= 0) { ::close(ptyMaster); ptyMaster = -1; }
    }
};

TEST_F(SocatSerialTest, ConnectToVirtualPtySucceeds)
{
    if (!socatAvailable || ptyMaster < 0) {
        GTEST_SKIP() << "socat / pty not available";
    }

    SerialConfig cfg;
    cfg.device   = slaveName;
    cfg.baudRate = 115200;

    SerialConnection conn(cfg);
    int rc = conn.connect();
    EXPECT_EQ(rc, TCM_OK);
    EXPECT_EQ(conn.getState(), ConnectionState::Connected);
    EXPECT_GE(conn.getFd(), 0);

    conn.disconnect();
    EXPECT_EQ(conn.getState(), ConnectionState::Disconnected);
    EXPECT_EQ(conn.getFd(), -1);
}

TEST_F(SocatSerialTest, SendAndRecvOverPty)
{
    if (!socatAvailable || ptyMaster < 0) {
        GTEST_SKIP() << "socat / pty not available";
    }

    SerialConfig cfg;
    cfg.device   = slaveName;
    cfg.baudRate = 115200;

    SerialConnection conn(cfg);
    ASSERT_EQ(conn.connect(), TCM_OK);

    const uint8_t tx[] = { 'H', 'i', '\n' };
    int sent = conn.send(tx);
    EXPECT_EQ(sent, static_cast<int>(sizeof(tx)));

    // Give kernel time to echo to master
    usleep(20'000);

    uint8_t rx[64]{};
    // Read from master side
    ssize_t n = ::read(ptyMaster, rx, sizeof(rx));
    EXPECT_GT(n, 0);

    conn.disconnect();
}

TEST_F(SocatSerialTest, DoubleConnectIsIdempotent)
{
    if (!socatAvailable || ptyMaster < 0) {
        GTEST_SKIP() << "socat / pty not available";
    }

    SerialConfig cfg;
    cfg.device   = slaveName;
    cfg.baudRate = 9600;

    SerialConnection conn(cfg);
    EXPECT_EQ(conn.connect(), TCM_OK);
    EXPECT_EQ(conn.connect(), TCM_OK); // second call should be no-op
    EXPECT_EQ(conn.getState(), ConnectionState::Connected);

    conn.disconnect();
}
