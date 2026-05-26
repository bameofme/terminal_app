#pragma once

#include <cstdint>
#include <string>

namespace tcm {

struct TelnetConfig {
    std::string host;
    uint16_t    port             = 23;
    int         connectTimeoutMs = 5000;
    bool        suppressGoAhead  = true;
};

} // namespace tcm
