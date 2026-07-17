#include "seat.hpp"
#include <cstdio>
#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon.h>

namespace chroma {

SeatManager::SeatManager(WlrootsServer* server, Canvas* canvas, FocusTracker* focus,
                         InputRouter* input_router, StackManager* stacks,
                         MagnetismEngine* magnetism, XdgShellHandler* xdg_handler,
                         SceneRenderer* renderer)
    : server_{server}, canvas_{canvas}, focus_{focus},
      input_router_{input_router}, stacks_{stacks}, magnetism_{magnetism},
      xdg_handler_{xdg_handler}, renderer_{renderer}
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
    wl_list_init(&on_touch_down_.link);
    wl_list_init(&on_touch_up_.link);
    wl_list_init(&on_touch_motion_.link);
    wl_list_init(&on_touch_cancel_.link);
}

SeatManager::~SeatManager() {
    if (!listeners_connected_) return;

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
    wl_list_remove(&on_touch_down_.link);
    wl_list_remove(&on_touch_up_.link);
    wl_list_remove(&on_touch_motion_.link);
    wl_list_remove(&on_touch_cancel_.link);
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

    listeners_connected_ = true;
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
    // If an exclusive layer surface (wofi, rofi, etc.) currently owns
    // the keyboard, don't steal focus away from it.
    if (on_check_exclusive_focus && on_check_exclusive_focus()) {
        return;
    }

    WindowId focused = focus_->current();
    
    // Skip if focus hasn't changed
    if (focused == prev_focused_) return;
    
    // Deactivate previously focused window
    if (prev_focused_ != INVALID_WINDOW) {
        if (auto* tl = xdg_handler->toplevel_for(prev_focused_)) {
            // wlroots 0.20: set_activated internally calls
            // wlr_xdg_surface_schedule_configure, which asserts initialized.
            if (tl->base->initialized) {
                wlr_xdg_toplevel_set_activated(tl, false);
                // No need for explicit schedule_configure — set_activated does it.
            }
        }
    }
    prev_focused_ = focused;

    // Notify observers (e.g. foreign-toplevel handler)
    if (on_focus_changed) {
        on_focus_changed(focused);
    }

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
    // wlroots 0.20: set_activated internally calls schedule_configure,
    // which asserts initialized. Guard against uninitialized surfaces.
    if (toplevel->base->initialized) {
        wlr_xdg_toplevel_set_activated(toplevel, true);
    }
    
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
    case WLR_INPUT_DEVICE_TOUCH: {
        auto* touch = wlr_touch_from_input_device(device);
        wlr_cursor_attach_input_device(server_->cursor, device);
        on_touch_down_.notify = handle_touch_down;
        wl_signal_add(&touch->events.down, &on_touch_down_);
        on_touch_up_.notify = handle_touch_up;
        wl_signal_add(&touch->events.up, &on_touch_up_);
        on_touch_motion_.notify = handle_touch_motion;
        wl_signal_add(&touch->events.motion, &on_touch_motion_);
        on_touch_cancel_.notify = handle_touch_cancel;
        wl_signal_add(&touch->events.cancel, &on_touch_cancel_);
        touch_ = touch;
        std::printf("[chroma] Touch device attached\n");
        break;
    }
    case WLR_INPUT_DEVICE_TABLET:
    case WLR_INPUT_DEVICE_TABLET_PAD:
    case WLR_INPUT_DEVICE_SWITCH:
        std::printf("[chroma] Input device type not yet supported (ignored)\n");
        break;
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

    // wlroots always provides evdev keycodes regardless of backend (DRM,
    // libinput, or nested). xkbcommon expects keycodes starting at 9
    // (evdev keycode + 8) — this is the standard Linux input mapping and
    // does not vary between backends.
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
                    self->magnetism_->group_nearby(*self->canvas_, f, config::GROUP_RADIUS);
                break;
            }
            case Action::JUMP_NEXT_GROUP:
                self->canvas_->cycle_next_group(screen_size);
                break;
            case Action::JUMP_PREV_GROUP:
                self->canvas_->cycle_prev_group(screen_size);
                break;
            case Action::JUMP_GROUP_1:
            case Action::JUMP_GROUP_2:
            case Action::JUMP_GROUP_3:
            case Action::JUMP_GROUP_4:
            case Action::JUMP_GROUP_5:
            case Action::JUMP_GROUP_6:
            case Action::JUMP_GROUP_7:
            case Action::JUMP_GROUP_8:
            case Action::JUMP_GROUP_9: {
                int idx = static_cast<int>(action)
                        - static_cast<int>(Action::JUMP_GROUP_1) + 1;
                GroupId gid = self->canvas_->group_at_index(idx);
                if (gid != NO_GROUP)
                    self->canvas_->jump_to_group(gid, screen_size);
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
            // State was modified — schedule a frame
            self->server_->schedule_all_frames();
        }
    }

    if (!handled) {
        wlr_seat_set_keyboard(self->server_->seat, self->keyboard_);
        // Forward evdev keycode to the focused client. The Wayland protocol
        // defines keycodes as evdev scancodes, and wlroots always provides
        // evdev keycodes in keyboard events regardless of the input backend
        // (DRM/libinput, libinput, or nested/Wayland). The client's own
        // xkbcommon state handles the evdev→xkb (+8) conversion internally,
        // just as we do above for compositor-side keybinding dispatch.
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
    
    // Sync super-held state to InputRouter for scroll-zoom gating
    self->input_router_->set_super_held(
        (self->modifier_state_ & Mod::SUPER) != 0);
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
    // Raw button events are processed by wlr_cursor (attached via
    // wlr_cursor_attach_input_device) and re-emitted as cursor events.
    // The cursor handler (handle_cursor_button) does compositor logic
    // and forwards to clients.  Don't double-forward here.
    (void)self;
    (void)event;
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
        self->server_->schedule_all_frames();
    }
    else if (self->window_dragging_) {
        Vec2 delta = (self->cursor_pos_ - old_pos) / self->canvas_->zoom;
        if (delta.x != 0.0f || delta.y != 0.0f) {
            self->window_drag_moved_ = true;
            auto* win = self->canvas_->get(self->dragged_window_);
            Vec2 new_pos = win->canvas_pos + delta;

            // Collision: jiggle on overlap but don't block movement.
            // Windows can freely overlap — Super key gates drag-to-stack instead.
            {
                Rect new_rect{new_pos, win->size};
                bool any_overlap = false;
                for (const auto& [other_id, other] : self->canvas_->all_windows()) {
                    if (other_id == self->dragged_window_) continue;
                    if (!other.mapped) continue;
                    if (win->stack != NO_STACK && win->stack == other.stack) continue;
                    if (new_rect.overlaps(other.canvas_rect())) {
                        any_overlap = true;
                        break;
                    }
                }
                if (any_overlap) {
                    self->renderer_->trigger_jiggle(self->dragged_window_,
                        delta * -1.0f);
                }
            }

            self->canvas_->move_window(self->dragged_window_, new_pos);
            self->server_->schedule_all_frames();
        }
    }
    else if (self->resize_dragging_) {
        Vec2 delta = (self->cursor_pos_ - old_pos) / self->canvas_->zoom;
        self->apply_resize(delta);
        self->server_->schedule_all_frames();
    }

    self->input_router_->on_pointer_move(self->cursor_pos_);

    // Perform a scene hit test to find the surface under the cursor,
    // then update the seat's pointer focus so button events are routed.
    double sx = 0, sy = 0;
    wlr_surface* surface_at_cursor = nullptr;
    wlr_scene_node* node = wlr_scene_node_at(
        &self->server_->scene->tree.node, self->cursor_pos_.x, self->cursor_pos_.y, &sx, &sy);
    if (node && node->type == WLR_SCENE_NODE_BUFFER) {
        auto* scene_buffer = wlr_scene_buffer_from_node(node);
        if (scene_buffer) {
            auto* scene_surface = wlr_scene_surface_try_from_buffer(scene_buffer);
            if (scene_surface) {
                surface_at_cursor = scene_surface->surface;
            }
        }
    }

    if (surface_at_cursor) {
        wlr_seat_pointer_notify_enter(self->server_->seat, surface_at_cursor, sx, sy);
    } else {
        wlr_seat_pointer_clear_focus(self->server_->seat);
    }
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
        self->server_->schedule_all_frames();
    }
    else if (self->window_dragging_) {
        Vec2 delta = (self->cursor_pos_ - old_pos) / self->canvas_->zoom;
        if (delta.x != 0.0f || delta.y != 0.0f) {
            self->window_drag_moved_ = true;
            auto* win = self->canvas_->get(self->dragged_window_);
            Vec2 new_pos = win->canvas_pos + delta;

            // Collision: jiggle on overlap but don't block movement.
            // Windows can freely overlap — Super key gates drag-to-stack instead.
            {
                Rect new_rect{new_pos, win->size};
                bool any_overlap = false;
                for (const auto& [other_id, other] : self->canvas_->all_windows()) {
                    if (other_id == self->dragged_window_) continue;
                    if (!other.mapped) continue;
                    if (win->stack != NO_STACK && win->stack == other.stack) continue;
                    if (new_rect.overlaps(other.canvas_rect())) {
                        any_overlap = true;
                        break;
                    }
                }
                if (any_overlap) {
                    self->renderer_->trigger_jiggle(self->dragged_window_,
                        delta * -1.0f * self->canvas_->zoom);
                }
            }

            self->canvas_->move_window(self->dragged_window_, new_pos);
            self->server_->schedule_all_frames();
        }
    }
    else if (self->resize_dragging_) {
        Vec2 delta = (self->cursor_pos_ - old_pos) / self->canvas_->zoom;
        self->apply_resize(delta);
        self->server_->schedule_all_frames();
    }

    self->input_router_->on_pointer_move(self->cursor_pos_);

    // Perform a scene hit test to find the surface under the cursor,
    // then update the seat's pointer focus so button events are routed.
    double sx = 0, sy = 0;
    wlr_surface* surface_at_cursor = nullptr;
    wlr_scene_node* node = wlr_scene_node_at(
        &self->server_->scene->tree.node, self->cursor_pos_.x, self->cursor_pos_.y, &sx, &sy);
    if (node && node->type == WLR_SCENE_NODE_BUFFER) {
        auto* scene_buffer = wlr_scene_buffer_from_node(node);
        if (scene_buffer) {
            auto* scene_surface = wlr_scene_surface_try_from_buffer(scene_buffer);
            if (scene_surface) {
                surface_at_cursor = scene_surface->surface;
            }
        }
    }

    if (surface_at_cursor) {
        wlr_seat_pointer_notify_enter(self->server_->seat, surface_at_cursor, sx, sy);
    } else {
        wlr_seat_pointer_clear_focus(self->server_->seat);
    }
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

    // If an exclusive layer surface (wofi, rofi) is open, skip all
    // window drag/resize/click logic — the click belongs to the overlay.
    bool exclusive_layer_active = self->on_check_exclusive_focus
                               && self->on_check_exclusive_focus();

    if (pressed && exclusive_layer_active) {
        if (self->on_get_exclusive_surface) {
            wlr_surface* surface = self->on_get_exclusive_surface();
            Vec2 surf_pos{0, 0};
            if (self->on_get_exclusive_surface_pos) {
                surf_pos = self->on_get_exclusive_surface_pos();
            }
            if (surface) {
                double sx = self->cursor_pos_.x - surf_pos.x;
                double sy = self->cursor_pos_.y - surf_pos.y;
                wlr_seat_pointer_notify_enter(self->server_->seat, surface, sx, sy);
                wlr_seat_pointer_notify_motion(self->server_->seat,
                    event->time_msec, sx, sy);
            }
        }
    }

    // Regular left press on a window → start potential drag or resize
    if (pressed && is_left && !exclusive_layer_active) {
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
            auto* win = self->canvas_->get(id);

            // --- Handle stacked window drag ---
            // Dragging a stacked window away from its stack → unstack it.
            // Dragging within the stack area → promote to top (active card).
            if (win && win->stack != NO_STACK) {
                auto* stack = self->stacks_->get(win->stack);
                bool still_overlaps = false;
                if (stack) {
                    Rect drag_rect = win->canvas_rect();
                    for (WindowId other_id : stack->windows) {
                        if (other_id == id) continue;
                        auto* other = self->canvas_->get(other_id);
                        if (other && drag_rect.overlaps(other->canvas_rect())) {
                            still_overlaps = true;
                            break;
                        }
                    }
                }

                if (!still_overlaps) {
                    // Dragged away from the stack → unstack
                    self->stacks_->unstack(*self->canvas_, id);
                } else {
                    // Still in the stack area → promote to front (top card)
                    if (stack) {
                        // Move the stack position to where the user placed this card
                        stack->position = win->canvas_pos;
                        // Reorder: remove from current position, insert at front
                        stack->push(id);
                    }
                }
            }

            // --- Super+drag to stack (for non-stacked windows) ---
            if (self->input_router_->super_held() && win && win->stack == NO_STACK) {
                Rect drag_rect = win->canvas_rect();
                for (const auto& [other_id, other] : self->canvas_->all_windows()) {
                    if (other_id == id) continue;
                    if (!other.mapped) continue;
                    if (drag_rect.overlaps(other.canvas_rect())) {
                        // Find or create a stack for the target window
                        StackId target_stack = other.stack;
                        if (target_stack == NO_STACK) {
                            target_stack = self->stacks_->create(other.canvas_pos);
                            self->stacks_->stack(*self->canvas_, other_id, target_stack);
                        }
                        // Stack the dragged window
                        self->stacks_->stack(*self->canvas_, id, target_stack);
                        break;
                    }
                }
            }

            self->magnetism_->apply(*self->canvas_, id);
        } else {
            // No movement → click-to-focus
            self->canvas_->set_focus(id);
            self->focus_->focused(id);
            self->update_keyboard_focus(self->xdg_handler_);
        }
        self->server_->schedule_all_frames();
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
            if (win && tl->base->initialized) {
                int w = std::max(100, (int)win->size.x);
                int h = std::max(100, (int)win->size.y);
                // set_size internally calls schedule_configure (wlroots 0.20+)
                wlr_xdg_toplevel_set_size(tl, w, h);
            }
        }
        self->magnetism_->apply(*self->canvas_, id);
        self->server_->schedule_all_frames();
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
        self->server_->schedule_all_frames();
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

    // Soft collision: jiggle when resizing into another window,
    // but allow the resize to proceed (windows can overlap freely).
    for (const auto& [other_id, other] : canvas_->all_windows()) {
        if (other_id == resized_window_) continue;
        if (!other.mapped) continue;
        if (win->stack != NO_STACK && win->stack == other.stack) continue;
        if (r.overlaps(other.canvas_rect())) {
            // Visual feedback only — don't block the resize
            renderer_->trigger_jiggle(other_id, {0, 0}); // subtle jiggle on the other window
        }
    }

    win->canvas_pos = r.pos;
    win->size = r.size;
}

// ============================================================================
// Touch handlers
// ============================================================================

void SeatManager::handle_touch_down(wl_listener* listener, void* data) {
    SeatManager* self = wl_container_of(listener, self, on_touch_down_);
    auto* event = static_cast<wlr_touch_down_event*>(data);

    // Convert normalized (0..1) coords to screen-space
    Vec2 screen_size = self->active_output_size();
    double screen_x = event->x * screen_size.x;
    double screen_y = event->y * screen_size.y;

    // Notify the seat — surface-local coordinates come from scene hit test
    double sx = 0, sy = 0;
    wlr_surface* surface = nullptr;
    wlr_scene_node* node = wlr_scene_node_at(
        &self->server_->scene->tree.node, screen_x, screen_y, &sx, &sy);
    if (node) {
        auto* scene_buffer = wlr_scene_buffer_from_node(node);
        if (scene_buffer) {
            auto* scene_surface = wlr_scene_surface_try_from_buffer(scene_buffer);
            if (scene_surface) {
                surface = scene_surface->surface;
            }
        }
    }

    wlr_seat_touch_notify_down(self->server_->seat, surface,
        event->time_msec, event->touch_id, sx, sy);
}

void SeatManager::handle_touch_up(wl_listener* listener, void* data) {
    SeatManager* self = wl_container_of(listener, self, on_touch_up_);
    auto* event = static_cast<wlr_touch_up_event*>(data);
    wlr_seat_touch_notify_up(self->server_->seat,
        event->time_msec, event->touch_id);
}

void SeatManager::handle_touch_motion(wl_listener* listener, void* data) {
    SeatManager* self = wl_container_of(listener, self, on_touch_motion_);
    auto* event = static_cast<wlr_touch_motion_event*>(data);

    Vec2 screen_size = self->active_output_size();
    double screen_x = event->x * screen_size.x;
    double screen_y = event->y * screen_size.y;

    double sx = 0, sy = 0;
    wlr_scene_node* node = wlr_scene_node_at(
        &self->server_->scene->tree.node, screen_x, screen_y, &sx, &sy);
    (void)node; // sx,sy already filled by wlr_scene_node_at

    wlr_seat_touch_notify_motion(self->server_->seat,
        event->time_msec, event->touch_id, sx, sy);
}

void SeatManager::handle_touch_cancel(wl_listener* listener, void* data) {
    SeatManager* self = wl_container_of(listener, self, on_touch_cancel_);
    auto* event = static_cast<wlr_touch_cancel_event*>(data);
    (void)event;
    wlr_seat_touch_notify_cancel(self->server_->seat, nullptr);
}

} // namespace chroma
