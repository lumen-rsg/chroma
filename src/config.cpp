/// @file config.cpp
/// @brief TOML-based configuration loading with recursive import resolution.

#include "config.hpp"
#include "keys.hpp"

#include "toml.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <set>
#include <cstring>

namespace chroma {

// ============================================================================
// Helpers
// ============================================================================

/// Resolve a path relative to a base directory.
/// If `path` is absolute, return it as-is.
static std::string resolve_path(const std::string& base_dir, const std::string& path) {
    if (path.empty()) return path;
    if (path[0] == '/' || path[0] == '~') {
        // Expand ~ to $HOME
        if (path[0] == '~') {
            const char* home = std::getenv("HOME");
            if (home) {
                return std::string(home) + path.substr(1);
            }
        }
        return path;
    }
    // Relative: join with base_dir
    if (base_dir.empty() || base_dir == ".") return path;
    return base_dir + "/" + path;
}

/// Get the directory containing a file path.
static std::string dir_of(const std::string& path) {
    auto pos = path.rfind('/');
    if (pos == std::string::npos) return ".";
    return path.substr(0, pos);
}

/// Get the canonical absolute path of a file (for cycle detection).
static std::string canonical_path(const std::string& path) {
    std::error_code ec;
    auto p = std::filesystem::canonical(path, ec);
    if (ec) return path; // fallback to original
    return p.string();
}

/// Parse a TOML array of floats into a C array of `N` floats.
/// Missing or excess elements are silently handled.
template<size_t N>
static void parse_float_array(const toml::node* node, float (&out)[N], const float (&defaults)[N]) {
    if (!node || !node->is_array()) {
        for (size_t i = 0; i < N; i++) out[i] = defaults[i];
        return;
    }
    const auto& arr = *node->as_array();
    for (size_t i = 0; i < N; i++) {
        if (i < arr.size()) {
            if (auto val = arr[i].value<float>()) {
                out[i] = *val;
            } else if (auto val = arr[i].value<double>()) {
                out[i] = static_cast<float>(*val);
            } else if (auto val = arr[i].value<int>()) {
                out[i] = static_cast<float>(*val);
            } else {
                out[i] = defaults[i];
            }
        } else {
            out[i] = defaults[i];
        }
    }
}

/// Parse a single SpawnRule from a TOML table node.
static SpawnRule parse_spawn_rule(const toml::table& tbl) {
    SpawnRule rule;
    if (auto val = tbl["program"].value<std::string>()) {
        rule.program = *val;
    }
    if (auto arr = tbl["args"].as_array()) {
        for (const auto& elem : *arr) {
            if (auto s = elem.value<std::string>()) {
                rule.args.push_back(*s);
            }
        }
    }
    if (auto val = tbl["workdir"].value<std::string>()) {
        rule.workdir = *val;
    }
    return rule;
}

/// Parse a single Keybinding from a TOML table node.
static Keybinding parse_keybind(const toml::table& tbl) {
    Keybinding kb;
    if (auto val = tbl["keys"].value<std::string>()) {
        kb.keys = *val;
    }
    if (auto val = tbl["action"].value<std::string>()) {
        kb.action = *val;
    }
    if (auto val = tbl["arg"].value<std::string>()) {
        kb.arg = *val;
    }
    return kb;
}

// ============================================================================
// ChromaConfig methods
// ============================================================================

void ChromaConfig::reset() {
    *this = ChromaConfig{};
}

void ChromaConfig::merge_from(const ChromaConfig& other) {
    // Vectors: prepend other's entries (main file entries stay at end)
    pre_init.insert(pre_init.begin(), other.pre_init.begin(), other.pre_init.end());
    post_init.insert(post_init.begin(), other.post_init.begin(), other.post_init.end());
    binds.insert(binds.begin(), other.binds.begin(), other.binds.end());

    // Theme: use other's values only if ours are still at default
    // We detect "default" by comparing against a fresh ChromaConfig's defaults.
    static const ChromaConfig defaults;

    if (std::memcmp(theme.background, defaults.theme.background, sizeof(theme.background)) == 0)
        std::memcpy(theme.background, other.theme.background, sizeof(theme.background));

    if (std::memcmp(theme.border_focused, defaults.theme.border_focused, sizeof(theme.border_focused)) == 0)
        std::memcpy(theme.border_focused, other.theme.border_focused, sizeof(theme.border_focused));

    if (std::memcmp(theme.border_unfocused, defaults.theme.border_unfocused, sizeof(theme.border_unfocused)) == 0)
        std::memcpy(theme.border_unfocused, other.theme.border_unfocused, sizeof(theme.border_unfocused));

    if (theme.border_width == defaults.theme.border_width)
        theme.border_width = other.theme.border_width;

    if (theme.shadow_enabled == defaults.theme.shadow_enabled)
        theme.shadow_enabled = other.theme.shadow_enabled;

    if (theme.shadow_layers == defaults.theme.shadow_layers)
        theme.shadow_layers = other.theme.shadow_layers;

    if (theme.shadow_offset_x == defaults.theme.shadow_offset_x)
        theme.shadow_offset_x = other.theme.shadow_offset_x;

    if (theme.shadow_offset_y == defaults.theme.shadow_offset_y)
        theme.shadow_offset_y = other.theme.shadow_offset_y;

    if (theme.shadow_grow == defaults.theme.shadow_grow)
        theme.shadow_grow = other.theme.shadow_grow;

    if (std::memcmp(theme.shadow_alphas, defaults.theme.shadow_alphas, sizeof(theme.shadow_alphas)) == 0)
        std::memcpy(theme.shadow_alphas, other.theme.shadow_alphas, sizeof(theme.shadow_alphas));

    if (std::memcmp(theme.group_hues, defaults.theme.group_hues, sizeof(theme.group_hues)) == 0)
        std::memcpy(theme.group_hues, other.theme.group_hues, sizeof(theme.group_hues));

    // Animations
    if (animations.move_duration == defaults.animations.move_duration)
        animations.move_duration = other.animations.move_duration;
    if (animations.resize_duration == defaults.animations.resize_duration)
        animations.resize_duration = other.animations.resize_duration;
    if (animations.open_duration == defaults.animations.open_duration)
        animations.open_duration = other.animations.open_duration;
    if (animations.close_duration == defaults.animations.close_duration)
        animations.close_duration = other.animations.close_duration;
    if (animations.viewport_duration == defaults.animations.viewport_duration)
        animations.viewport_duration = other.animations.viewport_duration;
    if (animations.snap_threshold == defaults.animations.snap_threshold)
        animations.snap_threshold = other.animations.snap_threshold;

    // Input
    if (input.pan_step == defaults.input.pan_step)
        input.pan_step = other.input.pan_step;
    if (input.zoom_step == defaults.input.zoom_step)
        input.zoom_step = other.input.zoom_step;
    if (input.snap_distance == defaults.input.snap_distance)
        input.snap_distance = other.input.snap_distance;
    if (input.group_radius == defaults.input.group_radius)
        input.group_radius = other.input.group_radius;
    if (input.min_zoom == defaults.input.min_zoom)
        input.min_zoom = other.input.min_zoom;
    if (input.max_zoom == defaults.input.max_zoom)
        input.max_zoom = other.input.max_zoom;
}

// ============================================================================
// TOML → ChromaConfig parser
// ============================================================================

/// Parse a single file into a ChromaConfig (without resolving imports).
/// Returns the parsed config. `warnings` and `errors` are appended to.
static ChromaConfig parse_one_file(const std::string& path,
                                    std::vector<std::string>& warnings,
                                    std::vector<std::string>& errors) {
    ChromaConfig cfg;

    toml::table tbl;
    try {
        tbl = toml::parse_file(path);
    } catch (const toml::parse_error& e) {
        errors.push_back(std::string("Parse error in ") + path + ": " + e.what());
        return cfg;
    } catch (const std::exception& e) {
        errors.push_back(std::string("Error reading ") + path + ": " + e.what());
        return cfg;
    }

    // --- imports ---
    if (auto* imports_node = tbl.get("imports")) {
        if (auto* imports_arr = imports_node->as_array()) {
            for (const auto& elem : *imports_arr) {
                if (auto s = elem.value<std::string>()) {
                    cfg.imports.push_back(*s);
                }
            }
        }
    }

    // --- [[pre_init]] ---
    if (auto* pre = tbl.get_as<toml::array>("pre_init")) {
        for (const auto& elem : *pre) {
            if (auto* t = elem.as_table()) {
                auto rule = parse_spawn_rule(*t);
                if (!rule.program.empty()) {
                    cfg.pre_init.push_back(std::move(rule));
                } else {
                    warnings.push_back("[[pre_init]] entry without 'program' in " + path + " — skipped");
                }
            }
        }
    }

    // --- [[post_init]] ---
    if (auto* post = tbl.get_as<toml::array>("post_init")) {
        for (const auto& elem : *post) {
            if (auto* t = elem.as_table()) {
                auto rule = parse_spawn_rule(*t);
                if (!rule.program.empty()) {
                    cfg.post_init.push_back(std::move(rule));
                } else {
                    warnings.push_back("[[post_init]] entry without 'program' in " + path + " — skipped");
                }
            }
        }
    }

    // --- [[bind]] ---
    if (auto* binds_arr = tbl.get_as<toml::array>("bind")) {
        for (const auto& elem : *binds_arr) {
            if (auto* t = elem.as_table()) {
                auto kb = parse_keybind(*t);
                if (!kb.keys.empty() && !kb.action.empty()) {
                    cfg.binds.push_back(std::move(kb));
                } else {
                    warnings.push_back("[[bind]] entry missing 'keys' or 'action' in " + path + " — skipped");
                }
            }
        }
    }

    // --- [theme] ---
    if (auto* theme_node = tbl.get("theme")) {
        if (auto* theme_tbl = theme_node->as_table()) {
            // Background
            parse_float_array(theme_tbl->get("background"), cfg.theme.background, cfg.theme.background);

            // Border
            if (auto* border_node = theme_tbl->get("border")) {
                if (auto* border_tbl = border_node->as_table()) {
                    parse_float_array(border_tbl->get("focused"), cfg.theme.border_focused, cfg.theme.border_focused);
                    parse_float_array(border_tbl->get("unfocused"), cfg.theme.border_unfocused, cfg.theme.border_unfocused);
                    if (auto w = (*border_tbl)["width"].value<int>()) {
                        cfg.theme.border_width = *w;
                    }
                }
            }

            // Shadow
            if (auto* shadow_node = theme_tbl->get("shadow")) {
                if (auto* shadow_tbl = shadow_node->as_table()) {
                    if (auto v = (*shadow_tbl)["enabled"].value<bool>()) {
                        cfg.theme.shadow_enabled = *v;
                    }
                    if (auto v = (*shadow_tbl)["layers"].value<int>()) {
                        cfg.theme.shadow_layers = std::clamp(*v, 0, 8);
                    }
                    if (auto* off = (*shadow_tbl).get("offset")) {
                        if (auto* off_arr = off->as_array(); off_arr && off_arr->size() >= 2) {
                            if (auto x = (*off_arr)[0].value<double>()) cfg.theme.shadow_offset_x = static_cast<float>(*x);
                            if (auto y = (*off_arr)[1].value<double>()) cfg.theme.shadow_offset_y = static_cast<float>(*y);
                        }
                    }
                    if (auto v = (*shadow_tbl)["grow"].value<double>()) {
                        cfg.theme.shadow_grow = static_cast<float>(*v);
                    }
                    parse_float_array(shadow_tbl->get("alphas"), cfg.theme.shadow_alphas, cfg.theme.shadow_alphas);
                }
            }

            // Group hues
            if (auto* hues_node = theme_tbl->get("group_hues")) {
                if (auto* hues_arr = hues_node->as_array()) {
                    for (size_t g = 0; g < 6 && g < hues_arr->size(); g++) {
                        if (auto* rgb = (*hues_arr)[g].as_array(); rgb && rgb->size() >= 3) {
                            for (int c = 0; c < 3; c++) {
                                if (auto v = (*rgb)[c].value<double>()) {
                                    cfg.theme.group_hues[g][c] = static_cast<float>(*v);
                                }
                            }
                        }
                    }
                }
            }

            // Animations (can be under [theme.animations])
            if (auto* anim_node = theme_tbl->get("animations")) {
                if (auto* anim_tbl = anim_node->as_table()) {
                    if (auto v = (*anim_tbl)["move_duration"].value<double>())
                        cfg.animations.move_duration = static_cast<float>(*v);
                    if (auto v = (*anim_tbl)["resize_duration"].value<double>())
                        cfg.animations.resize_duration = static_cast<float>(*v);
                    if (auto v = (*anim_tbl)["open_duration"].value<double>())
                        cfg.animations.open_duration = static_cast<float>(*v);
                    if (auto v = (*anim_tbl)["close_duration"].value<double>())
                        cfg.animations.close_duration = static_cast<float>(*v);
                    if (auto v = (*anim_tbl)["viewport_duration"].value<double>())
                        cfg.animations.viewport_duration = static_cast<float>(*v);
                    if (auto v = (*anim_tbl)["snap_threshold"].value<double>())
                        cfg.animations.snap_threshold = static_cast<float>(*v);
                }
            }
        }
    }

    // --- [input] ---
    if (auto* input_node = tbl.get("input")) {
        if (auto* input_tbl = input_node->as_table()) {
            if (auto v = (*input_tbl)["pan_step"].value<double>())
                cfg.input.pan_step = static_cast<float>(*v);
            if (auto v = (*input_tbl)["zoom_step"].value<double>())
                cfg.input.zoom_step = static_cast<float>(*v);
            if (auto v = (*input_tbl)["snap_distance"].value<double>())
                cfg.input.snap_distance = static_cast<float>(*v);
            if (auto v = (*input_tbl)["group_radius"].value<double>())
                cfg.input.group_radius = static_cast<float>(*v);
            if (auto v = (*input_tbl)["min_zoom"].value<double>())
                cfg.input.min_zoom = static_cast<float>(*v);
            if (auto v = (*input_tbl)["max_zoom"].value<double>())
                cfg.input.max_zoom = static_cast<float>(*v);
        }
    }

    return cfg;
}

/// Recursively resolve imports and merge into a single ChromaConfig.
/// @param path  Path to the config file to parse.
/// @param visited  Set of canonical paths already visited (for cycle detection).
/// @param is_root  True if this is the root (main) config file.
/// @param warnings  Appended to with non-fatal diagnostics.
/// @param errors  Appended to with fatal diagnostics.
static ChromaConfig resolve_imports(const std::string& path,
                                     std::set<std::string>& visited,
                                     bool is_root,
                                     std::vector<std::string>& warnings,
                                     std::vector<std::string>& errors) {
    std::string canon = canonical_path(path);
    if (visited.count(canon)) {
        warnings.push_back("Skipping already-imported file: " + path);
        return ChromaConfig{};
    }
    visited.insert(canon);

    ChromaConfig cfg = parse_one_file(path, warnings, errors);

    // Save imports list before resolving (we'll clear it after)
    auto import_paths = std::move(cfg.imports);
    cfg.imports.clear();

    // Resolve imports recursively
    std::string base_dir = dir_of(path);
    for (const auto& imp : import_paths) {
        std::string resolved = resolve_path(base_dir, imp);

        // Check if the file exists
        std::ifstream f(resolved);
        if (!f.good()) {
            warnings.push_back("Imported file not found: " + resolved + " (from " + path + ")");
            continue;
        }

        ChromaConfig imported = resolve_imports(resolved, visited, false, warnings, errors);

        // Merge: for the root config, imported values are defaults (root wins).
        // For non-root, later imports override earlier ones in merge order.
        if (is_root) {
            // Root merges from imported: imported values are defaults
            cfg.merge_from(imported);
        } else {
            // Non-root: newer imports win (swap merge direction)
            imported.merge_from(cfg);
            cfg = std::move(imported);
        }
    }

    return cfg;
}

// ============================================================================
// Public API
// ============================================================================

ConfigResult load_config(const std::string& path) {
    ConfigResult result;

    // Check if file exists
    std::ifstream f(path);
    if (!f.good()) {
        // No config file — use defaults
        result.success = true;
        result.config.reset();
        result.warnings.push_back("Config file not found at " + path + " — using defaults");
        return result;
    }
    f.close();

    std::set<std::string> visited;
    result.config = resolve_imports(path, visited, true, result.warnings, result.errors);

    if (result.errors.empty()) {
        result.success = true;
    } else {
        result.success = false;
        // Fall back to defaults on fatal errors
        result.config.reset();
    }

    return result;
}

std::string default_config_toml() {
    return R"(# Chroma WM default configuration
# Location: ~/.config/chroma/config.toml (or $XDG_CONFIG_HOME/chroma/config.toml)
#
# You can split this across multiple files using imports:
#   imports = ["theme.toml", "keybinds.toml", "programs.toml"]
# Import paths are relative to the importing file.

# -------------------------------------------------------------------
# Pre-init programs — launched BEFORE the Wayland backend starts.
# Use for environment setup, authentication agents, etc.
# -------------------------------------------------------------------
# [[pre_init]]
# program = "/usr/bin/systemctl"
# args = ["--user", "import-environment", "WAYLAND_DISPLAY"]

# -------------------------------------------------------------------
# Post-init programs — launched AFTER the Wayland backend is up.
# Use for status bars, wallpapers, notification daemons, etc.
# -------------------------------------------------------------------
# [[post_init]]
# program = "waybar"
# args = []
#
# [[post_init]]
# program = "swaybg"
# args = ["-i", "~/wallpaper.jpg"]

# -------------------------------------------------------------------
# Keybindings
# -------------------------------------------------------------------
# Format: [[bind]]
#           keys = "Mod1+Mod2+Key"
#           action = "action_name"
#           arg = "optional argument"
#
# Modifiers: Super, Shift, Ctrl, Alt
# Keys: A-Z, 0-9, F1-F12, Tab, Return, Escape, Space, Backspace,
#       Delete, Left, Right, Up, Down, Plus, Minus, Equal,
#       LeftBracket, RightBracket, Period, Comma, Slash,
#       Backslash, Semicolon, Apostrophe, Grave
#
# Internal actions: quit, reload_config,
#   pan_left, pan_right, pan_up, pan_down,
#   zoom_in, zoom_out,
#   focus_next, focus_prev,
#   stack_cycle, stack_window, unstack_window,
#   group_here, jump_next_group, jump_prev_group,
#   jump_group_1 .. jump_group_9
# External actions:
#   spawn (requires arg = "program args"), exec

[[bind]]
keys = "Super+Shift+E"
action = "quit"

[[bind]]
keys = "Super+Tab"
action = "focus_next"

[[bind]]
keys = "Super+Shift+Tab"
action = "focus_prev"

[[bind]]
keys = "Super+Left"
action = "pan_left"

[[bind]]
keys = "Super+Right"
action = "pan_right"

[[bind]]
keys = "Super+Up"
action = "pan_up"

[[bind]]
keys = "Super+Down"
action = "pan_down"

[[bind]]
keys = "Super+Equal"
action = "zoom_in"

[[bind]]
keys = "Super+Minus"
action = "zoom_out"

[[bind]]
keys = "Super+S"
action = "stack_cycle"

[[bind]]
keys = "Super+Shift+S"
action = "stack_window"

[[bind]]
keys = "Super+Shift+X"
action = "unstack_window"

[[bind]]
keys = "Super+G"
action = "group_here"

[[bind]]
keys = "Super+LeftBracket"
action = "jump_prev_group"

[[bind]]
keys = "Super+RightBracket"
action = "jump_next_group"

[[bind]]
keys = "Super+1"
action = "jump_group_1"

[[bind]]
keys = "Super+2"
action = "jump_group_2"

[[bind]]
keys = "Super+3"
action = "jump_group_3"

[[bind]]
keys = "Super+4"
action = "jump_group_4"

[[bind]]
keys = "Super+5"
action = "jump_group_5"

[[bind]]
keys = "Super+6"
action = "jump_group_6"

[[bind]]
keys = "Super+7"
action = "jump_group_7"

[[bind]]
keys = "Super+8"
action = "jump_group_8"

[[bind]]
keys = "Super+9"
action = "jump_group_9"

# Uncomment to enable config reload keybind:
# [[bind]]
# keys = "Super+Shift+R"
# action = "reload_config"

# Uncomment to add a launcher:
# [[bind]]
# keys = "Super+Return"
# action = "spawn"
# arg = "alacritty"
#
# [[bind]]
# keys = "Super+D"
# action = "spawn"
# arg = "wofi --show drun"

# -------------------------------------------------------------------
# Theme
# -------------------------------------------------------------------
[theme]
# Background color (R, G, B, A) — fills areas not covered by windows
background = [0.08, 0.08, 0.10, 1.0]

[theme.border]
# Border colors for focused and unfocused windows
focused = [0.38, 0.30, 0.60, 0.85]
unfocused = [0.18, 0.18, 0.22, 0.60]
width = 1

[theme.shadow]
enabled = true
layers = 4
offset = [2.0, 2.0]
grow = 4.0
# Alpha per shadow layer (innermost to outermost)
alphas = [0.18, 0.12, 0.07, 0.03]

# Group indicator hues (6 colors, each [R, G, B])
group_hues = [
    [0.95, 0.45, 0.20],
    [0.25, 0.65, 0.95],
    [0.40, 0.80, 0.35],
    [0.85, 0.35, 0.75],
    [0.95, 0.85, 0.15],
    [0.30, 0.75, 0.80],
]

[theme.animations]
move_duration = 0.15
resize_duration = 0.20
open_duration = 0.25
close_duration = 0.18
viewport_duration = 0.35
snap_threshold = 5.0

# -------------------------------------------------------------------
# Input tuning
# -------------------------------------------------------------------
[input]
pan_step = 100.0
zoom_step = 0.1
snap_distance = 200.0
group_radius = 800.0
min_zoom = 0.25
max_zoom = 4.0
)";
}

} // namespace chroma
