#pragma once

/// @file window.hpp
/// @brief ChromaWindow — the domain entity representing a managed toplevel window.
///
/// Owns no Wayland or wlroots resources; those live in the adapter layer
/// (XdgShellHandler). This separation keeps the domain pure and testable.

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

    ChromaWindow() = default;

    ChromaWindow(WindowId id, std::string app_id, Vec2 size = {800, 600})
        : id{id}, app_id{std::move(app_id)}, size{size}
    {}

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
