#include "renderer.hpp"
#include <cstdio>
#include <ctime>

namespace chroma {

SceneRenderer::SceneRenderer(WlrootsServer* server, Canvas* canvas,
                             XdgShellHandler* xdg_handler, StackManager* stacks)
    : server_{server}, canvas_{canvas}, xdg_handler_{xdg_handler}, stacks_{stacks}
{}

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

    // Create border background rect (slightly larger than window, colorful)
    float border_color[4] = {0.2f, 0.2f, 0.22f, 1.0f};  // unfocused default
    data.bg_rect = wlr_scene_rect_create(tree, 0, 0, border_color);
    if (data.bg_rect) {
        wlr_scene_node_lower_to_bottom(&data.bg_rect->node);
    }

    scene_data_[id] = data;
}

void SceneRenderer::on_window_removed(WindowId id) {
    auto it = scene_data_.find(id);
    if (it == scene_data_.end()) return;

    wlr_scene_node_destroy(&it->second.tree->node);
    scene_data_.erase(it);
}

void SceneRenderer::on_window_mapped(WindowId id, wlr_surface* surface) {
    auto it = scene_data_.find(id);
    if (it == scene_data_.end()) return;

    auto& data = it->second;

    // Remove previous surface node if any
    if (data.surface_node) {
        wlr_scene_node_destroy(data.surface_node);
        data.surface_node = nullptr;
    }

    if (surface) {
        // Create a scene buffer for the surface
        auto* scene_surface = wlr_scene_surface_create(data.tree, surface);
        if (scene_surface) {
            data.surface_node = &scene_surface->buffer->node;
        }
    }

    scene_data_[id] = data;
}

void SceneRenderer::render_frame(wlr_scene_output* scene_output, wlr_output* output) {
    Vec2 screen_size = {
        static_cast<float>(output->width),
        static_cast<float>(output->height)
    };

    // Update positions for all mapped windows
    for (const auto& [id, win] : canvas_->all_windows()) {
        if (!win.mapped) continue;

        auto it = scene_data_.find(id);
        if (it == scene_data_.end()) continue;

        auto& data = it->second;
        Rect screen_rect = canvas_->canvas_to_screen(win.canvas_rect(), screen_size);

        static int rd = 0;
        if (rd < 5 && (screen_rect.w() != data.last_size.x || screen_rect.h() != data.last_size.y)) {
            std::fprintf(stderr, "[chroma] Render resize: canvas=(%.0f,%.0f %.0fx%.0f) screen=(%.0f,%.0f %.0fx%.0f)\n",
                win.canvas_pos.x, win.canvas_pos.y, win.size.x, win.size.y,
                screen_rect.x(), screen_rect.y(), screen_rect.w(), screen_rect.h());
            rd++;
        }

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

        wlr_scene_node_set_position(&data.tree->node,
            static_cast<int>(screen_rect.x() + offset_x),
            static_cast<int>(screen_rect.y() + offset_y));

        constexpr int BORDER_W = 4;  // border thickness in screen pixels

        // Update border only when focus or size changes (avoids flicker)
        if (data.bg_rect && (win.focused != data.was_focused || 
            screen_rect.size.x != data.last_size.x || 
            screen_rect.size.y != data.last_size.y)) {
            data.was_focused = win.focused;
            data.last_size = {screen_rect.size.x, screen_rect.size.y};
            
            float color[4];
            if (win.focused) {
                color[0] = 0.35f; color[1] = 0.28f; color[2] = 0.55f; color[3] = 1.0f;
            } else {
                color[0] = 0.18f; color[1] = 0.18f; color[2] = 0.20f; color[3] = 1.0f;
            }
            wlr_scene_rect_set_color(data.bg_rect, color);
            wlr_scene_rect_set_size(data.bg_rect,
                static_cast<int>(screen_rect.w()),
                static_cast<int>(screen_rect.h()));
        }

        if (data.surface_node) {
            // Surface is inset within the border
            wlr_scene_node_set_position(data.surface_node, BORDER_W, BORDER_W);
            auto* scene_buffer = wlr_scene_buffer_from_node(data.surface_node);
            if (scene_buffer) {
                wlr_scene_buffer_set_dest_size(scene_buffer,
                    static_cast<int>(screen_rect.w()) - BORDER_W * 2,
                    static_cast<int>(screen_rect.h()) - BORDER_W * 2);
            }
        }
    }

    wlr_scene_output_commit(scene_output, nullptr);
    
    // Explicitly send frame_done to all surfaces — ensures clients
    // receive frame callbacks even if the scene commit didn't trigger them.
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(scene_output, &now);
    
    wlr_output_schedule_frame(output);
}

WindowSceneData* SceneRenderer::get_scene_data(WindowId id) {
    auto it = scene_data_.find(id);
    return it != scene_data_.end() ? &it->second : nullptr;
}

} // namespace chroma
