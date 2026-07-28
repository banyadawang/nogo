// ============================================================
// EngineBridge — wraps the AlphaPig engine for GUI consumption.
// Single source of truth for board state.
// ============================================================

#include "Alphapig.cpp"   // pulls in Network.cpp via #include
#include "EngineBridge.h"

#include <cstring>
#include <chrono>

// ============================================================
EngineBridge::EngineBridge(const char* weightFile)
{
    m_net = new Network(weightFile);
    m_ai  = new AlphaPig(m_net);
}

EngineBridge::~EngineBridge()
{
    delete m_ai;
    delete m_net;
}

const int* EngineBridge::board() const
{
    return m_ai->board;
}

int EngineBridge::boardAt(int idx) const
{
    return m_ai->board[idx];
}

void EngineBridge::applyMove(int idx, int color)
{
    m_ai->board[idx] = color;
}

void EngineBridge::resetBoard()
{
    std::memset(m_ai->board, 0, sizeof(m_ai->board));
}

// ============================================================
// Rules
// ============================================================
bool EngineBridge::isLegalMove(int idx, int color) const
{
    return m_ai->judgeAvailable(m_ai->board, idx, color);
}

std::vector<int> EngineBridge::getLegalMoves(int color) const
{
    std::vector<int> moves;
    for (int i = 0; i < 81; ++i) {
        if (m_ai->board[i] == 0 && m_ai->judgeAvailable(m_ai->board, i, color))
            moves.push_back(i);
    }
    return moves;
}

// ============================================================
// AI search
// ============================================================
int EngineBridge::choose(int lastStep)
{
    auto t0 = std::chrono::high_resolution_clock::now();

    // trainChoose populates debug_info internally
    auto [best, probs] = m_ai->trainChoose(lastStep);

    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();

    // Copy from engine DebugInfo to GUI SearchInfo
    const auto& di = m_ai->debug_info;
    m_si.playouts     = di.playouts;
    m_si.thinkingTime = elapsed;
    m_si.value        = di.value;
    m_si.bestMove     = di.bestMove;
    m_si.totalMoves   = di.totalMoves;
    m_si.bestRow      = (di.bestMove >= 0) ? di.bestMove / 9 : -1;
    m_si.bestCol      = (di.bestMove >= 0) ? di.bestMove % 9 : -1;
    m_si.resigned     = (best < 0);

    std::memcpy(m_si.policy.data(), di.policy, sizeof(di.policy));
    for (int i = 0; i < 81; ++i)
        m_si.visits[i] = di.visits[i];

    return best;
}
