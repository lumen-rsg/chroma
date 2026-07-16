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
