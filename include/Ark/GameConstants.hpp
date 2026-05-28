#pragma once

namespace Ark::GameConst {

inline constexpr int MAX_OPS = 8;
inline constexpr float BEAM_DURATION_MS = 120.0F;
inline constexpr float REDEPLOY_COOLDOWN_MS = 90000.0F;
inline constexpr float OPERATOR_DEATH_ANIMATION_MAX_MS = 4500.0F;

inline constexpr float BAGPIPE_SP_PER_SKILL = 4.0F;
inline constexpr int BAGPIPE_MAX_CHARGES = 3;
inline constexpr float BAGPIPE_MAX_SP =
    BAGPIPE_SP_PER_SKILL * static_cast<float>(BAGPIPE_MAX_CHARGES);
inline constexpr float BAGPIPE_SKILL_DURATION_MS = 1000.0F;

} // namespace Ark::GameConst
