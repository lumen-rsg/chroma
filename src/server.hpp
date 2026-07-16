#pragma once

#include <vector>
#include <string>
#include <functional>

extern "C" {
#define WLR_USE_UNSTABLE
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/allocator.h>
// wlr_scene.h and color.h are loaded from compat/ with [static N] patched out
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_touch.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
	#include <wlr/types/wlr_xdg_decoration_v1.h>
	#include <wlr/types/wlr_layer_shell_v1.h>
		#include <wlr/types/wlr_primary_selection_v1.h>
		#include <wlr/types/wlr_ext_data_control_v1.h>
		#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
		#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_tearing_control_v1.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_session_lock_v1.h>
#include <wlr/types/wlr_security_context_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
}

namespace chroma {

class WlrootsServer {
public:
    wl_display*        display{nullptr};
    wlr_backend*       backend{nullptr};
    wlr_renderer*      renderer{nullptr};
    wlr_allocator*     allocator{nullptr};
    wlr_scene*         scene{nullptr};
    wlr_scene_output_layout* scene_layout{nullptr};
    wlr_compositor*    compositor{nullptr};
    wlr_output_layout* output_layout{nullptr};
    wlr_seat*          seat{nullptr};
    wlr_cursor*        cursor{nullptr};
    wlr_xcursor_manager* xcursor_mgr{nullptr};
    wlr_xdg_shell*     xdg_shell{nullptr};
    wlr_xdg_activation_v1* xdg_activation{nullptr};
    wlr_xdg_decoration_manager_v1* xdg_decoration_mgr{nullptr};
    wlr_layer_shell_v1* layer_shell{nullptr};
    wlr_foreign_toplevel_manager_v1* foreign_toplevel_mgr{nullptr};
    wlr_screencopy_manager_v1* screencopy_mgr{nullptr};

    wl_listener on_new_output;
    wl_listener on_new_input;

    std::vector<wlr_scene_output*> scene_outputs;

    bool listeners_connected_{false};  // guards destructor cleanup

    /// Called after common output setup (init render, mode, layout, scene output).
    /// ChromaApp wires this to set up per-output frame callbacks.
    std::function<void(wlr_output*, wlr_scene_output*)> on_output_created;

    WlrootsServer() = default;
    ~WlrootsServer();

    bool init();
    bool start_backend();  // call after all listeners are connected
    void run();
    void terminate();

    /// Schedule a frame on all connected outputs. Call after any input-driven
    /// state change that needs re-rendering (pan, zoom, move, resize, etc.).
    void schedule_all_frames();

private:
    bool init_backend();
    bool init_renderer_and_allocator();
    bool init_scene();
    bool init_seat();

    static void handle_new_output(wl_listener* listener, void* data);
    static void handle_new_input(wl_listener* listener, void* data);
};

} // namespace chroma
