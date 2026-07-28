// ============================================================
// AlphaPig GUI — NoGo human-vs-AI client  (SFML 2.6)
//
// Phases GUI-1 ~ GUI-7:
//   - 9x9 board with stone rendering
//   - Human click → legality check → engine move
//   - AI search in separate thread (no UI freeze)
//   - MCTS visit heatmap
//   - Policy top-10 display
//   - Value history (last 10)
//   - Debug mode (D key)
// ============================================================

#include <SFML/Graphics.hpp>

#include "BoardRenderer.h"
#include "GameController.h"
#include "EngineBridge.h"
#include "SearchInfo.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <deque>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// ============================================================
// Constants
// ============================================================
constexpr int WIN_W       = 900;
constexpr int WIN_H       = 700;
constexpr int PANEL_X     = 680;   // BOARD_AREA_W + 20
constexpr int LINE_H      = 22;
constexpr int VALUE_HIST_N = 10;

// ============================================================
// Font helper
// ============================================================
sf::Font g_font;

void loadAppFont()
{
    const char* paths[] = {
        "assets/font.ttf",
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/simhei.ttf",
    };
    for (auto p : paths) {
        if (g_font.loadFromFile(p)) return;
    }
}

// ============================================================
// Info panel drawing (SFML 2.6 — all sf::Text built via setters)
// ============================================================
static void makeLabel(sf::Text& t, const std::string& s, unsigned sz,
                      sf::Color c, float x, float y)
{
    t.setFont(g_font);
    t.setString(s);
    t.setCharacterSize(sz);
    t.setFillColor(c);
    t.setPosition(x, y);
}

struct PanelCtx
{
    sf::RenderWindow& w;
    int y = 30;
};

static void drawLine(PanelCtx& ctx, const std::string& text,
                     sf::Color col = sf::Color::White)
{
    sf::Text t;
    makeLabel(t, text, 16, col, (float)PANEL_X, (float)ctx.y);
    ctx.w.draw(t);
    ctx.y += LINE_H;
}

static void drawSection(PanelCtx& ctx, const std::string& title)
{
    ctx.y += 8;
    sf::Text t;
    makeLabel(t, title, 17, sf::Color(255, 200, 80), (float)PANEL_X, (float)ctx.y);
    t.setStyle(sf::Text::Bold);
    ctx.w.draw(t);
    ctx.y += LINE_H + 4;
}

// ============================================================
// Full info panel render
// ============================================================
static void drawInfoPanel(sf::RenderWindow& window,
                          const GameController& ctrl,
                          bool aiThinking,
                          bool illegalMsg,
                          const std::deque<float>& valueHistory,
                          bool debugMode)
{
    PanelCtx ctx{window, 30};
    const auto& si = ctrl.getSearchInfo();

    // ---- Player ----
    drawSection(ctx, "=== Player ===");
    {
        Player cp = ctrl.currentPlayer();
        drawLine(ctx, (cp == Player::BLACK) ? "BLACK (You)" : "WHITE (AI)",
                 (cp == Player::BLACK) ? sf::Color(200, 200, 200)
                                       : sf::Color(255, 220, 100));
    }

    if (ctrl.isGameOver()) {
        ctx.y += 6;
        drawLine(ctx, ctrl.getResultMessage(), sf::Color(100, 255, 100));
    }

    if (illegalMsg) {
        ctx.y += 6;
        drawLine(ctx, "Illegal Move!", sf::Color(255, 80, 80));
    }

    if (aiThinking) {
        ctx.y += 6;
        drawLine(ctx, "AI thinking...", sf::Color(255, 200, 50));
    }

    // ---- Search Info ----
    if (si.playouts > 0) {
        drawSection(ctx, "=== Search ===");
        {
            std::ostringstream ss;
            ss << "Playouts: " << si.playouts;
            drawLine(ctx, ss.str());
        }
        {
            std::ostringstream ss;
            ss << "Time: " << std::fixed << std::setprecision(2)
               << si.thinkingTime << "s";
            drawLine(ctx, ss.str());
        }
        {
            std::ostringstream ss;
            ss << "Value: " << std::fixed << std::setprecision(4) << si.value;
            drawLine(ctx, ss.str());
        }
        {
            std::ostringstream ss;
            ss << "Best: (" << si.bestRow << "," << si.bestCol << ")";
            drawLine(ctx, ss.str(), sf::Color(100, 255, 100));
        }
        drawLine(ctx, "Legal moves: " + std::to_string(si.totalMoves));

        // ---- Top 10 Policy ----
        drawSection(ctx, "=== Top Policy ===");
        struct Entry { int idx; int visits; float prob; };
        std::vector<Entry> entries;
        for (int i = 0; i < 81; ++i) {
            if (si.visits[i] > 0)
                entries.push_back({i, si.visits[i], si.policy[i]});
        }
        std::sort(entries.begin(), entries.end(),
                  [](const Entry& a, const Entry& b) { return a.visits > b.visits; });

        int show = std::min(10, (int)entries.size());
        for (int i = 0; i < show; ++i) {
            auto& e = entries[i];
            int r = e.idx / 9, c = e.idx % 9;
            std::ostringstream ss;
            ss << "(" << r << "," << c << ") "
               << std::fixed << std::setprecision(1) << (e.prob * 100.f) << "%"
               << " v:" << e.visits;
            sf::Color col = (i == 0) ? sf::Color(100, 255, 100)
                           : (i < 3) ? sf::Color(200, 200, 200)
                           : sf::Color(150, 150, 150);
            drawLine(ctx, ss.str(), col);
        }

        // ---- Value History ----
        if (!valueHistory.empty()) {
            drawSection(ctx, "=== Value Hist ===");
            for (auto it = valueHistory.rbegin(); it != valueHistory.rend(); ++it) {
                std::ostringstream ss;
                ss << std::fixed << std::setprecision(4) << *it;
                drawLine(ctx, ss.str(),
                         (*it > 0 ? sf::Color(100, 200, 100) : sf::Color(200, 100, 100)));
            }
        }
    }

    // ---- Debug mode ----
    if (debugMode) {
        drawSection(ctx, "=== DEBUG ===");
        const int* bd = ctrl.getBoard();
        // Print board as 9x9 grid
        for (int row = 0; row < 9; ++row) {
            std::ostringstream ss;
            for (int col = 0; col < 9; ++col) {
                int v = bd[row * 9 + col];
                ss << (v == 1 ? 'X' : v == -1 ? 'O' : '.') << ' ';
            }
            drawLine(ctx, ss.str(), sf::Color(120, 120, 120));
        }
        {
            auto moves = ctrl.getLegalMoves();
            std::ostringstream ss;
            ss << "Legal: " << moves.size();
            drawLine(ctx, ss.str(), sf::Color(120, 200, 120));
        }
        {
            std::ostringstream ss;
            ss << "Last step: " << ctrl.getLastStep();
            drawLine(ctx, ss.str(), sf::Color(120, 120, 200));
        }
    }

    // ---- Bottom help ----
    ctx.y = WIN_H - 55;
    drawLine(ctx, "[R] New  [H] Heatmap  [D] Debug", sf::Color(140, 140, 140));
    drawLine(ctx, "[Esc] Quit", sf::Color(140, 140, 140));
}

// ============================================================
// Main
// ============================================================
int main(int argc, char* argv[])
{
    std::string weightPath = "network.weights";
    if (argc > 1) weightPath = argv[1];

    // ---- Window ----
    sf::RenderWindow window(sf::VideoMode(WIN_W, WIN_H, 32),
                            "AlphaPig NoGo - GUI",
                            sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);
    window.setVerticalSyncEnabled(true);

    loadAppFont();

    // ---- Engine & controller ----
    GameController controller(weightPath);
    BoardRenderer  renderer;

    // ---- UI state ----
    std::atomic<bool> aiThinking{false};
    std::atomic<int>  aiMove{-2};       // -2 = idle, -1 = resign, 0..80 = move
    bool showHeatmap  = true;
    bool debugMode    = false;
    bool illegalMsg   = false;
    int  illegalTimer = 0;
    int  hoverIdx      = -1;
    bool hoverLegal    = false;

    // Value history
    std::deque<float> valueHistory;

    // ---- Main loop ----
    while (window.isOpen()) {
        // ---- SFML events ----
        sf::Event event;
        while (window.pollEvent(event)) {
            switch (event.type) {
            case sf::Event::Closed:
                window.close();
                break;

            case sf::Event::KeyPressed:
                switch (event.key.code) {
                case sf::Keyboard::Escape:
                    window.close();
                    break;
                case sf::Keyboard::R:
                    controller.reset();
                    aiThinking = false;
                    aiMove     = -2;
                    valueHistory.clear();
                    illegalMsg = false;
                    break;
                case sf::Keyboard::H:
                    showHeatmap = !showHeatmap;
                    break;
                case sf::Keyboard::D:
                    debugMode = !debugMode;
                    break;
                default:
                    break;
                }
                break;

            case sf::Event::MouseButtonPressed:
                if (event.mouseButton.button == sf::Mouse::Left
                    && !aiThinking
                    && !controller.isGameOver()
                    && controller.currentPlayer() == Player::BLACK)
                {
                    int mx = event.mouseButton.x;
                    int my = event.mouseButton.y;
                    if (mx < BOARD_AREA_W && my < BOARD_AREA_W) {
                        int idx = BoardRenderer::pixelToIndex((float)mx, (float)my);
                        if (idx >= 0) {
                            if (!controller.isLegalMove(idx)) {
                                illegalMsg   = true;
                                illegalTimer = 90;  // ~1.5 s at 60 fps
                            } else {
                                illegalMsg = false;
                                controller.playHuman(idx);
                                if (!controller.isGameOver()) {
                                    // Launch AI in background thread
                                    aiThinking = true;
                                    aiMove     = -2;
                                    std::thread([&controller, &aiMove, &aiThinking,
                                                 &valueHistory]() {
                                        // Only compute — does NOT modify board
                                        int m = controller.computeAIMove();
                                        aiMove.store(m);
                                        aiThinking = false;
                                        float v = controller.getSearchInfo().value;
                                        valueHistory.push_back(v);
                                        while ((int)valueHistory.size() > VALUE_HIST_N)
                                            valueHistory.pop_front();
                                    }).detach();
                                }
                            }
                        }
                    }
                }
                break;

            case sf::Event::MouseMoved:
                if (event.mouseMove.x < BOARD_AREA_W
                    && event.mouseMove.y < BOARD_AREA_W) {
                    hoverIdx = BoardRenderer::pixelToIndex(
                        (float)event.mouseMove.x, (float)event.mouseMove.y);
                    hoverLegal = (hoverIdx >= 0) && controller.isLegalMove(hoverIdx);
                } else {
                    hoverIdx   = -1;
                    hoverLegal = false;
                }
                break;

            default:
                break;
            }
        }

        // ---- Illegal-message timer ----
        if (illegalMsg && --illegalTimer <= 0)
            illegalMsg = false;

        // ---- AI result (apply on main thread to avoid race) ----
        if (!aiThinking && aiMove.load() >= -1) {
            int m = aiMove.exchange(-2);   // consume the result
            if (m >= 0 || m == -1) {
                controller.applyAIMove(m);
            }
        }

        // ---- Render ----
        window.clear(sf::Color(30, 30, 35));

        // Heatmap
        const float* heatmap = nullptr;
        static float hm[81];
        if (showHeatmap) {
            const auto& si = controller.getSearchInfo();
            for (int i = 0; i < 81; ++i)
                hm[i] = (float)si.visits[i];
            heatmap = hm;
        }

        renderer.draw(window, controller.getBoard(),
                      controller.getLastStep(),
                      hoverIdx, hoverLegal,
                      heatmap);
        renderer.drawPanelBackground(window);

        drawInfoPanel(window, controller,
                      aiThinking.load(),
                      illegalMsg,
                      valueHistory,
                      debugMode);

        window.display();
    }

    return 0;
}
