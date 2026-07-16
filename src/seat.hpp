#pragma once

#include <functional>

#include "server.hpp"
#include "canvas.hpp"
#include "focus.hpp"
#include "input.hpp"
#include "stack.hpp"
#include "magnetism.hpp"
#include "xdg_handler.hpp"
#include "renderer.hpp"

namespace chroma {

// ============================================================================
// SeatManager — handles keyboard, pointer, and cursor input
// ============================================================================

/// Translates wlroots input events into domain actions via InputRouter.
class SeatManager {
public:
    SeatManager(WlrootsServer* server, Canvas* canvas, FocusTracker* focus,
                InputRouter* input_router, StackManager* stacks,
                MagnetismEngine* magnetism, XdgShellHandler* xdg_handler,
                SceneRenderer* renderer);
    ~SeatManager();

    /// Connect to input device signals. Call after server init.
    void connect();

    /// Returns the current cursor position in screen coordinates.
    Vec2 cursor_position() const { return cursor_pos_; }

    /// Return the output size for the currently focused output (or first output).
    Vec2 active_output_size() const;

    /// Update keyboard focus to match canvas focus state.
    void update_keyboard_focus(XdgShellHandler* xdg_handler);

    /// Callback invoked when keyboard focus changes (for foreign-toplevel, etc.).
    std::function<void(WindowId)> on_focus_changed;
    
    /// Detect which resize edge(s) the cursor is near on a window rect.
    static uint32_t detect_resize_edge(const Rect& win_rect, Vec2 cursor_canvas, int margin);
    
    /// Apply a resize delta to the currently resized window.
    void apply_resize(Vec2 delta_canvas);

private:
    WlrootsServer* server_;
    Canvas* canvas_;
    FocusTracker* focus_;
    InputRouter* input_router_;
    StackManager* stacks_;
    MagnetismEngine* magnetism_;
    XdgShellHandler* xdg_handler_{nullptr};
    SceneRenderer* renderer_{nullptr};

    Vec2 cursor_pos_{0, 0};
    WindowId prev_focused_{INVALID_WINDOW};  // tracks last keyboard focus target
    
    // Viewport panning (Alt+drag)
    bool viewport_dragging_{false};
    
    // Window move drag
    bool window_dragging_{false};
    WindowId dragged_window_{INVALID_WINDOW};
    Vec2 window_drag_offset_{0, 0};
    bool window_drag_moved_{false};
    
    // Window resize drag
    bool resize_dragging_{false};
    WindowId resized_window_{INVALID_WINDOW};
    uint32_t resize_edges_{0};  // wlr_edges bitmask
    Rect resize_initial_rect_;   // window rect at drag start
    
    static constexpr int RESIZE_BORDER = 8;  // px from edge to trigger resize

    // Keyboard state
    wlr_keyboard* keyboard_{nullptr};
    uint32_t modifier_state_{0};

    // Touch state
    wlr_touch* touch_{nullptr};

    bool listeners_connected_{false};  // guards destructor cleanup

    // Listeners
    wl_listener on_new_input_;
    wl_listener on_keyboard_key_;
    wl_listener on_keyboard_modifiers_;
    wl_listener on_keyboard_destroy_;
    wl_listener on_pointer_motion_;
    wl_listener on_pointer_motion_absolute_;
    wl_listener on_pointer_button_;
    wl_listener on_pointer_axis_;
    wl_listener on_pointer_destroy_;
    wl_listener on_cursor_motion_;
    wl_listener on_cursor_motion_absolute_;
    wl_listener on_cursor_button_;
    wl_listener on_cursor_axis_;
    wl_listener on_cursor_frame_;

    // Touch listeners
    wl_listener on_touch_down_;
    wl_listener on_touch_up_;
    wl_listener on_touch_motion_;
    wl_listener on_touch_cancel_;

    static void handle_new_input(wl_listener* listener, void* data);
    void attach_input_device(wlr_input_device* device);

    // Keyboard handlers
    static void handle_keyboard_key(wl_listener* listener, void* data);
    static void handle_keyboard_modifiers(wl_listener* listener, void* data);
    static void handle_keyboard_destroy(wl_listener* listener, void* data);

    // Pointer handlers
    static void handle_pointer_motion(wl_listener* listener, void* data);
    static void handle_pointer_motion_absolute(wl_listener* listener, void* data);
    static void handle_pointer_button(wl_listener* listener, void* data);
    static void handle_pointer_axis(wl_listener* listener, void* data);
    static void handle_pointer_destroy(wl_listener* listener, void* data);

    // Cursor handlers (after wlr_cursor processes the event)
    static void handle_cursor_motion(wl_listener* listener, void* data);
    static void handle_cursor_motion_absolute(wl_listener* listener, void* data);
    static void handle_cursor_button(wl_listener* listener, void* data);
    static void handle_cursor_axis(wl_listener* listener, void* data);
    static void handle_cursor_frame(wl_listener* listener, void* data);

    // Touch handlers
    static void handle_touch_down(wl_listener* listener, void* data);
    static void handle_touch_up(wl_listener* listener, void* data);
    static void handle_touch_motion(wl_listener* listener, void* data);
    static void handle_touch_cancel(wl_listener* listener, void* data);

    void process_key(uint32_t keysym, bool pressed);
    void process_button(uint32_t button, bool pressed);
    void update_cursor_position();
};

} // namespace chroma
