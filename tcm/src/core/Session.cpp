#include "core/Session.hpp"

#include <iomanip>
#include <random>
#include <sstream>

namespace tcm {

// ---------------------------------------------------------------------------
// generateUUID — RFC 4122 version 4 (random)
// Format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
// ---------------------------------------------------------------------------
std::string generateUUID() {
    static std::random_device              rd;
    static std::mt19937_64                 gen(rd());
    std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);

    uint64_t hi = dist(gen);
    uint64_t lo = dist(gen);

    // Set version 4: bits 15-12 of the third group → 0100
    hi = (hi & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;

    // Set variant 10xx: top two bits of the fourth group → 10
    lo = (lo & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    std::ostringstream ss;
    ss << std::hex << std::setfill('0')
       << std::setw(8)  << ((hi >> 32) & 0xFFFFFFFFULL) << '-'
       << std::setw(4)  << ((hi >> 16) & 0xFFFFULL)     << '-'
       << std::setw(4)  << ( hi        & 0xFFFFULL)     << '-'
       << std::setw(4)  << ((lo >> 48) & 0xFFFFULL)     << '-'
       << std::setw(12) << ( lo        & 0x0000FFFFFFFFFFFFULL);

    return ss.str();
}

} // namespace tcm
