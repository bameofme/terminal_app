#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/EventBus.hpp"
#include "core/EpollLoop.hpp"
#include "core/Session.hpp"
#include "core/SessionManager.hpp"
#include "credential/CredentialStore.hpp"
#include "logger/AsciicastLogger.hpp"
#include "protocol/SessionConfig.hpp"
#include "tui/Renderer.hpp"
#include "tui/SessionForm.hpp"
#include "tui/SessionListView.hpp"
#include "tui/SwitcherOverlay.hpp"

namespace tcm {

struct AppConfig {
    std::string configPath;   // ~/.config/tcm/sessions.json
    std::string logDir;       // ~/.local/share/tcm/logs/
    std::string credPath;     // ~/.config/tcm/credentials.tcmc
};

class Application {
public:
    explicit Application(AppConfig config);
    ~Application();

    // Initialize all modules
    bool init();

    // Run main event loop (blocks until quit)
    void run();

    // Request quit
    void quit();

    bool isRunning() const;

private:
    AppConfig m_config;
    bool m_running = false;
    std::string m_statusMsg;   // shown in status bar
    // Core modules
    std::unique_ptr<EventBus>       m_bus;
    std::unique_ptr<SessionManager> m_sessionManager;
    std::unique_ptr<EpollLoop>      m_epollLoop;

    // TUI modules
    std::unique_ptr<Renderer>         m_renderer;
    std::unique_ptr<SessionListView>  m_sessionListView;
    std::unique_ptr<SwitcherOverlay>  m_switcherOverlay;
    std::unique_ptr<SessionForm>      m_sessionForm;

    // Support modules
    std::unique_ptr<AsciicastLogger>  m_logger;
    std::unique_ptr<CredentialStore>  m_credentialStore;

    // Sessions data (for TUI)
    std::vector<SessionInfo>    m_sessionInfos;

    // Config
    std::vector<SessionConfig>  m_sessionConfigs;

    // EventBus subscription tokens
    std::vector<EventBus::Token> m_tokens;

    // Event wiring
    void wireEvents();

    // Config persistence
    bool loadSessions();
    bool saveSessions();

    // Session lifecycle
    void onConnectSession(const std::string& sessionId);
    void onDisconnectSession(const std::string& sessionId);
    void onAddSession(const SessionConfig& config);
    void onDeleteSession(const std::string& sessionId);
    void onSwitchSession(const std::string& toId);

    // Signal handlers
    static void signalHandler(int sig);
    static Application* s_instance;

    // Update session info list for TUI
    void updateSessionInfos();

    // Key dispatch
    void handleKey(int key);

    // Active view state
    enum class ViewState { SessionList, InSession, AddForm };
    ViewState m_viewState = ViewState::SessionList;
};

} // namespace tcm
