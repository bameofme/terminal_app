#pragma once

#include "core/IConnection.hpp"
#include <gmock/gmock.h>

namespace tcm::test {

class MockConnection : public tcm::IConnection {
public:
    MOCK_METHOD(int, connect, (), (noexcept, override));
    MOCK_METHOD(int, send, (std::span<const uint8_t> data), (noexcept, override));
    MOCK_METHOD(int, recv, (std::span<uint8_t> buf), (noexcept, override));
    MOCK_METHOD(void, disconnect, (), (noexcept, override));
    MOCK_METHOD(int, getFd, (), (const, noexcept, override));
    MOCK_METHOD(tcm::ConnectionState, getState, (), (const, noexcept, override));
};

} // namespace tcm::test
