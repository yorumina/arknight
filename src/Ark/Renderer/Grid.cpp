#include "App.hpp"
#include "Ark/Renderer.hpp"
#include "RendererShared.hpp"

#include <algorithm>
#include <array>
#include <cmath>

using namespace Ark;
using namespace Ark::RendererConst;

namespace {
ImVec2 Lerp(const ImVec2& a, const ImVec2& b, float t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

ImVec2 QuadPoint(const ImVec2& q1, const ImVec2& q2, const ImVec2& q3, const ImVec2& q4,
                 float u, float v) {
    return Lerp(Lerp(q1, q2, u), Lerp(q4, q3, u), v);
}

float QuadWidth(const ImVec2& q1, const ImVec2& q2) {
    const float dx = q2.x - q1.x;
    const float dy = q2.y - q1.y;
    return std::sqrt(dx * dx + dy * dy);
}

struct TilePalette {
    ImU32 base = IM_COL32(86, 92, 102, 255);
    ImU32 panel = IM_COL32(118, 126, 140, 255);
    ImU32 edge = IM_COL32(207, 216, 229, 115);
    ImU32 accent = IM_COL32(210, 218, 230, 140);
};

bool IsHighgroundVisual(TileType tile) {
    return tile == TileType::HIGHGROUND || tile == TileType::UNUSABLE_HIGHGROUND;
}

TilePalette PaletteForTile(TileType tile, bool hasStageArt) {
    const int aBase = hasStageArt ? 150 : 255;
    const int aPanel = hasStageArt ? 116 : 255;
    const int aEdge = hasStageArt ? 125 : 155;

    switch (tile) {
    case TileType::ROAD:
        return {
            IM_COL32(42, 48, 56, aBase),
            IM_COL32(68, 76, 86, aPanel),
            IM_COL32(184, 194, 208, aEdge),
            IM_COL32(205, 212, 222, hasStageArt ? 110 : 165)
        };
    case TileType::HIGHGROUND:
    case TileType::UNUSABLE_HIGHGROUND:
        return {
            IM_COL32(118, 125, 142, hasStageArt ? 168 : 255),
            IM_COL32(173, 182, 206, hasStageArt ? 138 : 255),
            IM_COL32(235, 240, 255, hasStageArt ? 155 : 220),
            IM_COL32(110, 122, 150, hasStageArt ? 125 : 180)
        };
    case TileType::SPAWN:
        return {
            IM_COL32(55, 44, 48, hasStageArt ? 145 : 255),
            IM_COL32(91, 67, 68, hasStageArt ? 112 : 245),
            IM_COL32(236, 86, 86, hasStageArt ? 150 : 205),
            IM_COL32(255, 90, 90, hasStageArt ? 145 : 210)
        };
    case TileType::GOAL:
        return {
            IM_COL32(38, 51, 68, hasStageArt ? 145 : 255),
            IM_COL32(55, 82, 112, hasStageArt ? 112 : 245),
            IM_COL32(92, 170, 255, hasStageArt ? 150 : 205),
            IM_COL32(85, 210, 255, hasStageArt ? 145 : 210)
        };
    case TileType::GROUND:
    default:
        return {
            IM_COL32(68, 74, 83, aBase),
            IM_COL32(104, 112, 126, aPanel),
            IM_COL32(203, 212, 226, aEdge),
            IM_COL32(140, 152, 170, hasStageArt ? 96 : 138)
        };
    }
}

void DrawRivets(ImDrawList* draw, const ImVec2& q1, const ImVec2& q2, const ImVec2& q3, const ImVec2& q4,
                ImU32 color, float radius) {
    draw->AddCircleFilled(QuadPoint(q1, q2, q3, q4, 0.13F, 0.13F), radius, color, 10);
    draw->AddCircleFilled(QuadPoint(q1, q2, q3, q4, 0.87F, 0.13F), radius, color, 10);
    draw->AddCircleFilled(QuadPoint(q1, q2, q3, q4, 0.87F, 0.87F), radius, color, 10);
    draw->AddCircleFilled(QuadPoint(q1, q2, q3, q4, 0.13F, 0.87F), radius, color, 10);
}

void DrawTileSurface(ImDrawList* draw, const ImVec2& q1, const ImVec2& q2, const ImVec2& q3, const ImVec2& q4,
                     TileType tile, int row, int col, bool hasStageArt, bool elevatedTop) {
    const TilePalette palette = PaletteForTile(tile, hasStageArt);
    const float w = std::max(1.0F, QuadWidth(q1, q2));
    const float lineW = std::clamp(w * 0.012F, 0.8F, 1.8F);
    const float inset = elevatedTop ? 0.08F : 0.10F;

    draw->AddQuadFilled(q1, q2, q3, q4, palette.base);

    const ImVec2 i1 = QuadPoint(q1, q2, q3, q4, inset, inset);
    const ImVec2 i2 = QuadPoint(q1, q2, q3, q4, 1.0F - inset, inset);
    const ImVec2 i3 = QuadPoint(q1, q2, q3, q4, 1.0F - inset, 1.0F - inset);
    const ImVec2 i4 = QuadPoint(q1, q2, q3, q4, inset, 1.0F - inset);
    draw->AddQuadFilled(i1, i2, i3, i4, palette.panel);
    draw->AddQuad(i1, i2, i3, i4, palette.edge, lineW);

    if (((row + col) & 1) == 0) {
        draw->AddLine(QuadPoint(q1, q2, q3, q4, 0.08F, 0.92F),
                      QuadPoint(q1, q2, q3, q4, 0.92F, 0.08F),
                      IM_COL32(255, 255, 255, hasStageArt ? 24 : 34), lineW);
    } else {
        draw->AddLine(QuadPoint(q1, q2, q3, q4, 0.08F, 0.08F),
                      QuadPoint(q1, q2, q3, q4, 0.92F, 0.92F),
                      IM_COL32(0, 0, 0, hasStageArt ? 26 : 40), lineW);
    }

    if (tile == TileType::ROAD) {
        const ImVec2 a = QuadPoint(q1, q2, q3, q4, 0.22F, 0.50F);
        const ImVec2 b = QuadPoint(q1, q2, q3, q4, 0.78F, 0.50F);
        draw->AddLine(a, b, palette.accent, std::clamp(w * 0.025F, 1.0F, 3.0F));
    }

    if (tile == TileType::SPAWN || tile == TileType::GOAL) {
        draw->AddQuad(i1, i2, i3, i4, palette.accent, std::clamp(w * 0.020F, 1.3F, 3.0F));
    } else {
        DrawRivets(draw, q1, q2, q3, q4, IM_COL32(24, 29, 36, hasStageArt ? 90 : 135),
                   std::clamp(w * 0.020F, 1.2F, 2.5F));
    }

    draw->AddQuad(q1, q2, q3, q4, palette.edge, lineW);
}

void DrawTileImage(ImDrawList* draw, GLuint textureId,
                   const ImVec2& q1, const ImVec2& q2, const ImVec2& q3, const ImVec2& q4,
                   bool hasStageArt) {
    draw->AddImageQuad(
        reinterpret_cast<void*>(static_cast<intptr_t>(textureId)),
        q1, q2, q3, q4,
        {0.0F, 0.0F}, {1.0F, 0.0F}, {1.0F, 1.0F}, {0.0F, 1.0F},
        IM_COL32(255, 255, 255, hasStageArt ? 218 : 255)
    );
    draw->AddQuad(q1, q2, q3, q4, IM_COL32(235, 242, 255, hasStageArt ? 92 : 132), 1.1F);
}
} // namespace

std::shared_ptr<Util::Image> Ark::AppRenderer::GetTileImage(const std::string& imagePath) {
    if (imagePath.empty()) return nullptr;
    auto it = m_App.m_TileImageCache.find(imagePath);
    if (it != m_App.m_TileImageCache.end()) return it->second;

    auto image = std::make_shared<Util::Image>(imagePath);
    m_App.m_TileImageCache[imagePath] = image;
    return image;
}

void Ark::AppRenderer::DrawGrid() {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const auto layout = m_App.GetBoardLayout();
    const bool artMode = m_App.UsesBoardArtTransform();
    const float pad = layout.cellSize * 0.03F;
    const float highgroundOffset = layout.cellSize * 0.22F;
    const float markerOffset = layout.cellSize * 0.5F;
    const bool hasStageArt = !m_App.m_StageBackgroundPath.empty();

    for (int row = 0; row < m_App.m_StageHeight; ++row) {
        for (int col = 0; col < m_App.m_StageWidth; ++col) {
            const auto tile = m_App.m_TileMap[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)];
            if (tile == TileType::EMPTY) continue;
            const glm::ivec2 cell{col, row};
            if (artMode && !m_App.IsBoardArtCellMapped(cell)) continue;

            const auto q = artMode
                ? m_App.GetCellQuad(cell)
                : std::array<ImVec2, 4>{
                    m_App.ToScreenPosition({layout.topLeftX + col * layout.cellSize + pad,
                                            layout.topLeftY - row * layout.cellSize - pad}),
                    m_App.ToScreenPosition({layout.topLeftX + (col + 1) * layout.cellSize - pad,
                                            layout.topLeftY - row * layout.cellSize - pad}),
                    m_App.ToScreenPosition({layout.topLeftX + (col + 1) * layout.cellSize - pad,
                                            layout.topLeftY - (row + 1) * layout.cellSize + pad}),
                    m_App.ToScreenPosition({layout.topLeftX + col * layout.cellSize + pad,
                                            layout.topLeftY - (row + 1) * layout.cellSize + pad}),
                };
            const ImVec2 q1 = q[0];
            const ImVec2 q2 = q[1];
            const ImVec2 q3 = q[2];
            const ImVec2 q4 = q[3];
            const bool hasTileImagePath =
                static_cast<std::size_t>(row) < m_App.m_TileImageMap.size() &&
                static_cast<std::size_t>(col) < m_App.m_TileImageMap[static_cast<std::size_t>(row)].size() &&
                !m_App.m_TileImageMap[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)].empty();
            const std::shared_ptr<Util::Image> tileImage = (hasTileImagePath && !IsHighgroundVisual(tile))
                ? GetTileImage(m_App.m_TileImageMap[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)])
                : nullptr;
            const GLuint tileTexture = tileImage ? tileImage->GetTextureId() : 0;
            auto drawBaseSurface = [&]() {
                if (tileTexture != 0) {
                    DrawTileImage(draw, tileTexture, q1, q2, q3, q4, hasStageArt);
                } else {
                    DrawTileSurface(draw, q1, q2, q3, q4, tile, row, col, hasStageArt, false);
                }
            };

            bool isSpecialMarker = (tile == TileType::SPAWN || tile == TileType::GOAL);
            if (isSpecialMarker) {
                const ImU32 markerSide = (tile == TileType::SPAWN) ? COLOR_SPAWN_SIDE : COLOR_GOAL_SIDE;
                const ImVec2 u1{q1.x, q1.y - markerOffset};
                const ImVec2 u2{q2.x, q2.y - markerOffset};
                const ImVec2 u3{q3.x, q3.y - markerOffset};
                const ImVec2 u4{q4.x, q4.y - markerOffset};

                // Draw all side faces so tint remains visible from any camera pan.
                draw->AddQuadFilled(u1, u2, q2, q1, markerSide);
                draw->AddQuadFilled(u2, u3, q3, q2, markerSide);
                draw->AddQuadFilled(u3, u4, q4, q3, markerSide);
                draw->AddQuadFilled(u4, u1, q1, q4, markerSide);
                draw->AddLine(q1, u1, COLOR_GRID_WHITE_MAIN, 1.0F);
                draw->AddLine(q2, u2, COLOR_GRID_WHITE_MAIN, 1.0F);
                draw->AddLine(q3, u3, COLOR_GRID_WHITE_MAIN, 1.0F);
                draw->AddLine(q4, u4, COLOR_GRID_WHITE_MAIN, 1.0F);
                drawBaseSurface();
            } else {
                drawBaseSurface();
            }

            if (IsHighgroundVisual(tile)) {
                const ImVec2 u1{q1.x, q1.y - highgroundOffset};
                const ImVec2 u2{q2.x, q2.y - highgroundOffset};
                const ImVec2 u3{q3.x, q3.y - highgroundOffset};
                const ImVec2 u4{q4.x, q4.y - highgroundOffset};

                // Side shadow faces for elevated tiles.
                draw->AddQuadFilled(u2, u3, q3, q2, COLOR_HIGH_SIDE);
                draw->AddQuadFilled(u3, u4, q4, q3, COLOR_HIGH_SIDE);
                draw->AddLine(q1, u1, COLOR_GRID_WHITE_MAIN, 1.1F);
                draw->AddLine(q2, u2, COLOR_GRID_WHITE_MAIN, 1.1F);
                draw->AddLine(q3, u3, COLOR_GRID_WHITE_MAIN, 1.1F);
                draw->AddLine(q4, u4, COLOR_GRID_WHITE_MAIN, 1.1F);
            }

            draw->AddQuad(q1, q2, q3, q4, COLOR_GRID_WHITE_MAIN, 1.2F);
        }
    }
}

void Ark::AppRenderer::DrawMarkerTopLayer() {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const auto layout = m_App.GetBoardLayout();
    const bool artMode = m_App.UsesBoardArtTransform();
    const float pad = layout.cellSize * 0.03F;
    const float markerOffset = layout.cellSize * 0.5F;

    for (int row = 0; row < m_App.m_StageHeight; ++row) {
        for (int col = 0; col < m_App.m_StageWidth; ++col) {
            const auto tile = m_App.m_TileMap[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)];
            if (tile != TileType::SPAWN && tile != TileType::GOAL) continue;
            const glm::ivec2 cell{col, row};
            if (artMode && !m_App.IsBoardArtCellMapped(cell)) continue;

            const auto q = artMode
                ? m_App.GetCellQuad(cell)
                : std::array<ImVec2, 4>{
                    m_App.ToScreenPosition({layout.topLeftX + col * layout.cellSize + pad,
                                            layout.topLeftY - row * layout.cellSize - pad}),
                    m_App.ToScreenPosition({layout.topLeftX + (col + 1) * layout.cellSize - pad,
                                            layout.topLeftY - row * layout.cellSize - pad}),
                    m_App.ToScreenPosition({layout.topLeftX + (col + 1) * layout.cellSize - pad,
                                            layout.topLeftY - (row + 1) * layout.cellSize + pad}),
                    m_App.ToScreenPosition({layout.topLeftX + col * layout.cellSize + pad,
                                            layout.topLeftY - (row + 1) * layout.cellSize + pad}),
                };
            const ImVec2 q1 = q[0];
            const ImVec2 q2 = q[1];
            const ImVec2 q3 = q[2];
            const ImVec2 q4 = q[3];
            const ImVec2 u1{q1.x, q1.y - markerOffset};
            const ImVec2 u2{q2.x, q2.y - markerOffset};
            const ImVec2 u3{q3.x, q3.y - markerOffset};
            const ImVec2 u4{q4.x, q4.y - markerOffset};

            const ImU32 markerTop = (tile == TileType::SPAWN) ? COLOR_SPAWN_TOP : COLOR_GOAL_TOP;
            draw->AddQuadFilled(u1, u2, u3, u4, markerTop);
            draw->AddQuad(u1, u2, u3, u4, COLOR_GRID_WHITE_BOLD, 1.3F);
            draw->AddLine(u1, u3, COLOR_GRID_WHITE_BOLD, 1.2F);
            draw->AddLine(u2, u4, COLOR_GRID_WHITE_BOLD, 1.2F);
        }
    }
}

void Ark::AppRenderer::DrawHighgroundTopLayer() {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const auto layout = m_App.GetBoardLayout();
    const bool artMode = m_App.UsesBoardArtTransform();
    const float pad = layout.cellSize * 0.03F;
    const float highgroundOffset = layout.cellSize * 0.22F;
    const bool hasStageArt = !m_App.m_StageBackgroundPath.empty();

    for (int row = 0; row < m_App.m_StageHeight; ++row) {
        for (int col = 0; col < m_App.m_StageWidth; ++col) {
            const auto tile = m_App.m_TileMap[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)];
            if (!IsHighgroundVisual(tile)) continue;
            const glm::ivec2 cell{col, row};
            if (artMode && !m_App.IsBoardArtCellMapped(cell)) continue;

            const auto q = artMode
                ? m_App.GetCellQuad(cell)
                : std::array<ImVec2, 4>{
                    m_App.ToScreenPosition({layout.topLeftX + col * layout.cellSize + pad,
                                            layout.topLeftY - row * layout.cellSize - pad}),
                    m_App.ToScreenPosition({layout.topLeftX + (col + 1) * layout.cellSize - pad,
                                            layout.topLeftY - row * layout.cellSize - pad}),
                    m_App.ToScreenPosition({layout.topLeftX + (col + 1) * layout.cellSize - pad,
                                            layout.topLeftY - (row + 1) * layout.cellSize + pad}),
                    m_App.ToScreenPosition({layout.topLeftX + col * layout.cellSize + pad,
                                            layout.topLeftY - (row + 1) * layout.cellSize + pad}),
                };
            const ImVec2 q1 = q[0];
            const ImVec2 q2 = q[1];
            const ImVec2 q3 = q[2];
            const ImVec2 q4 = q[3];
            const ImVec2 u1{q1.x, q1.y - highgroundOffset};
            const ImVec2 u2{q2.x, q2.y - highgroundOffset};
            const ImVec2 u3{q3.x, q3.y - highgroundOffset};
            const ImVec2 u4{q4.x, q4.y - highgroundOffset};
            const bool hasTileImagePath =
                static_cast<std::size_t>(row) < m_App.m_TileImageMap.size() &&
                static_cast<std::size_t>(col) < m_App.m_TileImageMap[static_cast<std::size_t>(row)].size() &&
                !m_App.m_TileImageMap[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)].empty();
            const std::shared_ptr<Util::Image> tileImage = hasTileImagePath
                ? GetTileImage(m_App.m_TileImageMap[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)])
                : nullptr;
            const GLuint tileTexture = tileImage ? tileImage->GetTextureId() : 0;

            if (tileTexture != 0) {
                DrawTileImage(draw, tileTexture, u1, u2, u3, u4, hasStageArt);
            } else {
                DrawTileSurface(draw, u1, u2, u3, u4, TileType::HIGHGROUND, row, col, hasStageArt, true);
            }
            draw->AddQuad(u1, u2, u3, u4, COLOR_GRID_WHITE_BOLD, 1.45F);
        }
    }
}
