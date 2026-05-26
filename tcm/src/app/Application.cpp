#include "app/Application.hpp"

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/wait.h>
#include <thread>
#include <chrono>
#include <unistd.h>

#include <nlohmann/json.hpp>

#include "core/Session.hpp"
#include "protocol/ConnectionFactory.hpp"

namespace tcm {

// ---------------------------------------------------------------------------
// Static members
// ---------------------------------------------------------------------------
Application* Application::s_instance = nullptr;

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------
Application::Application(AppConfig config)
    : m_config(std::move(config))
{}

Application::~Application() {
    if (m_epollLoop) {
        m_epollLoop->stop();
        m_epollLoop->join();
    }
    if (m_renderer && m_renderer->isInitialized()) {
        m_renderer->teardown();
    }
    for (auto tok : m_tokens) {
        m_bus->unsubscribe(tok);
    }
    s_instance = nullptr;
}

// ---------------------------------------------------------------------------
// init()
// ---------------------------------------------------------------------------
bool Application::init() {
    s_instance = this;

    // 1. Create required directories
    try {
        std::filesystem::create_directories(
            std::filesystem::path(m_config.configPath).parent_path());
        std::filesystem::create_directories(m_config.logDir);
    } catch (const std::exception& e) {
        std::cerr << "TCM: Failed to create directories: " << e.what() << "\n";
        return false;
    }

    // 2. Core modules
    m_bus            = std::make_unique<EventBus>();
    m_sessionManager = std::make_unique<SessionManager>(*m_bus);
    m_epollLoop      = std::make_unique<EpollLoop>();

    // 3. TUI modules
    m_renderer = std::make_unique<Renderer>();
    if (!m_renderer->init()) {
        return false;
    }

    m_sessionListView = std::make_unique<SessionListView>(m_sessionInfos, *m_bus);
    m_switcherOverlay = std::make_unique<SwitcherOverlay>(m_sessionInfos, *m_bus);
    m_sessionForm     = std::make_unique<SessionForm>(*m_bus);

    // 4. Support modules
    m_logger          = std::make_unique<AsciicastLogger>();
    m_credentialStore = std::make_unique<CredentialStore>(m_config.credPath);
    m_credentialStore->load(); // ignore error if file doesn't exist yet

    // 5. Load sessions from config file
    loadSessions();

    // 6. Wire events
    wireEvents();

    // 7. Start epoll loop in background thread
    m_epollLoop->runAsync();

    // 8. Install signal handlers
    struct sigaction sa{};
    sa.sa_handler = Application::signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGWINCH, &sa, nullptr);

    // SIGCHLD: reap children to avoid zombies
    struct sigaction sa_chld{};
    sa_chld.sa_handler = [](int) {
        int saved = errno;
        while (waitpid(-1, nullptr, WNOHANG) > 0) {}
        errno = saved;
    };
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa_chld, nullptr);

    m_running = true;
    return true;
}

// ---------------------------------------------------------------------------
// wireEvents()
// ---------------------------------------------------------------------------
void Application::wireEvents() {
    // Session switched → update active & switch view
    m_tokens.push_back(m_bus->subscribe<SessionSwitchedEvent>(
        [this](const SessionSwitchedEvent& ev) {
            onSwitchSession(ev.toId);
        }));

    // Escape key → show switcher overlay
    m_tokens.push_back(m_bus->subscribe<KeyPressedEvent>(
        [this](const KeyPressedEvent& ev) {
            if (ev.isEscape) {
                m_switcherOverlay->show();
            }
        }));

    // Data received → log output + display in InSession mode
    m_tokens.push_back(m_bus->subscribe<DataReceivedEvent>(
        [this](const DataReceivedEvent& ev) {
            if (!ev.isInput && m_logger->isLogging()) {
                m_logger->logOutput(ev.data);
            }
            // Write to terminal when in raw passthrough mode
            if (!ev.isInput && m_rawMode.load()) {
                if (::write(STDOUT_FILENO, ev.data.data(), ev.data.size()) < 0) { /* ignore */ }
            }
        }));

    // Macro done → update status bar via renderer
    m_tokens.push_back(m_bus->subscribe<MacroDoneEvent>(
        [this](const MacroDoneEvent& ev) {
            if (m_renderer && m_renderer->isInitialized()) {
                std::string msg = ev.success
                    ? "Macro done: " + ev.sessionId
                    : "Macro failed: " + ev.sessionId;
                m_renderer->drawStatusBar(msg);
                m_renderer->refresh();
            }
        }));

    // Session disconnected → cleanup
    m_tokens.push_back(m_bus->subscribe<SessionDisconnectedEvent>(
        [this](const SessionDisconnectedEvent& ev) {
            Session* s = m_sessionManager->findById(ev.sessionId);
            if (s) {
                s->state = SessionState::Idle;
            }
            if (m_logger->isLogging()) {
                m_logger->stop();
            }
            if (m_viewState == ViewState::InSession) {
                m_viewState = ViewState::SessionList;
            }
            updateSessionInfos();
        }));

    // User detached → return to session list, keep session connected
    m_tokens.push_back(m_bus->subscribe<SessionDetachedEvent>(
        [this](const SessionDetachedEvent&) {
            m_viewState = ViewState::SessionList;
        }));
}

// ---------------------------------------------------------------------------
// enterRawMode() / leaveRawMode() - InSession terminal passthrough
// ---------------------------------------------------------------------------
void Application::enterRawMode() {
    if (m_rawMode.load()) return;
    m_renderer->suspend();          // def_prog_mode() + endwin()

    // Set stdin to raw/non-blocking for direct I/O
    struct termios raw;
    if (tcgetattr(STDIN_FILENO, &raw) == 0) {
        if (!m_termiosSaved) {
            m_savedTermios = raw;
            m_termiosSaved = true;
        }
        cfmakeraw(&raw);
        raw.c_cc[VMIN]  = 0;   // non-blocking
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags >= 0) fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    m_rawMode.store(true);
}

void Application::leaveRawMode() {
    if (!m_rawMode.load()) return;
    m_rawMode.store(false);

    // Restore stdin to blocking mode
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags >= 0) fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);

    // Restore original terminal settings
    if (m_termiosSaved) {
        tcsetattr(STDIN_FILENO, TCSANOW, &m_savedTermios);
    }
    m_renderer->resume();           // reset_prog_mode() + refresh()
}

// ---------------------------------------------------------------------------
void Application::run() {
    while (m_running) {
        // Sync raw mode: exit raw mode if we're no longer InSession
        if (m_rawMode.load() && m_viewState != ViewState::InSession) {
            leaveRawMode();
        }

        if (m_viewState == ViewState::InSession &&
                !m_switcherOverlay->isVisible()) {
            // Auto-enter raw mode for terminal passthrough
            if (!m_rawMode.load()) {
                enterRawMode();
            }

            // Read stdin directly and forward to active connection
            uint8_t buf[256];
            ssize_t n = ::read(STDIN_FILENO, buf, sizeof(buf));
            if (n > 0) {
                // Ctrl+] (0x1D) — show switcher
                bool gotSwitch = false;
                for (ssize_t i = 0; i < n; i++) {
                    if (buf[i] == 0x1D) { gotSwitch = true; break; }
                }
                if (gotSwitch) {
                    leaveRawMode();
                    m_switcherOverlay->show();
                } else {
                    Session* active = m_sessionManager->getActive();
                    if (active && active->conn &&
                        active->conn->getState() == ConnectionState::Connected) {
                        active->conn->send(
                            std::span<const uint8_t>(buf, static_cast<size_t>(n)));
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        // Normal ncurses rendering path
        m_renderer->clear();

        switch (m_viewState) {
            case ViewState::SessionList:
                m_sessionListView->render(*m_renderer);
                // Override status bar if there's a message (e.g. connection failure)
                if (!m_statusMsg.empty()) {
                    m_renderer->drawStatusBar(m_statusMsg);
                }
                break;

            case ViewState::InSession:
                // Switcher overlay visible: draw background title bar
                m_renderer->drawTitleBar("TCM \xe2\x80\x94 Press Ctrl+] to switch sessions");
                break;

            case ViewState::AddForm:
                m_sessionForm->render(*m_renderer);
                break;
        }

        // Show switcher overlay if visible
        if (m_switcherOverlay->isVisible()) {
            m_switcherOverlay->render(*m_renderer);
        }

        m_renderer->refresh();

        // Handle input (non-blocking)
        int key = m_renderer->getKey();
        if (key != -1) {
            handleKey(key);
        }

        // ~60 fps
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // Ensure terminal is restored on exit
    if (m_rawMode.load()) leaveRawMode();
}

// ---------------------------------------------------------------------------
// handleKey()
// ---------------------------------------------------------------------------
void Application::handleKey(int key) {
    // Clear any status message on next key press
    m_statusMsg.clear();

    // Switcher overlay gets priority
    if (m_switcherOverlay->isVisible()) {
        m_switcherOverlay->handleKey(key);
        return;
    }

    switch (m_viewState) {
        case ViewState::SessionList:
            // Let the list view consume the key first
            if (!m_sessionListView->handleKey(key)) {
                // Keys not consumed by the list
                if (key == 'a' || key == 'A') {
                    m_sessionForm->reset();
                    m_viewState = ViewState::AddForm;
                } else if (key == 'q' || key == 'Q') {
                    quit();
                } else if (key == 10 || key == 13 || key == 343 /*KEY_ENTER*/) {
                    // Enter — connect selected session
                    std::string id = m_sessionListView->getSelectedId();
                    if (!id.empty()) {
                        onConnectSession(id);
                    }
                } else if (key == 'd' || key == 'D') {
                    std::string id = m_sessionListView->getSelectedId();
                    if (!id.empty()) {
                        onDeleteSession(id);
                    }
                }
            }
            break;

        case ViewState::InSession: {
            // Keys in InSession are handled in the run() raw path.
            // This branch only executes while ncurses is active (e.g. overlay
            // just closed and we haven't re-entered raw mode yet).
            if (key == 0x1D) {
                m_switcherOverlay->show();
            }
            break;
        }

        case ViewState::AddForm:
            if (!m_sessionForm->handleKey(key)) {
                // Not consumed
            }
            // Check form result
            {
                FormResult res = m_sessionForm->getResult();
                if (res == FormResult::Confirmed && m_sessionForm->isValid()) {
                    onAddSession(m_sessionForm->getConfig());
                    m_viewState = ViewState::SessionList;
                } else if (res == FormResult::Cancelled) {
                    m_viewState = ViewState::SessionList;
                }
            }
            break;
    }
}

// ---------------------------------------------------------------------------
// quit()
// ---------------------------------------------------------------------------
void Application::quit() {
    m_running = false;
}

bool Application::isRunning() const {
    return m_running;
}

// ---------------------------------------------------------------------------
// loadSessions()
// ---------------------------------------------------------------------------
bool Application::loadSessions() {
    std::ifstream f(m_config.configPath);
    if (!f.is_open()) {
        return false; // No file yet — start fresh
    }

    try {
        nlohmann::json j = nlohmann::json::parse(f);
        if (!j.is_array()) return false;

        m_sessionConfigs.clear();
        for (const auto& item : j) {
            SessionConfig cfg = SessionConfig::fromJson(item);
            m_sessionConfigs.push_back(cfg);

            // Add to SessionManager as Idle sessions (no connection yet)
            Session s;
            s.id    = generateUUID();
            s.name  = cfg.name;
            s.type  = cfg.type;
            s.state = SessionState::Idle;
            m_sessionManager->add(std::move(s));
        }
    } catch (const std::exception& e) {
        std::cerr << "TCM: Failed to load sessions: " << e.what() << "\n";
        return false;
    }

    updateSessionInfos();
    return true;
}

// ---------------------------------------------------------------------------
// saveSessions()
// ---------------------------------------------------------------------------
bool Application::saveSessions() {
    try {
        nlohmann::json j = nlohmann::json::array();
        for (const auto& cfg : m_sessionConfigs) {
            j.push_back(cfg.toJson());
        }

        std::ofstream f(m_config.configPath);
        if (!f.is_open()) return false;
        f << j.dump(2) << "\n";
    } catch (const std::exception& e) {
        std::cerr << "TCM: Failed to save sessions: " << e.what() << "\n";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Session lifecycle
// ---------------------------------------------------------------------------
void Application::onConnectSession(const std::string& sessionId) {
    Session* s = m_sessionManager->findById(sessionId);
    if (!s) return;

    // Find matching config by name/type
    SessionConfig* cfg = nullptr;
    for (auto& c : m_sessionConfigs) {
        if (c.name == s->name && c.type == s->type) {
            cfg = &c;
            break;
        }
    }
    if (!cfg) return;

    // Create connection
    s->conn  = ConnectionFactory::create(*cfg);
    s->state = SessionState::Connecting;
    updateSessionInfos();

    int rc = s->conn->connect();
    if (rc != TCM_OK) {
        s->state = SessionState::Error;
        std::string target;
        if (cfg->ssh)         target = cfg->ssh->host + ":" + std::to_string(cfg->ssh->port);
        else if (cfg->telnet) target = cfg->telnet->host + ":" + std::to_string(cfg->telnet->port);
        else if (cfg->serial) target = cfg->serial->device;
        m_statusMsg = "Failed to connect " + s->name + " -> " + target;
        updateSessionInfos();
        return;
    }

    s->state = SessionState::Connected;
    updateSessionInfos();

    // Activate this session
    m_sessionManager->setActive(sessionId);

    // Start logging
    auto [cols, rows] = m_renderer->getSize();
    m_logger->start(s->name, cols, rows, m_config.logDir);

    // Register fd with epoll
    int fd = s->conn->getFd();
    if (fd >= 0) {
        m_epollLoop->addFd(fd, [this, sessionId, fd](int) {
            Session* sess = m_sessionManager->findById(sessionId);
            if (!sess || !sess->conn) return;
            uint8_t buf[4096];
            int n = sess->conn->recv(std::span<uint8_t>(buf, sizeof(buf)));
            if (n > 0) {
                DataReceivedEvent ev;
                ev.sessionId = sessionId;
                ev.data.assign(buf, buf + n);
                ev.isInput = false;
                m_bus->publish(ev);
            } else if (n <= 0) {
                sess->state = SessionState::Idle;
                m_epollLoop->removeFd(fd);
                m_bus->publish(SessionDisconnectedEvent{sessionId});
            }
        });
    }

    m_bus->publish(SessionConnectedEvent{sessionId});
    m_viewState = ViewState::InSession;
}

void Application::onDisconnectSession(const std::string& sessionId) {
    Session* s = m_sessionManager->findById(sessionId);
    if (!s) return;

    if (s->conn) {
        int fd = s->conn->getFd();
        if (fd >= 0) m_epollLoop->removeFd(fd);
        s->conn->disconnect();
        s->conn.reset();
    }
    s->state = SessionState::Idle;
    updateSessionInfos();

    if (m_logger->isLogging()) m_logger->stop();
    if (m_viewState == ViewState::InSession) {
        m_viewState = ViewState::SessionList;
    }

    m_bus->publish(SessionDisconnectedEvent{sessionId});
}

void Application::onAddSession(const SessionConfig& config) {
    // Add to config list
    m_sessionConfigs.push_back(config);

    // Add to session manager
    Session s;
    s.id    = generateUUID();
    s.name  = config.name;
    s.type  = config.type;
    s.state = SessionState::Idle;
    m_sessionManager->add(std::move(s));

    updateSessionInfos();
    saveSessions();
}

void Application::onDeleteSession(const std::string& sessionId) {
    Session* s = m_sessionManager->findById(sessionId);
    if (!s) return;

    // Remove config by matching name
    std::string name = s->name;
    SessionType type = s->type;

    if (s->conn) {
        int fd = s->conn->getFd();
        if (fd >= 0) m_epollLoop->removeFd(fd);
        s->conn->disconnect();
    }

    m_sessionManager->remove(sessionId);

    auto it = std::find_if(m_sessionConfigs.begin(), m_sessionConfigs.end(),
        [&](const SessionConfig& c) { return c.name == name && c.type == type; });
    if (it != m_sessionConfigs.end()) {
        m_sessionConfigs.erase(it);
    }

    updateSessionInfos();
    saveSessions();
}

void Application::onSwitchSession(const std::string& toId) {
    m_sessionManager->setActive(toId);
    m_switcherOverlay->hide();

    Session* s = m_sessionManager->findById(toId);
    if (s && s->state == SessionState::Connected) {
        m_viewState = ViewState::InSession;
    } else {
        m_viewState = ViewState::SessionList;
    }
}

// ---------------------------------------------------------------------------
// updateSessionInfos()
// ---------------------------------------------------------------------------
void Application::updateSessionInfos() {
    m_sessionInfos.clear();
    m_sessionManager->forEach([this](Session& s) {
        SessionInfo info;
        info.id    = s.id;
        info.name  = s.name;
        info.state = s.state;

        // Determine host string from config
        for (const auto& cfg : m_sessionConfigs) {
            if (cfg.name == s.name && cfg.type == s.type) {
                if (cfg.ssh && s.type == SessionType::SSH) {
                    info.host    = cfg.ssh->host;
                    info.typeStr = "SSH";
                } else if (cfg.serial && s.type == SessionType::Serial) {
                    info.host    = cfg.serial->device;
                    info.typeStr = "SER";
                } else if (cfg.telnet && s.type == SessionType::Telnet) {
                    info.host    = cfg.telnet->host;
                    info.typeStr = "TEL";
                }
                break;
            }
        }
        if (info.typeStr.empty()) {
            switch (s.type) {
                case SessionType::SSH:    info.typeStr = "SSH"; break;
                case SessionType::Serial: info.typeStr = "SER"; break;
                case SessionType::Telnet: info.typeStr = "TEL"; break;
            }
        }

        m_sessionInfos.push_back(std::move(info));
    });
}

// ---------------------------------------------------------------------------
// signalHandler()
// ---------------------------------------------------------------------------
void Application::signalHandler(int sig) {
    if (!s_instance) return;

    switch (sig) {
        case SIGTERM:
        case SIGINT:
            s_instance->quit();
            break;
        case SIGWINCH:
            // Terminal resize — renderer will pick up new size on next render
            break;
        default:
            break;
    }
}

} // namespace tcm
