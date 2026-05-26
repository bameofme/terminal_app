#pragma once

#include <map>
#include <string>
#include <vector>

namespace tcm {

enum class OnTimeoutAction { Abort, Skip, Continue };

struct Step {
    std::string expect;      // regex pattern to wait for
    std::string send;        // string to send (may contain {var})
    int timeoutMs = 5000;
    OnTimeoutAction onTimeout = OnTimeoutAction::Abort;
};

struct Recipe {
    std::string name;
    std::string description;
    std::vector<Step> steps;
    std::map<std::string, std::string> variables;  // default values

    // Load from file
    static Recipe load(const std::string& filePath);

    // Load from JSON string (easier to test)
    static Recipe loadFromString(const std::string& json);

    // Validate recipe
    bool isValid() const;
    std::string validationError() const;
};

} // namespace tcm
