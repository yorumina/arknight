// ?????????????????????????????????????????????????????????????????????????????
// GameLogic.cpp  ?? Wave management, game state, stage init & animation loading
// ?????????????????????????????????????????????????????????????????????????????
#include "App.hpp"
#include "Ark/Renderer.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <tuple>

#include "config.hpp"
#include "Ark/StageLoader.hpp"
#include "Util/Input.hpp"
#include "Util/Keycode.hpp"

using namespace Ark;

namespace {
constexpr float WAVE_CLEAR_DP        = 5.0F;
constexpr float REDEPLOY_COOLDOWN_MS = 90000.0F; // 90 seconds
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
} // namespace

// ?????????????????????????????????????????????????????????????????????????????
// RESET / INIT
// ?????????????????????????????????????????????????????????????????????????????
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

    m_DP          = 12.0F;
    m_LifePoint   = 3;
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
    if (m_Renderer) {
        m_Renderer->LoadOperatorThumbnails();
    }
}

void App::LoadOperatorAnimations() {
    Util::Animation::ClearDecodedMediaCache();
    m_OperatorAnims.clear();
    m_OperatorAnims.resize(m_OperatorTemplates.size());

    // Helper: case-insensitive string
    auto toLower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    };

    auto extensionOf = [&](const std::filesystem::path& path) {
        return toLower(path.extension().string());
    };

    auto isApngPath = [&](const std::filesystem::path& path) {
        return extensionOf(path) == ".apng";
    };

    auto isSupportedAnimationPath = [&](const std::filesystem::path& path) {
        const auto ext = extensionOf(path);
        return ext == ".webm" || ext == ".apng";
    };

    auto sameStem = [&](const std::filesystem::path& lhs, const std::filesystem::path& rhs) {
        return toLower(lhs.stem().string()) == toLower(rhs.stem().string());
    };

    auto loadAnimClipFromMedia = [](const std::filesystem::path& mediaPath, bool loop) {
        OperatorAnimClip clip;
        clip.mediaPath = mediaPath.string();
        clip.loop = loop;
        return clip;
    };

    auto shouldPreferClip = [&](const OperatorAnimClip& target, const OperatorAnimClip& clip) {
        if (target.Empty() || clip.Empty()) return false;
        const std::filesystem::path targetPath(target.mediaPath);
        const std::filesystem::path clipPath(clip.mediaPath);
        return sameStem(targetPath, clipPath) &&
               !isApngPath(targetPath) &&
               isApngPath(clipPath);
    };

    auto assignClipIfLoaded = [&](OperatorAnimClip& target, OperatorAnimClip clip, bool force = false) {
        if (clip.Empty()) return;
        if (target.Empty() ||
            shouldPreferClip(target, clip) ||
            (force && !shouldPreferClip(clip, target))) {
            target = std::move(clip);
        }
    };

    // Helper: classify a single animation file and assign it to the correct clip
    // in the given set of 5 slots (start, def, attack, skill, die).
    auto classifyAndAssign = [&](const std::filesystem::path& mediaPath,
                                 OperatorAnimClip& start, OperatorAnimClip& def,
                                 OperatorAnimClip& attack, OperatorAnimClip& skill,
                                 OperatorAnimClip& die) {
        std::string animName = toLower(mediaPath.stem().string());

        const bool isDefaultAnim = animName.find("default") != std::string::npos;
        const bool isIdleAnim    = animName.find("idle") != std::string::npos;

        if (isDefaultAnim || isIdleAnim) {
            assignClipIfLoaded(def, loadAnimClipFromMedia(mediaPath, true), isDefaultAnim);
        } else if (animName.find("start") != std::string::npos
                   && animName.find("skill") == std::string::npos) {
            assignClipIfLoaded(start, loadAnimClipFromMedia(mediaPath, false));
        } else if (animName.find("attack") != std::string::npos) {
            // Prefer "attackloop" or "attack" over "attackbegin"/"attackend"
            if (animName.find("begin") != std::string::npos || animName.find("end") != std::string::npos) {
                assignClipIfLoaded(attack, loadAnimClipFromMedia(mediaPath, false));
            } else {
                assignClipIfLoaded(attack, loadAnimClipFromMedia(mediaPath, false), true);
            }
        } else if (animName.find("skill") != std::string::npos) {
            // "skillloop" or "skill" (prefer loop for the main skill anim)
            assignClipIfLoaded(skill, loadAnimClipFromMedia(mediaPath, true),
                               animName.find("loop") != std::string::npos);
        } else if (animName.find("die") != std::string::npos) {
            assignClipIfLoaded(die, loadAnimClipFromMedia(mediaPath, false));
        }
    };

    // Helper: scan all supported animation files in a single directory (non-recursive)
    auto scanDir = [&](const std::filesystem::path& dir,
                       OperatorAnimClip& start, OperatorAnimClip& def,
                       OperatorAnimClip& attack, OperatorAnimClip& skill,
                       OperatorAnimClip& die) {
        if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) return;
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            if (!isSupportedAnimationPath(entry.path())) continue;
            classifyAndAssign(entry.path(), start, def, attack, skill, die);
        }
    };

    const auto operatorDir = Ark::ResolveOperatorDir();
    if (operatorDir.empty()) return;

    // For each operator, scan the three subdirectories explicitly:
    //   front/       → front-facing clips (operator faces RIGHT)
    //   back/        → back-facing clips  (operator faces UP)
    //   front_flip/  → flipped front clips (operator faces LEFT or DOWN)
    for (std::size_t i = 0; i < m_OperatorTemplates.size(); ++i) {
        const auto& op = m_OperatorTemplates[i];
        const auto photoDir = operatorDir / op.id / "photo";
        if (!std::filesystem::exists(photoDir) || !std::filesystem::is_directory(photoDir)) continue;

        auto& pack = m_OperatorAnims[i];

        // front/ → RIGHT-facing
        scanDir(photoDir / "front",
                pack.start, pack.def, pack.attack, pack.skill, pack.die);

        // back/ → UP-facing
        scanDir(photoDir / "back",
                pack.startBack, pack.defBack, pack.attackBack, pack.skillBack, pack.dieBack);

        // front_flip/ → LEFT/DOWN-facing (pre-generated horizontally-flipped front)
        scanDir(photoDir / "front_flip",
                pack.startFlip, pack.defFlip, pack.attackFlip, pack.skillFlip, pack.dieFlip);
    }

    // Startup should stay light: by default we only index clip paths here.
    // Actual media decoding is lazy and backed by a persistent disk cache, so
    // previously seen clips can be restored quickly without re-running ffmpeg.
    auto envEnabled = [&](const char* name) {
        const char* raw = std::getenv(name);
        if (raw == nullptr || raw[0] == '\0') return false;
        const std::string value = toLower(raw);
        return value != "0" && value != "false" && value != "none" && value != "off";
    };
    if (!envEnabled("ARKNIGHT_ANIMATION_PRELOAD")) {
        return;
    }

    // Optional compatibility path for machines that prefer a longer loading screen
    // over any first-use animation decode.
    auto warmClip = [](const OperatorAnimClip& clip) {
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

bool App::LoadStageFromJsonModule() {
    auto result = Ark::LoadStageFromJson(m_CurrentStageFile);
    if (!result.has_value()) return false;

    auto& d = *result;
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

// ?????????????????????????????????????????????????????????????????????????????
// WAVE
// ?????????????????????????????????????????????????????????????????????????????
void App::StartWave() {
    if (m_GameOver || m_WaveRunning || m_MissionClear || m_PreStageWaiting || m_WavePlans.empty()) return;
    m_WaveRunning    = true;
    m_WaveElapsedSec = 0.0F;
    m_NextSpawnIndex = 0;
    m_SpawnedWaveUnits = 0;
    m_CurrentWave    = 0;
    m_Enemies.clear();
    m_Beams.clear();
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

// ?????????????????????????????????????????????????????????????????????????????
// UPDATE (main game tick)
// ?????????????????????????????????????????????????????????????????????????????
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

    // DP regen only while wave is running
    if (m_WaveRunning) {
        m_DP = std::min(m_MaxDP, m_DP + m_DPRegenPerSec * dtMs / 1000.0F);
    }

    const float dtSec = dtMs / 1000.0F;
    UpdateWave(dtSec * BATTLE_TIME_SCALE);
    UpdateEnemies(dtSec);
    UpdateOperators(dtMs);
    CleanupDefeatedEnemies();

    // Remove dead operators (trigger redeploy cooldown)
    for (const auto& op : m_Operators) {
        if (op.hp <= 0.0F) {
            if (m_SelectedOperatorId == op.id) m_SelectedOperatorId = -1;
            m_OperatorRedeployCooldownMs[op.typeIndex] = REDEPLOY_COOLDOWN_MS;
            // Cleanup animation instance
            if (static_cast<std::size_t>(op.typeIndex) < m_OperatorAnims.size()) {
                m_OperatorAnims[op.typeIndex].activeInstances.erase(op.id);
            }
        }
    }
    m_Operators.erase(
        std::remove_if(m_Operators.begin(), m_Operators.end(),
                       [](const Operator& op){ return op.hp <= 0.0F; }),
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
