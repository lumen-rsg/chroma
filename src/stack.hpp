#pragma once

/// @file stack.hpp
/// @brief CardStack and StackManager — window card-stacking with visual
/// cascading. Stacks group windows as a deck where only the top card is
/// fully visible; cycling rotates the deck. Pure domain logic.

#include "types.hpp"
#include "canvas.hpp"
#include <unordered_map>
#include <vector>

namespace chroma {

// ============================================================================
// CardStack — a deck of stacked windows
// ============================================================================

class CardStack {
public:
    StackId id{NO_STACK};

    /// The canvas position of the stack (where the top card sits).
    Vec2 position{0, 0};

    /// Windows in this stack, ordered front-to-back.
    /// [0] is the active (top) card, [n-1] is the bottom.
    std::vector<WindowId> windows;

    /// Get the top (active) window.
    WindowId top() const {
        return windows.empty() ? INVALID_WINDOW : windows.front();
    }

    /// Number of windows in the stack.
    size_t depth() const { return windows.size(); }

    /// True if empty.
    bool empty() const { return windows.empty(); }

    /// Push a window onto the top of the stack.
    void push(WindowId w) {
        // Remove if already present, then insert at front
        auto it = std::find(windows.begin(), windows.end(), w);
        if (it != windows.end()) windows.erase(it);
        windows.insert(windows.begin(), w);
    }

    /// Remove and return the top window. Returns INVALID_WINDOW if empty.
    WindowId pop() {
        if (windows.empty()) return INVALID_WINDOW;
        WindowId w = windows.front();
        windows.erase(windows.begin());
        return w;
    }

    /// Cycle the stack: top goes to bottom, second becomes top.
    /// Returns the new top window.
    WindowId cycle() {
        if (windows.size() < 2) return top();
        WindowId old_top = windows.front();
        windows.erase(windows.begin());
        windows.push_back(old_top);
        return windows.front();
    }

    /// Visual offset for a card at the given index (0 = top, no offset).
    Vec2 offset_for(size_t index) const {
        if (index == 0) return {0, 0};
        float d = static_cast<float>(index) * config::STACK_OFFSET;
        return {d, d};  // diagonal cascade
    }

    /// Remove a specific window from anywhere in the stack.
    /// Returns true if the window was found and removed.
    bool remove(WindowId w) {
        auto it = std::find(windows.begin(), windows.end(), w);
        if (it == windows.end()) return false;
        windows.erase(it);
        return true;
    }
};

// ============================================================================
// StackManager — manages all card stacks on the canvas
// ============================================================================

class StackManager {
public:
    /// Create a new empty stack at the given canvas position.
    StackId create(Vec2 pos);

    /// Remove a stack (unstacking all windows first).
    void destroy(Canvas& canvas, StackId id);

    /// Stack a window onto a stack. Window must not already be in a stack.
    void stack(Canvas& canvas, WindowId window_id, StackId stack_id);

    /// Unstack a window (restore to the stack's canvas position).
    void unstack(Canvas& canvas, WindowId window_id);

    /// Cycle a stack (bring next card to front).
    WindowId cycle(Canvas& canvas, StackId stack_id);

    /// Get a stack by ID.
    CardStack* get(StackId id);
    const CardStack* get(StackId id) const;

    const std::unordered_map<StackId, CardStack>& all_stacks() const { return stacks_; }

private:
    std::unordered_map<StackId, CardStack> stacks_;
    StackId next_sid_{1};
};

} // namespace chroma
