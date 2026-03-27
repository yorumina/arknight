// ─────────────────────────────────────────────────────────────────────────────
// Camera.cpp  –  Projection & coordinate mapping
// ─────────────────────────────────────────────────────────────────────────────
#include "App.hpp"
#include "config.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

using namespace Ark;

// ── Coordinate helpers ───────────────────────────────────────────────────────

// 60-degree top-down oblique view (grid parallel to camera axis):
//   screenX = ptsdX + W/2              (X unchanged)
//   screenY = H/2 - ptsdY * cos(60°)   (Y foreshortened)
ImVec2 App::ToScreenPosition(const glm::vec2& p) const {
    const float W = static_cast<float>(PTSD_Config::WINDOW_WIDTH);
    const float H = static_cast<float>(PTSD_Config::WINDOW_HEIGHT);
    return { p.x + W * 0.5F,
             H * 0.5F - p.y * PERSPECTIVE_Y_SCALE };
}

Ark::BoardLayout App::GetBoardLayout() const {
    const float W = static_cast<float>(PTSD_Config::WINDOW_WIDTH);
    const float H = static_cast<float>(PTSD_Config::WINDOW_HEIGHT);

    const float hudRW  = 362.0F, mL = 24, mR = 22, mT = 46, mB = 28;
    const float availW = std::max(220.0F, W - hudRW - mL - mR);
    const float availH = std::max(220.0F, H - mT - mB);

    const float cols = static_cast<float>(std::max(1, m_StageWidth));
    const float rows = static_cast<float>(std::max(1, m_StageHeight));

    // For orthogonal, the board is simply cols x rows in size.
    const float cellSizeRaw = std::floor(std::min(availW / cols, availH / rows));

    BoardLayout layout;
    layout.cellSize = std::clamp(cellSizeRaw, 24.0F, 96.0F);

    // Center the board in ptsd space
    layout.topLeftX = -(cols * 0.5F) * layout.cellSize;
    layout.topLeftY =  (rows * 0.5F) * layout.cellSize;

    return layout;
}

glm::vec2 App::ToPtsdPosition(const glm::vec2& boardPos) const {
    const auto layout = GetBoardLayout();
    return {layout.topLeftX + boardPos.x * layout.cellSize,
            layout.topLeftY - boardPos.y * layout.cellSize};
}

glm::vec2 App::ToBoardCenter(const glm::ivec2& cell) const {
    return {static_cast<float>(cell.x) + 0.5F, static_cast<float>(cell.y) + 0.5F};
}

std::optional<glm::ivec2> App::ToCell(const glm::vec2& p) const {
    if (!IsInsideBoard(p)) return std::nullopt;
    const auto layout = GetBoardLayout();
    const int col = static_cast<int>((p.x - layout.topLeftX) / layout.cellSize);
    const int row = static_cast<int>((layout.topLeftY - p.y) / layout.cellSize);
    if (col < 0 || col >= m_StageWidth || row < 0 || row >= m_StageHeight) return std::nullopt;
    return glm::ivec2(col, row);
}

bool App::IsInsideBoard(const glm::vec2& p) const {
    const auto layout = GetBoardLayout();
    return p.x >= layout.topLeftX && p.x < layout.topLeftX + m_StageWidth * layout.cellSize &&
           p.y <= layout.topLeftY && p.y > layout.topLeftY - m_StageHeight * layout.cellSize;
}
