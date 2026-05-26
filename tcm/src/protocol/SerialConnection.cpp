#include "protocol/SerialConnection.hpp"

#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

namespace tcm {

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

SerialConnection::SerialConnection(SerialConfig config)
    : m_config(std::move(config))
{}

SerialConnection::~SerialConnection()
{
    disconnect();
}

// ---------------------------------------------------------------------------
// Baud rate helper
// ---------------------------------------------------------------------------

speed_t SerialConnection::baudRateToSpeed(uint32_t baudRate)
{
    switch (baudRate) {
        case 300:    return B300;
        case 1200:   return B1200;
        case 2400:   return B2400;
        case 4800:   return B4800;
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        case 460800: return B460800;
        case 921600: return B921600;
        default:     return B0;   // invalid — caller must check
    }
}

// ---------------------------------------------------------------------------
// configurePort — apply termios settings
// ---------------------------------------------------------------------------

int SerialConnection::configurePort() noexcept
{
    struct termios tty{};

    // Start from a clean raw-mode baseline
    cfmakeraw(&tty);

    // Baud rate
    speed_t speed = baudRateToSpeed(m_config.baudRate);
    if (speed == B0) {
        return TCM_ERR_CONNECT;
    }
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    // Data bits
    tty.c_cflag &= ~static_cast<tcflag_t>(CSIZE);
    if (m_config.dataBits == 7) {
        tty.c_cflag |= CS7;
    } else {
        tty.c_cflag |= CS8;
    }

    // Parity
    tty.c_cflag &= ~static_cast<tcflag_t>(PARENB | PARODD);
    if (m_config.parity == SerialConfig::Parity::Odd) {
        tty.c_cflag |= (PARENB | PARODD);
    } else if (m_config.parity == SerialConfig::Parity::Even) {
        tty.c_cflag |= PARENB;
    }

    // Stop bits
    tty.c_cflag &= ~static_cast<tcflag_t>(CSTOPB);
    if (m_config.stopBits == SerialConfig::StopBits::Two) {
        tty.c_cflag |= CSTOPB;
    }

    // Flow control
    tty.c_cflag &= ~static_cast<tcflag_t>(CRTSCTS);
    tty.c_iflag &= ~static_cast<tcflag_t>(IXON | IXOFF | IXANY);
    if (m_config.flowControl == SerialConfig::FlowControl::Hardware) {
        tty.c_cflag |= CRTSCTS;
    } else if (m_config.flowControl == SerialConfig::FlowControl::Software) {
        tty.c_iflag |= (IXON | IXOFF);
    }

    // Enable receiver, ignore modem control lines
    tty.c_cflag |= (CREAD | CLOCAL);

    // Non-blocking read (VMIN=0, VTIME=0)
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(m_fd, TCSANOW, &tty) != 0) {
        return TCM_ERR_CONNECT;
    }
    return TCM_OK;
}

// ---------------------------------------------------------------------------
// connect
// ---------------------------------------------------------------------------

int SerialConnection::connect() noexcept
{
    if (m_state == ConnectionState::Connected) {
        return TCM_OK;
    }

    m_fd = ::open(m_config.device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (m_fd < 0) {
        return TCM_ERR_CONNECT;
    }

    // Must be a real TTY
    if (isatty(m_fd) == 0) {
        ::close(m_fd);
        m_fd = -1;
        return TCM_ERR_CONNECT;
    }

    // Save original settings for restoration on disconnect
    if (tcgetattr(m_fd, &m_savedTermios) != 0) {
        ::close(m_fd);
        m_fd = -1;
        return TCM_ERR_CONNECT;
    }

    int rc = configurePort();
    if (rc != TCM_OK) {
        ::close(m_fd);
        m_fd = -1;
        return rc;
    }

    m_state = ConnectionState::Connected;
    return TCM_OK;
}

// ---------------------------------------------------------------------------
// send
// ---------------------------------------------------------------------------

int SerialConnection::send(std::span<const uint8_t> data) noexcept
{
    if (m_state != ConnectionState::Connected || m_fd < 0) {
        return TCM_ERR_IO;
    }

    std::size_t written = 0;
    while (written < data.size()) {
        ssize_t n = ::write(m_fd,
                            data.data() + written,
                            data.size() - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            return TCM_ERR_IO;
        }
        written += static_cast<std::size_t>(n);
    }
    return static_cast<int>(written);
}

// ---------------------------------------------------------------------------
// recv
// ---------------------------------------------------------------------------

int SerialConnection::recv(std::span<uint8_t> buf) noexcept
{
    if (m_state != ConnectionState::Connected || m_fd < 0) {
        return TCM_ERR_IO;
    }

    ssize_t n = ::read(m_fd, buf.data(), buf.size());
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0; // no data available right now
        }
        if (errno == ENXIO) {
            return TCM_ERR_IO; // device unplugged
        }
        return TCM_ERR_IO;
    }
    return static_cast<int>(n);
}

// ---------------------------------------------------------------------------
// disconnect
// ---------------------------------------------------------------------------

void SerialConnection::disconnect() noexcept
{
    if (m_fd < 0) {
        return;
    }

    // Restore original port settings (best-effort)
    tcsetattr(m_fd, TCSANOW, &m_savedTermios);

    ::close(m_fd);
    m_fd    = -1;
    m_state = ConnectionState::Disconnected;
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

int SerialConnection::getFd() const noexcept
{
    return m_fd;
}

ConnectionState SerialConnection::getState() const noexcept
{
    return m_state;
}

} // namespace tcm
