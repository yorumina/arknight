#include "App.hpp"
#include "Ark/Renderer.hpp"
#include "RendererShared.hpp"
#include "config.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>

using namespace Ark;
using namespace Ark::RendererConst;

namespace {
ImVec2 RectCenter(const UiRect& rect) {
    return {(rect.minX + rect.maxX) * 0.5F, (rect.minY + rect.maxY) * 0.5F};
}

ImFont* FontForSize(float fontSize) {
    ImGuiIO& io = ImGui::GetIO();
    ImFont* best = io.FontDefault != nullptr ? io.FontDefault : ImGui::GetFont();
    float bestDiff = std::abs((best != nullptr ? best->FontSize : fontSize) - fontSize);
    for (ImFont* font : io.Fonts->Fonts) {
        if (font == nullptr) continue;
        const float diff = std::abs(font->FontSize - fontSize);
        if (diff < bestDiff) {
            best = font;
            bestDiff = diff;
        }
    }
    return best != nullptr ? best : ImGui::GetFont();
}

ImVec2 MeasureText(const char* text, float fontSize) {
    fontSize = std::round(fontSize);
    ImFont* font = FontForSize(fontSize);
    return font->CalcTextSizeA(fontSize, std::numeric_limits<float>::max(), 0.0F, text);
}

void AddTextAt(ImDrawList* draw, const ImVec2& pos, float fontSize, ImU32 color, const char* text) {
    fontSize = std::round(fontSize);
    const ImVec2 snapped{std::round(pos.x), std::round(pos.y)};
    draw->AddText(FontForSize(fontSize), fontSize, snapped, color, text);
}

void AddCenteredText(ImDrawList* draw, const ImVec2& center, float fontSize, ImU32 color, const char* text) {
    const ImVec2 size = MeasureText(text, fontSize);
    AddTextAt(draw, {center.x - size.x * 0.5F, center.y - size.y * 0.5F}, fontSize, color, text);
}

void AddStrongText(ImDrawList* draw, const ImVec2& pos, float fontSize, ImU32 color, const char* text) {
    AddTextAt(draw, {pos.x + 1.0F, pos.y}, fontSize, color, text);
    AddTextAt(draw, pos, fontSize, color, text);
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

std::string ResolveHudIconPath(const std::string& filename) {
    const std::array<std::filesystem::path, 6> bases{
        "data/levels_pic",
        "../data/levels_pic",
        "../../data/levels_pic",
        "../../../data/levels_pic",
        ".",
        "..",
    };
    for (const auto& base : bases) {
        const auto path = base / filename;
        if (std::filesystem::exists(path)) return path.lexically_normal().string();
    }
    return {};
}

std::shared_ptr<Util::Image> LoadHudIcon(const std::string& filename) {
    const auto path = ResolveHudIconPath(filename);
    if (path.empty()) return nullptr;
    return std::make_shared<Util::Image>(path);
}

void DrawIconImage(ImDrawList* draw, const std::shared_ptr<Util::Image>& image,
                   const ImVec2& center, float maxW, float maxH) {
    if (!image || image->GetTextureId() == 0) return;
    const glm::vec2 size = image->GetSize();
    if (size.x <= 0.0F || size.y <= 0.0F) return;

    const float scale = std::min(maxW / size.x, maxH / size.y);
    const float w = size.x * scale;
    const float h = size.y * scale;
    draw->AddImage(
        reinterpret_cast<void*>(static_cast<intptr_t>(image->GetTextureId())),
        {center.x - w * 0.5F, center.y - h * 0.5F},
        {center.x + w * 0.5F, center.y + h * 0.5F}
    );
}

void DrawButtonImage(ImDrawList* draw, const std::shared_ptr<Util::Image>& image,
                     const UiRect& rect, float inset, const ImVec2& uv0, const ImVec2& uv1) {
    if (!image || image->GetTextureId() == 0) return;

    draw->AddImage(
        reinterpret_cast<void*>(static_cast<intptr_t>(image->GetTextureId())),
        {rect.minX + inset, rect.minY + inset},
        {rect.maxX - inset, rect.maxY - inset},
        uv0,
        uv1
    );
}

void DrawButtonImageFit(ImDrawList* draw,
                        const std::shared_ptr<Util::Image>& image,
                        const UiRect& rect,
                        float inset) {
    if (!image || image->GetTextureId() == 0) return;

    const glm::vec2 size = image->GetSize();
    if (size.x <= 0.0F || size.y <= 0.0F) return;

    const float availW = std::max(1.0F, rect.maxX - rect.minX - inset * 2.0F);
    const float availH = std::max(1.0F, rect.maxY - rect.minY - inset * 2.0F);
    const float scale = std::min(availW / size.x, availH / size.y);
    const float w = size.x * scale;
    const float h = size.y * scale;
    const ImVec2 center = RectCenter(rect);

    draw->AddImage(
        reinterpret_cast<void*>(static_cast<intptr_t>(image->GetTextureId())),
        {center.x - w * 0.5F, center.y - h * 0.5F},
        {center.x + w * 0.5F, center.y + h * 0.5F},
        {0.0F, 0.0F},
        {1.0F, 1.0F}
    );
}
} // namespace

void Ark::AppRenderer::DrawOperatorBar(float screenW, float screenH) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const int opCount = static_cast<int>(m_App.m_OperatorTemplates.size());
    if (opCount <= 0) return;

    const float barY = screenH - OP_BAR_HEIGHT;

    std::vector<int> displayOps;
    for (int i = 0; i < opCount; ++i) {
        if (!m_App.IsOperatorTypeOnField(i)) displayOps.push_back(i);
    }
    std::stable_sort(displayOps.begin(), displayOps.end(),
                     [this](int lhs, int rhs) {
                         const auto& a = m_App.m_OperatorTemplates.at(static_cast<std::size_t>(lhs));
                         const auto& b = m_App.m_OperatorTemplates.at(static_cast<std::size_t>(rhs));
                         return a.cost < b.cost;
                     });
    const int dispCount = static_cast<int>(displayOps.size());
    if (dispCount <= 0) return;

    const float totalW = dispCount * OP_CARD_WIDTH + (dispCount - 1) * OP_CARD_SPACING;
    const float startX = screenW - totalW - 24.0F;

    for (int idx = 0; idx < dispCount; ++idx) {
        int i = displayOps[idx];
        const auto& t = m_App.m_OperatorTemplates[static_cast<std::size_t>(i)];

        bool affordable = m_App.m_DP >= static_cast<float>(t.cost);

        const float cx = startX + idx * (OP_CARD_WIDTH + OP_CARD_SPACING);
        const float cy = barY;
        GLuint cardTex = 0;
        if (static_cast<std::size_t>(i) < m_App.m_OperatorCards.size() &&
            m_App.m_OperatorCards[static_cast<std::size_t>(i)]) {
            cardTex = m_App.m_OperatorCards[static_cast<std::size_t>(i)]->GetTextureId();
        }

        if (cardTex != 0) {
            draw->AddImage(
                reinterpret_cast<void*>(static_cast<intptr_t>(cardTex)),
                {cx, cy},
                {cx + OP_CARD_WIDTH, cy + OP_CARD_HEIGHT},
                {0.0F, 0.0F},
                {1.0F, 1.0F},
                IM_COL32(255, 255, 255, 255)
            );
        } else {
            draw->AddRectFilled({cx, cy}, {cx + OP_CARD_WIDTH, cy + OP_CARD_HEIGHT},
                                IM_COL32(35, 40, 52, 230), 2.0F);
            const auto nameSize = ImGui::CalcTextSize(t.name.c_str());
            draw->AddText({cx + (OP_CARD_WIDTH - nameSize.x) * 0.5F, cy + 42.0F},
                          IM_COL32(235, 240, 248, 255), t.name.c_str());
        }

        if (!affordable) {
            draw->AddRectFilled({cx, cy}, {cx + OP_CARD_WIDTH, cy + OP_CARD_HEIGHT},
                                IM_COL32(0, 0, 0, 135), 2.0F);
        }
    }

    if (m_App.m_DraggingFromBar && m_App.m_DragOperatorType >= 0) {
        const float gx = m_App.m_DragScreenPos.x;
        const float gy = m_App.m_DragScreenPos.y;
        GLuint cardTex = 0;
        if (static_cast<std::size_t>(m_App.m_DragOperatorType) < m_App.m_OperatorCards.size() &&
            m_App.m_OperatorCards[static_cast<std::size_t>(m_App.m_DragOperatorType)]) {
            cardTex = m_App.m_OperatorCards[static_cast<std::size_t>(m_App.m_DragOperatorType)]->GetTextureId();
        }

        if (cardTex != 0) {
            draw->AddImage(
                reinterpret_cast<void*>(static_cast<intptr_t>(cardTex)),
                {gx - OP_CARD_WIDTH * 0.5F, gy - OP_CARD_HEIGHT * 0.5F},
                {gx + OP_CARD_WIDTH * 0.5F, gy + OP_CARD_HEIGHT * 0.5F},
                {0.0F, 0.0F},
                {1.0F, 1.0F},
                IM_COL32(255, 255, 255, 190)
            );
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
    static std::shared_ptr<Util::Image> dpIcon = LoadHudIcon("DP.png");

    const float scale = ComputeBattleUiLayout(screenW, screenH).scale;
    const float right = screenW - 36.0F * scale;
    const float labelH = 34.0F * scale;
    const float labelY = screenH - OP_BAR_HEIGHT - labelH - 14.0F * scale;
    const float dpBgX = right - 180.0F * scale;
    const float dpBgY = labelY - 73.0F * scale;
    const float dpBgH = 58.0F * scale;
    const float dpBgRight = right;
    const int deployed = static_cast<int>(m_App.m_Operators.size());
    const int remaining = std::max(0, MAX_OPS - deployed);
    const char* label = u8"剩餘可部屬角色：";
    const std::string remainingStr = std::to_string(remaining);
    const float textSize = 24.0F * scale;
    const ImVec2 labelSize = MeasureText(label, textSize);
    const ImVec2 remainingSize = MeasureText(remainingStr.c_str(), textSize);
    const float contentW = labelSize.x + remainingSize.x;
    const float contentX = right - contentW - 18.0F * scale;
    const float labelPanelX = contentX - 4.0F * scale;

    draw->AddRectFilled({dpBgX, dpBgY}, {dpBgRight, dpBgY + dpBgH}, IM_COL32(26, 28, 32, 146));
    draw->AddRectFilledMultiColor({dpBgX, dpBgY}, {dpBgRight, dpBgY + dpBgH},
                                  IM_COL32(44, 48, 54, 138), IM_COL32(16, 18, 23, 112),
                                  IM_COL32(16, 18, 23, 112), IM_COL32(44, 48, 54, 138));

    draw->AddRectFilled({labelPanelX, labelY}, {right, labelY + labelH}, IM_COL32(19, 21, 27, 184));
    draw->AddRectFilledMultiColor(
        {labelPanelX, labelY},
        {right, labelY + labelH},
        IM_COL32(26, 28, 35, 178),
        IM_COL32(14, 16, 23, 150),
        IM_COL32(14, 16, 23, 150),
        IM_COL32(26, 28, 35, 178)
    );

    const float iconSize = 56.0F * scale;
    const float iconX = dpBgX + 17.0F * scale;
    const float iconY = dpBgY - 4.0F * scale;
    if (dpIcon && dpIcon->GetTextureId() != 0) {
        draw->AddImage(
            reinterpret_cast<void*>(static_cast<intptr_t>(dpIcon->GetTextureId())),
            {iconX, iconY},
            {iconX + iconSize, iconY + iconSize}
        );
    } else {
        const ImVec2 center{iconX + iconSize * 0.5F, iconY + iconSize * 0.5F};
        draw->AddQuadFilled({center.x, center.y - 31.0F * scale},
                            {center.x + 31.0F * scale, center.y},
                            {center.x, center.y + 31.0F * scale},
                            {center.x - 31.0F * scale, center.y},
                            IM_COL32(248, 248, 248, 255));
        AddCenteredText(draw, center, 40.0F * scale, IM_COL32(0, 0, 0, 255), "C");
    }

    const std::string dpStr = std::to_string(static_cast<int>(m_App.m_DP));
    AddStrongText(draw, {dpBgX + 72.0F * scale, dpBgY - 1.0F * scale},
                  56.0F * scale, IM_COL32(250, 250, 250, 255), dpStr.c_str());

    const float lineY = dpBgY + dpBgH + 5.0F * scale;
    const float lineH = 5.0F * scale;
    const float regenProgress = std::clamp(m_App.m_DPRegenTimerMs / 1000.0F, 0.0F, 1.0F);
    draw->AddRectFilled({dpBgX, lineY}, {dpBgRight, lineY + lineH}, IM_COL32(35, 38, 43, 214));
    draw->AddRectFilled({dpBgX, lineY},
                        {dpBgX + (dpBgRight - dpBgX) * regenProgress, lineY + lineH},
                        IM_COL32(255, 255, 255, 255));

    const float textY = labelY + (labelH - textSize) * 0.5F - 2.0F * scale;

    AddStrongText(draw, {contentX, textY},
                  textSize, IM_COL32(250, 250, 250, 245), label);
    AddStrongText(draw, {contentX + labelSize.x, textY},
                  textSize, IM_COL32(250, 250, 250, 245), remainingStr.c_str());
}

void Ark::AppRenderer::DrawHUD(float screenW) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const float SW = screenW;
    const float SH = static_cast<float>(PTSD_Config::WINDOW_HEIGHT);
    const auto ui = ComputeBattleUiLayout(SW, SH);
    const float iconRounding = 4.0F * ui.scale;
    static std::shared_ptr<Util::Image> settingsButtonIcon = LoadHudIcon("Setting.png");
    static std::shared_ptr<Util::Image> speedButtonIcon1x = LoadHudIcon("1x.png");
    static std::shared_ptr<Util::Image> speedButtonIcon2x = LoadHudIcon("2X.png");
    static std::shared_ptr<Util::Image> pauseButtonIcon = LoadHudIcon("pause.png");

    // Top-center battle status (enemy count / life point)
    {
        static std::shared_ptr<Util::Image> enemyNumberIcon = LoadHudIcon("enemy_number.png");
        static std::shared_ptr<Util::Image> lifePointIcon = LoadHudIcon("life_point.png");

        const float panelW = 520.0F * ui.scale;
        const float panelH = 58.0F * ui.scale;
        const float px = SW * 0.5F - panelW * 0.5F;
        const float py = 15.0F * ui.scale;

        draw->AddRectFilled({px, py}, {px + panelW, py + panelH}, IM_COL32(38, 40, 42, 176));
        draw->AddRectFilledMultiColor({px, py}, {px + panelW, py + panelH},
                                      IM_COL32(55, 58, 62, 145), IM_COL32(36, 38, 43, 160),
                                      IM_COL32(36, 38, 43, 160), IM_COL32(55, 58, 62, 145));

        draw->AddLine({px + 15.0F * ui.scale, py + 7.0F * ui.scale},
                      {px + 15.0F * ui.scale, py + panelH - 7.0F * ui.scale},
                      IM_COL32(210, 216, 226, 160), 2.0F * ui.scale);
        draw->AddLine({px + 220.0F * ui.scale, py + 6.0F * ui.scale},
                      {px + 220.0F * ui.scale, py + panelH - 6.0F * ui.scale},
                      IM_COL32(170, 176, 190, 130), 1.5F * ui.scale);
        draw->AddLine({px + 372.0F * ui.scale, py + 6.0F * ui.scale},
                      {px + 372.0F * ui.scale, py + panelH - 6.0F * ui.scale},
                      IM_COL32(170, 176, 190, 130), 1.5F * ui.scale);
        draw->AddLine({px + panelW - 15.0F * ui.scale, py + 7.0F * ui.scale},
                      {px + panelW - 15.0F * ui.scale, py + panelH - 7.0F * ui.scale},
                      IM_COL32(210, 216, 226, 160), 2.0F * ui.scale);

        const ImVec2 enemyCenter{px + 92.0F * ui.scale, py + panelH * 0.5F + 1.0F * ui.scale};
        if (enemyNumberIcon) {
            DrawIconImage(draw, enemyNumberIcon, enemyCenter, 78.0F * ui.scale, 56.0F * ui.scale);
        } else {
            draw->AddCircle(enemyCenter, 13.0F * ui.scale, IM_COL32(247, 151, 39, 255), 28, 2.0F);
        }

        const std::string killInfo = std::to_string(m_App.m_KillCount) + "/" + std::to_string(m_App.m_TotalWaveUnits);
        AddTextAt(draw, {px + 156.0F * ui.scale, py + 11.0F * ui.scale},
                  39.0F * ui.scale, IM_COL32(246, 247, 250, 255), killInfo.c_str());

        const ImVec2 towerCenter{px + 306.0F * ui.scale, py + panelH * 0.5F + 1.0F * ui.scale};
        if (lifePointIcon) {
            DrawIconImage(draw, lifePointIcon, towerCenter, 62.0F * ui.scale, 54.0F * ui.scale);
        } else {
            draw->AddRectFilled({towerCenter.x - 8.0F * ui.scale, towerCenter.y - 9.0F * ui.scale},
                                {towerCenter.x + 8.0F * ui.scale, towerCenter.y + 9.0F * ui.scale},
                                IM_COL32(99, 197, 255, 220), 1.0F * ui.scale);
        }

        const std::string lp = std::to_string(m_App.m_LifePoint);
        AddTextAt(draw, {px + 390.0F * ui.scale, py + 10.0F * ui.scale},
                  40.0F * ui.scale, IM_COL32(255, 122, 141, 255), lp.c_str());
    }

    // Top-left settings button
    if (settingsButtonIcon) {
        DrawButtonImage(draw, settingsButtonIcon, ui.settingsButton, 0.0F, {0.0F, 0.0F}, {1.0F, 1.0F});
    } else {
        DrawTopUiButton(draw, ui.settingsButton, iconRounding, false);
        DrawGearIcon(draw, RectCenter(ui.settingsButton), 17.0F * ui.scale, IM_COL32(242, 246, 252, 255), 2.6F * ui.scale);
    }

    // Top-right speed and pause controls
    const bool isDoubleSpeed = m_App.m_GameSpeedMultiplier >= 1.5F;
    const auto& speedButtonIcon = isDoubleSpeed ? speedButtonIcon2x : speedButtonIcon1x;
    if (speedButtonIcon) {
        DrawButtonImage(draw, speedButtonIcon, ui.speedButton, 0.0F, {0.0F, 0.0F}, {1.0F, 1.0F});
    } else {
        DrawTopUiButton(draw, ui.speedButton, iconRounding, isDoubleSpeed);
    }
    if (pauseButtonIcon) {
        DrawButtonImage(draw, pauseButtonIcon, ui.pauseButton, 0.0F, {0.0F, 0.0F}, {1.0F, 1.0F});
    } else {
        DrawTopUiButton(draw, ui.pauseButton, iconRounding, m_App.m_GamePaused);
    }

    if (!speedButtonIcon) {
        const ImVec2 speedCenter = RectCenter(ui.speedButton);
        const std::string speedText = isDoubleSpeed ? "2X" : "1X";
        AddCenteredText(draw, {speedCenter.x, ui.speedButton.minY + 30.0F * ui.scale},
                        22.0F * ui.scale, IM_COL32(255, 255, 255, 255), speedText.c_str());
        DrawPlayIcon(draw, {speedCenter.x, ui.speedButton.maxY - 32.0F * ui.scale},
                     23.0F * ui.scale, 25.0F * ui.scale, IM_COL32(255, 255, 255, 245));
    }

    if (!pauseButtonIcon) {
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
