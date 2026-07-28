#include "BoardRenderer.h"

#include <cmath>
#include <algorithm>

// ============================================================
// Constructor — pre-configure all SFML shapes
// ============================================================
BoardRenderer::BoardRenderer()
{
    // ---- Stone shapes ----
    m_blackStone.setRadius(STONE_RADIUS);
    m_blackStone.setFillColor(sf::Color(20, 20, 20));
    m_blackStone.setOutlineColor(sf::Color(80, 80, 80));
    m_blackStone.setOutlineThickness(1.5f);
    m_blackStone.setOrigin(STONE_RADIUS, STONE_RADIUS);

    m_whiteStone.setRadius(STONE_RADIUS);
    m_whiteStone.setFillColor(sf::Color(235, 235, 235));
    m_whiteStone.setOutlineColor(sf::Color(100, 100, 100));
    m_whiteStone.setOutlineThickness(1.5f);
    m_whiteStone.setOrigin(STONE_RADIUS, STONE_RADIUS);

    // ---- Last-step marker (red ring around the stone) ----
    float markerR = STONE_RADIUS + 3.f;
    m_lastMarker.setRadius(markerR);
    m_lastMarker.setFillColor(sf::Color::Transparent);
    m_lastMarker.setOutlineColor(sf::Color(255, 50, 50));
    m_lastMarker.setOutlineThickness(3.f);
    m_lastMarker.setOrigin(markerR, markerR);

    // ---- Grid line ----
    m_gridLine.setSize(sf::Vector2f(
        (float)(CELL_SIZE * (BOARD_SIZE - 1)), 1.5f));
    m_gridLine.setFillColor(sf::Color(40, 40, 40));

    // ---- Star points ----
    m_starPoint.setRadius(4.f);
    m_starPoint.setFillColor(sf::Color(30, 30, 30));
    m_starPoint.setOrigin(4.f, 4.f);

    // ---- Hover hint (translucent stone preview) ----
    m_hoverHint.setRadius(STONE_RADIUS);
    m_hoverHint.setOrigin(STONE_RADIUS, STONE_RADIUS);
    m_hoverHint.setFillColor(sf::Color(0, 0, 0, 60));

    // ---- Heatmap tile (semi-transparent square per cell) ----
    m_heatmapTile.setSize(sf::Vector2f((float)CELL_SIZE, (float)CELL_SIZE));
    m_heatmapTile.setOrigin((float)CELL_SIZE / 2.f, (float)CELL_SIZE / 2.f);

    // ---- Panel separator ----
    m_panelSep.setSize(sf::Vector2f(2.f, (float)BOARD_AREA_H));
    m_panelSep.setFillColor(sf::Color(180, 180, 180));

    // ---- Illegal move overlay (red X mark on invalid hover) ----
    m_illegalOverlay.setRadius(STONE_RADIUS * 0.6f);
    m_illegalOverlay.setOrigin(STONE_RADIUS * 0.6f, STONE_RADIUS * 0.6f);
    m_illegalOverlay.setFillColor(sf::Color(255, 0, 0, 120));
}

// ============================================================
// Coordinate conversion
// ============================================================
sf::Vector2f BoardRenderer::indexToPixel(int idx)
{
    int row = idx / BOARD_SIZE;
    int col = idx % BOARD_SIZE;
    return sf::Vector2f(
        (float)(OFFSET + col * CELL_SIZE),
        (float)(OFFSET + row * CELL_SIZE));
}

int BoardRenderer::pixelToIndex(float px, float py)
{
    int col = static_cast<int>(std::round((px - (float)OFFSET) / (float)CELL_SIZE));
    int row = static_cast<int>(std::round((py - (float)OFFSET) / (float)CELL_SIZE));
    if (col < 0 || col >= BOARD_SIZE || row < 0 || row >= BOARD_SIZE)
        return -1;
    return row * BOARD_SIZE + col;
}

// ============================================================
// Master draw
// ============================================================
void BoardRenderer::draw(sf::RenderWindow& window,
                         const int* board,
                         int lastStep,
                         int hoverIdx,
                         bool hoverLegal,
                         const float* heatmap)
{
    m_lastStep   = lastStep;
    m_hoverIdx   = hoverIdx;
    m_hoverLegal = hoverLegal;
    m_heatmap    = heatmap;

    drawGrid(window);
    if (heatmap) drawHeatmap(window, heatmap);
    drawStarPoints(window);
    drawStones(window, board);
    if (lastStep >= 0) drawLastStep(window);
    if (hoverIdx >= 0) drawHover(window, hoverIdx, hoverLegal);
}

// ============================================================
// Draw 9x9 grid lines
// ============================================================
void BoardRenderer::drawGrid(sf::RenderWindow& window)
{
    // Board background
    sf::RectangleShape bg(sf::Vector2f((float)BOARD_AREA_W, (float)BOARD_AREA_H));
    bg.setPosition(0.f, 0.f);
    bg.setFillColor(sf::Color(220, 180, 120));
    window.draw(bg);

    sf::Color lineColor(40, 40, 40);

    // Horizontal lines
    for (int i = 0; i < BOARD_SIZE; ++i) {
        sf::RectangleShape line(sf::Vector2f(
            (float)(CELL_SIZE * (BOARD_SIZE - 1)), 1.5f));
        line.setFillColor(lineColor);
        line.setPosition((float)OFFSET, (float)(OFFSET + i * CELL_SIZE));
        window.draw(line);
    }

    // Vertical lines
    for (int i = 0; i < BOARD_SIZE; ++i) {
        sf::RectangleShape line(sf::Vector2f(
            1.5f, (float)(CELL_SIZE * (BOARD_SIZE - 1))));
        line.setFillColor(lineColor);
        line.setPosition((float)(OFFSET + i * CELL_SIZE), (float)OFFSET);
        window.draw(line);
    }

    // Outer border (thicker)
    float borderW = (float)(CELL_SIZE * (BOARD_SIZE - 1) + 4);
    sf::RectangleShape border(sf::Vector2f(borderW, borderW));
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineColor(sf::Color(20, 20, 20));
    border.setOutlineThickness(3.f);
    border.setPosition((float)(OFFSET - 2), (float)(OFFSET - 2));
    window.draw(border);
}

// ============================================================
// Draw stones
// ============================================================
void BoardRenderer::drawStones(sf::RenderWindow& window, const int* board)
{
    for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; ++i) {
        if (board[i] == Stone::EMPTY) continue;

        sf::Vector2f pos = indexToPixel(i);

        if (board[i] == Stone::BLACK) {
            m_blackStone.setPosition(pos);
            window.draw(m_blackStone);
        } else {
            m_whiteStone.setPosition(pos);
            window.draw(m_whiteStone);
        }
    }
}

// ============================================================
// Star points (hoshi)
// ============================================================
void BoardRenderer::drawStarPoints(sf::RenderWindow& window)
{
    for (auto& pt : STAR_POINTS) {
        int row = pt[0], col = pt[1];
        m_starPoint.setPosition(
            (float)(OFFSET + col * CELL_SIZE),
            (float)(OFFSET + row * CELL_SIZE));
        window.draw(m_starPoint);
    }
}

// ============================================================
// Last-step marker
// ============================================================
void BoardRenderer::drawLastStep(sf::RenderWindow& window)
{
    sf::Vector2f pos = indexToPixel(m_lastStep);
    m_lastMarker.setPosition(pos);
    window.draw(m_lastMarker);
}

// ============================================================
// Hover preview
// ============================================================
void BoardRenderer::drawHover(sf::RenderWindow& window, int idx, bool legal)
{
    sf::Vector2f pos = indexToPixel(idx);

    if (legal) {
        m_hoverHint.setPosition(pos);
        m_hoverHint.setFillColor(sf::Color(0, 120, 0, 80));
        window.draw(m_hoverHint);
    } else {
        m_illegalOverlay.setPosition(pos);
        window.draw(m_illegalOverlay);
    }
}

// ============================================================
// MCTS visit heatmap (Blue→Green→Yellow→Red)
// ============================================================
void BoardRenderer::drawHeatmap(sf::RenderWindow& window, const float* heatmap)
{
    // Find max for normalization
    float maxVal = 0.f;
    for (int i = 0; i < 81; ++i)
        if (heatmap[i] > maxVal) maxVal = heatmap[i];
    if (maxVal <= 0.f) return;

    for (int i = 0; i < 81; ++i) {
        if (heatmap[i] <= 0.f) continue;
        float t = heatmap[i] / maxVal;
        t = std::min(t, 1.f);

        // Blue (cold) -> Green -> Yellow -> Red (hot)
        sf::Color col;
        if (t < 0.5f) {
            float s = t * 2.f;
            col = sf::Color(
                static_cast<sf::Uint8>(255.f * s),
                static_cast<sf::Uint8>(100.f + 155.f * s),
                static_cast<sf::Uint8>(255.f * (1.f - s)),
                120);
        } else {
            float s = (t - 0.5f) * 2.f;
            col = sf::Color(
                255,
                static_cast<sf::Uint8>(255.f * (1.f - s)),
                0,
                120);
        }

        sf::Vector2f pos = indexToPixel(i);
        m_heatmapTile.setPosition(pos);
        m_heatmapTile.setFillColor(col);
        window.draw(m_heatmapTile);
    }
}

// ============================================================
// Panel separator line
// ============================================================
void BoardRenderer::drawPanelBackground(sf::RenderWindow& window)
{
    m_panelSep.setPosition((float)BOARD_AREA_W, 0.f);
    window.draw(m_panelSep);
}
