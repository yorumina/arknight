#include "App.hpp"
#include "Ark/Renderer.hpp"
#include "Ark/StageLoader.hpp"
#include "RendererShared.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>

using namespace Ark;
using namespace Ark::RendererConst;

// ── Load operator thumbnails ─────────────────────────────────────────────────
void Ark::AppRenderer::LoadOperatorThumbnails() {
    m_App.m_OperatorThumbnails.clear();
    m_App.m_OperatorThumbnails.resize(m_App.m_OperatorTemplates.size());
    m_App.m_OperatorCards.clear();
    m_App.m_OperatorCards.resize(m_App.m_OperatorTemplates.size());
    const auto operatorDir = Ark::ResolveOperatorDir();
    for (std::size_t i = 0; i < m_App.m_OperatorTemplates.size(); ++i) {
        const auto& op = m_App.m_OperatorTemplates[i];
        if (!operatorDir.empty()) {
            const auto photoDir = operatorDir / op.id / "photo";
            const auto preferredCard = photoDir / (op.id + ".png");
            if (std::filesystem::exists(preferredCard)) {
                m_App.m_OperatorCards[i] = std::make_shared<Util::Image>(preferredCard.string());
            } else if (std::filesystem::exists(photoDir) && std::filesystem::is_directory(photoDir)) {
                std::vector<std::filesystem::path> cards;
                for (const auto& entry : std::filesystem::directory_iterator(photoDir)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".png") {
                        cards.push_back(entry.path());
                    }
                }
                if (!cards.empty()) {
                    std::sort(cards.begin(), cards.end());
                    m_App.m_OperatorCards[i] = std::make_shared<Util::Image>(cards.front().string());
                }
            }
        }

        // Try to load the first frame of the default animation as a thumbnail
        std::string thumbnailPath = std::string(ASSETS_DIR) + "/sprites/operators/" + op.id;
        if (!std::filesystem::exists(thumbnailPath)) continue;

        // Look for a default anim folder and grab the first frame
        for (const auto& entry : std::filesystem::directory_iterator(thumbnailPath)) {
            if (!entry.is_directory()) continue;
            std::string dirName = entry.path().filename().string();
            // Convert to lowercase
            std::string lower = dirName;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.find("back") != std::string::npos) continue;
            if (lower.find("default") != std::string::npos) {
                // Find first PNG
                std::vector<std::string> frames;
                for (const auto& f : std::filesystem::directory_iterator(entry.path())) {
                    if (f.is_regular_file() && f.path().extension() == ".png") {
                        frames.push_back(f.path().string());
                    }
                }
                if (!frames.empty()) {
                    std::sort(frames.begin(), frames.end());
                    m_App.m_OperatorThumbnails[i] = std::make_shared<Util::Image>(frames[0]);
                }
                break;
            }
        }
    }
}

void Ark::AppRenderer::DrawBeams() {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    for (const auto& beam : m_App.m_Beams) {
        const float alpha = std::clamp(beam.ttlMs / BEAM_DURATION_MS, 0.0F, 1.0F);
        draw->AddLine(m_App.ToScreenPosition(m_App.ToPtsdPosition(beam.from)),
                      m_App.ToScreenPosition(m_App.ToPtsdPosition(beam.to)),
                      IM_COL32(255, 239, 120, static_cast<int>(255.0F * alpha)), 2.5F);
    }
}

void Ark::AppRenderer::DrawOperators(const BoardLayout& layout, bool drawHighgroundOnly) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();

    for (const auto& op : m_App.m_Operators) {
        const auto& opType = m_App.m_OperatorTemplates.at(static_cast<std::size_t>(op.typeIndex));
        bool isHigh = (opType.deployType == DeployType::HIGHGROUND_ONLY);
        if (isHigh != drawHighgroundOnly) continue;
        float yOff  = isHigh ? layout.cellSize * 0.22F : 0.0F;
        const float operatorVisualLift = layout.cellSize * 0.18F;

        auto center0 = m_App.ToScreenPosition(m_App.ToPtsdPosition(m_App.ToBoardCenter(op.cell)));
        ImVec2 center = {center0.x, center0.y - yOff - operatorVisualLift};

        // Body quad
        auto q = [&](glm::vec2 off) {
            ImVec2 v = m_App.ToScreenPosition(m_App.ToPtsdPosition(m_App.ToBoardCenter(op.cell) + off));
            v.y -= yOff + operatorVisualLift;
            return v;
        };
        float hs = 0.30F;
        ImVec2 p1 = q({-hs,  hs});
        ImVec2 p2 = q({ hs,  hs});
        ImVec2 p3 = q({ hs, -hs});
        ImVec2 p4 = q({-hs, -hs});

        if (isHigh) {
            // Side panel to look elevated
            float bodyOff = layout.cellSize * 0.22F;
            ImVec2 p1b = {p1.x, p1.y + bodyOff};
            ImVec2 p2b = {p2.x, p2.y + bodyOff};
            ImVec2 p3b = {p3.x, p3.y + bodyOff};
            ImVec2 p4b = {p4.x, p4.y + bodyOff};
            draw->AddQuadFilled(p1b, p2b, p3b, p4b, IM_COL32(80, 64, 38, 200)); // "base"
            draw->AddLine(p1b, p1, IM_COL32(80,64,38,200), 2.0F);
            draw->AddLine(p2b, p2, IM_COL32(80,64,38,200), 2.0F);
        }

        // Get per-operator animation texture
        GLuint tex = 0;
        if (static_cast<std::size_t>(op.typeIndex) < m_App.m_OperatorAnims.size()) {
            auto& pack = m_App.m_OperatorAnims[op.typeIndex];
            auto it = pack.activeInstances.find(op.id);
            if (it != pack.activeInstances.end() && it->second) {
                tex = it->second->GetTextureId();
            }
        }

        if (tex != 0) {
            float imgS = layout.cellSize * 0.8F;
            // No runtime UV flip needed — pre-generated front_flip/ handles LEFT/DOWN
            draw->AddImage(reinterpret_cast<void*>(static_cast<intptr_t>(tex)), 
                           {center.x - imgS, center.y - imgS}, 
                           {center.x + imgS, center.y + imgS});
        } else {
            draw->AddQuadFilled(p1, p2, p3, p4, opType.color);
            draw->AddQuad(p1, p2, p3, p4, IM_COL32(255,255,255,60), 1.5F);

            // Label
            const std::string sym(1, opType.name[0]);
            const auto ts = ImGui::CalcTextSize(sym.c_str());
            draw->AddText({center.x - ts.x*0.5F, center.y - ts.y*0.5F}, IM_COL32(12,14,19,255), sym.c_str());
        }

        // Direction indicator (red bar): now only shown for selected operator.
        if (op.id == m_App.m_SelectedOperatorId) {
            glm::vec2 dirOff = glm::vec2(op.direction.x, op.direction.y) * 0.5F;
            ImVec2 dirPt = m_App.ToScreenPosition(m_App.ToPtsdPosition(m_App.ToBoardCenter(op.cell) + dirOff));
            dirPt.y -= yOff + operatorVisualLift;
            draw->AddLine(center, dirPt, IM_COL32(255, 50, 50, 255), 3.5F);
        }

        // HP bar
        const float barW = layout.cellSize * 0.52F;
        const float barX = center0.x - barW * 0.5F;
        float barY = center0.y - layout.cellSize * 0.38F - yOff;
        const float hpNow = std::clamp(op.hp, 0.0F, op.maxHp);
        const float hpR = op.maxHp > 0 ? std::clamp(hpNow / op.maxHp, 0.0F, 1.0F) : 0.0F;
        ImU32 hpCol = IM_COL32(162,220,255,255);
        if (hpR < 0.30F) hpCol = IM_COL32(255, 120, 120, 255);
        else if (hpR < 0.60F) hpCol = IM_COL32(255, 196, 132, 255);
        draw->AddRectFilled({barX,      barY}, {barX+barW, barY+5.0F}, IM_COL32(35,40,48,255));
        draw->AddRectFilled({barX,      barY}, {barX+barW*hpR, barY+5.0F}, hpCol);

        // SP bar
        if (opType.maxSp > 0) {
            barY += 6.0F;
            const bool isBagpipe = (opType.name == "Bagpipe" || opType.name == "風笛");
            float spR = 0.0F;
            bool skillReady = false;

            if (isBagpipe) {
                const int charges = std::clamp(static_cast<int>(std::floor(op.sp / BAGPIPE_SP_PER_SKILL)),
                                               0, BAGPIPE_MAX_CHARGES);
                skillReady = charges > 0;
                spR = op.skillActive
                    ? std::clamp(op.skillTimerMs / BAGPIPE_SKILL_DURATION_MS, 0.0F, 1.0F)
                    : (skillReady ? 1.0F : std::clamp(op.sp / BAGPIPE_SP_PER_SKILL, 0.0F, 1.0F));
            } else {
                skillReady = (op.sp >= opType.maxSp);
                spR = op.skillActive
                    ? std::clamp(op.skillTimerMs / opType.skillDuration, 0.0F, 1.0F)
                    : std::clamp(op.sp / opType.maxSp, 0.0F, 1.0F);
            }

            ImU32 spCol = op.skillActive ? IM_COL32(255,165,0,255)
                        : (skillReady ? IM_COL32(255,215,0,255) : IM_COL32(100,150,255,255));
            draw->AddRectFilled({barX,barY},{barX+barW, barY+4.0F}, IM_COL32(35,40,48,255));
            draw->AddRectFilled({barX,barY},{barX+barW*spR, barY+4.0F}, spCol);
            if (skillReady && !op.skillActive) {
                // Pulsing border
                draw->AddRect({barX-1,barY-1},{barX+barW+1,barY+5.0F}, IM_COL32(255,215,0,180), 0, 0, 1.5F);
            }

            if (isBagpipe) {
                const int charges = std::clamp(static_cast<int>(std::floor(op.sp / BAGPIPE_SP_PER_SKILL)),
                                               0, BAGPIPE_MAX_CHARGES);
                const std::string chargeText = "x" + std::to_string(charges);
                draw->AddText({barX + barW + 4.0F, barY - 6.0F}, IM_COL32(255, 230, 130, 255), chargeText.c_str());
            }
        }
    }
}

void Ark::AppRenderer::DrawEnemies(const BoardLayout& layout) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    for (const auto& e : m_App.m_Enemies) {
        if (!e.alive && e.deathAnimationFinished) continue;
        auto c = m_App.ToScreenPosition(m_App.ToPtsdPosition(e.boardPos));
        c.y -= layout.cellSize * 0.18F;
        const float r = layout.cellSize * 0.20F * ENEMY_VISUAL_SCALE;
        const float imgR = layout.cellSize * 0.8F * ENEMY_VISUAL_SCALE;

        GLuint tex = 0;
        if (e.typeIndex >= 0 && e.typeIndex < static_cast<int>(m_App.m_EnemyAnims.size())) {
            auto& pack = m_App.m_EnemyAnims[static_cast<std::size_t>(e.typeIndex)];
            auto it = pack.activeInstances.find(e.id);
            if (it != pack.activeInstances.end() && it->second) {
                tex = it->second->GetTextureId();
            }
        }

        if (tex != 0) {
            draw->AddImage(reinterpret_cast<void*>(static_cast<intptr_t>(tex)),
                           {c.x - imgR, c.y - imgR},
                           {c.x + imgR, c.y + imgR});
        } else if (e.alive && m_App.m_ModelEnemy && m_App.m_ModelEnemy->GetTextureId() != 0) {
            draw->AddImage(reinterpret_cast<void*>(static_cast<intptr_t>(m_App.m_ModelEnemy->GetTextureId())),
                           {c.x - imgR, c.y - imgR},
                           {c.x + imgR, c.y + imgR});
        } else {
            draw->AddCircleFilled(c, r, e.color, 28);
            draw->AddCircle(c, r, IM_COL32(255,255,255,100), 28, 1.0F);
        }
    }

    for (const auto& e : m_App.m_Enemies) {
        if (!e.alive) continue;
        auto c = m_App.ToScreenPosition(m_App.ToPtsdPosition(e.boardPos));
        c.y -= layout.cellSize * 0.18F;
        const float r = layout.cellSize * 0.20F * ENEMY_VISUAL_SCALE;
        const float hpR    = e.maxHp > 0 ? std::clamp(e.hp/e.maxHp, 0.0F, 1.0F) : 0.0F;
        const float barHW  = layout.cellSize * 0.24F;
        const float barTop = c.y - r - 9.0F;
        draw->AddRectFilled({c.x - barHW, barTop},{c.x + barHW, barTop+4.0F}, IM_COL32(35,40,48,255));
        draw->AddRectFilled({c.x - barHW, barTop},{c.x - barHW + barHW*2*hpR, barTop+4.0F}, IM_COL32(228,92,92,255));
    }
}
