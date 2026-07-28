#pragma once

#include "SearchInfo.h"
#include <vector>

class Network;
class AlphaPig;

/// RAII holder for Network + AlphaPig.  Single source of truth for board state.
class EngineBridge
{
public:
    EngineBridge(const char* weightFile);
    ~EngineBridge();

    // ---- Board (single source of truth) ----
    const int* board() const;
    int  boardAt(int idx) const;

    /// Apply a stone to the engine board.  Does NOT check legality.
    void applyMove(int idx, int color);

    // ---- Rules ----
    bool isLegalMove(int idx, int color) const;

    /// Return all legal indices for `color` on the current board.
    std::vector<int> getLegalMoves(int color) const;

    // ---- AI ----
    /// Run MCTS and return chosen move (0..80), or -1 if resigned.
    /// Fills SearchInfo (thread‑safe: board is read‑only during search).
    int  choose(int lastStep);

    /// Last search debug info.
    const SearchInfo& getSearchInfo() const { return m_si; }

    // ---- Reset ----
    void resetBoard();

private:
    Network*  m_net = nullptr;
    AlphaPig* m_ai  = nullptr;
    SearchInfo m_si;
};
