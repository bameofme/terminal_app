#pragma once

#include <memory>

#include "core/IConnection.hpp"
#include "protocol/SessionConfig.hpp"

namespace tcm {

// ---------------------------------------------------------------------------
// ConnectionFactory — creates IConnection instances from a SessionConfig
// ---------------------------------------------------------------------------
class ConnectionFactory {
public:
    static std::unique_ptr<IConnection> create(const SessionConfig& config);
};

} // namespace tcm
