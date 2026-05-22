// Renderer.cpp - All drawing / rendering code
#include "App.hpp"
#include "Ark/Renderer.hpp"
#include "config.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>

using namespace Ark;

// Color constants
namespace {
constexpr ImU32 COLOR_GRID_WHITE_MAIN = IM_COL32(255, 255, 255, 205);
constexpr ImU32 COLOR_GRID_WHITE_FAINT= IM_COL32(255, 255, 255, 110);
constexpr ImU32 COLOR_GRID_WHITE_BOLD = IM_COL32(255, 255, 255, 235);
constexpr ImU32 COLOR_LOW_TOP          = IM_COL32(142, 142, 142, 255); // #8e8e8e
constexpr ImU32 COLOR_HIGH_TOP         = IM_COL32(255, 255, 255, 255); // #FFFFFF
constexpr ImU32 COLOR_HIGH_SIDE        = IM_COL32( 76,  76,  76, 255); // #4c4c4c
constexpr ImU32 COLOR_SPAWN_TOP        = IM_COL32(215,  58,  58, 170);
constexpr ImU32 COLOR_SPAWN_SIDE       = IM_COL32(140,  34,  34, 140);
constexpr ImU32 COLOR_GOAL_TOP         = IM_COL32( 65, 138, 245, 170);
constexpr ImU32 COLOR_GOAL_SIDE        = IM_COL32( 35,  84, 165, 140);
constexpr ImU32 COLOR_TEXT_MAIN = IM_COL32(236, 240, 245, 255);
constexpr ImU32 COLOR_TEXT_SUB  = IM_COL32(173, 183, 198, 255);

constexpr int   MAX_OPS         = 8;
constexpr float BEAM_DURATION_MS = 120.0F;
constexpr float BAGPIPE_SP_PER_SKILL = 4.0F;
constexpr int   BAGPIPE_MAX_CHARGES = 3;
constexpr float BAGPIPE_SKILL_DURATION_MS = 1000.0F;
constexpr float PRE_STAGE_TOTAL_MS = 3000.0F;
constexpr float PRE_STAGE_FADE_MS = 500.0F;
constexpr float FINISH_FADE_TO_BLACK_MS = 700.0F;
constexpr float FINISH_BLACKOUT_MS = 1000.0F;
constexpr float FINISH_FADE_IN_MS = 700.0F;
constexpr float FINISH_FADE_OUT_MS = 700.0F;

// Bottom operator bar layout
constexpr float OP_BAR_HEIGHT     = 100.0F;
constexpr float OP_CARD_WIDTH     = 80.0F;
constexpr float OP_CARD_HEIGHT    = 95.0F;
constexpr float OP_CARD_SPACING   = 6.0F;
constexpr float OP_CARD_PORTRAIT_HEIGHT = 60.0F;
constexpr float OP_CARD_INFO_HEIGHT = 35.0F;
constexpr float OP_CARD_ROUNDING  = 4.0F;
} // namespace

// ── Load operator thumbnails ─────────────────────────────────────────────────
void Ark::AppRenderer::LoadOperatorThumbnails() {
    m_App.m_OperatorThumbnails.clear();
    m_App.m_OperatorThumbnails.resize(m_App.m_OperatorTemplates.size());
    for (std::size_t i = 0; i < m_App.m_OperatorTemplates.size(); ++i) {
        const auto& op = m_App.m_OperatorTemplates[i];
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

// Draw
void Ark::AppRenderer::DrawScene(const glm::vec2& cursor) {
    const float W = static_cast<float>(PTSD_Config::WINDOW_WIDTH);
    const float H = static_cast<float>(PTSD_Config::WINDOW_HEIGHT);
    const auto layout = m_App.GetBoardLayout();
    auto drawGameplayFrame = [&]() {
        DrawGrid();
        DrawOperators(layout, false);
        DrawEnemies(layout);
        DrawHighgroundTopLayer();
        DrawMarkerTopLayer();
        DrawOperators(layout, true);
        DrawBeams();
        DrawDeployPreview(cursor, m_App.ToCell(cursor), layout);
        DrawHUD(W);
        DrawOperatorBar(W, H);
        DrawDeploymentInfo(W, H);
        
        // Draw Operator Details panel
        if (m_App.m_DraggingFromBar || m_App.m_WaitingForDirection) {
            if (m_App.m_DragOperatorType >= 0) {
                const auto& t = m_App.m_OperatorTemplates.at(static_cast<std::size_t>(m_App.m_DragOperatorType));
                DrawOperatorDetails(t, nullptr, H);
            }
        } else if (m_App.m_SelectedOperatorId != -1) {
            const Operator* op = nullptr;
            for (auto& o : m_App.m_Operators) {
                if (o.id == m_App.m_SelectedOperatorId) { op = &o; break; }
            }
            if (op) {
                const auto& t = m_App.m_OperatorTemplates.at(static_cast<std::size_t>(op->typeIndex));
                DrawOperatorDetails(t, op, H);
            }
        }
    };

    if (m_App.m_PreStageWaiting && !m_App.m_StageLoadingPath.empty()) {
        m_App.m_StageBackgroundPath = m_App.m_StageLoadingPath;
        const float t = std::clamp(m_App.m_PreStageTimerMs, 0.0F, PRE_STAGE_TOTAL_MS);
        const float fadeIn = std::clamp(t / PRE_STAGE_FADE_MS, 0.0F, 1.0F);
        const float fadeOut = std::clamp((PRE_STAGE_TOTAL_MS - t) / PRE_STAGE_FADE_MS, 0.0F, 1.0F);
        m_App.m_StageBackgroundAlpha = m_App.m_StageLoadingAlpha * std::min(fadeIn, fadeOut);
        DrawLoadingScreen();
        return;
    }

    if (m_App.m_MissionClear && !m_App.m_StageFinishPath.empty()) {
        const float t = std::max(0.0F, m_App.m_ClearTimerMs);
        if (t < FINISH_FADE_TO_BLACK_MS + FINISH_BLACKOUT_MS) {
            drawGameplayFrame();
            float blackAlpha = 1.0F;
            if (t < FINISH_FADE_TO_BLACK_MS) {
                blackAlpha = std::clamp(t / FINISH_FADE_TO_BLACK_MS, 0.0F, 1.0F);
            }
            ImGui::GetBackgroundDrawList()->AddRectFilled(
                {0.0F, 0.0F},
                {static_cast<float>(PTSD_Config::WINDOW_WIDTH), static_cast<float>(PTSD_Config::WINDOW_HEIGHT)},
                IM_COL32(0, 0, 0, static_cast<int>(blackAlpha * 255.0F))
            );
            return;
        }

        m_App.m_StageBackgroundPath = m_App.m_StageFinishPath;
        float alpha = std::clamp((t - FINISH_FADE_TO_BLACK_MS - FINISH_BLACKOUT_MS) / FINISH_FADE_IN_MS, 0.0F, 1.0F);
        if (m_App.m_FinishExitRequested) {
            alpha = std::min(alpha, 1.0F - std::clamp(m_App.m_FinishExitTimerMs / FINISH_FADE_OUT_MS, 0.0F, 1.0F));
        }
        m_App.m_StageBackgroundAlpha = m_App.m_StageFinishAlpha * alpha;
        DrawLoadingScreen();
        return;
    }

    drawGameplayFrame();
}

void Ark::AppRenderer::DrawLoadingScreen() {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const float screenW = static_cast<float>(PTSD_Config::WINDOW_WIDTH);
    const float screenH = static_cast<float>(PTSD_Config::WINDOW_HEIGHT);

    draw->AddRectFilled({0.0F, 0.0F}, {screenW, screenH}, IM_COL32(0, 0, 0, 255));

    if (m_App.m_StageBackgroundPath != m_App.m_StageOverlayLoadedPath) {
        m_App.m_StageBackground.reset();
        m_App.m_StageOverlayLoadedPath = m_App.m_StageBackgroundPath;
    }

    if (m_App.m_StageBackground == nullptr && !m_App.m_StageBackgroundPath.empty()) {
        m_App.m_StageBackground = std::make_shared<Util::Image>(m_App.m_StageBackgroundPath);
    }

    if (m_App.m_StageBackground != nullptr && m_App.m_StageBackground->GetTextureId() != 0) {
        const glm::vec2 imageSize = m_App.m_StageBackground->GetSize();
        if (imageSize.x > 0.0F && imageSize.y > 0.0F) {
            // Cover scaling: always fill full screen (crop overflow if needed).
            const float scale = std::max(screenW / imageSize.x, screenH / imageSize.y);
            const float drawW = imageSize.x * scale;
            const float drawH = imageSize.y * scale;
            const float x = (screenW - drawW) * 0.5F;
            const float y = (screenH - drawH) * 0.5F;
            const int alpha = static_cast<int>(std::clamp(m_App.m_StageBackgroundAlpha, 0.0F, 1.0F) * 255.0F);
            draw->AddImage(
                reinterpret_cast<void*>(static_cast<intptr_t>(m_App.m_StageBackground->GetTextureId())),
                {x, y},
                {x + drawW, y + drawH},
                {0.0F, 0.0F},
                {1.0F, 1.0F},
                IM_COL32(255, 255, 255, alpha)
            );
        }
    }
}

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

        auto center0 = m_App.ToScreenPosition(m_App.ToPtsdPosition(ToBoardCenter(op.cell)));
        ImVec2 center = {center0.x, center0.y - yOff};

        // Body quad
        auto q = [&](glm::vec2 off) {
            ImVec2 v = m_App.ToScreenPosition(m_App.ToPtsdPosition(ToBoardCenter(op.cell) + off));
            v.y -= yOff;
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
            ImVec2 dirPt = m_App.ToScreenPosition(m_App.ToPtsdPosition(ToBoardCenter(op.cell) + dirOff));
            dirPt.y -= yOff;
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
        if (!e.alive) continue;
        const auto c = m_App.ToScreenPosition(m_App.ToPtsdPosition(e.boardPos));
        const float r = layout.cellSize * 0.20F;
        if (m_App.m_ModelEnemy && m_App.m_ModelEnemy->GetTextureId() != 0) {
            float imgR = layout.cellSize * 0.4F;
            draw->AddImage(reinterpret_cast<void*>(static_cast<intptr_t>(m_App.m_ModelEnemy->GetTextureId())),
                           {c.x - imgR, c.y - imgR},
                           {c.x + imgR, c.y + imgR});
        } else {
            draw->AddCircleFilled(c, r, e.color, 28);
            draw->AddCircle(c, r, IM_COL32(255,255,255,100), 28, 1.0F);
        }

        const float hpR    = e.maxHp > 0 ? std::clamp(e.hp/e.maxHp, 0.0F, 1.0F) : 0.0F;
        const float barHW  = layout.cellSize * 0.24F;
        const float barTop = c.y - r - 9.0F;
        draw->AddRectFilled({c.x - barHW, barTop},{c.x + barHW, barTop+4.0F}, IM_COL32(35,40,48,255));
        draw->AddRectFilled({c.x - barHW, barTop},{c.x - barHW + barHW*2*hpR, barTop+4.0F}, IM_COL32(228,92,92,255));
    }
}

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

// ── Bottom operator bar (Arknights-style card row) ───────────────────────────
void Ark::AppRenderer::DrawOperatorBar(float screenW, float screenH) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const int opCount = static_cast<int>(m_App.m_OperatorTemplates.size());
    if (opCount <= 0) return;

    const float barY = screenH - OP_BAR_HEIGHT;

    // Semi-transparent bar background
    draw->AddRectFilled(
        {0.0F, barY - 5.0F}, {screenW, screenH},
        IM_COL32(15, 18, 25, 200)
    );
    // Top border line with gradient effect
    draw->AddLine({0.0F, barY - 5.0F}, {screenW, barY - 5.0F},
                  IM_COL32(80, 90, 110, 180), 1.5F);

    std::vector<int> displayOps;
    for (int i = 0; i < opCount; ++i) {
        if (!m_App.IsOperatorTypeOnField(i)) displayOps.push_back(i);
    }
    const int dispCount = static_cast<int>(displayOps.size());
    if (dispCount <= 0) return;

    const float totalW = dispCount * OP_CARD_WIDTH + (dispCount - 1) * OP_CARD_SPACING;
    const float startX = screenW - totalW - 24.0F;

    for (int idx = 0; idx < dispCount; ++idx) {
        int i = displayOps[idx];
        const auto& t = m_App.m_OperatorTemplates[static_cast<std::size_t>(i)];

        // Determine availability
        bool available = true;
        int cdSec = 0;
        // The operator is guaranteed not to be on field here
        auto cdIt = m_App.m_OperatorRedeployCooldownMs.find(i);
        if (cdIt != m_App.m_OperatorRedeployCooldownMs.end() && cdIt->second > 0.0F) {
            available = false;
            cdSec = static_cast<int>(std::ceil(cdIt->second / 1000.0F));
        }
        
        bool affordable = m_App.m_DP >= static_cast<float>(t.cost);
        bool canDeploy = available && affordable && m_App.m_WaveRunning &&
                         static_cast<int>(m_App.m_Operators.size()) < MAX_OPS;

        const float cx = startX + idx * (OP_CARD_WIDTH + OP_CARD_SPACING);
        const float cy = barY;

        // ── Card background ──
        ImU32 cardBg = canDeploy ? IM_COL32(35, 40, 55, 240)
                                 : IM_COL32(25, 28, 38, 200);
        ImU32 cardBorder = canDeploy ? IM_COL32(100, 120, 160, 200)
                                     : IM_COL32(60, 65, 80, 150);

        // Selected card glow
        if (m_App.m_DraggingFromBar && m_App.m_DragOperatorType == i) {
            cardBg = IM_COL32(55, 70, 100, 255);
            cardBorder = IM_COL32(140, 180, 255, 255);
        }

        draw->AddRectFilled({cx, cy}, {cx + OP_CARD_WIDTH, cy + OP_CARD_HEIGHT},
                            cardBg, OP_CARD_ROUNDING);
        draw->AddRect({cx, cy}, {cx + OP_CARD_WIDTH, cy + OP_CARD_HEIGHT},
                      cardBorder, OP_CARD_ROUNDING, 0, 1.2F);

        // ── Portrait area ──
        const float portraitY = cy + 2.0F;
        const float portraitW = OP_CARD_WIDTH - 4.0F;
        const float portraitH = OP_CARD_PORTRAIT_HEIGHT - 4.0F;
        const float portraitX = cx + 2.0F;

        // Draw thumbnail or colored placeholder
        GLuint thumbTex = 0;
        if (static_cast<std::size_t>(i) < m_App.m_OperatorThumbnails.size() &&
            m_App.m_OperatorThumbnails[static_cast<std::size_t>(i)]) {
            thumbTex = m_App.m_OperatorThumbnails[static_cast<std::size_t>(i)]->GetTextureId();
        }

        if (thumbTex != 0) {
            draw->AddImageRounded(
                reinterpret_cast<void*>(static_cast<intptr_t>(thumbTex)),
                {portraitX, portraitY},
                {portraitX + portraitW, portraitY + portraitH},
                {0.0F, 0.0F}, {1.0F, 1.0F},
                canDeploy ? IM_COL32(255,255,255,255) : IM_COL32(120,120,120,200),
                OP_CARD_ROUNDING
            );
        } else {
            // Colored placeholder with operator initial
            ImU32 portraitBg = canDeploy ? t.color : IM_COL32(60, 60, 60, 200);
            draw->AddRectFilled({portraitX, portraitY},
                                {portraitX + portraitW, portraitY + portraitH},
                                portraitBg, OP_CARD_ROUNDING);
            // Operator initial (large, centered)
            const std::string initStr(1, t.name[0]);
            ImGui::SetWindowFontScale(1.4F);
            const auto initSize = ImGui::CalcTextSize(initStr.c_str());
            ImGui::SetWindowFontScale(1.0F);
            draw->AddText(nullptr, 20.0F,
                {portraitX + (portraitW - initSize.x * 1.4F) * 0.5F,
                 portraitY + (portraitH - initSize.y * 1.4F) * 0.5F},
                IM_COL32(255, 255, 255, canDeploy ? 255 : 120),
                initStr.c_str());
        }

        // ── Unavailable overlay ──
        if (!canDeploy) {
            draw->AddRectFilled({portraitX, portraitY},
                                {portraitX + portraitW, portraitY + portraitH},
                                IM_COL32(0, 0, 0, 100), OP_CARD_ROUNDING);
            if (cdSec > 0) {
                const std::string cdText = std::to_string(cdSec) + "s";
                const auto cdSize = ImGui::CalcTextSize(cdText.c_str());
                draw->AddText({portraitX + (portraitW - cdSize.x) * 0.5F,
                               portraitY + (portraitH - cdSize.y) * 0.5F},
                              IM_COL32(255, 180, 100, 255), cdText.c_str());
            }
        }

        // ── Info bar at bottom of card ──
        const float infoY = cy + OP_CARD_PORTRAIT_HEIGHT;
        const float infoH = OP_CARD_INFO_HEIGHT;
        draw->AddRectFilled({cx, infoY}, {cx + OP_CARD_WIDTH, infoY + infoH},
                            IM_COL32(20, 22, 30, 240), 0.0F);

        // Deploy type indicator (small colored triangle)
        if (t.deployType == DeployType::GROUND_ONLY) {
            // Small ground triangle (blue)
            const float triX = cx + 6.0F;
            const float triY2 = infoY + 12.0F;
            draw->AddTriangleFilled({triX, triY2 + 6.0F}, {triX + 6.0F, triY2 - 2.0F},
                                     {triX + 12.0F, triY2 + 6.0F}, IM_COL32(100, 160, 255, 200));
        } else {
            // Small highground triangle (yellow)
            const float triX = cx + 6.0F;
            const float triY2 = infoY + 12.0F;
            draw->AddTriangleFilled({triX, triY2 + 6.0F}, {triX + 6.0F, triY2 - 2.0F},
                                     {triX + 12.0F, triY2 + 6.0F}, IM_COL32(255, 200, 60, 200));
        }

        // Cost number (right-aligned, prominent)
        const std::string costStr = std::to_string(t.cost);
        const auto costSize = ImGui::CalcTextSize(costStr.c_str());
        ImU32 costCol = affordable ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 100, 100, 255);
        draw->AddText({cx + OP_CARD_WIDTH - costSize.x - 6.0F, infoY + 4.0F},
                      costCol, costStr.c_str());

        // Operator name (small, bottom)
        const std::string nameStr = t.name; // Full name instead of arbitrary slicing
        const auto nameSize = ImGui::CalcTextSize(nameStr.c_str());
        draw->AddText({cx + (OP_CARD_WIDTH - nameSize.x) * 0.5F, infoY + 18.0F},
                      IM_COL32(180, 190, 210, canDeploy ? 255 : 120),
                      nameStr.c_str());
    }

    // ── Draw drag ghost if dragging ──
    if (m_App.m_DraggingFromBar && m_App.m_DragOperatorType >= 0) {
        const auto& t = m_App.m_OperatorTemplates.at(static_cast<std::size_t>(m_App.m_DragOperatorType));
        const float ghostS = 40.0F;
        const float gx = m_App.m_DragScreenPos.x;
        const float gy = m_App.m_DragScreenPos.y;

        // Ghost card following cursor
        GLuint thumbTex = 0;
        if (static_cast<std::size_t>(m_App.m_DragOperatorType) < m_App.m_OperatorThumbnails.size() &&
            m_App.m_OperatorThumbnails[static_cast<std::size_t>(m_App.m_DragOperatorType)]) {
            thumbTex = m_App.m_OperatorThumbnails[static_cast<std::size_t>(m_App.m_DragOperatorType)]->GetTextureId();
        }

        // Semi-transparent background
        draw->AddRectFilled({gx - ghostS, gy - ghostS}, {gx + ghostS, gy + ghostS},
                            IM_COL32(35, 45, 70, 180), 6.0F);
        draw->AddRect({gx - ghostS, gy - ghostS}, {gx + ghostS, gy + ghostS},
                      IM_COL32(160, 200, 255, 200), 6.0F, 0, 2.0F);

        if (thumbTex != 0) {
            draw->AddImageRounded(
                reinterpret_cast<void*>(static_cast<intptr_t>(thumbTex)),
                {gx - ghostS + 4.0F, gy - ghostS + 4.0F},
                {gx + ghostS - 4.0F, gy + ghostS - 4.0F},
                {0.0F, 0.0F}, {1.0F, 1.0F},
                IM_COL32(255, 255, 255, 200), 4.0F
            );
        } else {
            draw->AddRectFilled({gx - ghostS + 4.0F, gy - ghostS + 4.0F},
                                {gx + ghostS - 4.0F, gy + ghostS - 4.0F},
                                t.color, 4.0F);
            const std::string sym(1, t.name[0]);
            const auto ts = ImGui::CalcTextSize(sym.c_str());
            draw->AddText({gx - ts.x * 0.5F, gy - ts.y * 0.5F},
                          IM_COL32(255, 255, 255, 255), sym.c_str());
        }
    }
}

// ── Operator Details Panel (Left side) ────────────────────────────────────────
void Ark::AppRenderer::DrawOperatorDetails(const Ark::OperatorTemplate& t, const Ark::Operator* opOnField, float screenH) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const float panelW = 320.0F;
    const float panelH = 500.0F;
    const float px = 0.0F; 
    const float py = (screenH - panelH) * 0.5F; 
    
    // Panel background
    draw->AddRectFilled({px, py}, {px + panelW, py + panelH}, IM_COL32(20, 24, 32, 230));
    draw->AddRectFilled({px, py}, {px + 6.0F, py + panelH}, IM_COL32(30, 200, 255, 255)); // left strip
    
    float cy = py + 30.0F;
    
    // Name
    draw->AddText(ImGui::GetFont(), 32.0F, {px + 30.0F, cy}, IM_COL32(255, 255, 255, 255), t.name.c_str());
    cy += 50.0F;
    
    // Level Placeholder
    draw->AddText({px + 30.0F, cy}, IM_COL32(200, 200, 200, 255), u8"LV 60");
    cy += 40.0F;
    
    // Stats block
    const float statX = px + 30.0F;
    int curHp = opOnField ? static_cast<int>(opOnField->hp) : t.hp;
    std::string hpStr = std::to_string(curHp) + " / " + std::to_string(t.hp);
    
    const ImU32 labelCol = IM_COL32(150, 160, 180, 255);
    const ImU32 statCol = IM_COL32(255, 255, 255, 255);

    draw->AddText({statX, cy}, labelCol, u8"生命");
    draw->AddText({statX + 60.0F, cy}, statCol, hpStr.c_str());
    cy += 24.0F;
    
    draw->AddText({statX, cy}, labelCol, u8"攻擊");
    draw->AddText({statX + 60.0F, cy}, statCol, std::to_string(static_cast<int>(t.damage)).c_str());
    cy += 24.0F;
    
    draw->AddText({statX, cy}, labelCol, u8"防禦");
    draw->AddText({statX + 60.0F, cy}, statCol, std::to_string(opOnField ? static_cast<int>(opOnField->def) : t.def).c_str());
    cy += 24.0F;
    
    draw->AddText({statX, cy}, labelCol, u8"法抗");
    draw->AddText({statX + 60.0F, cy}, statCol, "0"); 
    cy += 24.0F;

    draw->AddText({statX, cy}, labelCol, u8"阻擋");
    draw->AddText({statX + 60.0F, cy}, statCol, std::to_string(t.blockCount).c_str());
    cy += 40.0F;
    
    // Line separator
    draw->AddLine({px + 10.0F, cy}, {px + panelW - 10.0F, cy}, IM_COL32(60, 70, 80, 255), 1.0F);
    cy += 20.0F;
    
    // Skill info placeholder
    draw->AddText({px + 30.0F, cy}, IM_COL32(200, 200, 200, 255), u8"技能   特性   天賦");
    cy += 30.0F;
    draw->AddRectFilled({px + 30.0F, cy}, {px + 80.0F, cy + 50.0F}, IM_COL32(80, 150, 80, 255)); // skill icon
    draw->AddText({px + 100.0F, cy}, IM_COL32(255, 255, 255, 255), u8"支援號令");
    
    int spNow = opOnField ? static_cast<int>(opOnField->sp) : t.initialSp;
    std::string spStr = std::to_string(spNow) + " / " + std::to_string(static_cast<int>(t.maxSp));
    draw->AddText({px + 100.0F, cy + 25.0F}, IM_COL32(150, 220, 150, 255), spStr.c_str());
}

// ── Compact right-side deployment info (DP + deployable count) ───────────────
void Ark::AppRenderer::DrawDeploymentInfo(float screenW, float screenH) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();

    // Position: bottom-right, just above the operator bar
    const float panelW = 180.0F;
    const float panelH = 50.0F;
    const float px = screenW - panelW - 12.0F;
    const float py = screenH - OP_BAR_HEIGHT - panelH - 12.0F;

    // Panel background
    draw->AddRectFilled({px, py}, {px + panelW, py + panelH},
                        IM_COL32(15, 18, 28, 220), 6.0F);
    draw->AddRect({px, py}, {px + panelW, py + panelH},
                  IM_COL32(70, 80, 100, 180), 6.0F);

    // DP display (like the screenshot: coin icon + number)
    const std::string dpStr = std::to_string(static_cast<int>(m_App.m_DP));
    draw->AddText(nullptr, 18.0F,
        {px + 10.0F, py + 6.0F},
        IM_COL32(255, 220, 100, 255), "DP");
    draw->AddText(nullptr, 22.0F,
        {px + 38.0F, py + 4.0F},
        IM_COL32(255, 255, 255, 255), dpStr.c_str());

    // Remaining deployable operator count
    int deployed = static_cast<int>(m_App.m_Operators.size());
    int remaining = MAX_OPS - deployed;
    const std::string deployStr = u8"剩餘可部屬角色: " + std::to_string(remaining);
    draw->AddText({px + 10.0F, py + 30.0F},
                  IM_COL32(180, 190, 210, 220), deployStr.c_str());
}

void Ark::AppRenderer::DrawHUD(float screenW) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();

    // Top-left: compact game info
    if (m_App.m_PreStageWaiting) {
        const int remainSec = std::max(1, static_cast<int>(std::ceil((3000.0F - m_App.m_PreStageTimerMs) / 1000.0F)));
        const std::string standby = "Stage opening in " + std::to_string(remainSec) + "s...";
        draw->AddText({26, 12}, IM_COL32(255,230,80,255), standby.c_str());
    } else if (!m_App.m_WaveRunning && !m_App.m_GameOver && !m_App.m_MissionClear) {
        draw->AddText({26, 12}, IM_COL32(255,230,80,255), "[SPACE] Start Wave");
    }

    // Top-center: Target / Castle info (as requested by user)
    {
        const std::string killInfo = std::to_string(m_App.m_KillCount) + "/" +
                                    std::to_string(m_App.m_TotalWaveUnits);
        const std::string lpStr = std::to_string(m_App.m_LifePoint); // Life points (3)
        const auto killSize = ImGui::CalcTextSize(killInfo.c_str());
        const auto lpSize = ImGui::CalcTextSize(lpStr.c_str());
        
        const float cx = screenW * 0.5F;
        const float cy = 16.0F; // vertical center of bar

        // We want: [Target] 0/33 | [Tower] 3
        draw->AddRectFilled({cx - 80.0F, cy - 14.0F}, {cx + 80.0F, cy + 14.0F}, IM_COL32(20, 24, 30, 200), 4.0F);
        
        const float sepX = cx + 20.0F;
        draw->AddLine({sepX, cy - 10.0F}, {sepX, cy + 10.0F}, IM_COL32(60, 70, 80, 255), 2.0F);
        
        // Target (Enemy kill count)
        // Icon (orange rect as placeholder)
        draw->AddRectFilled({cx - 70.0F, cy - 8.0F}, {cx - 54.0F, cy + 8.0F}, IM_COL32(255, 140, 0, 200));
        draw->AddText({cx - 45.0F, cy - killSize.y*0.5F}, IM_COL32(255, 255, 255, 255), killInfo.c_str());
        
        // Tower (Life Points)
        // Icon (blue rect as placeholder)
        draw->AddRectFilled({sepX + 10.0F, cy - 8.0F}, {sepX + 26.0F, cy + 8.0F}, IM_COL32(50, 180, 255, 200));
        draw->AddText({sepX + 35.0F, cy - lpSize.y*0.5F}, IM_COL32(255, 120, 120, 255), lpStr.c_str());
    }

    // Top-right: Wave indicator
    if (m_App.m_WaveRunning) {
        const std::string waveStr = "Wave " + std::to_string(m_App.m_CurrentWave) + "/" + std::to_string(std::max(1, m_App.m_TotalWaves));
        draw->AddText({screenW - 160.0F, 9.0F}, COLOR_TEXT_SUB, waveStr.c_str());
    }

    // (Operator Inspector removed as requested)

    // Overlay
    const float SW = static_cast<float>(PTSD_Config::WINDOW_WIDTH);
    const float SH = static_cast<float>(PTSD_Config::WINDOW_HEIGHT);
    if (m_App.m_GameOver) {
        draw->AddRectFilled({0,0},{SW,SH}, IM_COL32(0,0,0,145));
        const std::string t = "MISSION FAILED";
        auto ts = ImGui::CalcTextSize(t.c_str());
        draw->AddText({SW*0.5F-ts.x*0.5F, SH*0.5F-ts.y}, IM_COL32(255,120,120,255), t.c_str());
        const std::string h = "Press R to restart";
        auto hs = ImGui::CalcTextSize(h.c_str());
        draw->AddText({SW*0.5F-hs.x*0.5F, SH*0.5F+12}, COLOR_TEXT_MAIN, h.c_str());
    } else if (m_App.m_MissionClear) {
        draw->AddRectFilled({0,0},{SW,SH}, IM_COL32(0,0,0,110));
        const std::string t = "MISSION ACCOMPLISHED";
        auto ts = ImGui::CalcTextSize(t.c_str());
        draw->AddText({SW*0.5F-ts.x*0.5F, SH*0.5F-ts.y}, IM_COL32(128,236,177,255), t.c_str());
        const std::string h = "Loading next stage in 2s...";
        auto hs = ImGui::CalcTextSize(h.c_str());
        draw->AddText({SW*0.5F-hs.x*0.5F, SH*0.5F+14}, COLOR_TEXT_MAIN, h.c_str());
    }
}
