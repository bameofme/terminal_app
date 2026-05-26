#pragma once

#include <cstdint>
#include <span>

namespace tcm {

// ---------------------------------------------------------------------------
// Error codes
// ---------------------------------------------------------------------------
constexpr int TCM_OK          =  0;
constexpr int TCM_ERR_CONNECT = -1;
constexpr int TCM_ERR_TIMEOUT = -2;
constexpr int TCM_ERR_AUTH    = -3;
constexpr int TCM_ERR_IO      = -4;

// ---------------------------------------------------------------------------
// Connection lifecycle states
// ---------------------------------------------------------------------------
enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Error
};

// ---------------------------------------------------------------------------
// IConnection — pure virtual interface for all protocol adapters
// ---------------------------------------------------------------------------
class IConnection {
public:
    [[nodiscard]] virtual int connect() noexcept = 0;
    [[nodiscard]] virtual int send(std::span<const uint8_t> data) noexcept = 0;
    [[nodiscard]] virtual int recv(std::span<uint8_t> buf) noexcept = 0;
    virtual void disconnect() noexcept = 0;
    [[nodiscard]] virtual int getFd() const noexcept = 0;
    [[nodiscard]] virtual ConnectionState getState() const noexcept = 0;
    virtual ~IConnection() = default;
};

} // namespace tcm
