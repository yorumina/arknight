#pragma once

#include <algorithm>

namespace Ark::RendererConst {
inline constexpr ImU32 COLOR_GRID_WHITE_MAIN = IM_COL32(255, 255, 255, 205);
inline constexpr ImU32 COLOR_GRID_WHITE_FAINT= IM_COL32(255, 255, 255, 110);
inline constexpr ImU32 COLOR_GRID_WHITE_BOLD = IM_COL32(255, 255, 255, 235);
inline constexpr ImU32 COLOR_LOW_TOP          = IM_COL32(142, 142, 142, 255);
inline constexpr ImU32 COLOR_HIGH_TOP         = IM_COL32(255, 255, 255, 255);
inline constexpr ImU32 COLOR_HIGH_SIDE        = IM_COL32(76, 76, 76, 255);
inline constexpr ImU32 COLOR_SPAWN_TOP        = IM_COL32(215, 58, 58, 170);
inline constexpr ImU32 COLOR_SPAWN_SIDE       = IM_COL32(140, 34, 34, 140);
inline constexpr ImU32 COLOR_GOAL_TOP         = IM_COL32(65, 138, 245, 170);
inline constexpr ImU32 COLOR_GOAL_SIDE        = IM_COL32(35, 84, 165, 140);
inline constexpr ImU32 COLOR_TEXT_MAIN        = IM_COL32(236, 240, 245, 255);
inline constexpr ImU32 COLOR_TEXT_SUB         = IM_COL32(173, 183, 198, 255);

inline constexpr int MAX_OPS = 12;
inline constexpr float BEAM_DURATION_MS = 120.0F;
inline constexpr float BAGPIPE_SP_PER_SKILL = 4.0F;
inline constexpr int BAGPIPE_MAX_CHARGES = 3;
inline constexpr float BAGPIPE_SKILL_DURATION_MS = 1000.0F;
inline constexpr float PRE_STAGE_TOTAL_MS = 2000.0F;
inline constexpr float PRE_STAGE_FADE_MS = 500.0F;
inline constexpr float FINISH_FADE_TO_BLACK_MS = 700.0F;
inline constexpr float FINISH_BLACKOUT_MS = 1000.0F;
inline constexpr float FINISH_FADE_IN_MS = 700.0F;
inline constexpr float FINISH_FADE_OUT_MS = 700.0F;

inline constexpr float OP_BAR_HEIGHT = 116.0F;
inline constexpr float OP_CARD_WIDTH = 106.0F;
inline constexpr float OP_CARD_HEIGHT = 116.0F;
inline constexpr float OP_CARD_SPACING = 5.0F;
inline constexpr float OP_CARD_PORTRAIT_HEIGHT = 60.0F;
inline constexpr float OP_CARD_INFO_HEIGHT = 35.0F;
inline constexpr float OP_CARD_ROUNDING = 4.0F;

struct UiRect {
    float minX = 0.0F;
    float minY = 0.0F;
    float maxX = 0.0F;
    float maxY = 0.0F;

    bool Contains(float x, float y) const {
        return x >= minX && x <= maxX && y >= minY && y <= maxY;
    }
};

struct BattleUiLayout {
    float scale = 1.0F;
    UiRect settingsButton;
    UiRect speedButton;
    UiRect pauseButton;
    UiRect quitPanel;
    UiRect quitBackButton;
    UiRect quitConfirmButton;
};

inline BattleUiLayout ComputeBattleUiLayout(float screenW, float screenH) {
    BattleUiLayout layout{};
    layout.scale = std::clamp(std::min(screenW / 2048.0F, screenH / 1152.0F), 0.72F, 1.30F);

    const float margin = 22.0F * layout.scale;
    const float top = 20.0F * layout.scale;
    const float buttonHeight = 110.0F * layout.scale;
    const float settingsSize = 150.0F * layout.scale;
    const float speedWidth = 150.0F * layout.scale;
    const float pauseWidth = 118.0F * layout.scale;
    const float gap = 8.0F * layout.scale;

    layout.settingsButton = {margin, top, margin + settingsSize, top + buttonHeight};
    layout.pauseButton = {
        screenW - margin - pauseWidth,
        top,
        screenW - margin,
        top + buttonHeight
    };
    layout.speedButton = {
        layout.pauseButton.minX - gap - speedWidth,
        top,
        layout.pauseButton.minX - gap,
        top + buttonHeight
    };

    const float panelW = std::min(screenW * 0.92F, 1860.0F * layout.scale);
    const float panelH = std::min(screenH * 0.80F, 920.0F * layout.scale);
    const float panelX = (screenW - panelW) * 0.5F;
    const float panelY = (screenH - panelH) * 0.5F;
    layout.quitPanel = {panelX, panelY, panelX + panelW, panelY + panelH};

    const float buttonY = layout.quitPanel.maxY - 120.0F * layout.scale;
    const float midX = panelX + panelW * 0.5F;
    layout.quitBackButton = {panelX, buttonY, midX, layout.quitPanel.maxY};
    layout.quitConfirmButton = {midX, buttonY, panelX + panelW, layout.quitPanel.maxY};

    return layout;
}
} // namespace Ark::RendererConst
