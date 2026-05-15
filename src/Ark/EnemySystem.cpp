// ─────────────────────────────────────────────────────────────────────────────
// EnemySystem.cpp  –  Enemy spawning, movement & AI
// ─────────────────────────────────────────────────────────────────────────────
#include "App.hpp"

#include <algorithm>
#include <cmath>

using namespace Ark;

namespace {
constexpr float BEAM_DURATION_MS = 120.0F;
constexpr float ENEMY_SPEED_SCALE = 0.5F;
constexpr float MAX_DEATH_ANIMATION_MS = 4500.0F;

float ComputeEnemyToOperatorDamage(float rawDamage, float targetDef) {
    // Keep defense meaningful, but avoid "always 1 damage" stalemates.
    const float reduced = rawDamage - targetDef * 0.60F;
    const float floorByPercent = rawDamage * 0.10F;
    return std::max(5.0F, std::max(floorByPercent, reduced));
}
} // namespace

void App::MarkEnemyDefeated(Enemy& enemy) {
    enemy.hp = 0.0F;
    enemy.alive = false;
    enemy.blockedByOperatorId = -1;
    enemy.deathAnimationFinished = false;
    enemy.deathElapsedMs = 0.0F;
    enemy.animState = Enemy::AnimState::DIE;

    if (enemy.typeIndex < 0 ||
        enemy.typeIndex >= static_cast<int>(m_EnemyAnims.size())) {
        enemy.deathAnimationFinished = true;
        return;
    }

    auto& pack = m_EnemyAnims[static_cast<std::size_t>(enemy.typeIndex)];
    if (pack.die.Empty()) {
        pack.activeInstances.erase(enemy.id);
        pack.dieInstances.erase(enemy.id);
        enemy.deathAnimationFinished = true;
        return;
    }

    auto& anim = pack.dieInstances[enemy.id];
    if (!anim) {
        anim = std::make_shared<Util::Animation>(
            pack.die.mediaPath, true, pack.die.loop, false);
    } else {
        anim->Restart();
    }
    if (anim->GetFrameCount() == 0) {
        pack.die.mediaPath.clear();
        pack.activeInstances.erase(enemy.id);
        pack.dieInstances.erase(enemy.id);
        enemy.deathAnimationFinished = true;
        return;
    }
    pack.activeInstances[enemy.id] = anim;
}

void App::UpdateEnemyAnimation(Enemy& enemy, bool isMoving, bool isAttacking) {
    if (enemy.typeIndex < 0 ||
        enemy.typeIndex >= static_cast<int>(m_EnemyAnims.size())) {
        if (!enemy.alive) enemy.deathAnimationFinished = true;
        return;
    }

    auto& pack = m_EnemyAnims[static_cast<std::size_t>(enemy.typeIndex)];

    auto selectClip = [&](Enemy::AnimState state) -> EnemyAnimClip* {
        switch (state) {
        case Enemy::AnimState::IDLE:
            if (!pack.idle.Empty()) return &pack.idle;
            if (!pack.move.Empty()) return &pack.move;
            return nullptr;
        case Enemy::AnimState::MOVE:
            if (!pack.move.Empty()) return &pack.move;
            if (!pack.idle.Empty()) return &pack.idle;
            return nullptr;
        case Enemy::AnimState::ATTACK:
            return pack.attack.Empty() ? nullptr : &pack.attack;
        case Enemy::AnimState::DIE:
            return pack.die.Empty() ? nullptr : &pack.die;
        }
        return nullptr;
    };

    auto it = pack.activeInstances.find(enemy.id);
    auto animInst = (it != pack.activeInstances.end()) ? it->second : nullptr;

    Enemy::AnimState desiredState = isMoving ? Enemy::AnimState::MOVE : Enemy::AnimState::IDLE;
    if (!enemy.alive) {
        desiredState = Enemy::AnimState::DIE;
    } else if (isAttacking && selectClip(Enemy::AnimState::ATTACK) != nullptr) {
        desiredState = Enemy::AnimState::ATTACK;
    } else if (enemy.animState == Enemy::AnimState::ATTACK &&
               animInst &&
               animInst->GetState() != Util::Animation::State::ENDED) {
        desiredState = Enemy::AnimState::ATTACK;
    }

    EnemyAnimClip* clip = selectClip(desiredState);
    if (clip == nullptr) {
        if (!enemy.alive) {
            enemy.deathAnimationFinished = true;
            pack.activeInstances.erase(enemy.id);
        }
        return;
    }

    const bool useSharedLoop =
        desiredState == Enemy::AnimState::IDLE ||
        desiredState == Enemy::AnimState::MOVE;
    if (useSharedLoop) {
        auto& sharedInstance = desiredState == Enemy::AnimState::IDLE
            ? pack.sharedIdleInstance
            : pack.sharedMoveInstance;
        auto& sharedUpdateSerial = desiredState == Enemy::AnimState::IDLE
            ? pack.sharedIdleUpdateSerial
            : pack.sharedMoveUpdateSerial;

        if (!sharedInstance) {
            sharedInstance = std::make_shared<Util::Animation>(
                clip->mediaPath, true, clip->loop, false);
            if (sharedInstance->GetFrameCount() == 0) {
                clip->mediaPath.clear();
                pack.activeInstances.erase(enemy.id);
                return;
            }
        }

        enemy.animState = desiredState;
        pack.activeInstances[enemy.id] = sharedInstance;
        if (sharedUpdateSerial != m_EnemyAnimationUpdateSerial) {
            sharedInstance->Update();
            sharedUpdateSerial = m_EnemyAnimationUpdateSerial;
        }
        return;
    }

    const bool shouldRestartAnimation =
        isAttacking && desiredState == Enemy::AnimState::ATTACK;
    if (enemy.animState != desiredState || !animInst || shouldRestartAnimation) {
        enemy.animState = desiredState;
        auto& stateInstances = desiredState == Enemy::AnimState::ATTACK
            ? pack.attackInstances
            : pack.dieInstances;
        auto& cachedAnim = stateInstances[enemy.id];
        if (!cachedAnim) {
            cachedAnim = std::make_shared<Util::Animation>(
                clip->mediaPath, true, clip->loop, false);
        } else {
            cachedAnim->Restart();
        }
        animInst = cachedAnim;
        if (animInst->GetFrameCount() == 0) {
            clip->mediaPath.clear();
            pack.activeInstances.erase(enemy.id);
            stateInstances.erase(enemy.id);
            if (desiredState == Enemy::AnimState::DIE) {
                enemy.deathAnimationFinished = true;
            }
            return;
        }
        pack.activeInstances[enemy.id] = animInst;
    }

    if (animInst) {
        animInst->Update();
        if (desiredState == Enemy::AnimState::DIE &&
            animInst->GetState() == Util::Animation::State::ENDED) {
            enemy.deathAnimationFinished = true;
        }
    }
}

// ─── Spawn ──────────────────────────────────────────────────────────────────
void App::SpawnEnemy(const WavePlan& plan) {
    if (plan.routeIndex < 0 || plan.enemyTypeIndex < 0 ||
        plan.routeIndex >= static_cast<int>(m_Routes.size()) ||
        plan.enemyTypeIndex >= static_cast<int>(m_EnemyTemplates.size())) return;

    const auto& route  = m_Routes[static_cast<std::size_t>(plan.routeIndex)];
    const auto& tmpl   = m_EnemyTemplates[static_cast<std::size_t>(plan.enemyTypeIndex)];
    if (route.nodes.empty()) return;

    Enemy e;
    e.id               = m_NextEnemyId++;
    e.typeIndex        = plan.enemyTypeIndex;
    e.routeIndex       = plan.routeIndex;
    e.nodeIndex        = 0;
    e.boardPos         = route.nodes.front().boardPos;
    e.waitSec          = std::max(0.0F, route.nodes.front().waitSec);
    e.maxHp            = tmpl.hp;
    e.hp               = tmpl.hp;
    e.speed            = tmpl.speed;
    e.damage           = tmpl.damage;
    e.attackIntervalMs = tmpl.attackIntervalMs;
    e.attackCooldownMs = tmpl.attackIntervalMs; // start with full cd
    e.def              = tmpl.def;
    e.attackRange      = tmpl.attackRange;
    e.isRanged         = tmpl.isRanged;
    e.canAttackOperator= tmpl.canAttackOperator;
    e.color            = tmpl.color;
    e.alive            = true;
    m_Enemies.push_back(e);
}

// ─── Enemy update ───────────────────────────────────────────────────────────
void App::UpdateEnemies(float dtSec) {
    ++m_EnemyAnimationUpdateSerial;
    const float scaledDtSec = dtSec * ENEMY_SPEED_SCALE;

    // Reset block counts
    for (auto& op : m_Operators) op.blockedEnemyCount = 0;

    // Re-validate existing blocks
    for (auto& enemy : m_Enemies) {
        if (!enemy.alive || enemy.blockedByOperatorId < 0) continue;
        auto it = std::find_if(m_Operators.begin(), m_Operators.end(),
            [&](const Operator& op){ return op.id == enemy.blockedByOperatorId && op.hp > 0; });
        if (it != m_Operators.end()) {
            int blockCap = m_OperatorTemplates.at(it->typeIndex).blockCount;
            if (it->skillActive && (m_OperatorTemplates.at(it->typeIndex).name == "Myrtle" ||
                                    m_OperatorTemplates.at(it->typeIndex).name == "桃金娘")) blockCap = 0;
            if (it->blockedEnemyCount < blockCap) ++it->blockedEnemyCount;
            else enemy.blockedByOperatorId = -1;
        } else {
            enemy.blockedByOperatorId = -1;
        }
    }

    for (auto& enemy : m_Enemies) {
        if (!enemy.alive) {
            if (!enemy.deathAnimationFinished) {
                enemy.deathElapsedMs += dtSec * 1000.0F;
                UpdateEnemyAnimation(enemy, false, false);
                if (enemy.deathElapsedMs >= MAX_DEATH_ANIMATION_MS) {
                    enemy.deathAnimationFinished = true;
                }
            }
            continue;
        }

        const glm::vec2 frameStartPos = enemy.boardPos;
        bool didAttack = false;

        // Find blocking operator
        Operator* blockOp = nullptr;
        if (enemy.blockedByOperatorId >= 0) {
            auto it = std::find_if(m_Operators.begin(), m_Operators.end(),
                [&](const Operator& op){ return op.id == enemy.blockedByOperatorId && op.hp > 0; });
            if (it != m_Operators.end()) blockOp = &(*it);
        }

        // Try to acquire a block
        bool acquiredBlockThisFrame = false;
        if (!blockOp) {
            glm::ivec2 eCell(static_cast<int>(std::floor(enemy.boardPos.x)),
                             static_cast<int>(std::floor(enemy.boardPos.y)));
            for (auto& op : m_Operators) {
                if (op.hp <= 0) continue;
                if (op.cell != eCell) continue;
                int blockCap = m_OperatorTemplates.at(op.typeIndex).blockCount;
                if (op.skillActive && (m_OperatorTemplates.at(op.typeIndex).name == "Myrtle" ||
                                       m_OperatorTemplates.at(op.typeIndex).name == "桃金娘")) blockCap = 0;
                if (op.blockedEnemyCount < blockCap) {
                    blockOp = &op;
                    enemy.blockedByOperatorId = op.id;
                    ++op.blockedEnemyCount;
                    acquiredBlockThisFrame = true;
                    break;
                }
            }
        }

        if (blockOp) {
            // Close-range melee attack on the blocking operator
            if (enemy.canAttackOperator) {
                // Hit once immediately when contact/block is established,
                // then respect attack interval for subsequent hits.
                if (acquiredBlockThisFrame) {
                    enemy.attackCooldownMs = 0.0F;
                }
                enemy.attackCooldownMs -= scaledDtSec * 1000.0F;
                if (enemy.attackCooldownMs <= 0.0F) {
                    blockOp->hp -= ComputeEnemyToOperatorDamage(enemy.damage, blockOp->def);
                    blockOp->hp = std::max(0.0F, blockOp->hp);
                    enemy.attackCooldownMs = enemy.attackIntervalMs;
                    didAttack = true;
                }
            }
            UpdateEnemyAnimation(enemy, false, didAttack);
            continue; // blocked – don't move
        }

        // ── Ranged attacker: scans all operators in range ────────────────
        if (enemy.isRanged && enemy.attackRange > 0.0F) {
            enemy.attackCooldownMs -= scaledDtSec * 1000.0F;
            if (enemy.attackCooldownMs <= 0.0F) {
                // Find nearest operator within attack range
                Operator* rangedTarget = nullptr;
                float bestDistSq = enemy.attackRange * enemy.attackRange;
                for (auto& op : m_Operators) {
                    if (op.hp <= 0) continue;
                    // Range check in board space (tile distance)
                    const auto opBoardCenter = ToBoardCenter(op.cell);
                    const auto delta = opBoardCenter - enemy.boardPos;
                    const float distSq = glm::dot(delta, delta);
                    if (distSq <= bestDistSq) {
                        bestDistSq = distSq;
                        rangedTarget = &op;
                    }
                }
                if (rangedTarget) {
                    rangedTarget->hp -= ComputeEnemyToOperatorDamage(enemy.damage, rangedTarget->def);
                    rangedTarget->hp = std::max(0.0F, rangedTarget->hp);
                    // Visual beam: enemy -> operator
                    m_Beams.push_back(AttackBeam{enemy.boardPos, ToBoardCenter(rangedTarget->cell), BEAM_DURATION_MS});
                    didAttack = true;
                }
                enemy.attackCooldownMs = enemy.attackIntervalMs;
            }
            // Ranged enemies still walk (don't stop)
        }

        // Move along route
        if (enemy.routeIndex < 0 || enemy.routeIndex >= static_cast<int>(m_Routes.size())) {
            enemy.alive = false;
            enemy.deathAnimationFinished = true;
            continue;
        }
        const auto& route = m_Routes[static_cast<std::size_t>(enemy.routeIndex)];
        float timeLeft = scaledDtSec;

        while (timeLeft > 0.0F && enemy.alive) {
            if (enemy.waitSec > 0.0F) {
                const float c = std::min(enemy.waitSec, timeLeft);
                enemy.waitSec -= c; timeLeft -= c;
                if (enemy.waitSec > 0.0F) break;
            }
            if (enemy.nodeIndex + 1 >= route.nodes.size()) {
                enemy.alive = false;
                enemy.deathAnimationFinished = true;
                m_LifePoint = std::max(0, m_LifePoint - 1);
                if (m_LifePoint <= 0) { m_GameOver = true; m_WaveRunning = false; }
                break;
            }
            const auto tgt   = route.nodes[enemy.nodeIndex + 1].boardPos;
            const auto toTgt = tgt - enemy.boardPos;
            const float dist = glm::length(toTgt);
            if (dist <= 0.0001F) {
                enemy.boardPos = tgt;
                enemy.nodeIndex++;
                enemy.waitSec = std::max(0.0F, route.nodes[enemy.nodeIndex].waitSec);
                continue;
            }
            const float spd  = std::max(enemy.speed, 0.01F);
            const float trvl = dist / spd;
            if (trvl <= timeLeft) {
                enemy.boardPos = tgt;
                enemy.nodeIndex++;
                timeLeft -= trvl;
                enemy.waitSec = std::max(0.0F, route.nodes[enemy.nodeIndex].waitSec);
            } else {
                enemy.boardPos += toTgt / dist * (spd * timeLeft);
                timeLeft = 0.0F;
            }
        }

        if (enemy.alive) {
            const bool isMoving = glm::length(enemy.boardPos - frameStartPos) > 0.0001F;
            UpdateEnemyAnimation(enemy, isMoving, didAttack);
        }
    }
}

void App::CleanupDefeatedEnemies() {
    m_Enemies.erase(std::remove_if(m_Enemies.begin(), m_Enemies.end(),
        [&](const Enemy& e) {
            const bool shouldRemove = !e.alive && e.deathAnimationFinished;
            if (shouldRemove &&
                e.typeIndex >= 0 &&
                e.typeIndex < static_cast<int>(m_EnemyAnims.size())) {
                auto& pack = m_EnemyAnims[static_cast<std::size_t>(e.typeIndex)];
                pack.activeInstances.erase(e.id);
                pack.attackInstances.erase(e.id);
                pack.dieInstances.erase(e.id);
            }
            return shouldRemove;
        }), m_Enemies.end());
}
