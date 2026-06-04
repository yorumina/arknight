#pragma once

#include "Ark/GameConstants.hpp"

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

inline constexpr int MAX_OPS = Ark::GameConst::MAX_OPS;
inline constexpr float BEAM_DURATION_MS = Ark::GameConst::BEAM_DURATION_MS;
inline constexpr float BAGPIPE_SP_PER_SKILL = Ark::GameConst::BAGPIPE_SP_PER_SKILL;
inline constexpr int BAGPIPE_MAX_CHARGES = Ark::GameConst::BAGPIPE_MAX_CHARGES;
inline constexpr float BAGPIPE_SKILL_DURATION_MS = Ark::GameConst::BAGPIPE_SKILL_DURATION_MS;
inline constexpr float OPERATOR_VISUAL_SCALE = 1.1F;
inline constexpr float ENEMY_VISUAL_SCALE = 1.2F;
inline constexpr float PRE_STAGE_TOTAL_MS = 2000.0F;
inline constexpr float PRE_STAGE_FADE_MS = 500.0F;
inline constexpr float FINISH_FADE_TO_BLACK_MS = 700.0F;
inline constexpr float FINISH_BLACKOUT_MS = 1000.0F;
inline constexpr float FINISH_FADE_IN_MS = Ark::GameConst::MISSION_RESULT_FADE_IN_MS;
inline constexpr float FINISH_FADE_OUT_MS = Ark::GameConst::MISSION_RESULT_FADE_OUT_MS;
inline constexpr float MISSION_COMPLETE_SLIDE_MS = Ark::GameConst::MISSION_COMPLETE_SLIDE_MS;
inline constexpr float MISSION_COMPLETE_HOLD_MS = Ark::GameConst::MISSION_COMPLETE_HOLD_MS;
inline constexpr float MISSION_COMPLETE_TOTAL_MS = Ark::GameConst::MISSION_COMPLETE_TOTAL_MS;
inline constexpr float QUIT_PANEL_ASPECT = 2611.0F / 671.0F;
inline constexpr float QUIT_PANEL_BUTTON_HEIGHT_RATIO = 120.0F / 671.0F;

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
    UiRect mapToggleButton;
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
    const float topButtonWidth = 150.0F * layout.scale;
    const float settingsScale = 1.20F;
    const float gap = 8.0F * layout.scale;

    layout.settingsButton = {
        margin,
        top,
        margin + topButtonWidth * settingsScale,
        top + buttonHeight * settingsScale
    };
    const float utilityButtonW = 110.0F * layout.scale;
    const float utilityButtonH = 52.0F * layout.scale;
    layout.mapToggleButton = {
        layout.settingsButton.maxX + gap,
        top + (buttonHeight * settingsScale - utilityButtonH) * 0.5F,
        layout.settingsButton.maxX + gap + utilityButtonW,
        top + (buttonHeight * settingsScale + utilityButtonH) * 0.5F
    };
    layout.pauseButton = {
        screenW - margin - topButtonWidth,
        top,
        screenW - margin,
        top + buttonHeight
    };
    layout.speedButton = {
        layout.pauseButton.minX - gap - topButtonWidth,
        top,
        layout.pauseButton.minX - gap,
        top + buttonHeight
    };

    float panelW = std::min(screenW * 0.936F, 1916.0F * layout.scale);
    float panelH = panelW / QUIT_PANEL_ASPECT;
    const float maxPanelH = screenH * 0.56F;
    if (panelH > maxPanelH) {
        panelH = maxPanelH;
        panelW = panelH * QUIT_PANEL_ASPECT;
    }
    const float panelX = (screenW - panelW) * 0.5F;
    const float bottomMargin = 24.0F * layout.scale;
    const float preferredPanelY = screenH * 0.345F;
    const float panelY = std::min(preferredPanelY, std::max(bottomMargin, screenH - panelH - bottomMargin));
    layout.quitPanel = {panelX, panelY, panelX + panelW, panelY + panelH};

    const float buttonY = layout.quitPanel.maxY - panelH * QUIT_PANEL_BUTTON_HEIGHT_RATIO;
    const float midX = panelX + panelW * 0.5F;
    layout.quitBackButton = {panelX, buttonY, midX, layout.quitPanel.maxY};
    layout.quitConfirmButton = {midX, buttonY, panelX + panelW, layout.quitPanel.maxY};

    return layout;
}
} // namespace Ark::RendererConst
