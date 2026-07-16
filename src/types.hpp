#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace chroma {

// ============================================================================
// Math Primitives
// ============================================================================

struct Vec2 {
    float x{0.0f};
    float y{0.0f};

    constexpr Vec2() = default;
    constexpr Vec2(float x, float y) : x{x}, y{y} {}

    Vec2 operator-() const { return {-x, -y}; }

    Vec2 operator+(Vec2 other) const { return {x + other.x, y + other.y}; }
    Vec2 operator-(Vec2 other) const { return {x - other.x, y - other.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2 operator/(float s) const { return {x / s, y / s}; }

    Vec2& operator+=(Vec2 other) { x += other.x; y += other.y; return *this; }
    Vec2& operator-=(Vec2 other) { x -= other.x; y -= other.y; return *this; }
    Vec2& operator*=(float s) { x *= s; y *= s; return *this; }

    bool operator==(Vec2 other) const { return x == other.x && y == other.y; }
    bool operator!=(Vec2 other) const { return !(*this == other); }

    float length() const { return std::sqrt(x * x + y * y); }
    float length_squared() const { return x * x + y * y; }

    Vec2 normalized() const {
        float len = length();
        return len > 0.0f ? *this / len : Vec2{0, 0};
    }

    float dot(Vec2 other) const { return x * other.x + y * other.y; }
    float cross(Vec2 other) const { return x * other.y - y * other.x; }
};

inline Vec2 operator*(float s, Vec2 v) { return v * s; }

inline Vec2 lerp(Vec2 a, Vec2 b, float t) {
    return a + (b - a) * t;
}

inline float distance(Vec2 a, Vec2 b) {
    return (b - a).length();
}

inline float distance_squared(Vec2 a, Vec2 b) {
    return (b - a).length_squared();
}

// ============================================================================
// Axis-aligned Rectangle
// ============================================================================

struct Rect {
    Vec2 pos{0, 0};   // top-left corner
    Vec2 size{0, 0};  // width, height

    Rect() = default;
    Rect(Vec2 pos, Vec2 size) : pos{pos}, size{size} {}
    Rect(float x, float y, float w, float h) : pos{x, y}, size{w, h} {}

    float x() const { return pos.x; }
    float y() const { return pos.y; }
    float w() const { return size.x; }
    float h() const { return size.y; }

    float left() const { return pos.x; }
    float right() const { return pos.x + size.x; }
    float top() const { return pos.y; }
    float bottom() const { return pos.y + size.y; }

    Vec2 center() const { return pos + size * 0.5f; }
    Vec2 top_left() const { return pos; }
    Vec2 top_right() const { return {right(), top()}; }
    Vec2 bottom_left() const { return {left(), bottom()}; }
    Vec2 bottom_right() const { return {right(), bottom()}; }

    bool contains(Vec2 point) const {
        return point.x >= left() && point.x <= right()
            && point.y >= top()  && point.y <= bottom();
    }

    bool overlaps(const Rect& other) const {
        return left() < other.right()  && right() > other.left()
            && top()  < other.bottom() && bottom() > other.top();
    }

    Rect expanded(float margin) const {
        return {pos - Vec2{margin, margin}, size + Vec2{margin * 2, margin * 2}};
    }
};

// ============================================================================
// 2D Transform (translation + uniform scale)
// ============================================================================

struct Transform2D {
    Vec2 offset{0, 0};  // translation (viewport center in canvas space)
    float scale{1.0f};  // uniform zoom factor

    /// Transform from canvas space → screen space.
    /// screen = (canvas - offset) * scale + screen_center
    Vec2 apply(Vec2 canvas_point, Vec2 screen_center) const {
        return (canvas_point - offset) * scale + screen_center;
    }

    /// Transform from screen space → canvas space.
    /// canvas = (screen - screen_center) / scale + offset
    Vec2 inverse(Vec2 screen_point, Vec2 screen_center) const {
        return (screen_point - screen_center) / scale + offset;
    }

    Rect apply_rect(const Rect& canvas_rect, Vec2 screen_center) const {
        return Rect{
            apply(canvas_rect.pos, screen_center),
            canvas_rect.size * scale
        };
    }

    Rect inverse_rect(const Rect& screen_rect, Vec2 screen_center) const {
        return Rect{
            inverse(screen_rect.pos, screen_center),
            screen_rect.size / scale
        };
    }
};

// ============================================================================
// ID Types
// ============================================================================

using WindowId = uint64_t;
using GroupId  = uint64_t;
using StackId  = uint64_t;

constexpr WindowId INVALID_WINDOW = 0;
constexpr GroupId  NO_GROUP       = 0;
constexpr StackId  NO_STACK       = 0;

// ============================================================================
// Viewport Constants
// ============================================================================

namespace config {
    constexpr float MIN_ZOOM       = 0.25f;
    constexpr float MAX_ZOOM       = 4.0f;
    constexpr float ZOOM_STEP      = 0.1f;
    constexpr float PAN_STEP       = 100.0f;   // pixels per key press in canvas space
    constexpr float SNAP_DISTANCE  = 200.0f;   // pixels for magnetic snap
    constexpr float GRID_SIZE      = 50.0f;    // snap grid resolution
    constexpr float STACK_OFFSET   = 8.0f;     // visual offset per card in a stack
    constexpr float ATTRACTION_STR = 0.1f;     // magnetism pull strength (0-1)
}

} // namespace chroma
