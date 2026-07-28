#pragma once
// ============================================================
// FastScanAvailable — BFS-optimized legal-move scanner.
//
// Instead of iterating over all of father's available_last/next
// positions, this scans only positions near last_step.
//
// Algorithm (from Botzone reference):
//   1. From last_step, look at 4 adjacent positions.
//   2. Empty positions: check with judgeAvailable directly.
//   3. Occupied positions: BFS the connected group, count liberties.
//   4. If a group has exactly 1 liberty, that position is a candidate.
//   5. Filter father's available lists through the candidate set.
//
// Complexity: O(groups_size + candidates) vs O(all_candidates)
// ============================================================

#include <vector>
#include <queue>
#include <cstring>

// Board constants (shared with engine)
enum { BLK_EMPTY = 0, BLK_BLACK = 1, BLK_WHITE = -1 };

// Move on 9x9 board
inline int fsMoveTo(int p, int dir)
{
    switch (dir) {
    case 0: return (p += 9) < 81 ? p : -1;   // down
    case 1: return (p -= 9) >= 0 ? p : -1;   // up
    case 2: return p % 9 < 8 ? p + 1 : -1;   // right
    case 3: return p % 9 > 0 ? p - 1 : -1;   // left
    }
    return p;
}

// ============================================================
// Fast legal-move scanner.
//
// Parameters:
//   board[81]    — current board state
//   color         — player to move (BLK_BLACK or BLK_WHITE)
//   last_step     — opponent's last move (-1 if first move)
//   judgeAvail    — callback: bool(board, pos, color) as NoGo rule check
//   out_next       — [out] legal moves for `color` (available_next)
//   out_last       — [out] legal moves for `-color` (available_last)
//   father_next    — father node's available_next (for filtering out_last)
//   father_last    — father node's available_last (for filtering out_next)
// ============================================================
inline void fastScanAvailable(
    const int* board,
    int color,
    int last_step,
    bool (*judgeAvail)(const int*, int, int),
    std::vector<int>& out_next,
    std::vector<int>& out_last,
    const std::vector<int>& father_next,
    const std::vector<int>& father_last)
{
    bool ban_his[81] = {false};   // positions illegal for opponent (-color)
    bool ban_me[81]  = {false};   // positions illegal for us (color)
    bool vis[81]     = {false};   // visited BF groups

    // ---- Phase 1: scan near last_step ----
    if (last_step >= 0) {
        for (int dir = 0; dir < 4; dir++) {
            int p = fsMoveTo(last_step, dir);
            if (p < 0) continue;

            if (board[p] == BLK_EMPTY) {
                // Empty adjacent: check both players
                ban_me[p]  = !judgeAvail(board, p, color);
                ban_his[p] = !judgeAvail(board, p, -color);
            }
            else if (!vis[p]) {
                // Occupied: BFS the group, count liberties
                std::queue<int> queue;
                bool tgas_vis[81] = {false};
                int tgas = 0, tgas_size = 0;

                queue.push(p);
                while (!queue.empty()) {
                    int pq = queue.front();
                    queue.pop();
                    vis[pq] = true;

                    for (int d2 = 0; d2 < 4; d2++) {
                        int dp = fsMoveTo(pq, d2);
                        if (dp < 0) continue;
                        if (board[dp] == BLK_EMPTY && !tgas_vis[dp]) {
                            tgas_vis[dp] = true;
                            tgas_size++;
                            tgas = dp;
                        }
                        else if (board[dp] == board[pq] && !vis[dp]) {
                            queue.push(dp);
                        }
                    }
                }

                // If group has exactly 1 liberty, check it
                if (tgas_size == 1) {
                    ban_me[tgas]  = !judgeAvail(board, tgas, color);
                    ban_his[tgas] = !judgeAvail(board, tgas, -color);
                }
            }
        }
    }

    // ---- Phase 2: filter father's lists through ban flags ----
    // available_next = positions from father's available_last
    //   that are still empty and not banned for -color (opponent)
    for (auto i : father_last) {
        if (board[i] == BLK_EMPTY && !ban_his[i])
            out_next.push_back(i);
    }

    // available_last = positions from father's available_next
    //   that are still empty and not banned for color (us)
    for (auto i : father_next) {
        if (board[i] == BLK_EMPTY && !ban_me[i])
            out_last.push_back(i);
    }

    // ---- Phase 3: if last_step == -1 (first move), scan all ----
    if (last_step < 0) {
        // Fall back to scanning all father candidates
        // (already done in Phase 2, but need to check ALL empty positions)
        // For first move with no previous info, scan full board
        for (int i = 0; i < 81; i++) {
            if (board[i] != BLK_EMPTY) continue;

            // Check if already in out_next/out_last
            bool inNext = false, inLast = false;
            for (auto x : out_next) if (x == i) { inNext = true; break; }
            for (auto x : out_last) if (x == i) { inLast = true; break; }

            if (!inNext) {
                bool legal = judgeAvail(board, i, -color);
                if (legal) out_next.push_back(i);
                ban_his[i] = !legal;
            }
            if (!inLast) {
                bool legal = judgeAvail(board, i, color);
                if (legal) out_last.push_back(i);
                ban_me[i] = !legal;
            }
        }
    }
}
