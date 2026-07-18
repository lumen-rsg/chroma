#pragma once

/// @file input.hpp
/// @brief InputRouter — translates raw keysyms and modifier masks into
/// high-level domain Actions. Also defines the Action enum and key/modifier
/// constant namespaces used throughout the compositor.
///
/// The router is pure domain logic: it takes keysym + modifiers → Action.
/// The adapter layer (SeatManager) feeds it events from wlroots/libinput
/// and executes the returned actions.
///
/// Keybindings are config-driven via ChromaConfig. The config's [[bind]]
/// entries are parsed into a lookup map by rebuild_bindmap(). If the config
/// has no binds, a built-in default set is used (matching the previous
/// hardcoded behavior).

#include "types.hpp"
#include "config.hpp"
#include "canvas.hpp"
#include "focus.hpp"
#include "stack.hpp"
#include <functional>
#include <unordered_map>
#include <string_view>

namespace chroma {

// ============================================================================
// Key & Modifier constants
// ============================================================================

/// Modifier constants are defined in types.hpp (namespace Mod).
/// The constants below are XKB keysym values used by the compositor.

/// XKB keysym values we use.
namespace Key {
    constexpr uint32_t ESC       = 0xFF1B;
    constexpr uint32_t TAB       = 0xFF09;
    constexpr uint32_t RETURN    = 0xFF0D;
    constexpr uint32_t SPACE     = 0x0020;
    constexpr uint32_t BACKSPACE = 0xFF08;
    constexpr uint32_t DELETE    = 0xFFFF;

    constexpr uint32_t LEFT    = 0xFF51;
    constexpr uint32_t RIGHT   = 0xFF53;
    constexpr uint32_t UP      = 0xFF52;
    constexpr uint32_t DOWN    = 0xFF54;

    constexpr uint32_t Q = 0x0071;
    constexpr uint32_t E = 0x0065;
    constexpr uint32_t R = 0x0072;
    constexpr uint32_t F = 0x0066;
    constexpr uint32_t G = 0x0067;
    constexpr uint32_t S = 0x0073;
    constexpr uint32_t Z = 0x007a;
    constexpr uint32_t X = 0x0078;

    constexpr uint32_t PLUS  = 0x002B; // +
    constexpr uint32_t MINUS = 0x002D; // -
    constexpr uint32_t EQUAL = 0x003D; // =

    constexpr uint32_t NUM_1 = 0x0031;  // 1–9
    constexpr uint32_t NUM_9 = 0x0039;

    constexpr uint32_t LEFT_BRACKET  = 0x005B; // [
    constexpr uint32_t RIGHT_BRACKET = 0x005D; // ]
}

// ============================================================================
// Action enum
// ============================================================================

/// Action the input router can emit — used for testing / extensibility.
enum class Action {
    NONE,            ///< No action taken (key forwarded to client)
    PAN_LEFT,        ///< Pan viewport left
    PAN_RIGHT,       ///< Pan viewport right
    PAN_UP,          ///< Pan viewport up
    PAN_DOWN,        ///< Pan viewport down
    ZOOM_IN,         ///< Zoom viewport in (toward cursor)
    ZOOM_OUT,        ///< Zoom viewport out (away from cursor)
    FOCUS_NEXT,      ///< Cycle focus to next window in MRU order
    FOCUS_PREV,      ///< Cycle focus to previous window in MRU order
    STACK_CYCLE,     ///< Cycle the focused window's card stack
    STACK_WINDOW,    ///< Add focused window to a stack
    UNSTACK_WINDOW,  ///< Remove focused window from its stack
    GROUP_HERE,      ///< Create a magnetic group from nearby windows
    JUMP_NEXT_GROUP, ///< Jump viewport to the next group (by creation order)
    JUMP_PREV_GROUP, ///< Jump viewport to the previous group
    JUMP_GROUP_1,    ///< Jump directly to group 1
    JUMP_GROUP_2,    ///< Jump directly to group 2
    JUMP_GROUP_3,    ///< Jump directly to group 3
    JUMP_GROUP_4,    ///< Jump directly to group 4
    JUMP_GROUP_5,    ///< Jump directly to group 5
    JUMP_GROUP_6,    ///< Jump directly to group 6
    JUMP_GROUP_7,    ///< Jump directly to group 7
    JUMP_GROUP_8,    ///< Jump directly to group 8
    JUMP_GROUP_9,    ///< Jump directly to group 9
    CLOSE_WINDOW,    ///< Close the focused window
    QUIT,            ///< Terminate the compositor

    // Config-driven actions
    SPAWN,           ///< Spawn a program (arg passed via callback)
    EXEC,            ///< Replace compositor with a program (callback)
    RELOAD_CONFIG,   ///< Reload config from disk
};

/// Parse an action name string to an Action enum value.
/// Returns Action::NONE if the string doesn't match any known action.
Action parse_action_string(std::string_view name);

/// Convert an Action to its string name (for debug/logging).
std::string_view action_to_string(Action action);

// ============================================================================
// InputRouter — translates raw input into domain actions
// ============================================================================

class InputRouter {
public:
    InputRouter() = default;

    /// Set the config to use for keybind resolution.
    /// Call rebuild_bindmap() after setting a new config.
    void set_config(const ChromaConfig* config) { config_ = config; }

    /// Rebuild the internal keybind lookup map from the current config.
    /// If the config has no binds, populate with built-in defaults.
    void rebuild_bindmap();

    /// Process a key event. Returns the action taken (or NONE).
    /// Spawn/exec/reload actions are dispatched via callbacks and
    /// return Action::NONE (the caller sees no action to handle).
    Action on_key(Canvas& canvas, FocusTracker& focus, StackManager& stacks,
                  uint32_t keysym, uint32_t modifiers, Vec2 screen_size);

    /// Process a pointer button press/release at screen coordinates.
    Action on_pointer_button(Canvas& canvas, FocusTracker& focus,
                             Vec2 screen_pos, Vec2 screen_size,
                             uint32_t button, bool pressed);

    /// Process pointer motion.
    void on_pointer_move(Vec2 screen_pos);

    /// Whether the Super key is held (for drag operations).
    bool super_held() const { return super_held_; }

    /// Set the Super-key-held state (called from SeatManager modifier handler).
    void set_super_held(bool held) { super_held_ = held; }

    // --- Callbacks for config-driven actions ---

    /// Called when a "spawn" action fires. The arg string is the program to spawn.
    std::function<void(const std::string& arg)> on_spawn;

    /// Called when an "exec" action fires. The arg string is the program.
    std::function<void(const std::string& arg)> on_exec;

    /// Called when a "reload_config" action fires.
    std::function<void()> on_reload_config;

private:
    Vec2 pointer_screen_{0, 0};
    bool super_held_{false};

    const ChromaConfig* config_{nullptr};

    /// Resolved binding — action + optional arg, ready to execute.
    struct ResolvedBind {
        Action action{Action::NONE};
        std::string arg;
    };

    /// Lookup key = (modifiers << 32) | keysym → resolved action.
    std::unordered_map<uint64_t, ResolvedBind> bindmap_;

    /// Default keybinds (used when config has no binds).
    std::vector<Keybinding> default_binds_;

    /// Normalize a keysym to lowercase for matching (A-Z → a-z, etc.).
    static uint32_t keysym_lower(uint32_t keysym);
};

} // namespace chroma
