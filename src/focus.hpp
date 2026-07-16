#pragma once

/// @file focus.hpp
/// @brief FocusTracker — maintains MRU (Most-Recently-Used) focus history
/// for keyboard-driven window cycling (Alt+Tab style).
///
/// Pure domain logic with no Wayland dependencies. Uses a bounded deque
/// (max 50 entries) to prevent unbounded growth. Windows are automatically
/// removed from history when destroyed via the remove() method.

#include "types.hpp"
#include "canvas.hpp"
#include <deque>

namespace chroma {

// ============================================================================
// FocusTracker — maintains the Most-Recently-Used focus history
// ============================================================================

class FocusTracker {
public:
    /// Record that a window gained focus.
    void focused(WindowId id);

    /// Record that a window lost focus.
    void unfocused(WindowId id);
    
    /// Remove a window from the history entirely (called on window destroy).
    void remove(WindowId id);

    /// The currently focused window.
    WindowId current() const;

    /// The previously focused window (second in MRU).
    WindowId previous() const;

    /// Cycle focus to the next window in MRU order.
    /// Returns the newly focused window ID, or INVALID_WINDOW.
    WindowId cycle_next(Canvas& canvas);

    /// Cycle focus to the previous window.
    WindowId cycle_prev(Canvas& canvas);

    /// Number of windows in the history.
    size_t size() const { return history_.size(); }

    /// Clear all history.
    void clear();

private:
    std::deque<WindowId> history_;

    void bring_to_front(WindowId id);
};

// ============================================================================
// Implementation (inline)
// ============================================================================

inline void FocusTracker::focused(WindowId id) {
    if (id == INVALID_WINDOW) return;
    bring_to_front(id);
}

inline void FocusTracker::unfocused(WindowId id) {
    // We keep it in history for cycling back
    (void)id;
}

inline WindowId FocusTracker::current() const {
    return history_.empty() ? INVALID_WINDOW : history_.front();
}

inline WindowId FocusTracker::previous() const {
    return history_.size() > 1 ? history_[1] : INVALID_WINDOW;
}

inline WindowId FocusTracker::cycle_next(Canvas& canvas) {
    if (history_.size() < 2) return INVALID_WINDOW;
    // Move front to back, new front becomes focused
    WindowId old = history_.front();
    history_.pop_front();
    history_.push_back(old);
    WindowId next = history_.front();

    canvas.set_focus(next);
    return next;
}

inline WindowId FocusTracker::cycle_prev(Canvas& canvas) {
    if (history_.size() < 2) return INVALID_WINDOW;
    // Move back to front
    WindowId prev = history_.back();
    history_.pop_back();
    history_.push_front(prev);

    canvas.set_focus(prev);
    return prev;
}

inline void FocusTracker::clear() {
    history_.clear();
}

inline void FocusTracker::remove(WindowId id) {
    auto it = std::find(history_.begin(), history_.end(), id);
    if (it != history_.end()) {
        history_.erase(it);
    }
}

inline void FocusTracker::bring_to_front(WindowId id) {
    // Remove if present, then push front
    auto it = std::find(history_.begin(), history_.end(), id);
    if (it != history_.end()) {
        history_.erase(it);
    }
    history_.push_front(id);

    // Keep history bounded
    while (history_.size() > 50) {
        history_.pop_back();
    }
}

} // namespace chroma
