#include "app.hpp"
#include <csignal>
#include <cstdio>

namespace chroma {

// Signal handler for wl_event_loop_add_signal — receives ChromaApp* as user data
static int on_signal(int sig, void* data) {
    auto* app = static_cast<ChromaApp*>(data);
    std::fprintf(stderr, "[chroma] Received signal %d, shutting down.\n", sig);
    app->quit();
    return 0;
}

ChromaApp::ChromaApp()
    : xdg_handler_(&server_, &canvas_)
    , renderer_(&server_, &canvas_, &xdg_handler_, &stacks_)
    , seat_(&server_, &canvas_, &focus_, &input_router_, &stacks_, &magnetism_, &xdg_handler_)
{}

ChromaApp::~ChromaApp() {
    // Remove all frame listeners before outputs are destroyed
    for (auto& data : output_frames_) {
        wl_list_remove(&data.frame.link);
    }
    output_frames_.clear();
}

bool ChromaApp::init() {
    std::printf("[chroma] Initializing Chroma WM...\n");

    if (!server_.init()) {
        std::fprintf(stderr, "[chroma] Failed to initialize server\n");
        return false;
    }

    // Register signal handlers via the Wayland event loop (no global needed)
    {
        auto* loop = wl_display_get_event_loop(server_.display);
        wl_event_loop_add_signal(loop, SIGTERM, on_signal, this);
        wl_event_loop_add_signal(loop, SIGINT, on_signal, this);
    }

    // Wire XDG shell handler → domain + renderer
    xdg_handler_.on_window_created = [this](WindowId id, wlr_surface* s) {
        this->on_new_window(id, s);
    };
    xdg_handler_.on_window_mapped = [this](WindowId id, wlr_surface* s) {
        this->on_window_mapped(id, s);
    };
    xdg_handler_.on_window_removed = [this](WindowId id) {
        this->on_window_destroyed(id);
    };
    xdg_handler_.connect();

    // Wire seat manager → input
    seat_.connect();

    // Wire the server's output-created callback so we set up frame handlers
    // on top of the common output setup performed by WlrootsServer.
    server_.on_output_created = [this](wlr_output* output, wlr_scene_output* scene_output) {
        this->on_new_output(output, scene_output);
    };

    // Handle xdg_activation requests — use wl_container_of in the static handler
    if (server_.xdg_activation) {
        on_activation_request_.notify = handle_activation_request;
        wl_signal_add(&server_.xdg_activation->events.request_activate,
                      &on_activation_request_);
    }

    // NOW start the backend — all listeners are connected and will receive
    // the new_output / new_input events for existing devices.
    if (!server_.start_backend()) {
        std::fprintf(stderr, "[chroma] Failed to start backend\n");
        return false;
    }

    // Set up a background color for the scene
    // Use a very large rect so it covers any display size (up to 16K).
    float bg_color[4] = {0.08f, 0.08f, 0.10f, 1.0f};
    wlr_scene_rect_create(&server_.scene->tree, 16384, 16384, bg_color);

    std::printf("[chroma] Initialization complete.\n");
    std::printf("[chroma] WAYLAND_DISPLAY=%s\n", getenv("WAYLAND_DISPLAY"));
    std::printf("[chroma] Controls:\n");
    std::printf("[chroma]   Super+Arrows  = pan canvas\n");
    std::printf("[chroma]   Super+Plus/-  = zoom\n");
    std::printf("[chroma]   Super+Tab     = cycle focus\n");
    std::printf("[chroma]   Super+S       = cycle stack\n");
    std::printf("[chroma]   Super+Shift+S = stack window\n");
    std::printf("[chroma]   Super+G       = group nearby\n");
    std::printf("[chroma]   Super+Shift+E = quit\n");

    return true;
}

void ChromaApp::run() {
    std::printf("[chroma] Running...\n");
    server_.run();
    std::printf("[chroma] Shutting down.\n");
}

void ChromaApp::quit() {
    server_.terminate();
}

// ============================================================================
// Window lifecycle callbacks
// ============================================================================

void ChromaApp::on_new_window(WindowId id, wlr_surface* surface) {
    renderer_.on_window_added(id, surface);

    // Apply magnetic positioning
    magnetism_.apply(canvas_, id);

    server_.schedule_all_frames();
}

void ChromaApp::on_window_destroyed(WindowId id) {
    focus_.remove(id);
    renderer_.on_window_removed(id);

    server_.schedule_all_frames();
}

void ChromaApp::on_window_mapped(WindowId id, wlr_surface* surface) {
    renderer_.on_window_mapped(id, surface);

    // Auto-focus new windows
    canvas_.set_focus(id);
    focus_.focused(id);
    seat_.update_keyboard_focus(&xdg_handler_);

    // Apply magnetism
    magnetism_.apply(canvas_, id);

    server_.schedule_all_frames();
}

// ============================================================================
// Output handling
// ============================================================================

void ChromaApp::on_new_output(wlr_output* output, wlr_scene_output* scene_output) {
    std::printf("[chroma] Setting up frame handler for output '%s'\n", output->name);

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
    if (!event || !event->token) return;

    // Token-based activation — for now just suppress the GLFW warning
    (void)app;
}

} // namespace chroma
