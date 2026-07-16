#include "foreign_toplevel.hpp"
#include <cstdio>

namespace chroma {

// ============================================================================
// ForeignToplevelHandler
// ============================================================================

ForeignToplevelHandler::ForeignToplevelHandler(WlrootsServer* server)
    : server_{server}
{
    mgr_ = server_->foreign_toplevel_mgr;
}

ForeignToplevelHandler::~ForeignToplevelHandler() {
    // Destroy all handles (this also removes our listeners via handle_destroy)
    for (auto& [id, handle] : handles_) {
        wlr_foreign_toplevel_handle_v1_destroy(handle);
    }
    handles_.clear();
    // data_ entries are removed by handle_destroy callbacks
    data_.clear();
}

wlr_foreign_toplevel_manager_v1*
ForeignToplevelHandler::manager() const {
    return mgr_;
}

// ============================================================================
// Window lifecycle
// ============================================================================

void ForeignToplevelHandler::on_window_created(WindowId id,
                                                const std::string& title,
                                                const std::string& app_id) {
    if (!mgr_) return;

    auto* handle = wlr_foreign_toplevel_handle_v1_create(mgr_);
    if (!handle) {
        std::fprintf(stderr, "[chroma] Failed to create foreign-toplevel handle for window %lu\n",
                     id);
        return;
    }

    handles_[id] = handle;

    // Set initial properties
    if (!title.empty())
        wlr_foreign_toplevel_handle_v1_set_title(handle, title.c_str());
    if (!app_id.empty())
        wlr_foreign_toplevel_handle_v1_set_app_id(handle, app_id.c_str());

    // Per-handle tracking data
    auto td = std::make_unique<ForeignToplevelData>();
    td->handler = this;
    td->window_id = id;
    td->handle = handle;

    ForeignToplevelData* td_raw = td.get();
    data_[handle] = std::move(td);

    // --- Wire request listeners ---

    td_raw->request_activate.notify = handle_request_activate;
    wl_signal_add(&handle->events.request_activate, &td_raw->request_activate);

    td_raw->request_maximize.notify = handle_request_maximize;
    wl_signal_add(&handle->events.request_maximize, &td_raw->request_maximize);

    td_raw->request_minimize.notify = handle_request_minimize;
    wl_signal_add(&handle->events.request_minimize, &td_raw->request_minimize);

    td_raw->request_fullscreen.notify = handle_request_fullscreen;
    wl_signal_add(&handle->events.request_fullscreen, &td_raw->request_fullscreen);

    td_raw->request_close.notify = handle_request_close;
    wl_signal_add(&handle->events.request_close, &td_raw->request_close);

    td_raw->handle_destroy.notify = handle_destroy;
    wl_signal_add(&handle->events.destroy, &td_raw->handle_destroy);
}

void ForeignToplevelHandler::on_window_destroyed(WindowId id) {
    auto it = handles_.find(id);
    if (it == handles_.end()) return;

    wlr_foreign_toplevel_handle_v1_destroy(it->second);
    // handle_destroy callback erases from handles_ and data_
}

void ForeignToplevelHandler::on_title_changed(WindowId id,
                                               const std::string& title) {
    auto it = handles_.find(id);
    if (it == handles_.end()) return;

    wlr_foreign_toplevel_handle_v1_set_title(it->second, title.c_str());
}

void ForeignToplevelHandler::on_app_id_changed(WindowId id,
                                                const std::string& app_id) {
    auto it = handles_.find(id);
    if (it == handles_.end()) return;

    wlr_foreign_toplevel_handle_v1_set_app_id(it->second, app_id.c_str());
}

void ForeignToplevelHandler::on_focus_changed(WindowId id) {
    // Clear activation on the previously active window
    if (active_ != INVALID_WINDOW) {
        auto old = handles_.find(active_);
        if (old != handles_.end()) {
            wlr_foreign_toplevel_handle_v1_set_activated(old->second, false);
        }
    }

    active_ = id;

    // Set activation on the new window
    if (id != INVALID_WINDOW) {
        auto it = handles_.find(id);
        if (it != handles_.end()) {
            wlr_foreign_toplevel_handle_v1_set_activated(it->second, true);
        }
    }
}

void ForeignToplevelHandler::on_output_enter(WindowId id, wlr_output* output) {
    if (!output) return;

    auto it = handles_.find(id);
    if (it == handles_.end()) return;

    wlr_foreign_toplevel_handle_v1_output_enter(it->second, output);
}

// ============================================================================
// Static signal handlers
// ============================================================================

void ForeignToplevelHandler::handle_request_activate(wl_listener* listener,
                                                      void* /*data*/) {
    ForeignToplevelData* td = wl_container_of(listener, td, request_activate);
    std::printf("[chroma] Foreign-toplevel: request_activate for window %lu\n",
                td->window_id);

    if (td->handler->on_client_activate_request) {
        td->handler->on_client_activate_request(td->window_id);
    }
}

void ForeignToplevelHandler::handle_request_maximize(wl_listener* listener,
                                                      void* /*data*/) {
    ForeignToplevelData* td = wl_container_of(listener, td, request_maximize);
    std::printf("[chroma] Foreign-toplevel: request_maximize for window %lu (ignored)\n",
                td->window_id);
    // Maximize not supported in canvas WM — acknowledge but ignore
    wlr_foreign_toplevel_handle_v1_set_maximized(td->handle, false);
}

void ForeignToplevelHandler::handle_request_minimize(wl_listener* listener,
                                                      void* /*data*/) {
    ForeignToplevelData* td = wl_container_of(listener, td, request_minimize);
    std::printf("[chroma] Foreign-toplevel: request_minimize for window %lu (ignored)\n",
                td->window_id);
    wlr_foreign_toplevel_handle_v1_set_minimized(td->handle, false);
}

void ForeignToplevelHandler::handle_request_fullscreen(wl_listener* listener,
                                                        void* /*data*/) {
    ForeignToplevelData* td = wl_container_of(listener, td, request_fullscreen);
    std::printf("[chroma] Foreign-toplevel: request_fullscreen for window %lu (ignored)\n",
                td->window_id);
    wlr_foreign_toplevel_handle_v1_set_fullscreen(td->handle, false);
}

void ForeignToplevelHandler::handle_request_close(wl_listener* listener,
                                                   void* /*data*/) {
    ForeignToplevelData* td = wl_container_of(listener, td, request_close);
    std::printf("[chroma] Foreign-toplevel: request_close for window %lu (ignored)\n",
                td->window_id);
    // We could send an XDG close event here in the future.
    // For now, closing via the compositor's own input is sufficient.
}

void ForeignToplevelHandler::handle_destroy(wl_listener* listener,
                                             void* /*data*/) {
    ForeignToplevelData* td = wl_container_of(listener, td, handle_destroy);

    // Remove listeners so they don't fire again
    wl_list_remove(&td->request_activate.link);
    wl_list_remove(&td->request_maximize.link);
    wl_list_remove(&td->request_minimize.link);
    wl_list_remove(&td->request_fullscreen.link);
    wl_list_remove(&td->request_close.link);
    wl_list_remove(&td->handle_destroy.link);

    // Erase from maps
    td->handler->handles_.erase(td->window_id);
    td->handler->data_.erase(td->handle);

    if (td->handler->active_ == td->window_id) {
        td->handler->active_ = INVALID_WINDOW;
    }
}

} // namespace chroma
