#include "xdg_handler.hpp"
#include "window.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>

namespace chroma {

// ============================================================================
// Per-toplevel data
// ============================================================================

struct ToplevelData {
    XdgShellHandler* handler;
    WindowId window_id{INVALID_WINDOW};
    wlr_xdg_toplevel* toplevel;
    bool destroyed_{false};  // guard against double-destroy

    // Listeners on wlr_surface (map/unmap/commit/destroy)
    wl_listener surface_map;
    wl_listener surface_unmap;
    wl_listener surface_commit;
    wl_listener surface_destroy;

    // Listeners on wlr_xdg_toplevel
    wl_listener toplevel_destroy;
    wl_listener set_title;
    wl_listener set_app_id;
    wl_listener request_move;
    wl_listener request_resize;
};

// ============================================================================
// XdgShellHandler
// ============================================================================

XdgShellHandler::XdgShellHandler(WlrootsServer* server, Canvas* canvas)
    : server_{server}, canvas_{canvas}
{
    wl_list_init(&on_new_toplevel_.link);
    wl_list_init(&on_new_popup_.link);
    wl_list_init(&on_new_toplevel_decoration_.link);
}

XdgShellHandler::~XdgShellHandler() {
    // Clean up all per-toplevel listeners first, before xdg_shell is destroyed
    // and fires destroy events that would access our now-deleted members.
    for (auto& [surface, td] : surface_data_) {
        wl_list_remove(&td->surface_map.link);
        wl_list_remove(&td->surface_unmap.link);
        wl_list_remove(&td->surface_commit.link);
        wl_list_remove(&td->surface_destroy.link);
        wl_list_remove(&td->toplevel_destroy.link);
        wl_list_remove(&td->set_title.link);
        wl_list_remove(&td->set_app_id.link);
        wl_list_remove(&td->request_move.link);
        wl_list_remove(&td->request_resize.link);
        // unique_ptr will handle deletion when cleared
    }
    surface_data_.clear();
    
    // Remove our listeners from xdg_shell to prevent assertion on shutdown
    wl_list_remove(&on_new_toplevel_.link);
    wl_list_remove(&on_new_popup_.link);

    // Remove decoration listener if connected
    if (server_->xdg_decoration_mgr) {
        wl_list_remove(&on_new_toplevel_decoration_.link);
    }
}

void XdgShellHandler::connect() {
    on_new_toplevel_.notify = handle_new_toplevel;
    wl_signal_add(&server_->xdg_shell->events.new_toplevel, &on_new_toplevel_);

    on_new_popup_.notify = handle_new_popup;
    wl_signal_add(&server_->xdg_shell->events.new_popup, &on_new_popup_);

    if (server_->xdg_decoration_mgr) {
        on_new_toplevel_decoration_.notify = handle_new_toplevel_decoration;
        wl_signal_add(&server_->xdg_decoration_mgr->events.new_toplevel_decoration,
                      &on_new_toplevel_decoration_);
    }
}

wlr_xdg_toplevel* XdgShellHandler::toplevel_for(WindowId id) const {
    auto it = id_to_toplevel_.find(id);
    return it != id_to_toplevel_.end() ? it->second : nullptr;
}

WindowId XdgShellHandler::window_for(wlr_xdg_toplevel* toplevel) const {
    auto it = toplevel_to_id_.find(toplevel);
    return it != toplevel_to_id_.end() ? it->second : INVALID_WINDOW;
}

// ============================================================================
// Static signal handlers
// ============================================================================

void XdgShellHandler::handle_new_toplevel(wl_listener* listener, void* data) {
    XdgShellHandler* handler = wl_container_of(listener, handler, on_new_toplevel_);
    wlr_xdg_toplevel* toplevel = static_cast<wlr_xdg_toplevel*>(data);
    wlr_xdg_surface* xdg_surface = toplevel->base;
    wlr_surface* wlr_surf = xdg_surface->surface;

    auto td = std::make_unique<ToplevelData>();
    ToplevelData* td_raw = td.get();
    td_raw->handler = handler;
    td_raw->toplevel = toplevel;

    ChromaWindow win;
    win.size = {800, 600};
    WindowId wid = handler->canvas_->add(std::move(win));
    td_raw->window_id = wid;

    handler->id_to_toplevel_[wid] = toplevel;
    handler->toplevel_to_id_[toplevel] = wid;
    handler->surface_data_[wlr_surf] = std::move(td);

    // --- Surface events (on wlr_surface) ---

    td_raw->surface_map.notify = [](wl_listener* l, void* /*data*/) {
        ToplevelData* td = wl_container_of(l, td_raw, surface_map);
        auto* win = td->handler->canvas_->get(td->window_id);
        if (win) win->mapped = true;
        // Wire into renderer
        if (td->handler->on_window_mapped) {
            td->handler->on_window_mapped(td->window_id, td->toplevel->base->surface);
        }
        std::printf("[chroma] Window %lu mapped: '%s' (%s) %dx%d\n",
                    td->window_id, win->title.c_str(), win->app_id.c_str(),
                    (int)win->size.x, (int)win->size.y);
    };
    wl_signal_add(&wlr_surf->events.map, &td_raw->surface_map);

    td_raw->surface_unmap.notify = [](wl_listener* l, void* /*data*/) {
        ToplevelData* td = wl_container_of(l, td_raw, surface_unmap);
        auto* win = td->handler->canvas_->get(td->window_id);
        if (win) win->mapped = false;
    };
    wl_signal_add(&wlr_surf->events.unmap, &td_raw->surface_unmap);

    td_raw->surface_commit.notify = [](wl_listener* l, void* /*data*/) {
        ToplevelData* td = wl_container_of(l, td_raw, surface_commit);
        auto* win = td->handler->canvas_->get(td->window_id);
        wlr_xdg_surface* xdg = td->toplevel->base;
        wlr_xdg_toplevel* toplevel = td->toplevel;

        if (win && xdg->initial_commit) {
            // Give the client a reasonable default size
            int width = std::max(100, (int)win->size.x);
            int height = std::max(100, (int)win->size.y);
            wlr_xdg_toplevel_set_size(toplevel, width, height);
            wlr_xdg_toplevel_set_activated(toplevel, true);
            wlr_xdg_surface_schedule_configure(xdg);
        }
        // Note: don't overwrite win->size from client geometry on every commit —
        // the compositor controls the size. The client's geometry reflects the
        // size it's rendering at, which may lag behind our desired size during
        // interactive resize.
    };
    wl_signal_add(&wlr_surf->events.commit, &td_raw->surface_commit);

    td_raw->surface_destroy.notify = [](wl_listener* l, void* /*data*/) {
        ToplevelData* td = wl_container_of(l, td_raw, surface_destroy);
        td->handler->on_destroy(td);
    };
    wl_signal_add(&wlr_surf->events.destroy, &td_raw->surface_destroy);

    // --- Toplevel events ---

    td_raw->toplevel_destroy.notify = [](wl_listener* l, void* /*data*/) {
        ToplevelData* td = wl_container_of(l, td_raw, toplevel_destroy);
        td->handler->on_destroy(td);
    };
    wl_signal_add(&toplevel->events.destroy, &td_raw->toplevel_destroy);

    td_raw->set_title.notify = [](wl_listener* l, void* /*data*/) {
        ToplevelData* td = wl_container_of(l, td_raw, set_title);
        auto* win = td->handler->canvas_->get(td->window_id);
        if (win && td->toplevel->title)
            win->title = td->toplevel->title;
    };
    wl_signal_add(&toplevel->events.set_title, &td_raw->set_title);

    td_raw->set_app_id.notify = [](wl_listener* l, void* /*data*/) {
        ToplevelData* td = wl_container_of(l, td_raw, set_app_id);
        auto* win = td->handler->canvas_->get(td->window_id);
        if (win && td->toplevel->app_id)
            win->app_id = td->toplevel->app_id;
    };
    wl_signal_add(&toplevel->events.set_app_id, &td_raw->set_app_id);

    td_raw->request_move.notify = [](wl_listener* l, void* /*data*/) {
        ToplevelData* td = wl_container_of(l, td_raw, request_move);
        std::printf("[chroma] Client requested move (ignored)\n");
        (void)td;
    };
    wl_signal_add(&toplevel->events.request_move, &td_raw->request_move);

    td_raw->request_resize.notify = [](wl_listener* l, void* data) {
        ToplevelData* td = wl_container_of(l, td_raw, request_resize);
        wlr_xdg_toplevel_resize_event* evt = static_cast<wlr_xdg_toplevel_resize_event*>(data);
        std::printf("[chroma] Client requested resize edges=%u (ignored)\n", evt->edges);
        (void)td;
    };
    wl_signal_add(&toplevel->events.request_resize, &td_raw->request_resize);

    std::printf("[chroma] New toplevel: window %lu\n", wid);

    // Wire into renderer
    if (handler->on_window_created) {
        handler->on_window_created(wid, wlr_surf);
    }
}

void XdgShellHandler::handle_new_popup(wl_listener* listener, void* data) {
    XdgShellHandler* handler = wl_container_of(listener, handler, on_new_popup_);
    wlr_xdg_popup* popup = static_cast<wlr_xdg_popup*>(data);
    (void)handler;
    (void)popup;
    std::printf("[chroma] New popup\n");
}

void XdgShellHandler::handle_new_toplevel_decoration(wl_listener* listener, void* data) {
    XdgShellHandler* handler = wl_container_of(listener, handler, on_new_toplevel_decoration_);
    auto* decor = static_cast<wlr_xdg_toplevel_decoration_v1*>(data);
    (void)handler;

    // Always request client-side decorations (CSD) — we don't draw SSD yet
    wlr_xdg_toplevel_decoration_v1_set_mode(decor,
        WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
}

// ============================================================================
// Cleanup
// ============================================================================

void XdgShellHandler::on_destroy(ToplevelData* td) {
    if (!td) return;
    if (td->destroyed_) return;  // guard against double-destroy
    td->destroyed_ = true;
    WindowId wid = td->window_id;

    // Remove listeners
    wl_list_remove(&td->surface_map.link);
    wl_list_remove(&td->surface_unmap.link);
    wl_list_remove(&td->surface_commit.link);
    wl_list_remove(&td->surface_destroy.link);
    wl_list_remove(&td->toplevel_destroy.link);
    wl_list_remove(&td->set_title.link);
    wl_list_remove(&td->set_app_id.link);
    wl_list_remove(&td->request_move.link);
    wl_list_remove(&td->request_resize.link);

    // Remove from maps
    wlr_surface* wlr_surf = td->toplevel->base->surface;
    surface_data_.erase(wlr_surf);
    id_to_toplevel_.erase(wid);
    toplevel_to_id_.erase(td->toplevel);

    // Remove from domain
    canvas_->remove(wid);

    // unique_ptr in surface_data_ handles deletion after erase
    std::printf("[chroma] Window %lu destroyed\n", wid);

    // Wire into renderer
    if (on_window_removed) {
        on_window_removed(wid);
    }
}

void XdgShellHandler::on_map(wlr_xdg_surface*)   {}
void XdgShellHandler::on_unmap(wlr_xdg_surface*) {}
void XdgShellHandler::on_commit(wlr_xdg_surface*) {}
void XdgShellHandler::on_request_move(wlr_xdg_toplevel*) {}
void XdgShellHandler::on_request_resize(wlr_xdg_toplevel*, uint32_t) {}
void XdgShellHandler::on_set_title(wlr_xdg_toplevel*) {}
void XdgShellHandler::on_set_app_id(wlr_xdg_toplevel*) {}

} // namespace chroma
