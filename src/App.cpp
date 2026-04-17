// ?????????????????????????????????????????????????????????????????????????????
// App.cpp  ??Arknights clone   (main lifecycle & input handling)
//
// Implementation is split across multiple files:
//   Camera.cpp        ??Projection & coordinate mapping
//   EnemySystem.cpp   ??Enemy spawning, movement & AI
//   OperatorSystem.cpp??Operator combat, skills & deployment helpers
//   Renderer.cpp      ??All drawing / rendering code
//   GameLogic.cpp     ??Wave management, game state, stage init
// ?????????????????????????????????????????????????????????????????????????????
#include "App.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "Util/Input.hpp"
#include "Util/Keycode.hpp"
#include "Util/Time.hpp"
#include "config.hpp"

using namespace Ark;

namespace {
constexpr int   MAX_OPS          = 12;
constexpr float REDEPLOY_COOLDOWN_MS = 90000.0F; // 90 seconds
} // namespace

void App::Start() {
    InitializeStage();
    ResetDemo();
    m_CurrentState = State::UPDATE;

    m_ModelVanguard = std::make_shared<Util::Animation>(
        std::vector<std::string>{
            ASSETS_DIR "/sprites/cat/cat-0.bmp",
            ASSETS_DIR "/sprites/cat/cat-1.bmp",
            ASSETS_DIR "/sprites/cat/cat-2.bmp",
            ASSETS_DIR "/sprites/cat/cat-3.bmp",
            ASSETS_DIR "/sprites/cat/cat-4.bmp",
            ASSETS_DIR "/sprites/cat/cat-5.bmp",
            ASSETS_DIR "/sprites/cat/cat-6.bmp",
            ASSETS_DIR "/sprites/cat/cat-7.bmp",
        }, true, 100, true, 0);
    // You can swap this for any other model available
    m_ModelGuard = std::make_shared<Util::Image>(ASSETS_DIR "/sprites/giraffe.png");
    m_ModelEnemy = std::make_shared<Util::Image>(ASSETS_DIR "/sprites/giraffe.png");
}

void App::Update() {
    if (Util::Input::IfExit() || Util::Input::IsKeyUp(Util::Keycode::ESCAPE)) {
        m_CurrentState = State::END;
        return;
    }
    if (Util::Input::IsKeyDown(Util::Keycode::R)) ResetDemo();
    const float dt = std::clamp(Util::Time::GetDeltaTimeMs(), 0.0F, 100.0F);

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
        !m_GameOver && !m_MissionClear && !m_PreStageWaiting) {
        StartWave();
    }

    const auto raw = Util::Input::GetCursorPosition();
    const glm::vec2 rawCursor{raw.x, raw.y};
    UpdateCameraControls(dt, rawCursor);
    const glm::vec2 ptsdCursor = RawCursorToPtsd(rawCursor);

    if (!m_GameOver && !m_MissionClear && !m_PreStageWaiting) {
        if (Util::Input::IsKeyPressed(Util::Keycode::MOUSE_RB) && !m_IsDeploying) {
            const auto cell = ToCell(ptsdCursor);
            if (cell) {
                for (auto it = m_Operators.begin(); it != m_Operators.end(); ++it) {
                    if (it->cell == *cell) {
                        if (m_SelectedOperatorId == it->id) m_SelectedOperatorId = -1;
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
                        // Cleanup animation instance
                        if (static_cast<std::size_t>(it->typeIndex) < m_OperatorAnims.size()) {
                            m_OperatorAnims[it->typeIndex].activeInstances.erase(it->id);
                        }
                        // Start redeploy cooldown (90 seconds)
                        m_OperatorRedeployCooldownMs[it->typeIndex] = REDEPLOY_COOLDOWN_MS;
                        m_Operators.erase(it);
                        break;
                    }
                }
            }
        }

        if (Util::Input::IsKeyPressed(Util::Keycode::MOUSE_LB)) {
            if (!m_IsDeploying) {
                const auto cell = ToCell(ptsdCursor);
                bool clickedOperator = false;
                if (cell) {
                    // Check skill activation first
                    bool handled = false;
                    if (m_WaveRunning) {
                        for (auto& op : m_Operators) {
                            if (op.cell != *cell) continue;
                            clickedOperator = true;
                            m_SelectedOperatorId = op.id;
                            const auto& opType = m_OperatorTemplates.at(static_cast<std::size_t>(op.typeIndex));
                            const bool isBagpipe = (opType.name == "Bagpipe" || opType.name == "風笛");
                            if (!isBagpipe && opType.maxSp > 0 && op.sp >= opType.maxSp && !op.skillActive) {
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
                    // Must check: operator type not already on field AND not on redeploy cooldown
                    if (!handled && m_WaveRunning &&
                        IsOperatorTypeAvailable(m_SelectedOperatorType) &&
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
                if (!clickedOperator) m_SelectedOperatorId = -1;
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
                const bool isBagpipe = (opType.name == "Bagpipe" || opType.name == "風笛");
                newOp.sp        = isBagpipe ? 2.0F : opType.initialSp;
                if (isBagpipe) {
                    newOp.sp = std::clamp(newOp.sp, 0.0F, 12.0F); // 4 SP per use, up to 3 charges
                } else {
                    newOp.sp = std::min(newOp.sp, opType.maxSp);
                }

                m_Operators.push_back(newOp);
                // Keep inspector closed after deploy; only open on explicit click.
                m_SelectedOperatorId = -1;
                m_IsDeploying = false;
            }
        }
    }

    UpdateGame(dt);
    DrawScene(ptsdCursor);
}

void App::End() {}
