#pragma once

/// @file magnetism.hpp
/// @brief MagnetismEngine — magnetic window positioning: snap-to-group,
/// attraction (gravity metaphor), and grid-snapping. Applied after moves,
/// resizes, and window creation for a polished spatial experience.
///
/// The Config struct documents the magic constants and their physical
/// meaning (see attraction_scale and max_attraction_force).

#include "types.hpp"
#include "canvas.hpp"
#include <vector>
#include <functional>

namespace chroma {

// ============================================================================
// MagnetismEngine — magnetic window positioning
// ============================================================================

class MagnetismEngine {
public:
    struct Config {
        float snap_distance{config::SNAP_DISTANCE};       // pixels
        float attraction_strength{config::ATTRACTION_STR}; // 0–1 (scaled by attraction_scale)
        float grid_size{config::GRID_SIZE};               // snap grid
        /// Scales attraction_strength into pixel-force range.
        /// With the default scale (10000) and strength (0.1), the force at
        /// distance=100 px is 0.1 * 10000 / 10000 = 0.1 px/call.
        float attraction_scale{10000.0f};
        /// Maximum displacement applied to a group member in one call.
        float max_attraction_force{50.0f};
    };

    Config config;

    /// Snap `window_id` to the nearest group centroid if within snap_distance.
    /// Also applies grid-snapping for ungrouped windows.
    /// Called after a window is moved, resized, or created.
    void snap_to_nearby(Canvas& canvas, WindowId window_id);

    /// Apply gentle attraction: pull all windows in the same group
    /// slightly toward `moved_id` (gravity metaphor).
    void attract_group(Canvas& canvas, WindowId moved_id);

    /// Create a group from all windows within `radius` of `center_id`.
    /// Returns the new group's ID, or NO_GROUP if no windows nearby.
    GroupId group_nearby(Canvas& canvas, WindowId center_id, float radius);

    /// Apply grid snapping to an ungrouped window's position.
    /// Returns the snapped position.
    Vec2 snap_to_grid(Vec2 pos) const;

    /// Full magnetic pass: snap + attract for a moved window.
    void apply(Canvas& canvas, WindowId moved_id);
};

// ============================================================================
// Implementation (inline helpers)
// ============================================================================

inline Vec2 MagnetismEngine::snap_to_grid(Vec2 pos) const {
    float g = config.grid_size;
    return {
        std::round(pos.x / g) * g,
        std::round(pos.y / g) * g
    };
}

inline void MagnetismEngine::apply(Canvas& canvas, WindowId moved_id) {
    snap_to_nearby(canvas, moved_id);
    attract_group(canvas, moved_id);
}

} // namespace chroma
