#include "App.hpp"
#include "Ark/Renderer.hpp"
#include "RendererShared.hpp"

#include <cmath>

using namespace Ark;
using namespace Ark::RendererConst;

void Ark::AppRenderer::DrawDeployPreview(const glm::vec2& ptsdCursor, const std::optional<glm::ivec2>& hoverCell, const BoardLayout& layout) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();

    // Show grid highlight when dragging over valid cells
    if (m_App.m_DraggingFromBar && !m_App.m_WaitingForDirection && hoverCell && m_App.m_DragOperatorType >= 0) {
        int prevSelected = m_App.m_SelectedOperatorType;
        m_App.m_SelectedOperatorType = m_App.m_DragOperatorType;
        bool deployable = m_App.IsOperatorTypeAvailable(m_App.m_DragOperatorType) &&
                          m_App.IsDeployableCellForSelectedOperator(*hoverCell) && !m_App.IsCellOccupied(*hoverCell)
                          && static_cast<int>(m_App.m_Operators.size()) < MAX_OPS;
        const auto& opType = m_App.m_OperatorTemplates.at(static_cast<std::size_t>(m_App.m_DragOperatorType));
        bool affordable = m_App.m_DP >= static_cast<float>(opType.cost);
        m_App.m_SelectedOperatorType = prevSelected;

        ImU32 pv = (deployable && affordable) ? IM_COL32(89,225,167,90) : IM_COL32(225,89,89,90);

        float l = layout.topLeftX + hoverCell->x * layout.cellSize;
        float t = layout.topLeftY - hoverCell->y * layout.cellSize;
        float r = l + layout.cellSize, b = t - layout.cellSize;
        
        ImVec2 q1 = m_App.ToScreenPosition({l,t});
        ImVec2 q2 = m_App.ToScreenPosition({r,t});
        ImVec2 q3 = m_App.ToScreenPosition({r,b});
        ImVec2 q4 = m_App.ToScreenPosition({l,b});
        draw->AddQuadFilled(q1, q2, q3, q4, pv);
    }

    // Draw deploying operator model following cursor (during drag)
    if (m_App.m_DraggingFromBar && m_App.m_DragOperatorType >= 0 && !m_App.m_WaitingForDirection) {
        float l = layout.topLeftX + (hoverCell ? hoverCell->x : 0) * layout.cellSize;
        float t = layout.topLeftY - (hoverCell ? hoverCell->y : 0) * layout.cellSize;
        ImVec2 centerpos = hoverCell ? m_App.ToScreenPosition({l + layout.cellSize * 0.5F, t - layout.cellSize * 0.5F}) : ImVec2(m_App.m_DragScreenPos.x, m_App.m_DragScreenPos.y);

        GLuint thumbTex = 0;
        if (static_cast<std::size_t>(m_App.m_DragOperatorType) < m_App.m_OperatorThumbnails.size() &&
            m_App.m_OperatorThumbnails[static_cast<std::size_t>(m_App.m_DragOperatorType)]) {
            thumbTex = m_App.m_OperatorThumbnails[static_cast<std::size_t>(m_App.m_DragOperatorType)]->GetTextureId();
        }
        if (thumbTex != 0) {
            const float imgS = layout.cellSize * 0.8F;
            draw->AddImage(reinterpret_cast<void*>(static_cast<intptr_t>(thumbTex)),
                           {centerpos.x - imgS, centerpos.y - imgS},
                           {centerpos.x + imgS, centerpos.y + imgS});
        }
    }

    // Show direction selection highlighting (Image 1 and Image 2 styles)
    if (m_App.m_WaitingForDirection) {
        // Draw the placed operator model
        float l = layout.topLeftX + m_App.m_DirectionCell.x * layout.cellSize;
        float t = layout.topLeftY - m_App.m_DirectionCell.y * layout.cellSize;
        float r = l + layout.cellSize, b = t - layout.cellSize;
        
        ImVec2 q1 = m_App.ToScreenPosition({l,t});
        ImVec2 q2 = m_App.ToScreenPosition({r,t});
        ImVec2 q3 = m_App.ToScreenPosition({r,b});
        ImVec2 q4 = m_App.ToScreenPosition({l,b});
        
        // Image 1: White Diamond outline around the placed cell
        draw->AddQuad(q1, q2, q3, q4, IM_COL32(255,255,255,255), 4.0F);

        // Operator Sprite
        ImVec2 centerpos = m_App.ToScreenPosition({l + layout.cellSize * 0.5F, t - layout.cellSize * 0.5F});
        GLuint thumbTex = 0;
        if (static_cast<std::size_t>(m_App.m_DragOperatorType) < m_App.m_OperatorThumbnails.size() &&
            m_App.m_OperatorThumbnails[static_cast<std::size_t>(m_App.m_DragOperatorType)]) {
            thumbTex = m_App.m_OperatorThumbnails[static_cast<std::size_t>(m_App.m_DragOperatorType)]->GetTextureId();
        }
        if (thumbTex != 0) {
            const float imgS = layout.cellSize * 0.8F;
            draw->AddImage(reinterpret_cast<void*>(static_cast<intptr_t>(thumbTex)),
                           {centerpos.x - imgS, centerpos.y - imgS},
                           {centerpos.x + imgS, centerpos.y + imgS});
        }

        // Draw selection UI
        if (m_App.m_IsDirectionDragging) {
            const ImVec2 screenCursor = m_App.ToScreenPosition(ptsdCursor);
            // Check if within cancel distance
            glm::vec2 diff{
                screenCursor.x - m_App.m_DirectionDragStart.x,
                screenCursor.y - m_App.m_DirectionDragStart.y
            };
            bool hoveringCancel = (std::abs(diff.x) <= 25.0F && std::abs(diff.y) <= 25.0F);

            // Highlight the active drag direction (Image 2 yellow strip)
            if (!hoveringCancel && (diff.x != 0 || diff.y != 0)) {
                glm::ivec2 currDir{0, 0};
                if (std::abs(diff.x) > std::abs(diff.y)) currDir = diff.x > 0 ? glm::ivec2(1, 0) : glm::ivec2(-1, 0);
                else currDir = diff.y > 0 ? glm::ivec2(0, 1) : glm::ivec2(0, -1);
                
                glm::ivec2 hlCell = m_App.m_DirectionCell + currDir;
                float hll = layout.topLeftX + hlCell.x * layout.cellSize;
                float hlt = layout.topLeftY - hlCell.y * layout.cellSize;
                float hlr = hll + layout.cellSize, hlb = hlt - layout.cellSize;
                draw->AddQuadFilled(m_App.ToScreenPosition({hll,hlt}), m_App.ToScreenPosition({hlr,hlt}),
                                    m_App.ToScreenPosition({hlr,hlb}), m_App.ToScreenPosition({hll,hlb}),
                                    IM_COL32(255,160,0,180));
            }

            // Draw floating banner
            const char* bannerText = u8"拖回中心區域取消";
            const auto bannerSize = ImGui::CalcTextSize(bannerText);
            const float bPad = 8.0F;
            ImVec2 bPos{centerpos.x - bannerSize.x * 0.5F - 30.0F, centerpos.y - layout.cellSize - 20.0F};
            
            // Black polygon background for banner
            draw->AddRectFilled({bPos.x - bPad, bPos.y - bPad}, {bPos.x + bannerSize.x + bPad, bPos.y + bannerSize.y + bPad}, IM_COL32(0, 0, 0, 220), 2.0F);
            // little tail pointing down
            draw->AddTriangleFilled({bPos.x + bannerSize.x*0.5F - 5.0F, bPos.y + bannerSize.y + bPad},
                                    {bPos.x + bannerSize.x*0.5F + 5.0F, bPos.y + bannerSize.y + bPad},
                                    {bPos.x + bannerSize.x*0.5F, bPos.y + bannerSize.y + bPad + 10.0F}, IM_COL32(0, 0, 0, 220));
                                    
            draw->AddText(bPos, hoveringCancel ? IM_COL32(255, 50, 50, 255) : IM_COL32(255, 255, 255, 255), bannerText);
        } else {
            // Draw 4 small chevrons (Image 1 style) around center
            const float dist = layout.cellSize * 0.6F;
            const float s = 8.0F;
            // Up
            draw->AddLine({centerpos.x - s, centerpos.y - dist + s}, {centerpos.x, centerpos.y - dist}, IM_COL32(255,255,255,200), 3.0F);
            draw->AddLine({centerpos.x + s, centerpos.y - dist + s}, {centerpos.x, centerpos.y - dist}, IM_COL32(255,255,255,200), 3.0F);
            // Down
            draw->AddLine({centerpos.x - s, centerpos.y + dist - s}, {centerpos.x, centerpos.y + dist}, IM_COL32(255,255,255,200), 3.0F);
            draw->AddLine({centerpos.x + s, centerpos.y + dist - s}, {centerpos.x, centerpos.y + dist}, IM_COL32(255,255,255,200), 3.0F);
            // Left
            draw->AddLine({centerpos.x - dist + s, centerpos.y - s}, {centerpos.x - dist, centerpos.y}, IM_COL32(255,255,255,200), 3.0F);
            draw->AddLine({centerpos.x - dist + s, centerpos.y + s}, {centerpos.x - dist, centerpos.y}, IM_COL32(255,255,255,200), 3.0F);
            // Right
            draw->AddLine({centerpos.x + dist - s, centerpos.y - s}, {centerpos.x + dist, centerpos.y}, IM_COL32(255,255,255,200), 3.0F);
            draw->AddLine({centerpos.x + dist - s, centerpos.y + s}, {centerpos.x + dist, centerpos.y}, IM_COL32(255,255,255,200), 3.0F);
        }

        // Show attack range preview for the selected direction
        if (m_App.m_DragOperatorType >= 0) {
            const auto& st = m_App.m_OperatorTemplates.at(static_cast<std::size_t>(m_App.m_DragOperatorType));
            for (int dx = -5; dx <= 5; ++dx) for (int dy = -5; dy <= 5; ++dy) {
                glm::ivec2 rel{dx, dy};
                bool inR = false;
                if (rel.x == 0 && rel.y == 0) {
                    inR = true;
                } else if (st.deployType == DeployType::GROUND_ONLY) {
                    inR = rel == m_App.m_DeployingDirection;
                } else {
                    int fw = rel.x*m_App.m_DeployingDirection.x + rel.y*m_App.m_DeployingDirection.y;
                    int pp = rel.x*m_App.m_DeployingDirection.y - rel.y*m_App.m_DeployingDirection.x;
                    inR = (fw>=1&&fw<=4&&std::abs(pp)<=1);
                }
                if (!inR) continue;
                glm::ivec2 tc = m_App.m_DirectionCell + rel;
                if (tc.x<0||tc.x>=m_App.m_StageWidth||tc.y<0||tc.y>=m_App.m_StageHeight) continue;
                float rl = layout.topLeftX + tc.x * layout.cellSize;
                float rt = layout.topLeftY - tc.y * layout.cellSize;
                float rr = rl + layout.cellSize, rb = rt - layout.cellSize;

                ImVec2 rq1 = m_App.ToScreenPosition({rl,rt});
                ImVec2 rq2 = m_App.ToScreenPosition({rr,rt});
                ImVec2 rq3 = m_App.ToScreenPosition({rr,rb});
                ImVec2 rq4 = m_App.ToScreenPosition({rl,rb});
                draw->AddQuadFilled(rq1, rq2, rq3, rq4, IM_COL32(255,120,120,60));
                draw->AddQuad(rq1, rq2, rq3, rq4, IM_COL32(255,150,150,140), 1.0F);
            }
        }
    }

    // Show attack range for selected deployed operator
    if (m_App.m_SelectedOperatorId != -1 && !m_App.m_WaitingForDirection && !m_App.m_DraggingFromBar) {
        const Operator* selectedOp = nullptr;
        for (const auto& op : m_App.m_Operators) {
            if (op.id == m_App.m_SelectedOperatorId) {
                selectedOp = &op;
                break;
            }
        }
        if (selectedOp) {
            const auto& st = m_App.m_OperatorTemplates.at(static_cast<std::size_t>(selectedOp->typeIndex));
            for (int dx = -5; dx <= 5; ++dx) for (int dy = -5; dy <= 5; ++dy) {
                glm::ivec2 rel{dx, dy};
                bool inR = false;
                if (rel.x == 0 && rel.y == 0) {
                    inR = true;
                } else if (st.deployType == DeployType::GROUND_ONLY) {
                    inR = rel == selectedOp->direction;
                } else {
                    int fw = rel.x*selectedOp->direction.x + rel.y*selectedOp->direction.y;
                    int pp = rel.x*selectedOp->direction.y - rel.y*selectedOp->direction.x;
                    inR = (fw>=1&&fw<=4&&std::abs(pp)<=1);
                }
                if (!inR) continue;
                glm::ivec2 tc = selectedOp->cell + rel;
                if (tc.x<0||tc.x>=m_App.m_StageWidth||tc.y<0||tc.y>=m_App.m_StageHeight) continue;
                // Avoid drawing over the block itself or make it nice
                float rl = layout.topLeftX + tc.x * layout.cellSize;
                float rt = layout.topLeftY - tc.y * layout.cellSize;
                float rr = rl + layout.cellSize, rb = rt - layout.cellSize;

                ImVec2 rq1 = m_App.ToScreenPosition({rl,rt});
                ImVec2 rq2 = m_App.ToScreenPosition({rr,rt});
                ImVec2 rq3 = m_App.ToScreenPosition({rr,rb});
                ImVec2 rq4 = m_App.ToScreenPosition({rl,rb});
                // Draw orange highlight
                draw->AddQuadFilled(rq1, rq2, rq3, rq4, IM_COL32(255, 165, 0, 80));
                draw->AddQuad(rq1, rq2, rq3, rq4, IM_COL32(255, 180, 50, 160), 2.0F);
            }
        }
    }
}

