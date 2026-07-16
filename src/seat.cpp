#include "seat.hpp"
#include <cstdio>
#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon.h>

namespace chroma {

SeatManager::SeatManager(WlrootsServer* server, Canvas* canvas, FocusTracker* focus,
                         InputRouter* input_router, StackManager* stacks,
                         MagnetismEngine* magnetism, XdgShellHandler* xdg_handler)
    : server_{server}, canvas_{canvas}, focus_{focus},
      input_router_{input_router}, stacks_{stacks}, magnetism_{magnetism},
      xdg_handler_{xdg_handler}
{
    wl_list_init(&on_new_input_.link);
    wl_list_init(&on_keyboard_key_.link);
    wl_list_init(&on_keyboard_modifiers_.link);
    wl_list_init(&on_keyboard_destroy_.link);
    wl_list_init(&on_pointer_motion_.link);
    wl_list_init(&on_pointer_motion_absolute_.link);
    wl_list_init(&on_pointer_button_.link);
    wl_list_init(&on_pointer_axis_.link);
    wl_list_init(&on_pointer_destroy_.link);
    wl_list_init(&on_cursor_motion_.link);
    wl_list_init(&on_cursor_motion_absolute_.link);
    wl_list_init(&on_cursor_button_.link);
    wl_list_init(&on_cursor_axis_.link);
    wl_list_init(&on_cursor_frame_.link);
}

SeatManager::~SeatManager() {
    wl_list_remove(&on_new_input_.link);
    wl_list_remove(&on_keyboard_key_.link);
    wl_list_remove(&on_keyboard_modifiers_.link);
    wl_list_remove(&on_keyboard_destroy_.link);
    wl_list_remove(&on_pointer_motion_.link);
    wl_list_remove(&on_pointer_motion_absolute_.link);
    wl_list_remove(&on_pointer_button_.link);
    wl_list_remove(&on_pointer_axis_.link);
    wl_list_remove(&on_pointer_destroy_.link);
    wl_list_remove(&on_cursor_motion_.link);
    wl_list_remove(&on_cursor_motion_absolute_.link);
    wl_list_remove(&on_cursor_button_.link);
    wl_list_remove(&on_cursor_axis_.link);
    wl_list_remove(&on_cursor_frame_.link);
}

void SeatManager::connect() {
    on_new_input_.notify = handle_new_input;
    wl_signal_add(&server_->backend->events.new_input, &on_new_input_);

    on_cursor_motion_.notify = handle_cursor_motion;
    wl_signal_add(&server_->cursor->events.motion, &on_cursor_motion_);
    on_cursor_motion_absolute_.notify = handle_cursor_motion_absolute;
    wl_signal_add(&server_->cursor->events.motion_absolute, &on_cursor_motion_absolute_);
    on_cursor_button_.notify = handle_cursor_button;
    wl_signal_add(&server_->cursor->events.button, &on_cursor_button_);
    on_cursor_axis_.notify = handle_cursor_axis;
    wl_signal_add(&server_->cursor->events.axis, &on_cursor_axis_);
    on_cursor_frame_.notify = handle_cursor_frame;
    wl_signal_add(&server_->cursor->events.frame, &on_cursor_frame_);

    wlr_xcursor_manager_load(server_->xcursor_mgr, 1.0f);
    wlr_cursor_set_xcursor(server_->cursor, server_->xcursor_mgr, "left_ptr");
}

Vec2 SeatManager::active_output_size() const {
    wlr_output* output = wlr_output_layout_output_at(server_->output_layout,
        cursor_pos_.x, cursor_pos_.y);
    if (!output && !server_->scene_outputs.empty()) {
        auto* layout_output = wlr_output_layout_get(server_->output_layout, 0);
        if (layout_output) output = layout_output->output;
    }
    if (output) {
        return {static_cast<float>(output->width), static_cast<float>(output->height)};
    }
    return {1920, 1080};
}

void SeatManager::update_keyboard_focus(XdgShellHandler* xdg_handler) {
    WindowId focused = focus_->current();
    
    // Skip if focus hasn't changed
    static WindowId prev_focused = INVALID_WINDOW;
    if (focused == prev_focused) return;
    
    // Deactivate previously focused window
    if (prev_focused != INVALID_WINDOW) {
        if (auto* tl = xdg_handler->toplevel_for(prev_focused)) {
            wlr_xdg_toplevel_set_activated(tl, false);
            wlr_xdg_surface_schedule_configure(tl->base);
        }
    }
    prev_focused = focused;
    
    if (focused == INVALID_WINDOW) {
        std::fprintf(stderr, "[chroma] KB focus: clear\n");
        wlr_seat_keyboard_notify_clear_focus(server_->seat);
        return;
    }
    
    wlr_xdg_toplevel* toplevel = xdg_handler->toplevel_for(focused);
    if (!toplevel) {
        std::fprintf(stderr, "[chroma] KB focus: window %lu has no toplevel\n", focused);
        wlr_seat_keyboard_notify_clear_focus(server_->seat);
        return;
    }
    
    // Activate the new focused window
    wlr_xdg_toplevel_set_activated(toplevel, true);
    wlr_xdg_surface_schedule_configure(toplevel->base);
    
    wlr_surface* surface = toplevel->base->surface;
    if (!surface) return;
    
    wlr_keyboard_modifiers mods{};
    if (keyboard_) {
        mods = keyboard_->modifiers;
    }
    wlr_seat_keyboard_notify_enter(server_->seat, surface, nullptr, 0, &mods);
}

// ============================================================================
// New input device
// ============================================================================

void SeatManager::handle_new_input(wl_listener* listener, void* data) {
    SeatManager* self = wl_container_of(listener, self, on_new_input_);
    auto* device = static_cast<wlr_input_device*>(data);
    self->attach_input_device(device);
}

void SeatManager::attach_input_device(wlr_input_device* device) {
    switch (device->type) {
    case WLR_INPUT_DEVICE_KEYBOARD: {
        auto* kb = wlr_keyboard_from_input_device(device);
        if (!kb->keymap) {
            xkb_context* context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
            if (!context) break;
            xkb_keymap* keymap = xkb_keymap_new_from_names(context, nullptr,
                XKB_KEYMAP_COMPILE_NO_FLAGS);
            if (!keymap) { xkb_context_unref(context); break; }
            wlr_keyboard_set_keymap(kb, keymap);
            xkb_keymap_unref(keymap);
            xkb_context_unref(context);
        }
        wlr_keyboard_set_repeat_info(kb, 25, 600);
        on_keyboard_key_.notify = handle_keyboard_key;
        wl_signal_add(&kb->events.key, &on_keyboard_key_);
        on_keyboard_modifiers_.notify = handle_keyboard_modifiers;
        wl_signal_add(&kb->events.modifiers, &on_keyboard_modifiers_);
        on_keyboard_destroy_.notify = handle_keyboard_destroy;
        wl_signal_add(&device->events.destroy, &on_keyboard_destroy_);
        wlr_seat_set_keyboard(server_->seat, kb);
        keyboard_ = kb;
        break;
    }
    case WLR_INPUT_DEVICE_POINTER: {
        auto* pointer = wlr_pointer_from_input_device(device);
        on_pointer_motion_.notify = handle_pointer_motion;
        wl_signal_add(&pointer->events.motion, &on_pointer_motion_);
        on_pointer_motion_absolute_.notify = handle_pointer_motion_absolute;
        wl_signal_add(&pointer->events.motion_absolute, &on_pointer_motion_absolute_);
        on_pointer_button_.notify = handle_pointer_button;
        wl_signal_add(&pointer->events.button, &on_pointer_button_);
        on_pointer_axis_.notify = handle_pointer_axis;
        wl_signal_add(&pointer->events.axis, &on_pointer_axis_);
        on_pointer_destroy_.notify = handle_pointer_destroy;
        wl_signal_add(&device->events.destroy, &on_pointer_destroy_);
        wlr_cursor_attach_input_device(server_->cursor, device);
        break;
    }
    default: break;
    }
}

// ============================================================================
// Keyboard handlers
// ============================================================================

void SeatManager::handle_keyboard_key(wl_listener* listener, void* data) {
    SeatManager* self = wl_container_of(listener, self, on_keyboard_key_);
    wlr_keyboard_key_event* event = static_cast<wlr_keyboard_key_event*>(data);
    if (!self->keyboard_ || !self->keyboard_->xkb_state) return;

    xkb_keysym_t keysym = xkb_state_key_get_one_sym(
        self->keyboard_->xkb_state, event->keycode + 8);
    bool pressed = event->state == WL_KEYBOARD_KEY_STATE_PRESSED;

    bool handled = false;
    if (pressed) {
        Vec2 screen_size = self->active_output_size();
        Action action = self->input_router_->on_key(*self->canvas_, *self->focus_,
            *self->stacks_, static_cast<uint32_t>(keysym), self->modifier_state_,
            screen_size);

        if (action != Action::NONE) {
            handled = true;
            switch (action) {
            case Action::QUIT:
                self->server_->terminate();
                return;
            case Action::GROUP_HERE: {
                WindowId f = self->focus_->current();
                if (f != INVALID_WINDOW)
                    self->magnetism_->group_nearby(*self->canvas_, f, 300.0f);
                break;
            }
            case Action::FOCUS_NEXT:
            case Action::FOCUS_PREV: {
                WindowId f = self->focus_->current();
                if (f != INVALID_WINDOW) {
                    if (auto* win = self->canvas_->get(f))
                        self->canvas_->viewport_center = win->center();
                    self->update_keyboard_focus(self->xdg_handler_);
                }
                break;
            }
            default: break;
            }
        }
    }

    if (!handled) {
        wlr_seat_set_keyboard(self->server_->seat, self->keyboard_);
        // Forward raw keycode — backend provides evdev; Wayland protocol
        // expects evdev+8 but the nested Wayland backend may get raw evdev
        // from the host. Pass as-is and let the client's xkb handle it.
        wlr_seat_keyboard_notify_key(self->server_->seat, event->time_msec,
            event->keycode, event->state);
    }
}

void SeatManager::handle_keyboard_modifiers(wl_listener* listener, void* data) {
    SeatManager* self = wl_container_of(listener, self, on_keyboard_modifiers_);
    auto* kb = static_cast<wlr_keyboard*>(data);
    wlr_seat_set_keyboard(self->server_->seat, kb);
    wlr_seat_keyboard_notify_modifiers(self->server_->seat, &kb->modifiers);

    self->modifier_state_ = 0;
    if (xkb_state_mod_name_is_active(kb->xkb_state, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_DEPRESSED))
        self->modifier_state_ |= Mod::SHIFT;
    if (xkb_state_mod_name_is_active(kb->xkb_state, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_DEPRESSED))
        self->modifier_state_ |= Mod::CTRL;
    if (xkb_state_mod_name_is_active(kb->xkb_state, XKB_MOD_NAME_ALT, XKB_STATE_MODS_DEPRESSED))
        self->modifier_state_ |= Mod::ALT;
    if (xkb_state_mod_name_is_active(kb->xkb_state, XKB_MOD_NAME_LOGO, XKB_STATE_MODS_DEPRESSED))
        self->modifier_state_ |= Mod::SUPER;
}

void SeatManager::handle_keyboard_destroy(wl_listener* listener, void*) {
    SeatManager* self = wl_container_of(listener, self, on_keyboard_destroy_);
    self->keyboard_ = nullptr;
}

// ============================================================================
// Pointer handlers (raw device events)
// ============================================================================

void SeatManager::handle_pointer_motion(wl_listener* listener, void* data) {
    SeatManager* self = wl_container_of(listener, self, on_pointer_motion_);
    auto* event = static_cast<wlr_pointer_motion_event*>(data);
    wlr_cursor_move(self->server_->cursor, &event->pointer->base, event->delta_x, event->delta_y);
}

void SeatManager::handle_pointer_motion_absolute(wl_listener* listener, void* data) {
    SeatManager* self = wl_container_of(listener, self, on_pointer_motion_absolute_);
    auto* event = static_cast<wlr_pointer_motion_absolute_event*>(data);
    wlr_cursor_warp_absolute(self->server_->cursor, &event->pointer->base, event->x, event->y);
}

void SeatManager::handle_pointer_button(wl_listener* listener, void* data) {
    SeatManager* self = wl_container_of(listener, self, on_pointer_button_);
    auto* event = static_cast<wlr_pointer_button_event*>(data);
    wlr_seat_pointer_notify_button(self->server_->seat,
        event->time_msec, event->button, event->state);
}

void SeatManager::handle_pointer_axis(wl_listener* listener, void* data) {
    SeatManager* self = wl_container_of(listener, self, on_pointer_axis_);
    auto* event = static_cast<wlr_pointer_axis_event*>(data);
    wlr_seat_pointer_notify_axis(self->server_->seat,
        event->time_msec, event->orientation, event->delta, event->delta_discrete,
        event->source, event->relative_direction);
}

void SeatManager::handle_pointer_destroy(wl_listener* /*listener*/, void*) {
    // pointer removed, cursor continues
}

// ============================================================================
// Cursor handlers (post-wlr_cursor processing)
// ============================================================================

void SeatManager::handle_cursor_motion(wl_listener* listener, void* data) {
    SeatManager* self = wl_container_of(listener, self, on_cursor_motion_);
    auto* event = static_cast<wlr_pointer_motion_event*>(data);

    Vec2 old_pos = self->cursor_pos_;
    self->update_cursor_position();

    if (self->viewport_dragging_) {
        Vec2 delta = self->cursor_pos_ - old_pos;
        self->canvas_->pan(-delta / self->canvas_->zoom);
    }
    else if (self->window_dragging_) {
        Vec2 delta = (self->cursor_pos_ - old_pos) / self->canvas_->zoom;
        if (delta.x != 0.0f || delta.y != 0.0f) {
            self->window_drag_moved_ = true;
            self->canvas_->move_window(self->dragged_window_,
                self->canvas_->get(self->dragged_window_)->canvas_pos + delta);
        }
    }
    else if (self->resize_dragging_) {
        Vec2 delta = (self->cursor_pos_ - old_pos) / self->canvas_->zoom;
        self->apply_resize(delta);
    }

    self->input_router_->on_pointer_move(self->cursor_pos_);
    wlr_seat_pointer_notify_motion(self->server_->seat,
        event->time_msec, self->cursor_pos_.x, self->cursor_pos_.y);
}

void SeatManager::handle_cursor_motion_absolute(wl_listener* listener, void* data) {
    SeatManager* self = wl_container_of(listener, self, on_cursor_motion_absolute_);
    auto* event = static_cast<wlr_pointer_motion_absolute_event*>(data);

    Vec2 old_pos = self->cursor_pos_;
    self->update_cursor_position();

    if (self->viewport_dragging_) {
        Vec2 delta = self->cursor_pos_ - old_pos;
        self->canvas_->pan(-delta / self->canvas_->zoom);
    }
    else if (self->window_dragging_) {
        Vec2 delta = (self->cursor_pos_ - old_pos) / self->canvas_->zoom;
        if (delta.x != 0.0f || delta.y != 0.0f) {
            self->window_drag_moved_ = true;
            self->canvas_->move_window(self->dragged_window_,
                self->canvas_->get(self->dragged_window_)->canvas_pos + delta);
        }
    }
    else if (self->resize_dragging_) {
        Vec2 delta = (self->cursor_pos_ - old_pos) / self->canvas_->zoom;
        self->apply_resize(delta);
    }

    self->input_router_->on_pointer_move(self->cursor_pos_);
    wlr_seat_pointer_notify_motion(self->server_->seat,
        event->time_msec, self->cursor_pos_.x, self->cursor_pos_.y);
}

void SeatManager::handle_cursor_button(wl_listener* listener, void* data) {
    SeatManager* self = wl_container_of(listener, self, on_cursor_button_);
    auto* event = static_cast<wlr_pointer_button_event*>(data);

    bool pressed = event->state == WL_POINTER_BUTTON_STATE_PRESSED;
    bool is_left = event->button == BTN_LEFT;
    bool alt_held = (self->modifier_state_ & Mod::ALT) != 0;

    // Alt+Left drag → pan viewport
    if (is_left && alt_held) {
        self->viewport_dragging_ = pressed;
        return;
    }

    // Regular left press on a window → start potential drag or resize
    if (pressed && is_left) {
        Vec2 screen_size = self->active_output_size();
        Vec2 canvas_pos = self->canvas_->screen_to_canvas(self->cursor_pos_, screen_size);
        WindowId id = self->canvas_->window_at(canvas_pos);
        if (id != INVALID_WINDOW) {
            auto* win = self->canvas_->get(id);
            if (!win) return;
            
            // Check if cursor is near a window edge → resize mode
            uint32_t edges = detect_resize_edge(win->canvas_rect(), canvas_pos, RESIZE_BORDER);
            if (edges != WLR_EDGE_NONE) {
                self->resize_dragging_ = true;
                self->resized_window_ = id;
                self->resize_edges_ = edges;
                self->resize_initial_rect_ = win->canvas_rect();
                return;
            }
            
            // Otherwise → move mode
            self->window_dragging_ = true;
            self->dragged_window_ = id;
            self->window_drag_moved_ = false;
            self->window_drag_offset_ = canvas_pos - win->canvas_pos;
            return;
        }
    }

    // Left release after potential drag
    if (!pressed && is_left && self->window_dragging_) {
        self->window_dragging_ = false;
        WindowId id = self->dragged_window_;
        self->dragged_window_ = INVALID_WINDOW;

        if (self->window_drag_moved_) {
            // Actual drag → apply magnetism
            self->magnetism_->apply(*self->canvas_, id);
        } else {
            // No movement → click-to-focus
            self->canvas_->set_focus(id);
            self->focus_->focused(id);
            self->update_keyboard_focus(self->xdg_handler_);
        }
        return;
    }

    // Left release after resize drag
    if (!pressed && is_left && self->resize_dragging_) {
        self->resize_dragging_ = false;
        WindowId id = self->resized_window_;
        self->resized_window_ = INVALID_WINDOW;
        // Notify client of the final size
        if (auto* tl = self->xdg_handler_->toplevel_for(id)) {
            auto* win = self->canvas_->get(id);
            if (win) {
                int w = std::max(100, (int)win->size.x);
                int h = std::max(100, (int)win->size.y);
                wlr_xdg_toplevel_set_size(tl, w, h);
                wlr_xdg_surface_schedule_configure(tl->base);
            }
        }
        self->magnetism_->apply(*self->canvas_, id);
        return;
    }

    // Forward to client
    wlr_seat_pointer_notify_button(self->server_->seat,
        event->time_msec, event->button, event->state);
}

void SeatManager::handle_cursor_axis(wl_listener* listener, void* data) {
    SeatManager* self = wl_container_of(listener, self, on_cursor_axis_);
    auto* event = static_cast<wlr_pointer_axis_event*>(data);

    if (self->input_router_->super_held()) {
        Vec2 screen_size = self->active_output_size();
        float factor = 1.0f;
        if (event->orientation == WL_POINTER_AXIS_VERTICAL_SCROLL) {
            factor = event->delta > 0
                ? 1.0f - config::ZOOM_STEP : 1.0f + config::ZOOM_STEP;
        }
        self->canvas_->zoom_at(factor, self->cursor_pos_, screen_size);
        return;
    }

    wlr_seat_pointer_notify_axis(self->server_->seat,
        event->time_msec, event->orientation, event->delta, event->delta_discrete,
        event->source, event->relative_direction);
}

void SeatManager::handle_cursor_frame(wl_listener* listener, void*) {
    SeatManager* self = wl_container_of(listener, self, on_cursor_frame_);
    wlr_seat_pointer_notify_frame(self->server_->seat);
}

void SeatManager::update_cursor_position() {
    cursor_pos_ = {
        static_cast<float>(server_->cursor->x),
        static_cast<float>(server_->cursor->y)
    };
}

uint32_t SeatManager::detect_resize_edge(const Rect& win_rect, Vec2 cursor_canvas, int margin) {
    uint32_t edges = 0;
    if (cursor_canvas.x < win_rect.left() + margin)
        edges |= WLR_EDGE_LEFT;
    else if (cursor_canvas.x > win_rect.right() - margin)
        edges |= WLR_EDGE_RIGHT;
    if (cursor_canvas.y < win_rect.top() + margin)
        edges |= WLR_EDGE_TOP;
    else if (cursor_canvas.y > win_rect.bottom() - margin)
        edges |= WLR_EDGE_BOTTOM;
    return edges;
}

void SeatManager::apply_resize(Vec2 delta) {
    auto* win = canvas_->get(resized_window_);
    if (!win) return;

    // Apply incremental delta to the current window rect
    Rect r = win->canvas_rect();
    uint32_t edges = resize_edges_;

    if (edges & WLR_EDGE_LEFT) {
        r.pos.x += delta.x;
        r.size.x -= delta.x;
    }
    if (edges & WLR_EDGE_RIGHT) {
        r.size.x += delta.x;
    }
    if (edges & WLR_EDGE_TOP) {
        r.pos.y += delta.y;
        r.size.y -= delta.y;
    }
    if (edges & WLR_EDGE_BOTTOM) {
        r.size.y += delta.y;
    }

    if (r.size.x < 100) { r.size.x = 100; if (edges & WLR_EDGE_LEFT) r.pos.x = win->canvas_rect().right() - 100; }
    if (r.size.y < 100) { r.size.y = 100; if (edges & WLR_EDGE_TOP) r.pos.y = win->canvas_rect().bottom() - 100; }

    win->canvas_pos = r.pos;
    win->size = r.size;
}

} // namespace chroma
