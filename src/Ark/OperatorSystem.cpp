// ─────────────────────────────────────────────────────────────────────────────
// OperatorSystem.cpp  –  Operator combat, skills, animation & deployment helpers
// ─────────────────────────────────────────────────────────────────────────────
#include "App.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

using namespace Ark;

namespace {
constexpr int   MAX_OPS          = 12;
constexpr float BEAM_DURATION_MS = 120.0F;
constexpr float KILL_REWARD_DP   = 1.5F;
constexpr float BAGPIPE_SP_PER_SKILL = 4.0F;
constexpr int   BAGPIPE_MAX_CHARGES = 3;
constexpr float BAGPIPE_MAX_SP = BAGPIPE_SP_PER_SKILL * static_cast<float>(BAGPIPE_MAX_CHARGES);
constexpr float BAGPIPE_SKILL_DURATION_MS = 1000.0F;

bool IsBagpipe(const OperatorTemplate& opType) {
    return opType.name == "Bagpipe" || opType.name == "風笛";
}

bool IsMyrtle(const OperatorTemplate& opType) {
    return opType.name == "Myrtle" || opType.name == "桃金娘";
}
} // namespace

// ─── Operator update ────────────────────────────────────────────────────────
void App::UpdateOperators(float dtMs) {
    bool myrtleOnField = std::any_of(m_Operators.begin(), m_Operators.end(),
        [&](const Operator& op) {
            if (op.hp <= 0) return false;
            const auto& t = m_OperatorTemplates.at(op.typeIndex);
            return t.name == "Myrtle" || t.name == "桃金娘";
        });

    const float dtSec = dtMs / 1000.0F;

    for (auto& op : m_Operators) {
        if (op.hp <= 0) {
            if (op.animState != Ark::Operator::AnimState::DIE) {
                op.animState = Ark::Operator::AnimState::DIE;
                if (m_OperatorAnims[op.typeIndex].die) {
                    m_OperatorAnims[op.typeIndex].activeInstances[op.id] = std::make_shared<Util::Animation>(
                        m_OperatorAnims[op.typeIndex].die->GetFrames(), true, 41, false, 0);
                }
            }
            // Keep updating death animation until gone
            auto& inst = m_OperatorAnims[op.typeIndex].activeInstances[op.id];
            if (inst) inst->Update();
            continue;
        }

        const auto& opType = m_OperatorTemplates.at(static_cast<std::size_t>(op.typeIndex));
        auto& pack = m_OperatorAnims[op.typeIndex];

        // Initialize state if missing
        if (pack.activeInstances.find(op.id) == pack.activeInstances.end()) {
            if (pack.start) {
                op.animState = Ark::Operator::AnimState::START;
                pack.activeInstances[op.id] = std::make_shared<Util::Animation>(pack.start->GetFrames(), true, 41, false, 0);
            } else if (pack.def) {
                op.animState = Ark::Operator::AnimState::DEFAULT;
                pack.activeInstances[op.id] = std::make_shared<Util::Animation>(pack.def->GetFrames(), true, 41, true, 0);
            } else {
                op.animState = Ark::Operator::AnimState::DEFAULT;
            }
        }

        auto& animInst = pack.activeInstances[op.id];
        
        // State transitions
        if (animInst) {
            if (op.animState == Ark::Operator::AnimState::START && animInst->GetState() == Util::Animation::State::ENDED) {
                op.animState = Ark::Operator::AnimState::DEFAULT;
                if (pack.def) animInst = std::make_shared<Util::Animation>(pack.def->GetFrames(), true, 41, true, 0);
            } else if (op.animState == Ark::Operator::AnimState::ATTACK && animInst->GetState() == Util::Animation::State::ENDED) {
                op.animState = Ark::Operator::AnimState::DEFAULT;
                if (pack.def) animInst = std::make_shared<Util::Animation>(pack.def->GetFrames(), true, 41, true, 0);
            }
            
            // Skill state tracking
            if (op.skillActive && op.animState != Ark::Operator::AnimState::SKILL && pack.skill) {
                op.animState = Ark::Operator::AnimState::SKILL;
                animInst = std::make_shared<Util::Animation>(pack.skill->GetFrames(), true, 41, true, 0);
            } else if (!op.skillActive && op.animState == Ark::Operator::AnimState::SKILL) {
                op.animState = Ark::Operator::AnimState::DEFAULT;
                if (pack.def) animInst = std::make_shared<Util::Animation>(pack.def->GetFrames(), true, 41, true, 0);
            }

            animInst->Update();
        }

        const bool isBagpipe = IsBagpipe(opType);
        const bool isMyrtle = IsMyrtle(opType);

        // Myrtle talent: all vanguards regen 33 HP/s
        if (myrtleOnField && opType.isVanguard)
            op.hp = std::min(op.maxHp, op.hp + 33.0F * dtSec);

        // Skill timer / SP regen
        if (op.skillActive) {
            op.skillTimerMs -= dtMs;
            if (op.skillTimerMs <= 0.0F) {
                op.skillActive = false;
                op.skillTimerMs = 0.0F;
                if (!isBagpipe) {
                    op.sp = 0.0F;
                }
            }
        } else if (opType.maxSp > 0) {
            if (isBagpipe) {
                op.sp = std::min(BAGPIPE_MAX_SP, op.sp + 1.0F * dtSec);
            } else {
                op.sp = std::min(opType.maxSp, op.sp + 1.0F * dtSec);
            }
        }

        // Myrtle skill active: no attacking
        if (isMyrtle && op.skillActive) continue;

        // Attack targeting (used by both auto-skill trigger and regular attacks)
        const auto opPos = ToBoardCenter(op.cell);
        
        struct TargetInfo { int idx; float distSq; };
        std::vector<TargetInfo> validTargets;

        for (std::size_t i = 0; i < m_Enemies.size(); ++i) {
            const auto& e = m_Enemies[i];
            if (!e.alive) continue;

            glm::ivec2 eCel{ static_cast<int>(std::floor(e.boardPos.x)),
                             static_cast<int>(std::floor(e.boardPos.y)) };
            glm::ivec2 rel = eCel - op.cell;
            bool inRange = false;

            if (opType.deployType == DeployType::GROUND_ONLY) {
                inRange = (rel.x == 0 && rel.y == 0) || (rel == op.direction);
            } else {
                int fw = rel.x * op.direction.x + rel.y * op.direction.y;
                int pp = rel.x * op.direction.y - rel.y * op.direction.x;
                inRange = (fw >= 1 && fw <= 4 && std::abs(pp) <= 1) || (rel.x == 0 && rel.y == 0);
            }

            if (inRange) {
                const auto d = e.boardPos - opPos;
                const float sq = glm::dot(d, d);
                validTargets.push_back({static_cast<int>(i), sq});
            }
        }

        if (isBagpipe && !op.skillActive && op.sp >= BAGPIPE_SP_PER_SKILL && !validTargets.empty()) {
            op.skillActive = true;
            op.skillTimerMs = BAGPIPE_SKILL_DURATION_MS;
            op.sp = std::max(0.0F, op.sp - BAGPIPE_SP_PER_SKILL);
        }

        op.cooldownMs = std::max(0.0F, op.cooldownMs - dtMs);
        if (op.cooldownMs > 0.0F || validTargets.empty()) continue;

        std::sort(validTargets.begin(), validTargets.end(), [](const TargetInfo& a, const TargetInfo& b) {
            return a.distSq < b.distSq;
        });

        float dmgMult = 1.0F;
        int maxTargets = 1;

        if (isBagpipe) {
            bool talentTrigger = (rand() % 100) < 31;
            if (talentTrigger) {
                dmgMult = 1.30F;
                maxTargets += 1;
            }
            if (op.skillActive) {
                dmgMult = std::max(dmgMult, 2.0F);
                maxTargets += 1;
            }
        }

        int hitCount = 0;
        for (const auto& t : validTargets) {
            if (hitCount >= maxTargets) break;
            auto& tgt = m_Enemies[static_cast<std::size_t>(t.idx)];
            float finalDmg = opType.damage * dmgMult;
            tgt.hp -= std::max(1.0F, finalDmg - tgt.def);
            m_Beams.push_back(AttackBeam{opPos, tgt.boardPos, BEAM_DURATION_MS});
            
            if (tgt.hp <= 0.0F) {
                tgt.alive = false;
                ++m_KillCount;
                if (isBagpipe) {
                    m_DP = std::min(m_MaxDP, m_DP + 2.0F);
                } else {
                    m_DP = std::min(m_MaxDP, m_DP + KILL_REWARD_DP);
                }
            }
            hitCount++;
        }
        
        op.cooldownMs = opType.attackIntervalMs;
        
        // Trigger attack animation wrapper
        if (op.animState != Ark::Operator::AnimState::SKILL) { // Skill animations override attack
            op.animState = Ark::Operator::AnimState::ATTACK;
            auto& pack2 = m_OperatorAnims[op.typeIndex];
            if (pack2.attack) {
                pack2.activeInstances[op.id] = std::make_shared<Util::Animation>(pack2.attack->GetFrames(), true, 41, false, 0);
            }
        }
    }
}

// ─── Deployment helpers ─────────────────────────────────────────────────────
void App::HandleDeploymentClick(const glm::vec2& cursor) {
    const auto cell = ToCell(cursor);
    if (!cell || !IsDeployableCellForSelectedOperator(*cell) || IsCellOccupied(*cell)) return;
    if (static_cast<int>(m_Operators.size()) >= MAX_OPS) return;
    const auto& opType = m_OperatorTemplates.at(static_cast<std::size_t>(m_SelectedOperatorType));
    if (m_DP < static_cast<float>(opType.cost)) return;
    m_DP -= static_cast<float>(opType.cost);
    Operator newOp;
    newOp.id=m_NextOperatorId++; newOp.typeIndex=m_SelectedOperatorType;
    newOp.cell=*cell; newOp.direction={1,0}; newOp.cooldownMs=200;
    const bool isBagpipe = IsBagpipe(opType);
    newOp.hp=opType.hp;
    newOp.maxHp=opType.hp;
    newOp.def=opType.def;
    newOp.sp=isBagpipe ? 2.0F : opType.initialSp;
    newOp.sp=std::clamp(newOp.sp, 0.0F, isBagpipe ? BAGPIPE_MAX_SP : opType.maxSp);
    m_Operators.push_back(newOp);
}

void App::HandleSkillActivation(const glm::ivec2& cell) {
    for (auto& op : m_Operators) {
        if (op.cell != cell) continue;
        const auto& opType = m_OperatorTemplates.at(op.typeIndex);
        if (IsBagpipe(opType)) return; // Bagpipe skill is auto-triggered
        if (opType.maxSp > 0 && op.sp >= opType.maxSp && !op.skillActive) {
            op.skillActive  = true;
            op.skillTimerMs = opType.skillDuration;
            op.sp           = 0;
            if (IsMyrtle(opType)) m_DP = std::min(m_MaxDP, m_DP+6);
        }
        return;
    }
}

bool App::IsCellOccupied(const glm::ivec2& cell) const {
    return std::any_of(m_Operators.begin(), m_Operators.end(),
        [&](const Operator& op){ return op.cell == cell; });
}

bool App::IsDeployableCellForSelectedOperator(const glm::ivec2& cell) const {
    if (cell.x < 0 || cell.x >= m_StageWidth || cell.y < 0 || cell.y >= m_StageHeight) return false;
    const auto selectedIdx = static_cast<std::size_t>(m_SelectedOperatorType);
    if (selectedIdx >= m_OperatorTemplates.size()) return false;
    const auto tile = m_TileMap[static_cast<std::size_t>(cell.y)][static_cast<std::size_t>(cell.x)];
    return IsDeployableTile(tile, m_OperatorTemplates[selectedIdx].deployType);
}

bool App::IsDeployableTile(TileType tile, DeployType dt) const {
    if (dt == DeployType::GROUND_ONLY) return tile == TileType::GROUND || tile == TileType::ROAD;
    return tile == TileType::HIGHGROUND;
}

int App::FindRouteIndex(const std::string& id) const {
    for (std::size_t i = 0; i < m_Routes.size(); ++i)
        if (m_Routes[i].id == id) return static_cast<int>(i);
    return -1;
}

int App::FindEnemyTemplateIndex(const std::string& id) const {
    for (std::size_t i = 0; i < m_EnemyTemplates.size(); ++i)
        if (m_EnemyTemplates[i].id == id) return static_cast<int>(i);
    return -1;
}

// ─── Operator availability ──────────────────────────────────────────────────

bool App::IsOperatorTypeOnField(int typeIndex) const {
    return std::any_of(m_Operators.begin(), m_Operators.end(),
        [typeIndex](const Operator& op){ return op.typeIndex == typeIndex; });
}

bool App::IsOperatorTypeAvailable(int typeIndex) const {
    // Not available if already on the field
    if (IsOperatorTypeOnField(typeIndex)) return false;
    // Not available if on redeploy cooldown
    auto it = m_OperatorRedeployCooldownMs.find(typeIndex);
    if (it != m_OperatorRedeployCooldownMs.end() && it->second > 0.0F) return false;
    return true;
}

void App::UpdateRedeployCooldowns(float dtMs) {
    for (auto it = m_OperatorRedeployCooldownMs.begin(); it != m_OperatorRedeployCooldownMs.end(); ) {
        it->second -= dtMs;
        if (it->second <= 0.0F) {
            it = m_OperatorRedeployCooldownMs.erase(it);
        } else {
            ++it;
        }
    }
}
