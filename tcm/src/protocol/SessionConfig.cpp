#include "protocol/SessionConfig.hpp"

#include <stdexcept>

namespace tcm {

namespace {

// ---------------------------------------------------------------------------
// SessionType helpers
// ---------------------------------------------------------------------------
static std::string sessionTypeToString(SessionType t) {
    switch (t) {
        case SessionType::SSH:    return "SSH";
        case SessionType::Serial: return "Serial";
        case SessionType::Telnet: return "Telnet";
        default:                  return "SSH";
    }
}

static SessionType sessionTypeFromString(const std::string& s) {
    if (s == "SSH")    return SessionType::SSH;
    if (s == "Serial") return SessionType::Serial;
    if (s == "Telnet") return SessionType::Telnet;
    throw std::invalid_argument("Unknown session type: " + s);
}

// ---------------------------------------------------------------------------
// SshConfig helpers
// ---------------------------------------------------------------------------
static nlohmann::json tunnelToJson(const SshConfig::Tunnel& t) {
    return nlohmann::json{
        {"localHost",  t.localHost},
        {"localPort",  t.localPort},
        {"remoteHost", t.remoteHost},
        {"remotePort", t.remotePort}
    };
}

static SshConfig::Tunnel tunnelFromJson(const nlohmann::json& j) {
    SshConfig::Tunnel t;
    t.localHost  = j.at("localHost").get<std::string>();
    t.localPort  = j.at("localPort").get<uint16_t>();
    t.remoteHost = j.at("remoteHost").get<std::string>();
    t.remotePort = j.at("remotePort").get<uint16_t>();
    return t;
}

static nlohmann::json sshToJson(const SshConfig& c) {
    nlohmann::json j;
    j["host"]               = c.host;
    j["port"]               = c.port;
    j["username"]           = c.username;
    j["password"]           = c.password;
    j["privateKeyPath"]     = c.privateKeyPath;
    j["knownHostsPath"]     = c.knownHostsPath;
    j["connectTimeoutMs"]   = c.connectTimeoutMs;
    j["keepAliveIntervalS"] = c.keepAliveIntervalS;
    j["strictHostChecking"] = c.strictHostChecking;
    if (c.tunnel) j["tunnel"] = tunnelToJson(c.tunnel.value());
    return j;
}

static SshConfig sshFromJson(const nlohmann::json& j) {
    SshConfig c;
    c.host               = j.at("host").get<std::string>();
    c.port               = j.at("port").get<uint16_t>();
    c.username           = j.at("username").get<std::string>();
    c.password           = j.at("password").get<std::string>();
    c.privateKeyPath     = j.at("privateKeyPath").get<std::string>();
    c.knownHostsPath     = j.at("knownHostsPath").get<std::string>();
    c.connectTimeoutMs   = j.at("connectTimeoutMs").get<int>();
    c.keepAliveIntervalS = j.at("keepAliveIntervalS").get<int>();
    c.strictHostChecking = j.at("strictHostChecking").get<bool>();
    if (j.contains("tunnel")) c.tunnel = tunnelFromJson(j.at("tunnel"));
    return c;
}

// ---------------------------------------------------------------------------
// SerialConfig helpers
// ---------------------------------------------------------------------------
static std::string parityToString(SerialConfig::Parity p) {
    switch (p) {
        case SerialConfig::Parity::None: return "None";
        case SerialConfig::Parity::Odd:  return "Odd";
        case SerialConfig::Parity::Even: return "Even";
        default:                         return "None";
    }
}

static SerialConfig::Parity parityFromString(const std::string& s) {
    if (s == "None") return SerialConfig::Parity::None;
    if (s == "Odd")  return SerialConfig::Parity::Odd;
    if (s == "Even") return SerialConfig::Parity::Even;
    throw std::invalid_argument("Unknown parity: " + s);
}

static std::string stopBitsToString(SerialConfig::StopBits sb) {
    switch (sb) {
        case SerialConfig::StopBits::One: return "One";
        case SerialConfig::StopBits::Two: return "Two";
        default:                          return "One";
    }
}

static SerialConfig::StopBits stopBitsFromString(const std::string& s) {
    if (s == "One") return SerialConfig::StopBits::One;
    if (s == "Two") return SerialConfig::StopBits::Two;
    throw std::invalid_argument("Unknown stop bits: " + s);
}

static std::string flowControlToString(SerialConfig::FlowControl fc) {
    switch (fc) {
        case SerialConfig::FlowControl::None:     return "None";
        case SerialConfig::FlowControl::Hardware: return "Hardware";
        case SerialConfig::FlowControl::Software: return "Software";
        default:                                  return "None";
    }
}

static SerialConfig::FlowControl flowControlFromString(const std::string& s) {
    if (s == "None")     return SerialConfig::FlowControl::None;
    if (s == "Hardware") return SerialConfig::FlowControl::Hardware;
    if (s == "Software") return SerialConfig::FlowControl::Software;
    throw std::invalid_argument("Unknown flow control: " + s);
}

static nlohmann::json serialToJson(const SerialConfig& c) {
    return nlohmann::json{
        {"device",        c.device},
        {"baudRate",      c.baudRate},
        {"dataBits",      c.dataBits},
        {"parity",        parityToString(c.parity)},
        {"stopBits",      stopBitsToString(c.stopBits)},
        {"flowControl",   flowControlToString(c.flowControl)},
        {"readTimeoutMs", c.readTimeoutMs}
    };
}

static SerialConfig serialFromJson(const nlohmann::json& j) {
    SerialConfig c;
    c.device        = j.at("device").get<std::string>();
    c.baudRate      = j.at("baudRate").get<uint32_t>();
    c.dataBits      = j.at("dataBits").get<uint8_t>();
    c.parity        = parityFromString(j.at("parity").get<std::string>());
    c.stopBits      = stopBitsFromString(j.at("stopBits").get<std::string>());
    c.flowControl   = flowControlFromString(j.at("flowControl").get<std::string>());
    c.readTimeoutMs = j.at("readTimeoutMs").get<int>();
    return c;
}

// ---------------------------------------------------------------------------
// TelnetConfig helpers
// ---------------------------------------------------------------------------
static nlohmann::json telnetToJson(const TelnetConfig& c) {
    return nlohmann::json{
        {"host",             c.host},
        {"port",             c.port},
        {"connectTimeoutMs", c.connectTimeoutMs},
        {"suppressGoAhead",  c.suppressGoAhead}
    };
}

static TelnetConfig telnetFromJson(const nlohmann::json& j) {
    TelnetConfig c;
    c.host             = j.at("host").get<std::string>();
    c.port             = j.at("port").get<uint16_t>();
    c.connectTimeoutMs = j.at("connectTimeoutMs").get<int>();
    c.suppressGoAhead  = j.at("suppressGoAhead").get<bool>();
    return c;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// SessionConfig public methods
// ---------------------------------------------------------------------------
nlohmann::json SessionConfig::toJson() const {
    nlohmann::json j;
    j["name"] = name;
    j["type"] = sessionTypeToString(type);
    if (ssh)    j["ssh"]    = sshToJson(ssh.value());
    if (serial) j["serial"] = serialToJson(serial.value());
    if (telnet) j["telnet"] = telnetToJson(telnet.value());
    return j;
}

SessionConfig SessionConfig::fromJson(const nlohmann::json& j) {
    SessionConfig cfg;
    cfg.name = j.at("name").get<std::string>();
    cfg.type = sessionTypeFromString(j.at("type").get<std::string>());
    if (j.contains("ssh"))    cfg.ssh    = sshFromJson(j.at("ssh"));
    if (j.contains("serial")) cfg.serial = serialFromJson(j.at("serial"));
    if (j.contains("telnet")) cfg.telnet = telnetFromJson(j.at("telnet"));
    return cfg;
}

} // namespace tcm
