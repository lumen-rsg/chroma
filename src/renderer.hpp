#pragma once

#include "server.hpp"
#include "canvas.hpp"
#include "stack.hpp"
#include "xdg_handler.hpp"
#include "animation.hpp"
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
    wlr_scene_rect* bg_rect{nullptr};
    
    // Multi-layer drop shadow (4 rects behind the window)
    wlr_scene_rect* shadow_rects[4]{nullptr, nullptr, nullptr, nullptr};

    bool is_stacked{false};
    int stack_index{0};

    // --- Visual state (animated) — separate from logical Canvas state ---
    // These values smoothly chase the logical position/size each frame.
    Vec2 visual_pos{-1, -1};       // current animated screen position
    Vec2 visual_size{0, 0};        // current animated screen size
    float visual_opacity{1.0f};    // current opacity for shadows/border (0→1)

    // --- Open / close animation ---
    AnimationState open_anim;       // scale-up on map
    AnimationState close_anim;      // scale-down on unmap
    Vec2 open_origin{0, 0};        // screen-space center for scale origin

    // --- Dirty tracking: skip scene node updates when nothing changed ---
    bool was_focused{false};
    Vec2 last_screen_pos{-1, -1};    // last committed screen-space position
    Vec2 last_screen_size{0, 0};     // last committed screen-space size
    Vec2 last_stack_offset{-1, -1};  // last committed visual offset from card stacking
    float last_visual_opacity{-1.0f}; // last committed opacity

    /// Returns true if any open/close animation is active.
    bool is_animating() const {
        return open_anim.active || close_anim.active;
    }

    /// True if the visual state is still converging toward the logical state
    /// (smooth move/resize in progress).
    bool visual_converging{false};
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

    /// Signal that a window is unmapping — starts close animation.
    void on_window_unmapped(WindowId id);

    /// Update all visible window positions from canvas state,
    /// then commit the scene to the output. Called every frame.
    void render_frame(wlr_scene_output* scene_output, wlr_output* output);

    /// Get scene data for a window.
    WindowSceneData* get_scene_data(WindowId id);

    /// Returns true if any window has an active animation.
    bool has_active_animations() const;

private:
    WlrootsServer* server_;
    Canvas* canvas_;
    XdgShellHandler* xdg_handler_;
    StackManager* stacks_;

    std::unordered_map<WindowId, WindowSceneData> scene_data_;

    // Last frame timestamp for delta-time calculation.
    struct timespec last_frame_time_{0, 0};

    /// Create shadow rects for a window.
    void create_shadows(WindowSceneData& data, wlr_scene_tree* parent);

    /// Update shadow positions, sizes, and alpha for the current visual state.
    void update_shadows(WindowSceneData& data);

    /// Update the position of a single window's scene node.
    void update_window_visuals(WindowSceneData& data, const ChromaWindow& win,
                               Vec2 screen_size, float offset_x, float offset_y);

    /// Tick all animations, advancing by dt seconds.
    void tick_animations(float dt);
};

} // namespace chroma
