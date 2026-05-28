#pragma once

#include "Ark/ArkTypes.hpp"

#include <string>
#include <vector>

namespace Ark {

struct AnimationClip {
    std::string mediaPath;
    bool loop = true;

    bool Empty() const { return mediaPath.empty(); }
};

struct OperatorAnimationClips {
    AnimationClip start;
    AnimationClip def;
    AnimationClip attack;
    AnimationClip skill;
    AnimationClip die;

    AnimationClip startBack;
    AnimationClip defBack;
    AnimationClip attackBack;
    AnimationClip skillBack;
    AnimationClip dieBack;

    AnimationClip startFlip;
    AnimationClip defFlip;
    AnimationClip attackFlip;
    AnimationClip skillFlip;
    AnimationClip dieFlip;
};

struct EnemyAnimationClips {
    AnimationClip idle;
    AnimationClip move;
    AnimationClip attack;
    AnimationClip die;
};

std::vector<OperatorAnimationClips> LoadOperatorAnimationClips(
    const std::vector<OperatorTemplate>& operatorTemplates);

std::vector<EnemyAnimationClips> LoadEnemyAnimationClips(
    const std::vector<EnemyTemplate>& enemyTemplates);

} // namespace Ark
