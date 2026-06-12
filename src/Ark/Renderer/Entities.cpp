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
    m_App.m_OperatorPortraits.clear();
    m_App.m_OperatorPortraits.resize(m_App.m_OperatorTemplates.size());
    m_App.m_OperatorLevelImages.clear();
    m_App.m_OperatorLevelImages.resize(m_App.m_OperatorTemplates.size());
    m_App.m_OperatorSkillImages.clear();
    m_App.m_OperatorSkillImages.resize(m_App.m_OperatorTemplates.size());
    m_App.m_OperatorFeatureImages.clear();
    m_App.m_OperatorFeatureImages.resize(m_App.m_OperatorTemplates.size());

    auto loadCachedImage = [&](const std::filesystem::path& path) -> std::shared_ptr<Util::Image> {
        if (path.empty() || !std::filesystem::exists(path)) return {};
        const auto key = path.lexically_normal().string();
        auto it = m_App.m_StaticImageCache.find(key);
        if (it != m_App.m_StaticImageCache.end()) return it->second;

        auto image = std::make_shared<Util::Image>(key);
        m_App.m_StaticImageCache.emplace(key, image);
        return image;
    };

    const auto operatorDir = Ark::ResolveOperatorDir();
    m_App.m_VanguardIcon.reset();
    m_App.m_SniperIcon.reset();
    if (!operatorDir.empty()) {
        const auto vanguardIcon = operatorDir / "vanguard.png";
        const auto sniperIcon = operatorDir / "sniper.png";
        m_App.m_VanguardIcon = loadCachedImage(vanguardIcon);
        m_App.m_SniperIcon = loadCachedImage(sniperIcon);
    }
    for (std::size_t i = 0; i < m_App.m_OperatorTemplates.size(); ++i) {
        const auto& op = m_App.m_OperatorTemplates[i];
        if (!operatorDir.empty()) {
            const auto opDir = operatorDir / op.id;
            const auto photoDir = operatorDir / op.id / "photo";
            const auto preferredPortrait = photoDir / (op.id + "_pic.png");
            const auto preferredCard = photoDir / (op.id + ".png");
            auto loadFirstExisting = [&](const std::vector<std::filesystem::path>& candidates) {
                for (const auto& path : candidates) {
                    if (std::filesystem::exists(path)) {
                        return loadCachedImage(path);
                    }
                }
                return std::shared_ptr<Util::Image>{};
            };

            if (std::filesystem::exists(preferredPortrait)) {
                m_App.m_OperatorPortraits[i] = loadCachedImage(preferredPortrait);
            } else if (std::filesystem::exists(preferredCard)) {
                m_App.m_OperatorPortraits[i] = loadCachedImage(preferredCard);
            }

            if (std::filesystem::exists(preferredCard)) {
                m_App.m_OperatorCards[i] = loadCachedImage(preferredCard);
            } else if (std::filesystem::exists(photoDir) && std::filesystem::is_directory(photoDir)) {
                std::vector<std::filesystem::path> cards;
                for (const auto& entry : std::filesystem::directory_iterator(photoDir)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".png") {
                        cards.push_back(entry.path());
                    }
                }
                if (!cards.empty()) {
                    std::sort(cards.begin(), cards.end());
                    m_App.m_OperatorCards[i] = loadCachedImage(cards.front());
                }
            }

            m_App.m_OperatorLevelImages[i] = loadFirstExisting({
                opDir / (op.id + "_level.png"),
                photoDir / (op.id + "_level.png"),
                opDir / "level.png",
                photoDir / "level.png",
            });
            m_App.m_OperatorSkillImages[i] = loadFirstExisting({
                opDir / (op.id + "_skill.png"),
                photoDir / (op.id + "_skill.png"),
                opDir / "skill.png",
                photoDir / "skill.png",
            });
            m_App.m_OperatorFeatureImages[i] = loadFirstExisting({
                opDir / (op.id + "_feature.png"),
                photoDir / (op.id + "_feature.png"),
                opDir / "feature.png",
                photoDir / "feature.png",
            });
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
                    m_App.m_OperatorThumbnails[i] = loadCachedImage(frames[0]);
                }
                break;
            }
        }
    }
}

void Ark::AppRenderer::DrawBeams() {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const float stageYOffset = EntityYOffsetForStage(m_App.m_CurrentStageFile);
    for (const auto& beam : m_App.m_Beams) {
        const float alpha = std::clamp(beam.ttlMs / BEAM_DURATION_MS, 0.0F, 1.0F);
        ImVec2 from = m_App.ToScreenPosition(m_App.ToPtsdPosition(beam.from));
        ImVec2 to = m_App.ToScreenPosition(m_App.ToPtsdPosition(beam.to));
        from.y += stageYOffset;
        to.y += stageYOffset;
        draw->AddLine(from,
                      to,
                      IM_COL32(255, 239, 120, static_cast<int>(255.0F * alpha)), 2.5F);
    }
}

void Ark::AppRenderer::DrawOperators(const BoardLayout& layout, bool drawHighgroundOnly) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const float operatorScale = OperatorVisualScaleForStage(m_App.m_CurrentStageFile);
    const float placeholderScale = operatorScale / OPERATOR_VISUAL_SCALE;
    const float stageYOffset = EntityYOffsetForStage(m_App.m_CurrentStageFile);

    for (const auto& op : m_App.m_Operators) {
        if (!m_App.IsValidOperatorTypeIndex(op.typeIndex)) continue;
        const auto& opType = m_App.m_OperatorTemplates.at(static_cast<std::size_t>(op.typeIndex));
        bool isHigh = (opType.deployType == DeployType::HIGHGROUND_ONLY);
        if (isHigh != drawHighgroundOnly) continue;
        float yOff  = OperatorSpriteLiftPx(layout.cellSize, m_App.UsesBoardArtTransform(), isHigh);
        const float operatorVisualLift = layout.cellSize * 0.18F;

        const glm::vec2 boardCenter = m_App.ToBoardCenter(op.cell);
        auto center0 = m_App.ToScreenPosition(m_App.ToPtsdPosition(boardCenter));
        center0.y += stageYOffset;
        ImVec2 center = {center0.x, center0.y - yOff - operatorVisualLift};

        // Body quad
        auto q = [&](glm::vec2 off) {
            ImVec2 v = m_App.ToScreenPosition(m_App.ToPtsdPosition(boardCenter + off));
            v.y += stageYOffset;
            v.y -= yOff + operatorVisualLift;
            return v;
        };
        float hs = 0.30F * placeholderScale;
        ImVec2 p1 = q({-hs,  hs});
        ImVec2 p2 = q({ hs,  hs});
        ImVec2 p3 = q({ hs, -hs});
        ImVec2 p4 = q({-hs, -hs});

        if (isHigh && !m_App.UsesBoardArtTransform()) {
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
        if (op.typeIndex >= 0 && op.typeIndex < static_cast<int>(m_App.m_OperatorAnims.size())) {
            auto& pack = m_App.m_OperatorAnims[static_cast<std::size_t>(op.typeIndex)];
            auto it = pack.activeInstances.find(op.id);
            if (it != pack.activeInstances.end() && it->second) {
                tex = it->second->GetTextureId();
            }
        }

        if (tex != 0) {
            float imgS = layout.cellSize * 0.8F * operatorScale;
            // No runtime UV flip needed — pre-generated front_flip/ handles LEFT/DOWN
            draw->AddImage(reinterpret_cast<void*>(static_cast<intptr_t>(tex)), 
                           {center.x - imgS, center.y - imgS}, 
                           {center.x + imgS, center.y + imgS});
        } else {
            draw->AddQuadFilled(p1, p2, p3, p4, opType.color);
            draw->AddQuad(p1, p2, p3, p4, IM_COL32(255,255,255,60), 1.5F);

            // Label
            const std::string labelSource = !opType.id.empty() ? opType.id : opType.name;
            const std::string sym = labelSource.empty() ? "?" : labelSource.substr(0, 1);
            const auto ts = ImGui::CalcTextSize(sym.c_str());
            draw->AddText({center.x - ts.x*0.5F, center.y - ts.y*0.5F}, IM_COL32(12,14,19,255), sym.c_str());
        }

        // Direction indicator (red bar): now only shown for selected operator.
        if (op.id == m_App.m_SelectedOperatorId) {
            glm::vec2 dirOff = glm::vec2(op.direction.x, op.direction.y) * 0.5F;
            ImVec2 dirPt = m_App.ToScreenPosition(m_App.ToPtsdPosition(boardCenter + dirOff));
            dirPt.y += stageYOffset;
            dirPt.y -= yOff + operatorVisualLift;
            draw->AddLine(center, dirPt, IM_COL32(255, 50, 50, 255), 3.5F);
        }

        // HP bar
        const float barW = layout.cellSize * 0.52F * OPERATOR_STATUS_BAR_SCALE;
        const float hpBarH = 5.0F * OPERATOR_STATUS_BAR_SCALE;
        const float spBarH = 4.0F * OPERATOR_STATUS_BAR_SCALE;
        const float barGap = 6.0F * OPERATOR_STATUS_BAR_SCALE;
        const float barX = center0.x - barW * 0.5F;
        float barY = center0.y - layout.cellSize * 0.38F - yOff;
        const float hpNow = std::clamp(op.hp, 0.0F, op.maxHp);
        const float hpR = op.maxHp > 0 ? std::clamp(hpNow / op.maxHp, 0.0F, 1.0F) : 0.0F;
        ImU32 hpCol = IM_COL32(162,220,255,255);
        if (hpR < 0.30F) hpCol = IM_COL32(255, 120, 120, 255);
        else if (hpR < 0.60F) hpCol = IM_COL32(255, 196, 132, 255);
        draw->AddRectFilled({barX,      barY}, {barX+barW, barY+hpBarH}, IM_COL32(35,40,48,255));
        draw->AddRectFilled({barX,      barY}, {barX+barW*hpR, barY+hpBarH}, hpCol);

        // SP bar
        if (opType.maxSp > 0) {
            barY += barGap;
            const bool isBagpipe = (opType.name == "Bagpipe" || opType.name == "風笛");
            const float skillDuration = std::max(1.0F, opType.skillDuration);
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
                    ? std::clamp(op.skillTimerMs / skillDuration, 0.0F, 1.0F)
                    : std::clamp(op.sp / opType.maxSp, 0.0F, 1.0F);
            }

            ImU32 spCol = op.skillActive ? IM_COL32(255,165,0,255)
                        : (skillReady ? IM_COL32(255,215,0,255) : IM_COL32(100,150,255,255));
            draw->AddRectFilled({barX,barY},{barX+barW, barY+spBarH}, IM_COL32(35,40,48,255));
            draw->AddRectFilled({barX,barY},{barX+barW*spR, barY+spBarH}, spCol);
            if (skillReady && !op.skillActive) {
                // Pulsing border
                draw->AddRect({barX-1,barY-1},{barX+barW+1,barY+spBarH+1.0F}, IM_COL32(255,215,0,180), 0, 0, 1.5F);
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
    const float enemyScale = EnemyVisualScaleForStage(m_App.m_CurrentStageFile);
    const float stageYOffset = EntityYOffsetForStage(m_App.m_CurrentStageFile);
    for (const auto& e : m_App.m_Enemies) {
        if (!e.alive && e.deathAnimationFinished) continue;
        auto c = m_App.ToScreenPosition(m_App.ToPtsdPosition(e.boardPos));
        c.y -= layout.cellSize * 0.18F;
        c.y -= ENEMY_EXTRA_LIFT_PX;
        c.y += stageYOffset;
        const float r = layout.cellSize * 0.20F * enemyScale;
        const float imgR = layout.cellSize * 0.8F * enemyScale;

        GLuint tex = 0;
        if (e.typeIndex >= 0 && e.typeIndex < static_cast<int>(m_App.m_EnemyAnims.size())) {
            auto& pack = m_App.m_EnemyAnims[static_cast<std::size_t>(e.typeIndex)];
            auto it = pack.activeInstances.find(e.id);
            if (it != pack.activeInstances.end() && it->second) {
                tex = it->second->GetTextureId();
            }
        }

        if (tex != 0) {
            const ImVec2 uv0 = e.useFlipAnimation ? ImVec2{1.0F, 0.0F} : ImVec2{0.0F, 0.0F};
            const ImVec2 uv1 = e.useFlipAnimation ? ImVec2{0.0F, 1.0F} : ImVec2{1.0F, 1.0F};
            draw->AddImage(reinterpret_cast<void*>(static_cast<intptr_t>(tex)),
                           {c.x - imgR, c.y - imgR},
                           {c.x + imgR, c.y + imgR},
                           uv0,
                           uv1);
        } else if (e.alive && m_App.m_ModelEnemy && m_App.m_ModelEnemy->GetTextureId() != 0) {
            const ImVec2 uv0 = e.useFlipAnimation ? ImVec2{1.0F, 0.0F} : ImVec2{0.0F, 0.0F};
            const ImVec2 uv1 = e.useFlipAnimation ? ImVec2{0.0F, 1.0F} : ImVec2{1.0F, 1.0F};
            draw->AddImage(reinterpret_cast<void*>(static_cast<intptr_t>(m_App.m_ModelEnemy->GetTextureId())),
                           {c.x - imgR, c.y - imgR},
                           {c.x + imgR, c.y + imgR},
                           uv0,
                           uv1);
        } else {
            draw->AddCircleFilled(c, r, e.color, 28);
            draw->AddCircle(c, r, IM_COL32(255,255,255,100), 28, 1.0F);
        }
    }

    for (const auto& e : m_App.m_Enemies) {
        if (!e.alive) continue;
        auto c = m_App.ToScreenPosition(m_App.ToPtsdPosition(e.boardPos));
        c.y -= layout.cellSize * 0.18F;
        c.y -= ENEMY_EXTRA_LIFT_PX;
        c.y += stageYOffset;
        const float r = layout.cellSize * 0.20F * enemyScale;
        const float hpR    = e.maxHp > 0 ? std::clamp(e.hp/e.maxHp, 0.0F, 1.0F) : 0.0F;
        const float barHW  = layout.cellSize * 0.24F;
        const float barTop = c.y - r - 9.0F;
        draw->AddRectFilled({c.x - barHW, barTop},{c.x + barHW, barTop+4.0F}, IM_COL32(35,40,48,255));
        draw->AddRectFilled({c.x - barHW, barTop},{c.x - barHW + barHW*2*hpR, barTop+4.0F}, IM_COL32(228,92,92,255));
    }
}
