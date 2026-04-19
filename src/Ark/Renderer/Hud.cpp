#include "App.hpp"
#include "Ark/Renderer.hpp"
#include "RendererShared.hpp"
#include "config.hpp"

#include <algorithm>
#include <cmath>

using namespace Ark;
using namespace Ark::RendererConst;

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
    const std::string deployStr = u8"剩餘可放置角色: " + std::to_string(remaining);
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
