// ─────────────────────────────────────────────────────────────────────────────
// App.cpp  – Arknights clone   (modular, full rewrite)
// ─────────────────────────────────────────────────────────────────────────────
#include "App.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <tuple>

#include "Util/Input.hpp"
#include "Util/Keycode.hpp"
#include "Util/Time.hpp"
#include "config.hpp"

#include "Ark/StageLoader.hpp"

using namespace Ark;

// ─── colour constants ────────────────────────────────────────────────────────
namespace {
constexpr ImU32 COLOR_BG        = IM_COL32( 16,  19,  26, 255);
constexpr ImU32 COLOR_BOARD     = IM_COL32( 28,  34,  46, 255);
constexpr ImU32 COLOR_GRID      = IM_COL32( 52,  60,  77, 255);
constexpr ImU32 COLOR_EMPTY     = IM_COL32( 34,  39,  52, 255);
constexpr ImU32 COLOR_ROAD      = IM_COL32( 78,  88, 106, 255);
constexpr ImU32 COLOR_GROUND    = IM_COL32( 57,  72,  98, 255);
constexpr ImU32 COLOR_HIGHGROUND= IM_COL32(112,  94,  61, 255);
constexpr ImU32 COLOR_SPAWN     = IM_COL32( 44, 118,  78, 255);
constexpr ImU32 COLOR_GOAL      = IM_COL32(154,  72,  72, 255);
constexpr ImU32 COLOR_TEXT_MAIN = IM_COL32(236, 240, 245, 255);
constexpr ImU32 COLOR_TEXT_SUB  = IM_COL32(173, 183, 198, 255);
constexpr ImU32 COLOR_HOVER     = IM_COL32(255, 255, 255,  70);

constexpr int   MAX_OPS          = 12;
constexpr float BEAM_DURATION_MS = 120.0F;
constexpr float WAVE_CLEAR_DP    = 5.0F;
constexpr float KILL_REWARD_DP   = 1.5F;
} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// LIFECYCLE
// ─────────────────────────────────────────────────────────────────────────────
void App::Start() {
    InitializeStage();
    ResetDemo();
    m_CurrentState = State::UPDATE;
}

void App::Update() {
    if (Util::Input::IfExit() || Util::Input::IsKeyUp(Util::Keycode::ESCAPE)) {
        m_CurrentState = State::END;
        return;
    }
    if (Util::Input::IsKeyDown(Util::Keycode::R)) ResetDemo();

    // Operator selection (hotkeys)
    for (int i = 0; i < static_cast<int>(m_OperatorTemplates.size()); ++i) {
        // NUM_1 = NUM_0 + 1? Use array of keys
    }
    if (Util::Input::IsKeyDown(Util::Keycode::NUM_1)) m_SelectedOperatorType = 0;
    if (Util::Input::IsKeyDown(Util::Keycode::NUM_2)) m_SelectedOperatorType = 1;
    if (Util::Input::IsKeyDown(Util::Keycode::NUM_3)) m_SelectedOperatorType = 2;
    // Clamp selection to available operators
    if (m_SelectedOperatorType >= static_cast<int>(m_OperatorTemplates.size()))
        m_SelectedOperatorType = static_cast<int>(m_OperatorTemplates.size()) - 1;

    if (Util::Input::IsKeyDown(Util::Keycode::SPACE) && !m_WaveRunning &&
        !m_GameOver && !m_MissionClear) {
        StartWave();
    }

    // GetCursorPosition() returns PTSD coordinates:
    //   x = pixels from screen center (right = positive)
    //   y = pixels from screen center (up = positive)
    // ToScreenPosition does: screenX = ptsdX - ptsdY + W/2
    //                         screenY = H/2 - (ptsdX + ptsdY)*0.5
    // So the inverse isometric unproject:  rawX = isoScreenX-from-center, rawY = isoScreenY-from-center-up
    //   ptsdX = (rawX + 2*rawY) / 2
    //   ptsdY = (2*rawY - rawX) / 2
    const auto raw = Util::Input::GetCursorPosition();
    glm::vec2 ptsdCursor{
        (raw.x + 2.0F * raw.y) * 0.5F,
        (2.0F * raw.y - raw.x) * 0.5F
    };

    if (!m_GameOver && !m_MissionClear) {
        if (Util::Input::IsKeyPressed(Util::Keycode::MOUSE_RB) && !m_IsDeploying) {
            const auto cell = ToCell(ptsdCursor);
            if (cell) {
                for (auto it = m_Operators.begin(); it != m_Operators.end(); ++it) {
                    if (it->cell == *cell) {
                        const auto& opType = m_OperatorTemplates.at(static_cast<std::size_t>(it->typeIndex));
                        // Retreat refund
                        if (opType.name == "Bagpipe" || opType.name == "風笛") {
                            m_DP = std::min(m_MaxDP, m_DP + static_cast<float>(opType.cost)); 
                        } else {
                            m_DP = std::min(m_MaxDP, m_DP + std::floor(static_cast<float>(opType.cost) / 2.0F)); 
                        }
                        // Release blocked enemies
                        for (auto& e : m_Enemies) {
                            if (e.blockedByOperatorId == it->id) e.blockedByOperatorId = -1;
                        }
                        m_Operators.erase(it);
                        break;
                    }
                }
            }
        }

        if (Util::Input::IsKeyPressed(Util::Keycode::MOUSE_LB)) {
            if (!m_IsDeploying) {
                const auto cell = ToCell(ptsdCursor);
                if (cell) {
                    // Check skill activation first
                    bool handled = false;
                    if (m_WaveRunning) {
                        for (auto& op : m_Operators) {
                            if (op.cell != *cell) continue;
                            const auto& opType = m_OperatorTemplates.at(static_cast<std::size_t>(op.typeIndex));
                            if (opType.maxSp > 0 && op.sp >= opType.maxSp && !op.skillActive) {
                                op.skillActive  = true;
                                op.skillTimerMs = opType.skillDuration;
                                op.sp           = 0;
                                if (opType.name == "Myrtle" || opType.name == "桃金娘") {
                                    m_DP = std::min(m_MaxDP, m_DP + 6.0F);
                                }
                                handled = true;
                                break;
                            }
                        }
                    }
                    // Deploy: only allowed after wave started
                    if (!handled && m_WaveRunning &&
                        IsDeployableCellForSelectedOperator(*cell) && !IsCellOccupied(*cell) &&
                        static_cast<int>(m_Operators.size()) < MAX_OPS) {
                        const auto& opType = m_OperatorTemplates.at(static_cast<std::size_t>(m_SelectedOperatorType));
                        if (m_DP >= static_cast<float>(opType.cost)) {
                            m_IsDeploying    = true;
                            m_DeployingCell  = *cell;
                            m_DeployingDirection = {1, 0};
                        }
                    }
                }
            }
        }

        if (m_IsDeploying) {
            glm::vec2 cellCenter = ToPtsdPosition(ToBoardCenter(m_DeployingCell));
            glm::vec2 diff       = ptsdCursor - cellCenter;
            if (glm::length(diff) > 20.0F) {
                if (std::abs(diff.x) > std::abs(diff.y)) m_DeployingDirection = {diff.x > 0 ? 1 : -1, 0};
                else                                      m_DeployingDirection = {0, diff.y > 0 ? -1 : 1};
            }

            if (Util::Input::IsKeyUp(Util::Keycode::MOUSE_LB)) {
                const auto& opType = m_OperatorTemplates.at(static_cast<std::size_t>(m_SelectedOperatorType));
                m_DP -= static_cast<float>(opType.cost);

                Operator newOp;
                newOp.id        = m_NextOperatorId++;
                newOp.typeIndex = m_SelectedOperatorType;
                newOp.cell      = m_DeployingCell;
                newOp.direction = m_DeployingDirection;
                newOp.cooldownMs= 200.0F;
                newOp.hp        = opType.hp;
                newOp.maxHp     = opType.hp;
                newOp.def       = opType.def;
                newOp.sp        = opType.initialSp;
                
                // Talent: Bagpipe Vanguard SP Buff
                bool hasBagpipe = std::any_of(m_OperatorTemplates.begin(), m_OperatorTemplates.end(),
                    [](const OperatorTemplate& t) { return t.name == "Bagpipe" || t.name == "風笛"; });
                
                if (hasBagpipe && opType.isVanguard) {
                    newOp.sp += 10.0F;
                }
                if (opType.name == "Bagpipe" || opType.name == "風笛") {
                    newOp.sp += 4.0F; // Bagpipe deployment SP bonus
                }
                newOp.sp = std::min(newOp.sp, opType.maxSp);

                m_Operators.push_back(newOp);
                m_IsDeploying = false;
            }
        }
    }

    const float dt = std::clamp(Util::Time::GetDeltaTimeMs(), 0.0F, 100.0F);
    UpdateGame(dt);
    DrawScene(ptsdCursor);
}

void App::End() {}

// ─────────────────────────────────────────────────────────────────────────────
// RESET / INIT
// ─────────────────────────────────────────────────────────────────────────────
void App::ResetDemo() {
    m_GameOver    = false;
    m_MissionClear= false;
    m_ClearTimerMs= 0.0F;

    m_DP          = 12.0F;
    m_LifePoint   = 10;
    m_KillCount   = 0;

    m_SelectedOperatorType = 0;
    m_IsDeploying          = false;

    m_CurrentWave    = 0;
    m_WaveRunning    = false;
    m_WaveElapsedSec = 0.0F;
    m_NextSpawnIndex = 0;
    m_SpawnedWaveUnits = 0;
    m_TotalWaveUnits = static_cast<int>(m_WavePlans.size());

    m_NextOperatorId = 1;
    m_NextEnemyId    = 1;

    m_Enemies.clear();
    m_Operators.clear();
    m_Beams.clear();
}

void App::InitializeStage() {
    m_OperatorTemplates = Ark::LoadOperators();
    if (m_OperatorTemplates.empty()) {
        // Fallback hardcoded operators if JSON not found
        m_OperatorTemplates = {
            OperatorTemplate{"Vanguard","Vanguard",8,1500,400,300,1050,1,0,0,0, IM_COL32(68,160,255,255), DeployType::GROUND_ONLY, true},
            OperatorTemplate{"Sniper","Sniper",   11,1200,500,150,1000,0,0,0,0, IM_COL32(255,196,66,255), DeployType::HIGHGROUND_ONLY, false},
            OperatorTemplate{"Myrtle","桃金娘",  8,1654,508,400,1000,1,22,13,8000, IM_COL32(255,215,0,255), DeployType::GROUND_ONLY, true},
        };
    }
    if (!LoadStageFromJsonModule()) BuildFallbackStage();
}

bool App::LoadStageFromJsonModule() {
    auto result = Ark::LoadStageFromJson(m_CurrentStageFile);
    if (!result.has_value()) return false;

    auto& d = *result;
    m_StageWidth    = d.width;
    m_StageHeight   = d.height;
    m_StageName     = d.name;
    m_StageLoadSource = d.sourceFile;
    m_TileMap       = std::move(d.tileMap);
    m_Routes        = std::move(d.routes);
    m_EnemyTemplates= std::move(d.enemyTemplates);
    m_WavePlans     = std::move(d.wavePlans);
    m_TotalWaves    = d.totalWaves;
    m_TotalWaveUnits= static_cast<int>(m_WavePlans.size());
    return true;
}

void App::BuildFallbackStage() {
    m_StageWidth  = 14; m_StageHeight = 8;
    m_StageName   = "fallback_stage";
    m_StageLoadSource = "embedded fallback";

    m_TileMap.assign(static_cast<std::size_t>(m_StageHeight),
                     std::vector<TileType>(static_cast<std::size_t>(m_StageWidth), TileType::GROUND));
    for (int x = 0; x < m_StageWidth; ++x)
        m_TileMap[5][static_cast<std::size_t>(x)] = TileType::HIGHGROUND;

    const std::vector<glm::ivec2> routeCells{
        {0,4},{1,4},{2,4},{3,4},{4,4},{4,3},{4,2},
        {5,2},{6,2},{7,2},{8,2},{8,3},{8,4},{9,4},
        {10,4},{11,4},{11,5},{11,6},{12,6},{13,6},
    };
    m_Routes.clear();
    Route route; route.id = "main";
    for (std::size_t i = 0; i < routeCells.size(); ++i) {
        const auto& c = routeCells[i];
        auto& tile = m_TileMap[static_cast<std::size_t>(c.y)][static_cast<std::size_t>(c.x)];
        if (i == 0) tile = TileType::SPAWN;
        else if (i + 1 == routeCells.size()) tile = TileType::GOAL;
        else tile = TileType::ROAD;
        route.nodes.push_back(RouteNode{ToBoardCenter(c), (i == 4) ? 0.8F : 0.0F});
    }
    m_Routes.push_back(std::move(route));

    {
        EnemyTemplate slug;
        slug.id = "slug"; slug.enemyId = "slug";
        slug.hp = 90.0F; slug.speed = 1.2F; slug.damage = 100.0F;
        slug.attackIntervalMs = 2000.0F; slug.canAttackOperator = true;
        slug.color = IM_COL32(220,87,92,255);
        EnemyTemplate raider;
        raider.id = "raider"; raider.enemyId = "raider";
        raider.hp = 170.0F; raider.speed = 0.95F; raider.damage = 150.0F;
        raider.attackIntervalMs = 2000.0F; raider.canAttackOperator = true;
        raider.color = IM_COL32(236,142,75,255);
        m_EnemyTemplates = {slug, raider};
    }
    m_WavePlans.clear();
    for (int i = 0; i < 10; ++i) m_WavePlans.push_back(WavePlan{1,0,0, 1.2F*i});
    for (int i = 0; i <  6; ++i) m_WavePlans.push_back(WavePlan{2,1,0,12.0F+1.45F*i});
    std::sort(m_WavePlans.begin(), m_WavePlans.end(),
              [](const WavePlan& a, const WavePlan& b){ return a.spawnTimeSec < b.spawnTimeSec; });
    m_TotalWaves     = 2;
    m_TotalWaveUnits = static_cast<int>(m_WavePlans.size());
}

// ─────────────────────────────────────────────────────────────────────────────
// WAVE
// ─────────────────────────────────────────────────────────────────────────────
void App::StartWave() {
    if (m_GameOver || m_WaveRunning || m_MissionClear || m_WavePlans.empty()) return;
    m_WaveRunning    = true;
    m_WaveElapsedSec = 0.0F;
    m_NextSpawnIndex = 0;
    m_SpawnedWaveUnits = 0;
    m_CurrentWave    = 0;
    m_Enemies.clear();
    m_Beams.clear();
}

// ─────────────────────────────────────────────────────────────────────────────
// UPDATE
// ─────────────────────────────────────────────────────────────────────────────
void App::UpdateGame(float dtMs) {
    UpdateBeams(dtMs);

    if (m_GameOver || m_MissionClear) {
        if (m_MissionClear) {
            m_ClearTimerMs += dtMs;
            if (m_ClearTimerMs >= 2000.0F && m_CurrentStageFile != "Operation 1-1") {
                m_CurrentStageFile = "Operation 1-1";
                InitializeStage();
                ResetDemo();
            }
        }
        return;
    }

    // DP regen only while wave is running
    if (m_WaveRunning) {
        m_DP = std::min(m_MaxDP, m_DP + m_DPRegenPerSec * dtMs / 1000.0F);
    }

    const float dtSec = dtMs / 1000.0F;
    UpdateWave(dtSec);
    UpdateEnemies(dtSec);
    UpdateOperators(dtMs);
    CleanupDefeatedEnemies();

    // Remove dead operators
    m_Operators.erase(
        std::remove_if(m_Operators.begin(), m_Operators.end(),
                       [](const Operator& op){ return op.hp <= 0.0F; }),
        m_Operators.end());

    if (m_WaveRunning && m_NextSpawnIndex >= m_WavePlans.size() && m_Enemies.empty()) {
        m_WaveRunning = false;
        m_MissionClear= true;
        m_ClearTimerMs= 0.0F;
        m_DP = std::min(m_MaxDP, m_DP + WAVE_CLEAR_DP);
    }
}

void App::UpdateWave(float dtSec) {
    if (!m_WaveRunning) return;
    m_WaveElapsedSec += dtSec;
    while (m_NextSpawnIndex < m_WavePlans.size() &&
           m_WavePlans[m_NextSpawnIndex].spawnTimeSec <= m_WaveElapsedSec) {
        SpawnEnemy(m_WavePlans[m_NextSpawnIndex]);
        m_CurrentWave = std::max(m_CurrentWave, m_WavePlans[m_NextSpawnIndex].waveIndex);
        ++m_SpawnedWaveUnits;
        ++m_NextSpawnIndex;
    }
}

void App::SpawnEnemy(const WavePlan& plan) {
    if (plan.routeIndex < 0 || plan.enemyTypeIndex < 0 ||
        plan.routeIndex >= static_cast<int>(m_Routes.size()) ||
        plan.enemyTypeIndex >= static_cast<int>(m_EnemyTemplates.size())) return;

    const auto& route  = m_Routes[static_cast<std::size_t>(plan.routeIndex)];
    const auto& tmpl   = m_EnemyTemplates[static_cast<std::size_t>(plan.enemyTypeIndex)];
    if (route.nodes.empty()) return;

    Enemy e;
    e.id               = m_NextEnemyId++;
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

// ─── Enemy update ─────────────────────────────────────────────────────────────
void App::UpdateEnemies(float dtSec) {
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
        if (!enemy.alive) continue;

        // Find blocking operator
        Operator* blockOp = nullptr;
        if (enemy.blockedByOperatorId >= 0) {
            auto it = std::find_if(m_Operators.begin(), m_Operators.end(),
                [&](const Operator& op){ return op.id == enemy.blockedByOperatorId && op.hp > 0; });
            if (it != m_Operators.end()) blockOp = &(*it);
        }

        // Try to acquire a block
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
                    break;
                }
            }
        }

        if (blockOp) {
            // Close-range melee attack on the blocking operator
            if (enemy.canAttackOperator) {
                enemy.attackCooldownMs -= dtSec * 1000.0F;
                if (enemy.attackCooldownMs <= 0.0F) {
                    blockOp->hp -= std::max(1.0F, enemy.damage - blockOp->def);
                    enemy.attackCooldownMs = enemy.attackIntervalMs;
                }
            }
            continue; // blocked – don't move
        }

        // ── Ranged attacker: scans all operators in range ────────────────
        if (enemy.isRanged && enemy.attackRange > 0.0F) {
            enemy.attackCooldownMs -= dtSec * 1000.0F;
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
                    rangedTarget->hp -= std::max(1.0F, enemy.damage - rangedTarget->def);
                    // Visual beam: enemy -> operator
                    m_Beams.push_back(AttackBeam{enemy.boardPos, ToBoardCenter(rangedTarget->cell), BEAM_DURATION_MS});
                }
                enemy.attackCooldownMs = enemy.attackIntervalMs;
            }
            // Ranged enemies still walk (don't stop)
        }

        // Move along route
        if (enemy.routeIndex < 0 || enemy.routeIndex >= static_cast<int>(m_Routes.size())) {
            enemy.alive = false; continue;
        }
        const auto& route = m_Routes[static_cast<std::size_t>(enemy.routeIndex)];
        float timeLeft = dtSec;

        while (timeLeft > 0.0F && enemy.alive) {
            if (enemy.waitSec > 0.0F) {
                const float c = std::min(enemy.waitSec, timeLeft);
                enemy.waitSec -= c; timeLeft -= c;
                if (enemy.waitSec > 0.0F) break;
            }
            if (enemy.nodeIndex + 1 >= route.nodes.size()) {
                enemy.alive = false;
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
    }
}

// ─── Operator update ──────────────────────────────────────────────────────────
void App::UpdateOperators(float dtMs) {
    bool myrtleOnField = std::any_of(m_Operators.begin(), m_Operators.end(),
        [&](const Operator& op) {
            if (op.hp <= 0) return false;
            const auto& t = m_OperatorTemplates.at(op.typeIndex);
            return t.name == "Myrtle" || t.name == "桃金娘";
        });

    const float dtSec = dtMs / 1000.0F;

    for (auto& op : m_Operators) {
        if (op.hp <= 0) continue;
        const auto& opType = m_OperatorTemplates.at(static_cast<std::size_t>(op.typeIndex));

        // Myrtle talent: all vanguards regen 33 HP/s
        if (myrtleOnField && opType.isVanguard)
            op.hp = std::min(op.maxHp, op.hp + 33.0F * dtSec);

        // Skill timer / SP regen
        if (op.skillActive) {
            op.skillTimerMs -= dtMs;
            if (op.skillTimerMs <= 0.0F) { op.skillActive = false; op.sp = 0; }
        } else if (opType.maxSp > 0) {
            op.sp = std::min(opType.maxSp, op.sp + 1.0F * dtSec);
        }

        op.cooldownMs = std::max(0.0F, op.cooldownMs - dtMs);
        if (op.cooldownMs > 0.0F) continue;

        // Myrtle skill active: no attacking
        if ((opType.name == "Myrtle" || opType.name == "桃金娘") && op.skillActive) continue;

        // Attack targeting
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

        if (validTargets.empty()) continue;

        std::sort(validTargets.begin(), validTargets.end(), [](const TargetInfo& a, const TargetInfo& b) {
            return a.distSq < b.distSq;
        });

        float dmgMult = 1.0F;
        int maxTargets = 1;

        if (opType.name == "Bagpipe" || opType.name == "風笛") {
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
                if (opType.name == "Bagpipe" || opType.name == "風笛") {
                    m_DP = std::min(m_MaxDP, m_DP + 2.0F);
                } else {
                    m_DP = std::min(m_MaxDP, m_DP + KILL_REWARD_DP);
                }
            }
            hitCount++;
        }
        
        op.cooldownMs = opType.attackIntervalMs;
    }
}

void App::UpdateBeams(float dtMs) {
    for (auto& b : m_Beams) b.ttlMs -= dtMs;
    m_Beams.erase(std::remove_if(m_Beams.begin(), m_Beams.end(),
        [](const AttackBeam& b){ return b.ttlMs <= 0; }), m_Beams.end());
}

void App::CleanupDefeatedEnemies() {
    m_Enemies.erase(std::remove_if(m_Enemies.begin(), m_Enemies.end(),
        [](const Enemy& e){ return !e.alive; }), m_Enemies.end());
}

// ─────────────────────────────────────────────────────────────────────────────
// DRAW
// ─────────────────────────────────────────────────────────────────────────────
void App::DrawScene(const glm::vec2& cursor) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const float W = static_cast<float>(PTSD_Config::WINDOW_WIDTH);
    const float H = static_cast<float>(PTSD_Config::WINDOW_HEIGHT);
    const auto layout = GetBoardLayout();

    draw->AddRectFilled({0,0},{W,H}, COLOR_BG);

    // Board background quad
    ImVec2 b1 = ToScreenPosition({layout.topLeftX, layout.topLeftY});
    ImVec2 b2 = ToScreenPosition({layout.topLeftX + m_StageWidth  * layout.cellSize, layout.topLeftY});
    ImVec2 b3 = ToScreenPosition({layout.topLeftX + m_StageWidth  * layout.cellSize, layout.topLeftY - m_StageHeight * layout.cellSize});
    ImVec2 b4 = ToScreenPosition({layout.topLeftX, layout.topLeftY - m_StageHeight * layout.cellSize});
    draw->AddQuadFilled(b1, b2, b3, b4, COLOR_BOARD);

    DrawGrid();
    DrawBeams();
    DrawOperators(layout);
    DrawEnemies(layout);
    DrawDeployPreview(ToCell(cursor), layout);
    DrawHUD(W);
}

void App::DrawGrid() {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const auto layout = GetBoardLayout();

    for (int row = 0; row < m_StageHeight; ++row) {
        for (int col = 0; col < m_StageWidth; ++col) {
            const auto tile = m_TileMap[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)];
            ImU32 c = COLOR_EMPTY;
            switch (tile) {
                case TileType::ROAD:       c = COLOR_ROAD;       break;
                case TileType::GROUND:     c = COLOR_GROUND;     break;
                case TileType::HIGHGROUND: c = COLOR_HIGHGROUND; break;
                case TileType::SPAWN:      c = COLOR_SPAWN;      break;
                case TileType::GOAL:       c = COLOR_GOAL;       break;
                default: break;
            }
            float l = layout.topLeftX + col * layout.cellSize;
            float r = l + layout.cellSize;
            float t = layout.topLeftY - row * layout.cellSize;
            float b = t - layout.cellSize;

            ImVec2 p1 = ToScreenPosition({l,t});
            ImVec2 p2 = ToScreenPosition({r,t});
            ImVec2 p3 = ToScreenPosition({r,b});
            ImVec2 p4 = ToScreenPosition({l,b});

            // Highground: draw a slight elevated box to give height illusion
            if (tile == TileType::HIGHGROUND) {
                float hOff = layout.cellSize * 0.15F;
                ImVec2 q1 = {p1.x, p1.y - hOff};
                ImVec2 q2 = {p2.x, p2.y - hOff};
                ImVec2 q3 = {p3.x, p3.y - hOff};
                ImVec2 q4 = {p4.x, p4.y - hOff};
                // Side faces (darker)
                ImU32 sideColor = IM_COL32(80, 64, 38, 255);
                draw->AddQuadFilled(p1, q1, q2, p2, sideColor); // front-left
                draw->AddQuadFilled(p2, q2, q3, p3, IM_COL32(60,48,28,255)); // front-right
                draw->AddQuadFilled(q1, q2, q3, q4, COLOR_HIGHGROUND); // top
                draw->AddQuad(q1, q2, q3, q4, COLOR_GRID, 1.5F);
            } else {
                draw->AddQuadFilled(p1, p2, p3, p4, c);
                draw->AddQuad(p1, p2, p3, p4, COLOR_GRID, 1.5F);
            }
        }
    }
}

void App::DrawBeams() {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    for (const auto& beam : m_Beams) {
        const float alpha = std::clamp(beam.ttlMs / BEAM_DURATION_MS, 0.0F, 1.0F);
        draw->AddLine(ToScreenPosition(ToPtsdPosition(beam.from)),
                      ToScreenPosition(ToPtsdPosition(beam.to)),
                      IM_COL32(255, 239, 120, static_cast<int>(255.0F * alpha)), 2.5F);
    }
}

void App::DrawOperators(const BoardLayout& layout) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();

    for (const auto& op : m_Operators) {
        const auto& opType = m_OperatorTemplates.at(static_cast<std::size_t>(op.typeIndex));
        bool isHigh = (opType.deployType == DeployType::HIGHGROUND_ONLY);
        float yOff  = isHigh ? layout.cellSize * 0.22F : 0.0F;

        auto center0 = ToScreenPosition(ToPtsdPosition(ToBoardCenter(op.cell)));
        ImVec2 center = {center0.x, center0.y - yOff};

        // Body quad
        auto q = [&](glm::vec2 off) {
            ImVec2 v = ToScreenPosition(ToPtsdPosition(ToBoardCenter(op.cell) + off));
            v.y -= yOff;
            return v;
        };
        float hs = 0.30F;
        ImVec2 p1 = q({-hs,  hs});
        ImVec2 p2 = q({ hs,  hs});
        ImVec2 p3 = q({ hs, -hs});
        ImVec2 p4 = q({-hs, -hs});

        if (isHigh) {
            // Side panel to look elevated
            float bodyOff = layout.cellSize * 0.22F;
            ImVec2 p1b = {p1.x, p1.y + bodyOff};
            ImVec2 p2b = {p2.x, p2.y + bodyOff};
            ImVec2 p3b = {p3.x, p3.y + bodyOff};
            ImVec2 p4b = {p4.x, p4.y + bodyOff};
            draw->AddQuadFilled(p1b, p2b, p3b, p4b, IM_COL32(80, 64, 38, 200)); // "base"
            draw->AddLine(p1b, p1, IM_COL32(80,64,38,200), 2.0F);
            draw->AddLine(p2b, p2, IM_COL32(80,64,38,200), 2.0F);
        }
        draw->AddQuadFilled(p1, p2, p3, p4, opType.color);
        draw->AddQuad(p1, p2, p3, p4, IM_COL32(255,255,255,60), 1.5F);

        // Label
        const std::string sym(1, opType.name[0]);
        const auto ts = ImGui::CalcTextSize(sym.c_str());
        draw->AddText({center.x - ts.x*0.5F, center.y - ts.y*0.5F}, IM_COL32(12,14,19,255), sym.c_str());

        // Direction arrow
        glm::vec2 dirOff = glm::vec2(op.direction.x, op.direction.y) * 0.5F;
        ImVec2 dirPt = ToScreenPosition(ToPtsdPosition(ToBoardCenter(op.cell) + dirOff));
        dirPt.y -= yOff;
        draw->AddLine(center, dirPt, IM_COL32(255, 50, 50, 255), 3.5F);

        // HP bar
        const float barW = layout.cellSize * 0.52F;
        const float barX = center0.x - barW * 0.5F;
        float barY = center0.y - layout.cellSize * 0.38F - yOff;
        const float hpR = op.maxHp > 0 ? std::clamp(op.hp/op.maxHp, 0.0F, 1.0F) : 0.0F;
        draw->AddRectFilled({barX,      barY}, {barX+barW, barY+5.0F}, IM_COL32(35,40,48,255));
        draw->AddRectFilled({barX,      barY}, {barX+barW*hpR, barY+5.0F}, IM_COL32(101,228,122,255));

        // SP bar
        if (opType.maxSp > 0) {
            barY += 6.0F;
            const float spR = op.skillActive
                ? std::clamp(op.skillTimerMs / opType.skillDuration, 0.0F, 1.0F)
                : std::clamp(op.sp / opType.maxSp, 0.0F, 1.0F);
            ImU32 spCol = op.skillActive ? IM_COL32(255,165,0,255)
                        : (op.sp >= opType.maxSp ? IM_COL32(255,215,0,255) : IM_COL32(100,150,255,255));
            draw->AddRectFilled({barX,barY},{barX+barW, barY+4.0F}, IM_COL32(35,40,48,255));
            draw->AddRectFilled({barX,barY},{barX+barW*spR, barY+4.0F}, spCol);
            if (op.sp >= opType.maxSp && !op.skillActive) {
                // Pulsing border
                draw->AddRect({barX-1,barY-1},{barX+barW+1,barY+5.0F}, IM_COL32(255,215,0,180), 0, 0, 1.5F);
            }
        }
    }
}

void App::DrawEnemies(const BoardLayout& layout) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    for (const auto& e : m_Enemies) {
        if (!e.alive) continue;
        const auto c = ToScreenPosition(ToPtsdPosition(e.boardPos));
        const float r = layout.cellSize * 0.20F;
        draw->AddCircleFilled(c, r, e.color, 28);
        draw->AddCircle(c, r, IM_COL32(255,255,255,100), 28, 1.0F);

        const float hpR    = e.maxHp > 0 ? std::clamp(e.hp/e.maxHp, 0.0F, 1.0F) : 0.0F;
        const float barHW  = layout.cellSize * 0.24F;
        const float barTop = c.y - r - 9.0F;
        draw->AddRectFilled({c.x - barHW, barTop},{c.x + barHW, barTop+4.0F}, IM_COL32(35,40,48,255));
        draw->AddRectFilled({c.x - barHW, barTop},{c.x - barHW + barHW*2*hpR, barTop+4.0F}, IM_COL32(228,92,92,255));
    }
}

void App::DrawDeployPreview(const std::optional<glm::ivec2>& hoverCell, const BoardLayout& layout) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();

    if (hoverCell && !m_GameOver && !m_MissionClear && !m_IsDeploying && m_WaveRunning) {
        const auto& opType = m_OperatorTemplates.at(static_cast<std::size_t>(m_SelectedOperatorType));
        bool deployable = IsDeployableCellForSelectedOperator(*hoverCell) && !IsCellOccupied(*hoverCell)
                          && static_cast<int>(m_Operators.size()) < MAX_OPS;
        bool affordable = m_DP >= static_cast<float>(opType.cost);
        ImU32 pv = (deployable && affordable) ? IM_COL32(89,225,167,90) : IM_COL32(225,89,89,90);

        float l = layout.topLeftX + hoverCell->x * layout.cellSize;
        float t = layout.topLeftY - hoverCell->y * layout.cellSize;
        float r = l + layout.cellSize, b = t - layout.cellSize;
        draw->AddQuadFilled(ToScreenPosition({l,t}),ToScreenPosition({r,t}),
                            ToScreenPosition({r,b}),ToScreenPosition({l,b}), pv);
    }

    if (m_IsDeploying) {
        const auto& st = m_OperatorTemplates.at(static_cast<std::size_t>(m_SelectedOperatorType));
        for (int dx = -5; dx <= 5; ++dx) for (int dy = -5; dy <= 5; ++dy) {
            glm::ivec2 rel{dx, dy};
            bool inR = false;
            if (st.deployType == DeployType::GROUND_ONLY) {
                inR = (rel.x==0&&rel.y==0) || rel == m_DeployingDirection;
            } else {
                int fw = rel.x*m_DeployingDirection.x + rel.y*m_DeployingDirection.y;
                int pp = rel.x*m_DeployingDirection.y - rel.y*m_DeployingDirection.x;
                inR = (fw>=1&&fw<=4&&std::abs(pp)<=1)||(rel.x==0&&rel.y==0);
            }
            if (!inR) continue;
            glm::ivec2 tc = m_DeployingCell + rel;
            if (tc.x<0||tc.x>=m_StageWidth||tc.y<0||tc.y>=m_StageHeight) continue;
            float l = layout.topLeftX + tc.x * layout.cellSize;
            float tp= layout.topLeftY - tc.y * layout.cellSize;
            float r = l + layout.cellSize, b = tp - layout.cellSize;
            draw->AddQuadFilled(ToScreenPosition({l,tp}),ToScreenPosition({r,tp}),
                                ToScreenPosition({r,b}),ToScreenPosition({l,b}), IM_COL32(255,120,120,80));
            draw->AddQuad(ToScreenPosition({l,tp}),ToScreenPosition({r,tp}),
                          ToScreenPosition({r,b}),ToScreenPosition({l,b}), IM_COL32(255,150,150,180), 1.0F);
        }
        glm::vec2 dOff = glm::vec2(m_DeployingDirection) * 0.5F;
        draw->AddLine(ToScreenPosition(ToPtsdPosition(ToBoardCenter(m_DeployingCell))),
                      ToScreenPosition(ToPtsdPosition(ToBoardCenter(m_DeployingCell)+dOff)),
                      IM_COL32(255,50,50,255), 4.0F);
    }
}

void App::DrawHUD(float screenW) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    draw->AddText({26, 12}, COLOR_TEXT_MAIN, "Arknights Demo  |  PTSD Engine");
    if (!m_WaveRunning && !m_GameOver && !m_MissionClear)
        draw->AddText({26, 34}, IM_COL32(255,230,80,255), "[SPACE] Start Wave  (deployment enabled after start)");
    else
        draw->AddText({26, 34}, COLOR_TEXT_SUB,
            "1 Vanguard | 2 Sniper | 3 Myrtle | LMB Deploy/Skill | R Reset | ESC Exit");

    // HUD panel
    const float hx = screenW - 340.0F, hy = 22.0F, hw = 312.0F, hh = 360.0F;
    draw->AddRectFilled({hx,hy},{hx+hw,hy+hh}, IM_COL32(21,24,32,230), 8.0F);
    draw->AddRect({hx,hy},{hx+hw,hy+hh}, IM_COL32(70,80,96,255), 8.0F);

    std::string state = "Ready";
    if (m_WaveRunning)   state = "IN BATTLE";
    if (m_MissionClear)  state = "MISSION CLEAR";
    if (m_GameOver)      state = "MISSION FAILED";

    auto line = [&](float yo, ImU32 col, const std::string& s) {
        draw->AddText({hx+14, hy+yo}, col, s.c_str());
    };
    line(14,  COLOR_TEXT_MAIN, "State: " + state);
    line(34,  COLOR_TEXT_MAIN, "Stage: " + m_StageName);
    line(54,  COLOR_TEXT_SUB,  "Map: " + std::to_string(m_StageWidth) + "x" + std::to_string(m_StageHeight));
    line(80,  COLOR_TEXT_MAIN, "Wave: " + std::to_string(m_CurrentWave) + "/" + std::to_string(std::max(1,m_TotalWaves)));
    line(98,  COLOR_TEXT_SUB,  "Spawned: " + std::to_string(m_SpawnedWaveUnits) + "/" + std::to_string(m_TotalWaveUnits));
    line(116, COLOR_TEXT_SUB,  "Enemies: " + std::to_string(m_Enemies.size()));
    line(140, COLOR_TEXT_MAIN, "DP: " + std::to_string(static_cast<int>(m_DP)) + " / " + std::to_string(static_cast<int>(m_MaxDP)));
    line(160, COLOR_TEXT_MAIN, "LP: " + std::to_string(m_LifePoint));
    line(180, COLOR_TEXT_MAIN, "Kills: " + std::to_string(m_KillCount));

    line(210, COLOR_TEXT_SUB, "--- Operators ---");
    for (int i = 0; i < static_cast<int>(m_OperatorTemplates.size()); ++i) {
        const auto& t = m_OperatorTemplates[i];
        ImU32 col = (i == m_SelectedOperatorType) ? IM_COL32(255,255,255,255) : COLOR_TEXT_SUB;
        std::string info = "[" + std::to_string(i+1) + "] " + t.name + "  Cost " + std::to_string(t.cost);
        line(228.0F + i * 36.0F, col, info);
        std::string sub = "    HP:" + std::to_string(static_cast<int>(t.hp))
                        + " ATK:" + std::to_string(static_cast<int>(t.damage))
                        + " DEF:" + std::to_string(static_cast<int>(t.def))
                        + " BLK:" + std::to_string(t.blockCount);
        line(244.0F + i * 36.0F, COLOR_TEXT_SUB, sub);
    }

    // Overlay
    const float SW = static_cast<float>(PTSD_Config::WINDOW_WIDTH);
    const float SH = static_cast<float>(PTSD_Config::WINDOW_HEIGHT);
    if (m_GameOver) {
        draw->AddRectFilled({0,0},{SW,SH}, IM_COL32(0,0,0,145));
        const std::string t = "MISSION FAILED";
        auto ts = ImGui::CalcTextSize(t.c_str());
        draw->AddText({SW*0.5F-ts.x*0.5F, SH*0.5F-ts.y}, IM_COL32(255,120,120,255), t.c_str());
        const std::string h = "Press R to restart";
        auto hs = ImGui::CalcTextSize(h.c_str());
        draw->AddText({SW*0.5F-hs.x*0.5F, SH*0.5F+12}, COLOR_TEXT_MAIN, h.c_str());
    } else if (m_MissionClear) {
        draw->AddRectFilled({0,0},{SW,SH}, IM_COL32(0,0,0,110));
        const std::string t = "MISSION ACCOMPLISHED";
        auto ts = ImGui::CalcTextSize(t.c_str());
        draw->AddText({SW*0.5F-ts.x*0.5F, SH*0.5F-ts.y}, IM_COL32(128,236,177,255), t.c_str());
        const std::string h = "Loading next stage in 2s...";
        auto hs = ImGui::CalcTextSize(h.c_str());
        draw->AddText({SW*0.5F-hs.x*0.5F, SH*0.5F+14}, COLOR_TEXT_MAIN, h.c_str());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// HELPERS
// ─────────────────────────────────────────────────────────────────────────────
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
    newOp.hp=opType.hp; newOp.maxHp=opType.hp; newOp.def=opType.def; newOp.sp=opType.initialSp;
    m_Operators.push_back(newOp);
}

void App::HandleSkillActivation(const glm::ivec2& cell) {
    for (auto& op : m_Operators) {
        if (op.cell != cell) continue;
        const auto& opType = m_OperatorTemplates.at(op.typeIndex);
        if (opType.maxSp > 0 && op.sp >= opType.maxSp && !op.skillActive) {
            op.skillActive  = true;
            op.skillTimerMs = opType.skillDuration;
            op.sp           = 0;
            if (opType.name == "Myrtle" || opType.name == "桃金娘") m_DP = std::min(m_MaxDP, m_DP+6);
        }
        return;
    }
}

bool App::IsCellOccupied(const glm::ivec2& cell) const {
    return std::any_of(m_Operators.begin(), m_Operators.end(),
        [&](const Operator& op){ return op.cell == cell; });
}

bool App::IsInsideBoard(const glm::vec2& p) const {
    const auto layout = GetBoardLayout();
    return p.x >= layout.topLeftX && p.x < layout.topLeftX + m_StageWidth * layout.cellSize &&
           p.y <= layout.topLeftY && p.y > layout.topLeftY - m_StageHeight * layout.cellSize;
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

std::optional<glm::ivec2> App::ToCell(const glm::vec2& p) const {
    if (!IsInsideBoard(p)) return std::nullopt;
    const auto layout = GetBoardLayout();
    const int col = static_cast<int>((p.x - layout.topLeftX) / layout.cellSize);
    const int row = static_cast<int>((layout.topLeftY - p.y) / layout.cellSize);
    if (col < 0 || col >= m_StageWidth || row < 0 || row >= m_StageHeight) return std::nullopt;
    return glm::ivec2(col, row);
}

glm::vec2 App::ToBoardCenter(const glm::ivec2& cell) const {
    return {static_cast<float>(cell.x) + 0.5F, static_cast<float>(cell.y) + 0.5F};
}

glm::vec2 App::ToPtsdPosition(const glm::vec2& boardPos) const {
    const auto layout = GetBoardLayout();
    return {layout.topLeftX + boardPos.x * layout.cellSize,
            layout.topLeftY - boardPos.y * layout.cellSize};
}

// Standard isometric projection (60° top-down):
//   screenX = (ptsdX - ptsdY) + W/2
//   screenY = H/2 - (ptsdX + ptsdY) * 0.5
ImVec2 App::ToScreenPosition(const glm::vec2& p) const {
    const float W = static_cast<float>(PTSD_Config::WINDOW_WIDTH);
    const float H = static_cast<float>(PTSD_Config::WINDOW_HEIGHT);
    return { (p.x - p.y) + W * 0.5F,
              H * 0.5F - (p.x + p.y) * 0.5F };
}

Ark::BoardLayout App::GetBoardLayout() const {
    const float W = static_cast<float>(PTSD_Config::WINDOW_WIDTH);
    const float H = static_cast<float>(PTSD_Config::WINDOW_HEIGHT);

    const float hudRW  = 362.0F, mL = 24, mR = 22, mT = 46, mB = 28;
    const float availW = std::max(220.0F, W - hudRW - mL - mR);
    const float availH = std::max(220.0F, H - mT - mB);

    const float cols = static_cast<float>(std::max(1, m_StageWidth));
    const float rows = static_cast<float>(std::max(1, m_StageHeight));

    // For isometric, actual screen footprint is wider and shorter, so scale accordingly
    // Each tile is a diamond with diagonal 1:0.5 ratio ; screen width per tile ~ cellSize * sqrt(2)
    // We choose cellSize such that the board fits:
    const float cellSizeRaw = std::floor(std::min(availW / (cols + rows), availH / (cols + rows) * 2.0F));

    BoardLayout layout;
    layout.cellSize = std::clamp(cellSizeRaw, 24.0F, 64.0F);

    // Board center in ptsd space is at origin. Board spans from col 0..cols, row 0..rows (board space)
    // Translated to ptsd: topLeft board corner at (topLeftX, topLeftY)
    // We want the board center at (0.5*cols, 0.5*rows) board space = ptsd (topLeftX + 0.5*cols*cs, topLeftY - 0.5*rows*cs)
    // And that ptsd point should project to (W/2, H/2 + some offset)
    // For simplicity, center the board:
    layout.topLeftX = -(cols * 0.5F) * layout.cellSize;
    layout.topLeftY =  (rows * 0.5F) * layout.cellSize;

    return layout;
}
