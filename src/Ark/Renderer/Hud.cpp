#include "App.hpp"
#include "Ark/Renderer.hpp"
#include "RendererShared.hpp"
#include "config.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

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

std::string ResolveOperatorUiImagePath(const std::string& filename) {
    const std::array<std::filesystem::path, 5> bases{
        "data/operators",
        "../data/operators",
        "../../data/operators",
        "../../../data/operators",
        ".",
    };
    for (const auto& base : bases) {
        const auto path = base / filename;
        if (std::filesystem::exists(path)) return path.lexically_normal().string();
    }
    return {};
}

std::shared_ptr<Util::Image> LoadOperatorUiImage(const std::string& filename) {
    const auto path = ResolveOperatorUiImagePath(filename);
    if (path.empty()) return nullptr;
    return std::make_shared<Util::Image>(path);
}

void DrawIconImage(ImDrawList* draw, const std::shared_ptr<Util::Image>& image,
                   const ImVec2& center, float maxW, float maxH, int alpha = 255) {
    if (!image || image->GetTextureId() == 0) return;
    const glm::vec2 size = image->GetSize();
    if (size.x <= 0.0F || size.y <= 0.0F) return;

    const float scale = std::min(maxW / size.x, maxH / size.y);
    const float w = size.x * scale;
    const float h = size.y * scale;
    draw->AddImage(
        reinterpret_cast<void*>(static_cast<intptr_t>(image->GetTextureId())),
        {center.x - w * 0.5F, center.y - h * 0.5F},
        {center.x + w * 0.5F, center.y + h * 0.5F},
        {0.0F, 0.0F},
        {1.0F, 1.0F},
        IM_COL32(255, 255, 255, std::clamp(alpha, 0, 255))
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

void DrawImageCoverRectFadeRight(ImDrawList* draw,
                                 const std::shared_ptr<Util::Image>& image,
                                 const UiRect& rect,
                                 float fadeWidth,
                                 int alpha = 255,
                                 const ImVec2& offset = {0.0F, 0.0F}) {
    if (!image || image->GetTextureId() == 0) return;

    const glm::vec2 size = image->GetSize();
    if (size.x <= 0.0F || size.y <= 0.0F) return;

    const float rectW = std::max(1.0F, rect.maxX - rect.minX);
    const float rectH = std::max(1.0F, rect.maxY - rect.minY);
    const float requiredW = rectW + std::abs(offset.x) * 2.0F;
    const float requiredH = rectH + std::abs(offset.y) * 2.0F;
    const float scale = std::max(requiredH / size.y, requiredW / size.x);
    const float drawW = size.x * scale;
    const float drawH = size.y * scale;
    const ImVec2 center = RectCenter(rect);
    const ImVec2 min{center.x - drawW * 0.5F + offset.x, center.y - drawH * 0.5F + offset.y};
    const ImVec2 max{min.x + drawW, min.y + drawH};
    const float fadeStart = rect.maxX - std::max(1.0F, fadeWidth);

    draw->PushClipRect({rect.minX, rect.minY}, {rect.maxX, rect.maxY}, true);

    auto drawSegment = [&](float segX0, float segX1, int localAlpha) {
        if (localAlpha <= 0) return;

        const float x0 = std::max({segX0, rect.minX, min.x});
        const float x1 = std::min({segX1, rect.maxX, max.x});
        const float y0 = std::max(rect.minY, min.y);
        const float y1 = std::min(rect.maxY, max.y);
        if (x1 <= x0 || y1 <= y0) return;

        const float uv0x = std::clamp((x0 - min.x) / drawW, 0.0F, 1.0F);
        const float uv1x = std::clamp((x1 - min.x) / drawW, 0.0F, 1.0F);
        const float uv0y = std::clamp((y0 - min.y) / drawH, 0.0F, 1.0F);
        const float uv1y = std::clamp((y1 - min.y) / drawH, 0.0F, 1.0F);
        draw->AddImage(reinterpret_cast<void*>(static_cast<intptr_t>(image->GetTextureId())),
                       {x0, y0},
                       {x1, y1},
                       {uv0x, uv0y},
                       {uv1x, uv1y},
                       IM_COL32(255, 255, 255, localAlpha));
    };

    drawSegment(rect.minX, fadeStart, alpha);

    const int fadeSlices = std::clamp(static_cast<int>(std::ceil(std::max(1.0F, fadeWidth))), 24, 64);
    for (int i = 0; i < fadeSlices; ++i) {
        const float x0 = fadeStart + std::max(1.0F, fadeWidth) *
            static_cast<float>(i) / static_cast<float>(fadeSlices);
        const float x1 = fadeStart + std::max(1.0F, fadeWidth) *
            static_cast<float>(i + 1) / static_cast<float>(fadeSlices);
        const float fade = 1.0F - (static_cast<float>(i) + 0.5F) / static_cast<float>(fadeSlices);
        const float easedFade = fade * fade;
        drawSegment(x0, x1, static_cast<int>(static_cast<float>(alpha) * easedFade));
    }
    draw->PopClipRect();
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
        const bool available = m_App.IsOperatorTypeAvailable(i) &&
                               static_cast<int>(m_App.m_Operators.size()) < MAX_OPS;
        const bool selected = i == m_App.m_SelectedOperatorCardType ||
                              ((m_App.m_DraggingFromBar || m_App.m_WaitingForDirection) &&
                               i == m_App.m_DragOperatorType);

        const float cx = startX + idx * (OP_CARD_WIDTH + OP_CARD_SPACING);
        const float cy = barY - (selected ? 22.0F : 0.0F);
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
                IM_COL32(255, 255, 255, selected ? 255 : 238)
            );
        } else {
            draw->AddRectFilled({cx, cy}, {cx + OP_CARD_WIDTH, cy + OP_CARD_HEIGHT},
                                IM_COL32(35, 40, 52, 230), 2.0F);
            const auto nameSize = ImGui::CalcTextSize(t.name.c_str());
            draw->AddText({cx + (OP_CARD_WIDTH - nameSize.x) * 0.5F, cy + 42.0F},
                          IM_COL32(235, 240, 248, 255), t.name.c_str());
        }

        if (selected) {
            draw->AddRect({cx - 2.0F, cy - 2.0F}, {cx + OP_CARD_WIDTH + 2.0F, cy + OP_CARD_HEIGHT + 2.0F},
                          IM_COL32(255, 255, 255, 240), 0.0F, 0, 2.0F);
            draw->AddLine({cx, cy - 3.0F}, {cx + OP_CARD_WIDTH, cy - 3.0F},
                          IM_COL32(255, 226, 70, 245), 3.0F);
        }

        if (!affordable || !available) {
            draw->AddRectFilled({cx, cy}, {cx + OP_CARD_WIDTH, cy + OP_CARD_HEIGHT},
                                IM_COL32(0, 0, 0, 135), 2.0F);
        }
    }

}

// ── Operator Details Panel (Left side) ────────────────────────────────────────
void Ark::AppRenderer::DrawOperatorDetails(int typeIndex, const Ark::Operator* opOnField, float screenH) {
    if (typeIndex < 0 || typeIndex >= static_cast<int>(m_App.m_OperatorTemplates.size())) return;

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const auto& t = m_App.m_OperatorTemplates.at(static_cast<std::size_t>(typeIndex));
    const float screenW = static_cast<float>(PTSD_Config::WINDOW_WIDTH);
    const auto ui = ComputeBattleUiLayout(screenW, screenH);
    const float scale = ui.scale;
    const bool deploymentPreview = opOnField == nullptr &&
        (m_App.m_DraggingFromBar || m_App.m_WaitingForDirection);
    const float dragBlend = std::clamp(m_App.m_OperatorDetailDragBlend, 0.0F, 1.0F);

    const float panelW = std::clamp(screenW * 0.29F, 390.0F, 560.0F);
    const float panelH = screenH;
    const float portraitH = std::min(panelH * 0.62F, 500.0F);
    const float fadeW = 50.0F;
    const float px = 0.0F;
    const float py = 0.0F;
    const UiRect portraitRect{px, py, px + panelW + fadeW, py + panelH};
    const auto toInt = [](float v) { return static_cast<int>(std::lround(v)); };

    const float portraitAlpha = 238.0F + (108.0F - 238.0F) * dragBlend;
    const float portraitOffsetX = (-130.0F - 76.0F * dragBlend) * scale;
    static std::shared_ptr<Util::Image> infoBlurImage = LoadOperatorUiImage("blur.png");
    if (infoBlurImage && infoBlurImage->GetTextureId() != 0) {
        DrawImageCoverRectFadeRight(draw, infoBlurImage, portraitRect, fadeW,
                                    deploymentPreview ? 160 : 205,
                                    {portraitOffsetX, 0.0F});
    }

    std::shared_ptr<Util::Image> portrait;
    if (static_cast<std::size_t>(typeIndex) < m_App.m_OperatorPortraits.size()) {
        portrait = m_App.m_OperatorPortraits[static_cast<std::size_t>(typeIndex)];
    }
    if (portrait && portrait->GetTextureId() != 0) {
        DrawImageCoverRectFadeRight(draw, portrait, portraitRect, fadeW,
                                    static_cast<int>(portraitAlpha),
                                    {portraitOffsetX, 0.0F});
    }

    draw->AddRectFilledMultiColor({px, py}, {portraitRect.maxX, py + panelH},
                                  IM_COL32(0, 0, 0, 20), IM_COL32(0, 0, 0, 0),
                                  IM_COL32(0, 0, 0, 0),
                                  IM_COL32(0, 0, 0, deploymentPreview ? 136 : 168));

    const float iconSize = 80.0F * scale;
    const float iconX = px + 24.0F * scale;
    const float iconY = py + 120.0F * scale;
    draw->AddRectFilled({iconX - 5.0F * scale, iconY - 5.0F * scale},
                        {iconX + iconSize + 5.0F * scale, iconY + iconSize + 5.0F * scale},
                        IM_COL32(8, 10, 12, 185));
    if (m_App.m_VanguardIcon && m_App.m_VanguardIcon->GetTextureId() != 0) {
        DrawIconImage(draw, m_App.m_VanguardIcon,
                      {iconX + iconSize * 0.5F, iconY + iconSize * 0.5F},
                      iconSize, iconSize, 235);
    }

    const float nameX = px + 24.0F * scale;
    const float nameY = py + 244.0F * scale;
    std::shared_ptr<Util::Image> levelImage;
    if (static_cast<std::size_t>(typeIndex) < m_App.m_OperatorLevelImages.size()) {
        levelImage = m_App.m_OperatorLevelImages[static_cast<std::size_t>(typeIndex)];
    }
    if (levelImage && levelImage->GetTextureId() != 0) {
        DrawIconImage(draw, levelImage, {nameX + 94.0F * scale, nameY + 42.0F * scale},
                      151.0F * scale, 85.0F * scale, 245);
    } else {
        AddStrongText(draw, {nameX, nameY}, 34.0F * scale, IM_COL32(248, 248, 248, 255), t.name.c_str());
        AddTextAt(draw, {nameX, nameY + 44.0F * scale}, 20.0F * scale, IM_COL32(236, 240, 246, 238), "LV");
        AddStrongText(draw, {nameX + 64.0F * scale, nameY + 26.0F * scale},
                      58.0F * scale, IM_COL32(255, 255, 255, 255), "90");
    }

    const float statX = px + 24.0F * scale;
    float statY = nameY + 112.0F * scale;
    const ImU32 labelCol = IM_COL32(208, 214, 224, 220);
    const ImU32 statCol = IM_COL32(238, 243, 250, 245);
    auto drawStat = [&](const char* label, const std::string& value) {
        AddTextAt(draw, {statX, statY}, 22.0F * scale, labelCol, label);
        AddTextAt(draw, {statX + 58.0F * scale, statY}, 22.0F * scale, statCol, value.c_str());
        statY += 26.0F * scale;
    };
    drawStat(u8"攻擊", std::to_string(toInt(t.damage)));
    drawStat(u8"防禦", std::to_string(toInt(opOnField ? opOnField->def : t.def)));
    drawStat(u8"法抗", std::to_string(t.magicResistance));
    drawStat(u8"阻擋", std::to_string(t.blockCount));

    const float rangeBox = 122.0F * scale;
    const float rangeX = px + panelW * 0.39F;
    const float rangeY = nameY + 76.0F * scale;
    draw->AddRectFilled({rangeX, rangeY}, {rangeX + rangeBox, rangeY + rangeBox},
                        IM_COL32(2, 3, 5, 156), 8.0F * scale);
    draw->AddRect({rangeX, rangeY}, {rangeX + rangeBox, rangeY + rangeBox},
                  IM_COL32(255, 255, 255, 22), 8.0F * scale, 0, 1.0F * scale);
    auto drawRangePreview = [&]() {
        if (t.id == "Kroos") {
            constexpr int cols = 4;
            constexpr int rows = 3;
            const float gap = 5.0F * scale;
            const float cell = std::min((rangeBox - 28.0F * scale - gap * (cols - 1)) / cols,
                                        (rangeBox - 54.0F * scale - gap * (rows - 1)) / rows);
            const float gridW = cell * cols + gap * (cols - 1);
            const float gx = rangeX + (rangeBox - gridW) * 0.5F;
            const float gy = rangeY + 15.0F * scale;
            for (int row = 0; row < rows; ++row) {
                for (int col = 0; col < cols; ++col) {
                    const float x0 = gx + static_cast<float>(col) * (cell + gap);
                    const float y0 = gy + static_cast<float>(row) * (cell + gap);
                    const bool origin = col == 0 && row == 1;
                    draw->AddRectFilled({x0, y0}, {x0 + cell, y0 + cell},
                                        origin ? IM_COL32(255, 255, 255, 246)
                                               : IM_COL32(168, 172, 174, 150),
                                        1.0F * scale);
                    draw->AddRect({x0, y0}, {x0 + cell, y0 + cell},
                                  IM_COL32(235, 238, 238, origin ? 210 : 82),
                                  1.0F * scale, 0, 1.7F * scale);
                }
            }
            return;
        }

        draw->AddRectFilled({rangeX + 40.0F * scale, rangeY + 34.0F * scale},
                            {rangeX + 58.0F * scale, rangeY + 50.0F * scale}, IM_COL32(255, 255, 255, 255));
        draw->AddRect({rangeX + 58.0F * scale, rangeY + 34.0F * scale},
                      {rangeX + 75.0F * scale, rangeY + 50.0F * scale}, IM_COL32(255, 136, 35, 255), 0.0F, 0, 2.0F);
    };
    drawRangePreview();
    AddCenteredText(draw, {rangeX + rangeBox * 0.5F, rangeY + rangeBox - 24.0F * scale},
                    22.0F * scale, IM_COL32(255, 255, 255, 240), u8"攻擊範圍");

    const int curHp = opOnField ? toInt(opOnField->hp) : toInt(t.hp);
    const int maxHp = toInt(t.hp);
    const float hpY = py + portraitH - 28.0F * scale;
    const float hpLabelW = 58.0F * scale;
    draw->AddRectFilled({px, hpY}, {px + panelW, hpY + 9.0F * scale}, IM_COL32(17, 24, 32, 240));
    draw->AddRectFilled({px, hpY}, {px + panelW * 0.90F, hpY + 9.0F * scale}, IM_COL32(18, 181, 255, 255));
    draw->AddRectFilled({px + panelW * 0.80F, hpY + 9.0F * scale},
                        {px + panelW + 38.0F * scale, hpY + 42.0F * scale}, IM_COL32(28, 138, 218, 220));
    AddTextAt(draw, {px + panelW * 0.80F + 8.0F * scale, hpY + 12.0F * scale},
              20.0F * scale, IM_COL32(255, 255, 255, 238), u8"生命");
    const std::string hpStr = std::to_string(curHp) + "/" + std::to_string(maxHp);
    AddTextAt(draw, {px + panelW * 0.80F + hpLabelW, hpY + 9.0F * scale},
              26.0F * scale, IM_COL32(255, 255, 255, 255), hpStr.c_str());

    const float bodyTop = py + portraitH;
    draw->AddRectFilledMultiColor({px, bodyTop}, {px + panelW, py + panelH},
                                  IM_COL32(10, 12, 16, 196), IM_COL32(10, 12, 16, 0),
                                  IM_COL32(10, 12, 16, 0), IM_COL32(10, 12, 16, 218));
    const float tabH = 44.0F * scale;
    const float tabW = panelW / 3.0F;
    const std::array<const char*, 3> tabs{u8"技能", u8"特性", u8"天賦"};
    for (int i = 0; i < 3; ++i) {
        const float tx0 = px + tabW * static_cast<float>(i);
        const bool active = i == m_App.m_OperatorInfoTab;
        draw->AddRectFilled({tx0, bodyTop}, {tx0 + tabW, bodyTop + tabH},
                            active ? IM_COL32(72, 75, 80, 146) : IM_COL32(78, 80, 84, 90));
        if (active) {
            draw->AddLine({tx0, bodyTop + 1.0F}, {tx0 + tabW, bodyTop + 1.0F},
                          IM_COL32(235, 238, 244, 120), 1.5F * scale);
        }
        AddCenteredText(draw, {tx0 + tabW * 0.5F, bodyTop + tabH * 0.5F},
                        24.0F * scale,
                        active ? IM_COL32(255, 255, 255, 248) : IM_COL32(214, 218, 224, 205),
                        tabs[static_cast<std::size_t>(i)]);
    }

    auto imageAt = [&](const std::vector<std::shared_ptr<Util::Image>>& images) {
        if (static_cast<std::size_t>(typeIndex) < images.size()) {
            return images[static_cast<std::size_t>(typeIndex)];
        }
        return std::shared_ptr<Util::Image>{};
    };
    auto drawIconBox = [&](const std::shared_ptr<Util::Image>& image, const UiRect& rect) {
        draw->AddRectFilled({rect.minX, rect.minY}, {rect.maxX, rect.maxY}, IM_COL32(14, 15, 18, 210), 1.0F * scale);
        draw->AddRect({rect.minX, rect.minY}, {rect.maxX, rect.maxY}, IM_COL32(255, 255, 255, 70), 0.0F, 0, 1.6F * scale);
        if (image && image->GetTextureId() != 0) {
            DrawButtonImageFit(draw, image, rect, 0.0F);
        }
    };
    auto drawTag = [&](float& x, float y, const std::string& text, ImU32 fill) {
        const float fontSize = 20.0F * scale;
        const ImVec2 size = MeasureText(text.c_str(), fontSize);
        const float w = size.x + 20.0F * scale;
        draw->AddRectFilled({x, y}, {x + w, y + 30.0F * scale}, fill, 3.0F * scale);
        AddCenteredText(draw, {x + w * 0.5F, y + 15.0F * scale}, fontSize, IM_COL32(255, 255, 255, 244), text.c_str());
        x += w + 8.0F * scale;
    };
    auto drawWrapped = [&](const ImVec2& pos, float fontSize, ImU32 color,
                           const std::string& text, float wrapW) {
        draw->AddText(FontForSize(fontSize), fontSize, pos, color, text.c_str(), nullptr, wrapW);
    };
    auto traitInfo = [&]() {
        if (t.id == "Bagpipe") {
            return std::pair<std::string, std::string>{
                u8"衝鋒手",
                u8"擊殺敵人後獲得2點部署費用，撤退時返還該次部署費用"
            };
        }
        if (t.id == "Kroos") {
            return std::pair<std::string, std::string>{
                u8"速射手",
                u8"優先攻擊空中單位"
            };
        }
        return std::pair<std::string, std::string>{
            u8"執旗手",
            u8"技能發動期間阻擋數變為0，但使身前一名幹員阻擋數+1"
        };
    };
    auto talentInfo = [&]() {
        if (t.id == "Bagpipe") {
            return std::vector<std::pair<std::string, std::string>>{
                {u8"精密填彈", u8"每次攻擊有28%（+3%）的機率攻擊力提升至130%，且額外攻擊一個目標"},
                {u8"軍事傳統", u8"編入隊伍時所有【先鋒】幹員的初始技力+8（+2），自身部署時額外獲得4點技力"}
            };
        }
        if (t.id == "Kroos") {
            return std::vector<std::pair<std::string, std::string>>{
                {u8"要害瞄準·初級", u8"攻擊時，20%機率當次攻擊力提升至160%（+10%）"}
            };
        }
        return std::vector<std::pair<std::string, std::string>>{
            {u8"浮光躍金", u8"在場時，所有【先鋒】幹員每秒回復33（+3）點生命"}
        };
    };

    const float contentX = px + 24.0F * scale;
    const float contentY = bodyTop + tabH + 28.0F * scale;
    const float iconPage = 84.0F * scale;
    const float textX = contentX + iconPage + 24.0F * scale;
    const float wrapW = panelW - textX - 28.0F * scale;
    if (m_App.m_OperatorInfoTab == 0) {
        const UiRect iconRect{contentX, contentY, contentX + iconPage, contentY + iconPage};
        drawIconBox(imageAt(m_App.m_OperatorSkillImages), iconRect);
        const std::string skillName = !t.skillName.empty() ? t.skillName :
            (t.id == "Myrtle" ? u8"支援號令·β型" : u8"高效衝擊");
        const std::string skillDesc = t.id == "Myrtle"
            ? u8"停止攻擊，持續時間內回復總共14點部署費用"
            : (!t.skillDescription.empty() ? t.skillDescription : u8"下一次的攻擊力提升至200%，且額外攻擊一次，可充能3次");
        AddStrongText(draw, {textX, contentY - 2.0F * scale}, 25.0F * scale,
                      IM_COL32(255, 255, 255, 246), skillName.c_str());
        float tagX = textX;
        drawTag(tagX, contentY + 36.0F * scale, u8"自動回復", IM_COL32(138, 194, 66, 236));
        drawTag(tagX, contentY + 36.0F * scale, t.id == "Myrtle" ? u8"手動觸發" : u8"自動觸發",
                IM_COL32(126, 128, 132, 230));
        if (t.id == "Myrtle") {
            drawTag(tagX, contentY + 36.0F * scale, u8"8秒", IM_COL32(126, 128, 132, 230));
        }
        drawWrapped({textX, contentY + 76.0F * scale}, 24.0F * scale,
                    IM_COL32(250, 252, 255, 245), skillDesc, wrapW);
        if (t.id == "Bagpipe") {
            AddTextAt(draw, {textX, contentY + 126.0F * scale}, 23.0F * scale,
                      IM_COL32(238, 184, 60, 246), u8"可充能3次");
        }
    } else if (m_App.m_OperatorInfoTab == 1) {
        const UiRect iconRect{contentX, contentY, contentX + iconPage, contentY + iconPage};
        drawIconBox(imageAt(m_App.m_OperatorFeatureImages), iconRect);
        const auto [title, desc] = traitInfo();
        AddStrongText(draw, {textX, contentY}, 25.0F * scale,
                      IM_COL32(255, 255, 255, 246), title.c_str());
        drawWrapped({textX, contentY + 40.0F * scale}, 24.0F * scale,
                    IM_COL32(250, 252, 255, 245), desc, wrapW);
    } else {
        float y = contentY + 2.0F * scale;
        for (const auto& entry : talentInfo()) {
            const float titleSize = 23.0F * scale;
            const ImVec2 titleText = MeasureText(entry.first.c_str(), titleSize);
            draw->AddRectFilled({contentX, y}, {contentX + titleText.x + 18.0F * scale, y + 34.0F * scale},
                                IM_COL32(210, 214, 218, 218), 4.0F * scale);
            AddTextAt(draw, {contentX + 9.0F * scale, y + 4.0F * scale}, titleSize,
                      IM_COL32(48, 51, 56, 245), entry.first.c_str());
            drawWrapped({contentX, y + 48.0F * scale}, 24.0F * scale,
                        IM_COL32(250, 252, 255, 245), entry.second, panelW - 46.0F * scale);
            y += 128.0F * scale;
        }
    }

    static std::shared_ptr<Util::Image> settingsButtonIcon = LoadHudIcon("Setting.png");
    if (settingsButtonIcon && settingsButtonIcon->GetTextureId() != 0) {
        DrawButtonImage(draw, settingsButtonIcon, ui.settingsButton, 0.0F, {0.0F, 0.0F}, {1.0F, 1.0F});
    } else {
        DrawTopUiButton(draw, ui.settingsButton, 4.0F * scale, false);
        DrawGearIcon(draw, RectCenter(ui.settingsButton), 17.0F * scale,
                     IM_COL32(242, 246, 252, 255), 2.6F * scale);
    }
}

// ── Compact right-side deployment info (DP + deployable count) ───────────────
void Ark::AppRenderer::DrawDeploymentInfo(float screenW, float screenH) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    static std::shared_ptr<Util::Image> dpIcon = LoadHudIcon("DP.png");

    const float scale = ComputeBattleUiLayout(screenW, screenH).scale;
    const float right = screenW - 36.0F * scale;
    const float dpBgW = 180.0F * scale;
    const float labelW = dpBgW * 1.5F;
    const float labelH = 48.0F * scale;
    const float labelY = screenH - OP_BAR_HEIGHT - labelH - 14.0F * scale;
    const float dpBgX = right - dpBgW;
    const float dpBgY = labelY - 73.0F * scale;
    const float dpBgH = 58.0F * scale;
    const float dpBgRight = right;
    const float labelPanelX = right - labelW;
    const int deployed = static_cast<int>(m_App.m_Operators.size());
    const int remaining = std::max(0, MAX_OPS - deployed);
    const char* label = u8"剩餘可部屬角色：";
    const std::string remainingStr = std::to_string(remaining);
    const float textSize = 32.0F * scale;
    const ImVec2 labelSize = MeasureText(label, textSize);
    const ImVec2 remainingSize = MeasureText(remainingStr.c_str(), textSize);
    const float contentW = labelSize.x + remainingSize.x;
    const float contentX = right - contentW - 18.0F * scale;

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

    const float iconBoxSize = 56.0F * scale;
    const float iconSize = iconBoxSize * 0.8F;
    const float iconX = dpBgX + 17.0F * scale + (iconBoxSize - iconSize) * 0.5F;
    const float iconY = dpBgY - 4.0F * scale + (iconBoxSize - iconSize) * 0.5F;
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
    const bool dpAtCap = m_App.m_DP >= m_App.m_MaxDP || static_cast<int>(m_App.m_DP) >= 99;
    const float regenProgress = dpAtCap ? 0.0F : std::clamp(m_App.m_DPRegenTimerMs / 1000.0F, 0.0F, 1.0F);
    draw->AddRectFilled({dpBgX, lineY}, {dpBgRight, lineY + lineH}, IM_COL32(35, 38, 43, 214));
    if (!dpAtCap) {
        draw->AddRectFilled({dpBgX, lineY},
                            {dpBgX + (dpBgRight - dpBgX) * regenProgress, lineY + lineH},
                            IM_COL32(255, 255, 255, 255));
    }

    const float textY = labelY + (labelH - textSize) * 0.5F - 2.0F * scale;

    AddStrongText(draw, {contentX, textY},
                  textSize, IM_COL32(250, 250, 250, 245), label);
    AddStrongText(draw, {contentX + labelSize.x, textY},
                  textSize, IM_COL32(250, 250, 250, 245), remainingStr.c_str());
}

void Ark::AppRenderer::DrawMissionCompleteOverlay(float screenW, float screenH) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    static std::shared_ptr<Util::Image> missionCompleteImage = LoadHudIcon("mission complete.png");

    const auto ui = ComputeBattleUiLayout(screenW, screenH);
    const float scale = ui.scale;
    const float t = std::clamp(m_App.m_ClearTimerMs, 0.0F, MISSION_COMPLETE_TOTAL_MS);
    const float outStart = MISSION_COMPLETE_SLIDE_MS + MISSION_COMPLETE_HOLD_MS;

    auto easeOut = [](float x) {
        x = std::clamp(x, 0.0F, 1.0F);
        const float inv = 1.0F - x;
        return 1.0F - inv * inv * inv;
    };
    auto easeIn = [](float x) {
        x = std::clamp(x, 0.0F, 1.0F);
        return x * x * x;
    };

    float alpha = 1.0F;
    float slide = 1.0F;
    bool slidingOut = false;
    if (t < MISSION_COMPLETE_SLIDE_MS) {
        slide = easeOut(t / MISSION_COMPLETE_SLIDE_MS);
        alpha = slide;
    } else if (t >= outStart) {
        slide = easeIn((t - outStart) / MISSION_COMPLETE_SLIDE_MS);
        alpha = 1.0F - slide;
        slidingOut = true;
    }

    const int dimAlpha = static_cast<int>(58.0F * alpha);
    if (dimAlpha > 0) {
        draw->AddRectFilled({0.0F, 0.0F}, {screenW, screenH}, IM_COL32(0, 0, 0, dimAlpha));
    }

    const float bandH = std::clamp(screenH * 0.234F, 166.0F * scale, 273.0F * scale);
    const float bandW = std::min(screenW * 1.04F, 2132.0F * scale);
    const float bandLeft = (screenW - bandW) * 0.5F;
    const float bandRight = bandLeft + bandW;
    const float bandY = screenH * 0.50F;
    const float bandTop = bandY - bandH * 0.5F;
    const float bandBottom = bandY + bandH * 0.5F;
    const float slant = 48.0F * scale;
    const int bandAlpha = static_cast<int>(178.0F * alpha);
    if (bandAlpha > 0) {
        draw->AddQuadFilled({bandLeft + slant, bandTop},
                            {bandRight, bandTop},
                            {bandRight - slant, bandBottom},
                            {bandLeft, bandBottom},
                            IM_COL32(0, 0, 0, bandAlpha));
        draw->AddRectFilled({bandLeft + slant * 0.35F, bandTop + bandH * 0.44F},
                            {bandRight - slant * 0.35F, bandBottom},
                            IM_COL32(0, 0, 0, static_cast<int>(42.0F * alpha)));
    }

    const float imageMaxW = std::min(screenW * 1.42F, 2430.0F * scale);
    const float imageMaxH = std::min(screenH * 0.945F, 1058.0F * scale);
    float drawW = imageMaxW;
    float drawH = imageMaxH;
    if (missionCompleteImage && missionCompleteImage->GetTextureId() != 0) {
        const glm::vec2 imageSize = missionCompleteImage->GetSize();
        if (imageSize.x > 0.0F && imageSize.y > 0.0F) {
            const float imageScale = std::min(imageMaxW / imageSize.x, imageMaxH / imageSize.y);
            drawW = imageSize.x * imageScale;
            drawH = imageSize.y * imageScale;
        }
    }

    const float centeredX = screenW * 0.50F;
    const float hiddenLeftX = -drawW * 0.56F;
    const float hiddenRightX = screenW + drawW * 0.56F;
    const float centerX = slidingOut
        ? centeredX + (hiddenRightX - centeredX) * slide
        : hiddenLeftX + (centeredX - hiddenLeftX) * slide;
    const float centerY = screenH * 0.50F - 4.0F * scale;
    const int imageAlpha = static_cast<int>(255.0F * alpha);

    if (missionCompleteImage && missionCompleteImage->GetTextureId() != 0) {
        draw->AddImage(reinterpret_cast<void*>(static_cast<intptr_t>(missionCompleteImage->GetTextureId())),
                       {centerX - drawW * 0.5F, centerY - drawH * 0.5F},
                       {centerX + drawW * 0.5F, centerY + drawH * 0.5F},
                       {0.0F, 0.0F},
                       {1.0F, 1.0F},
                       IM_COL32(255, 255, 255, imageAlpha));
    } else {
        AddCenteredText(draw, {centerX, centerY}, 185.0F * scale,
                        IM_COL32(255, 255, 255, imageAlpha), "MISSION ACCOMPLISHED");
    }
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
    static std::shared_ptr<Util::Image> pauseWordImage = LoadHudIcon("pause_word.png");
    static std::shared_ptr<Util::Image> quitLogoImage = LoadHudIcon("IQUIT.png");
    static std::shared_ptr<Util::Image> quitPanelImage = LoadHudIcon("QUIT.png");

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

    if (m_App.m_CheatMode) {
        const float badgeY = ui.speedButton.maxY + 8.0F * ui.scale;
        const UiRect badge{
            ui.speedButton.minX,
            badgeY,
            ui.pauseButton.maxX,
            badgeY + 30.0F * ui.scale
        };
        draw->AddRectFilled({badge.minX, badge.minY}, {badge.maxX, badge.maxY},
                            IM_COL32(128, 34, 24, 188), 3.0F * ui.scale);
        draw->AddRect({badge.minX, badge.minY}, {badge.maxX, badge.maxY},
                      IM_COL32(255, 147, 86, 210), 3.0F * ui.scale, 0, 1.2F * ui.scale);
        AddCenteredText(draw, RectCenter(badge), 19.0F * ui.scale,
                        IM_COL32(255, 234, 216, 255), "CHEAT x10");
    }

    // Pause overlay
    if (m_App.m_GamePaused && !m_App.m_ShowQuitConfirm && !m_App.m_GameOver && !m_App.m_MissionClear) {
        draw->AddRectFilled({0.0F, 0.0F}, {SW, SH}, IM_COL32(0, 0, 0, 128));
        const ImVec2 center{SW * 0.5F, SH * 0.5F - 10.0F * ui.scale};
        if (pauseWordImage && pauseWordImage->GetTextureId() != 0) {
            DrawIconImage(draw, pauseWordImage, center, 536.0F * ui.scale, 227.0F * ui.scale);
        } else {
            AddCenteredText(draw, {center.x + 2.0F, center.y - 25.0F * ui.scale + 2.0F},
                            112.0F * ui.scale, IM_COL32(0, 0, 0, 140), "PAUSE");
            AddCenteredText(draw, {center.x, center.y - 25.0F * ui.scale},
                            112.0F * ui.scale, IM_COL32(245, 245, 245, 255), "PAUSE");
            AddCenteredText(draw, {SW * 0.5F, SH * 0.5F + 62.0F * ui.scale},
                            56.0F * ui.scale, IM_COL32(238, 238, 238, 245), u8"----暫停中----");
        }
    }

    // Settings -> quit confirmation dialog
    if (m_App.m_ShowQuitConfirm && !m_App.m_GameOver && !m_App.m_MissionClear) {
        draw->AddRectFilled({0.0F, 0.0F}, {SW, SH}, IM_COL32(8, 10, 16, 174));

        const UiRect& panel = ui.quitPanel;
        const float panelW = panel.maxX - panel.minX;
        const float panelH = panel.maxY - panel.minY;
        const bool hasQuitPanelImage = quitPanelImage && quitPanelImage->GetTextureId() != 0;

        draw->AddRectFilled({0.0F, 0.0F}, {SW, panel.minY}, IM_COL32(0, 0, 0, 70));
        if (quitLogoImage && quitLogoImage->GetTextureId() != 0) {
            const ImVec2 logoCenter{SW * 0.5F, panel.minY * 0.58F};
            DrawIconImage(draw, quitLogoImage, logoCenter,
                          std::min(panelW * 0.26F, 360.0F * ui.scale),
                          panel.minY * 0.78F);
        }

        if (hasQuitPanelImage) {
            DrawButtonImage(draw, quitPanelImage, panel, 0.0F, {0.0F, 0.0F}, {1.0F, 1.0F});
        } else {
            const float headerH = std::max(90.0F * ui.scale, panelH * 0.20F);
            const float bodyTop = panel.minY + headerH;

            draw->AddRectFilled({panel.minX, panel.minY}, {panel.maxX, bodyTop},
                                IM_COL32(43, 47, 57, 244), 2.0F * ui.scale, ImDrawFlags_RoundCornersTop);
            draw->AddRectFilled({panel.minX, bodyTop}, {panel.maxX, panel.maxY},
                                IM_COL32(236, 236, 238, 252), 2.0F * ui.scale, ImDrawFlags_RoundCornersBottom);
            draw->AddLine({panel.minX, bodyTop}, {panel.maxX, bodyTop}, IM_COL32(60, 63, 70, 230), 2.0F);
            draw->AddRect({panel.minX, panel.minY}, {panel.maxX, panel.maxY},
                          IM_COL32(104, 110, 124, 180), 2.0F * ui.scale, 0, 1.5F);

            const ImVec2 logoCenter{panel.minX + panelW * 0.5F, panel.minY + headerH * 0.46F};
            AddCenteredText(draw, {logoCenter.x - 22.0F * ui.scale, logoCenter.y + 2.0F * ui.scale},
                            100.0F * ui.scale, IM_COL32(245, 245, 247, 255), "Q");
            AddCenteredText(draw, {logoCenter.x + 18.0F * ui.scale, logoCenter.y - 2.0F * ui.scale},
                            46.0F * ui.scale, IM_COL32(245, 245, 247, 255), "QUIT");

            const char* msgA = u8"放棄行動";
            const char* msgB = u8"將會恢復6理智";
            const char* msgC = u8"（全部返還）";
            const char* msgD = u8"：";
            const float msgSize = 50.0F * ui.scale;
            const ImVec2 sizeA = MeasureText(msgA, msgSize);
            const ImVec2 sizeB = MeasureText(msgB, msgSize);
            const ImVec2 sizeC = MeasureText(msgC, msgSize);
            const ImVec2 sizeD = MeasureText(msgD, msgSize);
            float msgX = panel.minX + (panelW - (sizeA.x + sizeB.x + sizeC.x + sizeD.x)) * 0.5F;
            const float msgY = bodyTop + 38.0F * ui.scale;
            draw->AddText(ImGui::GetFont(), msgSize, {msgX, msgY}, IM_COL32(244, 132, 41, 255), msgA);
            msgX += sizeA.x;
            draw->AddText(ImGui::GetFont(), msgSize, {msgX, msgY}, IM_COL32(47, 50, 58, 255), msgB);
            msgX += sizeB.x;
            draw->AddText(ImGui::GetFont(), msgSize, {msgX, msgY}, IM_COL32(244, 132, 41, 255), msgC);
            msgX += sizeC.x;
            draw->AddText(ImGui::GetFont(), msgSize, {msgX, msgY}, IM_COL32(47, 50, 58, 255), msgD);

            draw->AddRectFilled({ui.quitBackButton.minX, ui.quitBackButton.minY},
                                {ui.quitBackButton.maxX, ui.quitBackButton.maxY},
                                IM_COL32(18, 19, 24, 245), 0.0F);
            draw->AddRectFilled({ui.quitConfirmButton.minX, ui.quitConfirmButton.minY},
                                {ui.quitConfirmButton.maxX, ui.quitConfirmButton.maxY},
                                IM_COL32(137, 21, 26, 245), 0.0F);
            AddCenteredText(draw, RectCenter(ui.quitBackButton), 52.0F * ui.scale,
                            IM_COL32(245, 245, 248, 255), u8"返回");
            AddCenteredText(draw, RectCenter(ui.quitConfirmButton), 52.0F * ui.scale,
                            IM_COL32(248, 248, 248, 255), u8"放棄行動");
        }
    }

    if (m_App.m_GameOver) {
        draw->AddRectFilled({0,0},{SW,SH}, IM_COL32(0,0,0,145));
        const std::string t = "MISSION FAILED";
        auto ts = ImGui::CalcTextSize(t.c_str());
        draw->AddText({SW*0.5F-ts.x*0.5F, SH*0.5F-ts.y}, IM_COL32(255,120,120,255), t.c_str());
        const std::string h = "Press R to restart";
        auto hs = ImGui::CalcTextSize(h.c_str());
        draw->AddText({SW*0.5F-hs.x*0.5F, SH*0.5F+12}, COLOR_TEXT_MAIN, h.c_str());
    }
}
