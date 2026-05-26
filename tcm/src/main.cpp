#include "app/Application.hpp"

#include <cstring>
#include <iostream>

int main(int argc, char** argv) {
    tcm::AppConfig config;

    // Default paths
    const char* home = getenv("HOME");
    if (!home) home = "/tmp";

    config.configPath = std::string(home) + "/.config/tcm/sessions.json";
    config.logDir     = std::string(home) + "/.local/share/tcm/logs";
    config.credPath   = std::string(home) + "/.config/tcm/credentials.tcmc";

    // Parse CLI args
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--version") == 0) {
            std::cout << "TCM v1.0.0 - Terminal Connection Manager\n";
            return 0;
        }
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config.configPath = argv[++i];
        }
        if (strcmp(argv[i], "--log-dir") == 0 && i + 1 < argc) {
            config.logDir = argv[++i];
        }
    }

    tcm::Application app(config);

    if (!app.init()) {
        std::cerr << "TCM: Failed to initialize. Is this running in a terminal?\n";
        return 1;
    }

    app.run();
    return 0;
}
