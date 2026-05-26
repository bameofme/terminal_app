#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <string>

#include "core/IConnection.hpp"
#include "core/EventBus.hpp"
#include "macro/Recipe.hpp"

namespace tcm {

enum class MacroState { Idle, Running, Done, Error };

class MacroEngine {
public:
    MacroEngine(IConnection* conn, EventBus& bus, const std::string& sessionId);

    // Start executing recipe with given variable values
    void start(const Recipe& recipe, const std::map<std::string, std::string>& vars = {});

    // Feed data from connection (call when data received)
    void feed(const uint8_t* data, size_t len);
    void feed(const std::string& data);

    // Stop recipe execution
    void stop();

    // Reset to idle state
    void reset();

    MacroState getState() const;
    int currentStep() const;
    int totalSteps() const;

private:
    IConnection* m_conn;
    EventBus& m_bus;
    std::string m_sessionId;
    Recipe m_recipe;
    std::map<std::string, std::string> m_vars;
    MacroState m_state = MacroState::Idle;
    int m_currentStep = 0;
    std::string m_buffer;
    std::chrono::steady_clock::time_point m_stepStartTime;

    void checkStep();
    void advanceStep();
    void executeStep(const Step& step);
    std::string interpolate(const std::string& tmpl) const;
    bool checkTimeout();
};

} // namespace tcm
