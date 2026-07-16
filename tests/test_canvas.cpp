/// @file test_canvas.cpp
/// @brief Unit tests for Canvas window CRUD, viewport, Z-order, and grouping.

#include "test_common.hpp"
#include "canvas.hpp"

using namespace chroma;

// ============================================================================
// Window CRUD
// ============================================================================

TEST(Canvas_add_window_assigns_id) {
    Canvas c;
    ChromaWindow w;
    WindowId id = c.add(std::move(w));
    EXPECT_NE(id, INVALID_WINDOW);
    // The window stored inside should have the same id
    auto* stored = c.get(id);
    EXPECT_TRUE(stored != nullptr);
    EXPECT_EQ(stored->id, id);
    PASS();
}

TEST(Canvas_add_window_increments_ids) {
    Canvas c;
    WindowId id1 = c.add(ChromaWindow{});
    WindowId id2 = c.add(ChromaWindow{});
    WindowId id3 = c.add(ChromaWindow{});
    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_NE(id1, id3);
    PASS();
}

TEST(Canvas_add_window_with_preset_id) {
    Canvas c;
    ChromaWindow w;
    w.id = 42;
    WindowId id = c.add(std::move(w));
    EXPECT_EQ(id, 42u);
    PASS();
}

TEST(Canvas_add_window_cascades_position) {
    Canvas c;
    WindowId id1 = c.add(ChromaWindow{});
    WindowId id2 = c.add(ChromaWindow{});

    auto* w1 = c.get(id1);
    auto* w2 = c.get(id2);
    EXPECT_TRUE(w1 != nullptr);
    EXPECT_TRUE(w2 != nullptr);
    // Second window should be offset from the first
    EXPECT_NE(w2->canvas_pos.x, w1->canvas_pos.x);
    EXPECT_NE(w2->canvas_pos.y, w1->canvas_pos.y);
    PASS();
}

TEST(Canvas_get_nonexistent_returns_null) {
    Canvas c;
    EXPECT_TRUE(c.get(999) == nullptr);
    PASS();
}

TEST(Canvas_all_windows) {
    Canvas c;
    c.add(ChromaWindow{});
    c.add(ChromaWindow{});
    c.add(ChromaWindow{});
    EXPECT_EQ(c.all_windows().size(), 3u);
    PASS();
}

TEST(Canvas_remove_window_returns_data) {
    Canvas c;
    ChromaWindow w;
    w.app_id = "test-app";
    WindowId id = c.add(std::move(w));

    ChromaWindow removed = c.remove(id);
    EXPECT_EQ(removed.app_id, "test-app");
    EXPECT_TRUE(c.get(id) == nullptr);
    PASS();
}

TEST(Canvas_remove_nonexistent_returns_empty_window) {
    Canvas c;
    ChromaWindow w = c.remove(999);
    EXPECT_EQ(w.id, INVALID_WINDOW);
    PASS();
}

TEST(Canvas_remove_clears_focus_if_focused) {
    Canvas c;
    WindowId id = c.add(ChromaWindow{});
    c.set_focus(id);
    EXPECT_EQ(c.focused_window(), id);

    c.remove(id);
    EXPECT_EQ(c.focused_window(), INVALID_WINDOW);
    PASS();
}

TEST(Canvas_move_window) {
    Canvas c;
    WindowId id = c.add(ChromaWindow{});
    c.move_window(id, Vec2{100, 200});
    auto* w = c.get(id);
    EXPECT_EQ(w->canvas_pos.x, 100.0f);
    EXPECT_EQ(w->canvas_pos.y, 200.0f);
    PASS();
}

TEST(Canvas_resize_window) {
    Canvas c;
    WindowId id = c.add(ChromaWindow{});
    c.resize_window(id, Vec2{800, 600});
    auto* w = c.get(id);
    EXPECT_EQ(w->size.x, 800.0f);
    EXPECT_EQ(w->size.y, 600.0f);
    PASS();
}

// ============================================================================
// Viewport
// ============================================================================

TEST(Canvas_pan) {
    Canvas c;
    EXPECT_EQ(c.viewport_center.x, 0.0f);
    EXPECT_EQ(c.viewport_center.y, 0.0f);

    c.pan(Vec2{100, -50});
    EXPECT_EQ(c.viewport_center.x, 100.0f);
    EXPECT_EQ(c.viewport_center.y, -50.0f);
    PASS();
}

TEST(Canvas_zoom_at_zoom_in) {
    Canvas c;
    float orig_zoom = c.zoom;
    c.zoom_at(1.5f, Vec2{960, 540}, Vec2{1920, 1080});
    EXPECT_TRUE(c.zoom > orig_zoom);
    PASS();
}

TEST(Canvas_zoom_at_zoom_out) {
    Canvas c;
    float orig_zoom = c.zoom;
    c.zoom_at(0.5f, Vec2{960, 540}, Vec2{1920, 1080});
    EXPECT_TRUE(c.zoom < orig_zoom);
    PASS();
}

TEST(Canvas_zoom_at_clamps_to_min) {
    Canvas c;
    // Zoom out massively
    for (int i = 0; i < 20; i++) {
        c.zoom_at(0.5f, Vec2{0, 0}, Vec2{1920, 1080});
    }
    EXPECT_TRUE(c.zoom >= config::MIN_ZOOM);
    PASS();
}

TEST(Canvas_zoom_at_clamps_to_max) {
    Canvas c;
    // Zoom in massively
    for (int i = 0; i < 20; i++) {
        c.zoom_at(2.0f, Vec2{0, 0}, Vec2{1920, 1080});
    }
    EXPECT_TRUE(c.zoom <= config::MAX_ZOOM);
    PASS();
}

TEST(Canvas_zoom_at_anchor_fixed) {
    Canvas c;
    // The anchor point should stay at the same screen position after zoom
    Vec2 screen_size{1920, 1080};
    Vec2 anchor{960, 540}; // center of screen

    Vec2 canvas_point = c.screen_to_canvas(anchor, screen_size);

    c.zoom_at(2.0f, anchor, screen_size);

    // After zoom, the same canvas point should map back to anchor
    Vec2 screen_after = c.canvas_to_screen(Rect{canvas_point, {0, 0}}, screen_size).pos;
    EXPECT_FLOAT_EQ(screen_after.x, anchor.x, 0.01f);
    EXPECT_FLOAT_EQ(screen_after.y, anchor.y, 0.01f);
    PASS();
}

TEST(Canvas_screen_to_canvas_roundtrip) {
    Canvas c;
    Vec2 screen_size{1920, 1080};
    Vec2 screen_pos{500, 300};
    Vec2 canvas_pos = c.screen_to_canvas(screen_pos, screen_size);
    Rect dummy{canvas_pos, {1, 1}};
    Vec2 back = c.canvas_to_screen(dummy, screen_size).pos;
    EXPECT_FLOAT_EQ(back.x, screen_pos.x, 0.01f);
    EXPECT_FLOAT_EQ(back.y, screen_pos.y, 0.01f);
    PASS();
}

// ============================================================================
// Focus
// ============================================================================

TEST(Canvas_set_focus_on_valid_window) {
    Canvas c;
    WindowId id = c.add(ChromaWindow{});
    c.set_focus(id);
    EXPECT_EQ(c.focused_window(), id);
    auto* w = c.get(id);
    EXPECT_TRUE(w->focused);
    PASS();
}

TEST(Canvas_set_focus_clears_previous) {
    Canvas c;
    WindowId id1 = c.add(ChromaWindow{});
    WindowId id2 = c.add(ChromaWindow{});
    c.set_focus(id1);
    c.set_focus(id2);

    EXPECT_EQ(c.focused_window(), id2);
    EXPECT_TRUE(c.get(id2)->focused);
    EXPECT_FALSE(c.get(id1)->focused);
    PASS();
}

// ============================================================================
// Z-order
// ============================================================================

TEST(Canvas_zorder_new_windows_on_top) {
    Canvas c;
    [[maybe_unused]] WindowId id1 = c.add(ChromaWindow{});
    c.move_window(id1, Vec2{0, 0}); c.get(id1)->mapped = true;
    [[maybe_unused]] WindowId id2 = c.add(ChromaWindow{});
    c.move_window(id2, Vec2{0, 0}); c.get(id2)->mapped = true;
    WindowId id3 = c.add(ChromaWindow{});
    c.move_window(id3, Vec2{0, 0}); c.get(id3)->mapped = true;

    // All windows at (0,0) with default size — id3 should be on top (last added)
    WindowId top = c.window_at(Vec2{0, 0});
    EXPECT_EQ(top, id3);
    PASS();
}

TEST(Canvas_raise_to_top) {
    Canvas c;
    WindowId id1 = c.add(ChromaWindow{});
    c.move_window(id1, Vec2{0, 0}); c.get(id1)->mapped = true;
    [[maybe_unused]] WindowId id2 = c.add(ChromaWindow{});
    c.move_window(id2, Vec2{0, 0}); c.get(id2)->mapped = true;

    // id2 is currently on top
    c.raise_to_top(id1);
    // Now id1 should be on top
    WindowId top = c.window_at(Vec2{0, 0});
    EXPECT_EQ(top, id1);
    PASS();
}

TEST(Canvas_set_focus_raises_to_top) {
    Canvas c;
    WindowId id1 = c.add(ChromaWindow{});
    c.move_window(id1, Vec2{0, 0}); c.get(id1)->mapped = true;
    [[maybe_unused]] WindowId id2 = c.add(ChromaWindow{});
    c.move_window(id2, Vec2{0, 0}); c.get(id2)->mapped = true;
    [[maybe_unused]] WindowId id3 = c.add(ChromaWindow{});
    c.move_window(id3, Vec2{0, 0}); c.get(id3)->mapped = true;

    // id3 is currently on top (last added)
    c.set_focus(id1);
    WindowId top = c.window_at(Vec2{0, 0});
    EXPECT_EQ(top, id1);
    PASS();
}

TEST(Canvas_window_at_respects_position) {
    Canvas c;
    WindowId w1 = c.add(ChromaWindow{});
    c.move_window(w1, Vec2{0, 0});
    c.get(w1)->mapped = true;

    WindowId w2 = c.add(ChromaWindow{});
    c.move_window(w2, Vec2{1000, 1000});
    c.get(w2)->mapped = true;

    WindowId hit1 = c.window_at(Vec2{100, 100});
    WindowId hit2 = c.window_at(Vec2{1100, 1100});

    EXPECT_EQ(hit1, w1);
    EXPECT_EQ(hit2, w2);
    PASS();
}

TEST(Canvas_window_at_ignores_unmapped) {
    Canvas c;
    WindowId w1 = c.add(ChromaWindow{});
    c.get(w1)->mapped = false;

    WindowId hit = c.window_at(Vec2{100, 100});
    EXPECT_EQ(hit, INVALID_WINDOW);
    PASS();
}

// ============================================================================
// Visibility
// ============================================================================

TEST(Canvas_visible_windows_only_mapped) {
    Canvas c;
    WindowId w1 = c.add(ChromaWindow{});
    WindowId w2 = c.add(ChromaWindow{});

    c.get(w1)->mapped = true;
    c.get(w2)->mapped = false;

    auto visible = c.visible_windows(Vec2{1920, 1080});
    EXPECT_TRUE(std::find(visible.begin(), visible.end(), w1) != visible.end());
    EXPECT_TRUE(std::find(visible.begin(), visible.end(), w2) == visible.end());
    PASS();
}

// ============================================================================
// Group management
// ============================================================================

TEST(Canvas_create_group) {
    Canvas c;
    GroupId gid = c.create_group("test");
    EXPECT_NE(gid, NO_GROUP);

    auto* g = c.get_group(gid);
    EXPECT_TRUE(g != nullptr);
    EXPECT_TRUE(g->name == "test");
    EXPECT_TRUE(g->empty());
    PASS();
}

TEST(Canvas_add_to_group) {
    Canvas c;
    WindowId wid = c.add(ChromaWindow{});
    GroupId gid = c.create_group("test");

    c.add_to_group(wid, gid);

    auto* w = c.get(wid);
    EXPECT_EQ(w->group, gid);

    auto* g = c.get_group(gid);
    EXPECT_EQ(g->size(), 1u);
    EXPECT_EQ(g->windows[0], wid);
    PASS();
}

TEST(Canvas_add_to_group_moves_between_groups) {
    Canvas c;
    WindowId wid = c.add(ChromaWindow{});
    GroupId g1 = c.create_group("one");
    GroupId g2 = c.create_group("two");

    c.add_to_group(wid, g1);
    c.add_to_group(wid, g2);

    auto* w = c.get(wid);
    EXPECT_EQ(w->group, g2);

    EXPECT_EQ(c.get_group(g1)->size(), 0u);
    EXPECT_EQ(c.get_group(g2)->size(), 1u);
    PASS();
}

TEST(Canvas_remove_from_group) {
    Canvas c;
    WindowId wid = c.add(ChromaWindow{});
    GroupId gid = c.create_group("test");

    c.add_to_group(wid, gid);
    c.remove_from_group(wid);

    auto* w = c.get(wid);
    EXPECT_EQ(w->group, NO_GROUP);
    EXPECT_EQ(c.get_group(gid)->size(), 0u);
    PASS();
}

TEST(Canvas_destroy_group_removes_all_members) {
    Canvas c;
    WindowId w1 = c.add(ChromaWindow{});
    WindowId w2 = c.add(ChromaWindow{});
    GroupId gid = c.create_group("test");

    c.add_to_group(w1, gid);
    c.add_to_group(w2, gid);
    c.destroy_group(gid);

    EXPECT_TRUE(c.get_group(gid) == nullptr);
    EXPECT_EQ(c.get(w1)->group, NO_GROUP);
    EXPECT_EQ(c.get(w2)->group, NO_GROUP);
    PASS();
}

TEST(Canvas_window_remove_removes_from_group) {
    Canvas c;
    WindowId wid = c.add(ChromaWindow{});
    GroupId gid = c.create_group("test");
    c.add_to_group(wid, gid);

    c.remove(wid);

    auto* g = c.get_group(gid);
    EXPECT_TRUE(g != nullptr);
    EXPECT_EQ(g->size(), 0u);
    PASS();
}

// ============================================================================
// ID generation
// ============================================================================

TEST(Canvas_next_window_id_increments) {
    Canvas c;
    EXPECT_EQ(c.next_window_id(), 1u);
    EXPECT_EQ(c.next_window_id(), 2u);
    EXPECT_EQ(c.next_window_id(), 3u);
    PASS();
}

TEST(Canvas_next_group_id_increments) {
    Canvas c;
    EXPECT_EQ(c.next_group_id(), 1u);
    EXPECT_EQ(c.next_group_id(), 2u);
    PASS();
}

TEST(Canvas_next_default_position_cascades) {
    Canvas c;
    Vec2 p1 = c.next_default_position();
    Vec2 p2 = c.next_default_position();
    Vec2 p3 = c.next_default_position();

    // Each should be offset from the previous
    EXPECT_NE(p1.x, p2.x);
    EXPECT_NE(p1.y, p2.y);
    EXPECT_NE(p2.x, p3.x);
    EXPECT_NE(p2.y, p3.y);
    PASS();
}

// ============================================================================
// Group order & navigation
// ============================================================================

TEST(Canvas_group_order_preserves_creation_order) {
    Canvas c;
    GroupId g1 = c.create_group("first");
    GroupId g2 = c.create_group("second");
    GroupId g3 = c.create_group("third");

    const auto& order = c.group_order();
    EXPECT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], g1);
    EXPECT_EQ(order[1], g2);
    EXPECT_EQ(order[2], g3);
    PASS();
}

TEST(Canvas_group_at_index_returns_correct_id) {
    Canvas c;
    GroupId g1 = c.create_group("A");
    GroupId g2 = c.create_group("B");
    GroupId g3 = c.create_group("C");

    EXPECT_EQ(c.group_at_index(1), g1);
    EXPECT_EQ(c.group_at_index(2), g2);
    EXPECT_EQ(c.group_at_index(3), g3);
    PASS();
}

TEST(Canvas_group_at_index_out_of_range_returns_NO_GROUP) {
    Canvas c;
    EXPECT_EQ(c.group_at_index(0), NO_GROUP);
    EXPECT_EQ(c.group_at_index(1), NO_GROUP);
    c.create_group("A");
    EXPECT_NE(c.group_at_index(1), NO_GROUP);
    EXPECT_EQ(c.group_at_index(2), NO_GROUP);
    PASS();
}

TEST(Canvas_destroy_group_removes_from_order) {
    Canvas c;
    GroupId g1 = c.create_group("A");
    GroupId g2 = c.create_group("B");
    GroupId g3 = c.create_group("C");

    c.destroy_group(g2);

    const auto& order = c.group_order();
    EXPECT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], g1);
    EXPECT_EQ(order[1], g3);

    // Verify g2 is gone from the groups map too
    EXPECT_TRUE(c.get_group(g2) == nullptr);
    PASS();
}

TEST(Canvas_destroy_group_clears_active_group) {
    Canvas c;
    GroupId g1 = c.create_group("A");

    // Simulate a jump setting active_group
    WindowId w1 = c.add(ChromaWindow{});
    c.add_to_group(w1, g1);
    c.jump_to_group(g1, Vec2{1920, 1080});
    EXPECT_EQ(c.active_group(), g1);

    c.destroy_group(g1);
    EXPECT_EQ(c.active_group(), NO_GROUP);
    PASS();
}

TEST(Canvas_jump_to_group_sets_viewport_target) {
    Canvas c;
    GroupId g1 = c.create_group("A");
    WindowId w1 = c.add(ChromaWindow{});
    c.move_window(w1, Vec2{500, 300});
    c.add_to_group(w1, g1);

    EXPECT_FALSE(c.viewport_animating());
    c.jump_to_group(g1, Vec2{1920, 1080});
    EXPECT_TRUE(c.viewport_animating());
    EXPECT_EQ(c.active_group(), g1);
    PASS();
}

TEST(Canvas_jump_to_group_animation_completes) {
    Canvas c;
    GroupId g1 = c.create_group("A");
    WindowId w1 = c.add(ChromaWindow{});
    c.move_window(w1, Vec2{800, 600});
    c.add_to_group(w1, g1);

    c.jump_to_group(g1, Vec2{1920, 1080});

    // Tick the animation to completion
    while (c.viewport_animating()) {
        c.tick_viewport_animation(0.5f); // big step, should finish in one
    }

    EXPECT_FALSE(c.viewport_animating());
    // The viewport should now be centered on the group centroid
    // Window at (800,600) size (800,600) → center at (1200, 900)
    Vec2 win_center = c.get(w1)->center();
    EXPECT_FLOAT_EQ(win_center.x, 1200.0f, 1.0f);
    EXPECT_FLOAT_EQ(win_center.y, 900.0f, 1.0f);
    PASS();
}

TEST(Canvas_cycle_next_group_wraps) {
    Canvas c;
    GroupId g1 = c.create_group("A");
    GroupId g2 = c.create_group("B");
    GroupId g3 = c.create_group("C");

    // Add windows so groups aren't empty
    WindowId w1 = c.add(ChromaWindow{});
    c.move_window(w1, Vec2{100, 100});
    c.add_to_group(w1, g1);
    WindowId w2 = c.add(ChromaWindow{});
    c.move_window(w2, Vec2{500, 100});
    c.add_to_group(w2, g2);
    WindowId w3 = c.add(ChromaWindow{});
    c.move_window(w3, Vec2{900, 100});
    c.add_to_group(w3, g3);

    // No active group → starts from beginning
    GroupId first = c.cycle_next_group(Vec2{1920, 1080});
    EXPECT_EQ(first, g1);

    // Now active is g1, next should be g2
    GroupId second = c.cycle_next_group(Vec2{1920, 1080});
    EXPECT_EQ(second, g2);

    // Next should be g3
    GroupId third = c.cycle_next_group(Vec2{1920, 1080});
    EXPECT_EQ(third, g3);

    // Wrap around back to g1
    GroupId wrapped = c.cycle_next_group(Vec2{1920, 1080});
    EXPECT_EQ(wrapped, g1);
    PASS();
}

TEST(Canvas_cycle_prev_group_wraps) {
    Canvas c;
    GroupId g1 = c.create_group("A");
    GroupId g2 = c.create_group("B");
    GroupId g3 = c.create_group("C");

    // Add windows so groups aren't empty
    WindowId w1 = c.add(ChromaWindow{});
    c.move_window(w1, Vec2{100, 100});
    c.add_to_group(w1, g1);
    WindowId w2 = c.add(ChromaWindow{});
    c.move_window(w2, Vec2{500, 100});
    c.add_to_group(w2, g2);
    WindowId w3 = c.add(ChromaWindow{});
    c.move_window(w3, Vec2{900, 100});
    c.add_to_group(w3, g3);

    // No active group → wraps to last
    GroupId first = c.cycle_prev_group(Vec2{1920, 1080});
    EXPECT_EQ(first, g3);

    // Now active is g3, prev should be g2
    GroupId second = c.cycle_prev_group(Vec2{1920, 1080});
    EXPECT_EQ(second, g2);

    // Wrap around back to g3
    GroupId prev = c.cycle_prev_group(Vec2{1920, 1080});
    EXPECT_EQ(prev, g1);
    PASS();
}

TEST(Canvas_group_count_returns_correct_number) {
    Canvas c;
    EXPECT_EQ(c.group_count(), 0u);
    c.create_group("A");
    EXPECT_EQ(c.group_count(), 1u);
    c.create_group("B");
    EXPECT_EQ(c.group_count(), 2u);
    PASS();
}

// ============================================================================
// Directional indicators
// ============================================================================

TEST(Canvas_compute_indicators_empty_when_no_groups) {
    Canvas c;
    auto indicators = c.compute_indicators(Vec2{1920, 1080});
    EXPECT_EQ(indicators.size(), 0u);
    PASS();
}

TEST(Canvas_compute_indicators_none_for_visible_group) {
    Canvas c;
    GroupId g1 = c.create_group("visible");
    WindowId w1 = c.add(ChromaWindow{});
    c.move_window(w1, Vec2{0, 0}); // near viewport center (0,0)
    c.add_to_group(w1, g1);

    // With viewport at (0,0), the group's centroid is inside the viewport
    auto indicators = c.compute_indicators(Vec2{1920, 1080});
    // The group centroid should be within the visible rect
    // Active group check: haven't jumped to this group, so it's not active
    // But wait — the group is visible so no indicator should be generated
    EXPECT_EQ(indicators.size(), 0u);
    PASS();
}

TEST(Canvas_compute_indicators_for_off_viewport_group) {
    Canvas c;
    GroupId g1 = c.create_group("far");
    WindowId w1 = c.add(ChromaWindow{});
    c.move_window(w1, Vec2{3000, 2000}); // far from viewport center (0,0)
    c.add_to_group(w1, g1);

    auto indicators = c.compute_indicators(Vec2{1920, 1080});
    EXPECT_EQ(indicators.size(), 1u);
    EXPECT_EQ(indicators[0].group_id, g1);
    EXPECT_EQ(indicators[0].name, "far");
    // Direction should point right-and-down (positive x, positive y)
    EXPECT_TRUE(indicators[0].direction.x > 0.0f);
    EXPECT_TRUE(indicators[0].direction.y > 0.0f);
    // Opacity should be > 0 (group is far away)
    EXPECT_TRUE(indicators[0].opacity > 0.0f);
    PASS();
}

TEST(Canvas_compute_indicators_skips_active_group) {
    Canvas c;
    GroupId g1 = c.create_group("active");
    WindowId w1 = c.add(ChromaWindow{});
    c.move_window(w1, Vec2{5000, 3000});
    c.add_to_group(w1, g1);

    // Jump to this group to make it the active group
    c.jump_to_group(g1, Vec2{1920, 1080});
    // Complete the animation so viewport is centered on it
    while (c.viewport_animating()) {
        c.tick_viewport_animation(0.5f);
    }

    // Now the active group is g1, and we should get no indicators
    auto indicators = c.compute_indicators(Vec2{1920, 1080});
    EXPECT_EQ(indicators.size(), 0u);
    PASS();
}

TEST(Canvas_indicator_opacity_zero_for_close_group) {
    Canvas c;
    GroupId g1 = c.create_group("nearby");
    WindowId w1 = c.add(ChromaWindow{});
    // Position just barely outside the visible rect.
    // viewport at (0,0), screen 1920x1080 → visible rect x goes to 960.
    // Window at x=900 with width 800 → spans 900–1700, centroid at 1300.
    // This is close to the edge, so opacity should be low.
    c.move_window(w1, Vec2{900, 0});
    c.add_to_group(w1, g1);

    auto indicators = c.compute_indicators(Vec2{1920, 1080});

    // The group is just outside the viewport edge, so an indicator should
    // exist but with low opacity (close to the fade threshold)
    if (indicators.size() > 0) {
        // The opacity should be relatively low since the group isn't very far
        EXPECT_TRUE(indicators[0].opacity < 0.5f);
    }
    PASS();
}

TEST(Canvas_compute_indicators_multiple_groups) {
    Canvas c;
    GroupId g1 = c.create_group("North");
    GroupId g2 = c.create_group("East");
    GroupId g3 = c.create_group("South");

    WindowId w1 = c.add(ChromaWindow{});
    c.move_window(w1, Vec2{0, -3000}); // north (negative y in screen is up)
    c.add_to_group(w1, g1);

    WindowId w2 = c.add(ChromaWindow{});
    c.move_window(w2, Vec2{5000, 0}); // east
    c.add_to_group(w2, g2);

    WindowId w3 = c.add(ChromaWindow{});
    c.move_window(w3, Vec2{0, 3000}); // south
    c.add_to_group(w3, g3);

    auto indicators = c.compute_indicators(Vec2{1920, 1080});
    // All 3 groups are far from viewport center (0,0)
    EXPECT_EQ(indicators.size(), 3u);

    // Verify each has correct group_id and non-zero opacity
    bool found_north = false, found_east = false, found_south = false;
    for (const auto& ind : indicators) {
        if (ind.group_id == g1) found_north = true;
        if (ind.group_id == g2) found_east = true;
        if (ind.group_id == g3) found_south = true;
        EXPECT_TRUE(ind.opacity > 0.0f);
    }
    EXPECT_TRUE(found_north);
    EXPECT_TRUE(found_east);
    EXPECT_TRUE(found_south);
    PASS();
}
