#include "logger/AsciicastLogger.hpp"

#include <cstdlib>
#include <ctime>
#include <filesystem>

namespace tcm {

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

AsciicastLogger::~AsciicastLogger() {
    stop();
}

bool AsciicastLogger::start(const std::string& sessionName, int termWidth, int termHeight,
                             const std::string& logDir) {
    if (m_logging) {
        stop();
    }

    m_sessionName  = sessionName;
    m_termWidth    = termWidth;
    m_termHeight   = termHeight;
    m_partNumber   = 1;
    m_bytesWritten = 0;

    // Resolve log directory
    std::string dir = logDir;
    if (dir.empty()) {
        const char* home = getenv("HOME");
        dir = std::string(home ? home : "/tmp") + "/.local/share/tcm/logs";
    }
    m_logDir = dir;

    // Create directory tree
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        return false;
    }

    // Build file path and open
    m_currentPath = buildFilePath(dir, sessionName, 0);
    m_file = fopen(m_currentPath.c_str(), "w");
    if (!m_file) {
        return false;
    }

    // Write header before recording start time so elapsed is 0-based
    writeHeader(termWidth, termHeight);
    m_startTime = std::chrono::steady_clock::now();
    m_logging   = true;
    return true;
}

void AsciicastLogger::stop() {
    if (!m_logging) {
        return;
    }
    if (m_file) {
        fflush(m_file);
        fclose(m_file);
        m_file = nullptr;
    }
    m_logging = false;
}

// ---------------------------------------------------------------------------
// Logging helpers
// ---------------------------------------------------------------------------

void AsciicastLogger::logOutput(const uint8_t* data, size_t len) {
    if (!m_logging || !m_file) {
        return;
    }
    if (m_bytesWritten > MAX_FILE_SIZE) {
        rotateFile(m_termWidth, m_termHeight);
    }
    writeEvent("o", data, len);
}

void AsciicastLogger::logOutput(const std::vector<uint8_t>& data) {
    logOutput(data.data(), data.size());
}

void AsciicastLogger::logInput(const uint8_t* data, size_t len) {
    if (!m_logging || !m_file) {
        return;
    }
    writeEvent("i", data, len);
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

bool AsciicastLogger::isLogging() const {
    return m_logging;
}

const std::string& AsciicastLogger::currentFilePath() const {
    return m_currentPath;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void AsciicastLogger::writeHeader(int width, int height) {
    auto now = std::chrono::system_clock::now();
    auto ts  = std::chrono::duration_cast<std::chrono::seconds>(
                   now.time_since_epoch())
                   .count();

    // Minimal JSON-escape the session name for the title field
    std::string title;
    for (char c : m_sessionName) {
        if (c == '"')       title += "\\\"";
        else if (c == '\\') title += "\\\\";
        else                title += c;
    }

    char buf[512];
    int n = snprintf(buf, sizeof(buf),
        "{\"version\":2,\"width\":%d,\"height\":%d,"
        "\"timestamp\":%lld,\"title\":\"%s\"}\n",
        width, height, static_cast<long long>(ts), title.c_str());

    if (n > 0 && m_file) {
        fwrite(buf, 1, static_cast<size_t>(n), m_file);
        m_bytesWritten += static_cast<size_t>(n);
    }
}

void AsciicastLogger::writeEvent(const char* type, const uint8_t* data, size_t len) {
    if (!m_file) {
        return;
    }
    double elapsed = elapsedSeconds();
    std::string escaped = jsonEscape(data, len);

    // Build "[<elapsed>,"<type>","<data>"]\n"
    char hdr[64];
    int hn = snprintf(hdr, sizeof(hdr), "[%.3f,\"%s\",\"", elapsed, type);
    if (hn <= 0) {
        return;
    }

    std::string line;
    line.reserve(static_cast<size_t>(hn) + escaped.size() + 4);
    line.append(hdr, static_cast<size_t>(hn));
    line.append(escaped);
    line.append("\"]\n");

    size_t written = fwrite(line.c_str(), 1, line.size(), m_file);
    m_bytesWritten += written;
}

std::string AsciicastLogger::jsonEscape(const uint8_t* data, size_t len) {
    std::string result;
    result.reserve(len * 2);

    for (size_t i = 0; i < len; ++i) {
        uint8_t b = data[i];
        switch (b) {
            case '\\': result += "\\\\"; break;
            case '"':  result += "\\\""; break;
            case '\r': result += "\\r";  break;
            case '\n': result += "\\n";  break;
            case '\t': result += "\\t";  break;
            default:
                if (b < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u00%02X", b);
                    result += buf;
                } else {
                    // ASCII printable and UTF-8 multibyte: pass through
                    result += static_cast<char>(b);
                }
                break;
        }
    }
    return result;
}

double AsciicastLogger::elapsedSeconds() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - m_startTime).count();
}

std::string AsciicastLogger::buildFilePath(const std::string& logDir,
                                            const std::string& sessionName,
                                            int part) {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    struct tm tm_info{};
    localtime_r(&t, &tm_info);

    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%Y%m%d_%H%M%S", &tm_info);

    std::string filename = sessionName + "_" + timebuf;
    if (part > 0) {
        filename += "_part" + std::to_string(part);
    }
    filename += ".cast";

    return logDir + "/" + filename;
}

void AsciicastLogger::rotateFile(int width, int height) {
    if (m_file) {
        fflush(m_file);
        fclose(m_file);
        m_file = nullptr;
    }

    m_partNumber++;
    m_bytesWritten = 0;
    m_currentPath  = buildFilePath(m_logDir, m_sessionName, m_partNumber);

    m_file = fopen(m_currentPath.c_str(), "w");
    if (!m_file) {
        m_logging = false;
        return;
    }
    writeHeader(width, height);
}

} // namespace tcm
