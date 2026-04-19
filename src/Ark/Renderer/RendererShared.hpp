#pragma once

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
inline constexpr float PRE_STAGE_TOTAL_MS = 3000.0F;
inline constexpr float PRE_STAGE_FADE_MS = 500.0F;
inline constexpr float FINISH_FADE_TO_BLACK_MS = 700.0F;
inline constexpr float FINISH_BLACKOUT_MS = 1000.0F;
inline constexpr float FINISH_FADE_IN_MS = 700.0F;
inline constexpr float FINISH_FADE_OUT_MS = 700.0F;

inline constexpr float OP_BAR_HEIGHT = 100.0F;
inline constexpr float OP_CARD_WIDTH = 80.0F;
inline constexpr float OP_CARD_HEIGHT = 95.0F;
inline constexpr float OP_CARD_SPACING = 6.0F;
inline constexpr float OP_CARD_PORTRAIT_HEIGHT = 60.0F;
inline constexpr float OP_CARD_INFO_HEIGHT = 35.0F;
inline constexpr float OP_CARD_ROUNDING = 4.0F;
} // namespace Ark::RendererConst
