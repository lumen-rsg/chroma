#pragma once

#include "server.hpp"
#include "canvas.hpp"
#include "stack.hpp"
#include "xdg_handler.hpp"
#include <unordered_map>

namespace chroma {

// ============================================================================
// SceneRenderer — renders Canvas state via wlr_scene
// ============================================================================

/// Per-window scene data stored alongside the wlr_scene_tree.
struct WindowSceneData {
    WindowId window_id;
    wlr_scene_tree* tree;          // root node for this window
    wlr_scene_node* surface_node;   // the actual surface node (or nullptr if not mapped)
    wlr_scene_rect* border_rect{nullptr};
    wlr_scene_rect* bg_rect{nullptr};
    bool is_stacked{false};
    int stack_index{0};

    // Dirty tracking: skip scene node updates when nothing changed.
    bool was_focused{false};
    Vec2 last_screen_pos{-1, -1};    // last screen-space position (init to sentinel)
    Vec2 last_screen_size{0, 0};     // last screen-space size
    Vec2 last_stack_offset{-1, -1};  // last visual offset from card stacking
};

/// Reads Canvas domain state and updates the wlr_scene to match.
/// Gets called every frame for each output.
class SceneRenderer {
public:
    SceneRenderer(WlrootsServer* server, Canvas* canvas,
                  XdgShellHandler* xdg_handler, StackManager* stacks);

    /// Create a scene representation for a window. Call when a window is added.
    void on_window_added(WindowId id, wlr_surface* surface);

    /// Remove a window's scene representation. Call when a window is removed.
    void on_window_removed(WindowId id);

    /// Update surface association when a window first maps.
    void on_window_mapped(WindowId id, wlr_surface* surface);

    /// Update all visible window positions from canvas state,
    /// then commit the scene to the output. Called every frame.
    void render_frame(wlr_scene_output* scene_output, wlr_output* output);

    /// Get scene data for a window.
    WindowSceneData* get_scene_data(WindowId id);

private:
    WlrootsServer* server_;
    Canvas* canvas_;
    XdgShellHandler* xdg_handler_;
    StackManager* stacks_;

    std::unordered_map<WindowId, WindowSceneData> scene_data_;

    /// Update the position of a single window's scene node.
    void update_window_position(WindowId id, Vec2 screen_size, bool is_stacked, int stack_idx);

    /// Create a solid-color rect behind windows (simple decoration / background).
    wlr_scene_rect* create_background_rect(wlr_scene_tree* parent, Vec2 size);
};

} // namespace chroma
