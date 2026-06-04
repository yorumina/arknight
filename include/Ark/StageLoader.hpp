#pragma once
#include "Ark/ArkTypes.hpp"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Ark {

// Searches across common relative paths for level JSON files
std::optional<std::filesystem::path> ResolveStagePath(const std::string& specificLevel);

// Searches across common relative paths for enemy JSON dir (data/enemy)
std::filesystem::path ResolveEnemyDir();

// Searches across common relative paths for operator JSON dir (data/operators)
std::filesystem::path ResolveOperatorDir();

// Loads all enemy definitions from the data/enemy directory
std::vector<EnemyTemplate> LoadEnemies(const std::vector<std::string>& enemyIds);

// Loads all operator definitions from the data/operators directory
std::vector<OperatorTemplate> LoadOperators();
std::vector<OperatorTemplate> LoadOperatorsWithFallback();

// Loads a stage JSON and populates stage data + wave plans
struct StageData {
    std::string name;
    std::string sourceFile;
    int width = 0; int height = 0;
    std::vector<std::vector<TileType>> tileMap;
    std::vector<std::vector<std::string>> tileImages;
    std::vector<Route> routes;
    std::vector<EnemyTemplate> enemyTemplates;
    std::vector<WavePlan> wavePlans;
    int totalWaves = 0;

    struct CameraConfig {
        float projectionScaleX = 1.0F;
        float projectionScaleY = 1.0F;
        float projectionSkewX = 0.0F;
        float zoom = 1.0F;
        float minZoom = 0.7F;
        float maxZoom = 1.8F;
        float panX = 0.0F;
        float panY = 0.0F;
    } camera;

    bool hasBoardLayoutOverride = false;
    BoardLayout boardLayoutOverride{};
    BoardArtTransform boardArt{};

    std::string backgroundImage;
    float backgroundAlpha = 1.0F;
    std::string loadingImage;
    float loadingAlpha = 1.0F;
    std::string finishImage;
    float finishAlpha = 1.0F;
};

struct StageLoadResult {
    std::optional<StageData> data;
    std::vector<std::string> errors;

    bool Succeeded() const { return data.has_value(); }
};

StageLoadResult LoadStageFromJsonDetailed(const std::string& stageFile);
std::optional<StageData> LoadStageFromJson(const std::string& stageFile);

} // namespace Ark
