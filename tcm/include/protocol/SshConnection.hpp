#pragma once

#include "core/IConnection.hpp"
#include "protocol/SshConfig.hpp"

#include <libssh2.h>

namespace tcm {

class SshConnection : public IConnection {
public:
    explicit SshConnection(SshConfig config);
    ~SshConnection() override;

    [[nodiscard]] int connect() noexcept override;
    [[nodiscard]] int send(std::span<const uint8_t> data) noexcept override;
    [[nodiscard]] int recv(std::span<uint8_t> buf) noexcept override;
    void disconnect() noexcept override;
    [[nodiscard]] int getFd() const noexcept override;
    [[nodiscard]] ConnectionState getState() const noexcept override;

private:
    SshConfig        m_config;
    int              m_sock    = -1;
    LIBSSH2_SESSION* m_session = nullptr;
    LIBSSH2_CHANNEL* m_channel = nullptr;
    ConnectionState  m_state   = ConnectionState::Disconnected;

    int  tcpConnect();
    int  sshHandshake();
    int  authenticate();
    int  openChannel();
    void cleanup() noexcept;
};

} // namespace tcm
