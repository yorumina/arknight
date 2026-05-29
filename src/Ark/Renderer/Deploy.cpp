#include "App.hpp"
#include "Ark/Renderer.hpp"
#include "RendererShared.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <vector>

using namespace Ark;
using namespace Ark::RendererConst;

void Ark::AppRenderer::DrawDeployPreview(const glm::vec2& ptsdCursor, const std::optional<glm::ivec2>& hoverCell, const BoardLayout& layout, bool drawUnderlay) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();

    auto cellQuad = [&](const glm::ivec2& cell) {
        const float l = layout.topLeftX + static_cast<float>(cell.x) * layout.cellSize;
        const float t = layout.topLeftY - static_cast<float>(cell.y) * layout.cellSize;
        const float r = l + layout.cellSize;
        const float b = t - layout.cellSize;
        return std::array<ImVec2, 4>{
            m_App.ToScreenPosition({l, t}),
            m_App.ToScreenPosition({r, t}),
            m_App.ToScreenPosition({r, b}),
            m_App.ToScreenPosition({l, b}),
        };
    };

    auto drawCellTint = [&](const glm::ivec2& cell, ImU32 fill, ImU32 line, float thickness = 1.0F) {
        const auto q = cellQuad(cell);
        draw->AddQuadFilled(q[0], q[1], q[2], q[3], fill);
        draw->AddQuad(q[0], q[1], q[2], q[3], line, thickness);
    };

    auto drawStripedCell = [&](const glm::ivec2& cell) {
        const auto q = cellQuad(cell);
        draw->AddQuadFilled(q[0], q[1], q[2], q[3], IM_COL32(238, 139, 28, 104));
        const float minX = std::min({q[0].x, q[1].x, q[2].x, q[3].x});
        const float maxX = std::max({q[0].x, q[1].x, q[2].x, q[3].x});
        const float minY = std::min({q[0].y, q[1].y, q[2].y, q[3].y});
        const float maxY = std::max({q[0].y, q[1].y, q[2].y, q[3].y});
        const float h = std::max(1.0F, maxY - minY);
        draw->PushClipRect({minX, minY}, {maxX, maxY}, true);
        for (float x = minX - h; x < maxX + h; x += 42.0F) {
            draw->AddLine({x, maxY}, {x + h, minY}, IM_COL32(255, 184, 38, 154), 14.0F);
        }
        draw->PopClipRect();
        draw->AddQuad(q[0], q[1], q[2], q[3], IM_COL32(255, 178, 45, 190), 1.6F);
    };

    auto isReadyToDeploy = [&](int typeIndex) {
        if (typeIndex < 0 || typeIndex >= static_cast<int>(m_App.m_OperatorTemplates.size())) return false;
        if (!m_App.IsOperatorTypeAvailable(typeIndex)) return false;
        if (static_cast<int>(m_App.m_Operators.size()) >= MAX_OPS) return false;
        const auto& opType = m_App.m_OperatorTemplates.at(static_cast<std::size_t>(typeIndex));
        return m_App.m_DP >= static_cast<float>(opType.cost);
    };

    auto canDeployTo = [&](int typeIndex, const glm::ivec2& cell) {
        return isReadyToDeploy(typeIndex) &&
               m_App.IsDeployableCellForOperatorType(typeIndex, cell) &&
               !m_App.IsCellOccupied(cell);
    };

    auto rangeCells = [&](int typeIndex, const glm::ivec2& origin, glm::ivec2 dir) {
        std::vector<glm::ivec2> cells;
        if (typeIndex < 0 || typeIndex >= static_cast<int>(m_App.m_OperatorTemplates.size())) return cells;
        if (dir.x == 0 && dir.y == 0) dir = {1, 0};
        const auto& st = m_App.m_OperatorTemplates.at(static_cast<std::size_t>(typeIndex));
        for (int dx = -5; dx <= 5; ++dx) {
            for (int dy = -5; dy <= 5; ++dy) {
                const glm::ivec2 rel{dx, dy};
                bool inRange = false;
                if (rel.x == 0 && rel.y == 0) {
                    inRange = true;
                } else if (st.deployType == DeployType::GROUND_ONLY) {
                    inRange = rel == dir;
                } else {
                    const int fw = rel.x * dir.x + rel.y * dir.y;
                    const int pp = rel.x * dir.y - rel.y * dir.x;
                    inRange = fw >= 1 && fw <= 4 && std::abs(pp) <= 1;
                }
                if (!inRange) continue;
                const glm::ivec2 cell = origin + rel;
                if (cell.x < 0 || cell.x >= m_App.m_StageWidth ||
                    cell.y < 0 || cell.y >= m_App.m_StageHeight) {
                    continue;
                }
                cells.push_back(cell);
            }
        }
        return cells;
    };

    auto pausedAnimFor = [](const AnimationClip& clip) -> std::shared_ptr<Util::Animation> {
        static std::map<std::string, std::shared_ptr<Util::Animation>> cache;
        if (clip.Empty()) return nullptr;
        auto it = cache.find(clip.mediaPath);
        if (it != cache.end()) return it->second;
        auto anim = std::make_shared<Util::Animation>(clip.mediaPath, false, clip.loop, false);
        anim->SetCurrentFrame(0);
        anim->Pause();
        cache[clip.mediaPath] = anim;
        return anim;
    };

    auto previewTexture = [&](int typeIndex, glm::ivec2 dir, bool forceFlip) -> GLuint {
        if (typeIndex < 0 || typeIndex >= static_cast<int>(m_App.m_OperatorAnims.size())) return 0;
        const auto& pack = m_App.m_OperatorAnims[static_cast<std::size_t>(typeIndex)];
        const AnimationClip* clip = nullptr;
        if (forceFlip && !pack.defFlip.Empty()) {
            clip = &pack.defFlip;
        } else if (dir.y < 0 && !pack.defBack.Empty()) {
            clip = &pack.defBack;
        } else if ((dir.x < 0 || dir.y > 0) && !pack.defFlip.Empty()) {
            clip = &pack.defFlip;
        } else if (!pack.def.Empty()) {
            clip = &pack.def;
        } else if (!pack.defFlip.Empty()) {
            clip = &pack.defFlip;
        } else if (!pack.defBack.Empty()) {
            clip = &pack.defBack;
        }
        if (clip != nullptr) {
            if (auto anim = pausedAnimFor(*clip)) {
                return anim->GetTextureId();
            }
        }
        if (static_cast<std::size_t>(typeIndex) < m_App.m_OperatorCards.size() &&
            m_App.m_OperatorCards[static_cast<std::size_t>(typeIndex)]) {
            return m_App.m_OperatorCards[static_cast<std::size_t>(typeIndex)]->GetTextureId();
        }
        return 0;
    };

    auto drawPreviewSprite = [&](int typeIndex, const ImVec2& center, glm::ivec2 dir, bool forceFlip, int alpha) {
        const GLuint tex = previewTexture(typeIndex, dir, forceFlip);
        if (tex == 0) return;
        const float imgS = layout.cellSize * 0.8F * OPERATOR_VISUAL_SCALE;
        draw->AddImage(reinterpret_cast<void*>(static_cast<intptr_t>(tex)),
                       {center.x - imgS, center.y - imgS},
                       {center.x + imgS, center.y + imgS},
                       {0.0F, 0.0F},
                       {1.0F, 1.0F},
                       IM_COL32(255, 255, 255, alpha));
    };

    const int activeCardType = (m_App.m_DragOperatorType >= 0 &&
                               (m_App.m_DraggingFromBar || m_App.m_WaitingForDirection))
        ? m_App.m_DragOperatorType
        : m_App.m_SelectedOperatorCardType;

    const bool hoverDeployable = m_App.m_DraggingFromBar && !m_App.m_WaitingForDirection &&
                                 hoverCell && canDeployTo(m_App.m_DragOperatorType, *hoverCell);

    if (drawUnderlay && activeCardType >= 0 && !m_App.m_WaitingForDirection && isReadyToDeploy(activeCardType)) {
        for (int y = 0; y < m_App.m_StageHeight; ++y) {
            for (int x = 0; x < m_App.m_StageWidth; ++x) {
                const glm::ivec2 cell{x, y};
                if (!canDeployTo(activeCardType, cell)) continue;
                drawCellTint(cell, IM_COL32(62, 210, 126, 70), IM_COL32(86, 245, 160, 118));
            }
        }
    }

    if (drawUnderlay && hoverDeployable) {
        for (const auto& cell : rangeCells(m_App.m_DragOperatorType, *hoverCell, {1, 0})) {
            drawStripedCell(cell);
        }
    }

    if (!drawUnderlay && m_App.m_DraggingFromBar && m_App.m_DragOperatorType >= 0 && !m_App.m_WaitingForDirection) {
        ImVec2 centerpos{m_App.m_DragScreenPos.x, m_App.m_DragScreenPos.y};
        if (hoverDeployable) {
            centerpos = m_App.ToScreenPosition(m_App.ToPtsdPosition(m_App.ToBoardCenter(*hoverCell)));
        }
        drawPreviewSprite(m_App.m_DragOperatorType, centerpos, {1, 0}, true, hoverDeployable ? 245 : 214);
    }

    // Show direction selection highlighting and direction-aware paused APNG preview.
    if (m_App.m_WaitingForDirection && m_App.m_DragOperatorType >= 0) {
        if (drawUnderlay) {
            for (const auto& cell : rangeCells(m_App.m_DragOperatorType, m_App.m_DirectionCell, m_App.m_DeployingDirection)) {
                drawStripedCell(cell);
            }
            return;
        }

        const auto q = cellQuad(m_App.m_DirectionCell);
        draw->AddQuad(q[0], q[1], q[2], q[3], IM_COL32(255, 255, 255, 245), 3.0F);
        const ImVec2 centerpos = m_App.ToScreenPosition(m_App.ToPtsdPosition(m_App.ToBoardCenter(m_App.m_DirectionCell)));
        drawPreviewSprite(m_App.m_DragOperatorType, centerpos, m_App.m_DeployingDirection, false, 250);

        if (!m_App.m_IsDirectionDragging) {
            const float dist = layout.cellSize * 0.58F;
            const float s = 8.0F;
            draw->AddLine({centerpos.x - s, centerpos.y - dist + s}, {centerpos.x, centerpos.y - dist}, IM_COL32(255,255,255,210), 3.0F);
            draw->AddLine({centerpos.x + s, centerpos.y - dist + s}, {centerpos.x, centerpos.y - dist}, IM_COL32(255,255,255,210), 3.0F);
            draw->AddLine({centerpos.x - s, centerpos.y + dist - s}, {centerpos.x, centerpos.y + dist}, IM_COL32(255,255,255,210), 3.0F);
            draw->AddLine({centerpos.x + s, centerpos.y + dist - s}, {centerpos.x, centerpos.y + dist}, IM_COL32(255,255,255,210), 3.0F);
            draw->AddLine({centerpos.x - dist + s, centerpos.y - s}, {centerpos.x - dist, centerpos.y}, IM_COL32(255,255,255,210), 3.0F);
            draw->AddLine({centerpos.x - dist + s, centerpos.y + s}, {centerpos.x - dist, centerpos.y}, IM_COL32(255,255,255,210), 3.0F);
            draw->AddLine({centerpos.x + dist - s, centerpos.y - s}, {centerpos.x + dist, centerpos.y}, IM_COL32(255,255,255,210), 3.0F);
            draw->AddLine({centerpos.x + dist - s, centerpos.y + s}, {centerpos.x + dist, centerpos.y}, IM_COL32(255,255,255,210), 3.0F);
        }
    }

    // Show attack range for selected deployed operator
    if (drawUnderlay && m_App.m_SelectedOperatorId != -1 && !m_App.m_WaitingForDirection && !m_App.m_DraggingFromBar) {
        const Operator* selectedOp = nullptr;
        for (const auto& op : m_App.m_Operators) {
            if (op.id == m_App.m_SelectedOperatorId) {
                selectedOp = &op;
                break;
            }
        }
        if (selectedOp) {
            for (const auto& cell : rangeCells(selectedOp->typeIndex, selectedOp->cell, selectedOp->direction)) {
                drawStripedCell(cell);
            }
        }
    }
}
