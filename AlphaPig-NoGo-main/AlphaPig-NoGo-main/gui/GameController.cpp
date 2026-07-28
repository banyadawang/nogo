#include "GameController.h"
#include "EngineBridge.h"

// ============================================================
GameController::GameController(const std::string& weightFile)
{
    m_bridge = new EngineBridge(weightFile.c_str());
}

GameController::~GameController()
{
    delete m_bridge;
}

// ---- Board (delegated) ----
const int* GameController::getBoard() const
{
    return m_bridge->board();
}

int GameController::getBoardAt(int idx) const
{
    return m_bridge->boardAt(idx);
}

bool GameController::isLegalMove(int idx) const
{
    return m_bridge->isLegalMove(idx, static_cast<int>(m_currentPlayer));
}

std::vector<int> GameController::getLegalMoves() const
{
    return m_bridge->getLegalMoves(static_cast<int>(m_currentPlayer));
}

const SearchInfo& GameController::getSearchInfo() const
{
    return m_bridge->getSearchInfo();
}

// ---- Game flow ----
Player GameController::winner() const
{
    // The player who CAN'T move loses → the other wins
    return (m_currentPlayer == Player::BLACK) ? Player::WHITE : Player::BLACK;
}

std::string GameController::getResultMessage() const
{
    if (!m_gameOver) return "";
    Player w = winner();
    if (w == Player::BLACK)
        return "BLACK (You) wins!";
    else
        return "WHITE (AI) wins!";
}

bool GameController::playHuman(int idx)
{
    if (m_gameOver)                     return false;
    if (m_currentPlayer != Player::BLACK) return false;
    if (!m_bridge->isLegalMove(idx, static_cast<int>(Player::BLACK)))
        return false;

    m_bridge->applyMove(idx, static_cast<int>(Player::BLACK));
    m_lastStep = idx;
    m_currentPlayer = Player::WHITE;
    updateGameOver();
    return true;
}

int GameController::computeAIMove()
{
    // MCTS only — does NOT modify board (safe for worker thread)
    if (m_gameOver)                       return -1;
    if (m_currentPlayer != Player::WHITE) return -1;
    return m_bridge->choose(m_lastStep);
}

void GameController::applyAIMove(int step)
{
    // Apply on main thread — safe to touch board
    if (step < 0) {
        m_gameOver = true;
        return;
    }
    m_bridge->applyMove(step, static_cast<int>(Player::WHITE));
    m_lastStep = step;
    m_currentPlayer = Player::BLACK;
    updateGameOver();
}

int GameController::playAI()
{
    // Blocking combined version (for simple usage)
    int step = computeAIMove();
    applyAIMove(step);
    return step;
}

void GameController::updateGameOver()
{
    auto moves = m_bridge->getLegalMoves(static_cast<int>(m_currentPlayer));
    if (moves.empty()) {
        m_gameOver = true;
    }
}

void GameController::reset()
{
    m_bridge->resetBoard();
    m_currentPlayer = Player::BLACK;
    m_gameOver      = false;
    m_lastStep      = -1;
}
