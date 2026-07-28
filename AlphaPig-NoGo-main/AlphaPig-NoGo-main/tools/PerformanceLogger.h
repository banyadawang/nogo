#pragma once
// ============================================================
// PerformanceLogger — records per‑move and per‑game MCTS stats.
// ============================================================

#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

struct MoveRecord
{
    int    gameId      = 0;
    int    moveNumber  = 0;
    double searchTimeMs = 0.0;
    int    playouts    = 0;
    int    rootVisits  = 0;
    bool   treeReused  = false;
    int    reusedVisits = 0;
    int    newNodes    = 0;
    float  rootValue   = 0.f;
    int    bestMove    = -1;
    int    totalMoves  = 0;
    int    winner      = 0;     // 1=BLACK, -1=WHITE (filled at game end)
};

class PerformanceLogger
{
public:
    PerformanceLogger() = default;

    void open(const std::string& filePath)
    {
        m_file.open(filePath);
        m_file << "# AlphaPig M1.0 Performance Log\n";
        m_file << "# game_id,move,time_ms,playouts,root_visits,"
               << "reused,reused_visits,new_nodes,root_value,"
               << "best_move,total_moves,winner\n";
    }

    void close()
    {
        if (m_file.is_open()) m_file.close();
    }

    void logMove(const MoveRecord& rec)
    {
        m_file << rec.gameId       << ","
               << rec.moveNumber   << ","
               << std::fixed << std::setprecision(2) << rec.searchTimeMs << ","
               << rec.playouts     << ","
               << rec.rootVisits   << ","
               << (rec.treeReused ? "1" : "0") << ","
               << rec.reusedVisits << ","
               << rec.newNodes     << ","
               << std::fixed << std::setprecision(4) << rec.rootValue << ","
               << rec.bestMove     << ","
               << rec.totalMoves   << ","
               << rec.winner       << "\n";
        m_records.push_back(rec);
    }

    // Write per‑game summary
    void writeGameSummary(int gameId, int totalMoves, int winner)
    {
        // Tag all moves of this game with the winner
        for (auto& r : m_records) {
            if (r.gameId == gameId) r.winner = winner;
        }

        // Compute per‑game stats
        double totalTime = 0;
        int    totalPlayouts = 0;
        int    totalRootVisits = 0;
        int    reuseCount = 0;
        int    reuseVisitsSum = 0;
        int    moveCount = 0;

        for (auto& r : m_records) {
            if (r.gameId != gameId) continue;
            totalTime      += r.searchTimeMs;
            totalPlayouts  += r.playouts;
            totalRootVisits += r.rootVisits;
            if (r.treeReused) {
                reuseCount++;
                reuseVisitsSum += r.reusedVisits;
            }
            moveCount++;
        }

        if (moveCount == 0) return;

        m_file << "# --- Game " << gameId << " Summary ---\n";
        m_file << "# moves=" << totalMoves
               << " winner=" << winner
               << " avg_time_ms=" << std::fixed << std::setprecision(1) << (totalTime / moveCount)
               << " avg_playouts=" << (totalPlayouts / moveCount)
               << " avg_root_visits=" << (totalRootVisits / moveCount)
               << " reuse_count=" << reuseCount
               << " reuse_rate=" << std::setprecision(3) << (100.0 * reuseCount / moveCount) << "%"
               << " avg_reused_visits=" << (reuseCount > 0 ? reuseVisitsSum / reuseCount : 0)
               << "\n";
    }

    // Final summary across all games
    void writeFinalSummary(int totalGames)
    {
        double totalTime = 0;
        int    totalPlayouts = 0;
        int    totalRootVisits = 0;
        int    reuseCount = 0;
        int    reuseVisitsSum = 0;
        int    moveCount = 0;

        for (auto& r : m_records) {
            totalTime      += r.searchTimeMs;
            totalPlayouts  += r.playouts;
            totalRootVisits += r.rootVisits;
            if (r.treeReused) {
                reuseCount++;
                reuseVisitsSum += r.reusedVisits;
            }
            moveCount++;
        }

        m_file << "\n# ======== FINAL SUMMARY ========\n";
        m_file << "# games=" << totalGames << "\n";
        m_file << "# total_moves=" << moveCount << "\n";
        m_file << "# avg_time_ms=" << std::fixed << std::setprecision(1)
               << (moveCount > 0 ? totalTime / moveCount : 0) << "\n";
        m_file << "# avg_playouts=" << (moveCount > 0 ? totalPlayouts / moveCount : 0) << "\n";
        m_file << "# avg_root_visits=" << (moveCount > 0 ? totalRootVisits / moveCount : 0) << "\n";
        m_file << "# reuse_count=" << reuseCount << "\n";
        m_file << "# reuse_rate=" << std::setprecision(3)
               << (moveCount > 0 ? 100.0 * reuseCount / moveCount : 0) << "%\n";
        m_file << "# avg_reused_visits=" << (reuseCount > 0 ? reuseVisitsSum / reuseCount : 0) << "\n";
    }

private:
    std::ofstream        m_file;
    std::vector<MoveRecord> m_records;
};
