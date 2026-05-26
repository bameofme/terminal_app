#include "protocol/IacParser.hpp"

namespace tcm {

// ---------------------------------------------------------------------------
// buildResponse — generate the appropriate IAC reply for a negotiation
// ---------------------------------------------------------------------------
std::vector<uint8_t> IacParser::buildResponse(uint8_t cmd, uint8_t opt)
{
    // Options we are willing to support
    auto supported = [](uint8_t o) {
        return o == OPT_ECHO || o == OPT_SUPPRESS_GA;
    };

    uint8_t reply = 0;
    switch (cmd) {
        case DO_CMD:  reply = supported(opt) ? WILL : WONT; break;
        case WILL:    reply = supported(opt) ? DO_CMD : DONT; break;
        case DONT:    reply = WONT; break;
        case WONT:    reply = DONT; break;
        default:      return {};
    }

    return { IAC_CMD, reply, opt };
}

// ---------------------------------------------------------------------------
// handleSubnegotiation — currently we drop all sub-negotiations
// ---------------------------------------------------------------------------
void IacParser::handleSubnegotiation()
{
    // No sub-negotiation options are acted on; just drop the buffer.
    m_subneg.clear();
}

// ---------------------------------------------------------------------------
// feed — process a chunk of raw bytes from the network
// ---------------------------------------------------------------------------
IacResult IacParser::feed(const uint8_t* data, size_t len)
{
    IacResult result;

    for (size_t i = 0; i < len; ++i) {
        const uint8_t byte = data[i];

        switch (m_state) {
            // -----------------------------------------------------------------
            case State::Normal:
                if (byte == IAC_CMD) {
                    m_state = State::GotIac;
                } else {
                    result.cleanData.push_back(byte);
                }
                break;

            // -----------------------------------------------------------------
            case State::GotIac:
                if (byte == IAC_CMD) {
                    // Escaped IAC — pass a single 0xFF through
                    result.cleanData.push_back(0xFF);
                    m_state = State::Normal;
                } else if (byte == SB) {
                    m_subneg.clear();
                    m_prevWasIac = false;
                    m_state = State::InSubneg;
                } else if (byte == WILL || byte == WONT ||
                           byte == DO_CMD || byte == DONT) {
                    m_command = byte;
                    m_state   = State::GotCommand;
                } else {
                    // SE, NOP, DM, BRK, IP, AO, AYT, EC, EL, GA — ignore
                    m_state = State::Normal;
                }
                break;

            // -----------------------------------------------------------------
            case State::GotCommand: {
                // byte is the option
                auto resp = buildResponse(m_command, byte);
                result.responses.insert(result.responses.end(),
                                        resp.begin(), resp.end());
                m_state = State::Normal;
                break;
            }

            // -----------------------------------------------------------------
            case State::InSubneg:
                if (m_prevWasIac) {
                    if (byte == SE) {
                        // End of sub-negotiation — drop it
                        handleSubnegotiation();
                        m_state = State::Normal;
                    } else if (byte == IAC_CMD) {
                        // Double IAC inside subneg — keep one
                        m_subneg.push_back(IAC_CMD);
                    } else {
                        // Should not happen in well-formed stream; treat as data
                        m_subneg.push_back(byte);
                    }
                    m_prevWasIac = false;
                } else {
                    if (byte == IAC_CMD) {
                        m_prevWasIac = true;
                    } else {
                        m_subneg.push_back(byte);
                    }
                }
                break;
        }
    }

    return result;
}

} // namespace tcm
