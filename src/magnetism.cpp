#include "magnetism.hpp"
#include <cmath>

namespace chroma {

void MagnetismEngine::snap_to_nearby(Canvas& canvas, WindowId window_id) {
    auto* w = canvas.get(window_id);
    if (!w) return;

    // Find the nearest group centroid
    GroupId nearest_group = NO_GROUP;
    float nearest_dist = config.snap_distance;

    for (const auto& [gid, group] : canvas.all_groups()) {
        if (group.empty()) continue;
        if (w->group == gid) continue; // already in this group

        Vec2 centroid = group.centroid(canvas);
        float dist = distance(w->center(), centroid);

        if (dist < nearest_dist) {
            nearest_dist = dist;
            nearest_group = gid;
        }
    }

    if (nearest_group != NO_GROUP) {
        // Snap to the group
        canvas.add_to_group(window_id, nearest_group);

        // Also snap position to align with the group's centroid
        Vec2 centroid = canvas.get_group(nearest_group)->centroid(canvas);
        // Position the window so its center aligns with the group centroid
        Vec2 new_pos = centroid - w->size * 0.5f;
        // Grid-align the position
        canvas.move_window(window_id, snap_to_grid(new_pos));

        // Resolve any overlaps caused by the snap
        Rect snap_rect = w->canvas_rect();
        for (const auto& [oid, ow] : canvas.all_windows()) {
            if (oid == window_id) continue;
            if (!ow.mapped) continue;
            if (ow.stack != NO_STACK && w->stack == ow.stack) continue;
            if (snap_rect.overlaps(ow.canvas_rect())) {
                Vec2 push = snap_rect.separation_vector(ow.canvas_rect());
                canvas.move_window(window_id, w->canvas_pos + push);
                snap_rect.pos = snap_rect.pos + push;
            }
        }

        return;
    }

    // No group nearby — apply grid snap to current position
    if (w->group == NO_GROUP) {
        canvas.move_window(window_id, snap_to_grid(w->canvas_pos));
    }
}

void MagnetismEngine::attract_group(Canvas& canvas, WindowId moved_id) {
    auto* w = canvas.get(moved_id);
    if (!w || w->group == NO_GROUP) return;

    auto* group = canvas.get_group(w->group);
    if (!group || group->size() < 2) return;

    Vec2 mover_center = w->center();

    for (auto other_id : group->windows) {
        if (other_id == moved_id) continue;
        auto* other = canvas.get(other_id);
        if (!other) continue;

        // Gravitational attraction between group members.
        //
        // Force diminishes with the square of the distance so that nearby
        // windows pull harder than distant ones.  The attraction_scale
        // constant maps the 0–1 config value into a usable pixel range.
        //
        // Example: at 100 px distance with strength=0.1 and scale=10000,
        //          force = 0.1 * 10000 / (100*100) = 0.1 px/call.
        Vec2 other_center = other->center();
        Vec2 dir = mover_center - other_center;
        float dist = dir.length();
        if (dist < 1.0f) continue;

        float force = config.attraction_strength * config.attraction_scale
                      / (dist * dist);
        force = std::min(force, config.max_attraction_force);

        Vec2 displacement = dir.normalized() * force;
        Vec2 new_pos = other->canvas_pos + displacement;
        canvas.move_window(other_id, new_pos);

        // Resolve overlaps with non-group windows (magnetism should not cause
        // windows from different groups to overlap — that's what stacks are for).
        Rect moved_rect = other->canvas_rect();  // re-read after move
        for (const auto& [oid, ow] : canvas.all_windows()) {
            if (oid == other_id) continue;
            if (oid == moved_id) continue;  // the dragged window, overlap is fine
            if (!ow.mapped) continue;
            if (ow.group == w->group) continue;  // same group: attraction pulls together
            if (ow.stack != NO_STACK && other->stack == ow.stack) continue;
            if (moved_rect.overlaps(ow.canvas_rect())) {
                Vec2 push = moved_rect.separation_vector(ow.canvas_rect());
                canvas.move_window(other_id, other->canvas_pos + push);
                moved_rect.pos = moved_rect.pos + push;
            }
        }
    }
}

GroupId MagnetismEngine::group_nearby(Canvas& canvas, WindowId center_id, float radius) {
    auto* center_w = canvas.get(center_id);
    if (!center_w) return NO_GROUP;

    Vec2 center_pos = center_w->center();
    float r2 = radius * radius;

    GroupId prev_group = center_w->group;  // save in case cluster is a singleton

    GroupId gid = canvas.create_group("cluster");

    // Add the center window
    canvas.add_to_group(center_id, gid);

    // Add nearby windows
    for (const auto& [id, w] : canvas.all_windows()) {
        if (id == center_id) continue;
        if (!w.mapped) continue;            // skip unmapped (not yet visible)
        if (w.stack != NO_STACK) continue;   // skip stacked windows
        if (distance_squared(center_pos, w.center()) <= r2) {
            canvas.add_to_group(id, gid);
        }
    }

    // If no neighbors were found, remove the orphan group
    auto* group = canvas.get_group(gid);
    if (group && group->size() <= 1) {
        canvas.destroy_group(gid);
        // Restore previous group membership
        if (prev_group != NO_GROUP) {
            canvas.add_to_group(center_id, prev_group);
        }
        std::fprintf(stderr, "[chroma] Group: no windows within %.0f px of window %lu\n",
                     radius, center_id);
        return NO_GROUP;
    }

    std::fprintf(stderr, "[chroma] Group %lu created ('%s'): %zu windows within %.0f px\n",
                 gid, group ? group->name.c_str() : "?", group ? group->size() : 0, radius);
    return gid;
}

} // namespace chroma
