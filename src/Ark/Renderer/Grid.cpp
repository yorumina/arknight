#include "App.hpp"
#include "Ark/Renderer.hpp"
#include "RendererShared.hpp"

using namespace Ark;
using namespace Ark::RendererConst;

void Ark::AppRenderer::DrawGrid() {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const auto layout = m_App.GetBoardLayout();
    const float pad = layout.cellSize * 0.03F;
    const float highgroundOffset = layout.cellSize * 0.22F;
    const float markerOffset = layout.cellSize * 0.5F;

    for (int row = 0; row < m_App.m_StageHeight; ++row) {
        for (int col = 0; col < m_App.m_StageWidth; ++col) {
            const auto tile = m_App.m_TileMap[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)];
            if (tile == TileType::EMPTY) continue;

            const float l = layout.topLeftX + col * layout.cellSize + pad;
            const float r = l + layout.cellSize - 2.0F * pad;
            const float t = layout.topLeftY - row * layout.cellSize - pad;
            const float b = t - layout.cellSize + 2.0F * pad;

            const ImVec2 q1 = m_App.ToScreenPosition({l, t});
            const ImVec2 q2 = m_App.ToScreenPosition({r, t});
            const ImVec2 q3 = m_App.ToScreenPosition({r, b});
            const ImVec2 q4 = m_App.ToScreenPosition({l, b});

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
                draw->AddQuadFilled(q1, q2, q3, q4, COLOR_LOW_TOP);
            } else {
                draw->AddQuadFilled(q1, q2, q3, q4, COLOR_LOW_TOP);
            }

            if (tile == TileType::HIGHGROUND) {
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
    const float pad = layout.cellSize * 0.03F;
    const float markerOffset = layout.cellSize * 0.5F;

    for (int row = 0; row < m_App.m_StageHeight; ++row) {
        for (int col = 0; col < m_App.m_StageWidth; ++col) {
            const auto tile = m_App.m_TileMap[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)];
            if (tile != TileType::SPAWN && tile != TileType::GOAL) continue;

            const float l = layout.topLeftX + col * layout.cellSize + pad;
            const float r = l + layout.cellSize - 2.0F * pad;
            const float t = layout.topLeftY - row * layout.cellSize - pad;
            const float b = t - layout.cellSize + 2.0F * pad;

            const ImVec2 q1 = m_App.ToScreenPosition({l, t});
            const ImVec2 q2 = m_App.ToScreenPosition({r, t});
            const ImVec2 q3 = m_App.ToScreenPosition({r, b});
            const ImVec2 q4 = m_App.ToScreenPosition({l, b});
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
    const float pad = layout.cellSize * 0.03F;
    const float highgroundOffset = layout.cellSize * 0.22F;

    for (int row = 0; row < m_App.m_StageHeight; ++row) {
        for (int col = 0; col < m_App.m_StageWidth; ++col) {
            const auto tile = m_App.m_TileMap[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)];
            if (tile != TileType::HIGHGROUND) continue;

            const float l = layout.topLeftX + col * layout.cellSize + pad;
            const float r = l + layout.cellSize - 2.0F * pad;
            const float t = layout.topLeftY - row * layout.cellSize - pad;
            const float b = t - layout.cellSize + 2.0F * pad;

            const ImVec2 q1 = m_App.ToScreenPosition({l, t});
            const ImVec2 q2 = m_App.ToScreenPosition({r, t});
            const ImVec2 q3 = m_App.ToScreenPosition({r, b});
            const ImVec2 q4 = m_App.ToScreenPosition({l, b});
            const ImVec2 u1{q1.x, q1.y - highgroundOffset};
            const ImVec2 u2{q2.x, q2.y - highgroundOffset};
            const ImVec2 u3{q3.x, q3.y - highgroundOffset};
            const ImVec2 u4{q4.x, q4.y - highgroundOffset};

            draw->AddQuadFilled(u1, u2, u3, u4, COLOR_HIGH_TOP);
            draw->AddQuad(u1, u2, u3, u4, COLOR_GRID_WHITE_BOLD, 1.45F);
        }
    }
}
