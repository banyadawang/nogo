#pragma once

#include "SearchInfo.h"
#include "BoardRenderer.h"
#include <string>

class EngineBridge;

enum class Player : int
{
    BLACK = Stone::BLACK,   // +1 — human goes first
    WHITE = Stone::WHITE,   // -1 — AI
};

/// GameController — thin coordinator.
/// Does NOT own board state; EngineBridge is the single source of truth.
class GameController
{
public:
    GameController(const std::string& weightFile);
    ~GameController();

    // ---- Board access (delegated to EngineBridge) ----
    const int* getBoard() const;
    int  getBoardAt(int idx) const;

    // ---- Game flow ----
    Player currentPlayer() const  { return m_currentPlayer; }
    bool   isGameOver() const     { return m_gameOver; }
    Player winner() const;

    /// Human plays at index. Returns true if legal and applied.
    bool playHuman(int idx);

    /// AI computes its move (MCTS only, board unchanged). Returns index or -1.
    /// Call this from a worker thread to avoid UI freeze.
    int  computeAIMove();

    /// Apply a previously-computed AI move (call from main thread).
    void applyAIMove(int step);

    /// Combined compute+apply (blocks; for simple usage).
    int  playAI();

    /// Check legality (delegated to engine).
    bool isLegalMove(int idx) const;

    /// All legal moves for current player.
    std::vector<int> getLegalMoves() const;

    /// Last played index (for display marker).
    int  getLastStep() const  { return m_lastStep; }

    /// Who just won (for display message).
    std::string getResultMessage() const;

    // ---- Search debug info ----
    const SearchInfo& getSearchInfo() const;

    // ---- Reset ----
    void reset();

    // ---- Engine access (for debug) ----
    EngineBridge* getBridge()  { return m_bridge; }

private:
    Player         m_currentPlayer = Player::BLACK;
    bool           m_gameOver      = false;
    int            m_lastStep      = -1;
    EngineBridge*  m_bridge        = nullptr;

    /// Called after each move to check terminal condition.
    void updateGameOver();
};
