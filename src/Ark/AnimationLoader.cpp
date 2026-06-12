#include "Ark/AnimationLoader.hpp"

#include "Ark/StageLoader.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace Ark {
namespace {

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string ExtensionOf(const std::filesystem::path& path) {
    return ToLower(path.extension().string());
}

bool IsApngPath(const std::filesystem::path& path) {
    return ExtensionOf(path) == ".apng";
}

bool IsSupportedAnimationPath(const std::filesystem::path& path) {
    const auto ext = ExtensionOf(path);
    return ext == ".webm" || ext == ".apng";
}

bool SameStem(const std::filesystem::path& lhs, const std::filesystem::path& rhs) {
    return ToLower(lhs.stem().string()) == ToLower(rhs.stem().string());
}

bool EndsWith(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string StripFlipSuffix(std::string animName) {
    if (EndsWith(animName, "_flip")) {
        animName.erase(animName.size() - 5);
    } else if (EndsWith(animName, "_flipped")) {
        animName.erase(animName.size() - 8);
    }
    return animName;
}

AnimationClip MakeClip(const std::filesystem::path& mediaPath, bool loop) {
    AnimationClip clip;
    clip.mediaPath = mediaPath.string();
    clip.loop = loop;
    return clip;
}

bool ShouldPreferClip(const AnimationClip& target, const AnimationClip& clip) {
    if (target.Empty() || clip.Empty()) return false;
    const std::filesystem::path targetPath(target.mediaPath);
    const std::filesystem::path clipPath(clip.mediaPath);
    return SameStem(targetPath, clipPath) &&
           !IsApngPath(targetPath) &&
           IsApngPath(clipPath);
}

void AssignClip(AnimationClip& target, AnimationClip clip, bool force = false) {
    if (clip.Empty()) return;
    if (target.Empty() ||
        ShouldPreferClip(target, clip) ||
        (force && !ShouldPreferClip(clip, target))) {
        target = std::move(clip);
    }
}

} // namespace

std::vector<OperatorAnimationClips> LoadOperatorAnimationClips(
    const std::vector<OperatorTemplate>& operatorTemplates) {
    std::vector<OperatorAnimationClips> packs(operatorTemplates.size());

    auto classifyAndAssign = [](const std::filesystem::path& mediaPath,
                                AnimationClip& start,
                                AnimationClip& def,
                                AnimationClip& attack,
                                AnimationClip& skill,
                                AnimationClip& die) {
        const std::string animName = ToLower(mediaPath.stem().string());

        const bool isDefaultAnim = animName.find("default") != std::string::npos;
        const bool isIdleAnim = animName.find("idle") != std::string::npos;

        if (isDefaultAnim || isIdleAnim) {
            AssignClip(def, MakeClip(mediaPath, true), isDefaultAnim);
        } else if (animName.find("start") != std::string::npos &&
                   animName.find("skill") == std::string::npos) {
            AssignClip(start, MakeClip(mediaPath, false));
        } else if (animName.find("attack") != std::string::npos) {
            const bool isMainAttack =
                animName.find("begin") == std::string::npos &&
                animName.find("end") == std::string::npos;
            AssignClip(attack, MakeClip(mediaPath, false), isMainAttack);
        } else if (animName.find("skill") != std::string::npos) {
            AssignClip(skill, MakeClip(mediaPath, true),
                       animName.find("loop") != std::string::npos);
        } else if (animName.find("die") != std::string::npos) {
            AssignClip(die, MakeClip(mediaPath, false));
        }
    };

    auto scanDir = [&](const std::filesystem::path& dir,
                       AnimationClip& start,
                       AnimationClip& def,
                       AnimationClip& attack,
                       AnimationClip& skill,
                       AnimationClip& die) {
        if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) return;

        std::vector<std::filesystem::path> mediaFiles;
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            if (!IsSupportedAnimationPath(entry.path())) continue;
            mediaFiles.push_back(entry.path());
        }
        std::sort(mediaFiles.begin(), mediaFiles.end());
        for (const auto& mediaPath : mediaFiles) {
            classifyAndAssign(mediaPath, start, def, attack, skill, die);
        }
    };

    const auto operatorDir = ResolveOperatorDir();
    if (operatorDir.empty()) return packs;

    for (std::size_t i = 0; i < operatorTemplates.size(); ++i) {
        const auto photoDir = operatorDir / operatorTemplates[i].id / "photo";
        if (!std::filesystem::exists(photoDir) || !std::filesystem::is_directory(photoDir)) continue;

        auto& pack = packs[i];
        scanDir(photoDir / "front",
                pack.start, pack.def, pack.attack, pack.skill, pack.die);
        scanDir(photoDir / "back",
                pack.startBack, pack.defBack, pack.attackBack, pack.skillBack, pack.dieBack);
        scanDir(photoDir / "front_flip",
                pack.startFlip, pack.defFlip, pack.attackFlip, pack.skillFlip, pack.dieFlip);
    }

    return packs;
}

std::vector<EnemyAnimationClips> LoadEnemyAnimationClips(
    const std::vector<EnemyTemplate>& enemyTemplates) {
    std::vector<EnemyAnimationClips> packs(enemyTemplates.size());

    const auto enemyDir = ResolveEnemyDir();
    if (enemyDir.empty()) return packs;

    for (std::size_t i = 0; i < enemyTemplates.size(); ++i) {
        const auto& enemyType = enemyTemplates[i];
        const std::string enemyCode = !enemyType.enemyId.empty() ? enemyType.enemyId : enemyType.id;
        const std::string enemyCodeLower = ToLower(enemyCode);
        auto& pack = packs[i];

        auto classifyAndAssign = [&](const std::filesystem::path& mediaPath) {
            const std::string rawAnimName = ToLower(mediaPath.stem().string());
            const bool isFlip = EndsWith(rawAnimName, "_flip") || EndsWith(rawAnimName, "_flipped");
            if (isFlip) return;
            const std::string animName = StripFlipSuffix(rawAnimName);
            const bool isExactIdle = animName == enemyCodeLower + "_idle" || animName == "idle";
            const bool isExactMove = animName == enemyCodeLower + "_move" || animName == "move";
            const bool isExactAttack = animName == enemyCodeLower + "_attack" || animName == "attack";
            const bool isExactDie = animName == enemyCodeLower + "_die" || animName == "die";

            AnimationClip& idleTarget = pack.idle;
            AnimationClip& moveTarget = pack.move;
            AnimationClip& attackTarget = pack.attack;
            AnimationClip& dieTarget = pack.die;

            if (animName.find("die") != std::string::npos) {
                AssignClip(dieTarget, MakeClip(mediaPath, false), isExactDie);
            } else if (animName.find("attack") != std::string::npos) {
                const bool isMainAttack = isExactAttack ||
                    (animName.find("begin") == std::string::npos &&
                     animName.find("end") == std::string::npos);
                AssignClip(attackTarget, MakeClip(mediaPath, false), isMainAttack);
            } else if (animName.find("idle") != std::string::npos ||
                       animName.find("default") != std::string::npos) {
                AssignClip(idleTarget, MakeClip(mediaPath, true), isExactIdle);
            } else if (animName.find("move") != std::string::npos) {
                const bool isMainMove = isExactMove ||
                    animName.find("loop") != std::string::npos;
                AssignClip(moveTarget, MakeClip(mediaPath, true), isMainMove);
            }
        };

        auto scanDir = [&](const std::filesystem::path& dir, bool filterToEnemyPrefix) {
            if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) return;

            std::vector<std::filesystem::path> mediaFiles;
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (!entry.is_regular_file()) continue;
                if (!IsSupportedAnimationPath(entry.path())) continue;
                if (filterToEnemyPrefix) {
                    const auto stem = ToLower(entry.path().stem().string());
                    const auto prefix = enemyCodeLower + "_";
                    if (stem != enemyCodeLower && stem.rfind(prefix, 0) != 0) continue;
                }
                mediaFiles.push_back(entry.path());
            }
            std::sort(mediaFiles.begin(), mediaFiles.end());
            for (const auto& mediaPath : mediaFiles) {
                classifyAndAssign(mediaPath);
            }
        };

        scanDir(enemyDir / enemyCode, false);
        scanDir(enemyDir, true);
    }

    return packs;
}

} // namespace Ark
