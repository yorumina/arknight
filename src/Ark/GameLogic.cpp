// ?????????????????????????????????????????????????????????????????????????????
// GameLogic.cpp  ?? Wave management, game state, stage init & animation loading
// ?????????????????????????????????????????????????????????????????????????????
#include "App.hpp"

#include <algorithm>
#include <cmath>
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
constexpr float PRE_STAGE_WAIT_MS    = 3000.0F;  // 3 seconds
constexpr float FINISH_FADE_TO_BLACK_MS = 700.0F;
constexpr float FINISH_BLACKOUT_MS   = 1000.0F;  // 1 second
constexpr float FINISH_FADE_IN_MS    = 700.0F;
constexpr float FINISH_FADE_OUT_MS   = 700.0F;
constexpr float DEFAULT_CAMERA_SCALE_X = 1.0F;
constexpr float DEFAULT_CAMERA_SCALE_Y = PERSPECTIVE_Y_SCALE;
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
    m_ClearTimerMs= 0.0F;
    m_PreStageWaiting = true;
    m_PreStageTimerMs = 0.0F;
    m_FinishExitRequested = false;
    m_FinishExitTimerMs = 0.0F;

    m_DP          = 12.0F;
    m_LifePoint   = 10;
    m_KillCount   = 0;

    m_SelectedOperatorType = 0;
    m_IsDeploying          = false;
    m_SelectedOperatorId   = -1;
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
}

void App::LoadOperatorAnimations() {
    m_OperatorAnims.clear();
    m_OperatorAnims.resize(m_OperatorTemplates.size());

    // Helper: load a sequence of PNG frames from a directory as an Animation
    auto loadAnimFromDir = [](const std::string& dirPath, bool loop) -> std::shared_ptr<Util::Animation> {
        if (!std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath)) return nullptr;
        std::vector<std::string> frames;
        for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".png") {
                frames.push_back(entry.path().string());
            }
        }
        if (frames.empty()) return nullptr;
        std::sort(frames.begin(), frames.end());
        return std::make_shared<Util::Animation>(frames, true, 41, loop, 0); // ~24 FPS
    };

    // Helper: case-insensitive substring match for directory names
    auto toLower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
    };

    // For each operator, scan its sprite folder for animation directories
    for (std::size_t i = 0; i < m_OperatorTemplates.size(); ++i) {
        const auto& op = m_OperatorTemplates[i];
        std::string opSpriteDir = std::string(ASSETS_DIR) + "/sprites/operators/" + op.id;
        
        if (!std::filesystem::exists(opSpriteDir)) continue;

        // Scan all subdirectories and match them by keywords in their name
        for (const auto& entry : std::filesystem::directory_iterator(opSpriteDir)) {
            if (!entry.is_directory()) continue;
            std::string dirName = toLower(entry.path().filename().string());
            
            // Match by keyword: order matters (check "skillloop"/"skill" before "start")
            if (dirName.find("default") != std::string::npos) {
                if (!m_OperatorAnims[i].def)
                    m_OperatorAnims[i].def = loadAnimFromDir(entry.path().string(), true);
            } else if (dirName.find("start") != std::string::npos 
                       && dirName.find("skill") == std::string::npos) {
                if (!m_OperatorAnims[i].start)
                    m_OperatorAnims[i].start = loadAnimFromDir(entry.path().string(), false);
            } else if (dirName.find("attack") != std::string::npos) {
                // Prefer "attackloop" or "attack" over "attackbegin"/"attackend"
                if (dirName.find("begin") != std::string::npos || dirName.find("end") != std::string::npos) {
                    // Skip begin/end variants ??the loop/main clip is far more useful
                    if (!m_OperatorAnims[i].attack)
                        m_OperatorAnims[i].attack = loadAnimFromDir(entry.path().string(), false);
                } else {
                    m_OperatorAnims[i].attack = loadAnimFromDir(entry.path().string(), false);
                }
            } else if (dirName.find("skill") != std::string::npos) {
                // "skillloop" or "skill" (prefer loop for the main skill anim)
                if (dirName.find("loop") != std::string::npos || !m_OperatorAnims[i].skill)
                    m_OperatorAnims[i].skill = loadAnimFromDir(entry.path().string(), true);
            } else if (dirName.find("die") != std::string::npos) {
                if (!m_OperatorAnims[i].die)
                    m_OperatorAnims[i].die = loadAnimFromDir(entry.path().string(), false);
            }
        }
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
    m_Routes        = std::move(d.routes);
    m_EnemyTemplates= std::move(d.enemyTemplates);
    m_WavePlans     = std::move(d.wavePlans);
    m_TotalWaves    = d.totalWaves;
    m_TotalWaveUnits= static_cast<int>(m_WavePlans.size());

    m_HasBoardLayoutOverride = d.hasBoardLayoutOverride;
    if (m_HasBoardLayoutOverride) {
        m_BoardLayoutOverride = d.boardLayoutOverride;
    }

    const float stageProjection = std::max(0.05F, d.camera.projectionScaleX);
    m_Camera.projectionScaleX = stageProjection;
    m_Camera.projectionScaleY = stageProjection;
    m_Camera.minZoom = std::max(0.2F, d.camera.minZoom);
    m_Camera.maxZoom = std::max(m_Camera.minZoom, d.camera.maxZoom);
    m_Camera.defaultZoom = std::clamp(d.camera.zoom, m_Camera.minZoom, m_Camera.maxZoom);
    m_Camera.defaultPan = {d.camera.panX, d.camera.panY};
    ResetCameraToStageDefaults();

    m_StageLoadingPath = d.loadingImage;
    m_StageLoadingAlpha = d.loadingAlpha;
    m_StageFinishPath = d.finishImage;
    m_StageFinishAlpha = d.finishAlpha;
    m_StageBackgroundPath.clear();
    m_StageBackgroundAlpha = 1.0F;
    m_StageOverlayLoadedPath.clear();
    m_StageBackground.reset();
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
    m_StageLoadingPath.clear();
    m_StageFinishPath.clear();
    m_StageLoadingAlpha = 1.0F;
    m_StageFinishAlpha = 1.0F;
    m_Camera.projectionScaleX = DEFAULT_CAMERA_SCALE_X;
    m_Camera.projectionScaleY = DEFAULT_CAMERA_SCALE_Y;
    m_Camera.minZoom = DEFAULT_CAMERA_MIN_ZOOM;
    m_Camera.maxZoom = DEFAULT_CAMERA_MAX_ZOOM;
    m_Camera.defaultZoom = 1.0F;
    m_Camera.defaultPan = {0.0F, 0.0F};
    ResetCameraToStageDefaults();

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
    UpdateBeams(dtMs);

    if (m_PreStageWaiting) {
        m_PreStageTimerMs += dtMs;
        if (m_PreStageTimerMs >= PRE_STAGE_WAIT_MS) {
            m_PreStageWaiting = false;
        }
        return;
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
                        InitializeStage();
                        ResetDemo();
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
    UpdateWave(dtSec);
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
