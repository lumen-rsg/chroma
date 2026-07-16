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
// Shadow helpers
// ============================================================================

void SceneRenderer::create_shadows(WindowSceneData& data, wlr_scene_tree* parent) {
    // Shadow alpha values: innermost (darkest) to outermost (faintest)
    static const float shadow_alphas[] = {0.18f, 0.12f, 0.07f, 0.03f};
    
    for (int i = 0; i < config::SHADOW_LAYERS; i++) {
        float color[4] = {0.0f, 0.0f, 0.0f, shadow_alphas[i]};
        data.shadow_rects[i] = wlr_scene_rect_create(parent, 0, 0, color);
        if (data.shadow_rects[i]) {
            // Shadows go behind everything, including the bg_rect
            wlr_scene_node_lower_to_bottom(&data.shadow_rects[i]->node);
        }
    }
}

void SceneRenderer::update_shadows(WindowSceneData& data) {
    // Each shadow layer is offset progressively further and grows larger,
    // creating a soft, directional drop-shadow effect.
    float alpha = data.visual_opacity;
    
    for (int i = 0; i < config::SHADOW_LAYERS; i++) {
        if (!data.shadow_rects[i]) continue;
        
        // Progressive offset and expansion per layer
        float off_x = config::SHADOW_OFFSET_X * (i + 1);
        float off_y = config::SHADOW_OFFSET_Y * (i + 1);
        float grow = config::SHADOW_GROW * (i + 1);
        
        int sw = static_cast<int>(data.visual_size.x + grow * 2);
        int sh = static_cast<int>(data.visual_size.y + grow * 2);
        
        if (sw < 0) sw = 0;
        if (sh < 0) sh = 0;
        
        wlr_scene_node_set_position(&data.shadow_rects[i]->node,
            static_cast<int>(off_x - grow),
            static_cast<int>(off_y - grow));
        
        wlr_scene_rect_set_size(data.shadow_rects[i], sw, sh);
        
        // Fade shadows with visual_opacity
        float base_alpha = (i == 0) ? 0.18f : (i == 1) ? 0.12f : (i == 2) ? 0.07f : 0.03f;
        float color[4] = {0.0f, 0.0f, 0.0f, base_alpha * alpha};
        wlr_scene_rect_set_color(data.shadow_rects[i], color);
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
    data.visual_opacity = 0.0f;  // start transparent for open animation

    // Create shadows first (they go behind everything)
    create_shadows(data, tree);

    // Create background rect with unfocused default color.
    // Created after shadows so it renders on top of them naturally
    // (wlr_scene_tree children render in creation order, last = topmost).
    float border_color[4] = {0.15f, 0.15f, 0.18f, 0.0f};
    data.bg_rect = wlr_scene_rect_create(tree, 0, 0, border_color);

    // Initialize visual state from the window's canvas position (if available)
    if (auto* win = canvas_->get(id)) {
        // We don't have screen_size here, so use a reasonable default
        // The real visual state will be set on first render_frame
        data.open_origin = win->center();
    }

    scene_data_[id] = data;
}

void SceneRenderer::on_window_mapped(WindowId id, wlr_surface* surface) {
    auto it = scene_data_.find(id);
    if (it == scene_data_.end()) return;

    auto& data = it->second;

    // Cancel any close animation
    data.close_anim.stop();

    // Remove previous surface node if any
    if (data.surface_node) {
        wlr_scene_node_destroy(data.surface_node);
        data.surface_node = nullptr;
    }

    if (surface) {
        auto* scene_surface = wlr_scene_surface_create(data.tree, surface);
        if (scene_surface && scene_surface->buffer) {
            data.surface_node = &scene_surface->buffer->node;
        }
    }

    // Start open animation
    data.open_anim.start(config::ANIM_OPEN_DURATION);
    data.visual_opacity = 0.0f;
    
    // Record the open origin (window center in screen space will be set on first frame)
    if (auto* win = canvas_->get(id)) {
        data.open_origin = win->center();
    }

    scene_data_[id] = data;
}

void SceneRenderer::on_window_unmapped(WindowId id) {
    auto it = scene_data_.find(id);
    if (it == scene_data_.end()) return;

    auto& data = it->second;

    // Cancel any open animation
    data.open_anim.stop();

    // Start close animation (only if not already closing)
    if (!data.close_anim.active) {
        data.close_anim.start(config::ANIM_CLOSE_DURATION);
    }
}

void SceneRenderer::on_window_removed(WindowId id) {
    auto it = scene_data_.find(id);
    if (it == scene_data_.end()) return;

    // Always destroy scene nodes immediately — the toplevel is gone.
    wlr_scene_node_destroy(&it->second.tree->node);
    scene_data_.erase(it);
}

// ============================================================================
// Animation ticking
// ============================================================================

void SceneRenderer::tick_animations(float dt) {
    for (auto& [id, data] : scene_data_) {
        // Tick open animation
        if (data.open_anim.active) {
            data.open_anim.tick(dt);
            if (!data.open_anim.active) {
                // Animation complete — snap to full opacity and let visual catch up
                data.visual_opacity = 1.0f;
            }
        }

        // Tick close animation
        if (data.close_anim.active) {
            data.close_anim.tick(dt);
        }
    }
}

bool SceneRenderer::has_active_animations() const {
    for (const auto& [id, data] : scene_data_) {
        if (data.is_animating()) return true;
        if (data.visual_converging) return true;
    }
    return false;
}

// ============================================================================
// Visual update for a single window
// ============================================================================

void SceneRenderer::update_window_visuals(WindowSceneData& data, const ChromaWindow& win,
                                          Vec2 screen_size, float offset_x, float offset_y) {
    Rect screen_rect = canvas_->canvas_to_screen(win.canvas_rect(), screen_size);
    Vec2 logical_pos{screen_rect.x() + offset_x, screen_rect.y() + offset_y};
    Vec2 logical_size{screen_rect.w(), screen_rect.h()};

    // -- Open animation: scale up from center --
    if (data.open_anim.active) {
        float t = data.open_anim.progress();
        float eased = ease_out_back(t);
        data.visual_opacity = eased;  // fade in borders + shadows
        
        // Scale from open_origin (screen-space center of the window)
        // Start at 20% size, expand to 100%
        float scale = 0.2f + 0.8f * eased;
        
        // Update open_origin to track the current logical center
        // (so the animation follows the window if it moves during open)
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
        data.visual_opacity = 1.0f - eased;  // fade out
        
        float scale = 1.0f - 0.85f * eased;  // shrink to 15%
        
        Vec2 logical_center = logical_pos + logical_size * 0.5f;
        Vec2 scaled_size = logical_size * scale;
        Vec2 scaled_pos = logical_center - scaled_size * 0.5f;
        
        data.visual_pos = scaled_pos;
        data.visual_size = scaled_size;
    }
    // -- Smooth move/resize: lerp visual toward logical --
    else {
        // Initialize visual state on first frame or after animation
        if (data.visual_pos.x < 0 || data.visual_size.x < 1.0f) {
            data.visual_pos = logical_pos;
            data.visual_size = logical_size;
            data.visual_opacity = 1.0f;
        }
        
        // Smooth position tracking with exponential decay
        float move_t = 1.0f - std::exp(-0.016f / config::ANIM_MOVE_DURATION);  // ~60fps step
        Vec2 pos_delta = logical_pos - data.visual_pos;
        if (pos_delta.length_squared() < config::ANIM_SNAP_THRESHOLD * config::ANIM_SNAP_THRESHOLD) {
            data.visual_pos = logical_pos;
        } else {
            data.visual_pos = chroma::lerp(data.visual_pos, logical_pos, move_t);
        }
        
        // Smooth size tracking
        float resize_t = 1.0f - std::exp(-0.016f / config::ANIM_RESIZE_DURATION);
        Vec2 size_delta = logical_size - data.visual_size;
        if (size_delta.length_squared() < config::ANIM_SNAP_THRESHOLD * config::ANIM_SNAP_THRESHOLD) {
            data.visual_size = logical_size;
        } else {
            data.visual_size = chroma::lerp(data.visual_size, logical_size, resize_t);
        }
        
        // Opacity converges to 1.0
        if (data.visual_opacity < 0.995f) {
            data.visual_opacity = chroma::lerp(data.visual_opacity, 1.0f, move_t);
        } else {
            data.visual_opacity = 1.0f;
        }

        // Track whether we still need animation frames for convergence
        data.visual_converging = 
            (data.visual_pos - logical_pos).length_squared() > 0.5f ||
            (data.visual_size - logical_size).length_squared() > 0.5f ||
            (data.visual_opacity < 0.998f && data.visual_opacity > 0.0f);
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

    // Calculate delta time since last frame
    float dt = 0.0f;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (last_frame_time_.tv_sec != 0 || last_frame_time_.tv_nsec != 0) {
        dt = static_cast<float>(now.tv_sec - last_frame_time_.tv_sec) +
             static_cast<float>(now.tv_nsec - last_frame_time_.tv_nsec) / 1e9f;
        if (dt < 0.0f) dt = 0.0f;   // clock skew guard
        if (dt > 0.1f) dt = 0.1f;   // cap to avoid huge jumps after pause
    }
    last_frame_time_ = now;

    // Tick all animations
    tick_animations(dt);

    bool any_animating = false;

    // Render all windows: mapped ones + unmapped ones with active close animation
    for (const auto& [id, win] : canvas_->all_windows()) {
        auto it = scene_data_.find(id);
        if (it == scene_data_.end()) continue;

        auto& data = it->second;

        // Skip unmapped windows unless they have an active close animation
        if (!win.mapped && !data.close_anim.active) continue;

        // Mark this window for animation-driven frame scheduling
        if (data.is_animating() || data.visual_converging) any_animating = true;

        // Calculate stack offset
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

        // Update visual state (handles open/close/smooth-move animations)
        update_window_visuals(data, win, screen_size, offset_x, offset_y);

        // --- Position the scene tree ---
        wlr_scene_node_set_position(&data.tree->node,
            static_cast<int>(data.visual_pos.x),
            static_cast<int>(data.visual_pos.y));

        // --- Update shadows ---
        update_shadows(data);

        // --- Update border/background rect ---
        constexpr int BORDER_W = 4;
        if (data.bg_rect) {
            float color[4];
            if (win.focused) {
                // Vibrant purple for focused windows
                color[0] = 0.30f; color[1] = 0.24f; color[2] = 0.50f;
            } else {
                // Subtle dark for unfocused windows
                color[0] = 0.15f; color[1] = 0.15f; color[2] = 0.18f;
            }
            color[3] = data.visual_opacity * (win.focused ? 1.0f : 0.85f);
            
            wlr_scene_rect_set_color(data.bg_rect, color);
            wlr_scene_rect_set_size(data.bg_rect,
                static_cast<int>(data.visual_size.x),
                static_cast<int>(data.visual_size.y));
        }

        // --- Position surface node (inset within border) ---
        if (data.surface_node && win.mapped) {
            wlr_scene_node_set_position(data.surface_node, BORDER_W, BORDER_W);
            auto* scene_buffer = wlr_scene_buffer_from_node(data.surface_node);
            if (scene_buffer) {
                int surf_w = static_cast<int>(data.visual_size.x) - BORDER_W * 2;
                int surf_h = static_cast<int>(data.visual_size.y) - BORDER_W * 2;
                if (surf_w < 1) surf_w = 1;
                if (surf_h < 1) surf_h = 1;
                wlr_scene_buffer_set_dest_size(scene_buffer, surf_w, surf_h);
            }
        }

        // Update dirty tracking (track last committed visual state)
        data.last_screen_pos = data.visual_pos;
        data.last_screen_size = data.visual_size;
        data.last_stack_offset = {offset_x, offset_y};
        data.last_visual_opacity = data.visual_opacity;
        data.was_focused = win.focused;
    }

    // Commit the scene to the output
    wlr_scene_output_commit(scene_output, nullptr);
    
    // Send frame_done to all surfaces
    wlr_scene_output_send_frame_done(scene_output, &now);
    
    // Schedule next frame if:
    //   (a) wlr_scene has pending damage, OR
    //   (b) any window has an active animation
    if (wlr_scene_output_needs_frame(scene_output) || any_animating || has_active_animations()) {
        wlr_output_schedule_frame(output);
    }
}

WindowSceneData* SceneRenderer::get_scene_data(WindowId id) {
    auto it = scene_data_.find(id);
    return it != scene_data_.end() ? &it->second : nullptr;
}

} // namespace chroma
