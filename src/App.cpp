// ─────────────────────────────────────────────────────────────────────────────
// App.cpp  –Arknights clone   (main lifecycle & input handling)
//
// Implementation is split across multiple files:
//   Camera.cpp        –Projection & coordinate mapping
//   EnemySystem.cpp   –Enemy spawning, movement & AI
//   OperatorSystem.cpp–Operator combat, skills & deployment helpers
//   Renderer.cpp      –All drawing / rendering code
//   GameLogic.cpp     - Wave management, game state, stage init
// ─────────────────────────────────────────────────────────────────────────────
#include "App.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "Ark/GameConstants.hpp"
#include "Ark/Renderer.hpp"
#include "Ark/Renderer/RendererShared.hpp"
#include "Ark/StageLoader.hpp"
#include "Util/Input.hpp"
#include "Util/Keycode.hpp"
#include "Util/Time.hpp"
#include "config.hpp"

using namespace Ark;

namespace {
constexpr int MAX_OPS = Ark::GameConst::MAX_OPS;
constexpr float REDEPLOY_COOLDOWN_MS = Ark::GameConst::REDEPLOY_COOLDOWN_MS;

// Bottom operator bar layout constants (screen-space)
constexpr float OP_BAR_HEIGHT     = Ark::RendererConst::OP_BAR_HEIGHT;
constexpr float OP_CARD_WIDTH     = Ark::RendererConst::OP_CARD_WIDTH;
constexpr float OP_CARD_HEIGHT    = Ark::RendererConst::OP_CARD_HEIGHT;
constexpr float OP_CARD_SPACING   = Ark::RendererConst::OP_CARD_SPACING;
constexpr float LOADING_FADE_IN_MS = 500.0F;
constexpr float LOADING_HOLD_MS = 250.0F;
constexpr float LOADING_FADE_OUT_MS = 500.0F;
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
    m_LoadingTimerMs = 0.0F;
    m_CurrentState = State::LOADING;
}

void App::Loading() {
    if (Util::Input::IfExit() || Util::Input::IsKeyUp(Util::Keycode::ESCAPE)) {
        m_CurrentState = State::END;
        return;
    }

    const float dt = std::clamp(Util::Time::GetDeltaTimeMs(), 0.0F, 100.0F);

    auto drawLoading = [&](float alpha) {
        if (m_Renderer && !m_StageLoadingPath.empty()) {
            m_Renderer->DrawImageCover(m_StageLoadingPath, m_StageLoadingAlpha * std::clamp(alpha, 0.0F, 1.0F), true);
        }
    };

    // Phase 0: fade in the loading art before heavy initialization starts.
    if (m_LoadingPhase == 0) {
        m_LoadingTimerMs += dt;
        drawLoading(m_LoadingTimerMs / LOADING_FADE_IN_MS);
        if (m_LoadingTimerMs >= LOADING_FADE_IN_MS) {
            m_LoadingPhase = 1;
            m_LoadingTimerMs = 0.0F;
        }
        return;
    }

    // Phase 1: do heavy initialization while the loading art stays fully visible.
    if (m_LoadingPhase == 1) {
        drawLoading(1.0F);
        LoadOperatorAnimations();
        LoadEnemyAnimations();
        if (m_Renderer) {
            m_Renderer->LoadOperatorThumbnails();
        }
        m_LoadingPhase = 2;
        m_LoadingTimerMs = 0.0F;
        return;
    }

    // Phase 2: hold briefly, then fade out before entering gameplay.
    if (m_LoadingPhase == 2) {
        m_LoadingTimerMs += dt;
        if (m_LoadingTimerMs < LOADING_HOLD_MS) {
            drawLoading(1.0F);
            return;
        }

        const float fadeT = (m_LoadingTimerMs - LOADING_HOLD_MS) / LOADING_FADE_OUT_MS;
        drawLoading(1.0F - fadeT);
        if (fadeT < 1.0F) return;
    }

    // Done — transition to game. Since the loading screen fades here, skip the
    // pre-stage wait.
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

    auto buildDisplayOps = [&]() {
        std::vector<int> displayOps;
        const int opCount = static_cast<int>(m_OperatorTemplates.size());
        for (int i = 0; i < opCount; ++i) {
            if (!IsOperatorTypeOnField(i)) displayOps.push_back(i);
        }
        std::stable_sort(displayOps.begin(), displayOps.end(),
                         [this](int lhs, int rhs) {
                             const auto& a = m_OperatorTemplates.at(static_cast<std::size_t>(lhs));
                             const auto& b = m_OperatorTemplates.at(static_cast<std::size_t>(rhs));
                             return a.cost < b.cost;
                         });
        return displayOps;
    };

    auto findOperatorCardAt = [&](float x, float y) {
        const float barY = screenH - OP_BAR_HEIGHT;
        if (y < barY - 28.0F || y > barY + OP_CARD_HEIGHT || !m_WaveRunning) return -1;

        const auto displayOps = buildDisplayOps();
        const int dispCount = static_cast<int>(displayOps.size());
        if (dispCount <= 0) return -1;

        const float totalW = dispCount * OP_CARD_WIDTH + (dispCount - 1) * OP_CARD_SPACING;
        const float startX = screenW - totalW - 24.0F;
        for (int idx = 0; idx < dispCount; ++idx) {
            const int typeIndex = displayOps[idx];
            const float cx = startX + idx * (OP_CARD_WIDTH + OP_CARD_SPACING);
            const float cy = barY - (typeIndex == m_SelectedOperatorCardType ? 22.0F : 0.0F);
            if (x >= cx && x <= cx + OP_CARD_WIDTH && y >= cy && y <= cy + OP_CARD_HEIGHT) {
                return typeIndex;
            }
        }
        return -1;
    };

    auto canDragOperatorCard = [&](int typeIndex) {
        if (typeIndex < 0 || typeIndex >= static_cast<int>(m_OperatorTemplates.size())) return false;
        if (!IsOperatorTypeAvailable(typeIndex)) return false;
        if (static_cast<int>(m_Operators.size()) >= MAX_OPS) return false;
        const auto& opType = m_OperatorTemplates.at(static_cast<std::size_t>(typeIndex));
        return m_DP >= static_cast<float>(opType.cost);
    };

    auto operatorInfoVisible = [&]() {
        return m_DraggingFromBar || m_WaitingForDirection ||
               m_SelectedOperatorCardType >= 0 || m_SelectedOperatorId != -1;
    };

    auto operatorInfoTabAt = [&](float x, float y) {
        if (!operatorInfoVisible()) return -1;
        const float scale = uiLayout.scale;
        const float panelW = std::clamp(screenW * 0.29F, 390.0F, 560.0F);
        const float panelH = std::max(360.0F, screenH - OP_BAR_HEIGHT + 8.0F);
        const float portraitH = std::min(panelH * 0.62F, 500.0F);
        const float tabH = 44.0F * scale;
        if (x < 0.0F || x > panelW || y < portraitH || y > portraitH + tabH) return -1;
        const int tab = static_cast<int>(std::floor(x / (panelW / 3.0F)));
        return std::clamp(tab, 0, 2);
    };

    if (!m_GameOver && !m_MissionClear && !m_PreStageWaiting && !m_ShowQuitConfirm &&
        !uiConsumedLeftClick && Util::Input::IsKeyDown(Util::Keycode::MOUSE_LB)) {
        const int clickedTab = operatorInfoTabAt(screenCursorX, screenCursorY);
        if (clickedTab >= 0) {
            m_OperatorInfoTab = clickedTab;
            uiConsumedLeftClick = true;
        } else if (m_GamePaused) {
            const int clickedCard = findOperatorCardAt(screenCursorX, screenCursorY);
            if (clickedCard >= 0) {
                m_SelectedOperatorCardType = clickedCard;
                m_DragOperatorType = clickedCard;
                m_SelectedOperatorType = clickedCard;
                m_SelectedOperatorId = -1;
                m_OperatorInfoTab = 0;
                uiConsumedLeftClick = true;
            }
        }
    }

    if (!m_GameOver && !m_MissionClear && !m_PreStageWaiting &&
        !m_GamePaused && !m_ShowQuitConfirm && !uiConsumedLeftClick) {
        if (m_OperatorCardPressActive) {
            m_DragScreenPos = {screenCursorX, screenCursorY};
            if (Util::Input::IsKeyPressed(Util::Keycode::MOUSE_LB)) {
                const glm::vec2 diff = m_DragScreenPos - m_OperatorCardPressPos;
                if (!m_DraggingFromBar && glm::length(diff) > 8.0F &&
                    canDragOperatorCard(m_PressedOperatorCardType)) {
                    m_DraggingFromBar = true;
                    m_DragOperatorType = m_PressedOperatorCardType;
                    m_SelectedOperatorType = m_PressedOperatorCardType;
                    m_SelectedOperatorCardType = m_PressedOperatorCardType;
                    m_SelectedOperatorId = -1;
                    m_OperatorCardPressActive = false;
                    m_PressedOperatorCardType = -1;
                }
            }
            if (Util::Input::IsKeyUp(Util::Keycode::MOUSE_LB)) {
                m_OperatorCardPressActive = false;
                m_PressedOperatorCardType = -1;
            }
        }

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
                        newOp.deathAnimationFinished = false;
                        newOp.deathElapsedMs = 0.0F;
                        newOp.redeployCooldownStarted = false;
                        const bool isBagpipe = (opType.name == "Bagpipe" || opType.name == u8"風笛");
                        newOp.sp        = isBagpipe ? 2.0F : opType.initialSp;
                        if (isBagpipe) {
                            newOp.sp = std::clamp(newOp.sp, 0.0F, Ark::GameConst::BAGPIPE_MAX_SP);
                        } else {
                            newOp.sp = std::min(newOp.sp, opType.maxSp);
                        }

                        m_Operators.push_back(newOp);
                        m_SelectedOperatorId = -1;
                        m_SelectedOperatorCardType = -1;
                        m_DragOperatorType = -1;
                    } else {
                        // Cancel deployment if dropped inside the center area
                        m_SelectedOperatorId = -1;
                    }
                    m_WaitingForDirection = false;
                    m_DraggingFromBar = false;
                    m_IsDeploying = false;
                    m_IsDirectionDragging = false;
                    m_OperatorCardPressActive = false;
                    m_PressedOperatorCardType = -1;
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
        else if (Util::Input::IsKeyDown(Util::Keycode::MOUSE_LB) &&
                 !m_DraggingFromBar && !m_OperatorCardPressActive) {
            const int clickedCard = findOperatorCardAt(screenCursorX, screenCursorY);
            if (clickedCard >= 0) {
                m_SelectedOperatorCardType = clickedCard;
                m_DragOperatorType = clickedCard;
                m_SelectedOperatorType = clickedCard;
                m_SelectedOperatorId = -1;
                m_DragScreenPos = {screenCursorX, screenCursorY};
                m_OperatorInfoTab = 0;
                if (canDragOperatorCard(clickedCard)) {
                    m_OperatorCardPressActive = true;
                    m_PressedOperatorCardType = clickedCard;
                    m_OperatorCardPressPos = m_DragScreenPos;
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
                        m_SelectedOperatorCardType = -1;
                        m_DragOperatorType = -1;
                        m_OperatorCardPressActive = false;
                        m_PressedOperatorCardType = -1;
                        m_OperatorInfoTab = 0;
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
                if (!clickedOperator) {
                    m_SelectedOperatorId = -1;
                    m_SelectedOperatorCardType = -1;
                }
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
                    IsDeployableCellForOperatorType(m_DragOperatorType, *cell) && !IsCellOccupied(*cell) &&
                    static_cast<int>(m_Operators.size()) < MAX_OPS) {
                    const auto& opType = m_OperatorTemplates.at(static_cast<std::size_t>(m_DragOperatorType));
                    if (m_DP >= static_cast<float>(opType.cost)) {
                        // Enter direction selection phase
                        m_WaitingForDirection = true;
                        m_DirectionCell = *cell;
                        m_DeployingDirection = {1, 0};
                        m_IsDeploying = true;
                        m_DeployingCell = *cell;
                        m_DraggingFromBar = false;
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
            m_OperatorCardPressActive = false;
            m_PressedOperatorCardType = -1;
        }
    }

    const bool slowOperatorInfo = operatorInfoVisible() && !m_GamePaused && !m_ShowQuitConfirm;
    const float playSpeed = m_GameSpeedMultiplier * (slowOperatorInfo ? 0.25F : 1.0F);
    const float gameDt = (m_MissionClear || m_GameOver) ? dt : dt * playSpeed;
    UpdateGame(gameDt);
    if (m_Renderer) {
        m_Renderer->DrawScene(ptsdCursor);
    }
}

void App::End() {}
