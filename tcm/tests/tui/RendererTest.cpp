#include "tui/Renderer.hpp"

#include <gtest/gtest.h>

using tcm::ColorPair;
using tcm::Renderer;

// In headless (non-tty) env, init() returns false and all methods become no-ops.

TEST(RendererTest, InitReturnsFalseInNonTty) {
    Renderer r;
    // CI/test environment has no real terminal → must return false
    bool result = r.init();
    EXPECT_FALSE(result);
}

TEST(RendererTest, IsInitializedFalseBeforeInit) {
    Renderer r;
    EXPECT_FALSE(r.isInitialized());
}

TEST(RendererTest, TeardownBeforeInitDoesNotCrash) {
    Renderer r;
    EXPECT_NO_THROW(r.teardown());
    EXPECT_FALSE(r.isInitialized());
}

TEST(RendererTest, DrawBoxDoesNotCrashWhenNotInitialized) {
    Renderer r;
    EXPECT_NO_THROW(r.drawBox(0, 0, 10, 5));
}

TEST(RendererTest, DrawTextDoesNotCrashWhenNotInitialized) {
    Renderer r;
    EXPECT_NO_THROW(r.drawText(0, 0, "hello"));
    EXPECT_NO_THROW(r.drawText(0, 0, "hello", ColorPair::Error));
}

TEST(RendererTest, DrawTextTruncatedDoesNotCrashWhenNotInitialized) {
    Renderer r;
    EXPECT_NO_THROW(r.drawTextTruncated(0, 0, 5, "hello world"));
}

TEST(RendererTest, DrawStatusBarDoesNotCrashWhenNotInitialized) {
    Renderer r;
    EXPECT_NO_THROW(r.drawStatusBar("Status: OK"));
}

TEST(RendererTest, DrawTitleBarDoesNotCrashWhenNotInitialized) {
    Renderer r;
    EXPECT_NO_THROW(r.drawTitleBar("TCM v1.0"));
}

TEST(RendererTest, FillLineDoesNotCrashWhenNotInitialized) {
    Renderer r;
    EXPECT_NO_THROW(r.fillLine(0, ColorPair::Normal));
}

TEST(RendererTest, GetKeyReturnsMinusOneWhenNotInitialized) {
    Renderer r;
    int key = -999;
    EXPECT_NO_THROW(key = r.getKey());
    EXPECT_EQ(key, -1);
}

TEST(RendererTest, GetSizeReturnsZeroWhenNotInitialized) {
    Renderer r;
    auto [cols, rows] = r.getSize();
    EXPECT_EQ(cols, 0);
    EXPECT_EQ(rows, 0);
}

TEST(RendererTest, RefreshAndClearDoNotCrashWhenNotInitialized) {
    Renderer r;
    EXPECT_NO_THROW(r.refresh());
    EXPECT_NO_THROW(r.clear());
}

TEST(RendererTest, MoveCursorHideShowDoNotCrashWhenNotInitialized) {
    Renderer r;
    EXPECT_NO_THROW(r.moveCursor(5, 5));
    EXPECT_NO_THROW(r.hideCursor());
    EXPECT_NO_THROW(r.showCursor());
}

TEST(RendererTest, MultipleInitCallsOnlyInitOnce) {
    Renderer r;
    bool first  = r.init();  // should return false in headless
    bool second = r.init();  // should also return false (not re-initialized)
    EXPECT_FALSE(first);
    EXPECT_FALSE(second);
    EXPECT_FALSE(r.isInitialized());
}

TEST(RendererTest, MultipleTeardownCallsDoNotCrash) {
    Renderer r;
    EXPECT_NO_THROW(r.teardown());
    EXPECT_NO_THROW(r.teardown());
    EXPECT_NO_THROW(r.teardown());
}

TEST(RendererTest, DestructorDoesNotCrashWhenNotInitialized) {
    // Destructor calls teardown() internally
    EXPECT_NO_THROW({
        Renderer r;
    });
}
