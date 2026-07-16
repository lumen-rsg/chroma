#pragma once

#include "server.hpp"
#include "canvas.hpp"
#include <functional>
#include <unordered_map>

namespace chroma {

struct ToplevelData;

class XdgShellHandler {
public:
    XdgShellHandler(WlrootsServer* server, Canvas* canvas);
    ~XdgShellHandler();

    /// Connect to the XDG shell signals.
    void connect();

    /// Map from our WindowId → wlr_xdg_toplevel for adapter lookups.
    wlr_xdg_toplevel* toplevel_for(WindowId id) const;
    WindowId window_for(wlr_xdg_toplevel* toplevel) const;

    /// Callbacks for wiring into the renderer (set by ChromaApp).
    std::function<void(WindowId, wlr_surface*)> on_window_created;
    std::function<void(WindowId, wlr_surface*)> on_window_mapped;
    std::function<void(WindowId)> on_window_removed;

private:
    friend struct ToplevelData;

    WlrootsServer* server_;
    Canvas* canvas_;

    std::unordered_map<WindowId, wlr_xdg_toplevel*> id_to_toplevel_;
    std::unordered_map<wlr_xdg_toplevel*, WindowId> toplevel_to_id_;
    std::unordered_map<wlr_surface*, ToplevelData*> surface_data_;

    wl_listener on_new_toplevel_;
    wl_listener on_new_popup_;

    static void handle_new_toplevel(wl_listener* listener, void* data);
    static void handle_new_popup(wl_listener* listener, void* data);

    void on_destroy(ToplevelData* td);

    // Direct instance handlers (unused, logic in lambdas)
    void on_map(wlr_xdg_surface* surface);
    void on_unmap(wlr_xdg_surface* surface);
    void on_commit(wlr_xdg_surface* surface);
    void on_request_move(wlr_xdg_toplevel* toplevel);
    void on_request_resize(wlr_xdg_toplevel* toplevel, uint32_t edges);
    void on_set_title(wlr_xdg_toplevel* toplevel);
    void on_set_app_id(wlr_xdg_toplevel* toplevel);
};

} // namespace chroma
