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

namespace {
ImVec2 Add(const ImVec2& a, const ImVec2& b) {
    return {a.x + b.x, a.y + b.y};
}

ImVec2 Sub(const ImVec2& a, const ImVec2& b) {
    return {a.x - b.x, a.y - b.y};
}

ImVec2 Mul(const ImVec2& v, float scalar) {
    return {v.x * scalar, v.y * scalar};
}

float Cross(const ImVec2& a, const ImVec2& b) {
    return a.x * b.y - a.y * b.x;
}

float SignedArea(const std::array<ImVec2, 4>& polygon) {
    float area = 0.0F;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const ImVec2& a = polygon[i];
        const ImVec2& b = polygon[(i + 1) % polygon.size()];
        area += a.x * b.y - b.x * a.y;
    }
    return area * 0.5F;
}

bool IsInsideEdge(const ImVec2& point, const ImVec2& a, const ImVec2& b, bool clipIsCcw) {
    const float side = Cross(Sub(b, a), Sub(point, a));
    return clipIsCcw ? side >= -0.25F : side <= 0.25F;
}

ImVec2 LineIntersection(const ImVec2& p0, const ImVec2& p1, const ImVec2& e0, const ImVec2& e1) {
    const ImVec2 r = Sub(p1, p0);
    const ImVec2 s = Sub(e1, e0);
    const float denom = Cross(r, s);
    if (std::abs(denom) < 0.0001F) return p1;
    const float t = Cross(Sub(e0, p0), s) / denom;
    return Add(p0, Mul(r, t));
}

std::vector<ImVec2> ClipPolygonToQuad(std::vector<ImVec2> subject,
                                      const std::array<ImVec2, 4>& clip) {
    const bool clipIsCcw = SignedArea(clip) >= 0.0F;
    for (std::size_t edge = 0; edge < clip.size(); ++edge) {
        const ImVec2 e0 = clip[edge];
        const ImVec2 e1 = clip[(edge + 1) % clip.size()];
        const std::vector<ImVec2> input = subject;
        subject.clear();
        if (input.empty()) break;

        ImVec2 previous = input.back();
        bool previousInside = IsInsideEdge(previous, e0, e1, clipIsCcw);
        for (const ImVec2& current : input) {
            const bool currentInside = IsInsideEdge(current, e0, e1, clipIsCcw);
            if (currentInside) {
                if (!previousInside) {
                    subject.push_back(LineIntersection(previous, current, e0, e1));
                }
                subject.push_back(current);
            } else if (previousInside) {
                subject.push_back(LineIntersection(previous, current, e0, e1));
            }
            previous = current;
            previousInside = currentInside;
        }
    }
    return subject;
}
} // namespace

void Ark::AppRenderer::DrawDeployPreview(const std::optional<glm::ivec2>& hoverCell, const BoardLayout& layout, bool drawUnderlay) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const float operatorScale = OperatorVisualScaleForStage(m_App.m_CurrentStageFile);
    const float stageYOffset = EntityYOffsetForStage(m_App.m_CurrentStageFile);

    auto cellQuad = [&](const glm::ivec2& cell) {
        return m_App.GetCellQuad(cell);
    };

    auto drawCellTint = [&](const glm::ivec2& cell, ImU32 fill, ImU32 line, float thickness = 1.0F) {
        const auto q = cellQuad(cell);
        draw->AddQuadFilled(q[0], q[1], q[2], q[3], fill);
        draw->AddQuad(q[0], q[1], q[2], q[3], line, thickness);
    };

    auto drawStripedCell = [&](const glm::ivec2& cell) {
        const auto q = cellQuad(cell);
        const float minX = std::min({q[0].x, q[1].x, q[2].x, q[3].x});
        const float maxX = std::max({q[0].x, q[1].x, q[2].x, q[3].x});
        const float minY = std::min({q[0].y, q[1].y, q[2].y, q[3].y});
        const float maxY = std::max({q[0].y, q[1].y, q[2].y, q[3].y});
        const float h = std::max(1.0F, maxY - minY);
        const float stripeThickness = 14.0F;
        for (float x = minX - h; x < maxX + h; x += 42.0F) {
            const ImVec2 p0{x, maxY};
            const ImVec2 p1{x + h, minY};
            const ImVec2 dir = Sub(p1, p0);
            const float len = std::max(1.0F, std::sqrt(dir.x * dir.x + dir.y * dir.y));
            const ImVec2 normal{-dir.y / len * stripeThickness * 0.5F,
                                dir.x / len * stripeThickness * 0.5F};
            auto clipped = ClipPolygonToQuad({
                Add(p0, normal),
                Add(p1, normal),
                Sub(p1, normal),
                Sub(p0, normal),
            }, q);
            if (clipped.size() >= 3) {
                draw->AddConvexPolyFilled(clipped.data(), static_cast<int>(clipped.size()),
                                          IM_COL32(255, 184, 38, 96));
            }
        }
        draw->AddQuad(q[0], q[1], q[2], q[3], IM_COL32(255, 178, 45, 132), 1.6F);
    };

    auto operatorSpriteCenter = [&](int typeIndex, const glm::ivec2& cell) {
        ImVec2 center = m_App.ToScreenPosition(m_App.ToPtsdPosition(m_App.ToBoardCenter(cell)));
        bool isHigh = false;
        if (typeIndex >= 0 && typeIndex < static_cast<int>(m_App.m_OperatorTemplates.size())) {
            const auto& opType = m_App.m_OperatorTemplates.at(static_cast<std::size_t>(typeIndex));
            isHigh = opType.deployType == DeployType::HIGHGROUND_ONLY;
        }
        const float yOff = OperatorSpriteLiftPx(layout.cellSize, m_App.UsesBoardArtTransform(), isHigh);
        center.y -= yOff + layout.cellSize * 0.18F;
        center.y += stageYOffset;
        return center;
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

    auto isDrawableRangeCell = [&](const glm::ivec2& cell) {
        if (cell.x < 0 || cell.x >= m_App.m_StageWidth ||
            cell.y < 0 || cell.y >= m_App.m_StageHeight) {
            return false;
        }
        if (m_App.m_TileMap[static_cast<std::size_t>(cell.y)][static_cast<std::size_t>(cell.x)] ==
            TileType::EMPTY) {
            return false;
        }
        return !m_App.UsesBoardArtTransform() || m_App.IsBoardArtCellMapped(cell);
    };

    auto rangeCells = [&](int typeIndex, const glm::ivec2& origin, glm::ivec2 dir) {
        std::vector<glm::ivec2> cells;
        if (typeIndex < 0 || typeIndex >= static_cast<int>(m_App.m_OperatorTemplates.size())) return cells;
        if (dir.x == 0 && dir.y == 0) dir = {1, 0};
        const auto& st = m_App.m_OperatorTemplates.at(static_cast<std::size_t>(typeIndex));
        for (int dx = -5; dx <= 5; ++dx) {
            for (int dy = -5; dy <= 5; ++dy) {
                const glm::ivec2 rel{dx, dy};
                const bool inRange = OperatorAttackRangeContains(st, rel, dir);
                if (!inRange) continue;
                const glm::ivec2 cell = origin + rel;
                if (!isDrawableRangeCell(cell)) continue;
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
        const float imgS = layout.cellSize * 0.8F * operatorScale;
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
    std::optional<glm::ivec2> effectiveHoverCell = hoverCell;
    if (m_App.m_DraggingFromBar && !m_App.m_WaitingForDirection && m_App.m_IsDeploying) {
        effectiveHoverCell = m_App.m_DeployingCell;
    }

    const bool hoverDeployable = m_App.m_DraggingFromBar && !m_App.m_WaitingForDirection &&
                                 effectiveHoverCell && canDeployTo(m_App.m_DragOperatorType, *effectiveHoverCell);

    if (drawUnderlay && activeCardType >= 0 && !m_App.m_DraggingFromBar &&
        !m_App.m_WaitingForDirection && isReadyToDeploy(activeCardType)) {
        for (int y = 0; y < m_App.m_StageHeight; ++y) {
            for (int x = 0; x < m_App.m_StageWidth; ++x) {
                const glm::ivec2 cell{x, y};
                if (!canDeployTo(activeCardType, cell)) continue;
                drawCellTint(cell, IM_COL32(62, 210, 126, 42), IM_COL32(86, 245, 160, 78));
            }
        }
    }

    if (drawUnderlay && hoverDeployable) {
        drawCellTint(*effectiveHoverCell, IM_COL32(238, 139, 28, 82), IM_COL32(255, 178, 45, 154), 1.8F);
    }

    if (!drawUnderlay && m_App.m_DraggingFromBar && m_App.m_DragOperatorType >= 0 && !m_App.m_WaitingForDirection) {
        ImVec2 centerpos{m_App.m_DragScreenPos.x, m_App.m_DragScreenPos.y};
        if (hoverDeployable) {
            centerpos = operatorSpriteCenter(m_App.m_DragOperatorType, *effectiveHoverCell);
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
        const ImVec2 centerpos = operatorSpriteCenter(m_App.m_DragOperatorType, m_App.m_DirectionCell);
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
