#include "macro/MacroEngine.hpp"

#include <regex>
#include <span>
#include <string>

namespace tcm {

MacroEngine::MacroEngine(IConnection* conn, EventBus& bus, const std::string& sessionId)
    : m_conn(conn), m_bus(bus), m_sessionId(sessionId) {}

void MacroEngine::start(const Recipe& recipe, const std::map<std::string, std::string>& vars) {
    m_recipe = recipe;
    // Start with recipe defaults, then override with supplied vars
    m_vars = recipe.variables;
    for (const auto& [k, v] : vars) {
        m_vars[k] = v;
    }
    m_state = MacroState::Running;
    m_currentStep = 0;
    m_buffer.clear();
    m_stepStartTime = std::chrono::steady_clock::now();

    m_bus.publish(MacroTriggeredEvent{recipe.name, m_sessionId});
}

void MacroEngine::feed(const uint8_t* data, size_t len) {
    m_buffer.append(reinterpret_cast<const char*>(data), len);
    checkStep();
}

void MacroEngine::feed(const std::string& data) {
    m_buffer += data;
    checkStep();
}

void MacroEngine::stop() {
    if (m_state == MacroState::Running) {
        m_state = MacroState::Error;
        m_bus.publish(MacroFailedEvent{m_sessionId, "Stopped by user"});
    }
}

void MacroEngine::reset() {
    m_state = MacroState::Idle;
    m_currentStep = 0;
    m_buffer.clear();
}

MacroState MacroEngine::getState() const { return m_state; }
int MacroEngine::currentStep() const { return m_currentStep; }
int MacroEngine::totalSteps() const { return static_cast<int>(m_recipe.steps.size()); }

void MacroEngine::checkStep() {
    if (m_state != MacroState::Running) return;
    if (m_currentStep >= static_cast<int>(m_recipe.steps.size())) return;

    if (checkTimeout()) return;

    const Step& step = m_recipe.steps[m_currentStep];
    try {
        std::regex re(step.expect);
        if (std::regex_search(m_buffer, re)) {
            executeStep(step);
            advanceStep();
        }
    } catch (const std::regex_error&) {
        // Invalid regex — treat as no match
    }
}

void MacroEngine::executeStep(const Step& step) {
    const std::string toSend = interpolate(step.send);
    if (!toSend.empty() && m_conn) {
        const std::span<const uint8_t> sp(
            reinterpret_cast<const uint8_t*>(toSend.data()), toSend.size());
        m_conn->send(sp);
    }
    m_buffer.clear();
}

void MacroEngine::advanceStep() {
    m_currentStep++;
    if (m_currentStep >= static_cast<int>(m_recipe.steps.size())) {
        m_state = MacroState::Done;
        m_bus.publish(MacroDoneEvent{m_sessionId, true});
    } else {
        m_stepStartTime = std::chrono::steady_clock::now();
    }
}

std::string MacroEngine::interpolate(const std::string& tmpl) const {
    std::string result = tmpl;
    for (const auto& [key, val] : m_vars) {
        const std::string placeholder = "{" + key + "}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.size(), val);
            pos += val.size();
        }
    }
    return result;
}

bool MacroEngine::checkTimeout() {
    if (m_currentStep >= static_cast<int>(m_recipe.steps.size())) return false;

    const Step& step = m_recipe.steps[m_currentStep];
    const auto elapsed = std::chrono::steady_clock::now() - m_stepStartTime;
    const auto elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    if (elapsedMs > step.timeoutMs) {
        switch (step.onTimeout) {
            case OnTimeoutAction::Abort:
                m_state = MacroState::Error;
                m_bus.publish(MacroFailedEvent{
                    m_sessionId,
                    "Timeout on step " + std::to_string(m_currentStep)});
                return true;
            case OnTimeoutAction::Skip:
                advanceStep();
                return true;
            case OnTimeoutAction::Continue:
                // Fall through to pattern matching
                return false;
        }
    }
    return false;
}

} // namespace tcm
