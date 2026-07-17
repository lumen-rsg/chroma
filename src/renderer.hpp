#pragma once

#include "config.hpp"
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

    // Thin border outlines — positioned OUTSIDE the window area so they
    // never overlap the surface buffer. This avoids bleeding through
    // transparent CSD shadow regions.
    wlr_scene_rect* border_top{nullptr};
    wlr_scene_rect* border_bottom{nullptr};
    wlr_scene_rect* border_left{nullptr};
    wlr_scene_rect* border_right{nullptr};

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

    /// Collision bounce animation — purely visual, does not affect canvas position.
    JiggleState jiggle;
};

/// Reads Canvas domain state and updates the wlr_scene to match.
/// Gets called every frame for each output.
class SceneRenderer {
public:
    SceneRenderer(WlrootsServer* server, Canvas* canvas,
                  XdgShellHandler* xdg_handler, StackManager* stacks);

    /// Set the runtime config for theme values.
    void set_config(const ChromaConfig* config) { config_ = config; }

    /// Register a scene tree that must always render on top of windows.
    /// Call for layer-shell overlay and top trees (panels, launchers, etc.).
    void register_overlay_tree(wlr_scene_tree* tree);

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

    /// Trigger a collision jiggle on a window (visual-only bounce).
    /// @param id  The window to jiggle
    /// @param impact_direction  Direction of the impact (screen pixels)
    void trigger_jiggle(WindowId id, Vec2 impact_direction);

private:
    WlrootsServer* server_;
    Canvas* canvas_;
    XdgShellHandler* xdg_handler_;
    StackManager* stacks_;
    const ChromaConfig* config_{nullptr};

    std::unordered_map<WindowId, WindowSceneData> scene_data_;

    // Last frame timestamp for delta-time calculation.
    struct timespec last_frame_time_{0, 0};

    /// Create shadow rects for a window (using config for layer count).
    void create_shadows(WindowSceneData& data, wlr_scene_tree* parent);

    /// Create border outline rects for a window.
    void create_borders(WindowSceneData& data, wlr_scene_tree* parent);

    /// Update shadow positions, sizes, and alpha for the current visual state.
    void update_shadows(WindowSceneData& data);

    /// Update border outline positions, sizes, and colors.
    void update_borders(WindowSceneData& data, bool focused);

    /// Update the position of a single window's scene node.
    void update_window_visuals(WindowSceneData& data, const ChromaWindow& win,
                               Vec2 screen_size, float offset_x, float offset_y,
                               float dt);

    /// Tick all animations, advancing by dt seconds.
    void tick_animations(float dt);

    /// Synchronise wlr_scene node order with the Canvas Z-order so that
    /// topmost windows in the domain model render on top.
    void sync_scene_z_order();

    /// Scene trees that must always render above windows (layer-shell overlay/top).
    std::vector<wlr_scene_tree*> overlay_trees_;

    /// --- Group indicator HUD ---

    /// Per-indicator scene nodes: three rects forming a directional chevron.
    struct IndicatorNode {
        GroupId group_id;
        wlr_scene_rect* pieces[3];  ///< 3 rects: center spike + two wings
    };

    /// Pool of indicator scene nodes.
    std::vector<IndicatorNode> indicator_nodes_;

    /// Ensure we have exactly `count` indicator nodes, creating or
    /// destroying wlr_scene_rect children as needed.
    void ensure_indicator_nodes(size_t count);

    /// Update indicator positions, orientations, and opacities.
    void update_group_indicators(Vec2 screen_size);

    /// Color for a group based on its position in the order (config-driven palette).
    static void group_color(const ChromaConfig* config, size_t order_index,
                            float alpha, float out_color[4]);
};

} // namespace chroma
