// ============================================================
// scanAvailable correctness & performance benchmark v2
// Correctly simulates father-child board state differences.
// ============================================================
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <vector>
#include <algorithm>

#ifndef NETWORK_CPP_INCLUDED
#include "../SelfPlay/Network.cpp"
#endif
#include "../SelfPlay/Alphapig.cpp"

// Reference scan (original simple version)
static void refScanAvailable(AlphaPig::TreeNode* node, AlphaPig& ai)
{
    int* board = node->board;
    for (auto i : node->father->available_last)
        if (ai.judgeAvailable(board, i, -node->color))
            node->available_next.push_back(i);
    for (auto i : node->father->available_next)
        if (ai.judgeAvailable(board, i, node->color))
            node->available_last.push_back(i);
}

static bool vecsEq(const std::vector<int>& a, const std::vector<int>& b) {
    if (a.size() != b.size()) return false;
    auto sa = a, sb = b;
    std::sort(sa.begin(), sa.end());
    std::sort(sb.begin(), sb.end());
    return sa == sb;
}

int main()
{
    printf("\n=== scanAvailable Benchmark v2 ===\n\n");

    Network net("network.weights");
    AlphaPig engine(&net);

    // ---- Part 1: Correctness (10,000 random states) ----
    printf("Part 1: Correctness (10,000 random states)\n");
    int pass = 0, fail = 0;
    srand(42);

    for (int t = 0; t < 10000; t++) {
        // Generate random board WITHOUT the last stone
        int fatherBoard[81] = {0};
        int stoneCount = rand() % 30;  // 0-29 stones
        int lastStep = -1;
        for (int s = 0; s < stoneCount; s++) {
            int pos = rand() % 81;
            if (fatherBoard[pos] == 0) {
                fatherBoard[pos] = (s % 2 == 0) ? 1 : -1;
                lastStep = pos;
            }
        }

        // Build father node from father's board
        AlphaPig::TreeNode father;
        father.color = (stoneCount % 2 == 0) ? 1 : -1;
        father.last_step = lastStep;
        memcpy(father.board, fatherBoard, sizeof(fatherBoard));
        for (int i = 0; i < 81; i++) {
            if (fatherBoard[i] == 0) {
                if (engine.judgeAvailable(fatherBoard, i, father.color))
                    father.available_next.push_back(i);
                if (engine.judgeAvailable(fatherBoard, i, -father.color))
                    father.available_last.push_back(i);
            }
        }
        if (father.available_next.empty()) continue;

        // Pick a move from father's available_next
        int step = father.available_next[rand() % father.available_next.size()];

        // Child's board = father's board + new stone
        int childBoard[81];
        memcpy(childBoard, fatherBoard, sizeof(fatherBoard));
        childBoard[step] = -father.color;  // child plays as -father.color

        // Create two identical children
        AlphaPig::TreeNode child1, child2;
        memcpy(child1.board, childBoard, sizeof(childBoard));
        memcpy(child2.board, childBoard, sizeof(childBoard));
        child1.color = -father.color;
        child2.color = -father.color;
        child1.last_step = step;
        child2.last_step = step;
        child1.father = &father;
        child2.father = &father;

        // NEW scanner (BFS)
        engine.scanAvailable(&child1);

        // OLD scanner (reference)
        refScanAvailable(&child2, engine);

        if (vecsEq(child1.available_next, child2.available_next) &&
            vecsEq(child1.available_last, child2.available_last)) {
            pass++;
        } else {
            fail++;
            if (fail <= 3) {
                printf("  FAIL #%d: next_eq=%d last_eq=%d\n", fail,
                       vecsEq(child1.available_next, child2.available_next),
                       vecsEq(child1.available_last, child2.available_last));
            }
        }
    }

    printf("  Result: %d PASS, %d FAIL\n\n", pass, fail);

    // ---- Part 2: Performance (100,000 calls) ----
    printf("Part 2: Performance (100,000 calls)\n");

    // Pre-generate test cases
    struct TC {
        AlphaPig::TreeNode father;
        AlphaPig::TreeNode child1, child2;
    };
    std::vector<TC> cases;
    srand(123);
    for (int t = 0; t < 2000; t++) {
        int fb[81] = {0};
        int sc = rand() % 30, ls = -1;
        for (int s = 0; s < sc; s++) {
            int pos = rand() % 81;
            if (fb[pos] == 0) { fb[pos] = (s%2==0)?1:-1; ls = pos; }
        }
        TC tc;
        tc.father.color = (sc % 2 == 0) ? 1 : -1;
        tc.father.last_step = ls;
        memcpy(tc.father.board, fb, sizeof(fb));
        for (int i = 0; i < 81; i++) {
            if (fb[i] == 0) {
                if (engine.judgeAvailable(fb, i, tc.father.color))
                    tc.father.available_next.push_back(i);
                if (engine.judgeAvailable(fb, i, -tc.father.color))
                    tc.father.available_last.push_back(i);
            }
        }
        if (!tc.father.available_next.empty()) {
            int step = tc.father.available_next[rand() % tc.father.available_next.size()];
            int cb[81]; memcpy(cb, fb, sizeof(fb)); cb[step] = -tc.father.color;
            memcpy(tc.child1.board, cb, sizeof(cb));
            memcpy(tc.child2.board, cb, sizeof(cb));
            tc.child1.color = tc.child2.color = -tc.father.color;
            tc.child1.last_step = tc.child2.last_step = step;
            tc.child1.father = tc.child2.father = &tc.father;
            cases.push_back(tc);
        }
    }
    printf("  Test cases: %zu\n", (size_t)cases.size());

    // Warmup
    for (int i = 0; i < 500; i++) {
        auto& tc = cases[i % cases.size()];
        tc.child1.available_next.clear(); tc.child1.available_last.clear();
        engine.scanAvailable(&tc.child1);
    }

    // Benchmark NEW
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100000; i++) {
        auto& tc = cases[i % cases.size()];
        tc.child1.available_next.clear(); tc.child1.available_last.clear();
        engine.scanAvailable(&tc.child1);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double newTime = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // Benchmark OLD
    for (int i = 0; i < 500; i++) {
        auto& tc = cases[i % cases.size()];
        tc.child2.available_next.clear(); tc.child2.available_last.clear();
        refScanAvailable(&tc.child2, engine);
    }
    t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100000; i++) {
        auto& tc = cases[i % cases.size()];
        tc.child2.available_next.clear(); tc.child2.available_last.clear();
        refScanAvailable(&tc.child2, engine);
    }
    auto t2 = std::chrono::high_resolution_clock::now();
    double oldTime = std::chrono::duration<double, std::milli>(t2 - t0).count();

    printf("  Old (full): %.2f ms, %.3f us/call\n", oldTime, oldTime/100000.0*1000.0);
    printf("  New (BFS):  %.2f ms, %.3f us/call\n", newTime, newTime/100000.0*1000.0);
    if (oldTime > 0) printf("  Speedup: %.2fx\n\n", oldTime / newTime);

    return fail > 0 ? 1 : 0;
}
