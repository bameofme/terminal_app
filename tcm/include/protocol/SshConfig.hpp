#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace tcm {

struct SshConfig {
    std::string host;
    uint16_t    port             = 22;
    std::string username;
    std::string password;           // empty nếu dùng key
    std::string privateKeyPath;     // empty nếu dùng password
    std::string knownHostsPath;     // default: ~/.ssh/known_hosts
    int         connectTimeoutMs   = 10000;
    int         keepAliveIntervalS = 30;
    bool        strictHostChecking = false;  // simplified for now

    struct Tunnel {
        std::string localHost  = "127.0.0.1";
        uint16_t    localPort  = 0;
        std::string remoteHost;
        uint16_t    remotePort = 0;
    };

    std::optional<Tunnel> tunnel;
};

} // namespace tcm
