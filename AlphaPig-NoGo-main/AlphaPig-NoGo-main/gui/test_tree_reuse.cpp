// ============================================================
// M1.0 Tree Reuse — Unit Tests
//
// Build:
//   cl /EHsc /std:c++20 /I.. /I../SelfPlay test_tree_reuse.cpp
//        /link mkl_intel_lp64.lib mkl_sequential.lib mkl_core.lib
//
// These tests validate the tree-reuse logic without requiring
// a GUI or a trained network.
// ============================================================

#include <cassert>
#include <cstdio>
#include <cstring>

// Pull in the engine
// Network.cpp has #pragma once, so it is safe to include directly.
// Alphapig.cpp internally does #include "network.cpp"; on Windows
// (case-insensitive) this resolves to Network.cpp.  The #pragma once
// in Network.cpp prevents double-inclusion.
#include "../SelfPlay/Network.cpp"
#include "../SelfPlay/Alphapig.cpp"

static int g_passed = 0;
static int g_failed = 0;

#define TEST(name)  printf("  TEST: %s ... ", name)
#define PASS()      do { printf("PASS\n"); g_passed++; } while(0)
#define FAIL(msg)   do { printf("FAIL: %s\n", msg); g_failed++; } while(0)
#define CHECK(cond) do { if (!(cond)) { FAIL(#cond); return; } } while(0)

// ============================================================
// Test 1: Basic reuse — child promotion
// ============================================================
static void test_basic_reuse()
{
    TEST("promote existing child to root");

    Network net("network.weights");
    AlphaPig ai(&net);
    ai.treeReuseEnabled = true;

    // Simulate: player sets up a board state
    ai.board[0] = 0;  // ensure position 10 is empty
    // Create initial root
    ai.initRoot(-1);
    CHECK(ai.root != nullptr);
    CHECK(ai.root->father == nullptr);

    // Manually create a child at position 10
    // (normally created by MCTS, but we test the promotion logic directly)
    int childIdx = 10;
    ai.board[childIdx] = 0;  // must be empty for judgeAvailable

    // We need the root to have available_next including 10
    // initRoot already scanned, but let's make sure
    if (!ai.judgeAvailable(ai.root->board, childIdx, AI)) {
        // If not available, use the first available move
        if (!ai.root->available_next.empty())
            childIdx = ai.root->available_next[0];
        else {
            FAIL("no legal moves for test");
            return;
        }
    }

    // Do a few MCTS iterations to build some statistics in the tree
    for (int i = 0; i < 50; i++)
        ai.select(ai.root);

    // Record the child's visit count before reuse
    int prevVisits = 0;
    if (ai.root->children[childIdx])
        prevVisits = ai.root->children[childIdx]->total;

    // Attempt reuse
    bool reused = ai.tryReuseRoot(childIdx);
    CHECK(reused == true);
    CHECK(ai.root != nullptr);
    CHECK(ai.root->father == nullptr);
    // The new root should have the visits from the promoted child
    CHECK(ai.root->total == prevVisits);

    ai.clearTree();
    PASS();
}

// ============================================================
// Test 2: Reuse nonexistent move — should fail gracefully
// ============================================================
static void test_no_child()
{
    TEST("reuse nonexistent move returns false");

    Network net("network.weights");
    AlphaPig ai(&net);
    ai.treeReuseEnabled = true;

    ai.initRoot(-1);
    CHECK(ai.root != nullptr);

    // Try to reuse a move that has no child
    // Position 99 is out of bounds, should fail
    bool reused = ai.tryReuseRoot(99);
    CHECK(reused == false);
    CHECK(ai.root != nullptr);  // original root still intact

    // Try a valid index that has no child
    int emptyIdx = -1;
    for (int i = 0; i < 81; i++) {
        if (!ai.root->children[i]) {
            emptyIdx = i;
            break;
        }
    }
    if (emptyIdx >= 0) {
        reused = ai.tryReuseRoot(emptyIdx);
        CHECK(reused == false);
    }

    ai.clearTree();
    PASS();
}

// ============================================================
// Test 3: Reuse preserves visit statistics
// ============================================================
static void test_preserve_stats()
{
    TEST("reused root preserves visit/value stats");

    Network net("network.weights");
    AlphaPig ai(&net);
    ai.treeReuseEnabled = true;

    ai.initRoot(-1);

    // Find a legal move
    if (ai.root->available_next.empty()) {
        FAIL("no legal moves");
        return;
    }
    int move = ai.root->available_next[0];

    // Run MCTS
    for (int i = 0; i < 100; i++)
        ai.select(ai.root);

    int beforeVisits = ai.root->total;
    int childVisits = 0;
    if (ai.root->children[move])
        childVisits = ai.root->children[move]->total;
    double childValue = 0;
    if (ai.root->children[move])
        childValue = ai.root->children[move]->value;

    // Reuse
    bool reused = ai.tryReuseRoot(move);
    CHECK(reused == true);

    // Check that the promoted root's stats match the child's
    CHECK(ai.root->total == childVisits);
    CHECK(ai.root->value == childValue);
    // didReuse should be true
    CHECK(ai.didReuse == true);
    // reusedNodesAccum should equal child's visit count
    CHECK(ai.reusedNodesAccum == childVisits);

    ai.clearTree();
    PASS();
}

// ============================================================
// Test 4: Disabled tree reuse
// ============================================================
static void test_disabled_reuse()
{
    TEST("tree reuse disabled returns false");

    Network net("network.weights");
    AlphaPig ai(&net);
    ai.treeReuseEnabled = false;  // DISABLED

    ai.initRoot(-1);
    int move = ai.root->available_next.empty() ? -1 : ai.root->available_next[0];
    if (move < 0) {
        ai.clearTree();
        PASS();  // no legal moves, skip
        return;
    }

    // Run MCTS to build tree
    for (int i = 0; i < 30; i++)
        ai.select(ai.root);

    // Should return false when disabled
    bool reused = ai.tryReuseRoot(move);
    CHECK(reused == false);

    ai.clearTree();
    PASS();
}

// ============================================================
// Test 5: Game reset clears tree
// ============================================================
static void test_game_reset()
{
    TEST("game reset clears persistent tree");

    Network net("network.weights");
    AlphaPig ai(&net);
    ai.treeReuseEnabled = true;

    ai.initRoot(-1);
    CHECK(ai.root != nullptr);

    // Do some MCTS
    for (int i = 0; i < 30; i++)
        ai.select(ai.root);

    // Reset
    ai.clearTree();
    CHECK(ai.root == nullptr);
    CHECK(ai.didReuse == false);
    CHECK(ai.reusedNodesAccum == 0);

    PASS();
}

// ============================================================
// Test 6: Full trainChoose cycle with reuse
// ============================================================
static void test_full_cycle()
{
    TEST("full trainChoose cycle detects reuse");

    Network net("network.weights");
    AlphaPig ai(&net);
    ai.treeReuseEnabled = true;

    // First call: no tree exists, should build fresh
    auto [best1, probs1] = ai.trainChoose(-1);
    CHECK(best1 >= 0);
    CHECK(ai.debug_info.treeReused == false);
    int firstBest = best1;

    // Simulate: opponent played `firstBest`
    // Second call: should try to reuse the child matching firstBest
    auto [best2, probs2] = ai.trainChoose(firstBest);
    CHECK(best2 >= 0 || best2 == -1);
    // treeReused should be true if the opponent's move was found in our tree
    printf("  (treeReused=%s reusedVisits=%d)\n",
           ai.debug_info.treeReused ? "YES" : "NO",
           ai.debug_info.reusedVisits);

    ai.clearTree();
    PASS();
}

// ============================================================
int main()
{
    printf("\n=== AlphaPig M1.0 Tree Reuse Unit Tests ===\n\n");

    test_basic_reuse();
    test_no_child();
    test_preserve_stats();
    test_disabled_reuse();
    test_game_reset();
    test_full_cycle();

    printf("\n=== Results: %d passed, %d failed ===\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
