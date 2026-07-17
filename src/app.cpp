#include "app.hpp"
#include "config.hpp"
#include "spawn.hpp"
#include <csignal>
#include <cstdio>
#include <cstdlib>

namespace chroma {

// ============================================================================
// Signal handler
// ============================================================================

int ChromaApp::on_signal(int sig, void* data) {
    auto* app = static_cast<ChromaApp*>(data);
    if (sig == SIGHUP) {
        std::fprintf(stderr, "[chroma] Received SIGHUP, reloading config.\n");
        app->reload_config();
        return 0;
    }
    std::fprintf(stderr, "[chroma] Received signal %d, shutting down.\n", sig);
    app->quit();
    return 0;
}

// ============================================================================
// Construction / Destruction
// ============================================================================

ChromaApp::ChromaApp(std::string config_path)
    : config_path_(std::move(config_path))
    , xdg_handler_(&server_, &canvas_)
    , layer_shell_(&server_)
    , foreign_toplevel_(&server_)
    , renderer_(&server_, &canvas_, &xdg_handler_, &stacks_)
    , seat_(&server_, &canvas_, &focus_, &input_router_, &stacks_, &magnetism_, &xdg_handler_, &renderer_)
{}

ChromaApp::~ChromaApp() {
    // Clean up spawned children
    terminate_spawned_children();

    // Remove all frame listeners before outputs are destroyed
    for (auto& data : output_frames_) {
        wl_list_remove(&data.frame.link);
    }
    output_frames_.clear();
}

// ============================================================================
// Config and spawning helpers
// ============================================================================

void ChromaApp::run_pre_init() {
    for (const auto& rule : config_.pre_init) {
        std::printf("[chroma] pre-init: spawning '%s'\n", rule.program.c_str());
        spawn_process(rule.program, rule.args, rule.workdir);
    }
}

void ChromaApp::run_post_init() {
    for (const auto& rule : config_.post_init) {
        std::printf("[chroma] post-init: spawning '%s'\n", rule.program.c_str());
        spawn_process(rule.program, rule.args, rule.workdir);
    }
}

void ChromaApp::print_bindings() const {
    std::printf("[chroma] Active keybindings:\n");
    for (const auto& kb : config_.binds) {
        std::printf("[chroma]   %-24s → %s", kb.keys.c_str(), kb.action.c_str());
        if (!kb.arg.empty()) std::printf(" (%s)", kb.arg.c_str());
        std::printf("\n");
    }
    if (config_.binds.empty()) {
        std::printf("[chroma]   (using built-in defaults)\n");
    }
}

void ChromaApp::reload_config() {
    std::printf("[chroma] Reloading config from %s...\n", config_path_.c_str());

    auto result = load_config(config_path_);
    if (!result.success) {
        std::fprintf(stderr, "[chroma] Config reload failed — keeping previous config.\n");
        for (const auto& err : result.errors) {
            std::fprintf(stderr, "[chroma]   ERROR: %s\n", err.c_str());
        }
        return;
    }

    for (const auto& warn : result.warnings) {
        std::fprintf(stderr, "[chroma]   WARNING: %s\n", warn.c_str());
    }

    config_ = std::move(result.config);

    // Rebuild the keybind map
    input_router_.rebuild_bindmap();

    // Update renderer config pointer (same config_ address, but values changed)
    renderer_.set_config(&config_);

    std::printf("[chroma] Config reloaded successfully (%zu binds).\n", config_.binds.size());
    print_bindings();
}

// ============================================================================
// Init
// ============================================================================

bool ChromaApp::init() {
    std::printf("╔══════════════════════════════════╗\n");
    std::printf("║       Chroma WM  v0.1.0          ║\n");
    std::printf("║  spatial canvas window manager   ║\n");
    std::printf("╚══════════════════════════════════╝\n\n");

    // --- 1. Load config ---
    if (config_path_.empty()) {
        const char* xdg = std::getenv("XDG_CONFIG_HOME");
        if (xdg && xdg[0]) {
            config_path_ = std::string(xdg) + "/chroma/config.toml";
        } else {
            const char* home = std::getenv("HOME");
            config_path_ = home ? std::string(home) + "/.config/chroma/config.toml"
                                : "config.toml";
        }
    }

    std::printf("[chroma] Loading config from %s\n", config_path_.c_str());
    auto result = load_config(config_path_);
    if (result.success) {
        config_ = std::move(result.config);
        for (const auto& warn : result.warnings) {
            std::fprintf(stderr, "[chroma] Config warning: %s\n", warn.c_str());
        }
    } else {
        std::fprintf(stderr, "[chroma] Config errors — using built-in defaults:\n");
        for (const auto& err : result.errors) {
            std::fprintf(stderr, "[chroma]   ERROR: %s\n", err.c_str());
        }
        config_.reset();
    }

    // --- 2. Run pre-init programs ---
    run_pre_init();

    // --- 3. Initialize server ---
    std::printf("[chroma] Initializing Chroma WM...\n");

    if (!server_.init()) {
        std::fprintf(stderr, "[chroma] Failed to initialize server\n");
        return false;
    }

    // --- 4. Wire signal handlers ---
    {
        auto* loop = wl_display_get_event_loop(server_.display);
        wl_event_loop_add_signal(loop, SIGTERM, on_signal, this);
        wl_event_loop_add_signal(loop, SIGINT, on_signal, this);
        wl_event_loop_add_signal(loop, SIGHUP, on_signal, this);
    }

    // --- 5. Set up InputRouter with config ---
    input_router_.set_config(&config_);
    input_router_.rebuild_bindmap();

    // Wire input callbacks for spawn/exec/reload
    input_router_.on_spawn = [this](const std::string& arg) {
        // arg may contain program + args separated by spaces.
        // Simple split: first word is program, rest are args.
        if (arg.empty()) return;
        size_t space = arg.find(' ');
        std::string prog = arg.substr(0, space);
        std::vector<std::string> spawn_args;
        if (space != std::string::npos) {
            // Simple space-split for remaining args
            std::string rest = arg.substr(space + 1);
            size_t pos = 0;
            while (pos < rest.size()) {
                // Skip leading spaces
                while (pos < rest.size() && rest[pos] == ' ') pos++;
                if (pos >= rest.size()) break;
                size_t end = rest.find(' ', pos);
                if (end == std::string::npos) end = rest.size();
                spawn_args.push_back(rest.substr(pos, end - pos));
                pos = end;
            }
        }
        spawn_process(prog, spawn_args);
    };
    input_router_.on_exec = [this](const std::string& arg) {
        if (arg.empty()) return;
        size_t space = arg.find(' ');
        std::string prog = arg.substr(0, space);
        std::vector<std::string> exec_args;
        if (space != std::string::npos) {
            std::string rest = arg.substr(space + 1);
            size_t pos = 0;
            while (pos < rest.size()) {
                while (pos < rest.size() && rest[pos] == ' ') pos++;
                if (pos >= rest.size()) break;
                size_t end = rest.find(' ', pos);
                if (end == std::string::npos) end = rest.size();
                exec_args.push_back(rest.substr(pos, end - pos));
                pos = end;
            }
        }
        exec_process(prog, exec_args);
    };
    input_router_.on_reload_config = [this]() {
        this->reload_config();
    };

    // --- 6. Wire XDG shell handler → domain + renderer ---
    xdg_handler_.on_window_created = [this](WindowId id, wlr_surface* s) {
        this->on_new_window(id, s);
    };
    xdg_handler_.on_window_mapped = [this](WindowId id, wlr_surface* s) {
        this->on_window_mapped(id, s);
    };
    xdg_handler_.on_window_unmapped = [this](WindowId id) {
        this->renderer_.on_window_unmapped(id);
        this->server_.schedule_all_frames();
    };
    xdg_handler_.on_window_removed = [this](WindowId id) {
        this->on_window_destroyed(id);
    };
    xdg_handler_.connect();

    // Wire XDG title/app_id changes → foreign-toplevel
    xdg_handler_.on_title_changed = [this](WindowId id, const std::string& title) {
        this->foreign_toplevel_.on_title_changed(id, title);
    };
    xdg_handler_.on_app_id_changed = [this](WindowId id, const std::string& app_id) {
        this->foreign_toplevel_.on_app_id_changed(id, app_id);
    };

    // --- 7. Wire foreign-toplevel client activate requests → focus ---
    foreign_toplevel_.on_client_activate_request = [this](WindowId id) {
        this->canvas_.set_focus(id);
        this->focus_.focused(id);
        this->foreign_toplevel_.on_focus_changed(id);
        this->seat_.update_keyboard_focus(&xdg_handler_);
        this->server_.schedule_all_frames();
    };

    // Wire layer shell
    layer_shell_.connect();

    // Wire seat manager → input
    seat_.connect();

    // Prevent XDG focus changes from stealing keyboard from exclusive layer
    // surfaces (wofi, rofi, etc.). The layer shell handler tracks whether
    // any surface has exclusive keyboard interactivity.
    seat_.on_check_exclusive_focus = [this]() -> bool {
        for (auto* so : server_.scene_outputs) {
            if (layer_shell_.has_exclusive_focus(so->output)) {
                return true;
            }
        }
        return false;
    };

    // Provide the exclusive layer surface for pointer event routing.
    seat_.on_get_exclusive_surface = [this]() -> wlr_surface* {
        for (auto* so : server_.scene_outputs) {
            auto* s = layer_shell_.exclusive_surface_for(so->output);
            if (s) return s;
        }
        return nullptr;
    };
    seat_.on_get_exclusive_surface_pos = [this]() -> Vec2 {
        for (auto* so : server_.scene_outputs) {
            auto* s = layer_shell_.exclusive_surface_for(so->output);
            if (s) return layer_shell_.exclusive_surface_position(so->output);
        }
        return Vec2{0, 0};
    };

    // Wire seat focus changes → foreign-toplevel
    seat_.on_focus_changed = [this](WindowId id) {
        this->foreign_toplevel_.on_focus_changed(id);
    };

    // Wire the server's output-created callback
    server_.on_output_created = [this](wlr_output* output, wlr_scene_output* scene_output) {
        this->on_new_output(output, scene_output);
    };

    // --- 8. XDG activation requests ---
    if (server_.xdg_activation) {
        on_activation_request_.notify = handle_activation_request;
        wl_signal_add(&server_.xdg_activation->events.request_activate,
                      &on_activation_request_);
    }

    // --- 9. Pass config to renderer ---
    renderer_.set_config(&config_);

    // --- 10. Start the backend ---
    if (!server_.start_backend()) {
        std::fprintf(stderr, "[chroma] Failed to start backend\n");
        return false;
    }

    // --- 11. Background color (from config) ---
    {
        float bg_color[4];
        bg_color[0] = config_.theme.background[0];
        bg_color[1] = config_.theme.background[1];
        bg_color[2] = config_.theme.background[2];
        bg_color[3] = config_.theme.background[3];
        wlr_scene_rect_create(&server_.scene->tree, 16384, 16384, bg_color);
    }

    std::printf("[chroma] Initialization complete.\n");
    std::printf("[chroma] WAYLAND_DISPLAY=%s\n", getenv("WAYLAND_DISPLAY"));
    print_bindings();

    // --- 12. Run post-init programs ---
    run_post_init();

    return true;
}

void ChromaApp::run() {
    std::printf("[chroma] Running...\n");
    server_.run();
    std::printf("[chroma] Shutting down.\n");
}

void ChromaApp::quit() {
    terminate_spawned_children();
    server_.terminate();
}

// ============================================================================
// Window lifecycle callbacks
// ============================================================================

void ChromaApp::on_new_window(WindowId id, wlr_surface* surface) {
    renderer_.on_window_added(id, surface);

    // Notify foreign-toplevel
    auto* win = canvas_.get(id);
    if (win) {
        foreign_toplevel_.on_window_created(id, win->title, win->app_id);
    }

    // Apply magnetic positioning
    magnetism_.apply(canvas_, id);

    server_.schedule_all_frames();
}

void ChromaApp::on_window_destroyed(WindowId id) {
    focus_.remove(id);
    renderer_.on_window_removed(id);
    foreign_toplevel_.on_window_destroyed(id);

    server_.schedule_all_frames();
}

void ChromaApp::on_window_mapped(WindowId id, wlr_surface* surface) {
    renderer_.on_window_mapped(id, surface);

    // Auto-focus new windows
    canvas_.set_focus(id);
    focus_.focused(id);
    foreign_toplevel_.on_focus_changed(id);
    seat_.update_keyboard_focus(&xdg_handler_);

    // Enter the first available output so taskbars can see the window
    if (!server_.scene_outputs.empty()) {
        foreign_toplevel_.on_output_enter(id, server_.scene_outputs[0]->output);
    }

    // Apply magnetism
    magnetism_.apply(canvas_, id);

    server_.schedule_all_frames();
}

// ============================================================================
// Output handling
// ============================================================================

void ChromaApp::on_new_output(wlr_output* output, wlr_scene_output* scene_output) {
    std::printf("[chroma] Setting up frame handler for output '%s'\n", output->name);

    // Create per-output layer shell trees
    layer_shell_.on_output_created(output);

    // Register overlay/top trees with renderer so they always render above windows
    for (auto* tree : layer_shell_.overlay_and_top_trees()) {
        renderer_.register_overlay_tree(tree);
    }

    output_frames_.emplace_back();
    OutputFrameData& data = output_frames_.back();
    data.output = output;
    data.scene_output = scene_output;
    data.app = this;
    data.frame.notify = handle_output_frame;
    wl_signal_add(&output->events.frame, &data.frame);

    // Request first frame
    wlr_output_schedule_frame(output);
}

void ChromaApp::handle_output_frame(wl_listener* listener, void* /*data*/) {
    OutputFrameData* frame_data = wl_container_of(listener, frame_data, frame);
    ChromaApp* app = frame_data->app;

    if (!app) return;

    app->renderer_.render_frame(frame_data->scene_output, frame_data->output);
}

void ChromaApp::handle_activation_request(wl_listener* listener, void* data) {
    ChromaApp* app = wl_container_of(listener, app, on_activation_request_);
    if (!app) return;

    auto* event = static_cast<wlr_xdg_activation_v1_request_activate_event*>(data);
    if (!event) return;

    wlr_surface* surface = event->surface;
    if (!surface && event->token) {
        surface = event->token->surface;
    }

    if (surface) {
        auto* xdg_surface = wlr_xdg_surface_try_from_wlr_surface(surface);
        if (xdg_surface && xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
            WindowId id = app->xdg_handler_.window_for(xdg_surface->toplevel);
            if (id != INVALID_WINDOW) {
                std::printf("[chroma] Activation request for window %lu\n", id);
                app->canvas_.set_focus(id);
                app->focus_.focused(id);
                app->foreign_toplevel_.on_focus_changed(id);
                app->seat_.update_keyboard_focus(&app->xdg_handler_);
                app->server_.schedule_all_frames();
            }
        }
    }
}

} // namespace chroma
