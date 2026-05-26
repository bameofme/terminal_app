#pragma once

#include "core/IConnection.hpp"
#include "protocol/SerialConfig.hpp"

#include <termios.h>

namespace tcm {

class SerialConnection : public IConnection {
public:
    explicit SerialConnection(SerialConfig config);
    ~SerialConnection() override;

    [[nodiscard]] int connect() noexcept override;
    [[nodiscard]] int send(std::span<const uint8_t> data) noexcept override;
    [[nodiscard]] int recv(std::span<uint8_t> buf) noexcept override;
    void disconnect() noexcept override;
    [[nodiscard]] int getFd() const noexcept override;
    [[nodiscard]] ConnectionState getState() const noexcept override;

private:
    SerialConfig    m_config;
    int             m_fd    = -1;
    struct termios  m_savedTermios{};
    ConnectionState m_state = ConnectionState::Disconnected;

    static speed_t baudRateToSpeed(uint32_t baudRate);
    int configurePort() noexcept;
};

} // namespace tcm
