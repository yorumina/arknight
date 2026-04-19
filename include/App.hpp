#ifndef APP_HPP
#define APP_HPP

#include "pch.hpp" // IWYU pragma: export

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "Ark/ArkTypes.hpp"
#include "Util/Animation.hpp"
#include "Util/Image.hpp"

// Top-down orthographic board projection (no perspective foreshortening).
constexpr float PERSPECTIVE_Y_SCALE = 1.0F;

namespace Ark {
class AppRenderer;
class GameLogic;
}

class App {
    friend class Ark::AppRenderer;
    friend class Ark::GameLogic;
public:
    enum class State { START, UPDATE, END };
    State GetCurrentState() const { return m_CurrentState; }

    void Start();
    void Update();
    void End(); // NOLINT(readability-convert-member-functions-to-static)

private:
    struct CameraState {
        float projectionScaleX = 1.0F;
        float projectionScaleY = PERSPECTIVE_Y_SCALE;
        float zoom = 1.0F;
        float minZoom = 0.7F;
        float maxZoom = 1.8F;
        glm::vec2 pan{0.0F, 0.0F};
        glm::vec2 defaultPan{0.0F, 0.0F};
        float defaultZoom = 1.0F;
        bool dragging = false;
        glm::vec2 dragCursorStart{0.0F, 0.0F};
        glm::vec2 dragPanStart{0.0F, 0.0F};
    };
    void InitializeStage();
    bool LoadStageFromJsonModule();
    void BuildFallbackStage();
    void LoadOperatorAnimations();
    void ResetCameraToStageDefaults();

    void UpdateGame(float dt);
    void UpdateWave(float deltaTimeSec);
    void UpdateBeams(float deltaTimeMs);
    void StartWave();
    void ResetDemo();
    void SpawnEnemy(const Ark::WavePlan& plan);
    void UpdateEnemies(float deltaTimeSec);
    void UpdateOperators(float deltaTimeMs);
    void CleanupDefeatedEnemies();
    void UpdateRedeployCooldowns(float dtMs);

    // Input
    void HandleDeploymentClick(const glm::vec2& cursorPtsd);
    void HandleSkillActivation(const glm::ivec2& cell);
    void UpdateCameraControls(float deltaTimeMs, const glm::vec2& rawCursor);
    glm::vec2 RawCursorToPtsd(const glm::vec2& rawCursor) const;

    bool IsCellOccupied(const glm::ivec2& cell) const;
    bool IsInsideBoard(const glm::vec2& ptsdPos) const;
    bool IsDeployableCellForSelectedOperator(const glm::ivec2& cell) const;
    bool IsDeployableTile(Ark::TileType tile, Ark::DeployType deployType) const;
    int  FindRouteIndex(const std::string& routeId) const;
    int  FindEnemyTemplateIndex(const std::string& enemyId) const;

    // ?? Operator availability ?????????????????????????????????????
    bool IsOperatorTypeOnField(int typeIndex) const;
    bool IsOperatorTypeAvailable(int typeIndex) const;

    std::optional<glm::ivec2> ToCell(const glm::vec2& ptsdPos) const;
    glm::vec2  ToBoardCenter(const glm::ivec2& cell) const;
    glm::vec2  ToPtsdPosition(const glm::vec2& boardPos) const;
    ImVec2     ToScreenPosition(const glm::vec2& ptsdPos) const;
    Ark::BoardLayout GetBoardLayout() const;

private:
    // ?? Stage ??????????????????????????????????????????????????????
    int         m_StageWidth = 14;
    int         m_StageHeight = 8;
    std::string m_StageName = "fallback_stage";
    std::string m_StageLoadSource = "fallback";
    std::string m_CurrentStageFile = "Operation 1-1/stage";

    std::vector<std::vector<Ark::TileType>> m_TileMap;
    std::vector<Ark::Route>                 m_Routes;
    std::vector<Ark::EnemyTemplate>         m_EnemyTemplates;
    std::vector<Ark::WavePlan>              m_WavePlans;
    CameraState m_Camera;
    bool m_HasBoardLayoutOverride = false;
    Ark::BoardLayout m_BoardLayoutOverride{};
    std::string m_StageBackgroundPath;
    float m_StageBackgroundAlpha = 1.0F;
    std::shared_ptr<Util::Image> m_StageBackground;
    std::string m_StageOverlayLoadedPath;
    std::string m_StageLoadingPath;
    float m_StageLoadingAlpha = 1.0F;
    std::string m_StageFinishPath;
    float m_StageFinishAlpha = 1.0F;

    // ?? Operator definitions (loaded from JSON) ????????????????????
    std::vector<Ark::OperatorTemplate> m_OperatorTemplates;

    // ?? State machine ??????????????????????????????????????????????
    State m_CurrentState = State::START;
    bool  m_GameOver     = false;
    bool  m_MissionClear = false;
    float m_ClearTimerMs = 0.0F;
    bool  m_PreStageWaiting = true;
    float m_PreStageTimerMs = 0.0F;
    bool  m_FinishExitRequested = false;
    float m_FinishExitTimerMs = 0.0F;

    // ?€?€ Economy ?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€
    float m_DP            = 10.0F;
    float m_MaxDP         = 99.0F;
    float m_DPRegenPerSec = 1.0F;
    int   m_LifePoint     = 10;
    int   m_KillCount     = 0;

    // ?? Deployment ?????????????????????????????????????????????????
    int  m_SelectedOperatorType = 0;
    bool m_IsDeploying          = false;
    glm::ivec2 m_DeployingCell{0, 0};
    glm::ivec2 m_DeployingDirection{1, 0};
    int m_SelectedOperatorId = -1;

    // ?? Drag-and-drop deployment ??????????????????????????????????????????????
    bool m_DraggingFromBar       = false;   // currently dragging from operator bar
    int  m_DragOperatorType      = -1;      // which operator type is being dragged
    glm::vec2 m_DragScreenPos{0, 0};        // current screen-space drag position
    bool m_WaitingForDirection   = false;    // placed on cell, waiting for direction click
    glm::ivec2 m_DirectionCell{0, 0};       // cell where operator was dropped
    
    // Direction selection drag state
    bool m_IsDirectionDragging = false;
    glm::vec2 m_DirectionDragStart{0, 0};

    // ?? Wave ??????????????????????????????????????????????????????????????
    int         m_CurrentWave     = 0;
    int         m_TotalWaves      = 0;
    bool        m_WaveRunning     = false;
    int         m_TotalWaveUnits  = 0;
    int         m_SpawnedWaveUnits= 0;
    float       m_WaveElapsedSec  = 0.0F;
    std::size_t m_NextSpawnIndex  = 0;

    // ?? ID counters ????????????????????????????????????????????????
    int m_NextOperatorId = 1;
    int m_NextEnemyId    = 1;

    // ?? Live entities ??????????????????????????????????????????????
    std::vector<Ark::Enemy>      m_Enemies;
    std::vector<Ark::Operator>   m_Operators;
    std::vector<Ark::AttackBeam> m_Beams;

    // ?? Redeploy cooldown (per operator type index, in ms) ????????
    // After retreat or death, 90 seconds before redeployment
    std::map<int, float> m_OperatorRedeployCooldownMs;

    // ?? Models ?????????????????????????????????????????????????????
    struct OperatorAnimPack {
        std::shared_ptr<Util::Animation> start;
        std::shared_ptr<Util::Animation> def;
        std::shared_ptr<Util::Animation> attack;
        std::shared_ptr<Util::Animation> skill;
        std::shared_ptr<Util::Animation> die;
        // Tracking active animations for currently deployed operators
        std::map<int, std::shared_ptr<Util::Animation>> activeInstances; 
    };
    std::vector<OperatorAnimPack> m_OperatorAnims;

    std::shared_ptr<Util::Animation> m_ModelVanguard;
    std::shared_ptr<Util::Image>     m_ModelGuard;
    std::shared_ptr<Util::Image>     m_ModelEnemy;

    std::vector<std::shared_ptr<Util::Image>> m_OperatorThumbnails;
    std::shared_ptr<Ark::AppRenderer> m_Renderer;
};

#endif
