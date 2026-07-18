#include "input.hpp"
#include "keys.hpp"
#include <cstdio>
#include <linux/input-event-codes.h>

namespace chroma {

// ============================================================================
// Action string parsing
// ============================================================================

Action parse_action_string(std::string_view name) {
    // Sorted by frequency of use for a slight lookup speedup
    if (name == "quit")              return Action::QUIT;
    if (name == "spawn")             return Action::SPAWN;
    if (name == "exec")              return Action::EXEC;
    if (name == "reload_config")     return Action::RELOAD_CONFIG;
    if (name == "focus_next")        return Action::FOCUS_NEXT;
    if (name == "focus_prev")        return Action::FOCUS_PREV;
    if (name == "pan_left")          return Action::PAN_LEFT;
    if (name == "pan_right")         return Action::PAN_RIGHT;
    if (name == "pan_up")            return Action::PAN_UP;
    if (name == "pan_down")          return Action::PAN_DOWN;
    if (name == "zoom_in")           return Action::ZOOM_IN;
    if (name == "zoom_out")          return Action::ZOOM_OUT;
    if (name == "stack_cycle")       return Action::STACK_CYCLE;
    if (name == "stack_window")      return Action::STACK_WINDOW;
    if (name == "unstack_window")    return Action::UNSTACK_WINDOW;
    if (name == "group_here")        return Action::GROUP_HERE;
    if (name == "jump_next_group")   return Action::JUMP_NEXT_GROUP;
    if (name == "jump_prev_group")   return Action::JUMP_PREV_GROUP;
    if (name == "jump_group_1")      return Action::JUMP_GROUP_1;
    if (name == "jump_group_2")      return Action::JUMP_GROUP_2;
    if (name == "jump_group_3")      return Action::JUMP_GROUP_3;
    if (name == "jump_group_4")      return Action::JUMP_GROUP_4;
    if (name == "jump_group_5")      return Action::JUMP_GROUP_5;
    if (name == "jump_group_6")      return Action::JUMP_GROUP_6;
    if (name == "jump_group_7")      return Action::JUMP_GROUP_7;
    if (name == "jump_group_8")      return Action::JUMP_GROUP_8;
    if (name == "jump_group_9")      return Action::JUMP_GROUP_9;
    if (name == "close_window")      return Action::CLOSE_WINDOW;
    return Action::NONE;
}

std::string_view action_to_string(Action action) {
    switch (action) {
    case Action::QUIT:            return "quit";
    case Action::SPAWN:           return "spawn";
    case Action::EXEC:            return "exec";
    case Action::RELOAD_CONFIG:   return "reload_config";
    case Action::FOCUS_NEXT:      return "focus_next";
    case Action::FOCUS_PREV:      return "focus_prev";
    case Action::PAN_LEFT:        return "pan_left";
    case Action::PAN_RIGHT:       return "pan_right";
    case Action::PAN_UP:          return "pan_up";
    case Action::PAN_DOWN:        return "pan_down";
    case Action::ZOOM_IN:         return "zoom_in";
    case Action::ZOOM_OUT:        return "zoom_out";
    case Action::STACK_CYCLE:     return "stack_cycle";
    case Action::STACK_WINDOW:    return "stack_window";
    case Action::UNSTACK_WINDOW:  return "unstack_window";
    case Action::GROUP_HERE:      return "group_here";
    case Action::JUMP_NEXT_GROUP: return "jump_next_group";
    case Action::JUMP_PREV_GROUP: return "jump_prev_group";
    case Action::JUMP_GROUP_1:    return "jump_group_1";
    case Action::JUMP_GROUP_2:    return "jump_group_2";
    case Action::JUMP_GROUP_3:    return "jump_group_3";
    case Action::JUMP_GROUP_4:    return "jump_group_4";
    case Action::JUMP_GROUP_5:    return "jump_group_5";
    case Action::JUMP_GROUP_6:    return "jump_group_6";
    case Action::JUMP_GROUP_7:    return "jump_group_7";
    case Action::JUMP_GROUP_8:    return "jump_group_8";
    case Action::JUMP_GROUP_9:    return "jump_group_9";
    case Action::CLOSE_WINDOW:    return "close_window";
    case Action::NONE:
    default:                      return "none";
    }
}

// ============================================================================
// Default keybinds (used when config has no [[bind]] entries)
// ============================================================================

static std::vector<Keybinding> make_default_binds() {
    return {
        {"Super+Shift+E",    "quit",           "", 0, 0},
        {"Super+Tab",        "focus_next",     "", 0, 0},
        {"Super+Shift+Tab",  "focus_prev",     "", 0, 0},
        {"Super+Left",       "pan_left",       "", 0, 0},
        {"Super+Right",      "pan_right",      "", 0, 0},
        {"Super+Up",         "pan_up",         "", 0, 0},
        {"Super+Down",       "pan_down",       "", 0, 0},
        {"Super+Equal",      "zoom_in",        "", 0, 0},
        {"Super+Minus",      "zoom_out",       "", 0, 0},
        {"Super+S",          "stack_cycle",    "", 0, 0},
        {"Super+Shift+S",    "stack_window",   "", 0, 0},
        {"Super+Shift+X",    "unstack_window", "", 0, 0},
        {"Super+G",          "group_here",     "", 0, 0},
        {"Super+LeftBracket","jump_prev_group","", 0, 0},
        {"Super+RightBracket","jump_next_group","", 0, 0},
        {"Super+1",          "jump_group_1",   "", 0, 0},
        {"Super+2",          "jump_group_2",   "", 0, 0},
        {"Super+3",          "jump_group_3",   "", 0, 0},
        {"Super+4",          "jump_group_4",   "", 0, 0},
        {"Super+5",          "jump_group_5",   "", 0, 0},
        {"Super+6",          "jump_group_6",   "", 0, 0},
        {"Super+7",          "jump_group_7",   "", 0, 0},
        {"Super+8",          "jump_group_8",   "", 0, 0},
        {"Super+9",          "jump_group_9",   "", 0, 0},
        {"Super+Q",          "close_window",   "", 0, 0},
    };
}

// ============================================================================
// InputRouter — bindmap management
// ============================================================================

uint32_t InputRouter::keysym_lower(uint32_t keysym) {
    // A-Z → a-z
    if (keysym >= 0x0041 && keysym <= 0x005A) return keysym + 0x20;
    // À-Ö → à-ö
    if (keysym >= 0x00C0 && keysym <= 0x00D6) return keysym + 0x20;
    // Ø-Þ → ø-þ
    if (keysym >= 0x00D8 && keysym <= 0x00DE) return keysym + 0x20;
    return keysym;
}

void InputRouter::rebuild_bindmap() {
    bindmap_.clear();
    default_binds_.clear();

    const std::vector<Keybinding>* source = nullptr;

    if (config_ && !config_->binds.empty()) {
        source = &config_->binds;
    } else {
        // Use built-in defaults
        default_binds_ = make_default_binds();
        source = &default_binds_;
    }

    for (const auto& kb : *source) {
        // Parse the key string
        auto parsed = parse_keybind_string(kb.keys);
        if (!parsed) {
            std::fprintf(stderr, "[chroma] Warning: Unknown key in bind '%s' — skipping\n",
                         kb.keys.c_str());
            continue;
        }

        // Parse the action
        Action act = parse_action_string(kb.action);
        if (act == Action::NONE) {
            std::fprintf(stderr, "[chroma] Warning: Unknown action '%s' for key '%s' — skipping\n",
                         kb.action.c_str(), kb.keys.c_str());
            continue;
        }

        // Build lookup key: (modifiers << 32) | keysym_lower
        uint64_t lookup_key = (static_cast<uint64_t>(parsed->modifiers) << 32)
                            | static_cast<uint64_t>(keysym_lower(parsed->keysym));

        // Later binds with the same key chord override earlier ones
        bindmap_[lookup_key] = ResolvedBind{act, kb.arg};
    }
}

// ============================================================================
// InputRouter — key processing
// ============================================================================

Action InputRouter::on_key(Canvas& canvas, FocusTracker& focus, StackManager& stacks,
                           uint32_t keysym, uint32_t modifiers, Vec2 screen_size)
{
    // Remove Caps Lock from modifiers — it shouldn't affect keybinds.
    // Also clear Mod::MOD2 (Num Lock) and Mod::MOD3 (Scroll Lock).
    uint32_t clean_mods = modifiers & ~(Mod::CAPS | Mod::MOD2 | Mod::MOD3);

    // Build lookup key
    uint64_t lookup_key = (static_cast<uint64_t>(clean_mods) << 32)
                        | static_cast<uint64_t>(keysym_lower(keysym));

    // Look up in bindmap
    auto it = bindmap_.find(lookup_key);
    if (it == bindmap_.end()) {
        return Action::NONE;
    }

    const ResolvedBind& resolved = it->second;
    Action action = resolved.action;

    // Handle special actions that need callbacks or complex logic
    switch (action) {
    case Action::SPAWN:
        if (on_spawn) on_spawn(resolved.arg);
        return Action::NONE; // handled by callback

    case Action::EXEC:
        if (on_exec) on_exec(resolved.arg);
        return Action::NONE; // handled by callback

    case Action::RELOAD_CONFIG:
        if (on_reload_config) on_reload_config();
        return Action::NONE; // handled by callback

    case Action::QUIT:
        return Action::QUIT;

    case Action::PAN_LEFT:
        canvas.pan({-config::PAN_STEP / canvas.zoom, 0});
        return Action::PAN_LEFT;

    case Action::PAN_RIGHT:
        canvas.pan({config::PAN_STEP / canvas.zoom, 0});
        return Action::PAN_RIGHT;

    case Action::PAN_UP:
        canvas.pan({0, -config::PAN_STEP / canvas.zoom});
        return Action::PAN_UP;

    case Action::PAN_DOWN:
        canvas.pan({0, config::PAN_STEP / canvas.zoom});
        return Action::PAN_DOWN;

    case Action::ZOOM_IN:
        canvas.zoom_at(1.0f + config::ZOOM_STEP, screen_size * 0.5f, screen_size);
        return Action::ZOOM_IN;

    case Action::ZOOM_OUT:
        canvas.zoom_at(1.0f - config::ZOOM_STEP, screen_size * 0.5f, screen_size);
        return Action::ZOOM_OUT;

    case Action::FOCUS_NEXT: {
        bool shift = (clean_mods & Mod::SHIFT) != 0;
        // The config bind determines which direction based on modifiers.
        if (shift) {
            if (focus.cycle_prev(canvas) != INVALID_WINDOW)
                return Action::FOCUS_PREV;
        } else {
            if (focus.cycle_next(canvas) != INVALID_WINDOW)
                return Action::FOCUS_NEXT;
        }
        return Action::NONE;
    }
    case Action::FOCUS_PREV:
        if (focus.cycle_prev(canvas) != INVALID_WINDOW)
            return Action::FOCUS_PREV;
        return Action::NONE;

    case Action::STACK_CYCLE: {
        WindowId focused = focus.current();
        if (focused != INVALID_WINDOW) {
            auto* w = canvas.get(focused);
            if (w && w->stack != NO_STACK) {
                stacks.cycle(canvas, w->stack);
                return Action::STACK_CYCLE;
            }
        }
        return Action::NONE;
    }

    case Action::STACK_WINDOW: {
        WindowId focused = focus.current();
        if (focused != INVALID_WINDOW) {
            auto* w = canvas.get(focused);
            if (w && w->stack == NO_STACK) {
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

    case Action::UNSTACK_WINDOW: {
        WindowId focused = focus.current();
        if (focused != INVALID_WINDOW) {
            stacks.unstack(canvas, focused);
            return Action::UNSTACK_WINDOW;
        }
        return Action::NONE;
    }

    case Action::GROUP_HERE:
        // This action is handled by the MagnetismEngine — we return it for the
        // app layer to call magnetism.group_nearby()
        return Action::GROUP_HERE;

    case Action::JUMP_NEXT_GROUP:
        return Action::JUMP_NEXT_GROUP;

    case Action::JUMP_PREV_GROUP:
        return Action::JUMP_PREV_GROUP;

    case Action::JUMP_GROUP_1:
    case Action::JUMP_GROUP_2:
    case Action::JUMP_GROUP_3:
    case Action::JUMP_GROUP_4:
    case Action::JUMP_GROUP_5:
    case Action::JUMP_GROUP_6:
    case Action::JUMP_GROUP_7:
    case Action::JUMP_GROUP_8:
    case Action::JUMP_GROUP_9:
        return action;

    case Action::CLOSE_WINDOW:
        return Action::CLOSE_WINDOW; // handled by app layer (SeatManager)

    case Action::NONE:
    default:
        return Action::NONE;
    }
}

// ============================================================================
// InputRouter — pointer processing (unchanged)
// ============================================================================

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
