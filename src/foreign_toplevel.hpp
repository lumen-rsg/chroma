#pragma once

#include "server.hpp"
#include "types.hpp"
#include <functional>
#include <unordered_map>
#include <memory>

namespace chroma {

// ============================================================================
// ForeignToplevelHandler — ext_foreign_toplevel_list_v1 adapter
// ============================================================================

/// Per-handle tracking data (analogous to ToplevelData in XdgShellHandler).
struct ForeignToplevelData {
    class ForeignToplevelHandler* handler{nullptr};
    WindowId window_id{INVALID_WINDOW};
    wlr_foreign_toplevel_handle_v1* handle{nullptr};

    /// Listeners on the handle's request events.
    wl_listener request_activate;
    wl_listener request_maximize;
    wl_listener request_minimize;
    wl_listener request_fullscreen;
    wl_listener request_close;
    wl_listener handle_destroy;
};

/// Manages wlr_foreign_toplevel_handle_v1 lifecycle: creates handles
/// for each Chroma window so taskbars/docks/switchers can list and
/// interact with them.
class ForeignToplevelHandler {
public:
    explicit ForeignToplevelHandler(WlrootsServer* server);
    ~ForeignToplevelHandler();

    /// Returns the foreign-toplevel manager (created in server init).
    wlr_foreign_toplevel_manager_v1* manager() const;

    // --- Window lifecycle ---

    /// Create a foreign-toplevel handle for a new window.
    void on_window_created(WindowId id, const std::string& title,
                           const std::string& app_id);

    /// Destroy the handle for a closing window.
    void on_window_destroyed(WindowId id);

    /// Update the title on the handle.
    void on_title_changed(WindowId id, const std::string& title);

    /// Update the app_id on the handle.
    void on_app_id_changed(WindowId id, const std::string& app_id);

    /// Mark the given window as activated; clear activation on all others.
    void on_focus_changed(WindowId id);

    /// Register that a window is present on an output.
    void on_output_enter(WindowId id, wlr_output* output);

    /// Callback invoked when a client requests activation of a window
    /// (e.g. from a taskbar). The app layer should wire this to focus
    /// the window and update keyboard state.
    std::function<void(WindowId)> on_client_activate_request;

private:
    WlrootsServer* server_;
    wlr_foreign_toplevel_manager_v1* mgr_{nullptr};

    /// Map from our WindowId → foreign-toplevel handle.
    std::unordered_map<WindowId, wlr_foreign_toplevel_handle_v1*> handles_;

    /// Per-handle tracking data (listeners), keyed by handle pointer.
    std::unordered_map<wlr_foreign_toplevel_handle_v1*,
                       std::unique_ptr<ForeignToplevelData>> data_;

    WindowId active_{INVALID_WINDOW};

    // --- Static signal handlers (one set, reused via container_of → ForeignToplevelData) ---

    static void handle_request_activate(wl_listener* listener, void* data);
    static void handle_request_maximize(wl_listener* listener, void* data);
    static void handle_request_minimize(wl_listener* listener, void* data);
    static void handle_request_fullscreen(wl_listener* listener, void* data);
    static void handle_request_close(wl_listener* listener, void* data);
    static void handle_destroy(wl_listener* listener, void* data);
};

} // namespace chroma
