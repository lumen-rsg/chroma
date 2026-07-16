#pragma once

#include "types.hpp"
#include <string>

namespace chroma {

// ============================================================================
// ChromaWindow — domain entity for a managed window
// ============================================================================

/// Represents one application window (XDG toplevel) on the canvas.
/// Owns no Wayland resources — those live in the adapter layer.
class ChromaWindow {
public:
    WindowId id{INVALID_WINDOW};
    std::string app_id;
    std::string title;

    /// Position of the window's top-left in canvas coordinates.
    Vec2 canvas_pos{0, 0};

    /// The window's geometry size (as reported by the client).
    Vec2 size{800, 600};

    /// Affinity group this window belongs to (0 = none).
    GroupId group{NO_GROUP};

    /// Card stack this window belongs to (0 = none).
    StackId stack{NO_STACK};

    /// Whether the surface is currently mapped (has a buffer attached).
    bool mapped{false};

    /// Whether this window currently has keyboard focus.
    bool focused{false};

    // Default initial placement — center in viewport
    static inline Vec2 next_default_pos{-400, -300};
    static constexpr Vec2 default_offset{50, 50};

    ChromaWindow() = default;

    ChromaWindow(WindowId id, std::string app_id, Vec2 size = {800, 600})
        : id{id}, app_id{std::move(app_id)}, size{size}
    {
        canvas_pos = next_default_pos;
        next_default_pos = next_default_pos + default_offset;
        if (next_default_pos.x > 800.0f) next_default_pos = {-400, next_default_pos.y + 300};
        if (next_default_pos.y > 600.0f) next_default_pos = {-400, -300};
    }

    /// The window's bounding rectangle in canvas coordinates.
    Rect canvas_rect() const {
        return Rect{canvas_pos, size};
    }

    /// Center point of the window in canvas coordinates.
    Vec2 center() const {
        return canvas_pos + size * 0.5f;
    }
};

} // namespace chroma
