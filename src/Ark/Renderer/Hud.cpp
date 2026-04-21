#include "App.hpp"
#include "Ark/Renderer.hpp"
#include "RendererShared.hpp"
#include "config.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <limits>

using namespace Ark;
using namespace Ark::RendererConst;

namespace {
ImVec2 RectCenter(const UiRect& rect) {
    return {(rect.minX + rect.maxX) * 0.5F, (rect.minY + rect.maxY) * 0.5F};
}

ImVec2 MeasureText(const char* text, float fontSize) {
    ImFont* font = ImGui::GetFont();
    return font->CalcTextSizeA(fontSize, std::numeric_limits<float>::max(), 0.0F, text);
}

void AddCenteredText(ImDrawList* draw, const ImVec2& center, float fontSize, ImU32 color, const char* text) {
    const ImVec2 size = MeasureText(text, fontSize);
    draw->AddText(ImGui::GetFont(), fontSize,
                  {center.x - size.x * 0.5F, center.y - size.y * 0.5F},
                  color, text);
}

void DrawTopUiButton(ImDrawList* draw, const UiRect& rect, float rounding, bool highlighted) {
    const ImU32 base = highlighted ? IM_COL32(72, 84, 108, 220) : IM_COL32(34, 40, 52, 210);
    const ImU32 line = highlighted ? IM_COL32(145, 176, 230, 220) : IM_COL32(116, 124, 145, 190);
    const ImU32 border = highlighted ? IM_COL32(190, 212, 255, 220) : IM_COL32(100, 108, 126, 170);

    draw->AddRectFilled({rect.minX, rect.minY}, {rect.maxX, rect.maxY}, base, rounding);
    draw->AddLine({rect.minX + 1.0F, rect.minY + 1.0F},
                  {rect.maxX - 1.0F, rect.minY + 1.0F}, line, 2.0F);
    draw->AddRect({rect.minX, rect.minY}, {rect.maxX, rect.maxY}, border, rounding, 0, 1.4F);
}

void DrawGearIcon(ImDrawList* draw, const ImVec2& center, float radius, ImU32 color, float thickness) {
    for (int i = 0; i < 8; ++i) {
        const float ang = (static_cast<float>(i) / 8.0F) * 6.2831853F;
        const ImVec2 p0{center.x + std::cos(ang) * radius * 0.90F, center.y + std::sin(ang) * radius * 0.90F};
        const ImVec2 p1{center.x + std::cos(ang) * radius * 1.20F, center.y + std::sin(ang) * radius * 1.20F};
        draw->AddLine(p0, p1, color, thickness);
    }
    draw->AddCircle(center, radius, color, 32, thickness);
    draw->AddCircle(center, radius * 0.42F, color, 24, thickness);
}

void DrawPauseIcon(ImDrawList* draw, const ImVec2& center, float width, float height, ImU32 color) {
    const float barW = width * 0.22F;
    const float gap = width * 0.15F;
    draw->AddRectFilled({center.x - gap * 0.5F - barW, center.y - height * 0.5F},
                        {center.x - gap * 0.5F, center.y + height * 0.5F}, color, 1.5F);
    draw->AddRectFilled({center.x + gap * 0.5F, center.y - height * 0.5F},
                        {center.x + gap * 0.5F + barW, center.y + height * 0.5F}, color, 1.5F);
}

void DrawPlayIcon(ImDrawList* draw, const ImVec2& center, float width, float height, ImU32 color) {
    draw->AddTriangleFilled({center.x - width * 0.35F, center.y - height * 0.5F},
                            {center.x - width * 0.35F, center.y + height * 0.5F},
                            {center.x + width * 0.45F, center.y}, color);
}
} // namespace

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
    const auto toInt = [](float v) { return static_cast<int>(std::lround(v)); };
    const int curHp = opOnField ? toInt(opOnField->hp) : toInt(t.hp);
    const int maxHp = toInt(t.hp);
    std::string hpStr = std::to_string(curHp) + " / " + std::to_string(maxHp);
    
    const ImU32 labelCol = IM_COL32(150, 160, 180, 255);
    const ImU32 statCol = IM_COL32(255, 255, 255, 255);

    draw->AddText({statX, cy}, labelCol, u8"生命");
    draw->AddText({statX + 60.0F, cy}, statCol, hpStr.c_str());
    cy += 24.0F;
    
    draw->AddText({statX, cy}, labelCol, u8"攻擊");
    draw->AddText({statX + 60.0F, cy}, statCol, std::to_string(toInt(t.damage)).c_str());
    cy += 24.0F;
    
    draw->AddText({statX, cy}, labelCol, u8"防禦");
    draw->AddText({statX + 60.0F, cy}, statCol, std::to_string(opOnField ? toInt(opOnField->def) : toInt(t.def)).c_str());
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
    
    const int spNow = opOnField ? toInt(opOnField->sp) : toInt(t.initialSp);
    const int maxSp = toInt(t.maxSp);
    std::string spStr = std::to_string(spNow) + " / " + std::to_string(maxSp);
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
    const float SW = screenW;
    const float SH = static_cast<float>(PTSD_Config::WINDOW_HEIGHT);
    const auto ui = ComputeBattleUiLayout(SW, SH);
    const float iconRounding = 4.0F * ui.scale;

    // Top-center battle status (enemy count / life point)
    {
        const float panelW = 460.0F * ui.scale;
        const float panelH = 52.0F * ui.scale;
        const float px = SW * 0.5F - panelW * 0.5F;
        const float py = 10.0F * ui.scale;

        draw->AddRectFilled({px, py}, {px + panelW, py + panelH}, IM_COL32(30, 36, 46, 210), 3.0F * ui.scale);
        draw->AddRect({px, py}, {px + panelW, py + panelH}, IM_COL32(96, 104, 122, 170), 3.0F * ui.scale, 0, 1.0F);

        draw->AddLine({px + 180.0F * ui.scale, py + 7.0F * ui.scale},
                      {px + 180.0F * ui.scale, py + panelH - 7.0F * ui.scale},
                      IM_COL32(105, 114, 132, 160), 1.5F);
        draw->AddLine({px + 310.0F * ui.scale, py + 7.0F * ui.scale},
                      {px + 310.0F * ui.scale, py + panelH - 7.0F * ui.scale},
                      IM_COL32(105, 114, 132, 160), 1.5F);

        const ImVec2 enemyCenter{px + 72.0F * ui.scale, py + panelH * 0.5F};
        draw->AddCircleFilled(enemyCenter, 14.0F * ui.scale, IM_COL32(20, 20, 20, 180), 28);
        draw->AddCircle(enemyCenter, 13.0F * ui.scale, IM_COL32(247, 151, 39, 255), 28, 2.0F);
        draw->AddLine({enemyCenter.x - 10.0F * ui.scale, enemyCenter.y},
                      {enemyCenter.x + 10.0F * ui.scale, enemyCenter.y},
                      IM_COL32(247, 151, 39, 230), 1.6F);
        draw->AddLine({enemyCenter.x, enemyCenter.y - 10.0F * ui.scale},
                      {enemyCenter.x, enemyCenter.y + 10.0F * ui.scale},
                      IM_COL32(247, 151, 39, 230), 1.6F);

        const std::string killInfo = std::to_string(m_App.m_KillCount) + "/" + std::to_string(m_App.m_TotalWaveUnits);
        draw->AddText(ImGui::GetFont(), 20.0F * ui.scale,
                      {px + 110.0F * ui.scale, py + 12.0F * ui.scale},
                      IM_COL32(241, 244, 248, 255), killInfo.c_str());

        const ImVec2 towerCenter{px + 350.0F * ui.scale, py + panelH * 0.5F};
        draw->AddRectFilled({towerCenter.x - 8.0F * ui.scale, towerCenter.y - 9.0F * ui.scale},
                            {towerCenter.x + 8.0F * ui.scale, towerCenter.y + 9.0F * ui.scale},
                            IM_COL32(99, 197, 255, 220), 1.0F * ui.scale);
        draw->AddTriangleFilled({towerCenter.x - 10.0F * ui.scale, towerCenter.y - 9.0F * ui.scale},
                                {towerCenter.x + 10.0F * ui.scale, towerCenter.y - 9.0F * ui.scale},
                                {towerCenter.x, towerCenter.y - 16.0F * ui.scale},
                                IM_COL32(99, 197, 255, 220));

        const std::string lp = std::to_string(m_App.m_LifePoint);
        draw->AddText(ImGui::GetFont(), 20.0F * ui.scale,
                      {px + 385.0F * ui.scale, py + 12.0F * ui.scale},
                      IM_COL32(255, 127, 142, 255), lp.c_str());
    }

    // Top-left settings button
    DrawTopUiButton(draw, ui.settingsButton, iconRounding, false);
    DrawGearIcon(draw, RectCenter(ui.settingsButton), 17.0F * ui.scale, IM_COL32(242, 246, 252, 255), 2.6F * ui.scale);

    // Top-right speed and pause controls
    const bool isDoubleSpeed = m_App.m_GameSpeedMultiplier >= 1.5F;
    DrawTopUiButton(draw, ui.speedButton, iconRounding, isDoubleSpeed);
    DrawTopUiButton(draw, ui.pauseButton, iconRounding, m_App.m_GamePaused);

    {
        const ImVec2 speedCenter = RectCenter(ui.speedButton);
        const std::string speedText = isDoubleSpeed ? "2X" : "1X";
        AddCenteredText(draw, {speedCenter.x, ui.speedButton.minY + 30.0F * ui.scale},
                        22.0F * ui.scale, IM_COL32(255, 255, 255, 255), speedText.c_str());
        DrawPlayIcon(draw, {speedCenter.x, ui.speedButton.maxY - 32.0F * ui.scale},
                     23.0F * ui.scale, 25.0F * ui.scale, IM_COL32(255, 255, 255, 245));
    }

    {
        const ImVec2 pauseCenter = RectCenter(ui.pauseButton);
        if (m_App.m_GamePaused) {
            DrawPlayIcon(draw, pauseCenter, 32.0F * ui.scale, 34.0F * ui.scale, IM_COL32(255, 255, 255, 255));
        } else {
            DrawPauseIcon(draw, pauseCenter, 28.0F * ui.scale, 32.0F * ui.scale, IM_COL32(255, 255, 255, 255));
        }
    }

    // Pause overlay
    if (m_App.m_GamePaused && !m_App.m_ShowQuitConfirm && !m_App.m_GameOver && !m_App.m_MissionClear) {
        draw->AddRectFilled({0.0F, 0.0F}, {SW, SH}, IM_COL32(0, 0, 0, 128));
        const ImVec2 center{SW * 0.5F, SH * 0.5F - 35.0F * ui.scale};
        AddCenteredText(draw, {center.x + 2.0F, center.y + 2.0F}, 112.0F * ui.scale, IM_COL32(0, 0, 0, 140), "PAUSE");
        AddCenteredText(draw, center, 112.0F * ui.scale, IM_COL32(245, 245, 245, 255), "PAUSE");
        AddCenteredText(draw, {SW * 0.5F, SH * 0.5F + 62.0F * ui.scale},
                        56.0F * ui.scale, IM_COL32(238, 238, 238, 245), u8"----暫停中----");
    }

    // Settings -> quit confirmation dialog
    if (m_App.m_ShowQuitConfirm && !m_App.m_GameOver && !m_App.m_MissionClear) {
        draw->AddRectFilled({0.0F, 0.0F}, {SW, SH}, IM_COL32(8, 10, 16, 174));

        const UiRect& panel = ui.quitPanel;
        const float panelW = panel.maxX - panel.minX;
        const float panelH = panel.maxY - panel.minY;
        const float headerH = std::max(120.0F * ui.scale, panelH * 0.24F);
        const float bodyTop = panel.minY + headerH;

        draw->AddRectFilled({panel.minX, panel.minY}, {panel.maxX, bodyTop},
                            IM_COL32(43, 47, 57, 244), 2.0F * ui.scale, ImDrawFlags_RoundCornersTop);
        draw->AddRectFilled({panel.minX, bodyTop}, {panel.maxX, panel.maxY},
                            IM_COL32(236, 236, 238, 252), 2.0F * ui.scale, ImDrawFlags_RoundCornersBottom);
        draw->AddLine({panel.minX, bodyTop}, {panel.maxX, bodyTop}, IM_COL32(60, 63, 70, 230), 2.0F);
        draw->AddRect({panel.minX, panel.minY}, {panel.maxX, panel.maxY},
                      IM_COL32(104, 110, 124, 180), 2.0F * ui.scale, 0, 1.5F);

        const float stripeY = bodyTop - 18.0F * ui.scale;
        const float stripeW = 12.0F * ui.scale;
        for (int i = 0; i < 3; ++i) {
            const float x0 = panel.minX + 8.0F * ui.scale + i * (stripeW + 3.0F * ui.scale);
            draw->AddRectFilled({x0, stripeY}, {x0 + stripeW, stripeY + 16.0F * ui.scale},
                                IM_COL32(246, 213, 79, 240));
        }

        const ImVec2 logoCenter{panel.minX + panelW * 0.5F, panel.minY + headerH * 0.46F};
        AddCenteredText(draw, {logoCenter.x - 22.0F * ui.scale, logoCenter.y + 2.0F * ui.scale},
                        125.0F * ui.scale, IM_COL32(245, 245, 247, 255), "Q");
        AddCenteredText(draw, {logoCenter.x + 18.0F * ui.scale, logoCenter.y - 2.0F * ui.scale},
                        58.0F * ui.scale, IM_COL32(245, 245, 247, 255), "QUIT");

        const char* msgA = u8"放棄行動";
        const char* msgB = u8"將會恢復6理智";
        const char* msgC = u8"（全部返還）";
        const char* msgD = u8"：";
        const float msgSize = 58.0F * ui.scale;
        const ImVec2 sizeA = MeasureText(msgA, msgSize);
        const ImVec2 sizeB = MeasureText(msgB, msgSize);
        const ImVec2 sizeC = MeasureText(msgC, msgSize);
        const ImVec2 sizeD = MeasureText(msgD, msgSize);
        float msgX = panel.minX + (panelW - (sizeA.x + sizeB.x + sizeC.x + sizeD.x)) * 0.5F;
        const float msgY = bodyTop + 54.0F * ui.scale;
        draw->AddText(ImGui::GetFont(), msgSize, {msgX, msgY}, IM_COL32(244, 132, 41, 255), msgA);
        msgX += sizeA.x;
        draw->AddText(ImGui::GetFont(), msgSize, {msgX, msgY}, IM_COL32(47, 50, 58, 255), msgB);
        msgX += sizeB.x;
        draw->AddText(ImGui::GetFont(), msgSize, {msgX, msgY}, IM_COL32(244, 132, 41, 255), msgC);
        msgX += sizeC.x;
        draw->AddText(ImGui::GetFont(), msgSize, {msgX, msgY}, IM_COL32(47, 50, 58, 255), msgD);

        const float rewardW = std::min(panelW * 0.42F, 460.0F * ui.scale);
        const float rewardH = 130.0F * ui.scale;
        const float rewardX = panel.minX + (panelW - rewardW) * 0.5F;
        const float rewardY = msgY + 92.0F * ui.scale;
        draw->AddRectFilledMultiColor({rewardX, rewardY}, {rewardX + rewardW, rewardY + rewardH},
                                      IM_COL32(65, 68, 75, 245), IM_COL32(52, 55, 62, 245),
                                      IM_COL32(58, 61, 68, 245), IM_COL32(45, 47, 54, 245));
        draw->AddRect({rewardX, rewardY}, {rewardX + rewardW, rewardY + rewardH},
                      IM_COL32(125, 128, 138, 190), 0.0F, 0, 1.3F);

        const ImVec2 tokenCenter{rewardX + 78.0F * ui.scale, rewardY + rewardH * 0.5F};
        const float tokenR = 43.0F * ui.scale;
        draw->AddCircleFilled(tokenCenter, tokenR + 7.0F * ui.scale, IM_COL32(254, 223, 44, 255), 40);
        draw->AddCircleFilled(tokenCenter, tokenR, IM_COL32(74, 79, 90, 255), 40);
        draw->AddCircle(tokenCenter, tokenR * 0.76F, IM_COL32(198, 204, 217, 220), 30, 2.0F);
        draw->AddPolyline(
            std::array<ImVec2, 5>{
                ImVec2{tokenCenter.x - 5.0F * ui.scale, tokenCenter.y - 22.0F * ui.scale},
                ImVec2{tokenCenter.x + 8.0F * ui.scale, tokenCenter.y - 5.0F * ui.scale},
                ImVec2{tokenCenter.x - 1.0F * ui.scale, tokenCenter.y - 5.0F * ui.scale},
                ImVec2{tokenCenter.x + 5.0F * ui.scale, tokenCenter.y + 21.0F * ui.scale},
                ImVec2{tokenCenter.x - 9.0F * ui.scale, tokenCenter.y + 3.0F * ui.scale}
            }.data(), 5, IM_COL32(255, 255, 255, 240), ImDrawFlags_None, 3.0F * ui.scale);

        AddCenteredText(draw, {rewardX + rewardW * 0.70F, rewardY + rewardH * 0.52F},
                        90.0F * ui.scale, IM_COL32(246, 246, 249, 255), "+6");

        AddCenteredText(draw, {panel.minX + panelW * 0.5F, rewardY + rewardH + 70.0F * ui.scale},
                        62.0F * ui.scale, IM_COL32(40, 43, 49, 255), u8"是否放棄行動?");

        draw->AddRectFilled({ui.quitBackButton.minX, ui.quitBackButton.minY},
                            {ui.quitBackButton.maxX, ui.quitBackButton.maxY},
                            IM_COL32(18, 19, 24, 245), 0.0F);
        draw->AddRectFilled({ui.quitConfirmButton.minX, ui.quitConfirmButton.minY},
                            {ui.quitConfirmButton.maxX, ui.quitConfirmButton.maxY},
                            IM_COL32(137, 21, 26, 245), 0.0F);
        draw->AddLine({ui.quitBackButton.maxX, ui.quitBackButton.minY},
                      {ui.quitBackButton.maxX, ui.quitBackButton.maxY},
                      IM_COL32(80, 82, 90, 180), 1.0F);

        AddCenteredText(draw, RectCenter(ui.quitBackButton), 64.0F * ui.scale,
                        IM_COL32(245, 245, 248, 255), u8"返回");
        AddCenteredText(draw, RectCenter(ui.quitConfirmButton), 64.0F * ui.scale,
                        IM_COL32(248, 248, 248, 255), u8"放棄行動");
    }

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
