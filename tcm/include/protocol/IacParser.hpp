#pragma once

#include <cstdint>
#include <vector>

namespace tcm {

// ---------------------------------------------------------------------------
// Telnet command constants
// ---------------------------------------------------------------------------
constexpr uint8_t IAC_CMD            = 0xFF;
constexpr uint8_t WILL               = 0xFB;
constexpr uint8_t WONT               = 0xFC;
constexpr uint8_t DO_CMD             = 0xFD;
constexpr uint8_t DONT               = 0xFE;
constexpr uint8_t SB                 = 0xFA;
constexpr uint8_t SE                 = 0xF0;

constexpr uint8_t OPT_ECHO           = 0x01;
constexpr uint8_t OPT_SUPPRESS_GA    = 0x03;
constexpr uint8_t OPT_TERMINAL_TYPE  = 0x18;
constexpr uint8_t OPT_WINDOW_SIZE    = 0x1F;

// ---------------------------------------------------------------------------
// IacResult — output of a single feed() call
// ---------------------------------------------------------------------------
struct IacResult {
    std::vector<uint8_t> cleanData;   // data after stripping IAC sequences
    std::vector<uint8_t> responses;   // IAC responses to send back
};

// ---------------------------------------------------------------------------
// IacParser — state-machine Telnet IAC sequence parser
// ---------------------------------------------------------------------------
class IacParser {
public:
    IacResult feed(const uint8_t* data, size_t len);

private:
    enum class State { Normal, GotIac, GotCommand, InSubneg } m_state{State::Normal};
    uint8_t              m_command = 0;
    std::vector<uint8_t> m_subneg;
    bool                 m_prevWasIac = false; // tracks IAC inside subneg

    std::vector<uint8_t> buildResponse(uint8_t cmd, uint8_t opt);
    void handleSubnegotiation();
};

} // namespace tcm
