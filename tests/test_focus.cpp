/// @file test_focus.cpp
/// @brief Unit tests for FocusTracker MRU (Most-Recently-Used) logic.

#include "test_common.hpp"
#include "canvas.hpp"
#include "focus.hpp"

using namespace chroma;

// Helper: create a canvas with a few windows
static Canvas make_canvas_with_windows(int count) {
    Canvas c;
    for (int i = 0; i < count; i++) {
        ChromaWindow w;
        c.add(std::move(w));
    }
    return c;
}

TEST(FocusTracker_initial_state_empty) {
    FocusTracker ft;
    EXPECT_EQ(ft.current(), INVALID_WINDOW);
    EXPECT_EQ(ft.previous(), INVALID_WINDOW);
    EXPECT_EQ(ft.size(), 0u);
    PASS();
}

TEST(FocusTracker_focused_adds_to_front) {
    FocusTracker ft;
    ft.focused(5);
    EXPECT_EQ(ft.current(), 5u);
    EXPECT_EQ(ft.size(), 1u);
    PASS();
}

TEST(FocusTracker_focused_multiple_order) {
    FocusTracker ft;
    ft.focused(1);
    ft.focused(2);
    ft.focused(3);
    // 3 should be at front (most recent)
    EXPECT_EQ(ft.current(), 3u);
    EXPECT_EQ(ft.previous(), 2u);
    EXPECT_EQ(ft.size(), 3u);
    PASS();
}

TEST(FocusTracker_focused_reorders_existing) {
    FocusTracker ft;
    ft.focused(1);
    ft.focused(2);
    ft.focused(3);
    // Now focus 1 again — should move to front
    ft.focused(1);
    EXPECT_EQ(ft.current(), 1u);
    EXPECT_EQ(ft.previous(), 3u);
    EXPECT_EQ(ft.size(), 3u);
    PASS();
}

TEST(FocusTracker_invalid_window_ignored) {
    FocusTracker ft;
    ft.focused(INVALID_WINDOW);
    EXPECT_EQ(ft.size(), 0u);
    PASS();
}

TEST(FocusTracker_unfocused_keeps_window) {
    FocusTracker ft;
    ft.focused(5);
    ft.unfocused(5);
    // Window should still be in history for cycling back
    EXPECT_EQ(ft.size(), 1u);
    EXPECT_EQ(ft.current(), 5u);
    PASS();
}

TEST(FocusTracker_remove_clears_from_history) {
    FocusTracker ft;
    ft.focused(1);
    ft.focused(2);
    ft.focused(3);
    ft.remove(2);
    EXPECT_EQ(ft.size(), 2u);
    EXPECT_EQ(ft.current(), 3u);
    EXPECT_EQ(ft.previous(), 1u);
    PASS();
}

TEST(FocusTracker_remove_current) {
    FocusTracker ft;
    ft.focused(1);
    ft.focused(2);
    ft.remove(2);
    EXPECT_EQ(ft.current(), 1u);
    EXPECT_EQ(ft.size(), 1u);
    PASS();
}

TEST(FocusTracker_remove_last) {
    FocusTracker ft;
    ft.focused(1);
    ft.remove(1);
    EXPECT_EQ(ft.size(), 0u);
    EXPECT_EQ(ft.current(), INVALID_WINDOW);
    PASS();
}

TEST(FocusTracker_remove_nonexistent_is_noop) {
    FocusTracker ft;
    ft.focused(1);
    ft.remove(99);
    EXPECT_EQ(ft.size(), 1u);
    EXPECT_EQ(ft.current(), 1u);
    PASS();
}

TEST(FocusTracker_clear) {
    FocusTracker ft;
    ft.focused(1);
    ft.focused(2);
    ft.focused(3);
    ft.clear();
    EXPECT_EQ(ft.size(), 0u);
    EXPECT_EQ(ft.current(), INVALID_WINDOW);
    PASS();
}

TEST(FocusTracker_cycle_next_with_one_window) {
    FocusTracker ft;
    Canvas c = make_canvas_with_windows(5);
    ft.focused(1);
    WindowId result = ft.cycle_next(c);
    EXPECT_EQ(result, INVALID_WINDOW); // need at least 2
    PASS();
}

TEST(FocusTracker_cycle_next_with_two_windows) {
    FocusTracker ft;
    Canvas c = make_canvas_with_windows(5);
    ft.focused(1);
    ft.focused(2);
    // Current = 2, previous = 1
    WindowId result = ft.cycle_next(c);
    EXPECT_EQ(result, 1u); // 2 moves to back, 1 becomes front
    EXPECT_EQ(ft.current(), 1u);
    EXPECT_EQ(c.focused_window(), 1u);
    PASS();
}

TEST(FocusTracker_cycle_prev_with_two_windows) {
    FocusTracker ft;
    Canvas c = make_canvas_with_windows(5);
    ft.focused(1);
    ft.focused(2);
    // Current = 2, previous = 1
    WindowId result = ft.cycle_prev(c);
    EXPECT_EQ(result, 1u); // 1 moves to front from back
    EXPECT_EQ(ft.current(), 1u);
    EXPECT_EQ(c.focused_window(), 1u);
    PASS();
}

TEST(FocusTracker_cycle_next_full_rotation) {
    FocusTracker ft;
    Canvas c = make_canvas_with_windows(5);
    ft.focused(1);
    ft.focused(2);
    ft.focused(3);
    // Order: 3, 2, 1
    EXPECT_EQ(ft.current(), 3u);

    ft.cycle_next(c);
    EXPECT_EQ(ft.current(), 2u); // 2 becomes front
    EXPECT_EQ(c.focused_window(), 2u);

    ft.cycle_next(c);
    EXPECT_EQ(ft.current(), 1u); // 1 becomes front
    EXPECT_EQ(c.focused_window(), 1u);

    ft.cycle_next(c);
    EXPECT_EQ(ft.current(), 3u); // back to 3
    EXPECT_EQ(c.focused_window(), 3u);
    PASS();
}

TEST(FocusTracker_cycle_prev_full_rotation) {
    FocusTracker ft;
    Canvas c = make_canvas_with_windows(5);
    ft.focused(1);
    ft.focused(2);
    ft.focused(3);
    // Order: 3, 2, 1

    ft.cycle_prev(c);
    EXPECT_EQ(ft.current(), 1u); // back to front

    ft.cycle_prev(c);
    EXPECT_EQ(ft.current(), 2u);

    ft.cycle_prev(c);
    EXPECT_EQ(ft.current(), 3u);
    PASS();
}

TEST(FocusTracker_history_bounded_to_50) {
    FocusTracker ft;
    // Push 60 windows — should only keep the latest 50
    for (uint64_t i = 1; i <= 60; i++) {
        ft.focused(i);
    }
    EXPECT_TRUE(ft.size() <= 50u);
    // The most recent (60) should be current
    EXPECT_EQ(ft.current(), 60u);
    // The oldest (1-10) should have been evicted
    EXPECT_TRUE(ft.size() == 50u);
    PASS();
}
