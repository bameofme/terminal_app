#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "logger/AsciicastLogger.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class AsciicastLoggerTest : public ::testing::Test {
protected:
    std::string       m_testDir;
    tcm::AsciicastLogger m_logger;

    void SetUp() override {
        // Unique temp dir per test run (pid + monotonic tick)
        auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
        m_testDir = "/tmp/tcm_test_logs_" + std::to_string(getpid()) +
                    "_" + std::to_string(tick);
        fs::create_directories(m_testDir);
    }

    void TearDown() override {
        m_logger.stop();
        fs::remove_all(m_testDir);
    }

    // Read non-empty lines from a file
    std::vector<std::string> readLines(const std::string& path) {
        std::vector<std::string> lines;
        std::ifstream f(path);
        std::string line;
        while (std::getline(f, line)) {
            if (!line.empty()) {
                lines.push_back(line);
            }
        }
        return lines;
    }
};

// ---------------------------------------------------------------------------
// Test 1: start() creates file and sets isLogging() = true
// ---------------------------------------------------------------------------
TEST_F(AsciicastLoggerTest, StartCreatesFileAndSetsLogging) {
    EXPECT_FALSE(m_logger.isLogging());
    bool ok = m_logger.start("test-session", 80, 24, m_testDir);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(m_logger.isLogging());
    EXPECT_TRUE(fs::exists(m_logger.currentFilePath()));
}

// ---------------------------------------------------------------------------
// Test 2: stop() after start() sets isLogging() = false
// ---------------------------------------------------------------------------
TEST_F(AsciicastLoggerTest, StopSetsLoggingFalse) {
    m_logger.start("test-session", 80, 24, m_testDir);
    EXPECT_TRUE(m_logger.isLogging());
    m_logger.stop();
    EXPECT_FALSE(m_logger.isLogging());
}

// ---------------------------------------------------------------------------
// Test 3: start() writes a valid asciicast v2 JSON header
// ---------------------------------------------------------------------------
TEST_F(AsciicastLoggerTest, StartWritesValidHeader) {
    m_logger.start("my-session", 120, 40, m_testDir);
    std::string path = m_logger.currentFilePath();
    m_logger.stop();

    auto lines = readLines(path);
    ASSERT_GE(lines.size(), 1u);

    json header = json::parse(lines[0]);
    EXPECT_EQ(header["version"].get<int>(), 2);
    EXPECT_EQ(header["width"].get<int>(), 120);
    EXPECT_EQ(header["height"].get<int>(), 40);
    EXPECT_TRUE(header.contains("timestamp"));
    EXPECT_EQ(header["title"].get<std::string>(), "my-session");
}

// ---------------------------------------------------------------------------
// Test 4: logOutput() appends an event with type "o" and correct data
// ---------------------------------------------------------------------------
TEST_F(AsciicastLoggerTest, LogOutputAppendsEvent) {
    m_logger.start("test-session", 80, 24, m_testDir);

    std::string data = "hello";
    m_logger.logOutput(reinterpret_cast<const uint8_t*>(data.c_str()), data.size());
    std::string path = m_logger.currentFilePath();
    m_logger.stop();

    auto lines = readLines(path);
    ASSERT_GE(lines.size(), 2u);  // header + event

    json event = json::parse(lines[1]);
    ASSERT_TRUE(event.is_array());
    ASSERT_EQ(event.size(), 3u);
    EXPECT_GE(event[0].get<double>(), 0.0);
    EXPECT_EQ(event[1].get<std::string>(), "o");
    EXPECT_EQ(event[2].get<std::string>(), "hello");
}

// ---------------------------------------------------------------------------
// Test 5: logInput() appends an event with type "i"
// ---------------------------------------------------------------------------
TEST_F(AsciicastLoggerTest, LogInputAppendsInputEvent) {
    m_logger.start("test-session", 80, 24, m_testDir);

    std::string data = "ls\n";
    m_logger.logInput(reinterpret_cast<const uint8_t*>(data.c_str()), data.size());
    std::string path = m_logger.currentFilePath();
    m_logger.stop();

    auto lines = readLines(path);
    ASSERT_GE(lines.size(), 2u);

    json event = json::parse(lines[1]);
    ASSERT_TRUE(event.is_array());
    EXPECT_EQ(event[1].get<std::string>(), "i");
}

// ---------------------------------------------------------------------------
// Test 6: timestamps are monotonically increasing across consecutive events
// ---------------------------------------------------------------------------
TEST_F(AsciicastLoggerTest, TimestampsAreMonotonic) {
    m_logger.start("test-session", 80, 24, m_testDir);

    const uint8_t byte = 'x';
    m_logger.logOutput(&byte, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    m_logger.logOutput(&byte, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    m_logger.logOutput(&byte, 1);

    std::string path = m_logger.currentFilePath();
    m_logger.stop();

    auto lines = readLines(path);
    ASSERT_GE(lines.size(), 4u);  // header + 3 events

    double prev = -1.0;
    for (size_t i = 1; i < lines.size(); ++i) {
        json ev = json::parse(lines[i]);
        double ts = ev[0].get<double>();
        EXPECT_GT(ts, prev) << "Non-monotonic timestamp at event " << i;
        prev = ts;
    }
}

// ---------------------------------------------------------------------------
// Test 7: stop() flushes and closes the file (file is readable after stop)
// ---------------------------------------------------------------------------
TEST_F(AsciicastLoggerTest, StopFlushesAndClosesFile) {
    m_logger.start("test-session", 80, 24, m_testDir);

    std::string data = "some output";
    m_logger.logOutput(reinterpret_cast<const uint8_t*>(data.c_str()), data.size());

    std::string path = m_logger.currentFilePath();
    m_logger.stop();

    EXPECT_TRUE(fs::exists(path));
    EXPECT_GT(fs::file_size(path), static_cast<uintmax_t>(0));

    auto lines = readLines(path);
    EXPECT_GE(lines.size(), 2u);
}

// ---------------------------------------------------------------------------
// Test 8: currentFilePath() returns the correct file path
// ---------------------------------------------------------------------------
TEST_F(AsciicastLoggerTest, CurrentFilePathIsCorrect) {
    m_logger.start("mysession", 80, 24, m_testDir);
    std::string path = m_logger.currentFilePath();
    m_logger.stop();

    EXPECT_FALSE(path.empty());
    EXPECT_TRUE(fs::exists(path));
    // Path must be inside the test directory
    EXPECT_EQ(path.substr(0, m_testDir.size()), m_testDir);
}

// ---------------------------------------------------------------------------
// Test 9: multiple stop() calls must not crash
// ---------------------------------------------------------------------------
TEST_F(AsciicastLoggerTest, MultipleStopCallsAreSafe) {
    m_logger.start("test-session", 80, 24, m_testDir);
    EXPECT_NO_THROW(m_logger.stop());
    EXPECT_NO_THROW(m_logger.stop());
    EXPECT_NO_THROW(m_logger.stop());
    EXPECT_FALSE(m_logger.isLogging());
}

// ---------------------------------------------------------------------------
// Test 10: file path contains session name + YYYYMMDD timestamp pattern
// ---------------------------------------------------------------------------
TEST_F(AsciicastLoggerTest, FilePathContainsSessionNameAndTimestamp) {
    m_logger.start("my-app-session", 80, 24, m_testDir);
    std::string path = m_logger.currentFilePath();
    m_logger.stop();

    // Must contain the session name
    EXPECT_NE(path.find("my-app-session"), std::string::npos);

    // Must end with .cast
    ASSERT_GE(path.size(), 5u);
    EXPECT_EQ(path.substr(path.size() - 5), ".cast");

    // Must contain a run of 8 consecutive digits (YYYYMMDD)
    bool hasTimestamp = false;
    for (size_t i = 0; i + 8 <= path.size(); ++i) {
        bool allDigits = true;
        for (size_t j = i; j < i + 8; ++j) {
            if (!isdigit(static_cast<unsigned char>(path[j]))) {
                allDigits = false;
                break;
            }
        }
        if (allDigits) {
            hasTimestamp = true;
            break;
        }
    }
    EXPECT_TRUE(hasTimestamp) << "No YYYYMMDD pattern found in: " << path;
}
