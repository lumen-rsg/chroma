/// @file test_types.cpp
/// @brief Unit tests for Vec2, Rect, and Transform2D math primitives.

#include "test_common.hpp"
#include "types.hpp"

using namespace chroma;

// ============================================================================
// Vec2
// ============================================================================

TEST(Vec2_default_construction) {
    Vec2 v;
    EXPECT_EQ(v.x, 0.0f);
    EXPECT_EQ(v.y, 0.0f);
    PASS();
}

TEST(Vec2_parameter_construction) {
    Vec2 v(3.0f, 4.0f);
    EXPECT_EQ(v.x, 3.0f);
    EXPECT_EQ(v.y, 4.0f);
    PASS();
}

TEST(Vec2_negation) {
    Vec2 v(3.0f, -4.0f);
    Vec2 n = -v;
    EXPECT_EQ(n.x, -3.0f);
    EXPECT_EQ(n.y, 4.0f);
    PASS();
}

TEST(Vec2_addition) {
    Vec2 a(1, 2), b(3, 4);
    Vec2 c = a + b;
    EXPECT_EQ(c.x, 4.0f);
    EXPECT_EQ(c.y, 6.0f);
    PASS();
}

TEST(Vec2_subtraction) {
    Vec2 a(5, 7), b(2, 3);
    Vec2 c = a - b;
    EXPECT_EQ(c.x, 3.0f);
    EXPECT_EQ(c.y, 4.0f);
    PASS();
}

TEST(Vec2_scalar_multiplication) {
    Vec2 v(2, 3);
    Vec2 r = v * 4.0f;
    EXPECT_EQ(r.x, 8.0f);
    EXPECT_EQ(r.y, 12.0f);
    PASS();
}

TEST(Vec2_scalar_multiplication_left) {
    Vec2 v(2, 3);
    Vec2 r = 4.0f * v;
    EXPECT_EQ(r.x, 8.0f);
    EXPECT_EQ(r.y, 12.0f);
    PASS();
}

TEST(Vec2_scalar_division) {
    Vec2 v(8, 12);
    Vec2 r = v / 2.0f;
    EXPECT_EQ(r.x, 4.0f);
    EXPECT_EQ(r.y, 6.0f);
    PASS();
}

TEST(Vec2_compound_addition) {
    Vec2 v(1, 2);
    v += Vec2(3, 4);
    EXPECT_EQ(v.x, 4.0f);
    EXPECT_EQ(v.y, 6.0f);
    PASS();
}

TEST(Vec2_compound_subtraction) {
    Vec2 v(5, 7);
    v -= Vec2(2, 3);
    EXPECT_EQ(v.x, 3.0f);
    EXPECT_EQ(v.y, 4.0f);
    PASS();
}

TEST(Vec2_compound_scalar_mul) {
    Vec2 v(2, 3);
    v *= 3.0f;
    EXPECT_EQ(v.x, 6.0f);
    EXPECT_EQ(v.y, 9.0f);
    PASS();
}

TEST(Vec2_equality) {
    Vec2 a(1, 2), b(1, 2), c(1, 3);
    EXPECT_TRUE(a == b);
    EXPECT_TRUE(a != c);
    EXPECT_FALSE(a == c);
    PASS();
}

TEST(Vec2_length) {
    Vec2 v(3, 4);
    EXPECT_EQ(v.length(), 5.0f);
    EXPECT_EQ(v.length_squared(), 25.0f);
    PASS();
}

TEST(Vec2_zero_length) {
    Vec2 v(0, 0);
    EXPECT_EQ(v.length(), 0.0f);
    EXPECT_EQ(v.length_squared(), 0.0f);
    PASS();
}

TEST(Vec2_normalized) {
    Vec2 v(3, 0);
    Vec2 n = v.normalized();
    EXPECT_FLOAT_EQ(n.x, 1.0f, 0.0001f);
    EXPECT_FLOAT_EQ(n.y, 0.0f, 0.0001f);
    PASS();
}

TEST(Vec2_normalized_zero_vector) {
    Vec2 v(0, 0);
    Vec2 n = v.normalized();
    EXPECT_EQ(n.x, 0.0f);
    EXPECT_EQ(n.y, 0.0f);
    PASS();
}

TEST(Vec2_dot_product) {
    Vec2 a(1, 2), b(3, 4);
    EXPECT_EQ(a.dot(b), 11.0f); // 1*3 + 2*4 = 11
    PASS();
}

TEST(Vec2_cross_product) {
    Vec2 a(1, 0), b(0, 1);
    EXPECT_EQ(a.cross(b), 1.0f);
    EXPECT_EQ(b.cross(a), -1.0f);
    PASS();
}

TEST(lerp_function) {
    Vec2 a(0, 0), b(10, 10);
    Vec2 mid = lerp(a, b, 0.5f);
    EXPECT_EQ(mid.x, 5.0f);
    EXPECT_EQ(mid.y, 5.0f);

    Vec2 at_a = lerp(a, b, 0.0f);
    EXPECT_EQ(at_a.x, 0.0f);
    EXPECT_EQ(at_a.y, 0.0f);

    Vec2 at_b = lerp(a, b, 1.0f);
    EXPECT_EQ(at_b.x, 10.0f);
    EXPECT_EQ(at_b.y, 10.0f);
    PASS();
}

TEST(distance_function) {
    Vec2 a(0, 0), b(3, 4);
    EXPECT_EQ(distance(a, b), 5.0f);
    EXPECT_EQ(distance_squared(a, b), 25.0f);
    PASS();
}

// ============================================================================
// Rect
// ============================================================================

TEST(Rect_default_construction) {
    Rect r;
    EXPECT_EQ(r.pos.x, 0.0f);
    EXPECT_EQ(r.pos.y, 0.0f);
    EXPECT_EQ(r.size.x, 0.0f);
    EXPECT_EQ(r.size.y, 0.0f);
    PASS();
}

TEST(Rect_vec_construction) {
    Rect r(Vec2{10, 20}, Vec2{100, 200});
    EXPECT_EQ(r.x(), 10.0f);
    EXPECT_EQ(r.y(), 20.0f);
    EXPECT_EQ(r.w(), 100.0f);
    EXPECT_EQ(r.h(), 200.0f);
    PASS();
}

TEST(Rect_edges) {
    Rect r(10, 20, 100, 200);
    EXPECT_EQ(r.left(), 10.0f);
    EXPECT_EQ(r.right(), 110.0f);
    EXPECT_EQ(r.top(), 20.0f);
    EXPECT_EQ(r.bottom(), 220.0f);
    PASS();
}

TEST(Rect_center) {
    Rect r(0, 0, 100, 200);
    Vec2 c = r.center();
    EXPECT_EQ(c.x, 50.0f);
    EXPECT_EQ(c.y, 100.0f);
    PASS();
}

TEST(Rect_corners) {
    Rect r(10, 20, 100, 200);
    EXPECT_EQ(r.top_left().x, 10.0f);
    EXPECT_EQ(r.top_left().y, 20.0f);
    EXPECT_EQ(r.top_right().x, 110.0f);
    EXPECT_EQ(r.top_right().y, 20.0f);
    EXPECT_EQ(r.bottom_left().x, 10.0f);
    EXPECT_EQ(r.bottom_left().y, 220.0f);
    EXPECT_EQ(r.bottom_right().x, 110.0f);
    EXPECT_EQ(r.bottom_right().y, 220.0f);
    PASS();
}

TEST(Rect_contains_point_inside) {
    Rect r(10, 20, 100, 200);
    EXPECT_TRUE(r.contains(Vec2{50, 100}));   // center
    EXPECT_TRUE(r.contains(Vec2{10, 20}));    // top-left corner
    EXPECT_TRUE(r.contains(Vec2{110, 220}));  // bottom-right corner
    PASS();
}

TEST(Rect_contains_point_outside) {
    Rect r(10, 20, 100, 200);
    EXPECT_FALSE(r.contains(Vec2{5, 100}));    // left
    EXPECT_FALSE(r.contains(Vec2{115, 100}));  // right
    EXPECT_FALSE(r.contains(Vec2{50, 15}));    // above
    EXPECT_FALSE(r.contains(Vec2{50, 225}));   // below
    PASS();
}

TEST(Rect_overlaps) {
    Rect a(0, 0, 100, 100);
    Rect b(50, 50, 100, 100);
    EXPECT_TRUE(a.overlaps(b));
    EXPECT_TRUE(b.overlaps(a));
    PASS();
}

TEST(Rect_overlaps_no_overlap) {
    Rect a(0, 0, 100, 100);
    Rect b(101, 0, 100, 100);   // to the right
    Rect c(0, 101, 100, 100);   // below
    EXPECT_FALSE(a.overlaps(b));
    EXPECT_FALSE(a.overlaps(c));
    PASS();
}

TEST(Rect_overlaps_touching_edges) {
    Rect a(0, 0, 100, 100);
    Rect b(100, 0, 100, 100);   // exactly adjacent (right edge at left)
    // Touching edges don't overlap (left < right, not left <= right)
    EXPECT_FALSE(a.overlaps(b));
    PASS();
}

TEST(Rect_expanded) {
    Rect r(10, 20, 100, 200);
    Rect e = r.expanded(5.0f);
    EXPECT_EQ(e.left(), 5.0f);
    EXPECT_EQ(e.top(), 15.0f);
    EXPECT_EQ(e.w(), 110.0f);
    EXPECT_EQ(e.h(), 210.0f);
    PASS();
}

// ============================================================================
// Transform2D
// ============================================================================

TEST(Transform2D_identity) {
    Transform2D t;
    Vec2 canvas{100, 200};
    Vec2 center{960, 540};
    // With offset=0, scale=1: screen = canvas + center
    Vec2 screen = t.apply(canvas, center);
    EXPECT_EQ(screen.x, 1060.0f);  // 100 + 960
    EXPECT_EQ(screen.y, 740.0f);   // 200 + 540
    PASS();
}

TEST(Transform2D_apply_with_offset) {
    Transform2D t{{500, 300}, 1.0f};  // offset=(500,300), scale=1
    Vec2 center{960, 540};
    // screen = (canvas - offset) * scale + center
    Vec2 screen = t.apply(Vec2{500, 300}, center);
    EXPECT_EQ(screen.x, 960.0f);  // (500-500)*1 + 960
    EXPECT_EQ(screen.y, 540.0f);  // (300-300)*1 + 540
    PASS();
}

TEST(Transform2D_apply_with_scale) {
    Transform2D t{{0, 0}, 2.0f};
    Vec2 center{100, 100};
    // screen = (canvas - 0) * 2 + center
    Vec2 screen = t.apply(Vec2{50, 30}, center);
    EXPECT_EQ(screen.x, 200.0f);  // 50*2 + 100
    EXPECT_EQ(screen.y, 160.0f);  // 30*2 + 100
    PASS();
}

TEST(Transform2D_inverse_roundtrip) {
    Transform2D t{{200, 100}, 1.5f};
    Vec2 center{960, 540};
    Vec2 canvas{350, 280};
    Vec2 screen = t.apply(canvas, center);
    Vec2 back = t.inverse(screen, center);
    EXPECT_FLOAT_EQ(back.x, canvas.x, 0.0001f);
    EXPECT_FLOAT_EQ(back.y, canvas.y, 0.0001f);
    PASS();
}

TEST(Transform2D_apply_rect) {
    Transform2D t{{100, 100}, 2.0f};
    Vec2 center{960, 540};
    Rect canvas_rect{200, 200, 50, 30};
    Rect screen_rect = t.apply_rect(canvas_rect, center);
    // pos: (200-100)*2 + 960 = 1160, (200-100)*2 + 540 = 740
    EXPECT_EQ(screen_rect.pos.x, 1160.0f);
    EXPECT_EQ(screen_rect.pos.y, 740.0f);
    // size: 50*2 = 100, 30*2 = 60
    EXPECT_EQ(screen_rect.size.x, 100.0f);
    EXPECT_EQ(screen_rect.size.y, 60.0f);
    PASS();
}

TEST(Transform2D_inverse_rect_roundtrip) {
    Transform2D t{{200, 100}, 1.5f};
    Vec2 center{960, 540};
    Rect canvas_rect{350, 280, 400, 300};
    Rect screen_rect = t.apply_rect(canvas_rect, center);
    Rect back = t.inverse_rect(screen_rect, center);
    EXPECT_FLOAT_EQ(back.pos.x, canvas_rect.pos.x, 0.0001f);
    EXPECT_FLOAT_EQ(back.pos.y, canvas_rect.pos.y, 0.0001f);
    EXPECT_FLOAT_EQ(back.size.x, canvas_rect.size.x, 0.0001f);
    EXPECT_FLOAT_EQ(back.size.y, canvas_rect.size.y, 0.0001f);
    PASS();
}

// ============================================================================
// Config constants
// ============================================================================

TEST(Config_constants_are_sane) {
    EXPECT_TRUE(config::MIN_ZOOM > 0.0f);
    EXPECT_TRUE(config::MAX_ZOOM > config::MIN_ZOOM);
    EXPECT_TRUE(config::ZOOM_STEP > 0.0f);
    EXPECT_TRUE(config::PAN_STEP > 0.0f);
    EXPECT_TRUE(config::SNAP_DISTANCE > 0.0f);
    EXPECT_TRUE(config::GRID_SIZE > 0.0f);
    EXPECT_TRUE(config::STACK_OFFSET > 0.0f);
    EXPECT_TRUE(config::ATTRACTION_STR >= 0.0f && config::ATTRACTION_STR <= 1.0f);
    PASS();
}
