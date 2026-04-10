// Renderer.cpp - All drawing / rendering code
#include "App.hpp"
#include "config.hpp"

#include <algorithm>
#include <cmath>
#include <string>

using namespace Ark;

// Color constants
namespace {
constexpr ImU32 COLOR_GRID_WHITE_MAIN = IM_COL32(255, 255, 255, 205);
constexpr ImU32 COLOR_GRID_WHITE_FAINT= IM_COL32(255, 255, 255, 110);
constexpr ImU32 COLOR_GRID_WHITE_BOLD = IM_COL32(255, 255, 255, 235);
constexpr ImU32 COLOR_TEXT_MAIN = IM_COL32(236, 240, 245, 255);
constexpr ImU32 COLOR_TEXT_SUB  = IM_COL32(173, 183, 198, 255);

constexpr int   MAX_OPS         = 12;
constexpr float BEAM_DURATION_MS = 120.0F;
constexpr float BAGPIPE_SP_PER_SKILL = 4.0F;
constexpr int   BAGPIPE_MAX_CHARGES = 3;
constexpr float BAGPIPE_SKILL_DURATION_MS = 1000.0F;
} // namespace

// Draw
void App::DrawScene(const glm::vec2& cursor) {
    const float W = static_cast<float>(PTSD_Config::WINDOW_WIDTH);
    const auto layout = GetBoardLayout();

    DrawGrid();
    DrawBeams();
    DrawOperators(layout);
    DrawEnemies(layout);
    DrawDeployPreview(ToCell(cursor), layout);
    DrawHUD(W);
}

void App::DrawGrid() {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const auto layout = GetBoardLayout();
    const float pad = layout.cellSize * 0.03F;
    const float highgroundOffset = layout.cellSize * 0.22F;

    for (int row = 0; row < m_StageHeight; ++row) {
        for (int col = 0; col < m_StageWidth; ++col) {
            const auto tile = m_TileMap[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)];
            if (tile == TileType::EMPTY) continue;

            const float l = layout.topLeftX + col * layout.cellSize + pad;
            const float r = l + layout.cellSize - 2.0F * pad;
            const float t = layout.topLeftY - row * layout.cellSize - pad;
            const float b = t - layout.cellSize + 2.0F * pad;

            const ImVec2 q1 = ToScreenPosition({l, t});
            const ImVec2 q2 = ToScreenPosition({r, t});
            const ImVec2 q3 = ToScreenPosition({r, b});
            const ImVec2 q4 = ToScreenPosition({l, b});

            draw->AddQuad(q1, q2, q3, q4, COLOR_GRID_WHITE_MAIN, 1.2F);
            draw->AddLine(q1, q3, COLOR_GRID_WHITE_FAINT, 1.0F);
            draw->AddLine(q2, q4, COLOR_GRID_WHITE_FAINT, 1.0F);

            if (tile == TileType::HIGHGROUND) {
                const ImVec2 u1{q1.x, q1.y - highgroundOffset};
                const ImVec2 u2{q2.x, q2.y - highgroundOffset};
                const ImVec2 u3{q3.x, q3.y - highgroundOffset};
                const ImVec2 u4{q4.x, q4.y - highgroundOffset};
                draw->AddQuad(u1, u2, u3, u4, COLOR_GRID_WHITE_BOLD, 1.45F);
                draw->AddLine(q1, u1, COLOR_GRID_WHITE_MAIN, 1.1F);
                draw->AddLine(q2, u2, COLOR_GRID_WHITE_MAIN, 1.1F);
                draw->AddLine(q3, u3, COLOR_GRID_WHITE_MAIN, 1.1F);
                draw->AddLine(q4, u4, COLOR_GRID_WHITE_MAIN, 1.1F);
                draw->AddLine(u1, u3, COLOR_GRID_WHITE_FAINT, 1.0F);
                draw->AddLine(u2, u4, COLOR_GRID_WHITE_FAINT, 1.0F);
            }
        }
    }
}
void App::DrawBeams() {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    for (const auto& beam : m_Beams) {
        const float alpha = std::clamp(beam.ttlMs / BEAM_DURATION_MS, 0.0F, 1.0F);
        draw->AddLine(ToScreenPosition(ToPtsdPosition(beam.from)),
                      ToScreenPosition(ToPtsdPosition(beam.to)),
                      IM_COL32(255, 239, 120, static_cast<int>(255.0F * alpha)), 2.5F);
    }
}

void App::DrawOperators(const BoardLayout& layout) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();

    for (const auto& op : m_Operators) {
        const auto& opType = m_OperatorTemplates.at(static_cast<std::size_t>(op.typeIndex));
        bool isHigh = (opType.deployType == DeployType::HIGHGROUND_ONLY);
        float yOff  = isHigh ? layout.cellSize * 0.22F : 0.0F;

        auto center0 = ToScreenPosition(ToPtsdPosition(ToBoardCenter(op.cell)));
        ImVec2 center = {center0.x, center0.y - yOff};

        // Body quad
        auto q = [&](glm::vec2 off) {
            ImVec2 v = ToScreenPosition(ToPtsdPosition(ToBoardCenter(op.cell) + off));
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
        if (static_cast<std::size_t>(op.typeIndex) < m_OperatorAnims.size()) {
            auto& pack = m_OperatorAnims[op.typeIndex];
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

        // Direction arrow
        glm::vec2 dirOff = glm::vec2(op.direction.x, op.direction.y) * 0.5F;
        ImVec2 dirPt = ToScreenPosition(ToPtsdPosition(ToBoardCenter(op.cell) + dirOff));
        dirPt.y -= yOff;
        draw->AddLine(center, dirPt, IM_COL32(255, 50, 50, 255), 3.5F);

        // HP bar
        const float barW = layout.cellSize * 0.52F;
        const float barX = center0.x - barW * 0.5F;
        float barY = center0.y - layout.cellSize * 0.38F - yOff;
        const float hpR = op.maxHp > 0 ? std::clamp(op.hp/op.maxHp, 0.0F, 1.0F) : 0.0F;
        draw->AddRectFilled({barX,      barY}, {barX+barW, barY+5.0F}, IM_COL32(35,40,48,255));
        draw->AddRectFilled({barX,      barY}, {barX+barW*hpR, barY+5.0F}, IM_COL32(101,228,122,255));

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

void App::DrawEnemies(const BoardLayout& layout) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    for (const auto& e : m_Enemies) {
        if (!e.alive) continue;
        const auto c = ToScreenPosition(ToPtsdPosition(e.boardPos));
        const float r = layout.cellSize * 0.20F;
        if (m_ModelEnemy && m_ModelEnemy->GetTextureId() != 0) {
            float imgR = layout.cellSize * 0.4F;
            draw->AddImage(reinterpret_cast<void*>(static_cast<intptr_t>(m_ModelEnemy->GetTextureId())),
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

void App::DrawDeployPreview(const std::optional<glm::ivec2>& hoverCell, const BoardLayout& layout) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();

    if (hoverCell && !m_GameOver && !m_MissionClear && !m_IsDeploying && m_WaveRunning) {
        const auto& opType = m_OperatorTemplates.at(static_cast<std::size_t>(m_SelectedOperatorType));
        bool deployable = IsOperatorTypeAvailable(m_SelectedOperatorType) &&
                          IsDeployableCellForSelectedOperator(*hoverCell) && !IsCellOccupied(*hoverCell)
                          && static_cast<int>(m_Operators.size()) < MAX_OPS;
        bool affordable = m_DP >= static_cast<float>(opType.cost);
        ImU32 pv = (deployable && affordable) ? IM_COL32(89,225,167,90) : IM_COL32(225,89,89,90);

        float l = layout.topLeftX + hoverCell->x * layout.cellSize;
        float t = layout.topLeftY - hoverCell->y * layout.cellSize;
        float r = l + layout.cellSize, b = t - layout.cellSize;
        
        ImVec2 q1 = ToScreenPosition({l,t});
        ImVec2 q2 = ToScreenPosition({r,t});
        ImVec2 q3 = ToScreenPosition({r,b});
        ImVec2 q4 = ToScreenPosition({l,b});
        draw->AddQuadFilled(q1, q2, q3, q4, pv);
    }

    if (m_IsDeploying) {
        const auto& st = m_OperatorTemplates.at(static_cast<std::size_t>(m_SelectedOperatorType));
        for (int dx = -5; dx <= 5; ++dx) for (int dy = -5; dy <= 5; ++dy) {
            glm::ivec2 rel{dx, dy};
            bool inR = false;
            if (st.deployType == DeployType::GROUND_ONLY) {
                inR = (rel.x==0&&rel.y==0) || rel == m_DeployingDirection;
            } else {
                int fw = rel.x*m_DeployingDirection.x + rel.y*m_DeployingDirection.y;
                int pp = rel.x*m_DeployingDirection.y - rel.y*m_DeployingDirection.x;
                inR = (fw>=1&&fw<=4&&std::abs(pp)<=1)||(rel.x==0&&rel.y==0);
            }
            if (!inR) continue;
            glm::ivec2 tc = m_DeployingCell + rel;
            if (tc.x<0||tc.x>=m_StageWidth||tc.y<0||tc.y>=m_StageHeight) continue;
            float l = layout.topLeftX + tc.x * layout.cellSize;
            float tp= layout.topLeftY - tc.y * layout.cellSize;
            float r = l + layout.cellSize, b = tp - layout.cellSize;

            ImVec2 q1 = ToScreenPosition({l,tp});
            ImVec2 q2 = ToScreenPosition({r,tp});
            ImVec2 q3 = ToScreenPosition({r,b});
            ImVec2 q4 = ToScreenPosition({l,b});
            draw->AddQuadFilled(q1, q2, q3, q4, IM_COL32(255,120,120,80));
            draw->AddQuad(q1, q2, q3, q4, IM_COL32(255,150,150,180), 1.0F);
        }
        glm::vec2 dOff = glm::vec2(m_DeployingDirection) * 0.5F;
        draw->AddLine(ToScreenPosition(ToPtsdPosition(ToBoardCenter(m_DeployingCell))),
                      ToScreenPosition(ToPtsdPosition(ToBoardCenter(m_DeployingCell)+dOff)),
                      IM_COL32(255,50,50,255), 4.0F);
    }
}

void App::DrawHUD(float screenW) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    draw->AddText({26, 12}, COLOR_TEXT_MAIN, "Arknights Demo  |  PTSD Engine");
    if (!m_WaveRunning && !m_GameOver && !m_MissionClear)
        draw->AddText({26, 34}, IM_COL32(255,230,80,255), "[SPACE] Start Wave  (deployment enabled after start)");
    else
        draw->AddText({26, 34}, COLOR_TEXT_SUB,
            "1 Bagpipe | 2 Sniper | 3 Myrtle | LMB Deploy/Skill | MMB Drag | Wheel Zoom | WASD Pan | C Reset Cam | R Reset | ESC Exit");

    // HUD panel
    const float hx = screenW - 340.0F, hy = 22.0F, hw = 312.0F, hh = 360.0F;
    draw->AddRectFilled({hx,hy},{hx+hw,hy+hh}, IM_COL32(21,24,32,230), 8.0F);
    draw->AddRect({hx,hy},{hx+hw,hy+hh}, IM_COL32(70,80,96,255), 8.0F);

    std::string state = "Ready";
    if (m_WaveRunning)   state = "IN BATTLE";
    if (m_MissionClear)  state = "MISSION CLEAR";
    if (m_GameOver)      state = "MISSION FAILED";

    auto line = [&](float yo, ImU32 col, const std::string& s) {
        draw->AddText({hx+14, hy+yo}, col, s.c_str());
    };
    line(14,  COLOR_TEXT_MAIN, "State: " + state);
    line(34,  COLOR_TEXT_MAIN, "Stage: " + m_StageName);
    line(54,  COLOR_TEXT_SUB,  "Map: " + std::to_string(m_StageWidth) + "x" + std::to_string(m_StageHeight));
    line(80,  COLOR_TEXT_MAIN, "Wave: " + std::to_string(m_CurrentWave) + "/" + std::to_string(std::max(1,m_TotalWaves)));
    line(98,  COLOR_TEXT_SUB,  "Spawned: " + std::to_string(m_SpawnedWaveUnits) + "/" + std::to_string(m_TotalWaveUnits));
    line(116, COLOR_TEXT_SUB,  "Enemies: " + std::to_string(m_Enemies.size()));
    line(140, COLOR_TEXT_MAIN, "DP: " + std::to_string(static_cast<int>(m_DP)) + " / " + std::to_string(static_cast<int>(m_MaxDP)));
    line(160, COLOR_TEXT_MAIN, "LP: " + std::to_string(m_LifePoint));
    line(180, COLOR_TEXT_MAIN, "Kills: " + std::to_string(m_KillCount));

    line(210, COLOR_TEXT_SUB, "--- Operators ---");
    for (int i = 0; i < static_cast<int>(m_OperatorTemplates.size()); ++i) {
        const auto& t = m_OperatorTemplates[i];

        // Determine status label
        std::string status;
        bool available = true;
        if (IsOperatorTypeOnField(i)) {
            status = " [ON FIELD]";
            available = false;
        } else {
            auto cdIt = m_OperatorRedeployCooldownMs.find(i);
            if (cdIt != m_OperatorRedeployCooldownMs.end() && cdIt->second > 0.0F) {
                int cdSec = static_cast<int>(std::ceil(cdIt->second / 1000.0F));
                status = " [CD " + std::to_string(cdSec) + "s]";
                available = false;
            }
        }

        ImU32 col;
        if (i == m_SelectedOperatorType) {
            col = available ? IM_COL32(255,255,255,255) : IM_COL32(255,120,120,255);
        } else {
            col = available ? COLOR_TEXT_SUB : IM_COL32(120,120,120,150);
        }

        std::string info = "[" + std::to_string(i+1) + "] " + t.name + "  Cost " + std::to_string(t.cost) + status;
        line(228.0F + i * 36.0F, col, info);
        std::string sub = "    HP:" + std::to_string(static_cast<int>(t.hp))
                        + " ATK:" + std::to_string(static_cast<int>(t.damage))
                        + " DEF:" + std::to_string(static_cast<int>(t.def))
                        + " BLK:" + std::to_string(t.blockCount);
        line(244.0F + i * 36.0F, COLOR_TEXT_SUB, sub);
    }

    // Overlay
    const float SW = static_cast<float>(PTSD_Config::WINDOW_WIDTH);
    const float SH = static_cast<float>(PTSD_Config::WINDOW_HEIGHT);
    if (m_GameOver) {
        draw->AddRectFilled({0,0},{SW,SH}, IM_COL32(0,0,0,145));
        const std::string t = "MISSION FAILED";
        auto ts = ImGui::CalcTextSize(t.c_str());
        draw->AddText({SW*0.5F-ts.x*0.5F, SH*0.5F-ts.y}, IM_COL32(255,120,120,255), t.c_str());
        const std::string h = "Press R to restart";
        auto hs = ImGui::CalcTextSize(h.c_str());
        draw->AddText({SW*0.5F-hs.x*0.5F, SH*0.5F+12}, COLOR_TEXT_MAIN, h.c_str());
    } else if (m_MissionClear) {
        draw->AddRectFilled({0,0},{SW,SH}, IM_COL32(0,0,0,110));
        const std::string t = "MISSION ACCOMPLISHED";
        auto ts = ImGui::CalcTextSize(t.c_str());
        draw->AddText({SW*0.5F-ts.x*0.5F, SH*0.5F-ts.y}, IM_COL32(128,236,177,255), t.c_str());
        const std::string h = "Loading next stage in 2s...";
        auto hs = ImGui::CalcTextSize(h.c_str());
        draw->AddText({SW*0.5F-hs.x*0.5F, SH*0.5F+14}, COLOR_TEXT_MAIN, h.c_str());
    }
}
