// GameLogic.cpp - Wave management, game state, and stage init.
#include "App.hpp"
#include "Ark/Renderer.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <tuple>

#include "Ark/GameConstants.hpp"
#include "config.hpp"
#include "Ark/StageLoader.hpp"
#include "Util/Input.hpp"
#include "Util/Keycode.hpp"

using namespace Ark;

namespace {
constexpr float WAVE_CLEAR_DP        = 5.0F;
constexpr float REDEPLOY_COOLDOWN_MS = GameConst::REDEPLOY_COOLDOWN_MS;
constexpr float PRE_STAGE_WAIT_MS    = 1500.0F;  // 2 seconds
constexpr float FINISH_FADE_TO_BLACK_MS = 700.0F;
constexpr float FINISH_BLACKOUT_MS   = 1000.0F;  // 1 second
constexpr float FINISH_FADE_IN_MS    = 700.0F;
constexpr float FINISH_FADE_OUT_MS   = 700.0F;
constexpr float BATTLE_TIME_SCALE     = 0.5F;
constexpr float DEFAULT_CAMERA_SCALE_X = 1.0F;
constexpr float DEFAULT_CAMERA_SCALE_Y = PERSPECTIVE_Y_SCALE;
constexpr float DEFAULT_CAMERA_SKEW_X = PERSPECTIVE_X_SHEAR;
constexpr float DEFAULT_CAMERA_MIN_ZOOM = 0.7F;
constexpr float DEFAULT_CAMERA_MAX_ZOOM = 1.8F;
constexpr const char* STAGE_1_1_FILE = "Operation 1-1/stage";
constexpr const char* STAGE_1_2_FILE = "Operation 1-2/stage";

const char* GetEnvOption(const char* name) {
    if (const char* value = std::getenv(name);
        value != nullptr && value[0] != '\0') {
        return value;
    }

    std::string lowerName(name);
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lowerName != name) {
        if (const char* value = std::getenv(lowerName.c_str());
            value != nullptr && value[0] != '\0') {
            return value;
        }
    }
    return nullptr;
}

bool EnvEnabled(const char* name) {
    const char* raw = GetEnvOption(name);
    if (raw == nullptr || raw[0] == '\0') return false;

    std::string value(raw);
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value != "0" && value != "false" && value != "none" && value != "off";
}

} // namespace

// Reset / init
void App::ResetDemo() {
    m_GameOver    = false;
    m_MissionClear= false;
    m_GamePaused  = false;
    m_ShowQuitConfirm = false;
    m_PauseBeforeQuitConfirm = false;
    m_GameSpeedMultiplier = 1.0F;
    m_ClearTimerMs= 0.0F;
    m_PreStageWaiting = true;
    m_PreStageTimerMs = 0.0F;
    m_FinishExitRequested = false;
    m_FinishExitTimerMs = 0.0F;

    m_DP          = 10.0F;
    m_DPRegenTimerMs = 0.0F;
    m_LifePoint   = 10;
    m_KillCount   = 0;

    m_SelectedOperatorType = 0;
    m_IsDeploying          = false;
    m_SelectedOperatorId   = -1;
    m_DraggingFromBar      = false;
    m_DragOperatorType     = -1;
    m_WaitingForDirection  = false;
    ResetCameraToStageDefaults();

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
    m_OperatorRedeployCooldownMs.clear();
    for (auto& pack : m_OperatorAnims) pack.activeInstances.clear();
    for (auto& pack : m_EnemyAnims) {
        pack.activeInstances.clear();
        pack.attackInstances.clear();
        pack.dieInstances.clear();
    }
}

void App::InitializeStage() {
    m_OperatorTemplates = Ark::LoadOperators();
    if (m_OperatorTemplates.empty()) {
        // Fallback hardcoded operators if JSON not found
        m_OperatorTemplates = {
            OperatorTemplate{"Bagpipe","風笛",  11,2664,769,382,1000,1,4,2,1000, IM_COL32(225,120,80,255), DeployType::GROUND_ONLY, true},
            OperatorTemplate{"Sniper","Sniper",   11,1200,500,150,1000,0,0,0,0, IM_COL32(255,196,66,255), DeployType::HIGHGROUND_ONLY, false},
            OperatorTemplate{"Myrtle","桃金娘",  8,1654,508,400,1000,1,22,13,8000, IM_COL32(255,215,0,255), DeployType::GROUND_ONLY, true},
        };
    }
    if (!LoadStageFromJsonModule()) BuildFallbackStage();
    LoadOperatorAnimations();
    LoadEnemyAnimations();
    if (m_Renderer) {
        m_Renderer->LoadOperatorThumbnails();
    }
}

void App::LoadOperatorAnimations() {
    Util::Animation::ClearDecodedMediaCache();
    m_OperatorAnims.clear();
    const auto clipPacks = Ark::LoadOperatorAnimationClips(m_OperatorTemplates);
    m_OperatorAnims.resize(clipPacks.size());
    for (std::size_t i = 0; i < clipPacks.size(); ++i) {
        static_cast<Ark::OperatorAnimationClips&>(m_OperatorAnims[i]) = clipPacks[i];
    }

    // Startup should stay light: by default we only index clip paths here.
    // Actual media decoding is lazy and backed by a persistent disk cache, so
    // previously seen clips can be restored quickly without re-running ffmpeg.
    if (!EnvEnabled("ARKNIGHT_ANIMATION_PRELOAD")) {
        return;
    }

    // Optional compatibility path for machines that prefer a longer loading screen
    // over any first-use animation decode.
    auto warmClip = [](const AnimationClip& clip) {
        if (!clip.Empty()) {
            Util::Animation warmup(clip.mediaPath, false, clip.loop, false);
        }
    };
    for (const auto& pack : m_OperatorAnims) {
        // Front
        warmClip(pack.start);
        warmClip(pack.def);
        warmClip(pack.attack);
        warmClip(pack.skill);
        warmClip(pack.die);
        
        // Back
        warmClip(pack.startBack);
        warmClip(pack.defBack);
        warmClip(pack.attackBack);
        warmClip(pack.skillBack);
        warmClip(pack.dieBack);
        
        // Flipped Front
        warmClip(pack.startFlip);
        warmClip(pack.defFlip);
        warmClip(pack.attackFlip);
        warmClip(pack.skillFlip);
        warmClip(pack.dieFlip);
    }
}

void App::LoadEnemyAnimations() {
    m_EnemyAnims.clear();
    const auto clipPacks = Ark::LoadEnemyAnimationClips(m_EnemyTemplates);
    m_EnemyAnims.resize(clipPacks.size());
    for (std::size_t i = 0; i < clipPacks.size(); ++i) {
        static_cast<Ark::EnemyAnimationClips&>(m_EnemyAnims[i]) = clipPacks[i];
    }

    if (!EnvEnabled("ARKNIGHT_ANIMATION_PRELOAD") &&
        !EnvEnabled("ARKNIGHT_ENEMY_ANIMATION_PRELOAD")) {
        return;
    }

    PreloadEnemyAnimationClips();
}

void App::PreloadEnemyAnimationClips() {
    if (m_EnemyAnims.empty()) return;

    std::vector<bool> usedTypes(m_EnemyAnims.size(), false);
    for (const auto& plan : m_WavePlans) {
        if (plan.enemyTypeIndex >= 0 &&
            plan.enemyTypeIndex < static_cast<int>(usedTypes.size())) {
            usedTypes[static_cast<std::size_t>(plan.enemyTypeIndex)] = true;
        }
    }

    if (std::none_of(usedTypes.begin(), usedTypes.end(), [](bool used) { return used; })) {
        std::fill(usedTypes.begin(), usedTypes.end(), true);
    }

    std::set<std::string> warmedPaths;
    auto warmClip = [&](AnimationClip& clip) {
        if (clip.Empty()) return;
        if (!warmedPaths.insert(clip.mediaPath).second) return;

        Util::Animation warm(clip.mediaPath, false, clip.loop, false);
        if (warm.GetFrameCount() == 0) {
            clip.mediaPath.clear();
        }
    };

    for (std::size_t i = 0; i < m_EnemyAnims.size(); ++i) {
        if (!usedTypes[i]) continue;
        auto& pack = m_EnemyAnims[i];
        warmClip(pack.idle);
        warmClip(pack.move);
        warmClip(pack.attack);
        warmClip(pack.die);
    }
}

bool App::LoadStageFromJsonModule() {
    auto result = Ark::LoadStageFromJsonDetailed(m_CurrentStageFile);
    if (!result.Succeeded()) return false;

    auto& d = *result.data;
    m_StageWidth    = d.width;
    m_StageHeight   = d.height;
    m_StageName     = d.name;
    m_StageLoadSource = d.sourceFile;
    m_TileMap       = std::move(d.tileMap);
    m_TileImageMap  = std::move(d.tileImages);
    m_Routes        = std::move(d.routes);
    m_EnemyTemplates= std::move(d.enemyTemplates);
    m_WavePlans     = std::move(d.wavePlans);
    m_TotalWaves    = d.totalWaves;
    m_TotalWaveUnits= static_cast<int>(m_WavePlans.size());

    m_HasBoardLayoutOverride = d.hasBoardLayoutOverride;
    if (m_HasBoardLayoutOverride) {
        m_BoardLayoutOverride = d.boardLayoutOverride;
    }

    m_Camera.projectionScaleX = std::max(0.05F, d.camera.projectionScaleX);
    m_Camera.projectionScaleY = std::max(0.05F, d.camera.projectionScaleY);
    m_Camera.projectionSkewX = d.camera.projectionSkewX;
    m_Camera.minZoom = std::max(0.2F, d.camera.minZoom);
    m_Camera.maxZoom = std::max(m_Camera.minZoom, d.camera.maxZoom);
    m_Camera.defaultZoom = std::clamp(d.camera.zoom, m_Camera.minZoom, m_Camera.maxZoom);
    m_Camera.defaultPan = {d.camera.panX, d.camera.panY};
    ResetCameraToStageDefaults();

    m_StageLoadingPath = d.loadingImage;
    m_StageLoadingAlpha = d.loadingAlpha;
    m_StageFinishPath = d.finishImage;
    m_StageFinishAlpha = d.finishAlpha;
    m_StageBackgroundPath = d.backgroundImage;
    m_StageBackgroundAlpha = d.backgroundAlpha;
    m_StageOverlayLoadedPath.clear();
    m_StageBackground.reset();
    m_TileImageCache.clear();
    return true;
}

void App::BuildFallbackStage() {
    m_StageWidth  = 14; m_StageHeight = 8;
    m_StageName   = "fallback_stage";
    m_StageLoadSource = "embedded fallback";
    m_HasBoardLayoutOverride = false;
    m_StageBackgroundPath.clear();
    m_StageBackgroundAlpha = 1.0F;
    m_StageBackground.reset();
    m_StageOverlayLoadedPath.clear();
    m_TileImageCache.clear();
    m_StageLoadingPath.clear();
    m_StageFinishPath.clear();
    m_StageLoadingAlpha = 1.0F;
    m_StageFinishAlpha = 1.0F;
    m_Camera.projectionScaleX = DEFAULT_CAMERA_SCALE_X;
    m_Camera.projectionScaleY = DEFAULT_CAMERA_SCALE_Y;
    m_Camera.projectionSkewX = DEFAULT_CAMERA_SKEW_X;
    m_Camera.minZoom = DEFAULT_CAMERA_MIN_ZOOM;
    m_Camera.maxZoom = DEFAULT_CAMERA_MAX_ZOOM;
    m_Camera.defaultZoom = 1.0F;
    m_Camera.defaultPan = {0.0F, 0.0F};
    ResetCameraToStageDefaults();

    m_TileMap.assign(static_cast<std::size_t>(m_StageHeight),
                     std::vector<TileType>(static_cast<std::size_t>(m_StageWidth), TileType::GROUND));
    m_TileImageMap.assign(static_cast<std::size_t>(m_StageHeight),
                          std::vector<std::string>(static_cast<std::size_t>(m_StageWidth)));
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

// Wave
void App::StartWave() {
    if (m_GameOver || m_WaveRunning || m_MissionClear || m_PreStageWaiting || m_WavePlans.empty()) return;
    m_WaveRunning    = true;
    m_WaveElapsedSec = 0.0F;
    m_NextSpawnIndex = 0;
    m_SpawnedWaveUnits = 0;
    m_DPRegenTimerMs = 0.0F;
    m_CurrentWave    = 0;
    m_Enemies.clear();
    m_Beams.clear();
    for (auto& pack : m_EnemyAnims) {
        pack.activeInstances.clear();
        pack.attackInstances.clear();
        pack.dieInstances.clear();
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

// Main game tick
void App::UpdateGame(float dtMs) {
    if (m_ShowQuitConfirm || m_GamePaused) {
        return;
    }

    UpdateBeams(dtMs);

    if (m_PreStageWaiting) {
        m_PreStageTimerMs += dtMs;
        if (m_PreStageTimerMs < PRE_STAGE_WAIT_MS) return;

        m_PreStageWaiting = false;
        // Auto-start stage immediately after the 1s wait.
        if (!m_WaveRunning && !m_GameOver && !m_MissionClear) {
            StartWave();
        }
    }

    if (m_GameOver || m_MissionClear) {
        if (m_MissionClear) {
            m_ClearTimerMs += dtMs;

            const float clickableAt = FINISH_FADE_TO_BLACK_MS + FINISH_BLACKOUT_MS + FINISH_FADE_IN_MS;
            if (!m_FinishExitRequested) {
                if (m_ClearTimerMs >= clickableAt && Util::Input::IsKeyPressed(Util::Keycode::MOUSE_LB)) {
                    m_FinishExitRequested = true;
                    m_FinishExitTimerMs = 0.0F;
                }
            } else {
                m_FinishExitTimerMs += dtMs;
                if (m_FinishExitTimerMs >= FINISH_FADE_OUT_MS) {
                    if (Ark::ResolveStagePath(STAGE_1_2_FILE).has_value()) {
                        m_CurrentStageFile = STAGE_1_2_FILE;
                        // Reload stage JSON (lightweight), then go through LOADING for animations
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
                        m_LoadingPhase = 0;
                        m_CurrentState = State::LOADING;
                    } else {
                        m_CurrentState = State::END;
                    }
                }
            }
        }
        return;
    }

    // DP regenerates in visible 1-second ticks. UpdateGame already receives
    // speed-scaled dt, so 2x speed completes a tick in 0.5 real seconds.
    if (m_WaveRunning) {
        m_DPRegenTimerMs += dtMs * std::max(0.0F, m_DPRegenPerSec);
        while (m_DPRegenTimerMs >= 1000.0F) {
            m_DP = std::min(m_MaxDP, m_DP + 1.0F);
            m_DPRegenTimerMs -= 1000.0F;
        }
    } else {
        m_DPRegenTimerMs = 0.0F;
    }

    const float dtSec = dtMs / 1000.0F;
    UpdateWave(dtSec * BATTLE_TIME_SCALE);
    UpdateEnemies(dtSec);
    UpdateOperators(dtMs);
    CleanupDefeatedEnemies();

    // Dead operators stay visible until their death animation finishes.
    for (auto& op : m_Operators) {
        if (op.hp <= 0.0F && !op.redeployCooldownStarted) {
            if (m_SelectedOperatorId == op.id) m_SelectedOperatorId = -1;
            m_OperatorRedeployCooldownMs[op.typeIndex] = REDEPLOY_COOLDOWN_MS;
            op.redeployCooldownStarted = true;
        }
    }
    m_Operators.erase(
        std::remove_if(m_Operators.begin(), m_Operators.end(),
                       [&](const Operator& op) {
            const bool shouldRemove = op.hp <= 0.0F && op.deathAnimationFinished;
            if (shouldRemove &&
                op.typeIndex >= 0 &&
                op.typeIndex < static_cast<int>(m_OperatorAnims.size())) {
                m_OperatorAnims[static_cast<std::size_t>(op.typeIndex)].activeInstances.erase(op.id);
            }
            return shouldRemove;
        }),
        m_Operators.end());

    // Tick redeploy cooldowns
    UpdateRedeployCooldowns(dtMs);

    if (m_WaveRunning && m_NextSpawnIndex >= m_WavePlans.size() && m_Enemies.empty()) {
        m_WaveRunning = false;
        m_MissionClear= true;
        m_ClearTimerMs= 0.0F;
        m_DP = std::min(m_MaxDP, m_DP + WAVE_CLEAR_DP);
    }
}

void App::UpdateBeams(float dtMs) {
    for (auto& b : m_Beams) b.ttlMs -= dtMs;
    m_Beams.erase(std::remove_if(m_Beams.begin(), m_Beams.end(),
        [](const AttackBeam& b){ return b.ttlMs <= 0; }), m_Beams.end());
}
