#pragma once
#include "Ark/ArkTypes.hpp"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Ark {

// Searches across common relative paths for level JSON files
std::optional<std::filesystem::path> ResolveStagePath(const std::string& specificLevel);

// Searches across common relative paths for enemy JSON dir
std::filesystem::path ResolveEnemyDir();

// Searches across common relative paths for operator JSON dir  
std::filesystem::path ResolveOperatorDir();

// Loads all enemy definitions from the enemy/ directory
std::vector<EnemyTemplate> LoadEnemies(const std::vector<std::string>& enemyIds);

// Loads all operator definitions from the operators/ directory
std::vector<OperatorTemplate> LoadOperators();

// Loads a stage JSON and populates stage data + wave plans
struct StageData {
    std::string name;
    std::string sourceFile;
    int width = 0; int height = 0;
    std::vector<std::vector<TileType>> tileMap;
    std::vector<Route> routes;
    std::vector<EnemyTemplate> enemyTemplates;
    std::vector<WavePlan> wavePlans;
    int totalWaves = 0;

    struct CameraConfig {
        float projectionScaleX = 1.0F;
        float projectionScaleY = 0.5F;
        float zoom = 1.0F;
        float minZoom = 0.7F;
        float maxZoom = 1.8F;
        float panX = 0.0F;
        float panY = 0.0F;
    } camera;

    bool hasBoardLayoutOverride = false;
    BoardLayout boardLayoutOverride{};

    std::string backgroundImage;
    float backgroundAlpha = 1.0F;
};
std::optional<StageData> LoadStageFromJson(const std::string& stageFile);

} // namespace Ark
