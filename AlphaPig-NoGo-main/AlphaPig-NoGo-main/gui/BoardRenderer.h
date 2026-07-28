#pragma once

#include <SFML/Graphics.hpp>
#include <string>

// ============================================================
// BoardRenderer — draws the 9×9 NoGo board and stones.
// ============================================================

// Board constants
inline constexpr int   BOARD_SIZE   = 9;
inline constexpr int   CELL_SIZE    = 70;
inline constexpr int   OFFSET       = 50;
inline constexpr float STONE_RADIUS = CELL_SIZE * 0.35f;

// Board drawing area: OFFSET + (BOARD_SIZE-1)*CELL_SIZE + OFFSET  → 660 px
// Panel starts at BOARD_PANEL_X
inline constexpr int   BOARD_AREA_W = OFFSET + (BOARD_SIZE - 1) * CELL_SIZE + OFFSET; // 660
inline constexpr int   BOARD_AREA_H = BOARD_AREA_W;

enum Stone : int
{
    EMPTY = 0,
    BLACK = 1,
    WHITE = -1,
};

class BoardRenderer
{
public:
    BoardRenderer();

    /// Convert board index [0..80] to pixel position
    static sf::Vector2f indexToPixel(int idx);

    /// Convert pixel position to board index; returns -1 if out of bounds
    static int pixelToIndex(float px, float py);

    /// Draw the full board (grid + stones + highlights) onto `window`
    void draw(sf::RenderWindow& window,
              const int* board,
              int lastStep          = -1,
              int hoverIdx          = -1,
              bool hoverLegal       = false,
              const float* heatmap  = nullptr);

    /// Render the right-side info panel (text already drawn by caller; this
    /// just draws the background separator line).
    void drawPanelBackground(sf::RenderWindow& window);

private:
    void drawGrid(sf::RenderWindow& window);
    void drawStones(sf::RenderWindow& window, const int* board);
    void drawStarPoints(sf::RenderWindow& window);
    void drawLastStep(sf::RenderWindow& window);
    void drawHover(sf::RenderWindow& window, int idx, bool legal);
    void drawHeatmap(sf::RenderWindow& window, const float* heatmap);

    // Star points for 9×9 (standard positions)
    static constexpr int STAR_POINTS[][2] = {
        {2,2}, {2,6}, {4,4}, {6,2}, {6,6}
    };

    sf::CircleShape m_blackStone;
    sf::CircleShape m_whiteStone;
    sf::CircleShape m_lastMarker;   // small dot inside last stone
    sf::RectangleShape m_gridLine;
    sf::CircleShape m_starPoint;
    sf::CircleShape m_hoverHint;
    sf::RectangleShape m_heatmapTile;
    sf::RectangleShape m_panelSep;
    sf::CircleShape    m_illegalOverlay;

    int m_lastStep = -1;
    int m_hoverIdx = -1;
    bool m_hoverLegal = false;
    const float* m_heatmap = nullptr;
};
