#pragma once

/// @file config.hpp
/// @brief Runtime configuration data model and TOML parser for Chroma WM.
///
/// Reads $XDG_CONFIG_HOME/chroma/config.toml (or a user-specified path)
/// and populates a ChromaConfig struct. Supports recursive imports for
/// splitting configuration across multiple files.

#include "types.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <functional>

namespace chroma {

// ============================================================================
// SpawnRule — a program to launch (pre/post-init or via keybind)
// ============================================================================

struct SpawnRule {
    std::string program;
    std::vector<std::string> args;
    std::string workdir;       // empty = inherit compositor's cwd
};

// ============================================================================
// Keybinding — maps a key chord to an action
// ============================================================================

struct Keybinding {
    std::string keys;          ///< e.g. "Super+Shift+E"
    std::string action;        ///< e.g. "quit", "spawn", "focus_next"
    std::string arg;           ///< optional argument (program for spawn/exec)

    // Parsed form (populated by InputRouter at bindmap build time)
    uint32_t modifiers{0};
    uint32_t keysym{0};
};

// ============================================================================
// Theme config — colors, borders, shadows
// ============================================================================

struct ThemeConfig {
    // Background
    float background[4]{0.08f, 0.08f, 0.10f, 1.0f};

    // Borders
    float border_focused[4]{0.38f, 0.30f, 0.60f, 0.85f};
    float border_unfocused[4]{0.18f, 0.18f, 0.22f, 0.60f};
    int border_width{1};

    // Shadows
    bool  shadow_enabled{true};
    int   shadow_layers{4};
    float shadow_offset_x{2.0f};
    float shadow_offset_y{2.0f};
    float shadow_grow{4.0f};
    float shadow_alphas[4]{0.18f, 0.12f, 0.07f, 0.03f};

    // Group indicator hues (6 entries, RGB each)
    float group_hues[6][3]{
        {0.95f, 0.45f, 0.20f},
        {0.25f, 0.65f, 0.95f},
        {0.40f, 0.80f, 0.35f},
        {0.85f, 0.35f, 0.75f},
        {0.95f, 0.85f, 0.15f},
        {0.30f, 0.75f, 0.80f},
    };
};

// ============================================================================
// Animation config — durations in seconds
// ============================================================================

struct AnimationConfig {
    float move_duration{0.15f};
    float resize_duration{0.20f};
    float open_duration{0.25f};
    float close_duration{0.18f};
    float viewport_duration{0.35f};
    float snap_threshold{5.0f};
};

// ============================================================================
// Input config — tuneables
// ============================================================================

struct InputConfig {
    float pan_step{100.0f};
    float zoom_step{0.1f};
    float snap_distance{200.0f};
    float group_radius{800.0f};
    float min_zoom{0.25f};
    float max_zoom{4.0f};
};

// ============================================================================
// ChromaConfig — top-level config
// ============================================================================

struct ChromaConfig {
    std::vector<std::string> imports;
    std::vector<SpawnRule>   pre_init;
    std::vector<SpawnRule>   post_init;
    std::vector<Keybinding>  binds;
    ThemeConfig              theme;
    AnimationConfig          animations;
    InputConfig              input;

    /// Reset all fields to compile-time defaults.
    void reset();

    /// Merge another config into this one. Arrays-of-tables (pre_init,
    /// post_init, binds) are concatenated. Scalar/table values from
    /// `other` are used only if the corresponding field in `this` is
    /// still at its default (i.e. main config overrides imports).
    void merge_from(const ChromaConfig& other);
};

// ============================================================================
// Config loading
// ============================================================================

/// Result of loading a config file.
struct ConfigResult {
    bool success{false};
    ChromaConfig config;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

/// Load and parse a config file, resolving all imports recursively.
/// @param path  Path to the main config TOML file.
/// @return  ConfigResult with success flag, populated config, and diagnostics.
ConfigResult load_config(const std::string& path);

/// Generate a default config as a TOML string.
/// Useful for --print-default-config.
std::string default_config_toml();

} // namespace chroma
