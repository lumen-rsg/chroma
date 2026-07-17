#pragma once

/// @file keys.hpp
/// @brief Key string parser — converts human-readable keybind strings
/// like "Super+Shift+E" into (modifier mask, XKB keysym) tuples and back.
///
/// Pure header, no dependencies beyond standard library.
/// Requires `types.hpp` to be included first (for Mod:: constants).

#include "types.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <optional>
#include <cctype>

namespace chroma {

// ============================================================================
// KeyParseResult
// ============================================================================

struct KeyParseResult {
    uint32_t modifiers{0};
    uint32_t keysym{0};
};

// ============================================================================
// Key string parser
// ============================================================================

/// Parse a keybind string like "Super+Shift+E" into modifier mask + keysym.
/// Returns std::nullopt on parse failure.
/// Supports: "Super", "Shift", "Ctrl", "Alt" as modifiers.
/// Keys are case-insensitive for letters.
std::optional<KeyParseResult> parse_keybind_string(std::string_view str);

/// Parse a single keysym name (e.g. "E", "Return", "Tab") to its XKB keysym.
/// Returns 0 if the name is not recognized.
uint32_t parse_keysym_string(std::string_view name);

/// Parse a modifier string (e.g. "Super+Shift") to a modifier mask.
/// Returns 0 if no modifier recognized.
uint32_t parse_modifier_string(std::string_view str);

/// Convert a keysym to its human-readable name (e.g. 0x0065 → "E").
/// Returns "Unknown" if not in the lookup table.
std::string_view keysym_to_string(uint32_t keysym);

/// Convert a modifier mask to a human-readable string (e.g. Mod::SUPER|Mod::SHIFT → "Super+Shift").
std::string modifier_to_string(uint32_t modifiers);

// ============================================================================
// Internal lookup tables (exposed for testing)
// ============================================================================

namespace key_lookup {

/// Build and return the modifier name → mask map.
inline const std::unordered_map<std::string_view, uint32_t>& modifier_map() {
    static const std::unordered_map<std::string_view, uint32_t> map = {
        {"super", Mod::SUPER},
        {"shift", Mod::SHIFT},
        {"ctrl",  Mod::CTRL},
        {"control", Mod::CTRL},
        {"alt",   Mod::ALT},
        {"mod1",  Mod::ALT},
    };
    return map;
}

/// Build and return the keysym name → keysym map.
inline const std::unordered_map<std::string_view, uint32_t>& keysym_map() {
    static const std::unordered_map<std::string_view, uint32_t> map = {
        // Letters (lowercase names)
        {"a", 0x0061}, {"b", 0x0062}, {"c", 0x0063}, {"d", 0x0064},
        {"e", 0x0065}, {"f", 0x0066}, {"g", 0x0067}, {"h", 0x0068},
        {"i", 0x0069}, {"j", 0x006a}, {"k", 0x006b}, {"l", 0x006c},
        {"m", 0x006d}, {"n", 0x006e}, {"o", 0x006f}, {"p", 0x0070},
        {"q", 0x0071}, {"r", 0x0072}, {"s", 0x0073}, {"t", 0x0074},
        {"u", 0x0075}, {"v", 0x0076}, {"w", 0x0077}, {"x", 0x0078},
        {"y", 0x0079}, {"z", 0x007a},

        // Digits
        {"0", 0x0030}, {"1", 0x0031}, {"2", 0x0032}, {"3", 0x0033},
        {"4", 0x0034}, {"5", 0x0035}, {"6", 0x0036}, {"7", 0x0037},
        {"8", 0x0038}, {"9", 0x0039},

        // Function keys
        {"f1",  0xFFBE}, {"f2",  0xFFBF}, {"f3",  0xFFC0},
        {"f4",  0xFFC1}, {"f5",  0xFFC2}, {"f6",  0xFFC3},
        {"f7",  0xFFC4}, {"f8",  0xFFC5}, {"f9",  0xFFC6},
        {"f10", 0xFFC7}, {"f11", 0xFFC8}, {"f12", 0xFFC9},

        // Navigation
        {"left",  0xFF51}, {"right", 0xFF53},
        {"up",    0xFF52}, {"down",  0xFF54},

        // Special keys
        {"return",    0xFF0D}, {"enter",     0xFF0D},
        {"tab",       0xFF09},
        {"escape",    0xFF1B}, {"esc",        0xFF1B},
        {"space",     0x0020},
        {"backspace", 0xFF08},
        {"delete",    0xFFFF}, {"del",        0xFFFF},
        {"home",      0xFF50},
        {"end",       0xFF57},
        {"pageup",    0xFF55}, {"page_up",    0xFF55},
        {"pagedown",  0xFF56}, {"page_down",  0xFF56},
        {"insert",    0xFF63}, {"ins",        0xFF63},

        // Symbols
        {"plus",         0x002B},
        {"minus",        0x002D},
        {"equal",        0x003D},
        {"leftbracket",  0x005B}, {"[", 0x005B},
        {"rightbracket", 0x005D}, {"]", 0x005D},
        {"period",       0x002E}, {".", 0x002E},
        {"comma",        0x002C}, {",", 0x002C},
        {"slash",        0x002F}, {"/", 0x002F},
        {"backslash",    0x005C}, {"\\", 0x005C},
        {"semicolon",    0x003B}, {";", 0x003B},
        {"apostrophe",   0x0027}, {"'", 0x0027},
        {"grave",        0x0060}, {"`", 0x0060},
        {"minus",        0x002D}, {"-", 0x002D},
    };
    return map;
}

/// Build the reverse map: keysym → name.
inline const std::unordered_map<uint32_t, std::string_view>& keysym_reverse_map() {
    static const std::unordered_map<uint32_t, std::string_view> map = []{
        std::unordered_map<uint32_t, std::string_view> m;
        for (const auto& [name, sym] : keysym_map()) {
            // Prefer shorter names (e.g. "esc" over "escape")
            auto it = m.find(sym);
            if (it == m.end() || name.size() < it->second.size()) {
                m[sym] = name;
            }
        }
        return m;
    }();
    return map;
}

/// Build the modifier mask → name map.
inline const std::unordered_map<uint32_t, std::string_view>& modifier_reverse_map() {
    static const std::unordered_map<uint32_t, std::string_view> map = {
        {Mod::SUPER, "Super"},
        {Mod::SHIFT, "Shift"},
        {Mod::CTRL,  "Ctrl"},
        {Mod::ALT,   "Alt"},
    };
    return map;
}

} // namespace key_lookup

// ============================================================================
// Implementation
// ============================================================================

inline std::optional<KeyParseResult> parse_keybind_string(std::string_view str) {
    KeyParseResult result{};

    // Split by '+'
    std::vector<std::string_view> parts;
    size_t start = 0;
    for (size_t i = 0; i <= str.size(); i++) {
        if (i == str.size() || str[i] == '+') {
            if (i > start) {
                parts.push_back(str.substr(start, i - start));
            }
            start = i + 1;
        }
    }

    if (parts.empty()) return std::nullopt;

    // The last part is the key; everything before is modifiers
    std::string_view key_name = parts.back();

    // Parse key
    // Try exact match first
    uint32_t keysym = parse_keysym_string(key_name);
    if (keysym == 0) {
        return std::nullopt;
    }
    result.keysym = keysym;

    // Parse modifiers
    for (size_t i = 0; i < parts.size() - 1; i++) {
        uint32_t mod = parse_modifier_string(parts[i]);
        if (mod == 0) {
            return std::nullopt; // unknown modifier
        }
        result.modifiers |= mod;
    }

    return result;
}

inline uint32_t parse_keysym_string(std::string_view name) {
    if (name.empty()) return 0;

    // Try exact match
    const auto& map = key_lookup::keysym_map();
    auto it = map.find(name);
    if (it != map.end()) return it->second;

    // Try lowercase
    std::string lower;
    lower.reserve(name.size());
    for (char c : name) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    it = map.find(lower);
    if (it != map.end()) return it->second;

    // Single character: use ASCII keysym
    if (name.size() == 1) {
        unsigned char c = static_cast<unsigned char>(name[0]);
        // Printable ASCII: use the ASCII value directly
        if (c >= 0x20 && c <= 0x7E) {
            // For uppercase letters, return the lowercase keysym
            // (XKB keysyms for letters are lowercase)
            if (c >= 'A' && c <= 'Z') {
                return static_cast<uint32_t>(c + 0x20); // to lowercase
            }
            return static_cast<uint32_t>(c);
        }
    }

    return 0;
}

inline uint32_t parse_modifier_string(std::string_view str) {
    const auto& map = key_lookup::modifier_map();

    // Try exact match
    auto it = map.find(str);
    if (it != map.end()) return it->second;

    // Try lowercase
    std::string lower;
    lower.reserve(str.size());
    for (char c : str) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    it = map.find(lower);
    if (it != map.end()) return it->second;

    return 0;
}

inline std::string_view keysym_to_string(uint32_t keysym) {
    const auto& map = key_lookup::keysym_reverse_map();
    auto it = map.find(keysym);
    if (it != map.end()) return it->second;

    // For ASCII letters/digits, return the character
    if (keysym >= 0x0020 && keysym <= 0x007E) {
        // Use a static buffer for single characters
        static thread_local char buf[2] = {};
        buf[0] = static_cast<char>(keysym);
        buf[1] = '\0';
        // Return as string_view — but careful: this is thread_local so safe
        // but the caller needs to use it before the next call.
        // For our use (logging), this is fine.
        return std::string_view(buf, 1);
    }

    return "Unknown";
}

inline std::string modifier_to_string(uint32_t modifiers) {
    if (modifiers == 0) return "None";

    std::string result;
    const auto& map = key_lookup::modifier_reverse_map();

    // Order: Super, Ctrl, Alt, Shift (conventional display order)
    const uint32_t order[] = {Mod::SUPER, Mod::CTRL, Mod::ALT, Mod::SHIFT};
    for (uint32_t mod : order) {
        if (modifiers & mod) {
            if (!result.empty()) result += '+';
            auto it = map.find(mod);
            if (it != map.end()) {
                result += it->second;
            }
        }
    }

    return result;
}

} // namespace chroma
