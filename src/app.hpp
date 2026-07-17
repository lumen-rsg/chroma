#pragma once

#include "config.hpp"
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
#include <string>

namespace chroma {

// ============================================================================
// ChromaApp — the application orchestrator
// ============================================================================

/// Owns all domain objects and adapters, wires them together,
/// and manages the main loop and frame rendering.
class ChromaApp {
public:
    /// @param config_path  Path to config.toml (empty = use default location).
    ChromaApp(std::string config_path = {});
    ~ChromaApp();

    /// Initialize everything. Returns true on success.
    bool init();

    /// Run the compositor. Blocks until quit.
    void run();

    /// Request quit. Calls terminate_spawned_children() and stops the loop.
    void quit();

    /// Reload the config file. Safe to call from signal handlers.
    void reload_config();

private:
    std::string config_path_;

    // --- Config ---
    ChromaConfig config_;

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

    // --- Signal handling ---
    static int on_signal(int sig, void* data);

    // --- Spawning ---
    void run_pre_init();
    void run_post_init();

    /// Print the bindmap to stdout (for debugging / verifying config).
    void print_bindings() const;
};

} // namespace chroma
