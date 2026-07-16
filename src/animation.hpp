#pragma once

/// @file animation.hpp
/// @brief Easing functions and animation state tracking for Chroma WM.
///
/// All animations are driven by the per-frame render loop in SceneRenderer.
/// Visual properties (position, size, opacity) smoothly chase their logical
/// (canvas) values using these easing functions.

#include <cmath>
#include <algorithm>

namespace chroma {

// ============================================================================
// Scalar interpolation
// ============================================================================

/// Linearly interpolate between two float values.
inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// ============================================================================
// Easing functions
// ============================================================================
// All functions take t in [0, 1] and return a value in [0, 1].
// They can overshoot (e.g. ease_out_back goes above 1.0 before settling).

/// Decelerating cubic ease-out: starts fast, ends slow.
inline float ease_out_cubic(float t) {
    float u = 1.0f - t;
    return 1.0f - u * u * u;
}

/// Accelerating then decelerating cubic ease-in-out.
inline float ease_in_out_cubic(float t) {
    if (t < 0.5f) {
        return 4.0f * t * t * t;
    } else {
        float u = -2.0f * t + 2.0f;
        return 1.0f - u * u * u * 0.5f;
    }
}

/// Ease-out with overshoot (back easing).
/// s controls the overshoot amount (default 1.70158 gives ~10% overshoot).
inline float ease_out_back(float t, float s = 1.70158f) {
    float u = t - 1.0f;
    return u * u * ((s + 1.0f) * u + s) + 1.0f;
}

// ============================================================================
// Animation state for open/close transitions
// ============================================================================

/// Tracks progress of a timed animation (open or close).
struct AnimationState {
    bool active{false};
    float elapsed{0.0f};    // seconds since animation started
    float duration{0.25f};  // total duration in seconds

    /// Start or restart the animation with the given duration.
    void start(float dur) {
        active = true;
        elapsed = 0.0f;
        duration = dur > 0.0f ? dur : 0.25f;
    }

    /// Advance the animation by dt seconds. Returns true if still active.
    bool tick(float dt) {
        if (!active) return false;
        elapsed += dt;
        if (elapsed >= duration) {
            elapsed = duration;
            active = false;
            return false;
        }
        return true;
    }

    /// Normalized progress [0, 1].
    float progress() const {
        if (duration <= 0.0f) return 1.0f;
        return std::clamp(elapsed / duration, 0.0f, 1.0f);
    }

    /// Stop the animation.
    void stop() {
        active = false;
        elapsed = 0.0f;
    }
};

} // namespace chroma
