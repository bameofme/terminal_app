#include "tui/KeyHandler.hpp"

#include "core/EventBus.hpp"

namespace tcm {

KeyHandler::KeyHandler(EventBus& bus) : m_bus(bus) {}

bool KeyHandler::detectEscapeSequence(uint8_t byte) {
    return byte == 0x1D;  // Ctrl+]
}

void KeyHandler::processRawInput(const uint8_t* buf, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        uint8_t byte   = buf[i];
        bool    isEsc  = detectEscapeSequence(byte);
        m_bus.publish(KeyPressedEvent{static_cast<int>(byte), isEsc});
    }
}

void KeyHandler::processKey(int key) {
    m_bus.publish(KeyPressedEvent{key, false});
}

}  // namespace tcm
