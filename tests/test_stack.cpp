/// @file test_stack.cpp
/// @brief Unit tests for CardStack and StackManager.

#include "test_common.hpp"
#include "stack.hpp"

using namespace chroma;

// ============================================================================
// CardStack
// ============================================================================

TEST(CardStack_empty_on_creation) {
    CardStack s;
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.depth(), 0u);
    EXPECT_EQ(s.top(), INVALID_WINDOW);
    PASS();
}

TEST(CardStack_push_adds_to_top) {
    CardStack s;
    s.push(1);
    EXPECT_EQ(s.depth(), 1u);
    EXPECT_EQ(s.top(), 1u);
    EXPECT_FALSE(s.empty());
    PASS();
}

TEST(CardStack_push_multiple) {
    CardStack s;
    s.push(1);
    s.push(2);
    s.push(3);
    EXPECT_EQ(s.depth(), 3u);
    EXPECT_EQ(s.top(), 3u); // last pushed is top
    PASS();
}

TEST(CardStack_push_dedup_moves_to_top) {
    CardStack s;
    s.push(1);
    s.push(2);
    s.push(1); // should move 1 to top, not duplicate
    EXPECT_EQ(s.depth(), 2u);
    EXPECT_EQ(s.top(), 1u);
    PASS();
}

TEST(CardStack_pop_returns_top) {
    CardStack s;
    s.push(1);
    s.push(2);
    WindowId w = s.pop();
    EXPECT_EQ(w, 2u);
    EXPECT_EQ(s.depth(), 1u);
    EXPECT_EQ(s.top(), 1u);
    PASS();
}

TEST(CardStack_pop_empty_returns_invalid) {
    CardStack s;
    WindowId w = s.pop();
    EXPECT_EQ(w, INVALID_WINDOW);
    EXPECT_TRUE(s.empty());
    PASS();
}

TEST(CardStack_cycle_with_one) {
    CardStack s;
    s.push(1);
    WindowId w = s.cycle();
    EXPECT_EQ(w, 1u);
    EXPECT_EQ(s.depth(), 1u);
    PASS();
}

TEST(CardStack_cycle_with_multiple) {
    CardStack s;
    s.push(1);
    s.push(2);
    s.push(3);
    // Top = 3, then 2, then 1
    WindowId w = s.cycle();
    EXPECT_EQ(w, 2u); // 3 moves to bottom, 2 becomes top
    EXPECT_EQ(s.depth(), 3u);

    // Verify order: [2, 1, 3]
    EXPECT_EQ(s.top(), 2u);
    s.cycle();
    EXPECT_EQ(s.top(), 1u); // now [1, 3, 2]
    s.cycle();
    EXPECT_EQ(s.top(), 3u); // back to [3, 2, 1]
    PASS();
}

TEST(CardStack_cycle_empty_returns_invalid) {
    CardStack s;
    WindowId w = s.cycle();
    EXPECT_EQ(w, INVALID_WINDOW);
    PASS();
}

TEST(CardStack_remove_existing) {
    CardStack s;
    s.push(1);
    s.push(2);
    s.push(3);
    EXPECT_TRUE(s.remove(2));
    EXPECT_EQ(s.depth(), 2u);
    // Order should be [3, 1]
    EXPECT_EQ(s.top(), 3u);
    PASS();
}

TEST(CardStack_remove_nonexistent) {
    CardStack s;
    s.push(1);
    EXPECT_FALSE(s.remove(99));
    EXPECT_EQ(s.depth(), 1u);
    PASS();
}

TEST(CardStack_remove_top) {
    CardStack s;
    s.push(1);
    s.push(2);
    EXPECT_TRUE(s.remove(2));
    EXPECT_EQ(s.top(), 1u);
    EXPECT_EQ(s.depth(), 1u);
    PASS();
}

TEST(CardStack_offset_for_top_is_zero) {
    CardStack s;
    Vec2 off = s.offset_for(0);
    EXPECT_EQ(off.x, 0.0f);
    EXPECT_EQ(off.y, 0.0f);
    PASS();
}

TEST(CardStack_offset_increases_with_index) {
    CardStack s;
    Vec2 off1 = s.offset_for(1);
    Vec2 off2 = s.offset_for(2);
    Vec2 off3 = s.offset_for(3);
    EXPECT_EQ(off1.x, config::STACK_OFFSET);
    EXPECT_EQ(off1.y, config::STACK_OFFSET);
    EXPECT_EQ(off2.x, 2.0f * config::STACK_OFFSET);
    EXPECT_EQ(off2.y, 2.0f * config::STACK_OFFSET);
    EXPECT_EQ(off3.x, 3.0f * config::STACK_OFFSET);
    EXPECT_EQ(off3.y, 3.0f * config::STACK_OFFSET);
    PASS();
}

// ============================================================================
// StackManager
// ============================================================================

TEST(StackManager_create_returns_valid_id) {
    StackManager mgr;
    StackId id = mgr.create(Vec2{100, 200});
    EXPECT_NE(id, NO_STACK);
    auto* s = mgr.get(id);
    EXPECT_TRUE(s != nullptr);
    EXPECT_EQ(s->position.x, 100.0f);
    EXPECT_EQ(s->position.y, 200.0f);
    PASS();
}

TEST(StackManager_get_nonexistent_returns_null) {
    StackManager mgr;
    EXPECT_TRUE(mgr.get(999) == nullptr);
    PASS();
}

TEST(StackManager_stack_window) {
    StackManager mgr;
    Canvas c;
    WindowId wid = c.add(ChromaWindow{});
    StackId sid = mgr.create(Vec2{50, 50});

    mgr.stack(c, wid, sid);

    auto* w = c.get(wid);
    EXPECT_TRUE(w != nullptr);
    EXPECT_EQ(w->stack, sid);

    auto* s = mgr.get(sid);
    EXPECT_EQ(s->depth(), 1u);
    EXPECT_EQ(s->top(), wid);
    PASS();
}

TEST(StackManager_stack_already_stacked_is_noop) {
    StackManager mgr;
    Canvas c;
    WindowId wid = c.add(ChromaWindow{});
    StackId sid1 = mgr.create(Vec2{0, 0});
    StackId sid2 = mgr.create(Vec2{100, 100});

    mgr.stack(c, wid, sid1);
    mgr.stack(c, wid, sid2); // should be no-op

    auto* w = c.get(wid);
    EXPECT_EQ(w->stack, sid1); // still in first stack
    PASS();
}

TEST(StackManager_unstack_window) {
    StackManager mgr;
    Canvas c;
    WindowId wid = c.add(ChromaWindow{});
    StackId sid = mgr.create(Vec2{50, 50});

    mgr.stack(c, wid, sid);
    mgr.unstack(c, wid);

    auto* w = c.get(wid);
    EXPECT_EQ(w->stack, NO_STACK);

    auto* s = mgr.get(sid);
    EXPECT_TRUE(s == nullptr); // stack removed because empty
    PASS();
}

TEST(StackManager_unstack_non_stacked_window_is_noop) {
    StackManager mgr;
    Canvas c;
    WindowId wid = c.add(ChromaWindow{});
    mgr.unstack(c, wid); // should not crash
    PASS();
}

TEST(StackManager_destroy_removes_stack_and_unstacks_windows) {
    StackManager mgr;
    Canvas c;

    WindowId w1 = c.add(ChromaWindow{});
    WindowId w2 = c.add(ChromaWindow{});
    StackId sid = mgr.create(Vec2{0, 0});

    mgr.stack(c, w1, sid);
    mgr.stack(c, w2, sid);
    mgr.destroy(c, sid);

    EXPECT_TRUE(mgr.get(sid) == nullptr);
    EXPECT_EQ(c.get(w1)->stack, NO_STACK);
    EXPECT_EQ(c.get(w2)->stack, NO_STACK);
    PASS();
}

TEST(StackManager_cycle_stack) {
    StackManager mgr;
    Canvas c;

    WindowId w1 = c.add(ChromaWindow{});
    WindowId w2 = c.add(ChromaWindow{});
    WindowId w3 = c.add(ChromaWindow{});
    StackId sid = mgr.create(Vec2{0, 0});

    mgr.stack(c, w1, sid);
    mgr.stack(c, w2, sid);
    mgr.stack(c, w3, sid);
    // Top = w3

    WindowId new_top = mgr.cycle(c, sid);
    EXPECT_EQ(new_top, w2); // w2 becomes top
    EXPECT_EQ(c.focused_window(), w2);

    // Verify stack order
    auto* s = mgr.get(sid);
    EXPECT_EQ(s->top(), w2);
    PASS();
}

TEST(StackManager_cycle_single_window) {
    StackManager mgr;
    Canvas c;

    WindowId w1 = c.add(ChromaWindow{});
    StackId sid = mgr.create(Vec2{0, 0});
    mgr.stack(c, w1, sid);

    WindowId result = mgr.cycle(c, sid);
    EXPECT_EQ(result, INVALID_WINDOW); // need at least 2
    EXPECT_EQ(c.focused_window(), INVALID_WINDOW);
    PASS();
}

TEST(StackManager_all_stacks) {
    StackManager mgr;
    mgr.create(Vec2{0, 0});
    mgr.create(Vec2{100, 100});
    EXPECT_EQ(mgr.all_stacks().size(), 2u);
    PASS();
}
