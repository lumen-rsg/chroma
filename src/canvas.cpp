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
    // Assign a default cascading position if none was set by the caller.
    if (window.canvas_pos.x == 0.0f && window.canvas_pos.y == 0.0f) {
        window.canvas_pos = next_default_position();
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

    // Remove from group BEFORE erasing — remove_from_group calls get(id)
    if (it->second.group != NO_GROUP) {
        remove_from_group(id);
    }

    ChromaWindow w = std::move(it->second);
    windows_.erase(it);

    // Remove from z_order
    auto z_it = std::find(z_order_.begin(), z_order_.end(), id);
    if (z_it != z_order_.end()) {
        z_order_.erase(z_it);
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
    groups_[id] = WindowGroup{.id = id, .name = std::move(name), .windows = {}, .magnetism_strength = config::ATTRACTION_STR};
    group_order_.push_back(id);
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

    // Remove from group order
    auto order_it = std::find(group_order_.begin(), group_order_.end(), id);
    if (order_it != group_order_.end()) {
        group_order_.erase(order_it);
    }

    // Clear active group if this was it
    if (active_group_ == id) {
        active_group_ = NO_GROUP;
    }
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

Vec2 Canvas::next_default_position() {
    Vec2 rel_pos = next_default_rel_pos_;
    next_default_rel_pos_ = next_default_rel_pos_ + default_offset_;
    if (next_default_rel_pos_.x > 800.0f) next_default_rel_pos_ = {-700, next_default_rel_pos_.y + 300};
    if (next_default_rel_pos_.y > 600.0f) next_default_rel_pos_ = {-700, -500};
    // Offset by the current viewport center so windows appear where the user
    // is looking, not at the fixed canvas origin.
    return viewport_center + rel_pos;
}

// ============================================================================
// Canvas — Group Navigation
// ============================================================================

GroupId Canvas::group_at_index(int index) const {
    // index is 1-based (1 → first group in order)
    if (index < 1 || static_cast<size_t>(index) > group_order_.size()) {
        return NO_GROUP;
    }
    return group_order_[static_cast<size_t>(index - 1)];
}

void Canvas::jump_to_group(GroupId id, Vec2 screen_size) {
    if (id == NO_GROUP) return;
    auto* group = get_group(id);
    if (!group || group->empty()) return;

    Vec2 centroid = group->centroid(*this);

    // Compute a zoom level that fits the group's bounds comfortably.
    // Find the bounding box of all group members.
    Rect bounds = group->windows.empty()
        ? Rect{centroid, {0, 0}}
        : get(group->windows[0])->canvas_rect();
    for (size_t i = 1; i < group->windows.size(); i++) {
        auto* w = get(group->windows[i]);
        if (!w) continue;
        Rect r = w->canvas_rect();
        if (r.left() < bounds.pos.x) bounds.pos.x = r.left();
        if (r.top() < bounds.pos.y) bounds.pos.y = r.top();
    }
    // Extend bounds to cover the full extent of all windows
    for (auto wid : group->windows) {
        auto* w = get(wid);
        if (!w) continue;
        Rect r = w->canvas_rect();
        float right = r.right();
        float bottom = r.bottom();
        if (right > bounds.right()) bounds.size.x = right - bounds.pos.x;
        if (bottom > bounds.bottom()) bounds.size.y = bottom - bounds.pos.y;
    }

    // Add padding (20% of group extent, min 200 canvas units)
    float pad = std::max(std::max(bounds.w(), bounds.h()) * 0.2f, 200.0f);
    Rect padded_bounds = bounds.expanded(pad);

    // Fit the padded bounds into the viewport
    float fit_zoom_x = screen_size.x / padded_bounds.w();
    float fit_zoom_y = screen_size.y / padded_bounds.h();
    float target_zoom = std::min(fit_zoom_x, fit_zoom_y);
    target_zoom = std::clamp(target_zoom, config::MIN_ZOOM, config::MAX_ZOOM);

    // Never zoom out beyond the current zoom level — the user can control
    // zoom manually via Super+scroll.  Zooming out too far makes everything
    // tiny and breaks the directional indicators (all groups become visible).
    if (target_zoom < zoom) target_zoom = zoom;

    // Animate to the new viewport state
    viewport_anim_.start(viewport_center, zoom,
                         centroid, target_zoom,
                         config::ANIM_VIEWPORT_DURATION);
    active_group_ = id;
}

GroupId Canvas::cycle_next_group(Vec2 screen_size) {
    // Count how many non-empty groups exist
    size_t valid_count = 0;
    for (GroupId gid : group_order_) {
        auto* g = get_group(gid);
        if (g && !g->empty()) valid_count++;
    }
    if (valid_count == 0) return NO_GROUP;

    // Find active_group_'s position in the order list
    auto it = std::find(group_order_.begin(), group_order_.end(), active_group_);
    size_t start_idx;
    if (it != group_order_.end()) {
        start_idx = static_cast<size_t>(it - group_order_.begin());
    } else {
        start_idx = group_order_.size() - 1; // so first advance goes to 0
    }

    // Advance, skipping empty groups, wrapping around
    for (size_t attempt = 0; attempt < group_order_.size(); attempt++) {
        start_idx = (start_idx + 1) % group_order_.size();
        GroupId candidate = group_order_[start_idx];
        auto* g = get_group(candidate);
        if (g && !g->empty()) {
            jump_to_group(candidate, screen_size);
            return candidate;
        }
    }

    return NO_GROUP; // all groups empty
}

GroupId Canvas::cycle_prev_group(Vec2 screen_size) {
    // Count how many non-empty groups exist
    size_t valid_count = 0;
    for (GroupId gid : group_order_) {
        auto* g = get_group(gid);
        if (g && !g->empty()) valid_count++;
    }
    if (valid_count == 0) return NO_GROUP;

    // Find active_group_'s position in the order list
    auto it = std::find(group_order_.begin(), group_order_.end(), active_group_);
    size_t start_idx;
    if (it != group_order_.end()) {
        start_idx = static_cast<size_t>(it - group_order_.begin());
    } else {
        start_idx = 0; // so first retreat wraps to end
    }

    // Retreat, skipping empty groups, wrapping around
    for (size_t attempt = 0; attempt < group_order_.size(); attempt++) {
        start_idx = (start_idx == 0) ? group_order_.size() - 1 : start_idx - 1;
        GroupId candidate = group_order_[start_idx];
        auto* g = get_group(candidate);
        if (g && !g->empty()) {
            jump_to_group(candidate, screen_size);
            return candidate;
        }
    }

    return NO_GROUP; // all groups empty
}

// ============================================================================
// Canvas — Viewport Animation
// ============================================================================

bool Canvas::tick_viewport_animation(float dt) {
    if (!viewport_anim_.active()) return false;

    bool still_active = viewport_anim_.tick(dt);

    viewport_center = viewport_anim_.current_center();
    zoom = viewport_anim_.current_zoom();

    if (!still_active) {
        // Snap to exact target
        viewport_center = viewport_anim_.target_center;
        zoom = viewport_anim_.target_zoom;
    }

    return still_active;
}

bool Canvas::viewport_animating() const {
    return viewport_anim_.active();
}

// ============================================================================
// Canvas — Directional Indicators
// ============================================================================

std::vector<GroupIndicator> Canvas::compute_indicators(Vec2 screen_size) const {
    std::vector<GroupIndicator> result;

    if (group_order_.empty()) return result;

    Rect view = visible_rect(screen_size);
    // The distance at which we start showing an indicator (1/2 view diagonal)
    // For a group completely outside the view, we want an indicator.
    float view_diag = view.size.length() * 0.6f;

    for (GroupId gid : group_order_) {
        auto* group = get_group(gid);
        if (!group || group->empty()) continue;
        if (gid == active_group_) continue;  // no indicator for the active group

        Vec2 centroid = group->centroid(*this);

        // Skip if the centroid is inside the visible rect
        if (view.contains(centroid)) continue;

        // Direction from viewport center to group centroid
        Vec2 to_group = centroid - viewport_center;
        float dist = to_group.length();
        if (dist < 1.0f) continue;

        Vec2 dir = to_group.normalized();

        // Compute where this direction intersects the screen edge.
        // In screen space, the center projects to (screen_center), and
        // the direction in screen space is the same (zoom is uniform).
        Vec2 screen_center = screen_size * 0.5f;
        Vec2 half_size = screen_size * 0.5f - Vec2{config::INDICATOR_MARGIN, config::INDICATOR_MARGIN};

        // Find intersection of ray from screen_center along `dir` with the screen rect.
        // We clamp to the closest edge.
        float tx = dir.x != 0.0f ? half_size.x / std::abs(dir.x) : 1e9f;
        float ty = dir.y != 0.0f ? half_size.y / std::abs(dir.y) : 1e9f;
        float t = std::min(tx, ty);

        Vec2 edge_pos = screen_center + dir * t;

        // Compute opacity: fade in as the group gets further from the viewport.
        // Starts fading at view_diag, full opacity at view_diag + fade_start.
        float fade_dist = dist - view_diag;
        float opacity = 0.0f;
        if (fade_dist > 0.0f) {
            opacity = std::min(fade_dist / config::INDICATOR_FADE_START, 1.0f);
            opacity = std::max(opacity, config::INDICATOR_MIN_OPACITY);
        }

        result.push_back(GroupIndicator{
            .group_id = gid,
            .name = group->name,
            .screen_edge_pos = edge_pos,
            .direction = dir,
            .opacity = opacity
        });
    }

    return result;
}

} // namespace chroma
