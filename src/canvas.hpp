#pragma once

#include "types.hpp"
#include "window.hpp"
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
// Canvas — the infinite 2D space that holds all windows
// ============================================================================

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
    /// `anchor` is in screen coordinates (typically screen center).
    void zoom_at(float factor, Vec2 anchor, Vec2 screen_size);

    /// The visible region of the canvas given a screen size.
    Rect visible_rect(Vec2 screen_size) const;

    /// Convert a canvas-space rect to screen-space.
    Rect canvas_to_screen(const Rect& r, Vec2 screen_size) const;

    /// Convert a screen-space point to canvas-space.
    Vec2 screen_to_canvas(Vec2 screen_pos, Vec2 screen_size) const;

    /// Get the current viewport transform.
    Transform2D viewport_transform(Vec2 screen_size) const;

    // --- Window management ---

    /// Add a window to the canvas. Returns its ID.
    WindowId add(ChromaWindow window);

    /// Remove a window and return its data. Window must exist.
    ChromaWindow remove(WindowId id);

    /// Move a window to a new canvas position.
    void move_window(WindowId id, Vec2 new_pos);

    /// Resize a window.
    void resize_window(WindowId id, Vec2 new_size);

    /// Get a window by ID. Returns nullptr if not found.
    ChromaWindow* get(WindowId id);
    const ChromaWindow* get(WindowId id) const;

    /// All windows, in insertion order.
    const std::unordered_map<WindowId, ChromaWindow>& all_windows() const { return windows_; }

    /// Windows that are currently visible in the viewport.
    std::vector<WindowId> visible_windows(Vec2 screen_size) const;

    /// Find the topmost window at `canvas_pos`. Returns INVALID_WINDOW if none.
    WindowId window_at(Vec2 canvas_pos) const;

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

    // --- ID generation ---

    WindowId next_window_id();
    GroupId next_group_id();
    StackId next_stack_id();

private:
    std::unordered_map<WindowId, ChromaWindow> windows_;
    std::unordered_map<GroupId, WindowGroup> groups_;
    WindowId focused_{INVALID_WINDOW};

    WindowId next_wid_{1};
    GroupId  next_gid_{1};
    StackId  next_sid_{1};
};

} // namespace chroma
