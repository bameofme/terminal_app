#include "tui/SessionForm.hpp"

#include <ncurses.h>

#include <stdexcept>
#include <string>

#include "tui/Renderer.hpp"

namespace tcm {

namespace {

const char* typeToStr(SessionType t) {
    switch (t) {
        case SessionType::SSH:    return "SSH";
        case SessionType::Serial: return "Serial";
        case SessionType::Telnet: return "Telnet";
    }
    return "SSH";
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

SessionForm::SessionForm(EventBus& bus) : m_bus(bus) {
    buildFields();
}

// ---------------------------------------------------------------------------
// Field builders
// ---------------------------------------------------------------------------

void SessionForm::buildSshFields() {
    m_fields.push_back({"Host",     "",    false, "hostname or IP"});
    m_fields.push_back({"Port",     "22",  false, "22"});
    m_fields.push_back({"Username", "",    false, "username"});
    m_fields.push_back({"Password", "",    true,  "(optional)"});
    m_fields.push_back({"Key Path", "",    false, "~/.ssh/id_rsa (optional)"});
}

void SessionForm::buildSerialFields() {
    m_fields.push_back({"Device",    "",       false, "/dev/ttyUSB0"});
    m_fields.push_back({"Baud Rate", "115200", false, "115200"});
}

void SessionForm::buildTelnetFields() {
    m_fields.push_back({"Host", "",   false, "hostname or IP"});
    m_fields.push_back({"Port", "23", false, "23"});
}

void SessionForm::buildFields() {
    // Preserve Name value across rebuilds (e.g. type changes)
    std::string savedName;
    if (!m_fields.empty()) {
        savedName = m_fields[0].value;
    }

    m_fields.clear();

    // [0] Name
    m_fields.push_back({"Name", savedName, false, "session name"});

    // [1] Type (read-only display; cycled via Tab)
    m_fields.push_back({"Type", typeToStr(m_type), false, "SSH/Serial/Telnet"});

    switch (m_type) {
        case SessionType::SSH:    buildSshFields();    break;
        case SessionType::Serial: buildSerialFields(); break;
        case SessionType::Telnet: buildTelnetFields(); break;
    }

    // Clamp selected field to valid range
    if (m_selectedField >= static_cast<int>(m_fields.size())) {
        m_selectedField = static_cast<int>(m_fields.size()) - 1;
    }
}

void SessionForm::selectNextType() {
    switch (m_type) {
        case SessionType::SSH:    m_type = SessionType::Serial; break;
        case SessionType::Serial: m_type = SessionType::Telnet; break;
        case SessionType::Telnet: m_type = SessionType::SSH;    break;
    }
    buildFields();
    m_selectedField = 1; // stay on Type field
}

int SessionForm::getCurrentCursorPos() const {
    if (m_selectedField < static_cast<int>(m_fields.size())) {
        return static_cast<int>(m_fields[static_cast<size_t>(m_selectedField)].value.size());
    }
    return 0;
}

// ---------------------------------------------------------------------------
// IRenderable
// ---------------------------------------------------------------------------

void SessionForm::render(Renderer& renderer) {
    auto [cols, rows] = renderer.getSize();

    const int formW = 60;
    const int formH = static_cast<int>(m_fields.size()) + 4;

    int x = (cols - formW) / 2;
    int y = (rows - formH) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    renderer.drawBox(x, y, formW, formH);
    renderer.drawText(x + 2, y + 1, "Session Configuration", ColorPair::TitleBar);

    for (int i = 0; i < static_cast<int>(m_fields.size()); ++i) {
        const auto& field    = m_fields[static_cast<size_t>(i)];
        bool        selected = (i == m_selectedField);

        std::string label = field.label + ": ";
        renderer.drawText(x + 2, y + 2 + i, label,
                          selected ? ColorPair::Highlighted : ColorPair::Normal);

        std::string display;
        if (field.value.empty()) {
            display = field.placeholder;
        } else if (field.isPassword) {
            display = std::string(field.value.size(), '*');
        } else {
            display = field.value;
        }

        int labelLen = static_cast<int>(label.size());
        renderer.drawTextTruncated(x + 2 + labelLen, y + 2 + i,
                                   formW - 4 - labelLen, display,
                                   selected ? ColorPair::Highlighted : ColorPair::Normal);
    }

    if (!m_errorMsg.empty()) {
        renderer.drawText(x + 2, y + formH - 2, "Error: " + m_errorMsg,
                          ColorPair::Error);
    } else {
        renderer.drawText(x + 2, y + formH - 2,
                          "Enter: confirm  Esc: cancel  Tab: next/cycle type",
                          ColorPair::StatusBar);
    }
}

bool SessionForm::handleKey(int key) {
    constexpr int ESC           = 27;
    constexpr int TAB           = 9;
    constexpr int ENTER_LF      = 10;
    constexpr int ENTER_CR      = 13;
    constexpr int BACKSPACE_DEL = 127;

    if (key == ESC) {
        m_errorMsg.clear();
        m_result = FormResult::Cancelled;
        return true;
    }

    if (key == ENTER_LF || key == ENTER_CR || key == KEY_ENTER) {
        if (isValid()) {
            m_errorMsg.clear();
            m_result = FormResult::Confirmed;
        } else {
            m_errorMsg = getValidationError();
        }
        return true;
    }

    // Tab on Type field (index 1) → cycle protocol type
    if (key == TAB && m_selectedField == 1) {
        selectNextType();
        return true;
    }

    if (key == KEY_DOWN || key == TAB) {
        if (m_selectedField < static_cast<int>(m_fields.size()) - 1) {
            ++m_selectedField;
        }
        return true;
    }

    if (key == KEY_UP) {
        if (m_selectedField > 0) {
            --m_selectedField;
        }
        return true;
    }

    if (key == KEY_BACKSPACE || key == BACKSPACE_DEL) {
        // Type field (index 1) is read-only
        if (m_selectedField != 1) {
            auto& val = m_fields[static_cast<size_t>(m_selectedField)].value;
            if (!val.empty()) { val.pop_back(); m_errorMsg.clear(); }
        }
        return true;
    }

    // Printable ASCII
    if (key >= 32 && key < 127) {
        if (m_selectedField != 1) {
            m_fields[static_cast<size_t>(m_selectedField)].value +=
                static_cast<char>(key);
            m_errorMsg.clear();
        }
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// Config I/O
// ---------------------------------------------------------------------------

void SessionForm::loadConfig(const SessionConfig& config) {
    m_type = config.type;
    buildFields();

    m_fields[0].value = config.name;
    // m_fields[1].value already set to typeToStr(m_type) in buildFields()

    switch (m_type) {
        case SessionType::SSH:
            if (config.ssh) {
                m_fields[2].value = config.ssh->host;
                m_fields[3].value = std::to_string(config.ssh->port);
                m_fields[4].value = config.ssh->username;
                m_fields[5].value = config.ssh->password;
                m_fields[6].value = config.ssh->privateKeyPath;
            }
            break;
        case SessionType::Serial:
            if (config.serial) {
                m_fields[2].value = config.serial->device;
                m_fields[3].value = std::to_string(config.serial->baudRate);
            }
            break;
        case SessionType::Telnet:
            if (config.telnet) {
                m_fields[2].value = config.telnet->host;
                m_fields[3].value = std::to_string(config.telnet->port);
            }
            break;
    }

    m_selectedField = 0;
    m_result        = FormResult::None;
}

SessionConfig SessionForm::getConfig() const {
    SessionConfig cfg;
    cfg.name = m_fields[0].value;
    cfg.type = m_type;

    switch (m_type) {
        case SessionType::SSH: {
            SshConfig ssh;
            if (m_fields.size() > 2) ssh.host     = m_fields[2].value;
            if (m_fields.size() > 3) {
                try   { ssh.port = static_cast<uint16_t>(std::stoi(m_fields[3].value)); }
                catch (...) { ssh.port = 22; }
            }
            if (m_fields.size() > 4) ssh.username       = m_fields[4].value;
            if (m_fields.size() > 5) ssh.password       = m_fields[5].value;
            if (m_fields.size() > 6) ssh.privateKeyPath = m_fields[6].value;
            cfg.ssh = ssh;
            break;
        }
        case SessionType::Serial: {
            SerialConfig serial;
            if (m_fields.size() > 2) serial.device = m_fields[2].value;
            if (m_fields.size() > 3) {
                try   { serial.baudRate = static_cast<uint32_t>(std::stoi(m_fields[3].value)); }
                catch (...) { serial.baudRate = 115200; }
            }
            cfg.serial = serial;
            break;
        }
        case SessionType::Telnet: {
            TelnetConfig telnet;
            if (m_fields.size() > 2) telnet.host = m_fields[2].value;
            if (m_fields.size() > 3) {
                try   { telnet.port = static_cast<uint16_t>(std::stoi(m_fields[3].value)); }
                catch (...) { telnet.port = 23; }
            }
            cfg.telnet = telnet;
            break;
        }
    }

    return cfg;
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

FormResult SessionForm::getResult() const {
    return m_result;
}

void SessionForm::reset() {
    m_type          = SessionType::SSH;
    m_selectedField = 0;
    m_result        = FormResult::None;
    m_errorMsg.clear();
    m_fields.clear(); // clear before buildFields() so savedName = ""
    buildFields();
}

std::string SessionForm::getValidationError() const {
    if (m_fields.empty() || m_fields[0].value.empty())
        return "Name is required";
    switch (m_type) {
        case SessionType::SSH:
            if (m_fields.size() > 2 && m_fields[2].value.empty()) return "Host is required";
            if (m_fields.size() > 4 && m_fields[4].value.empty()) return "Username is required";
            break;
        case SessionType::Serial:
            if (m_fields.size() > 2 && m_fields[2].value.empty()) return "Device is required";
            break;
        case SessionType::Telnet:
            if (m_fields.size() > 2 && m_fields[2].value.empty()) return "Host is required";
            break;
    }
    return "";
}

bool SessionForm::isValid() const {
    if (m_fields.empty() || m_fields[0].value.empty()) {
        return false; // Name required
    }
    switch (m_type) {
        case SessionType::SSH:
            if (m_fields.size() > 2 && m_fields[2].value.empty()) return false; // Host
            if (m_fields.size() > 4 && m_fields[4].value.empty()) return false; // Username
            break;
        case SessionType::Serial:
            if (m_fields.size() > 2 && m_fields[2].value.empty()) return false; // Device
            break;
        case SessionType::Telnet:
            if (m_fields.size() > 2 && m_fields[2].value.empty()) return false; // Host
            break;
    }
    return true;
}

} // namespace tcm
