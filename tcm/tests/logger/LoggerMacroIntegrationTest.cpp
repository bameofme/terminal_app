/**
 * Integration Test: Logger + MacroEngine
 *
 * Tests:
 *   1. AsciicastLogger file creation: start(), logOutput(), stop() → file exists & valid JSON
 *   2. MacroEngine + AsciicastLogger: feed data → macro matches → logger logs input
 *   3. Recipe load from file: write temp JSON file, Recipe::load() → verify struct
 */

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "core/EventBus.hpp"
#include "core/IConnection.hpp"
#include "logger/AsciicastLogger.hpp"
#include "macro/MacroEngine.hpp"
#include "macro/Recipe.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Minimal stub connection (does nothing) for MacroEngine
// ---------------------------------------------------------------------------
class StubConnection : public tcm::IConnection {
public:
    int  connect()                              noexcept override { return 0; }
    int  send(std::span<const uint8_t>)         noexcept override { return 0; }
    int  recv(std::span<uint8_t>)               noexcept override { return 0; }
    void disconnect()                           noexcept override {}
    int  getFd()                          const noexcept override { return -1; }
    tcm::ConnectionState getState()       const noexcept override {
        return tcm::ConnectionState::Disconnected;
    }
};

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class LoggerMacroIntegrationTest : public ::testing::Test {
protected:
    std::string m_testDir;

    void SetUp() override {
        auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
        m_testDir = "/tmp/tcm_lm_int_" + std::to_string(getpid()) +
                    "_" + std::to_string(tick);
        fs::create_directories(m_testDir);
    }

    void TearDown() override {
        fs::remove_all(m_testDir);
    }

    // Read all non-empty lines from a file
    std::vector<std::string> readLines(const std::string& path) {
        std::vector<std::string> lines;
        std::ifstream f(path);
        std::string   line;
        while (std::getline(f, line))
            if (!line.empty()) lines.push_back(line);
        return lines;
    }
};

// ---------------------------------------------------------------------------
// Test 1: AsciicastLogger file creation — valid header written
// ---------------------------------------------------------------------------
TEST_F(LoggerMacroIntegrationTest, AsciicastLoggerFileCreation) {
    tcm::AsciicastLogger logger;

    bool started = logger.start("int-session", 80, 24, m_testDir);
    ASSERT_TRUE(started);
    EXPECT_TRUE(logger.isLogging());
    EXPECT_TRUE(fs::exists(logger.currentFilePath()));

    const uint8_t output[] = "Hello, World!\r\n";
    logger.logOutput(output, sizeof(output) - 1);
    logger.stop();

    EXPECT_FALSE(logger.isLogging());

    auto lines = readLines(logger.currentFilePath());
    ASSERT_GE(lines.size(), 2u) << "Expected header + at least 1 event";

    // First line must be valid JSON header with "version":2
    json header = json::parse(lines[0]);
    EXPECT_EQ(header["version"].get<int>(), 2);
    EXPECT_EQ(header["width"].get<int>(),  80);
    EXPECT_EQ(header["height"].get<int>(), 24);

    // Second line must be a valid event array [timestamp, "o", data]
    json event = json::parse(lines[1]);
    ASSERT_TRUE(event.is_array());
    ASSERT_GE(event.size(), 3u);
    EXPECT_EQ(event[1].get<std::string>(), "o");
}

// ---------------------------------------------------------------------------
// Test 2: MacroEngine + AsciicastLogger — feed data through, logger stays active
// ---------------------------------------------------------------------------
TEST_F(LoggerMacroIntegrationTest, MacroEngineWithLogger) {
    tcm::EventBus         bus;
    StubConnection        conn;
    tcm::MacroEngine      engine(&conn, bus, "macro-log-sess");
    tcm::AsciicastLogger  logger;

    ASSERT_TRUE(logger.start("macro-test", 80, 24, m_testDir));

    // Build a simple recipe: expect "login:" then send "admin"
    tcm::Recipe recipe;
    recipe.name = "test-recipe";
    {
        tcm::Step step;
        step.expect    = "login:";
        step.send      = "admin\n";
        step.timeoutMs = 500;
        step.onTimeout = tcm::OnTimeoutAction::Abort;
        recipe.steps.push_back(step);
    }

    engine.start(recipe);
    EXPECT_EQ(engine.getState(), tcm::MacroState::Running);

    // Feed matching data
    const std::string triggerData = "login: ";
    engine.feed(triggerData);

    // Log what we fed
    const uint8_t* raw = reinterpret_cast<const uint8_t*>(triggerData.data());
    logger.logInput(raw, triggerData.size());

    EXPECT_TRUE(logger.isLogging());
    logger.stop();

    // File must exist and have content
    EXPECT_TRUE(fs::exists(logger.currentFilePath()));
    auto lines = readLines(logger.currentFilePath());
    ASSERT_GE(lines.size(), 1u);
}

// ---------------------------------------------------------------------------
// Test 3: Recipe load from file — write temp JSON, verify struct
// ---------------------------------------------------------------------------
TEST_F(LoggerMacroIntegrationTest, RecipeLoadFromFile) {
    // Write a minimal recipe JSON
    std::string recipePath = m_testDir + "/test_recipe.json";
    {
        json j;
        j["name"]        = "file-recipe";
        j["description"] = "Loaded from file";
        j["steps"] = json::array({
            {{"expect", "\\$"}, {"send", "ls -la\n"}, {"timeout_ms", 3000},
             {"on_timeout", "abort"}}
        });
        j["variables"] = json::object();

        std::ofstream f(recipePath);
        f << j.dump(2);
    }

    tcm::Recipe recipe = tcm::Recipe::load(recipePath);

    EXPECT_EQ(recipe.name,        "file-recipe");
    EXPECT_EQ(recipe.description, "Loaded from file");
    ASSERT_EQ(recipe.steps.size(), 1u);
    EXPECT_EQ(recipe.steps[0].expect, "\\$");
    EXPECT_EQ(recipe.steps[0].send,   "ls -la\n");
    EXPECT_EQ(recipe.steps[0].timeoutMs, 3000);
}
