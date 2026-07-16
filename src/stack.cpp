#include "stack.hpp"
#include <stdexcept>

namespace chroma {

StackId StackManager::create(Vec2 pos) {
    StackId id = next_sid_++;
    CardStack stack;
    stack.id = id;
    stack.position = pos;
    stacks_[id] = std::move(stack);
    return id;
}

void StackManager::destroy(Canvas& canvas, StackId id) {
    auto* s = get(id);
    if (!s) return;
    // Unstack all windows first
    auto win_ids = s->windows; // copy
    for (auto wid : win_ids) {
        unstack(canvas, wid);
    }
    stacks_.erase(id);
}

void StackManager::stack(Canvas& canvas, WindowId window_id, StackId stack_id) {
    auto* w = canvas.get(window_id);
    auto* s = get(stack_id);
    if (!w || !s) return;
    if (w->stack != NO_STACK) return; // already stacked

    // Move window to stack position
    canvas.move_window(window_id, s->position);

    w->stack = stack_id;
    s->push(window_id);
}

void StackManager::unstack(Canvas& canvas, WindowId window_id) {
    auto* w = canvas.get(window_id);
    if (!w || w->stack == NO_STACK) return;

    StackId sid = w->stack;
    if (auto* s = get(sid)) {
        s->remove(window_id);
        if (s->empty()) {
            stacks_.erase(sid);
        }
    }
    w->stack = NO_STACK;
}

WindowId StackManager::cycle(Canvas& canvas, StackId stack_id) {
    auto* s = get(stack_id);
    if (!s || s->depth() < 2) return INVALID_WINDOW;

    WindowId new_top = s->cycle();
    canvas.set_focus(new_top);
    return new_top;
}

CardStack* StackManager::get(StackId id) {
    auto it = stacks_.find(id);
    return it != stacks_.end() ? &it->second : nullptr;
}

const CardStack* StackManager::get(StackId id) const {
    auto it = stacks_.find(id);
    return it != stacks_.end() ? &it->second : nullptr;
}

} // namespace chroma
