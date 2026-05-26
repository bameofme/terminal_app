#include "macro/Recipe.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace tcm {

Recipe Recipe::loadFromString(const std::string& jsonStr) {
    using json = nlohmann::json;

    json j;
    try {
        j = json::parse(jsonStr);
    } catch (const json::exception& e) {
        throw std::runtime_error(std::string("JSON parse error: ") + e.what());
    }

    if (!j.contains("name") || !j["name"].is_string()) {
        throw std::runtime_error("Recipe missing required field 'name'");
    }
    if (!j.contains("steps") || !j["steps"].is_array()) {
        throw std::runtime_error("Recipe missing required field 'steps'");
    }

    Recipe recipe;
    recipe.name = j["name"].get<std::string>();

    if (j.contains("description") && j["description"].is_string()) {
        recipe.description = j["description"].get<std::string>();
    }

    for (const auto& stepJson : j["steps"]) {
        if (!stepJson.contains("expect") || !stepJson["expect"].is_string()) {
            throw std::runtime_error("Step missing required field 'expect'");
        }

        Step step;
        step.expect = stepJson["expect"].get<std::string>();

        if (stepJson.contains("send") && stepJson["send"].is_string()) {
            step.send = stepJson["send"].get<std::string>();
        }

        if (stepJson.contains("timeout_ms") && stepJson["timeout_ms"].is_number()) {
            step.timeoutMs = stepJson["timeout_ms"].get<int>();
        }

        if (stepJson.contains("on_timeout") && stepJson["on_timeout"].is_string()) {
            const std::string action = stepJson["on_timeout"].get<std::string>();
            if (action == "abort") {
                step.onTimeout = OnTimeoutAction::Abort;
            } else if (action == "skip") {
                step.onTimeout = OnTimeoutAction::Skip;
            } else if (action == "continue") {
                step.onTimeout = OnTimeoutAction::Continue;
            } else {
                throw std::runtime_error("Unknown on_timeout action: " + action);
            }
        }

        recipe.steps.push_back(std::move(step));
    }

    if (j.contains("variables") && j["variables"].is_object()) {
        for (const auto& [key, val] : j["variables"].items()) {
            recipe.variables[key] = val.is_string() ? val.get<std::string>() : "";
        }
    }

    return recipe;
}

Recipe Recipe::load(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open recipe file: " + filePath);
    }
    std::ostringstream oss;
    oss << file.rdbuf();
    return loadFromString(oss.str());
}

bool Recipe::isValid() const {
    if (name.empty()) return false;
    if (steps.empty()) return false;
    for (const auto& step : steps) {
        if (step.expect.empty()) return false;
    }
    return true;
}

std::string Recipe::validationError() const {
    if (name.empty()) return "Recipe name is empty";
    if (steps.empty()) return "Recipe has no steps";
    for (size_t i = 0; i < steps.size(); ++i) {
        if (steps[i].expect.empty()) {
            return "Step " + std::to_string(i) + " has empty expect pattern";
        }
    }
    return "";
}

} // namespace tcm
