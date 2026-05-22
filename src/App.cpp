// ─────────────────────────────────────────────────────────────────────────────
// App.cpp  –Arknights clone   (main lifecycle & input handling)
//
// Implementation is split across multiple files:
//   Camera.cpp        –Projection & coordinate mapping
//   EnemySystem.cpp   –Enemy spawning, movement & AI
//   OperatorSystem.cpp–Operator combat, skills & deployment helpers
//   Renderer.cpp      –All drawing / rendering code
//   GameLogic.cpp     –Wave management, game state, stage init
// ─────────────────────────────────────────────────────────────────────────────
#include "App.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "Ark/Renderer.hpp"
#include "Ark/Renderer/RendererShared.hpp"
#include "Ark/StageLoader.hpp"
#include "Util/Input.hpp"
#include "Util/Keycode.hpp"
#include "Util/Time.hpp"
#include "config.hpp"

using namespace Ark;

namespace {
constexpr int   MAX_OPS          = 8;
constexpr float REDEPLOY_COOLDOWN_MS = 90000.0F; // 90 seconds

// Bottom operator bar layout constants (screen-space)
constexpr float OP_BAR_HEIGHT     = Ark::RendererConst::OP_BAR_HEIGHT;
constexpr float OP_CARD_WIDTH     = Ark::RendererConst::OP_CARD_WIDTH;
constexpr float OP_CARD_HEIGHT    = Ark::RendererConst::OP_CARD_HEIGHT;
constexpr float OP_CARD_SPACING   = Ark::RendererConst::OP_CARD_SPACING;
} // namespace

void App::Start() {
    if (!m_Renderer) {
        m_Renderer = std::make_shared<Ark::AppRenderer>(*this);
    }

    // Phase 1: Lightweight stage JSON loading (fast — sets m_StageLoadingPath so
    // the loading screen image is available for rendering immediately).
    m_OperatorTemplates = Ark::LoadOperators();
    if (m_OperatorTemplates.empty()) {
        m_OperatorTemplates = {
            OperatorTemplate{"Bagpipe","風笛",  11,2664,769,382,1000,1,4,2,1000, IM_COL32(225,120,80,255), DeployType::GROUND_ONLY, true},
            OperatorTemplate{"Sniper","Sniper",   11,1200,500,150,1000,0,0,0,0, IM_COL32(255,196,66,255), DeployType::HIGHGROUND_ONLY, false},
            OperatorTemplate{"Myrtle","桃金娘",  8,1654,508,400,1000,1,22,13,8000, IM_COL32(255,215,0,255), DeployType::GROUND_ONLY, true},
        };
    }
    if (!LoadStageFromJsonModule()) BuildFallbackStage();
    ResetDemo();

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
    m_ModelGuard = std::make_shared<Util::Image>(ASSETS_DIR "/sprites/giraffe.png");
    m_ModelEnemy = std::make_shared<Util::Image>(ASSETS_DIR "/sprites/giraffe.png");

    // Transition to LOADING state — the loading screen will be rendered on the
    // next frame, then heavy work (animation decoding) happens afterwards.
    m_LoadingPhase = 0;
    m_CurrentState = State::LOADING;
}

void App::Loading() {
    if (Util::Input::IfExit() || Util::Input::IsKeyUp(Util::Keycode::ESCAPE)) {
        m_CurrentState = State::END;
        return;
    }

    // Phase 0: First frame — just render the loading screen so it appears
    // immediately. The heavy work is deferred to the next frame.
    if (m_LoadingPhase == 0) {
        if (m_Renderer && !m_StageLoadingPath.empty()) {
            m_Renderer->DrawImageCover(m_StageLoadingPath, m_StageLoadingAlpha, true);
        }
        m_LoadingPhase = 1;
        return;
    }

    // Phase 1: Do the heavy initialization work (animation decoding, thumbnails).
    // The loading screen from the previous frame is still visible.
    if (m_LoadingPhase == 1) {
        if (m_Renderer && !m_StageLoadingPath.empty()) {
            m_Renderer->DrawImageCover(m_StageLoadingPath, m_StageLoadingAlpha, true);
        }
        LoadOperatorAnimations();
        LoadEnemyAnimations();
        if (m_Renderer) {
            m_Renderer->LoadOperatorThumbnails();
        }
        m_LoadingPhase = 2;
        return;
    }

    // Phase 2: Done — transition to game. Since the loading screen was already shown
    // during decoding, we skip the pre-stage wait.
    m_PreStageWaiting = false;
    m_PreStageTimerMs = 0.0F;
    if (!m_WaveRunning && !m_GameOver && !m_MissionClear) {
        StartWave();
    }
    m_CurrentState = State::UPDATE;
}

void App::Update() {
    if (Util::Input::IfExit() || Util::Input::IsKeyUp(Util::Keycode::ESCAPE)) {
        m_CurrentState = State::END;
        return;
    }
    if (Util::Input::IsKeyDown(Util::Keycode::R)) ResetDemo();
    const float dt = std::clamp(Util::Time::GetDeltaTimeMs(), 0.0F, 100.0F);

    const auto raw = Util::Input::GetCursorPosition();
    const glm::vec2 rawCursor{raw.x, raw.y};

    // Screen-space cursor for bar hit-testing
    const float screenW = static_cast<float>(PTSD_Config::WINDOW_WIDTH);
    const float screenH = static_cast<float>(PTSD_Config::WINDOW_HEIGHT);
    const float screenCursorX = rawCursor.x + screenW * 0.5F;
    const float screenCursorY = screenH * 0.5F - rawCursor.y;
    const auto uiLayout = Ark::RendererConst::ComputeBattleUiLayout(screenW, screenH);

    bool uiConsumedLeftClick = false;
    if (!m_PreStageWaiting && !m_GameOver && !m_MissionClear &&
        Util::Input::IsKeyDown(Util::Keycode::MOUSE_LB)) {
        if (m_ShowQuitConfirm) {
            uiConsumedLeftClick = true;
            if (uiLayout.quitBackButton.Contains(screenCursorX, screenCursorY)) {
                m_ShowQuitConfirm = false;
                m_GamePaused = m_PauseBeforeQuitConfirm;
            } else if (uiLayout.quitConfirmButton.Contains(screenCursorX, screenCursorY)) {
                m_CurrentState = State::END;
                return;
            }
        } else if (uiLayout.settingsButton.Contains(screenCursorX, screenCursorY)) {
            uiConsumedLeftClick = true;
            m_PauseBeforeQuitConfirm = m_GamePaused;
            m_GamePaused = true;
            m_ShowQuitConfirm = true;
        } else if (uiLayout.speedButton.Contains(screenCursorX, screenCursorY)) {
            uiConsumedLeftClick = true;
            m_GameSpeedMultiplier = (m_GameSpeedMultiplier < 1.5F) ? 2.0F : 1.0F;
        } else if (uiLayout.pauseButton.Contains(screenCursorX, screenCursorY)) {
            uiConsumedLeftClick = true;
            m_GamePaused = !m_GamePaused;
        }
    }

    if (!m_PreStageWaiting && !m_GamePaused && !m_ShowQuitConfirm) {
        UpdateCameraControls(dt, rawCursor);
    }
    const glm::vec2 ptsdCursor = RawCursorToPtsd(rawCursor);

    if (!m_GameOver && !m_MissionClear && !m_PreStageWaiting &&
        !m_GamePaused && !m_ShowQuitConfirm && !uiConsumedLeftClick) {
        // ── Right-click: retreat operator ────────────────────────────
        if (Util::Input::IsKeyPressed(Util::Keycode::MOUSE_RB) && !m_IsDeploying &&
            !m_DraggingFromBar && !m_WaitingForDirection) {
            const auto cell = ToCell(ptsdCursor);
            if (cell) {
                for (auto it = m_Operators.begin(); it != m_Operators.end(); ++it) {
                    if (it->cell == *cell) {
                        if (m_SelectedOperatorId == it->id) m_SelectedOperatorId = -1;
                        const auto& opType = m_OperatorTemplates.at(static_cast<std::size_t>(it->typeIndex));
                        // Retreat refund
                        if (opType.name == "Bagpipe" || opType.name == u8"風笛") {
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

        // ── Direction selection phase ─────────────────────────────────
        if (m_WaitingForDirection) {
            if (Util::Input::IsKeyDown(Util::Keycode::MOUSE_LB) && !m_IsDirectionDragging) {
                m_IsDirectionDragging = true;
                m_DirectionDragStart = {screenCursorX, screenCursorY};
            }

            if (m_IsDirectionDragging) {
                glm::vec2 diff{
                    screenCursorX - m_DirectionDragStart.x,
                    screenCursorY - m_DirectionDragStart.y
                };
                if (std::abs(diff.x) > 25.0F || std::abs(diff.y) > 25.0F) {
                    if (std::abs(diff.x) > std::abs(diff.y)) {
                        m_DeployingDirection = diff.x > 0 ? glm::ivec2(1, 0) : glm::ivec2(-1, 0);
                    } else {
                        m_DeployingDirection = diff.y > 0 ? glm::ivec2(0, 1) : glm::ivec2(0, -1);
                    }
                }
                
                if (Util::Input::IsKeyUp(Util::Keycode::MOUSE_LB)) {
                    // Finalize deployment if they dragged far enough
                    if (std::abs(diff.x) > 25.0F || std::abs(diff.y) > 25.0F) {
                        const auto& opType = m_OperatorTemplates.at(static_cast<std::size_t>(m_DragOperatorType));
                        m_DP -= static_cast<float>(opType.cost);

                        Operator newOp;
                        newOp.id        = m_NextOperatorId++;
                        newOp.typeIndex = m_DragOperatorType;
                        newOp.cell      = m_DirectionCell;
                        newOp.direction = m_DeployingDirection;
                        newOp.cooldownMs= 200.0F;
                        newOp.hp        = opType.hp;
                        newOp.maxHp     = opType.hp;
                        newOp.def       = opType.def;
                        const bool isBagpipe = (opType.name == "Bagpipe" || opType.name == u8"風笛");
                        newOp.sp        = isBagpipe ? 2.0F : opType.initialSp;
                        if (isBagpipe) {
                            newOp.sp = std::clamp(newOp.sp, 0.0F, 12.0F);
                        } else {
                            newOp.sp = std::min(newOp.sp, opType.maxSp);
                        }

                        m_Operators.push_back(newOp);
                        m_SelectedOperatorId = -1;
                    } else {
                        // Cancel deployment if dropped inside the center area
                        m_SelectedOperatorId = -1;
                    }
                    m_WaitingForDirection = false;
                    m_DraggingFromBar = false;
                    m_IsDeploying = false;
                    m_IsDirectionDragging = false;
                }
            }

            // Right-click cancels direction selection
            if (Util::Input::IsKeyDown(Util::Keycode::MOUSE_RB)) {
                m_WaitingForDirection = false;
                m_DraggingFromBar = false;
                m_IsDeploying = false;
                m_IsDirectionDragging = false;
            }
        }
        // ── Drag from operator bar ────────────────────────────────────
        else if (Util::Input::IsKeyDown(Util::Keycode::MOUSE_LB) && !m_DraggingFromBar) {
            // Check if click is inside the bottom operator bar area
            const float barY = screenH - OP_BAR_HEIGHT;
            if (screenCursorY >= barY && m_WaveRunning) {
                const int opCount = static_cast<int>(m_OperatorTemplates.size());
                
                std::vector<int> displayOps;
                for (int i = 0; i < opCount; ++i) {
                    if (!IsOperatorTypeOnField(i)) displayOps.push_back(i);
                }
                std::stable_sort(displayOps.begin(), displayOps.end(),
                                 [this](int lhs, int rhs) {
                                     const auto& a = m_OperatorTemplates.at(static_cast<std::size_t>(lhs));
                                     const auto& b = m_OperatorTemplates.at(static_cast<std::size_t>(rhs));
                                     return a.cost < b.cost;
                                 });
                
                const int dispCount = static_cast<int>(displayOps.size());
                const float totalW = dispCount * OP_CARD_WIDTH + (dispCount - 1) * OP_CARD_SPACING;
                const float startX = screenW - totalW - 24.0F;

                for (int idx = 0; idx < dispCount; ++idx) {
                    int i = displayOps[idx];
                    float cx = startX + idx * (OP_CARD_WIDTH + OP_CARD_SPACING);
                    if (screenCursorX >= cx && screenCursorX <= cx + OP_CARD_WIDTH &&
                        screenCursorY >= barY && screenCursorY <= barY + OP_CARD_HEIGHT) {
                        // Check availability
                        if (IsOperatorTypeAvailable(i) &&
                            static_cast<int>(m_Operators.size()) < MAX_OPS) {
                            const auto& opType = m_OperatorTemplates.at(static_cast<std::size_t>(i));
                            if (m_DP >= static_cast<float>(opType.cost)) {
                                m_DraggingFromBar = true;
                                m_DragOperatorType = i;
                                m_SelectedOperatorType = i;
                                m_DragScreenPos = {screenCursorX, screenCursorY};
                            }
                        }
                        break;
                    }
                }
            } else {
                // Click on board – check skill activation or operator selection
                const auto cell = ToCell(ptsdCursor);
                bool clickedOperator = false;
                if (cell && m_WaveRunning) {
                    for (auto& op : m_Operators) {
                        if (op.cell != *cell) continue;
                        clickedOperator = true;
                        m_SelectedOperatorId = op.id;
                        const auto& opType = m_OperatorTemplates.at(static_cast<std::size_t>(op.typeIndex));
                        const bool isBagpipe = (opType.name == "Bagpipe" || opType.name == u8"風笛");
                        if (!isBagpipe && opType.maxSp > 0 && op.sp >= opType.maxSp && !op.skillActive) {
                            op.skillActive  = true;
                            op.skillTimerMs = opType.skillDuration;
                            op.sp           = 0;
                            if (opType.name == "Myrtle" || opType.name == u8"桃金娘") {
                                m_DP = std::min(m_MaxDP, m_DP + 6.0F);
                            }
                            break;
                        }
                    }
                }
                if (!clickedOperator) m_SelectedOperatorId = -1;
            }
        }

        // ── Update drag position ──────────────────────────────────────
        if (m_DraggingFromBar && Util::Input::IsKeyPressed(Util::Keycode::MOUSE_LB)) {
            m_DragScreenPos = {screenCursorX, screenCursorY};
            
            // Show deploy preview based on hovered cell
            const auto cell = ToCell(ptsdCursor);
            if (cell) {
                m_IsDeploying = true;
                m_DeployingCell = *cell;
            }
        }

        // ── Release drag – drop on cell ───────────────────────────────
        if (m_DraggingFromBar && Util::Input::IsKeyUp(Util::Keycode::MOUSE_LB)) {
            const auto cell = ToCell(ptsdCursor);
            if (cell && m_WaveRunning && m_DragOperatorType >= 0) {
                // Temporarily set selected type for deploy check
                int prevSelected = m_SelectedOperatorType;
                m_SelectedOperatorType = m_DragOperatorType;

                if (IsOperatorTypeAvailable(m_DragOperatorType) &&
                    IsDeployableCellForSelectedOperator(*cell) && !IsCellOccupied(*cell) &&
                    static_cast<int>(m_Operators.size()) < MAX_OPS) {
                    const auto& opType = m_OperatorTemplates.at(static_cast<std::size_t>(m_DragOperatorType));
                    if (m_DP >= static_cast<float>(opType.cost)) {
                        // Enter direction selection phase
                        m_WaitingForDirection = true;
                        m_DirectionCell = *cell;
                        m_DeployingDirection = {1, 0};
                        m_IsDeploying = true;
                        m_DeployingCell = *cell;
                    } else {
                        m_DraggingFromBar = false;
                        m_IsDeploying = false;
                    }
                } else {
                    m_DraggingFromBar = false;
                    m_IsDeploying = false;
                }
                m_SelectedOperatorType = prevSelected;
            } else {
                m_DraggingFromBar = false;
                m_IsDeploying = false;
            }
        }
    }

    UpdateGame(dt * m_GameSpeedMultiplier);
    if (m_Renderer) {
        m_Renderer->DrawScene(ptsdCursor);
    }
}

void App::End() {}
