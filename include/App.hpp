#ifndef APP_HPP
#define APP_HPP

#include "pch.hpp" // IWYU pragma: export

#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "Ark/AnimationLoader.hpp"
#include "Ark/ArkTypes.hpp"
#include "Util/Animation.hpp"
#include "Util/Image.hpp"

// Top-down orthographic battle board projection.
constexpr float PERSPECTIVE_Y_SCALE = 1.0F;
constexpr float PERSPECTIVE_X_SHEAR = 0.0F;

namespace Ark {
class AppRenderer;
}

class App {
    friend class Ark::AppRenderer;
public:
    enum class State { START, LOADING, UPDATE, END };
    State GetCurrentState() const { return m_CurrentState; }

    void Start();
    void Loading();
    void Update();
    void End(); // NOLINT(readability-convert-member-functions-to-static)

private:
    struct CameraState {
        float projectionScaleX = 1.0F;
        float projectionScaleY = PERSPECTIVE_Y_SCALE;
        float projectionSkewX = PERSPECTIVE_X_SHEAR;
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
    bool LoadStageFromJsonModule();
    void BuildFallbackStage();
    void LoadOperatorAnimations();
    void LoadEnemyAnimations();
    void PreloadEnemyAnimationClips();
    void ResetCameraToStageDefaults();

    void UpdateGame(float dt);
    void UpdateWave(float deltaTimeSec);
    void UpdateBeams(float deltaTimeMs);
    void StartWave();
    void ResetDemo();
    void SpawnEnemy(const Ark::WavePlan& plan);
    void UpdateEnemies(float deltaTimeSec);
    void UpdateEnemyAnimation(Ark::Enemy& enemy, bool isMoving, bool isAttacking, float deltaTimeMs);
    void MarkEnemyDefeated(Ark::Enemy& enemy);
    void UpdateOperators(float deltaTimeMs);
    void CleanupDefeatedEnemies();
    void UpdateRedeployCooldowns(float dtMs);

    // Input
    void HandleSkillActivation(const glm::ivec2& cell);
    void UpdateCameraControls(float deltaTimeMs, const glm::vec2& rawCursor);
    glm::vec2 RawCursorToPtsd(const glm::vec2& rawCursor) const;

    bool IsValidOperatorTypeIndex(int typeIndex) const;
    bool IsCellOccupied(const glm::ivec2& cell) const;
    bool IsDeployableCellForOperatorType(int typeIndex, const glm::ivec2& cell) const;
    bool IsInsideBoard(const glm::vec2& ptsdPos) const;
    bool IsDeployableTile(Ark::TileType tile, Ark::DeployType deployType) const;

    // Operator availability
    bool IsOperatorTypeOnField(int typeIndex) const;
    bool IsOperatorTypeAvailable(int typeIndex) const;

    std::optional<glm::ivec2> ToCell(const glm::vec2& ptsdPos) const;
    std::optional<glm::ivec2> ResolveDeploymentCell(int typeIndex, const glm::vec2& ptsdPos) const;
    void RebuildBoardArtScreenCache();
    glm::vec2  ToBoardCenter(const glm::ivec2& cell) const;
    glm::vec2  ToPtsdPosition(const glm::vec2& boardPos) const;
    ImVec2     ToScreenPosition(const glm::vec2& ptsdPos) const;
    Ark::BoardLayout GetBoardLayout() const;
    bool UsesBoardArtTransform() const;
    bool IsBoardArtCellMapped(const glm::ivec2& cell) const;
    std::array<ImVec2, 4> GetCellQuad(const glm::ivec2& cell) const;
    float GetBoardCellScreenSize() const;

private:
    // Stage
    int         m_StageWidth = 14;
    int         m_StageHeight = 8;
    std::string m_StageName = "fallback_stage";
    std::string m_StageLoadSource = "fallback";
    std::string m_CurrentStageFile = "Operation 1-1/stage";

    std::vector<std::vector<Ark::TileType>> m_TileMap;
    std::vector<std::vector<std::string>>   m_TileImageMap;
    std::vector<Ark::Route>                 m_Routes;
    std::vector<Ark::EnemyTemplate>         m_EnemyTemplates;
    std::vector<Ark::WavePlan>              m_WavePlans;
    CameraState m_Camera;
    bool m_HasBoardLayoutOverride = false;
    Ark::BoardLayout m_BoardLayoutOverride{};
    Ark::BoardArtTransform m_BoardArtTransform{};
    std::vector<std::vector<bool>> m_BoardArtCellMappedCache;
    std::vector<std::vector<std::array<ImVec2, 4>>> m_BoardArtCellQuadCache;
    std::string m_StageBackgroundPath;
    float m_StageBackgroundAlpha = 1.0F;
    std::shared_ptr<Util::Image> m_StageBackground;
    std::map<std::string, std::shared_ptr<Util::Image>> m_TileImageCache;
    std::string m_StageOverlayLoadedPath;
    std::string m_StageLoadingPath;
    float m_StageLoadingAlpha = 1.0F;
    std::string m_StageFinishPath;
    float m_StageFinishAlpha = 1.0F;

    // Operator definitions loaded from JSON
    std::vector<Ark::OperatorTemplate> m_OperatorTemplates;

    // State machine
    State m_CurrentState = State::START;
    bool  m_GameOver     = false;
    bool  m_MissionClear = false;
    bool  m_GamePaused   = false;
    bool  m_ShowQuitConfirm = false;
    bool  m_PauseBeforeQuitConfirm = false;
    float m_GameSpeedMultiplier = 1.0F; // 1x or 2x
    bool  m_ShowMapModel = false;
    bool  m_CheatMode = false;
    float m_GameOverTimerMs = 0.0F;
    float m_ClearTimerMs = 0.0F;
    bool  m_PreStageWaiting = true;
    float m_PreStageTimerMs = 0.0F;
    bool  m_FinishExitRequested = false;
    float m_FinishExitTimerMs = 0.0F;
    int   m_LoadingPhase = 0;  // 0=fade in, 1=load assets, 2=hold/fade out
    float m_LoadingTimerMs = 0.0F;

    // Economy
    float m_DP            = 10.0F;
    float m_MaxDP         = 99.0F;
    float m_DPRegenPerSec = 1.0F;
    float m_DPRegenTimerMs = 0.0F;
    int   m_LifePoint     = 10;
    int   m_KillCount     = 0;

    // Deployment
    bool m_IsDeploying          = false;
    glm::ivec2 m_DeployingCell{0, 0};
    glm::ivec2 m_DeployingDirection{1, 0};
    int m_SelectedOperatorId = -1;

    // Drag-and-drop deployment
    bool m_DraggingFromBar       = false;   // currently dragging from operator bar
    int  m_DragOperatorType      = -1;      // which operator type is being dragged
    glm::vec2 m_DragScreenPos{0, 0};        // current screen-space drag position
    int  m_SelectedOperatorCardType = -1;
    bool m_OperatorCardPressActive = false;
    int  m_PressedOperatorCardType = -1;
    glm::vec2 m_OperatorCardPressPos{0, 0};
    int  m_OperatorInfoTab = 0;              // 0=skill, 1=feature, 2=talent
    float m_OperatorDetailDragBlend = 0.0F;
    bool m_WaitingForDirection   = false;    // placed on cell, waiting for direction click
    glm::ivec2 m_DirectionCell{0, 0};       // cell where operator was dropped
    
    // Direction selection drag state
    bool m_IsDirectionDragging = false;
    glm::vec2 m_DirectionDragStart{0, 0};

    // Wave
    int         m_CurrentWave     = 0;
    int         m_TotalWaves      = 0;
    bool        m_WaveRunning     = false;
    int         m_TotalWaveUnits  = 0;
    int         m_SpawnedWaveUnits= 0;
    float       m_WaveElapsedSec  = 0.0F;
    std::size_t m_NextSpawnIndex  = 0;

    // ID counters
    int m_NextOperatorId = 1;
    int m_NextEnemyId    = 1;

    // Live entities
    std::vector<Ark::Enemy>      m_Enemies;
    std::vector<Ark::Operator>   m_Operators;
    std::vector<Ark::AttackBeam> m_Beams;

    // Redeploy cooldown per operator type index, in ms.
    // After retreat or death, 90 seconds before redeployment
    std::map<int, float> m_OperatorRedeployCooldownMs;

    // Models and animation runtime state
    struct OperatorAnimPack : Ark::OperatorAnimationClips {
        std::map<int, std::shared_ptr<Util::Animation>> activeInstances; 
    };
    std::vector<OperatorAnimPack> m_OperatorAnims;

    struct EnemyAnimPack : Ark::EnemyAnimationClips {
        std::shared_ptr<Util::Animation> sharedIdleInstance;
        std::shared_ptr<Util::Animation> sharedMoveInstance;
        std::shared_ptr<Util::Animation> sharedIdleFlipInstance;
        std::shared_ptr<Util::Animation> sharedMoveFlipInstance;
        unsigned long sharedIdleUpdateSerial = 0;
        unsigned long sharedMoveUpdateSerial = 0;
        unsigned long sharedIdleFlipUpdateSerial = 0;
        unsigned long sharedMoveFlipUpdateSerial = 0;
        std::map<int, std::shared_ptr<Util::Animation>> attackInstances;
        std::map<int, std::shared_ptr<Util::Animation>> dieInstances;
        std::map<int, std::shared_ptr<Util::Animation>> activeInstances;
    };
    std::vector<EnemyAnimPack> m_EnemyAnims;
    unsigned long m_EnemyAnimationUpdateSerial = 0;

    std::shared_ptr<Util::Animation> m_ModelVanguard;
    std::shared_ptr<Util::Image>     m_ModelGuard;
    std::shared_ptr<Util::Image>     m_ModelEnemy;

    std::vector<std::shared_ptr<Util::Image>> m_OperatorThumbnails;
    std::vector<std::shared_ptr<Util::Image>> m_OperatorCards;
    std::vector<std::shared_ptr<Util::Image>> m_OperatorPortraits;
    std::vector<std::shared_ptr<Util::Image>> m_OperatorLevelImages;
    std::vector<std::shared_ptr<Util::Image>> m_OperatorSkillImages;
    std::vector<std::shared_ptr<Util::Image>> m_OperatorFeatureImages;
    std::shared_ptr<Util::Image> m_VanguardIcon;
    std::shared_ptr<Util::Image> m_SniperIcon;
    std::map<std::string, std::shared_ptr<Util::Image>> m_StaticImageCache;
    std::shared_ptr<Ark::AppRenderer> m_Renderer;
};

#endif
