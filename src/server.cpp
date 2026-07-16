#include "server.hpp"
#include <cstdio>
#include <cstdlib>
#include <ctime>

namespace chroma {

WlrootsServer::~WlrootsServer() {
    if (listeners_connected_) {
        // Remove backend listeners before destroying anything
        wl_list_remove(&on_new_output.link);
        wl_list_remove(&on_new_input.link);
    }

    if (xcursor_mgr) wlr_xcursor_manager_destroy(xcursor_mgr);
    if (display) wl_display_destroy(display);
}

bool WlrootsServer::init() {
    display = wl_display_create();
    if (!display) {
        std::fprintf(stderr, "Failed to create wl_display\n");
        return false;
    }

    // Init listener links so cleanup is safe even if never connected
    wl_list_init(&on_new_output.link);
    wl_list_init(&on_new_input.link);

    if (!init_backend()) return false;
    if (!init_renderer_and_allocator()) return false;
    if (!init_scene()) return false;

    // Output layout — must be created BEFORE init_seat() so the cursor
    // can attach to it (required for absolute pointer events).
    output_layout = wlr_output_layout_create(display);

    // Attach the scene to the output layout — required for rendering
    scene_layout = wlr_scene_attach_output_layout(scene, output_layout);

    if (!init_seat()) return false;

    // Create standard protocols
    compositor = wlr_compositor_create(display, 6, renderer);
    if (!compositor) {
        std::fprintf(stderr, "Failed to create compositor\n");
        return false;
    }

    // Data device manager (for clipboard / drag-drop)
    wlr_data_device_manager_create(display);

    // Primary selection (middle-click paste)
    wlr_primary_selection_v1_device_manager_create(display);

    // Clipboard manager protocol (for clipboard persistence via external managers)
    wlr_ext_data_control_manager_v1_create(display, 1);

    // Subcompositor — required by many clients (kitty, firefox, etc.)
    wlr_subcompositor_create(display);

    // XDG shell
    xdg_shell = wlr_xdg_shell_create(display, 6);
    if (!xdg_shell) {
        std::fprintf(stderr, "Failed to create XDG shell\n");
        return false;
    }

    // XDG activation (for focus-stealing prevention and activation requests)
    xdg_activation = wlr_xdg_activation_v1_create(display);
    if (!xdg_activation) {
        std::fprintf(stderr, "Failed to create XDG activation\n");
        // non-fatal
    }

    // XDG decoration (so clients can negotiate CSD/SSD mode)
    xdg_decoration_mgr = wlr_xdg_decoration_manager_v1_create(display);
    if (!xdg_decoration_mgr) {
        std::fprintf(stderr, "Failed to create XDG decoration manager\n");
        // non-fatal
    }

    // Layer shell (for panels, wallpapers, overlays, lock screens)
    layer_shell = wlr_layer_shell_v1_create(display, 4);
    if (!layer_shell) {
        std::fprintf(stderr, "Failed to create layer shell\n");
        // non-fatal — the compositor still runs but without bars/overlays
    }

    // Foreign toplevel (for taskbars, docks, window switchers)
    foreign_toplevel_mgr = wlr_foreign_toplevel_manager_v1_create(display);
    if (!foreign_toplevel_mgr) {
        std::fprintf(stderr, "Failed to create foreign-toplevel manager\n");
        // non-fatal
    }

    // Screencopy (for screenshots: grim, slurp, wf-recorder, OBS)
    screencopy_mgr = wlr_screencopy_manager_v1_create(display);
    if (!screencopy_mgr) {
        std::fprintf(stderr, "Failed to create screencopy manager\n");
        // non-fatal
    }

    // Fractional scale (for HiDPI fractional scaling)
    if (!wlr_fractional_scale_manager_v1_create(display, 1)) {
        std::fprintf(stderr, "Failed to create fractional-scale manager\n");
        // non-fatal
    }

    // Presentation time (for frame timing, video sync)
    if (!wlr_presentation_create(display, backend, 1)) {
        std::fprintf(stderr, "Failed to create presentation-time global\n");
        // non-fatal
    }

    // Viewporter (for crop/scale surface views)
    if (!wlr_viewporter_create(display)) {
        std::fprintf(stderr, "Failed to create viewporter\n");
        // non-fatal
    }

    // Listen for backend events — these will fire when start_backend() is called
    on_new_output.notify = handle_new_output;
    wl_signal_add(&backend->events.new_output, &on_new_output);

    on_new_input.notify = handle_new_input;
    wl_signal_add(&backend->events.new_input, &on_new_input);

    listeners_connected_ = true;

    // Set environment for clients (before backend start so socket is ready)
    const char* socket_name = wl_display_add_socket_auto(display);
    if (!socket_name) {
        std::fprintf(stderr, "Failed to create Wayland socket\n");
        return false;
    }
    setenv("WAYLAND_DISPLAY", socket_name, 1);

    // NOTE: backend is NOT started here. Call start_backend() after all
    // external listeners (SeatManager, output handlers) are connected.

    return true;
}

bool WlrootsServer::start_backend() {
    if (!wlr_backend_start(backend)) {
        std::fprintf(stderr, "Failed to start backend\n");
        return false;
    }
    return true;
}

bool WlrootsServer::init_backend() {
    backend = wlr_backend_autocreate(wl_display_get_event_loop(display), nullptr);
    if (!backend) {
        std::fprintf(stderr, "Failed to create wlroots backend\n");
        return false;
    }
    return true;
}

bool WlrootsServer::init_renderer_and_allocator() {
    renderer = wlr_renderer_autocreate(backend);
    if (!renderer) {
        std::fprintf(stderr, "Failed to create renderer\n");
        return false;
    }

    wlr_renderer_init_wl_display(renderer, display);

    allocator = wlr_allocator_autocreate(backend, renderer);
    if (!allocator) {
        std::fprintf(stderr, "Failed to create allocator\n");
        return false;
    }

    return true;
}

bool WlrootsServer::init_scene() {
    scene = wlr_scene_create();
    if (!scene) {
        std::fprintf(stderr, "Failed to create scene\n");
        return false;
    }
    return true;
}

bool WlrootsServer::init_seat() {
    seat = wlr_seat_create(display, "seat0");
    if (!seat) {
        std::fprintf(stderr, "Failed to create seat\n");
        return false;
    }

    // Advertise keyboard and pointer support so clients bind the protocols
    wlr_seat_set_capabilities(seat,
        WL_SEAT_CAPABILITY_KEYBOARD | WL_SEAT_CAPABILITY_POINTER |
        WL_SEAT_CAPABILITY_TOUCH);

    cursor = wlr_cursor_create();
    if (!cursor) {
        std::fprintf(stderr, "Failed to create cursor\n");
        return false;
    }
    wlr_cursor_attach_output_layout(cursor, output_layout);

    xcursor_mgr = wlr_xcursor_manager_create(nullptr, 24);
    if (!xcursor_mgr) {
        std::fprintf(stderr, "Failed to create xcursor manager\n");
        return false;
    }

    return true;
}

void WlrootsServer::run() {
    wl_display_run(display);
}

void WlrootsServer::terminate() {
    wl_display_terminate(display);
}

void WlrootsServer::schedule_all_frames() {
    for (auto* scene_output : scene_outputs) {
        wlr_output_schedule_frame(scene_output->output);
    }
}

// ============================================================================
// Backend event handlers
// ============================================================================

void WlrootsServer::handle_new_output(wl_listener* listener, void* data) {
    WlrootsServer* server = wl_container_of(listener, server, on_new_output);
    wlr_output* output = static_cast<wlr_output*>(data);

    // Initialize the output for rendering
    wlr_output_init_render(output, server->allocator, server->renderer);

    // Configure the output using the state API (wlroots 0.20+)
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

    // Add to output layout
    auto* layout_output = wlr_output_layout_add_auto(server->output_layout, output);

    // Create scene output
    auto* scene_output = wlr_scene_output_create(server->scene, output);
    if (scene_output) {
        server->scene_outputs.push_back(scene_output);
        // Register with the scene layout so it renders
        wlr_scene_output_layout_add_output(server->scene_layout, layout_output, scene_output);
    }

    std::printf("[chroma] Output '%s' added: %dx%d\n",
                output->name, output->width, output->height);

    // Notify the app layer so it can set up per-output frame callbacks
    if (server->on_output_created) {
        server->on_output_created(output, scene_output);
    }
}

void WlrootsServer::handle_new_input(wl_listener* listener, void* data) {
    WlrootsServer* server = wl_container_of(listener, server, on_new_input);
    wlr_input_device* device = static_cast<wlr_input_device*>(data);

    // We handle input through the SeatManager — just log here
    const char* type_str = "unknown";
    switch (device->type) {
        case WLR_INPUT_DEVICE_KEYBOARD: type_str = "keyboard"; break;
        case WLR_INPUT_DEVICE_POINTER:  type_str = "pointer";  break;
        case WLR_INPUT_DEVICE_TOUCH:    type_str = "touch";    break;
        case WLR_INPUT_DEVICE_TABLET:   type_str = "tablet";   break;
        case WLR_INPUT_DEVICE_TABLET_PAD: type_str = "tablet_pad"; break;
        case WLR_INPUT_DEVICE_SWITCH:   type_str = "switch";   break;
    }
    std::printf("[chroma] Input device '%s': %s\n", device->name, type_str);
}

} // namespace chroma
