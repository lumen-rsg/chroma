#include "app.hpp"
#include <cstdio>
#include <csignal>

namespace chroma {

// Global pointer for signal handling
static ChromaApp* g_app = nullptr;

ChromaApp::ChromaApp()
    : xdg_handler_(&server_, &canvas_)
    , renderer_(&server_, &canvas_, &xdg_handler_, &stacks_)
    , seat_(&server_, &canvas_, &focus_, &input_router_, &stacks_, &magnetism_, &xdg_handler_)
{
    g_app = this;
}

ChromaApp::~ChromaApp() {
    // Remove all frame listeners before outputs are destroyed
    for (auto& data : output_frames_) {
        wl_list_remove(&data.frame.link);
    }
    output_frames_.clear();
    g_app = nullptr;
}

bool ChromaApp::init() {
    std::printf("[chroma] Initializing Chroma WM...\n");

    if (!server_.init()) {
        std::fprintf(stderr, "[chroma] Failed to initialize server\n");
        return false;
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

    // Set up output frame handlers
    // We need to override the server's new_output handler with our own
    // that both creates the scene output AND sets up frame callbacks
    setup_output_handlers();

    // Handle xdg_activation requests
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
    float bg_color[4] = {0.08f, 0.08f, 0.10f, 1.0f};
    wlr_scene_rect_create(&server_.scene->tree, 1920, 1080, bg_color);

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
}

void ChromaApp::on_window_destroyed(WindowId id) {
    renderer_.on_window_removed(id);
}

void ChromaApp::on_window_mapped(WindowId id, wlr_surface* surface) {
    renderer_.on_window_mapped(id, surface);

    // Auto-focus new windows
    canvas_.set_focus(id);
    focus_.focused(id);
    seat_.update_keyboard_focus(&xdg_handler_);

    // Apply magnetism
    magnetism_.apply(canvas_, id);
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
    data.frame.notify = handle_output_frame;
    wl_signal_add(&output->events.frame, &data.frame);

    // Request first frame
    wlr_output_schedule_frame(output);
}

void ChromaApp::handle_output_frame(wl_listener* listener, void* /*data*/) {
    OutputFrameData* frame_data = wl_container_of(listener, frame_data, frame);
    ChromaApp* app = g_app;

    if (!app) return;

    app->renderer_.render_frame(frame_data->scene_output, frame_data->output);
}

void ChromaApp::handle_activation_request(wl_listener* /*listener*/, void* data) {
    ChromaApp* app = g_app;
    if (!app) return;

    auto* event = static_cast<wlr_xdg_activation_v1_request_activate_event*>(data);
    if (!event || !event->token) return;

    // Token-based activation — for now just suppress the GLFW warning
    (void)app;
}

// ============================================================================
// Override server output handler
// ============================================================================

void ChromaApp::setup_output_handlers() {
    // Replace the server's new_output handler with our own
    // First remove the old one
    wl_list_remove(&server_.on_new_output.link);

    // Add our own
    server_.on_new_output.notify = [](wl_listener* /*listener*/, void* data) {
        ChromaApp* app = g_app;
        if (!app) return;

        auto* output = static_cast<wlr_output*>(data);
        auto* server = &app->server_;

        // Initialize output rendering
        wlr_output_init_render(output, server->allocator, server->renderer);

        wlr_output_state state;
        wlr_output_state_init(&state);

        if (!wl_list_empty(&output->modes)) {
            auto* mode = wlr_output_preferred_mode(output);
            wlr_output_state_set_mode(&state, mode);
        }
        wlr_output_state_set_enabled(&state, true);

        if (!wlr_output_commit_state(output, &state)) {
            std::fprintf(stderr, "Failed to commit output state\n");
            wlr_output_state_finish(&state);
            return;
        }
        wlr_output_state_finish(&state);

        auto* layout_output = wlr_output_layout_add_auto(server->output_layout, output);

        auto* scene_output = wlr_scene_output_create(server->scene, output);
        if (scene_output) {
            server->scene_outputs.push_back(scene_output);
            // Register scene output with the scene layout so it renders
            wlr_scene_output_layout_add_output(server->scene_layout, layout_output, scene_output);
        }

        std::printf("[chroma] Output '%s' added: %dx%d\n",
                    output->name, output->width, output->height);

        // Set up frame callback
        app->on_new_output(output, scene_output);
    };
    wl_signal_add(&server_.backend->events.new_output, &server_.on_new_output);
}

} // namespace chroma
