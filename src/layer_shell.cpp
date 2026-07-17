#include "layer_shell.hpp"
#include <cstdio>
#include <vector>

namespace chroma {

// ============================================================================
// LayerShellHandler
// ============================================================================

LayerShellHandler::LayerShellHandler(WlrootsServer* server)
    : server_{server}
{
    wl_list_init(&on_new_surface_.link);
}

LayerShellHandler::~LayerShellHandler() {
    // Clean up all per-surface listeners before the layer shell global
    // is destroyed (which would fire destroy events accessing freed memory).
    for (auto& [ls, data] : surfaces_) {
        wl_list_remove(&data.surface_map.link);
        wl_list_remove(&data.surface_unmap.link);
        wl_list_remove(&data.surface_commit.link);
        wl_list_remove(&data.surface_destroy.link);
    }
    surfaces_.clear();

    // Clean up per-output layer trees
    for (auto& [output, layers] : output_layers_) {
        if (layers.background) wlr_scene_node_destroy(&layers.background->node);
        if (layers.bottom)     wlr_scene_node_destroy(&layers.bottom->node);
        if (layers.top)        wlr_scene_node_destroy(&layers.top->node);
        if (layers.overlay)    wlr_scene_node_destroy(&layers.overlay->node);
    }
    output_layers_.clear();

    // Remove our listener from the layer shell
    if (server_->layer_shell) {
        wl_list_remove(&on_new_surface_.link);
    }
}

void LayerShellHandler::connect() {
    if (!server_->layer_shell) {
        std::fprintf(stderr, "[chroma] Layer shell not initialized\n");
        return;
    }
    on_new_surface_.notify = handle_new_surface;
    wl_signal_add(&server_->layer_shell->events.new_surface, &on_new_surface_);
    std::printf("[chroma] Layer shell handler connected\n");
}

// ============================================================================
// Output layer trees
// ============================================================================

void LayerShellHandler::on_output_created(wlr_output* output) {
    if (output_layers_.count(output)) return;  // already created

    OutputLayers layers;
    // Layer trees are parented directly under the root scene tree.
    // They render in z-order: background (bottom) → bottom → top → overlay (top).
    // Create in reverse z-order so each new node stacks on top, then
    // reorder to ensure correct layering.
    layers.background = wlr_scene_tree_create(&server_->scene->tree);
    layers.bottom     = wlr_scene_tree_create(&server_->scene->tree);
    layers.top        = wlr_scene_tree_create(&server_->scene->tree);
    layers.overlay    = wlr_scene_tree_create(&server_->scene->tree);

    // Ensure correct ordering: background at bottom, overlay at top
    wlr_scene_node_lower_to_bottom(&layers.background->node);
    wlr_scene_node_raise_to_top(&layers.bottom->node);
    wlr_scene_node_raise_to_top(&layers.top->node);
    wlr_scene_node_raise_to_top(&layers.overlay->node);

    output_layers_[output] = layers;
    std::printf("[chroma] Layer trees created for output '%s'\n", output->name);
}

void LayerShellHandler::on_output_removed(wlr_output* output) {
    auto it = output_layers_.find(output);
    if (it == output_layers_.end()) return;

    // Destroy all layer surfaces on this output first
    // (iterate a copy — surfaces_ changes during destroy callbacks)
    std::vector<wlr_layer_surface_v1*> to_remove;
    for (auto& [ls, data] : surfaces_) {
        if (data.output == output) {
            to_remove.push_back(ls);
        }
    }
    for (auto* ls : to_remove) {
        wlr_layer_surface_v1_destroy(ls);
    }
    // The destroy handlers will remove entries from surfaces_

    auto& layers = it->second;
    if (layers.background) wlr_scene_node_destroy(&layers.background->node);
    if (layers.bottom)     wlr_scene_node_destroy(&layers.bottom->node);
    if (layers.top)        wlr_scene_node_destroy(&layers.top->node);
    if (layers.overlay)    wlr_scene_node_destroy(&layers.overlay->node);
    output_layers_.erase(it);
}

wlr_scene_tree* LayerShellHandler::layer_tree_for(
    wlr_output* output, enum zwlr_layer_shell_v1_layer layer)
{
    auto it = output_layers_.find(output);
    if (it == output_layers_.end()) return nullptr;

    switch (layer) {
    case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND: return it->second.background;
    case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:     return it->second.bottom;
    case ZWLR_LAYER_SHELL_V1_LAYER_TOP:        return it->second.top;
    case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:    return it->second.overlay;
    default: return nullptr;
    }
}

// ============================================================================
// New surface handler
// ============================================================================

void LayerShellHandler::handle_new_surface(wl_listener* listener, void* data) {
    LayerShellHandler* handler = wl_container_of(listener, handler, on_new_surface_);
    auto* layer_surface = static_cast<wlr_layer_surface_v1*>(data);
    wlr_surface* wlr_surf = layer_surface->surface;

    // Assign an output if the client didn't specify one
    wlr_output* output = layer_surface->output;
    if (!output) {
        // Use the first available output
        if (!handler->server_->scene_outputs.empty()) {
            output = handler->server_->scene_outputs[0]->output;
        }
        if (!output) {
            std::fprintf(stderr, "[chroma] Layer surface has no output, destroying\n");
            wlr_layer_surface_v1_destroy(layer_surface);
            return;
        }
        layer_surface->output = output;
    }

    // Ensure layer trees exist for this output
    if (!handler->output_layers_.count(output)) {
        handler->on_output_created(output);
    }

    // Select the appropriate layer tree
    wlr_scene_tree* tree = handler->layer_tree_for(output,
        layer_surface->pending.layer);
    if (!tree) {
        std::fprintf(stderr, "[chroma] No layer tree for layer %d\n",
                     layer_surface->pending.layer);
        return;
    }

    // Create the scene helper
    auto* scene_layer = wlr_scene_layer_surface_v1_create(tree, layer_surface);
    if (!scene_layer) {
        std::fprintf(stderr, "[chroma] Failed to create scene layer surface\n");
        return;
    }

    // Set up per-surface tracking
    LayerSurfaceData sd;
    sd.handler = handler;
    sd.layer_surface = layer_surface;
    sd.scene_layer_surface = scene_layer;
    sd.output = output;
    sd.mapped = false;
    handler->surfaces_[layer_surface] = sd;

    // Pointer to the stored data (stable in the map)
    LayerSurfaceData* sdp = &handler->surfaces_[layer_surface];

    // --- Wire surface event listeners ---

    sdp->surface_map.notify = [](wl_listener* l, void* /*data*/) {
        LayerSurfaceData* sd = wl_container_of(l, sd, surface_map);
        sd->mapped = true;
        std::printf("[chroma] Layer surface mapped: namespace='%s' layer=%d\n",
                    sd->layer_surface->name_space ? sd->layer_surface->name_space : "(null)",
                    sd->layer_surface->current.layer);
        sd->handler->arrange_layers(sd->output);
    };
    wl_signal_add(&wlr_surf->events.map, &sdp->surface_map);

    sdp->surface_unmap.notify = [](wl_listener* l, void* /*data*/) {
        LayerSurfaceData* sd = wl_container_of(l, sd, surface_unmap);
        sd->mapped = false;
        std::printf("[chroma] Layer surface unmapped: namespace='%s'\n",
                    sd->layer_surface->name_space ? sd->layer_surface->name_space : "(null)");
        sd->handler->arrange_layers(sd->output);
    };
    wl_signal_add(&wlr_surf->events.unmap, &sdp->surface_unmap);

    sdp->surface_commit.notify = [](wl_listener* l, void* /*data*/) {
        LayerSurfaceData* sd = wl_container_of(l, sd, surface_commit);
        auto* ls = sd->layer_surface;

        if (ls->initial_commit) {
            // First commit after creation — arrange to set up initial position
            sd->handler->arrange_layers(sd->output);
        }

        // Check if the layer changed and reparent if needed
        if (ls->initialized && ls->current.layer != ls->pending.layer) {
            wlr_scene_tree* new_tree = sd->handler->layer_tree_for(
                sd->output, ls->pending.layer);
            if (new_tree && sd->scene_layer_surface) {
                wlr_scene_node_reparent(
                    &sd->scene_layer_surface->tree->node, new_tree);
                sd->handler->arrange_layers(sd->output);
            }
        }
    };
    wl_signal_add(&wlr_surf->events.commit, &sdp->surface_commit);

    sdp->surface_destroy.notify = [](wl_listener* l, void* /*data*/) {
        LayerSurfaceData* sd = wl_container_of(l, sd, surface_destroy);
        // The wlr_scene_layer_surface_v1 helper auto-cleans when the
        // layer_surface is destroyed. We just clean up our tracking data
        // and listeners.
        wl_list_remove(&sd->surface_map.link);
        wl_list_remove(&sd->surface_unmap.link);
        wl_list_remove(&sd->surface_commit.link);
        wl_list_remove(&sd->surface_destroy.link);

        wlr_output* output = sd->output;
        wlr_layer_surface_v1* ls = sd->layer_surface;

        // Remove from the surfaces map
        sd->handler->surfaces_.erase(ls);

        // Rearrange after removal
        sd->handler->arrange_layers(output);
    };
    wl_signal_add(&wlr_surf->events.destroy, &sdp->surface_destroy);

    std::printf("[chroma] New layer surface: namespace='%s' layer=%d output='%s'\n",
                layer_surface->name_space ? layer_surface->name_space : "(null)",
                layer_surface->pending.layer, output->name);
}

// ============================================================================
// Layer arrangement
// ============================================================================

void LayerShellHandler::arrange_layers(wlr_output* output) {
    auto it = output_layers_.find(output);
    if (it == output_layers_.end()) return;

    wlr_box full_area = {
        .x = 0,
        .y = 0,
        .width = static_cast<int>(output->width),
        .height = static_cast<int>(output->height),
    };
    wlr_box usable_area = full_area;

    // Process layers: overlay → top → bottom → background.
    // Two passes per layer: exclusive zone surfaces first, then non-exclusive.
    // wlr_scene_layer_surface_v1_configure handles the exclusive zone
    // reduction of usable_area automatically.

    static constexpr enum zwlr_layer_shell_v1_layer layer_order[] = {
        ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
        ZWLR_LAYER_SHELL_V1_LAYER_TOP,
        ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM,
        ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND,
    };

    for (auto layer : layer_order) {
        arrange_layer(output, it->second, layer, &full_area, &usable_area, true);   // exclusive
        arrange_layer(output, it->second, layer, &full_area, &usable_area, false);  // non-exclusive
    }

    // Trigger a frame so the scene updates are rendered
    server_->schedule_all_frames();

    // If any exclusive layer surface wants keyboard focus, give it focus now.
    update_keyboard_focus(output);
}

void LayerShellHandler::arrange_layer(
    wlr_output* output,
    OutputLayers& /*layers*/,
    enum zwlr_layer_shell_v1_layer layer,
    wlr_box* full_area,
    wlr_box* usable_area,
    bool exclusive_only)
{
    for (auto& [ls, data] : surfaces_) {
        if (data.output != output) continue;
        if (!data.scene_layer_surface) continue;

        // Check layer matches
        if (ls->current.layer != layer) continue;

        // Check exclusive zone filter
        bool has_exclusive = ls->current.exclusive_zone > 0;
        if (has_exclusive != exclusive_only) continue;

        // Skip unmapped surfaces (wlroots scene helper handles initial_commit)
        if (!data.mapped && !ls->initial_commit) continue;

        wlr_scene_layer_surface_v1_configure(
            data.scene_layer_surface, full_area, usable_area);
    }
}

// ============================================================================
// Keyboard focus for exclusive layer surfaces
// ============================================================================

bool LayerShellHandler::update_keyboard_focus(wlr_output* output) {
    // Scan overlay, then top layers for EXCLUSIVE keyboard interactivity.
    // The topmost (last created in the tree) EXCLUSIVE surface gets focus.

    static constexpr enum zwlr_layer_shell_v1_layer scan_layers[] = {
        ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
        ZWLR_LAYER_SHELL_V1_LAYER_TOP,
    };

    for (auto layer : scan_layers) {
        wlr_layer_surface_v1* best = nullptr;
        int best_z = -1;

        for (auto& [ls, data] : surfaces_) {
            if (data.output != output) continue;
            if (!data.mapped) continue;
            if (ls->current.layer != layer) continue;
            if (ls->current.keyboard_interactive !=
                ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE)
                continue;
            if (!data.scene_layer_surface) continue;

            // Find the z-index: count how many siblings come before this node
            auto* node = &data.scene_layer_surface->tree->node;
            int z = 0;
            wlr_scene_node* sibling;
            wl_list_for_each(sibling, &node->parent->children, link) {
                if (sibling == node) break;
                z++;
            }
            if (z > best_z) {
                best_z = z;
                best = ls;
            }
        }

        if (best) {
            wlr_seat_keyboard_notify_enter(server_->seat, best->surface,
                nullptr, 0, nullptr);
            std::printf("[chroma] KB focus to exclusive layer surface: '%s'\n",
                        best->name_space ? best->name_space : "(null)");
            return true;
        }
    }

    return false;
}

bool LayerShellHandler::has_exclusive_focus(wlr_output* output) const {
    static constexpr enum zwlr_layer_shell_v1_layer scan_layers[] = {
        ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
        ZWLR_LAYER_SHELL_V1_LAYER_TOP,
    };

    for (auto layer : scan_layers) {
        for (const auto& [ls, data] : surfaces_) {
            if (data.output != output) continue;
            if (!data.mapped) continue;
            if (ls->current.layer != layer) continue;
            if (ls->current.keyboard_interactive ==
                ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE) {
                return true;
            }
        }
    }
    return false;
}

wlr_surface* LayerShellHandler::exclusive_surface_for(wlr_output* output) const {
    static constexpr enum zwlr_layer_shell_v1_layer scan_layers[] = {
        ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
        ZWLR_LAYER_SHELL_V1_LAYER_TOP,
    };

    wlr_layer_surface_v1* best = nullptr;
    int best_z = -1;

    for (auto layer : scan_layers) {
        for (const auto& [ls, data] : surfaces_) {
            if (data.output != output) continue;
            if (!data.mapped) continue;
            if (ls->current.layer != layer) continue;
            if (ls->current.keyboard_interactive !=
                ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE)
                continue;
            if (!data.scene_layer_surface) continue;

            auto* node = &data.scene_layer_surface->tree->node;
            int z = 0;
            wlr_scene_node* sibling;
            wl_list_for_each(sibling, &node->parent->children, link) {
                if (sibling == node) break;
                z++;
            }
            if (z > best_z) {
                best_z = z;
                best = ls;
            }
        }
        if (best) break; // overlay takes priority over top
    }

    return best ? best->surface : nullptr;
}

Vec2 LayerShellHandler::exclusive_surface_position(wlr_output* output) const {
    static constexpr enum zwlr_layer_shell_v1_layer scan_layers[] = {
        ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
        ZWLR_LAYER_SHELL_V1_LAYER_TOP,
    };

    for (auto layer : scan_layers) {
        wlr_layer_surface_v1* best = nullptr;
        int best_z = -1;
        const LayerSurfaceData* best_data = nullptr;

        for (const auto& [ls, data] : surfaces_) {
            if (data.output != output) continue;
            if (!data.mapped) continue;
            if (ls->current.layer != layer) continue;
            if (ls->current.keyboard_interactive !=
                ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE)
                continue;
            if (!data.scene_layer_surface) continue;

            auto* node = &data.scene_layer_surface->tree->node;
            int z = 0;
            wlr_scene_node* sibling;
            wl_list_for_each(sibling, &node->parent->children, link) {
                if (sibling == node) break;
                z++;
            }
            if (z > best_z) {
                best_z = z;
                best = ls;
                best_data = &data;
            }
        }

        if (best && best_data && best_data->scene_layer_surface) {
            auto* node = &best_data->scene_layer_surface->tree->node;
            return Vec2{static_cast<float>(node->x), static_cast<float>(node->y)};
        }
    }

    return Vec2{0, 0};
}

std::vector<wlr_scene_tree*> LayerShellHandler::overlay_and_top_trees() const {
    std::vector<wlr_scene_tree*> trees;
    for (const auto& [output, layers] : output_layers_) {
        if (layers.overlay) trees.push_back(layers.overlay);
        if (layers.top)    trees.push_back(layers.top);
    }
    return trees;
}

} // namespace chroma
