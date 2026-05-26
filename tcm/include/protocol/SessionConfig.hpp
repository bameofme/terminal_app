#pragma once

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "core/Session.hpp"
#include "protocol/SerialConfig.hpp"
#include "protocol/SshConfig.hpp"
#include "protocol/TelnetConfig.hpp"

namespace tcm {

// ---------------------------------------------------------------------------
// SessionConfig — unified config combining all protocol configs
// ---------------------------------------------------------------------------
struct SessionConfig {
    std::string name;
    SessionType type = SessionType::SSH;

    std::optional<SshConfig>    ssh;
    std::optional<SerialConfig> serial;
    std::optional<TelnetConfig> telnet;

    nlohmann::json toJson() const;
    static SessionConfig fromJson(const nlohmann::json& j);
};

} // namespace tcm
