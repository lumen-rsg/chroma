#include "renderer.hpp"
#include <cstdio>
#include <ctime>
#include <cmath>

namespace chroma {

SceneRenderer::SceneRenderer(WlrootsServer* server, Canvas* canvas,
                             XdgShellHandler* xdg_handler, StackManager* stacks)
    : server_{server}, canvas_{canvas}, xdg_handler_{xdg_handler}, stacks_{stacks}
{}

// ============================================================================
// Config access helpers
// ============================================================================

/// Get shadow layer count from config (or default).
static int cfg_shadow_layers(const ChromaConfig* cfg) {
    if (cfg) return cfg->theme.shadow_layers;
    return 4;
}

/// Get shadow alphas from config (or default).
static const float* cfg_shadow_alphas(const ChromaConfig* cfg) {
    static const float defaults[] = {0.18f, 0.12f, 0.07f, 0.03f};
    if (cfg) return cfg->theme.shadow_alphas;
    return defaults;
}

/// Get shadow offset x from config.
static float cfg_shadow_off_x(const ChromaConfig* cfg) {
    if (cfg) return cfg->theme.shadow_offset_x;
    return 2.0f;
}

/// Get shadow offset y from config.
static float cfg_shadow_off_y(const ChromaConfig* cfg) {
    if (cfg) return cfg->theme.shadow_offset_y;
    return 2.0f;
}

/// Get shadow grow from config.
static float cfg_shadow_grow(const ChromaConfig* cfg) {
    if (cfg) return cfg->theme.shadow_grow;
    return 4.0f;
}

/// Get border width from config.
static int cfg_border_width(const ChromaConfig* cfg) {
    if (cfg) return cfg->theme.border_width;
    return 1;
}

/// Get animation duration from config (or default).
static float cfg_anim(const ChromaConfig* cfg, float AnimationConfig::* field, float def) {
    if (cfg) return cfg->animations.*field;
    return def;
}

/// Get input config value.
static float cfg_input(const ChromaConfig* cfg, float InputConfig::* field, float def) {
    if (cfg) return cfg->input.*field;
    return def;
}

// ============================================================================
// Shadow helpers
// ============================================================================

void SceneRenderer::create_shadows(WindowSceneData& data, wlr_scene_tree* parent) {
    int layers = cfg_shadow_layers(config_);
    const float* alphas = cfg_shadow_alphas(config_);

    for (int i = 0; i < layers && i < 4; i++) {
        float color[4] = {0.0f, 0.0f, 0.0f, alphas[i]};
        data.shadow_rects[i] = wlr_scene_rect_create(parent, 0, 0, color);
        if (data.shadow_rects[i]) {
            wlr_scene_node_lower_to_bottom(&data.shadow_rects[i]->node);
        }
    }
    // Clear any unused shadow rect slots
    for (int i = layers; i < 4; i++) {
        data.shadow_rects[i] = nullptr;
    }
}

void SceneRenderer::create_borders(WindowSceneData& data, wlr_scene_tree* parent) {
    // Default unfocused color
    float color[4];
    if (config_) {
        color[0] = config_->theme.border_unfocused[0];
        color[1] = config_->theme.border_unfocused[1];
        color[2] = config_->theme.border_unfocused[2];
        color[3] = config_->theme.border_unfocused[3];
    } else {
        color[0] = 0.15f; color[1] = 0.15f; color[2] = 0.18f; color[3] = 1.0f;
    }

    data.border_top    = wlr_scene_rect_create(parent, 0, 0, color);
    data.border_bottom = wlr_scene_rect_create(parent, 0, 0, color);
    data.border_left   = wlr_scene_rect_create(parent, 0, 0, color);
    data.border_right  = wlr_scene_rect_create(parent, 0, 0, color);
}

void SceneRenderer::update_shadows(WindowSceneData& data) {
    bool animating = data.open_anim.active || data.close_anim.active;
    float alpha = animating ? data.visual_opacity : 0.0f;

    int layers = cfg_shadow_layers(config_);
    float off_x = cfg_shadow_off_x(config_);
    float off_y = cfg_shadow_off_y(config_);
    float grow = cfg_shadow_grow(config_);
    const float* alphas = cfg_shadow_alphas(config_);

    for (int i = 0; i < layers && i < 4; i++) {
        if (!data.shadow_rects[i]) continue;

        float loff_x = off_x * (i + 1);
        float loff_y = off_y * (i + 1);
        float lgrow = grow * (i + 1);

        int sw = static_cast<int>(data.visual_size.x + lgrow * 2);
        int sh = static_cast<int>(data.visual_size.y + lgrow * 2);

        if (sw < 0) sw = 0;
        if (sh < 0) sh = 0;

        wlr_scene_node_set_position(&data.shadow_rects[i]->node,
            static_cast<int>(loff_x - lgrow),
            static_cast<int>(loff_y - lgrow));

        wlr_scene_rect_set_size(data.shadow_rects[i], sw, sh);

        float base_alpha = (i < 4) ? alphas[i] : 0.03f;
        float color[4] = {0.0f, 0.0f, 0.0f, base_alpha * alpha};
        wlr_scene_rect_set_color(data.shadow_rects[i], color);
    }
}

void SceneRenderer::update_borders(WindowSceneData& data, bool focused) {
    int BW = cfg_border_width(config_);
    if (BW < 0) BW = 0;

    float color[4];
    if (config_) {
        if (focused) {
            color[0] = config_->theme.border_focused[0];
            color[1] = config_->theme.border_focused[1];
            color[2] = config_->theme.border_focused[2];
            color[3] = config_->theme.border_focused[3];
        } else {
            color[0] = config_->theme.border_unfocused[0];
            color[1] = config_->theme.border_unfocused[1];
            color[2] = config_->theme.border_unfocused[2];
            color[3] = config_->theme.border_unfocused[3];
        }
    } else {
        if (focused) {
            color[0] = 0.38f; color[1] = 0.30f; color[2] = 0.60f; color[3] = 0.85f;
        } else {
            color[0] = 0.18f; color[1] = 0.18f; color[2] = 0.22f; color[3] = 0.60f;
        }
    }

    int w = static_cast<int>(data.visual_size.x);
    int h = static_cast<int>(data.visual_size.y);
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    if (data.border_top) {
        wlr_scene_node_set_position(&data.border_top->node, 0, -BW);
        wlr_scene_rect_set_size(data.border_top, w, BW);
        wlr_scene_rect_set_color(data.border_top, color);
    }
    if (data.border_bottom) {
        wlr_scene_node_set_position(&data.border_bottom->node, 0, h);
        wlr_scene_rect_set_size(data.border_bottom, w, BW);
        wlr_scene_rect_set_color(data.border_bottom, color);
    }
    if (data.border_left) {
        wlr_scene_node_set_position(&data.border_left->node, -BW, 0);
        wlr_scene_rect_set_size(data.border_left, BW, h);
        wlr_scene_rect_set_color(data.border_left, color);
    }
    if (data.border_right) {
        wlr_scene_node_set_position(&data.border_right->node, w, 0);
        wlr_scene_rect_set_size(data.border_right, BW, h);
        wlr_scene_rect_set_color(data.border_right, color);
    }
}

// ============================================================================
// Window lifecycle
// ============================================================================

void SceneRenderer::on_window_added(WindowId id, wlr_surface* /*surface*/) {
    auto* tree = wlr_scene_tree_create(&server_->scene->tree);
    if (!tree) {
        std::fprintf(stderr, "Failed to create scene tree for window %lu\n", id);
        return;
    }

    WindowSceneData data;
    data.window_id = id;
    data.tree = tree;
    data.surface_node = nullptr;
    data.visual_opacity = 0.0f;

    create_shadows(data, tree);
    create_borders(data, tree);

    // Background rect
    float border_color[4] = {0.15f, 0.15f, 0.18f, 0.0f};
    data.bg_rect = wlr_scene_rect_create(tree, 0, 0, border_color);

    if (auto* win = canvas_->get(id)) {
        data.open_origin = win->center();
    }

    scene_data_[id] = data;
}

void SceneRenderer::on_window_mapped(WindowId id, wlr_surface* /*surface*/) {
    auto it = scene_data_.find(id);
    if (it == scene_data_.end()) return;

    auto& data = it->second;

    data.close_anim.stop();

    if (data.surface_node) {
        wlr_scene_node_destroy(data.surface_node);
        data.surface_node = nullptr;
    }

    // Use wlr_scene_xdg_surface_create instead of wlr_scene_surface_create.
    // The latter explicitly ignores sub-surfaces, which breaks Firefox and
    // GNOME apps (nautilus, etc.) that rely on sub-surfaces for their content.
    // wlr_scene_xdg_surface_create also handles window-geometry offset
    // automatically, so we no longer need manual source_box / dest_size.
    if (auto* toplevel = xdg_handler_->toplevel_for(id)) {
        auto* scene_tree = wlr_scene_xdg_surface_create(data.tree, toplevel->base);
        if (scene_tree) {
            data.surface_node = &scene_tree->node;
        }
    }

    // Start open animation (use config duration)
    float dur = cfg_anim(config_, &AnimationConfig::open_duration, 0.25f);
    data.open_anim.start(dur);
    data.visual_opacity = 0.0f;

    if (auto* win = canvas_->get(id)) {
        data.open_origin = win->center();
    }

    scene_data_[id] = data;
}

void SceneRenderer::on_window_unmapped(WindowId id) {
    auto it = scene_data_.find(id);
    if (it == scene_data_.end()) return;

    auto& data = it->second;

    data.open_anim.stop();

    if (!data.close_anim.active) {
        float dur = cfg_anim(config_, &AnimationConfig::close_duration, 0.18f);
        data.close_anim.start(dur);
    }
}

void SceneRenderer::on_window_removed(WindowId id) {
    auto it = scene_data_.find(id);
    if (it == scene_data_.end()) return;

    wlr_scene_node_destroy(&it->second.tree->node);
    scene_data_.erase(it);
}

void SceneRenderer::trigger_jiggle(WindowId id, Vec2 impact_direction) {
    auto it = scene_data_.find(id);
    if (it == scene_data_.end()) return;
    float mag = impact_direction.length();
    if (mag < 0.01f) return;
    Vec2 impulse = impact_direction.normalized() * std::min(mag * canvas_->zoom, 60.0f);
    it->second.jiggle.start(impulse);
}

// ============================================================================
// Animation ticking
// ============================================================================

void SceneRenderer::tick_animations(float dt) {
    for (auto& [id, data] : scene_data_) {
        if (data.open_anim.active) {
            data.open_anim.tick(dt);
            if (!data.open_anim.active) {
                data.visual_opacity = 1.0f;
            }
        }

        if (data.close_anim.active) {
            data.close_anim.tick(dt);
        }
    }
}

bool SceneRenderer::has_active_animations() const {
    for (const auto& [id, data] : scene_data_) {
        if (data.is_animating()) return true;
        if (data.visual_converging) return true;
        if (data.jiggle.active()) return true;
    }
    return false;
}

void SceneRenderer::sync_scene_z_order() {
    const auto& z_order = canvas_->z_order();
    for (WindowId id : z_order) {
        auto it = scene_data_.find(id);
        if (it != scene_data_.end()) {
            wlr_scene_node_raise_to_top(&it->second.tree->node);
        }
    }

    // Re-raise overlay trees above all windows so panels, launchers
    // (wofi, waybar, etc.) always render on top.
    for (auto* tree : overlay_trees_) {
        if (tree) {
            wlr_scene_node_raise_to_top(&tree->node);
        }
    }
}

void SceneRenderer::register_overlay_tree(wlr_scene_tree* tree) {
    if (!tree) return;
    // Avoid duplicates
    for (auto* existing : overlay_trees_) {
        if (existing == tree) return;
    }
    overlay_trees_.push_back(tree);
}

// ============================================================================
// Visual update for a single window
// ============================================================================

void SceneRenderer::update_window_visuals(WindowSceneData& data, const ChromaWindow& win,
                                          Vec2 screen_size, float offset_x, float offset_y,
                                          float dt) {
    data.jiggle.tick(dt);

    Rect screen_rect = canvas_->canvas_to_screen(win.canvas_rect(), screen_size);
    Vec2 logical_pos{screen_rect.x() + offset_x, screen_rect.y() + offset_y};
    Vec2 logical_size{screen_rect.w(), screen_rect.h()};

    float move_dur = cfg_anim(config_, &AnimationConfig::move_duration, 0.15f);
    float resize_dur = cfg_anim(config_, &AnimationConfig::resize_duration, 0.20f);
    float snap_thresh = cfg_anim(config_, &AnimationConfig::snap_threshold, 5.0f);

    // -- Open animation: scale up from center --
    if (data.open_anim.active) {
        float t = data.open_anim.progress();
        float eased = ease_out_back(t);
        data.visual_opacity = eased;

        float scale = 0.2f + 0.8f * eased;

        Vec2 logical_center = logical_pos + logical_size * 0.5f;
        data.open_origin = logical_center;

        Vec2 scaled_size = logical_size * scale;
        Vec2 scaled_pos = data.open_origin - scaled_size * 0.5f;

        data.visual_pos = scaled_pos;
        data.visual_size = scaled_size;
    }
    // -- Close animation: scale down to center --
    else if (data.close_anim.active) {
        float t = data.close_anim.progress();
        float eased = ease_in_out_cubic(t);
        data.visual_opacity = 1.0f - eased;

        float scale = 1.0f - 0.85f * eased;

        Vec2 logical_center = logical_pos + logical_size * 0.5f;
        Vec2 scaled_size = logical_size * scale;
        Vec2 scaled_pos = logical_center - scaled_size * 0.5f;

        data.visual_pos = scaled_pos;
        data.visual_size = scaled_size;
    }
    // -- Smooth move/resize: lerp visual toward logical --
    else {
        if (data.visual_pos.x < 0 || data.visual_size.x < 1.0f) {
            data.visual_pos = logical_pos;
            data.visual_size = logical_size;
            data.visual_opacity = 1.0f;
        }

        float move_t = 1.0f - std::exp(-0.016f / move_dur);
        Vec2 pos_delta = logical_pos - data.visual_pos;
        if (pos_delta.length_squared() < snap_thresh * snap_thresh) {
            data.visual_pos = logical_pos;
        } else {
            data.visual_pos = chroma::lerp(data.visual_pos, logical_pos, move_t);
        }

        float resize_t = 1.0f - std::exp(-0.016f / resize_dur);
        Vec2 size_delta = logical_size - data.visual_size;
        if (size_delta.length_squared() < snap_thresh * snap_thresh) {
            data.visual_size = logical_size;
        } else {
            data.visual_size = chroma::lerp(data.visual_size, logical_size, resize_t);
        }

        if (data.visual_opacity < 0.995f) {
            data.visual_opacity = chroma::lerp(data.visual_opacity, 1.0f, move_t);
        } else {
            data.visual_opacity = 1.0f;
        }

        data.visual_converging =
            (data.visual_pos - logical_pos).length_squared() > 0.5f ||
            (data.visual_size - logical_size).length_squared() > 0.5f ||
            (data.visual_opacity < 0.998f && data.visual_opacity > 0.0f);
    }

    if (data.jiggle.active()) {
        data.visual_pos = data.visual_pos + data.jiggle.offset;
    }
}

// ============================================================================
// Frame rendering
// ============================================================================

void SceneRenderer::render_frame(wlr_scene_output* scene_output, wlr_output* output) {
    Vec2 screen_size = {
        static_cast<float>(output->width),
        static_cast<float>(output->height)
    };

    // Delta time
    float dt = 0.0f;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (last_frame_time_.tv_sec != 0 || last_frame_time_.tv_nsec != 0) {
        dt = static_cast<float>(now.tv_sec - last_frame_time_.tv_sec) +
             static_cast<float>(now.tv_nsec - last_frame_time_.tv_nsec) / 1e9f;
        if (dt < 0.0f) dt = 0.0f;
        if (dt > 0.1f) dt = 0.1f;
    }
    last_frame_time_ = now;

    tick_animations(dt);
    canvas_->tick_viewport_animation(dt);

    bool any_animating = false;

    for (const auto& [id, win] : canvas_->all_windows()) {
        auto it = scene_data_.find(id);
        if (it == scene_data_.end()) continue;

        auto& data = it->second;

        if (!win.mapped && !data.close_anim.active) continue;

        if (data.is_animating() || data.visual_converging) any_animating = true;

        // Stack offset
        float offset_x = 0.0f;
        float offset_y = 0.0f;
        if (win.stack != NO_STACK) {
            auto* stack = stacks_->get(win.stack);
            if (stack) {
                for (size_t i = 0; i < stack->windows.size(); i++) {
                    if (stack->windows[i] == id) {
                        Vec2 off = stack->offset_for(i);
                        offset_x = off.x;
                        offset_y = off.y;
                        break;
                    }
                }
            }
        }

        update_window_visuals(data, win, screen_size, offset_x, offset_y, dt);

        wlr_scene_node_set_position(&data.tree->node,
            static_cast<int>(data.visual_pos.x),
            static_cast<int>(data.visual_pos.y));

        update_shadows(data);
        update_borders(data, win.focused);

        // Background rect
        if (data.bg_rect) {
            float color[4];
            if (config_) {
                if (win.focused) {
                    color[0] = config_->theme.border_focused[0] * 0.8f;
                    color[1] = config_->theme.border_focused[1] * 0.8f;
                    color[2] = config_->theme.border_focused[2] * 0.8f;
                } else {
                    color[0] = config_->theme.border_unfocused[0];
                    color[1] = config_->theme.border_unfocused[1];
                    color[2] = config_->theme.border_unfocused[2];
                }
            } else {
                if (win.focused) {
                    color[0] = 0.30f; color[1] = 0.24f; color[2] = 0.50f;
                } else {
                    color[0] = 0.15f; color[1] = 0.15f; color[2] = 0.18f;
                }
            }
            color[3] = (data.open_anim.active || data.close_anim.active)
                           ? data.visual_opacity : 0.0f;

            wlr_scene_rect_set_color(data.bg_rect, color);
            wlr_scene_rect_set_size(data.bg_rect,
                static_cast<int>(data.visual_size.x),
                static_cast<int>(data.visual_size.y));
        }

        // Surface node — wlr_scene_xdg_surface_create handles window geometry
        // offset and sub-surfaces internally, so we just position at origin
        // and apply an optional clip during open/close animations.
        if (data.surface_node && win.mapped) {
            wlr_scene_node_set_position(data.surface_node, 0, 0);

            if (data.open_anim.active || data.close_anim.active) {
                int clip_w = static_cast<int>(data.visual_size.x);
                int clip_h = static_cast<int>(data.visual_size.y);
                if (clip_w < 1) clip_w = 1;
                if (clip_h < 1) clip_h = 1;
                struct wlr_box clip_box = {0, 0, clip_w, clip_h};
                wlr_scene_subsurface_tree_set_clip(data.surface_node, &clip_box);
            } else {
                wlr_scene_subsurface_tree_set_clip(data.surface_node, nullptr);
            }
        }

        data.last_screen_pos = data.visual_pos;
        data.last_screen_size = data.visual_size;
        data.last_stack_offset = {offset_x, offset_y};
        data.last_visual_opacity = data.visual_opacity;
        data.was_focused = win.focused;
    }

    sync_scene_z_order();
    update_group_indicators(screen_size);

    wlr_scene_output_commit(scene_output, nullptr);
    wlr_scene_output_send_frame_done(scene_output, &now);

    if (wlr_scene_output_needs_frame(scene_output) || any_animating ||
        has_active_animations() || canvas_->viewport_animating()) {
        wlr_output_schedule_frame(output);
    }
}

WindowSceneData* SceneRenderer::get_scene_data(WindowId id) {
    auto it = scene_data_.find(id);
    return it != scene_data_.end() ? &it->second : nullptr;
}

// ============================================================================
// Group Indicator HUD
// ============================================================================

void SceneRenderer::group_color(const ChromaConfig* config, size_t order_index,
                                 float alpha, float out_color[4]) {
    const float* h;
    if (config) {
        h = config->theme.group_hues[order_index % 6];
    } else {
        static const float default_hues[6][3] = {
            {0.95f, 0.45f, 0.20f},
            {0.25f, 0.65f, 0.95f},
            {0.40f, 0.80f, 0.35f},
            {0.85f, 0.35f, 0.75f},
            {0.95f, 0.85f, 0.15f},
            {0.30f, 0.75f, 0.80f},
        };
        h = default_hues[order_index % 6];
    }
    out_color[0] = h[0];
    out_color[1] = h[1];
    out_color[2] = h[2];
    out_color[3] = std::clamp(alpha, 0.0f, 1.0f);
}

void SceneRenderer::ensure_indicator_nodes(size_t count) {
    wlr_scene_tree* root = &server_->scene->tree;

    while (indicator_nodes_.size() < count) {
        IndicatorNode node;
        node.group_id = NO_GROUP;
        float color[4] = {1.0f, 1.0f, 1.0f, 0.0f};
        for (int i = 0; i < 3; i++) {
            node.pieces[i] = wlr_scene_rect_create(root, 0, 0, color);
        }
        indicator_nodes_.push_back(node);
    }

    while (indicator_nodes_.size() > count) {
        auto& node = indicator_nodes_.back();
        for (int i = 0; i < 3; i++) {
            if (node.pieces[i]) {
                wlr_scene_node_destroy(&node.pieces[i]->node);
            }
        }
        indicator_nodes_.pop_back();
    }
}

void SceneRenderer::update_group_indicators(Vec2 screen_size) {
    auto indicators = canvas_->compute_indicators(screen_size);

    ensure_indicator_nodes(indicators.size());

    for (size_t i = 0; i < indicators.size(); i++) {
        const auto& ind = indicators[i];
        auto& node = indicator_nodes_[i];
        node.group_id = ind.group_id;

        const auto& order = canvas_->group_order();
        size_t order_idx = 0;
        for (size_t j = 0; j < order.size(); j++) {
            if (order[j] == ind.group_id) {
                order_idx = j;
                break;
            }
        }

        float base_color[4];
        group_color(config_, order_idx, ind.opacity, base_color);

        float s = cfg_input(config_, &InputConfig::zoom_step, 0.0f);
        s = 20.0f; // indicator size in screen px (could make configurable later)
        Vec2 pos = ind.screen_edge_pos;
        Vec2 dir = ind.direction;

        // Center dot
        {
            int cx = static_cast<int>(pos.x);
            int cy = static_cast<int>(pos.y);
            wlr_scene_node_set_position(&node.pieces[0]->node, cx - 3, cy - 3);
            wlr_scene_rect_set_size(node.pieces[0], 7, 7);
            wlr_scene_rect_set_color(node.pieces[0], base_color);
        }

        // Left wing
        {
            Vec2 wing_pos = pos + Vec2{-dir.y, dir.x} * s * 0.5f + dir * s * 0.4f;
            int wx = static_cast<int>(wing_pos.x);
            int wy = static_cast<int>(wing_pos.y);
            wlr_scene_node_set_position(&node.pieces[1]->node, wx - 2, wy - 2);
            wlr_scene_rect_set_size(node.pieces[1], 5, 5);
            float wing_color[4];
            group_color(config_, order_idx, ind.opacity * 0.75f, wing_color);
            wlr_scene_rect_set_color(node.pieces[1], wing_color);
        }

        // Right wing
        {
            Vec2 wing_pos = pos - Vec2{-dir.y, dir.x} * s * 0.5f + dir * s * 0.4f;
            int wx = static_cast<int>(wing_pos.x);
            int wy = static_cast<int>(wing_pos.y);
            wlr_scene_node_set_position(&node.pieces[2]->node, wx - 2, wy - 2);
            wlr_scene_rect_set_size(node.pieces[2], 5, 5);
            float wing_color[4];
            group_color(config_, order_idx, ind.opacity * 0.75f, wing_color);
            wlr_scene_rect_set_color(node.pieces[2], wing_color);
        }
    }
}

} // namespace chroma
