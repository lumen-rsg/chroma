#pragma once

/// @file canvas.hpp
/// @brief The Canvas — the infinite 2D spatial model that holds all windows.
///
/// The Canvas is the heart of Chroma WM's spatial paradigm. It manages window
/// CRUD, viewport transform (pan/zoom), Z-ordering, and window grouping.
/// All coordinates are in "canvas space" — an unbounded 2D plane — unless
/// explicitly noted as screen space.

#include "types.hpp"
#include "window.hpp"
#include "animation.hpp"
#include <unordered_map>
#include <vector>
#include <string>

namespace chroma {

// ============================================================================
// WindowGroup — a named cluster of windows with magnetic affinity
// ============================================================================

struct WindowGroup {
    GroupId id{NO_GROUP};
    std::string name;
    std::vector<WindowId> windows;
    float magnetism_strength{config::ATTRACTION_STR};

    /// Center of mass of all windows in this group.
    Vec2 centroid(const class Canvas& canvas) const;

    /// True if this group is empty.
    bool empty() const { return windows.empty(); }
    size_t size() const { return windows.size(); }
};

// ============================================================================
// GroupIndicator — HUD element pointing toward an off-viewport group
// ============================================================================

/// Describes a directional indicator to render at the screen edge,
/// pointing toward a group that is not fully visible in the viewport.
struct GroupIndicator {
    GroupId group_id;
    std::string name;
    Vec2 screen_edge_pos;   ///< Screen-space position on the edge for arrow placement
    Vec2 direction;          ///< Normalized direction from viewport center toward the group
    float opacity{0.0f};    ///< 0→1 fade based on distance (1 = fully opaque)
};

// ============================================================================
// Canvas — the infinite 2D space that holds all windows
// ============================================================================

/// The infinite 2D spatial model that holds all managed windows.
///
/// Windows live at arbitrary (x, y) positions on an unbounded plane.
/// The viewport defines the region currently visible on screen:
///   - viewport_center: the canvas-space point at the center of the screen
///   - zoom: uniform scale factor (0.25–4.0)
///
/// Window ordering is tracked via an explicit Z-order list for deterministic
/// hit-testing, topmost-window-first.
class Canvas {
public:
    // --- Viewport state ---

    /// The point in canvas space that the viewport is centered on.
    Vec2 viewport_center{0, 0};

    /// Uniform zoom factor (0.25–4.0).
    float zoom{1.0f};

    /// Pan the viewport by `delta` canvas units.
    /// `delta` is in canvas space (already zoom-compensated).
    void pan(Vec2 delta);

    /// Zoom by `factor` (multiplicative), keeping `anchor` fixed on screen.
    /// @param factor  Multiplicative zoom factor (> 1 zooms in, < 1 zooms out)
    /// @param anchor  Screen-space point to keep fixed (typically cursor position)
    /// @param screen_size  Current output dimensions in pixels
    void zoom_at(float factor, Vec2 anchor, Vec2 screen_size);

    /// The visible region of the canvas given a screen size.
    /// @param screen_size  Current output dimensions in pixels
    /// @return  Axis-aligned rectangle in canvas space visible on screen
    Rect visible_rect(Vec2 screen_size) const;

    /// Convert a canvas-space rect to screen-space.
    Rect canvas_to_screen(const Rect& r, Vec2 screen_size) const;

    /// Convert a screen-space point to canvas-space.
    /// @param screen_pos  Pixel position on the output
    /// @param screen_size  Current output dimensions in pixels
    /// @return  Corresponding point in canvas space
    Vec2 screen_to_canvas(Vec2 screen_pos, Vec2 screen_size) const;

    /// Get the current viewport transform.
    Transform2D viewport_transform(Vec2 screen_size) const;

    // --- Window management ---

    /// Add a window to the canvas. Returns its ID.
    /// @param window  The window to add (its ID will be assigned if INVALID_WINDOW)
    /// @return  The assigned window ID
    WindowId add(ChromaWindow window);

    /// Remove a window and return its data. Window must exist.
    /// @param id  The window to remove
    /// @return  The removed window's data (for transferring state)
    ChromaWindow remove(WindowId id);

    /// Move a window to a new canvas position.
    /// @param id  Window to move
    /// @param new_pos  New top-left corner in canvas space
    void move_window(WindowId id, Vec2 new_pos);

    /// Resize a window.
    /// @param id  Window to resize
    /// @param new_size  New dimensions in canvas space (width, height)
    void resize_window(WindowId id, Vec2 new_size);

    /// Get a window by ID. Returns nullptr if not found.
    ChromaWindow* get(WindowId id);
    const ChromaWindow* get(WindowId id) const;

    /// All windows, in insertion order.
    const std::unordered_map<WindowId, ChromaWindow>& all_windows() const { return windows_; }

    /// Windows that are currently visible in the viewport.
    /// @param screen_size  Current output dimensions in pixels
    /// @return  IDs of windows whose bounding boxes intersect the viewport
    std::vector<WindowId> visible_windows(Vec2 screen_size) const;

    /// Find the topmost window at `canvas_pos`. Returns INVALID_WINDOW if none.
    /// Iterates the Z-order list from top to bottom.
    /// @param canvas_pos  Point in canvas space to test
    /// @return  The topmost window containing the point, or INVALID_WINDOW
    WindowId window_at(Vec2 canvas_pos) const;

    /// Raise a window to the top of the Z-order.
    void raise_to_top(WindowId id);

    /// Read-only access to the Z-order list (index 0 = bottom, back() = top).
    const std::vector<WindowId>& z_order() const { return z_order_; }

    // --- Focus ---

    void set_focus(WindowId id);
    WindowId focused_window() const { return focused_; }

    // --- Group management ---

    GroupId create_group(std::string name);
    void destroy_group(GroupId id);
    void add_to_group(WindowId window, GroupId group);
    void remove_from_group(WindowId window);

    WindowGroup* get_group(GroupId id);
    const WindowGroup* get_group(GroupId id) const;
    const std::unordered_map<GroupId, WindowGroup>& all_groups() const { return groups_; }

    // --- Group navigation ---

    /// Ordered list of group IDs in creation order (for deterministic cycling).
    const std::vector<GroupId>& group_order() const { return group_order_; }

    /// The group the viewport is currently targeting (via jump_to_group).
    GroupId active_group() const { return active_group_; }

    /// Number of groups currently on the canvas.
    size_t group_count() const { return group_order_.size(); }

    /// Get the group at a 1-based index in the order list (for Super+1..9).
    /// Returns NO_GROUP if the index is out of range.
    GroupId group_at_index(int index) const;

    /// Animate the viewport to center on a group's centroid with smart zoom.
    void jump_to_group(GroupId id, Vec2 screen_size);

    /// Jump to the next group in creation order (wraps around).
    GroupId cycle_next_group(Vec2 screen_size);

    /// Jump to the previous group in creation order (wraps around).
    GroupId cycle_prev_group(Vec2 screen_size);

    // --- Viewport animation ---

    /// Advance the viewport animation by dt seconds.
    /// Returns true while the animation is still active.
    bool tick_viewport_animation(float dt);

    /// True while the viewport is animating toward a target.
    bool viewport_animating() const;

    // --- Directional indicators ---

    /// Compute HUD indicators for groups whose centroids are outside the
    /// current viewport. Each indicator describes where on the screen edge
    /// to draw an arrow pointing toward that group.
    std::vector<GroupIndicator> compute_indicators(Vec2 screen_size) const;

    // --- ID generation ---

    WindowId next_window_id();
    GroupId next_group_id();
    StackId next_stack_id();

    /// Return the next default canvas position for a new window,
    /// advancing the internal counter (cascade layout).
    Vec2 next_default_position();

private:
    std::unordered_map<WindowId, ChromaWindow> windows_;
    std::unordered_map<GroupId, WindowGroup> groups_;
    WindowId focused_{INVALID_WINDOW};

    /// Z-order list: index 0 = bottom, back() = topmost.
    /// Maintained in sync with windows_; window_at iterates this in reverse.
    std::vector<WindowId> z_order_;

    /// Ordered group list in creation order for deterministic cycling.
    std::vector<GroupId> group_order_;

    /// The group currently targeted by the viewport (last jump target).
    GroupId active_group_{NO_GROUP};

    /// Viewport animation state for smooth group-jump transitions.
    ViewportAnimation viewport_anim_;

    WindowId next_wid_{1};
    GroupId  next_gid_{1};
    StackId  next_sid_{1};

    /// Default-placement cursor: each new window cascades diagonally
    /// relative to the current viewport center (so windows appear where
    /// the user is looking, not at the canvas origin).
    Vec2 next_default_rel_pos_{-700, -500};
    static constexpr Vec2 default_offset_{100, 80};
};

} // namespace chroma
