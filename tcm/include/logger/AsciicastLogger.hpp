#pragma once

#include <cstdint>
#include <cstdio>
#include <chrono>
#include <string>
#include <vector>

namespace tcm {

class AsciicastLogger {
public:
    AsciicastLogger() = default;
    ~AsciicastLogger();

    // Start logging. Creates file at logDir/sessionName_YYYYMMDD_HHMMSS.cast
    bool start(const std::string& sessionName, int termWidth = 220, int termHeight = 50,
               const std::string& logDir = "");  // empty = ~/.local/share/tcm/logs

    // Log output data (device → screen)
    void logOutput(const uint8_t* data, size_t len);
    void logOutput(const std::vector<uint8_t>& data);

    // Log input data (user keystrokes)
    void logInput(const uint8_t* data, size_t len);

    // Stop logging (flush + close)
    void stop();

    bool isLogging() const;
    const std::string& currentFilePath() const;

private:
    FILE* m_file = nullptr;
    std::chrono::steady_clock::time_point m_startTime;
    std::string m_sessionName;
    std::string m_currentPath;
    std::string m_logDir;
    bool m_logging = false;
    size_t m_bytesWritten = 0;
    int m_partNumber = 1;
    int m_termWidth  = 220;
    int m_termHeight = 50;
    static constexpr size_t MAX_FILE_SIZE = 50 * 1024 * 1024; // 50 MB

    void writeHeader(int width, int height);
    void writeEvent(const char* type, const uint8_t* data, size_t len);
    std::string jsonEscape(const uint8_t* data, size_t len);
    double elapsedSeconds() const;
    std::string buildFilePath(const std::string& logDir, const std::string& sessionName, int part = 0);
    void rotateFile(int width, int height);
};

} // namespace tcm
