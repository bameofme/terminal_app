#include "protocol/SshConnection.hpp"

#include <cerrno>
#include <cstring>
#include <netdb.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace tcm {

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

SshConnection::SshConnection(SshConfig config)
    : m_config(std::move(config))
{}

SshConnection::~SshConnection()
{
    cleanup();
}

// ---------------------------------------------------------------------------
// IConnection interface
// ---------------------------------------------------------------------------

int SshConnection::connect() noexcept
{
    if (m_state == ConnectionState::Connected) return TCM_OK;

    m_state = ConnectionState::Connecting;

    int rc = tcpConnect();
    if (rc != 0) {
        m_state = ConnectionState::Error;
        return rc;
    }

    rc = sshHandshake();
    if (rc != 0) {
        cleanup();
        m_state = ConnectionState::Error;
        return rc;
    }

    rc = authenticate();
    if (rc != 0) {
        cleanup();
        m_state = ConnectionState::Error;
        return rc;
    }

    rc = openChannel();
    if (rc != 0) {
        cleanup();
        m_state = ConnectionState::Error;
        return rc;
    }

    // Non-blocking I/O after channel is ready
    libssh2_session_set_blocking(m_session, 0);

    m_state = ConnectionState::Connected;
    return TCM_OK;
}

int SshConnection::send(std::span<const uint8_t> data) noexcept
{
    if (m_state != ConnectionState::Connected || !m_channel)
        return TCM_ERR_IO;

    size_t written = 0;
    while (written < data.size()) {
        ssize_t rc = libssh2_channel_write(
            m_channel,
            reinterpret_cast<const char*>(data.data() + written),
            data.size() - written);

        if (rc == LIBSSH2_ERROR_EAGAIN) continue;
        if (rc < 0) return TCM_ERR_IO;
        written += static_cast<size_t>(rc);
    }
    return static_cast<int>(written);
}

int SshConnection::recv(std::span<uint8_t> buf) noexcept
{
    if (m_state != ConnectionState::Connected || !m_channel)
        return TCM_ERR_IO;

    ssize_t rc = libssh2_channel_read(
        m_channel,
        reinterpret_cast<char*>(buf.data()),
        buf.size());

    if (rc == LIBSSH2_ERROR_EAGAIN) return 0;
    if (rc < 0) return TCM_ERR_IO;
    return static_cast<int>(rc);
}

void SshConnection::disconnect() noexcept
{
    cleanup();
    m_state = ConnectionState::Disconnected;
}

int SshConnection::getFd() const noexcept
{
    return m_sock;
}

ConnectionState SshConnection::getState() const noexcept
{
    return m_state;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

int SshConnection::tcpConnect()
{
    struct addrinfo hints{};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res = nullptr;
    std::string portStr  = std::to_string(m_config.port);

    int gai = getaddrinfo(m_config.host.c_str(), portStr.c_str(), &hints, &res);
    if (gai != 0 || res == nullptr)
        return TCM_ERR_CONNECT;

    m_sock = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (m_sock < 0) {
        freeaddrinfo(res);
        return TCM_ERR_CONNECT;
    }

    // Set O_NONBLOCK for timeout-aware connect
    int flags = fcntl(m_sock, F_GETFL, 0);
    if (flags < 0) flags = 0;
    fcntl(m_sock, F_SETFL, flags | O_NONBLOCK);

    int rc = ::connect(m_sock, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    if (rc == 0) {
        // Immediate success (rare on non-loopback)
        fcntl(m_sock, F_SETFL, flags);
        return 0;
    }

    if (errno != EINPROGRESS) {
        ::close(m_sock);
        m_sock = -1;
        return TCM_ERR_CONNECT;
    }

    // Wait for writable socket (connection complete or error)
    fd_set wfds;
    fd_set efds;
    FD_ZERO(&wfds);
    FD_ZERO(&efds);
    FD_SET(m_sock, &wfds);
    FD_SET(m_sock, &efds);

    struct timeval tv{};
    tv.tv_sec  = m_config.connectTimeoutMs / 1000;
    tv.tv_usec = (m_config.connectTimeoutMs % 1000) * 1000;

    rc = select(m_sock + 1, nullptr, &wfds, &efds, &tv);
    if (rc <= 0) {
        ::close(m_sock);
        m_sock = -1;
        return (rc == 0) ? TCM_ERR_TIMEOUT : TCM_ERR_CONNECT;
    }

    int err      = 0;
    socklen_t sl = sizeof(err);
    getsockopt(m_sock, SOL_SOCKET, SO_ERROR, &err, &sl);
    if (err != 0) {
        ::close(m_sock);
        m_sock = -1;
        return TCM_ERR_CONNECT;
    }

    // Restore blocking mode for libssh2 handshake / auth
    fcntl(m_sock, F_SETFL, flags);
    return 0;
}

int SshConnection::sshHandshake()
{
    m_session = libssh2_session_init();
    if (!m_session) return TCM_ERR_CONNECT;

    int rc = libssh2_session_handshake(m_session, m_sock);
    if (rc != 0) {
        libssh2_session_free(m_session);
        m_session = nullptr;
        return TCM_ERR_CONNECT;
    }
    return 0;
}

int SshConnection::authenticate()
{
    int rc;
    if (!m_config.password.empty()) {
        rc = libssh2_userauth_password(
            m_session,
            m_config.username.c_str(),
            m_config.password.c_str());
    } else {
        rc = libssh2_userauth_publickey_fromfile(
            m_session,
            m_config.username.c_str(),
            nullptr,                            // public-key path (auto-derived)
            m_config.privateKeyPath.c_str(),
            nullptr);                           // passphrase
    }
    return (rc == 0) ? 0 : TCM_ERR_AUTH;
}

int SshConnection::openChannel()
{
    m_channel = libssh2_channel_open_session(m_session);
    if (!m_channel) return TCM_ERR_CONNECT;

    if (libssh2_channel_request_pty(m_channel, "xterm-256color") != 0) {
        libssh2_channel_close(m_channel);
        libssh2_channel_free(m_channel);
        m_channel = nullptr;
        return TCM_ERR_CONNECT;
    }

    if (libssh2_channel_shell(m_channel) != 0) {
        libssh2_channel_close(m_channel);
        libssh2_channel_free(m_channel);
        m_channel = nullptr;
        return TCM_ERR_CONNECT;
    }

    return 0;
}

void SshConnection::cleanup() noexcept
{
    if (m_channel) {
        libssh2_channel_close(m_channel);
        libssh2_channel_free(m_channel);
        m_channel = nullptr;
    }
    if (m_session) {
        libssh2_session_disconnect(m_session, "Disconnecting");
        libssh2_session_free(m_session);
        m_session = nullptr;
    }
    if (m_sock >= 0) {
        ::close(m_sock);
        m_sock = -1;
    }
}

} // namespace tcm
