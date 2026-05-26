#pragma once

#include <cstdint>
#include <string>

namespace tcm {

struct SerialConfig {
    std::string device;               // e.g. "/dev/ttyUSB0"
    uint32_t    baudRate     = 115200;
    uint8_t     dataBits     = 8;     // 7 or 8

    enum class Parity { None, Odd, Even } parity = Parity::None;
    enum class StopBits { One, Two }     stopBits = StopBits::One;
    enum class FlowControl { None, Hardware, Software }
                             flowControl = FlowControl::None;

    int readTimeoutMs = 100;
};

} // namespace tcm
