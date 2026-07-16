#pragma once

#include "server.hpp"
#include "canvas.hpp"
#include "focus.hpp"
#include "input.hpp"
#include "stack.hpp"
#include "magnetism.hpp"
#include "xdg_handler.hpp"
#include "layer_shell.hpp"
#include "foreign_toplevel.hpp"
#include "renderer.hpp"
#include "seat.hpp"

#include <list>
#include <functional>

namespace chroma {

// ============================================================================
// ChromaApp — the application orchestrator
// ============================================================================

/// Owns all domain objects and adapters, wires them together,
/// and manages the main loop and frame rendering.
class ChromaApp {
public:
    ChromaApp();
    ~ChromaApp();

    /// Initialize everything. Returns true on success.
    bool init();

    /// Run the compositor. Blocks until quit.
    void run();

    /// Request quit.
    void quit();

private:
    // --- Domain (no wlroots deps) ---
    Canvas canvas_;
    FocusTracker focus_;
    InputRouter input_router_;
    StackManager stacks_;
    MagnetismEngine magnetism_;

    // --- Adapters (wlroots-aware) ---
    WlrootsServer server_;
    XdgShellHandler xdg_handler_;
    LayerShellHandler layer_shell_;
    ForeignToplevelHandler foreign_toplevel_;
    SceneRenderer renderer_;
    SeatManager seat_;

    // --- Per-output frame listeners ---
    struct OutputFrameData {
        wlr_output* output;
        wlr_scene_output* scene_output;
        wl_listener frame;
        ChromaApp* app;
    };
    std::list<OutputFrameData> output_frames_;

    // Activation listener
    wl_listener on_activation_request_;

    // --- Callbacks ---
    void on_new_window(WindowId id, wlr_surface* surface);
    void on_window_destroyed(WindowId id);
    void on_window_mapped(WindowId id, wlr_surface* surface);
    void on_new_output(wlr_output* output, wlr_scene_output* scene_output);

    static void handle_output_frame(wl_listener* listener, void* data);
    static void handle_activation_request(wl_listener* listener, void* data);
};

} // namespace chroma
