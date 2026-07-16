#include "input.hpp"
#include <linux/input-event-codes.h>

namespace chroma {

Action InputRouter::on_key(Canvas& canvas, FocusTracker& focus, StackManager& stacks,
                           uint32_t keysym, uint32_t modifiers, Vec2 screen_size)
{
    bool super = (modifiers & Mod::SUPER) != 0;
    bool shift = (modifiers & Mod::SHIFT) != 0;

    // Normalize letter keysyms to lowercase for matching
    uint32_t ksym_lower = keysym;
    if (keysym >= 0x0041 && keysym <= 0x005A)  // A-Z → a-z
        ksym_lower = keysym + 0x20;
    else if (keysym >= 0x00C0 && keysym <= 0x00D6)  // À-Ö
        ksym_lower = keysym + 0x20;
    else if (keysym >= 0x00D8 && keysym <= 0x00DE)  // Ø-Þ
        ksym_lower = keysym + 0x20;

    // --- Quit ---
    if (super && shift && ksym_lower == Key::E) {
        return Action::QUIT;
    }

    // --- Focus cycling ---
    if (super && keysym == Key::TAB) {
        return shift ? focus.cycle_prev(canvas) != INVALID_WINDOW
                         ? Action::FOCUS_PREV : Action::NONE
                     : focus.cycle_next(canvas) != INVALID_WINDOW
                         ? Action::FOCUS_NEXT : Action::NONE;
    }

    // --- Pan ---
    float pan_dist = config::PAN_STEP / canvas.zoom; // compensate for zoom
    if (super && keysym == Key::LEFT)  { canvas.pan({-pan_dist, 0}); return Action::PAN_LEFT; }
    if (super && keysym == Key::RIGHT) { canvas.pan({ pan_dist, 0}); return Action::PAN_RIGHT; }
    if (super && keysym == Key::UP)    { canvas.pan({0, -pan_dist}); return Action::PAN_UP; }
    if (super && keysym == Key::DOWN)  { canvas.pan({0,  pan_dist}); return Action::PAN_DOWN; }

    // --- Zoom ---
    if (super && (keysym == Key::PLUS || ksym_lower == Key::EQUAL)) {
        canvas.zoom_at(1.0f + config::ZOOM_STEP, screen_size * 0.5f, screen_size);
        return Action::ZOOM_IN;
    }
    if (super && keysym == Key::MINUS) {
        canvas.zoom_at(1.0f - config::ZOOM_STEP, screen_size * 0.5f, screen_size);
        return Action::ZOOM_OUT;
    }

    // --- Stack operations ---
    if (super && ksym_lower == Key::S && !shift) {
        WindowId focused = focus.current();
        if (focused != INVALID_WINDOW) {
            auto* w = canvas.get(focused);
            if (w && w->stack != NO_STACK) {
                // Cycle the stack this window belongs to
                stacks.cycle(canvas, w->stack);
                return Action::STACK_CYCLE;
            }
        }
        return Action::NONE;
    }

    // Stack current window with others of same app_id
    if (super && shift && ksym_lower == Key::S) {
        WindowId focused = focus.current();
        if (focused != INVALID_WINDOW) {
            auto* w = canvas.get(focused);
            if (w && w->stack == NO_STACK) {
                // Find or create a stack for this app_id
                StackId target_stack = NO_STACK;
                for (const auto& [sid, stack] : stacks.all_stacks()) {
                    if (!stack.empty()) {
                        auto* top = canvas.get(stack.top());
                        if (top && top->app_id == w->app_id) {
                            target_stack = sid;
                            break;
                        }
                    }
                }
                if (target_stack == NO_STACK) {
                    target_stack = stacks.create(w->canvas_pos);
                }
                stacks.stack(canvas, focused, target_stack);
                return Action::STACK_WINDOW;
            }
        }
        return Action::NONE;
    }

    // Unstack
    if (super && shift && ksym_lower == Key::X) {
        WindowId focused = focus.current();
        if (focused != INVALID_WINDOW) {
            stacks.unstack(canvas, focused);
            return Action::UNSTACK_WINDOW;
        }
        return Action::NONE;
    }

    // --- Group nearby windows ---
    if (super && ksym_lower == Key::G) {
        WindowId focused = focus.current();
        if (focused != INVALID_WINDOW) {
            // This action is handled by the MagnetismEngine — we return it for the
            // app layer to call magnetism.group_nearby()
            return Action::GROUP_HERE;
        }
        return Action::NONE;
    }

    // --- Group navigation (jump between groups) ---
    if (super && keysym == Key::LEFT_BRACKET) {
        return Action::JUMP_PREV_GROUP;
    }
    if (super && keysym == Key::RIGHT_BRACKET) {
        return Action::JUMP_NEXT_GROUP;
    }
    // Super+1..9 → direct jump to group by index
    if (super && !shift && keysym >= Key::NUM_1 && keysym <= Key::NUM_9) {
        int group_idx = static_cast<int>(keysym - Key::NUM_1); // 0-based
        switch (group_idx) {
            case 0: return Action::JUMP_GROUP_1;
            case 1: return Action::JUMP_GROUP_2;
            case 2: return Action::JUMP_GROUP_3;
            case 3: return Action::JUMP_GROUP_4;
            case 4: return Action::JUMP_GROUP_5;
            case 5: return Action::JUMP_GROUP_6;
            case 6: return Action::JUMP_GROUP_7;
            case 7: return Action::JUMP_GROUP_8;
            case 8: return Action::JUMP_GROUP_9;
            default: break;
        }
    }

    return Action::NONE;
}

Action InputRouter::on_pointer_button(Canvas& canvas, FocusTracker& focus,
                                      Vec2 screen_pos, Vec2 screen_size,
                                      uint32_t button, bool pressed)
{
    pointer_screen_ = screen_pos;

    if (!pressed) return Action::NONE;

    // Left click — focus window under cursor
    if (button == BTN_LEFT) {
        Vec2 canvas_pos = canvas.screen_to_canvas(screen_pos, screen_size);
        WindowId id = canvas.window_at(canvas_pos);
        if (id != INVALID_WINDOW) {
            canvas.set_focus(id);
            focus.focused(id);
        }
    }

    return Action::NONE;
}

void InputRouter::on_pointer_move(Vec2 screen_pos) {
    pointer_screen_ = screen_pos;
}

} // namespace chroma
