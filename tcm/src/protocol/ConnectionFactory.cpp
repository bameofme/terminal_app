#include "protocol/ConnectionFactory.hpp"

#include <stdexcept>

#include "protocol/SerialConnection.hpp"
#include "protocol/SshConnection.hpp"
#include "protocol/TelnetConnection.hpp"

namespace tcm {

std::unique_ptr<IConnection> ConnectionFactory::create(const SessionConfig& config) {
    switch (config.type) {
        case SessionType::SSH:
            if (!config.ssh)
                throw std::invalid_argument("SSH config not provided");
            return std::make_unique<SshConnection>(config.ssh.value());

        case SessionType::Serial:
            if (!config.serial)
                throw std::invalid_argument("Serial config not provided");
            return std::make_unique<SerialConnection>(config.serial.value());

        case SessionType::Telnet:
            if (!config.telnet)
                throw std::invalid_argument("Telnet config not provided");
            return std::make_unique<TelnetConnection>(config.telnet.value());

        default:
            throw std::invalid_argument("Unknown session type");
    }
}

} // namespace tcm
