#include "protocol/TelnetConnection.hpp"

#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace tcm {

// ---------------------------------------------------------------------------
TelnetConnection::TelnetConnection(TelnetConfig config)
    : m_config(std::move(config))
{}

// ---------------------------------------------------------------------------
TelnetConnection::~TelnetConnection()
{
    disconnect();
}

// ---------------------------------------------------------------------------
// tcpConnect — resolve host, create non-blocking socket, connect with timeout
// ---------------------------------------------------------------------------
int TelnetConnection::tcpConnect() noexcept
{
    // Resolve host
    struct addrinfo hints{};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    const std::string portStr = std::to_string(m_config.port);
    struct addrinfo* res = nullptr;
    if (::getaddrinfo(m_config.host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) {
        return TCM_ERR_CONNECT;
    }

    int sock = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        ::freeaddrinfo(res);
        return TCM_ERR_CONNECT;
    }

    // Set non-blocking
    int flags = ::fcntl(sock, F_GETFL, 0);
    if (flags < 0 || ::fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
        ::close(sock);
        ::freeaddrinfo(res);
        return TCM_ERR_CONNECT;
    }

    int rc = ::connect(sock, res->ai_addr, res->ai_addrlen);
    ::freeaddrinfo(res);

    if (rc == 0) {
        // Connected immediately (unlikely but possible for loopback)
        m_sock = sock;
        return TCM_OK;
    }

    if (errno != EINPROGRESS) {
        ::close(sock);
        return TCM_ERR_CONNECT;
    }

    // Wait for connection to complete using select()
    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(sock, &wfds);

    struct timeval tv{};
    tv.tv_sec  = m_config.connectTimeoutMs / 1000;
    tv.tv_usec = (m_config.connectTimeoutMs % 1000) * 1000;

    rc = ::select(sock + 1, nullptr, &wfds, nullptr, &tv);
    if (rc <= 0) {
        ::close(sock);
        return (rc == 0) ? TCM_ERR_TIMEOUT : TCM_ERR_CONNECT;
    }

    // Check SO_ERROR to confirm connection succeeded
    int sockErr = 0;
    socklen_t errLen = sizeof(sockErr);
    if (::getsockopt(sock, SOL_SOCKET, SO_ERROR, &sockErr, &errLen) < 0 || sockErr != 0) {
        ::close(sock);
        return TCM_ERR_CONNECT;
    }

    // Restore blocking mode
    if (::fcntl(sock, F_SETFL, flags) < 0) {
        ::close(sock);
        return TCM_ERR_CONNECT;
    }

    m_sock = sock;
    return TCM_OK;
}

// ---------------------------------------------------------------------------
int TelnetConnection::connect() noexcept
{
    if (m_state == ConnectionState::Connected) {
        return TCM_OK;
    }

    m_state = ConnectionState::Connecting;
    int rc  = tcpConnect();

    if (rc != TCM_OK) {
        m_state = ConnectionState::Error;
        return rc;
    }

    m_state = ConnectionState::Connected;
    return TCM_OK;
}

// ---------------------------------------------------------------------------
// send — escape 0xFF bytes then write to socket
// ---------------------------------------------------------------------------
int TelnetConnection::send(std::span<const uint8_t> data) noexcept
{
    if (m_state != ConnectionState::Connected || m_sock < 0) {
        return TCM_ERR_IO;
    }

    // Escape 0xFF → IAC IAC
    std::vector<uint8_t> escaped;
    escaped.reserve(data.size());
    for (uint8_t byte : data) {
        escaped.push_back(byte);
        if (byte == 0xFF) {
            escaped.push_back(0xFF);
        }
    }

    ssize_t written = ::write(m_sock, escaped.data(), escaped.size());
    if (written < 0) {
        m_state = ConnectionState::Error;
        return TCM_ERR_IO;
    }

    return TCM_OK;
}

// ---------------------------------------------------------------------------
// recv — read raw bytes, parse IAC, send responses, return clean data
// ---------------------------------------------------------------------------
int TelnetConnection::recv(std::span<uint8_t> buf) noexcept
{
    if (m_state != ConnectionState::Connected || m_sock < 0) {
        return TCM_ERR_IO;
    }

    if (buf.empty()) {
        return 0;
    }

    // If we have buffered clean data from a previous read, drain it first
    if (!m_pending.empty()) {
        size_t n = std::min(m_pending.size(), buf.size());
        std::copy(m_pending.begin(), m_pending.begin() + static_cast<ptrdiff_t>(n),
                  buf.begin());
        m_pending.erase(m_pending.begin(), m_pending.begin() + static_cast<ptrdiff_t>(n));
        return static_cast<int>(n);
    }

    // Read raw bytes from socket
    std::vector<uint8_t> raw(buf.size() * 2 + 256);
    ssize_t nread = ::read(m_sock, raw.data(), raw.size());
    if (nread < 0) {
        m_state = ConnectionState::Error;
        return TCM_ERR_IO;
    }
    if (nread == 0) {
        // Peer closed connection
        m_state = ConnectionState::Disconnected;
        return 0;
    }

    // Parse IAC sequences
    IacResult parsed = m_parser.feed(raw.data(), static_cast<size_t>(nread));

    // Send any IAC responses back
    if (!parsed.responses.empty()) {
        if (::write(m_sock, parsed.responses.data(), parsed.responses.size()) < 0) {
            // Ignore write errors for negotiation responses
        }
    }

    // Copy clean data to caller's buffer; buffer the overflow
    size_t n = std::min(parsed.cleanData.size(), buf.size());
    std::copy(parsed.cleanData.begin(),
              parsed.cleanData.begin() + static_cast<ptrdiff_t>(n),
              buf.begin());

    if (parsed.cleanData.size() > buf.size()) {
        m_pending.insert(m_pending.end(),
                         parsed.cleanData.begin() + static_cast<ptrdiff_t>(n),
                         parsed.cleanData.end());
    }

    return static_cast<int>(n);
}

// ---------------------------------------------------------------------------
void TelnetConnection::disconnect() noexcept
{
    if (m_sock >= 0) {
        ::close(m_sock);
        m_sock = -1;
    }
    m_state = ConnectionState::Disconnected;
}

// ---------------------------------------------------------------------------
int TelnetConnection::getFd() const noexcept
{
    return m_sock;
}

// ---------------------------------------------------------------------------
ConnectionState TelnetConnection::getState() const noexcept
{
    return m_state;
}

} // namespace tcm
