#pragma once

#include "core/IConnection.hpp"
#include "protocol/IacParser.hpp"
#include "protocol/TelnetConfig.hpp"

#include <span>
#include <vector>

namespace tcm {

class TelnetConnection : public IConnection {
public:
    explicit TelnetConnection(TelnetConfig config);
    ~TelnetConnection() override;

    [[nodiscard]] int connect() noexcept override;
    [[nodiscard]] int send(std::span<const uint8_t> data) noexcept override;
    [[nodiscard]] int recv(std::span<uint8_t> buf) noexcept override;
    void disconnect() noexcept override;
    [[nodiscard]] int getFd() const noexcept override;
    [[nodiscard]] ConnectionState getState() const noexcept override;

private:
    TelnetConfig    m_config;
    int             m_sock  = -1;
    IacParser       m_parser;
    ConnectionState m_state = ConnectionState::Disconnected;
    std::vector<uint8_t> m_pending;  // buffered clean data not yet returned

    int tcpConnect() noexcept;
};

} // namespace tcm
