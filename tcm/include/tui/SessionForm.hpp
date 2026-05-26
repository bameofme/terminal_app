#pragma once

#include <string>
#include <vector>

#include "core/EventBus.hpp"
#include "core/Session.hpp"
#include "protocol/SessionConfig.hpp"
#include "tui/IRenderable.hpp"

namespace tcm {

struct FormField {
    std::string label;
    std::string value;
    bool        isPassword  = false;
    std::string placeholder;
};

enum class FormResult { None, Confirmed, Cancelled };

class SessionForm : public IRenderable {
public:
    explicit SessionForm(EventBus& bus);

    void render(Renderer& renderer) override;
    bool handleKey(int key) override;

    // Set form for editing existing session
    void loadConfig(const SessionConfig& config);

    // Get result config after confirmation
    SessionConfig getConfig() const;

    // Get form result
    FormResult getResult() const;

    // Reset form to blank
    void reset();

    // Validation
    bool        isValid() const;
    std::string getValidationError() const;

private:
    EventBus&              m_bus;
    int                    m_selectedField = 0;
    std::vector<FormField> m_fields;
    FormResult             m_result = FormResult::None;
    SessionType            m_type   = SessionType::SSH;
    std::string            m_errorMsg;

    void buildFields();
    void buildSshFields();
    void buildSerialFields();
    void buildTelnetFields();
    void selectNextType();
    int  getCurrentCursorPos() const;
};

} // namespace tcm
