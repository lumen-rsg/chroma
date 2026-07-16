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

        // Gravitational pull: force diminishes with distance squared
        Vec2 other_center = other->center();
        Vec2 dir = mover_center - other_center;
        float dist = dir.length();
        if (dist < 1.0f) continue;

        float force = config.attraction_strength / (dist * dist) * 10000.0f;
        force = std::min(force, 50.0f); // cap the pull per frame

        Vec2 displacement = dir.normalized() * force;
        canvas.move_window(other_id, other->canvas_pos + displacement);
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
        if (w.stack != NO_STACK) continue; // skip stacked windows
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
        return NO_GROUP;
    }

    return gid;
}

} // namespace chroma
