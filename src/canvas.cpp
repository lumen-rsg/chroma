#include "canvas.hpp"
#include <algorithm>
#include <cstdio>

namespace chroma {

// ============================================================================
// WindowGroup
// ============================================================================

Vec2 WindowGroup::centroid(const Canvas& canvas) const {
    if (windows.empty()) return {0, 0};
    Vec2 sum{0, 0};
    for (auto id : windows) {
        if (auto* w = canvas.get(id)) {
            sum = sum + w->center();
        }
    }
    return sum / static_cast<float>(windows.size());
}

// ============================================================================
// Canvas — Viewport
// ============================================================================

void Canvas::pan(Vec2 delta) {
    viewport_center = viewport_center + delta;
}

void Canvas::zoom_at(float factor, Vec2 anchor, Vec2 screen_size) {
    float new_zoom = std::clamp(zoom * factor, config::MIN_ZOOM, config::MAX_ZOOM);
    if (new_zoom == zoom) return;

    // Convert anchor from screen to canvas space before zoom,
    // then adjust viewport so the anchor stays fixed.
    // canvas_point = (anchor - screen_center) / old_zoom + viewport_center
    Vec2 screen_center = screen_size * 0.5f;
    Vec2 canvas_anchor = (anchor - screen_center) / zoom + viewport_center;

    zoom = new_zoom;

    // After zoom, we want the same canvas_anchor to map to anchor:
    // anchor = (canvas_anchor - new_viewport) * new_zoom + screen_center
    // new_viewport = canvas_anchor - (anchor - screen_center) / new_zoom
    viewport_center = canvas_anchor - (anchor - screen_center) / zoom;
}

Rect Canvas::visible_rect(Vec2 screen_size) const {
    // The visible region in canvas space is the screen mapped backward.
    Vec2 half = screen_size * 0.5f / zoom;
    return Rect{viewport_center - half, screen_size / zoom};
}

Rect Canvas::canvas_to_screen(const Rect& r, Vec2 screen_size) const {
    Vec2 center = screen_size * 0.5f;
    return Rect{
        (r.pos - viewport_center) * zoom + center,
        r.size * zoom
    };
}

Vec2 Canvas::screen_to_canvas(Vec2 screen_pos, Vec2 screen_size) const {
    Vec2 center = screen_size * 0.5f;
    return (screen_pos - center) / zoom + viewport_center;
}

Transform2D Canvas::viewport_transform(Vec2 /*screen_size*/) const {
    return Transform2D{viewport_center, zoom};
}

// ============================================================================
// Canvas — Window Management
// ============================================================================

WindowId Canvas::add(ChromaWindow window) {
    WindowId id = window.id;
    if (id == INVALID_WINDOW) {
        id = next_window_id();
        window.id = id;
    }
    windows_[id] = std::move(window);
    z_order_.push_back(id);  // new windows go on top
    return id;
}

ChromaWindow Canvas::remove(WindowId id) {
    auto it = windows_.find(id);
    if (it == windows_.end()) {
        std::fprintf(stderr, "[chroma] Canvas::remove: window %lu not found\n", id);
        return ChromaWindow{};
    }
    ChromaWindow w = std::move(it->second);
    windows_.erase(it);

    // Remove from z_order
    auto z_it = std::find(z_order_.begin(), z_order_.end(), id);
    if (z_it != z_order_.end()) {
        z_order_.erase(z_it);
    }

    // Remove from group
    if (w.group != NO_GROUP) {
        remove_from_group(w.id);
    }

    // Clear focus if this was focused
    if (focused_ == id) {
        focused_ = INVALID_WINDOW;
    }

    return w;
}

void Canvas::move_window(WindowId id, Vec2 new_pos) {
    if (auto* w = get(id)) {
        w->canvas_pos = new_pos;
    }
}

void Canvas::resize_window(WindowId id, Vec2 new_size) {
    if (auto* w = get(id)) {
        w->size = new_size;
    }
}

ChromaWindow* Canvas::get(WindowId id) {
    auto it = windows_.find(id);
    return it != windows_.end() ? &it->second : nullptr;
}

const ChromaWindow* Canvas::get(WindowId id) const {
    auto it = windows_.find(id);
    return it != windows_.end() ? &it->second : nullptr;
}

std::vector<WindowId> Canvas::visible_windows(Vec2 screen_size) const {
    Rect view = visible_rect(screen_size);
    // Add a margin to catch windows that are partially visible
    Rect expanded = view.expanded(100.0f / zoom);

    std::vector<WindowId> result;
    for (const auto& [id, w] : windows_) {
        if (!w.mapped) continue;
        if (w.stack != NO_STACK) continue; // stacked windows handled by stack renderer
        if (expanded.overlaps(w.canvas_rect())) {
            result.push_back(id);
        }
    }
    return result;
}

WindowId Canvas::window_at(Vec2 canvas_pos) const {
    // Iterate z_order from topmost (back) to bottom (front).
    // First window that contains the point wins.
    for (auto it = z_order_.rbegin(); it != z_order_.rend(); ++it) {
        auto win_it = windows_.find(*it);
        if (win_it == windows_.end()) continue;
        const auto& w = win_it->second;
        if (!w.mapped) continue;
        if (w.canvas_rect().contains(canvas_pos)) {
            return *it;
        }
    }
    return INVALID_WINDOW;
}

// ============================================================================
// Canvas — Focus
// ============================================================================

void Canvas::set_focus(WindowId id) {
    if (focused_ != INVALID_WINDOW) {
        if (auto* w = get(focused_)) {
            w->focused = false;
        }
    }
    focused_ = id;
    if (auto* w = get(id)) {
        w->focused = true;
        raise_to_top(id);
    }
}

void Canvas::raise_to_top(WindowId id) {
    auto it = std::find(z_order_.begin(), z_order_.end(), id);
    if (it != z_order_.end()) {
        z_order_.erase(it);
    }
    z_order_.push_back(id);
}

// ============================================================================
// Canvas — Group Management
// ============================================================================

GroupId Canvas::create_group(std::string name) {
    GroupId id = next_group_id();
    groups_[id] = WindowGroup{id, std::move(name)};
    return id;
}

void Canvas::destroy_group(GroupId id) {
    if (auto* g = get_group(id)) {
        // Ungroup all windows in this group
        auto win_ids = g->windows; // copy — remove_from_group mutates
        for (auto wid : win_ids) {
            remove_from_group(wid);
        }
    }
    groups_.erase(id);
}

void Canvas::add_to_group(WindowId window_id, GroupId group_id) {
    auto* w = get(window_id);
    auto* g = get_group(group_id);
    if (!w || !g) return;

    // Remove from previous group
    if (w->group != NO_GROUP) {
        remove_from_group(window_id);
    }

    w->group = group_id;
    g->windows.push_back(window_id);
}

void Canvas::remove_from_group(WindowId window_id) {
    auto* w = get(window_id);
    if (!w || w->group == NO_GROUP) return;

    if (auto* g = get_group(w->group)) {
        auto& vec = g->windows;
        vec.erase(std::remove(vec.begin(), vec.end(), window_id), vec.end());
    }
    w->group = NO_GROUP;
}

WindowGroup* Canvas::get_group(GroupId id) {
    auto it = groups_.find(id);
    return it != groups_.end() ? &it->second : nullptr;
}

const WindowGroup* Canvas::get_group(GroupId id) const {
    auto it = groups_.find(id);
    return it != groups_.end() ? &it->second : nullptr;
}

// ============================================================================
// Canvas — ID Generation
// ============================================================================

WindowId Canvas::next_window_id() { return next_wid_++; }
GroupId  Canvas::next_group_id()  { return next_gid_++; }
StackId  Canvas::next_stack_id()  { return next_sid_++; }

} // namespace chroma
