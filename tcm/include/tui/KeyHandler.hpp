#pragma once

#include <cstdint>
#include <cstddef>

#include "core/EventBus.hpp"

namespace tcm {

class KeyHandler {
public:
    explicit KeyHandler(EventBus& bus);

    // Process raw input bytes, detect Ctrl+] (0x1D) and other special sequences
    void processRawInput(const uint8_t* buf, size_t len);

    // Process a single key code từ ncurses getch()
    void processKey(int key);

private:
    EventBus& m_bus;
    bool detectEscapeSequence(uint8_t byte);
};

}  // namespace tcm
