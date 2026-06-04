#include "App.hpp"
#include "config.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

#include "Util/Input.hpp"
#include "Util/Keycode.hpp"

using namespace Ark;

namespace {
constexpr float CAMERA_EPSILON = 0.0001F;
constexpr float KEYBOARD_PAN_SPEED = 780.0F;

float Distance(const glm::vec2& a, const glm::vec2& b) {
    const glm::vec2 d = b - a;
    return std::sqrt(d.x * d.x + d.y * d.y);
}

float Cross(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
    const glm::vec2 ab = b - a;
    const glm::vec2 ac = c - a;
    return ab.x * ac.y - ab.y * ac.x;
}

bool PointInTriangle(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
    const float c1 = Cross(a, b, p);
    const float c2 = Cross(b, c, p);
    const float c3 = Cross(c, a, p);
    const bool hasNeg = c1 < -0.001F || c2 < -0.001F || c3 < -0.001F;
    const bool hasPos = c1 > 0.001F || c2 > 0.001F || c3 > 0.001F;
    return !(hasNeg && hasPos);
}

bool PointInQuad(const glm::vec2& p, const std::array<glm::vec2, 4>& q) {
    return PointInTriangle(p, q[0], q[1], q[2]) || PointInTriangle(p, q[0], q[2], q[3]);
}

glm::vec2 ScaleBoardArtPoint(const BoardArtTransform& art, const glm::vec2& point) {
    if (art.referenceSize.x > 0.0F && art.referenceSize.y > 0.0F) {
        const glm::vec2 scale{
            static_cast<float>(PTSD_Config::WINDOW_WIDTH) / art.referenceSize.x,
            static_cast<float>(PTSD_Config::WINDOW_HEIGHT) / art.referenceSize.y
        };
        return {point.x * scale.x, point.y * scale.y};
    }
    return point;
}

std::array<glm::vec2, 4> ScaledCorners(const BoardArtTransform& art, const std::array<glm::vec2, 4>& source) {
    std::array<glm::vec2, 4> corners = source;
    for (auto& corner : corners) {
        corner = ScaleBoardArtPoint(art, corner);
    }
    return corners;
}

std::array<glm::vec2, 4> ScaledBoardArtCorners(const BoardArtTransform& art) {
    return ScaledCorners(art, art.corners);
}

std::optional<std::array<glm::vec2, 4>> ScaledBoardArtCellCorners(const BoardArtTransform& art,
                                                                  const glm::ivec2& cell) {
    if (cell.y < 0 || cell.x < 0 ||
        static_cast<std::size_t>(cell.y) >= art.cells.size() ||
        static_cast<std::size_t>(cell.x) >= art.cells[static_cast<std::size_t>(cell.y)].size()) {
        return std::nullopt;
    }
    const BoardArtCell& artCell = art.cells[static_cast<std::size_t>(cell.y)][static_cast<std::size_t>(cell.x)];
    if (!artCell.enabled) return std::nullopt;
    return ScaledCorners(art, artCell.corners);
}

glm::vec2 QuadPoint(const std::array<glm::vec2, 4>& corners, float u, float v) {
    const glm::vec2 top = corners[0] + (corners[1] - corners[0]) * u;
    const glm::vec2 bottom = corners[3] + (corners[2] - corners[3]) * u;
    return top + (bottom - top) * v;
}

glm::vec2 BoardArtFallbackPoint(const BoardArtTransform& art, int width, int height, const glm::vec2& boardPos) {
    const auto corners = ScaledBoardArtCorners(art);
    const float u = std::clamp(boardPos.x / static_cast<float>(std::max(1, width)), 0.0F, 1.0F);
    const float v = std::clamp(boardPos.y / static_cast<float>(std::max(1, height)), 0.0F, 1.0F);
    return QuadPoint(corners, u, v);
}

glm::vec2 BoardArtPoint(const BoardArtTransform& art, int width, int height, const glm::vec2& boardPos) {
    if (!art.cells.empty()) {
        int x = static_cast<int>(std::floor(boardPos.x));
        int y = static_cast<int>(std::floor(boardPos.y));
        float u = boardPos.x - static_cast<float>(x);
        float v = boardPos.y - static_cast<float>(y);
        if (x >= width) {
            x = width - 1;
            u = 1.0F;
        }
        if (y >= height) {
            y = height - 1;
            v = 1.0F;
        }
        if (x >= 0 && y >= 0 && x < width && y < height) {
            if (const auto corners = ScaledBoardArtCellCorners(art, {x, y})) {
                return QuadPoint(*corners, std::clamp(u, 0.0F, 1.0F), std::clamp(v, 0.0F, 1.0F));
            }
        }
    }
    return BoardArtFallbackPoint(art, width, height, boardPos);
}
}

void App::ResetCameraToStageDefaults() {
    m_Camera.zoom = std::clamp(m_Camera.defaultZoom, m_Camera.minZoom, m_Camera.maxZoom);
    m_Camera.pan = m_Camera.defaultPan;
    m_Camera.dragging = false;
}

glm::vec2 App::RawCursorToPtsd(const glm::vec2& rawCursor) const {
    if (UsesBoardArtTransform()) {
        const float W = static_cast<float>(PTSD_Config::WINDOW_WIDTH);
        const float H = static_cast<float>(PTSD_Config::WINDOW_HEIGHT);
        return {rawCursor.x + W * 0.5F, H * 0.5F - rawCursor.y};
    }

    const float sx = std::max(CAMERA_EPSILON, m_Camera.projectionScaleX * m_Camera.zoom);
    const float sy = std::max(CAMERA_EPSILON, m_Camera.projectionScaleY * m_Camera.zoom);
    const float worldY = rawCursor.y / sy;
    const float worldX = rawCursor.x / sx - worldY * m_Camera.projectionSkewX;
    return {worldX - m_Camera.pan.x, worldY - m_Camera.pan.y};
}

void App::UpdateCameraControls(float deltaTimeMs, const glm::vec2& rawCursor) {
    if (UsesBoardArtTransform()) {
        m_Camera.dragging = false;
        return;
    }

    if (Util::Input::IsKeyDown(Util::Keycode::C)) {
        ResetCameraToStageDefaults();
    }

    const float dtSec = std::max(0.0F, deltaTimeMs) / 1000.0F;
    glm::vec2 panRawDelta{0.0F, 0.0F};
    if (Util::Input::IsKeyPressed(Util::Keycode::A) || Util::Input::IsKeyPressed(Util::Keycode::LEFT)) {
        panRawDelta.x -= KEYBOARD_PAN_SPEED * dtSec;
    }
    if (Util::Input::IsKeyPressed(Util::Keycode::D) || Util::Input::IsKeyPressed(Util::Keycode::RIGHT)) {
        panRawDelta.x += KEYBOARD_PAN_SPEED * dtSec;
    }
    if (Util::Input::IsKeyPressed(Util::Keycode::W) || Util::Input::IsKeyPressed(Util::Keycode::UP)) {
        panRawDelta.y += KEYBOARD_PAN_SPEED * dtSec;
    }
    if (Util::Input::IsKeyPressed(Util::Keycode::S) || Util::Input::IsKeyPressed(Util::Keycode::DOWN)) {
        panRawDelta.y -= KEYBOARD_PAN_SPEED * dtSec;
    }

    auto rawDeltaToWorld = [&](const glm::vec2& rawDelta) {
        const float sx = std::max(CAMERA_EPSILON, m_Camera.projectionScaleX * m_Camera.zoom);
        const float sy = std::max(CAMERA_EPSILON, m_Camera.projectionScaleY * m_Camera.zoom);
        const float worldY = rawDelta.y / sy;
        const float worldX = rawDelta.x / sx - worldY * m_Camera.projectionSkewX;
        return glm::vec2{worldX, worldY};
    };

    m_Camera.pan += rawDeltaToWorld(panRawDelta);

    if (Util::Input::IsKeyDown(Util::Keycode::MOUSE_MB)) {
        m_Camera.dragging = true;
        m_Camera.dragCursorStart = rawCursor;
        m_Camera.dragPanStart = m_Camera.pan;
    }
    if (m_Camera.dragging && Util::Input::IsKeyPressed(Util::Keycode::MOUSE_MB)) {
        const glm::vec2 rawDelta = rawCursor - m_Camera.dragCursorStart;
        m_Camera.pan = m_Camera.dragPanStart + rawDeltaToWorld(rawDelta);
    }
    if (Util::Input::IsKeyUp(Util::Keycode::MOUSE_MB)) {
        m_Camera.dragging = false;
    }

    // Intentionally ignore mouse wheel to prevent scroll-based map movement.
}

ImVec2 App::ToScreenPosition(const glm::vec2& p) const {
    if (UsesBoardArtTransform()) {
        return {p.x, p.y};
    }

    const float W = static_cast<float>(PTSD_Config::WINDOW_WIDTH);
    const float H = static_cast<float>(PTSD_Config::WINDOW_HEIGHT);
    const float sx = m_Camera.projectionScaleX * m_Camera.zoom;
    const float sy = m_Camera.projectionScaleY * m_Camera.zoom;
    const float worldX = p.x + m_Camera.pan.x;
    const float worldY = p.y + m_Camera.pan.y;
    return {W * 0.5F + (worldX + worldY * m_Camera.projectionSkewX) * sx,
            H * 0.5F - worldY * sy};
}

BoardLayout App::GetBoardLayout() const {
    if (UsesBoardArtTransform()) {
        const auto corners = ScaledBoardArtCorners(m_BoardArtTransform);
        const float cols = static_cast<float>(std::max(1, m_StageWidth));
        const float rows = static_cast<float>(std::max(1, m_StageHeight));
        const float topW = Distance(corners[0], corners[1]) / cols;
        const float bottomW = Distance(corners[3], corners[2]) / cols;
        const float leftH = Distance(corners[0], corners[3]) / rows;
        const float rightH = Distance(corners[1], corners[2]) / rows;
        BoardLayout layout;
        layout.cellSize = std::max(1.0F, (topW + bottomW + leftH + rightH) * 0.25F);
        layout.topLeftX = corners[0].x;
        layout.topLeftY = corners[0].y;
        return layout;
    }

    if (m_HasBoardLayoutOverride) {
        return m_BoardLayoutOverride;
    }

    const float W = static_cast<float>(PTSD_Config::WINDOW_WIDTH);
    const float H = static_cast<float>(PTSD_Config::WINDOW_HEIGHT);

    const float marginL = 24.0F;
    const float marginR = 22.0F;
    const float availW = std::max(220.0F, W - marginL - marginR);
    const float availH = std::max(220.0F, H);

    const float cols = static_cast<float>(std::max(1, m_StageWidth));
    const float rows = static_cast<float>(std::max(1, m_StageHeight));
    const float projX = std::max(CAMERA_EPSILON, m_Camera.projectionScaleX);
    const float projY = std::max(CAMERA_EPSILON, m_Camera.projectionScaleY);
    const float boardProjW = (cols + std::abs(m_Camera.projectionSkewX) * rows) * projX;
    const float boardProjH = rows * projY;
    const float cellSizeRaw = std::floor(std::min(availW / boardProjW, availH / boardProjH));

    BoardLayout layout;
    layout.cellSize = std::clamp(cellSizeRaw, 24.0F, 180.0F);
    layout.topLeftX = -(cols * 0.5F) * layout.cellSize;
    layout.topLeftY = (rows * 0.5F) * layout.cellSize;
    return layout;
}

glm::vec2 App::ToPtsdPosition(const glm::vec2& boardPos) const {
    if (UsesBoardArtTransform()) {
        return BoardArtPoint(m_BoardArtTransform, m_StageWidth, m_StageHeight, boardPos);
    }

    const auto layout = GetBoardLayout();
    return {layout.topLeftX + boardPos.x * layout.cellSize,
            layout.topLeftY - boardPos.y * layout.cellSize};
}

glm::vec2 App::ToBoardCenter(const glm::ivec2& cell) const {
    return {static_cast<float>(cell.x) + 0.5F, static_cast<float>(cell.y) + 0.5F};
}

std::optional<glm::ivec2> App::ToCell(const glm::vec2& p) const {
    if (UsesBoardArtTransform()) {
        for (int row = 0; row < m_StageHeight; ++row) {
            for (int col = 0; col < m_StageWidth; ++col) {
                if (!IsBoardArtCellMapped({col, row})) continue;
                const auto q = GetCellQuad({col, row});
                const std::array<glm::vec2, 4> quad{
                    glm::vec2{q[0].x, q[0].y},
                    glm::vec2{q[1].x, q[1].y},
                    glm::vec2{q[2].x, q[2].y},
                    glm::vec2{q[3].x, q[3].y}
                };
                if (PointInQuad(p, quad)) {
                    return glm::ivec2{col, row};
                }
            }
        }
        return std::nullopt;
    }

    if (!IsInsideBoard(p)) return std::nullopt;
    const auto layout = GetBoardLayout();
    const int col = static_cast<int>((p.x - layout.topLeftX) / layout.cellSize);
    const int row = static_cast<int>((layout.topLeftY - p.y) / layout.cellSize);
    if (col < 0 || col >= m_StageWidth || row < 0 || row >= m_StageHeight) return std::nullopt;
    return glm::ivec2(col, row);
}

std::optional<glm::ivec2> App::ResolveDeploymentCell(int typeIndex, const glm::vec2& ptsdPos) const {
    const auto direct = ToCell(ptsdPos);
    if (direct) {
        if (IsDeployableCellForOperatorType(typeIndex, *direct) && !IsCellOccupied(*direct)) {
            return direct;
        }
        return std::nullopt;
    }

    if (!UsesBoardArtTransform() || !IsValidOperatorTypeIndex(typeIndex)) return std::nullopt;

    const float threshold = std::max(18.0F, GetBoardCellScreenSize() * 0.48F);
    const float thresholdSq = threshold * threshold;
    float bestDistSq = thresholdSq;
    std::optional<glm::ivec2> best;

    for (int row = 0; row < m_StageHeight; ++row) {
        for (int col = 0; col < m_StageWidth; ++col) {
            const glm::ivec2 cell{col, row};
            if (!IsBoardArtCellMapped(cell)) continue;
            if (!IsDeployableCellForOperatorType(typeIndex, cell) || IsCellOccupied(cell)) continue;

            const ImVec2 center = ToScreenPosition(ToPtsdPosition(ToBoardCenter(cell)));
            const glm::vec2 diff{center.x - ptsdPos.x, center.y - ptsdPos.y};
            const float distSq = diff.x * diff.x + diff.y * diff.y;
            if (distSq <= bestDistSq) {
                bestDistSq = distSq;
                best = cell;
            }
        }
    }

    return best;
}

bool App::IsInsideBoard(const glm::vec2& p) const {
    if (UsesBoardArtTransform()) {
        const auto corners = ScaledBoardArtCorners(m_BoardArtTransform);
        return PointInQuad(p, corners);
    }

    const auto layout = GetBoardLayout();
    return p.x >= layout.topLeftX && p.x < layout.topLeftX + m_StageWidth * layout.cellSize &&
           p.y <= layout.topLeftY && p.y > layout.topLeftY - m_StageHeight * layout.cellSize;
}

bool App::UsesBoardArtTransform() const {
    return m_BoardArtTransform.enabled && m_StageWidth > 0 && m_StageHeight > 0;
}

bool App::IsBoardArtCellMapped(const glm::ivec2& cell) const {
    if (!UsesBoardArtTransform()) return false;
    if (m_BoardArtTransform.cells.empty()) return true;
    if (cell.x < 0 || cell.y < 0 ||
        static_cast<std::size_t>(cell.y) >= m_BoardArtTransform.cells.size() ||
        static_cast<std::size_t>(cell.x) >=
            m_BoardArtTransform.cells[static_cast<std::size_t>(cell.y)].size()) {
        return false;
    }
    return m_BoardArtTransform.cells[static_cast<std::size_t>(cell.y)]
                                    [static_cast<std::size_t>(cell.x)].enabled;
}

std::array<ImVec2, 4> App::GetCellQuad(const glm::ivec2& cell) const {
    if (UsesBoardArtTransform()) {
        if (const auto corners = ScaledBoardArtCellCorners(m_BoardArtTransform, cell)) {
            return {
                ImVec2{(*corners)[0].x, (*corners)[0].y},
                ImVec2{(*corners)[1].x, (*corners)[1].y},
                ImVec2{(*corners)[2].x, (*corners)[2].y},
                ImVec2{(*corners)[3].x, (*corners)[3].y}
            };
        }
    }

    const glm::vec2 tl{static_cast<float>(cell.x), static_cast<float>(cell.y)};
    const glm::vec2 tr{static_cast<float>(cell.x + 1), static_cast<float>(cell.y)};
    const glm::vec2 br{static_cast<float>(cell.x + 1), static_cast<float>(cell.y + 1)};
    const glm::vec2 bl{static_cast<float>(cell.x), static_cast<float>(cell.y + 1)};
    return {
        ToScreenPosition(ToPtsdPosition(tl)),
        ToScreenPosition(ToPtsdPosition(tr)),
        ToScreenPosition(ToPtsdPosition(br)),
        ToScreenPosition(ToPtsdPosition(bl))
    };
}

float App::GetBoardCellScreenSize() const {
    return GetBoardLayout().cellSize;
}
