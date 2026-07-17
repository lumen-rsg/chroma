#pragma once

#include "types.hpp"
#include "server.hpp"
#include <unordered_map>
#include <vector>

namespace chroma {

// ============================================================================
// LayerShellHandler — wlr_layer_shell_v1 adapter
// ============================================================================

class LayerShellHandler;

/// Per-surface tracking data for a single layer surface.
struct LayerSurfaceData {
    LayerShellHandler* handler{nullptr};
    wlr_layer_surface_v1* layer_surface{nullptr};
    wlr_scene_layer_surface_v1* scene_layer_surface{nullptr};
    wlr_output* output{nullptr};
    bool mapped{false};

    // Listeners
    wl_listener surface_map;
    wl_listener surface_unmap;
    wl_listener surface_commit;
    wl_listener surface_destroy;
};

/// Four wlr_scene trees for a single output, one per layer.
struct OutputLayers {
    wlr_scene_tree* background{nullptr};
    wlr_scene_tree* bottom{nullptr};
    wlr_scene_tree* top{nullptr};
    wlr_scene_tree* overlay{nullptr};
};

/// Handles the wlr_layer_shell_v1 protocol: creates scene representations,
/// manages per-output layer trees, and arranges layer surfaces each frame.
class LayerShellHandler {
public:
    explicit LayerShellHandler(WlrootsServer* server);
    ~LayerShellHandler();

    /// Connect to the layer-shell new_surface signal.
    void connect();

    /// Create per-output layer trees when a new output appears.
    void on_output_created(wlr_output* output);

    /// Clean up per-output layer trees when an output is removed.
    void on_output_removed(wlr_output* output);

    /// Recompute positions for all layer surfaces on an output.
    /// Must be called after any layer surface state change
    /// (map, unmap, commit, destroy, layer change).
    void arrange_layers(wlr_output* output);

    /// After arrangement, find and focus the topmost exclusive surface
    /// on overlay/top. Returns true if an exclusive surface claimed focus.
    bool update_keyboard_focus(wlr_output* output);

    /// Check if the given output has an exclusive layer surface that
    /// currently owns keyboard focus. Pure query, no side effects.
    bool has_exclusive_focus(wlr_output* output) const;

    /// Return the wlr_surface of the topmost exclusive layer surface
    /// on the given output, or nullptr if none.
    wlr_surface* exclusive_surface_for(wlr_output* output) const;

    /// Return the screen position of the topmost exclusive layer surface
    /// on the given output. Returns (0,0) if none.
    Vec2 exclusive_surface_position(wlr_output* output) const;

    /// Return all overlay and top layer scene trees (for renderer z-order fixup).
    /// The renderer must re-raise these above windows each frame.
    std::vector<wlr_scene_tree*> overlay_and_top_trees() const;

private:
    WlrootsServer* server_;

    /// Map from wlr_output* → per-output layer scene trees.
    std::unordered_map<wlr_output*, OutputLayers> output_layers_;

    /// Map from wlr_layer_surface_v1* → per-surface tracking data.
    std::unordered_map<wlr_layer_surface_v1*, LayerSurfaceData> surfaces_;

    // --- Static signal handlers ---

    static void handle_new_surface(wl_listener* listener, void* data);

    wl_listener on_new_surface_;

    // --- helpers ---

    wlr_scene_tree* layer_tree_for(wlr_output* output,
                                   enum zwlr_layer_shell_v1_layer layer);
    void arrange_layer(wlr_output* output, OutputLayers& layers,
                       enum zwlr_layer_shell_v1_layer layer,
                       wlr_box* full_area, wlr_box* usable_area,
                       bool exclusive_only);
};

} // namespace chroma
