#pragma once

#include <string>
#include <utility>

namespace tcm {

// Color pair IDs
enum class ColorPair : int {
    Normal      = 1, // white on black
    Highlighted = 2, // black on white (reverse)
    StatusBar   = 3, // black on cyan
    TitleBar    = 4, // white on blue
    Connected   = 5, // green on black
    Error       = 6, // red on black
    Idle        = 7, // white on black (dim)
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    // Initialize ncurses. Returns false if terminal not available.
    bool init();

    // Cleanup ncurses, restore terminal
    void teardown();

    bool isInitialized() const;

    // Get terminal dimensions
    std::pair<int, int> getSize() const; // {cols, rows}

    // Drawing primitives (safe: clamp to terminal bounds)
    void drawBox(int x, int y, int w, int h);
    void drawText(int x, int y, const std::string& text,
                  ColorPair color = ColorPair::Normal);
    void drawTextTruncated(int x, int y, int maxWidth, const std::string& text,
                           ColorPair color = ColorPair::Normal);
    void drawStatusBar(const std::string& text);
    void drawTitleBar(const std::string& text);
    void fillLine(int y, ColorPair color);

    // Refresh screen
    void refresh();

    // Clear screen
    void clear();

    // Suspend ncurses to allow raw terminal passthrough (InSession mode).
    // Saves terminal state then calls endwin().
    void suspend();

    // Resume ncurses after suspend(); restores terminal state.
    void resume();

    // Get a key press (non-blocking). Returns -1 if no key.
    int getKey();

    // Set cursor position
    void moveCursor(int x, int y);
    void hideCursor();
    void showCursor();

private:
    bool m_initialized = false;
    void initColors();
};

} // namespace tcm
