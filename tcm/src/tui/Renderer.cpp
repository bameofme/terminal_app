#include "tui/Renderer.hpp"

#include <ncurses.h>
#include <unistd.h>

namespace tcm {

Renderer::Renderer() : m_initialized(false) {}

Renderer::~Renderer() {
    teardown();
}

bool Renderer::init() {
    if (m_initialized) {
        return true;
    }

    // Non-tty environment (headless/test) → do not initialize ncurses
    if (!isatty(STDOUT_FILENO)) {
        return false;
    }

    initscr();
    start_color();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    curs_set(0);
    initColors();

    m_initialized = true;
    return true;
}

void Renderer::initColors() {
    init_pair(static_cast<int>(ColorPair::Normal),      COLOR_WHITE,  COLOR_BLACK);
    init_pair(static_cast<int>(ColorPair::Highlighted), COLOR_BLACK,  COLOR_WHITE);
    init_pair(static_cast<int>(ColorPair::StatusBar),   COLOR_BLACK,  COLOR_CYAN);
    init_pair(static_cast<int>(ColorPair::TitleBar),    COLOR_WHITE,  COLOR_BLUE);
    init_pair(static_cast<int>(ColorPair::Connected),   COLOR_GREEN,  COLOR_BLACK);
    init_pair(static_cast<int>(ColorPair::Error),       COLOR_RED,    COLOR_BLACK);
    init_pair(static_cast<int>(ColorPair::Idle),        COLOR_WHITE,  COLOR_BLACK);
}

void Renderer::teardown() {
    if (!m_initialized) {
        return;
    }
    endwin();
    m_initialized = false;
}

bool Renderer::isInitialized() const {
    return m_initialized;
}

std::pair<int, int> Renderer::getSize() const {
    if (!m_initialized) {
        return {0, 0};
    }
    int rows = 0;
    int cols = 0;
    getmaxyx(stdscr, rows, cols);
    return {cols, rows};
}

void Renderer::drawBox(int x, int y, int w, int h) {
    if (!m_initialized || w < 2 || h < 2) {
        return;
    }

    auto [cols, rows] = getSize();

    // Clamp to terminal bounds
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + w > cols) w = cols - x;
    if (y + h > rows) h = rows - y;
    if (w < 2 || h < 2) {
        return;
    }

    // Top and bottom horizontal lines
    mvhline(y,         x + 1, ACS_HLINE, w - 2);
    mvhline(y + h - 1, x + 1, ACS_HLINE, w - 2);

    // Left and right vertical lines
    mvvline(y + 1, x,         ACS_VLINE, h - 2);
    mvvline(y + 1, x + w - 1, ACS_VLINE, h - 2);

    // Corners
    mvaddch(y,         x,         ACS_ULCORNER);
    mvaddch(y,         x + w - 1, ACS_URCORNER);
    mvaddch(y + h - 1, x,         ACS_LLCORNER);
    mvaddch(y + h - 1, x + w - 1, ACS_LRCORNER);
}

void Renderer::drawText(int x, int y, const std::string& text, ColorPair color) {
    if (!m_initialized) {
        return;
    }

    auto [cols, rows] = getSize();
    if (x < 0 || y < 0 || x >= cols || y >= rows) {
        return;
    }

    int available = cols - x;
    if (available <= 0) {
        return;
    }

    attron(COLOR_PAIR(static_cast<int>(color)));
    mvaddnstr(y, x, text.c_str(), available);
    attroff(COLOR_PAIR(static_cast<int>(color)));
}

void Renderer::drawTextTruncated(int x, int y, int maxWidth,
                                 const std::string& text, ColorPair color) {
    if (!m_initialized) {
        return;
    }

    auto [cols, rows] = getSize();
    if (x < 0 || y < 0 || x >= cols || y >= rows) {
        return;
    }

    int available = std::min(maxWidth, cols - x);
    if (available <= 0) {
        return;
    }

    attron(COLOR_PAIR(static_cast<int>(color)));
    mvaddnstr(y, x, text.c_str(), available);
    attroff(COLOR_PAIR(static_cast<int>(color)));
}

void Renderer::drawStatusBar(const std::string& text) {
    if (!m_initialized) {
        return;
    }

    auto [cols, rows] = getSize();
    int y = rows - 1;

    fillLine(y, ColorPair::StatusBar);
    drawText(0, y, text, ColorPair::StatusBar);
}

void Renderer::drawTitleBar(const std::string& text) {
    if (!m_initialized) {
        return;
    }

    fillLine(0, ColorPair::TitleBar);
    drawText(0, 0, text, ColorPair::TitleBar);
}

void Renderer::fillLine(int y, ColorPair color) {
    if (!m_initialized) {
        return;
    }

    auto [cols, rows] = getSize();
    if (y < 0 || y >= rows) {
        return;
    }

    attron(COLOR_PAIR(static_cast<int>(color)));
    mvhline(y, 0, ' ', cols);
    attroff(COLOR_PAIR(static_cast<int>(color)));
}

void Renderer::refresh() {
    if (!m_initialized) {
        return;
    }
    ::refresh();
}

void Renderer::clear() {
    if (!m_initialized) {
        return;
    }
    ::clear();
}

int Renderer::getKey() {
    if (!m_initialized) {
        return -1;
    }
    nodelay(stdscr, TRUE);
    int ch = getch();
    return (ch == ERR) ? -1 : ch;
}

void Renderer::moveCursor(int x, int y) {
    if (!m_initialized) {
        return;
    }
    ::move(y, x);
}

void Renderer::hideCursor() {
    if (!m_initialized) {
        return;
    }
    curs_set(0);
}

void Renderer::showCursor() {
    if (!m_initialized) {
        return;
    }
    curs_set(1);
}

} // namespace tcm
