// ============================================================
// SelfPlayValidate — validation harness for MCTS experiments.
//
// Reads a config file, runs N self‑play games with detailed
// per‑move performance logging, and writes CSV results.
//
// Usage:
//   SelfPlayValidate.exe configs/baseline.txt
//   SelfPlayValidate.exe configs/tree_reuse.txt
// ============================================================

#include <iostream>
#include <vector>
#include <fstream>
#include <chrono>
#include <cstring>
#include <string>
#include <omp.h>

#include "Alphapig.cpp"

// ---- Our tool headers ----
#include "../tools/Config.h"
#include "../tools/PerformanceLogger.h"

// ---- Training data (same as original) ----
struct TrainData
{
    float input[9 * 9 * 6] = { 0 };
    float output_policy[9 * 9] = { 0 };
    float output_value = 0;
};

static std::vector<TrainData> trainDatas;
static omp_lock_t g_lock;
static PerformanceLogger g_logger;
static Config g_cfg;

// ---- Game counter (atomic for OpenMP) ----
static int g_gameCounter = 0;
static omp_lock_t g_gameLock;

static TrainData makeTrainData(int* board, int p,
    std::vector<float> probs, int last_p, bool first)
{
    AlphaPig alphaPig(nullptr);
    TrainData res;
    for (int i = 0; i < 81; i++) {
        if (board[i] == AI)      res.input[0 * 81 + i] = 1;
        if (board[i] == OP)      res.input[1 * 81 + i] = 1;
        if (alphaPig.judgeAvailable(board, i, AI)) res.input[2 * 81 + i] = 1;
        if (alphaPig.judgeAvailable(board, i, OP)) res.input[3 * 81 + i] = 1;
        if (first)               res.input[4 * 81 + i] = 1;
        res.output_policy[i] = probs[i];
    }
    if (last_p >= 0 && last_p < 81)
        res.input[5 * 81 + last_p] = 1;
    return res;
}

// ============================================================
// Play one game with performance logging
// ============================================================
struct GameResult { int winner; int totalMoves; };

static GameResult playGameLogged(Network* net, int gameId)
{
    srand((unsigned int)(time(nullptr) + omp_get_thread_num() + gameId * 137));
    AlphaPig alphaPig1(net);
    AlphaPig alphaPig2(net);

    // Apply config
    
    std::vector<TrainData> trainData1, trainData2;
    int win = 0, last_p = -1, moveNum = 0;

    while (true)
    {
        // --- Player 1 ---
        auto t0 = std::chrono::high_resolution_clock::now();
        auto a1 = alphaPig1.trainChoose(last_p);
        auto t1 = std::chrono::high_resolution_clock::now();

        int p1 = std::get<0>(a1);
        auto& probs1 = std::get<1>(a1);

        // Log player 1's move
        {
            MoveRecord rec;
            rec.gameId      = gameId;
            rec.moveNumber  = moveNum++;
            rec.searchTimeMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
            rec.playouts    = alphaPig1.debug_info.playouts;
            rec.rootVisits  = alphaPig1.root ? alphaPig1.root->total : 0;
            rec.treeReused  = false;
            rec.reusedVisits = 0;
            rec.newNodes    = 0;
            rec.rootValue   = alphaPig1.debug_info.value;
            rec.bestMove    = p1;
            rec.totalMoves  = alphaPig1.debug_info.totalMoves;
            omp_set_lock(&g_lock);
            g_logger.logMove(rec);
            omp_unset_lock(&g_lock);
        }

        if (p1 == -1) {
            win = 2;
            for (size_t i = 0; i < trainData1.size(); i++) trainData1[i].output_value = -1;
            for (size_t i = 0; i < trainData2.size(); i++) trainData2[i].output_value = 1;
            break;
        }
        trainData1.push_back(makeTrainData(alphaPig1.board, p1, probs1, last_p, true));
        alphaPig1.board[p1] = AI;
        alphaPig2.board[p1] = OP;
        last_p = p1;

        // --- Player 2 ---
        auto t2 = std::chrono::high_resolution_clock::now();
        auto a2 = alphaPig2.trainChoose(last_p);
        auto t3 = std::chrono::high_resolution_clock::now();

        int p2 = std::get<0>(a2);
        auto& probs2 = std::get<1>(a2);

        // Log player 2's move
        {
            MoveRecord rec;
            rec.gameId      = gameId;
            rec.moveNumber  = moveNum++;
            rec.searchTimeMs = std::chrono::duration<double, std::milli>(t3 - t2).count();
            rec.playouts    = alphaPig2.debug_info.playouts;
            rec.rootVisits  = alphaPig2.root ? alphaPig2.root->total : 0;
            rec.treeReused  = false;
            rec.reusedVisits = 0;
            rec.newNodes    = 0;
            rec.rootValue   = alphaPig2.debug_info.value;
            rec.bestMove    = p2;
            rec.totalMoves  = alphaPig2.debug_info.totalMoves;
            omp_set_lock(&g_lock);
            g_logger.logMove(rec);
            omp_unset_lock(&g_lock);
        }

        if (p2 == -1) {
            win = 1;
            for (size_t i = 0; i < trainData1.size(); i++) trainData1[i].output_value = 1;
            for (size_t i = 0; i < trainData2.size(); i++) trainData2[i].output_value = -1;
            break;
        }
        trainData2.push_back(makeTrainData(alphaPig2.board, p2, probs2, last_p, false));
        alphaPig1.board[p2] = OP;
        alphaPig2.board[p2] = AI;
        last_p = p2;
    }

    omp_set_lock(&g_lock);
    trainDatas.insert(trainDatas.end(), trainData1.begin(), trainData1.end());
    trainDatas.insert(trainDatas.end(), trainData2.begin(), trainData2.end());
    g_logger.writeGameSummary(gameId, moveNum, (win == 1 ? 1 : -1));
    omp_unset_lock(&g_lock);

    return { win, moveNum };
}

// ============================================================
// Main
// ============================================================
int main(int argc, char* argv[])
{
    system("chcp 65001");

    std::string cfgPath = "configs/baseline.txt";
    if (argc > 1) cfgPath = argv[1];

    std::cout << "=== AlphaPig M1.0 Validation ===\n";
    std::cout << "Config: " << cfgPath << "\n";

    if (!g_cfg.load(cfgPath)) {
        std::cerr << "ERROR: cannot load config " << cfgPath << "\n";
        return 1;
    }

    std::string weightPath = g_cfg.getStr("model.weight_file", "network.weights");
    int numGames  = g_cfg.getInt("test.num_games", 100);
    int baseSeed  = g_cfg.getInt("test.seed", 42);
    bool treeReuse = g_cfg.getBool("mcts.tree_reuse", false);
    std::string outPath = g_cfg.getStr("test.output", "results/result.csv");

    std::cout << "Tree Reuse: " << (treeReuse ? "ON" : "OFF") << "\n";
    std::cout << "Games: " << numGames << "\n";
    std::cout << "Output: " << outPath << "\n\n";

    // Init
    omp_init_lock(&g_lock);
    omp_init_lock(&g_gameLock);
    g_logger.open(outPath);

    srand((unsigned int)baseSeed);

    auto t0 = std::chrono::high_resolution_clock::now();
    int wins1 = 0, wins2 = 0;

#pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < numGames; i++) {
        Network net(weightPath);
        auto result = playGameLogged(&net, i);

        omp_set_lock(&g_gameLock);
        if (result.winner == 1) wins1++;
        else wins2++;
        g_gameCounter++;
        if (g_gameCounter % 10 == 0 || g_gameCounter == numGames) {
            std::cout << "\r  Games: " << g_gameCounter << "/" << numGames
                      << "  P1=" << wins1 << "  P2=" << wins2 << std::flush;
        }
        omp_unset_lock(&g_gameLock);
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();
    std::cout << "\n\nDone in " << std::fixed << std::setprecision(1) << elapsed << " s\n";
    std::cout << "P1 wins: " << wins1 << ", P2 wins: " << wins2 << "\n";

    g_logger.writeFinalSummary(numGames);
    g_logger.close();

    omp_destroy_lock(&g_lock);
    omp_destroy_lock(&g_gameLock);

    std::cout << "Results written to: " << outPath << "\n";
    return 0;
}
