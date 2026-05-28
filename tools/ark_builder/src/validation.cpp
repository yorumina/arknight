#include "validation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "stage.hpp"

namespace ark_builder {
namespace {

auto ResolveEnemyDir() -> std::filesystem::path {
    const std::array<std::filesystem::path, 4> candidates{
        std::filesystem::path("data/enemy"),
        std::filesystem::path("../data/enemy"),
        std::filesystem::path("../../data/enemy"),
        std::filesystem::path("../../../data/enemy"),
    };

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate) &&
            std::filesystem::is_directory(candidate)) {
            return candidate.lexically_normal();
        }
    }
    return {};
}

auto LoadEnemyAliases() -> std::unordered_map<std::string, std::unordered_set<std::string>> {
    std::unordered_map<std::string, std::unordered_set<std::string>> aliases;
    const auto enemyDir = ResolveEnemyDir();
    if (enemyDir.empty()) return aliases;

    for (const auto& entry : std::filesystem::directory_iterator(enemyDir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;

        try {
            std::ifstream file(entry.path());
            json enemy;
            file >> enemy;
            const std::string displayId = enemy.value("id", entry.path().stem().string());
            const std::string enemyId = enemy.value("enemy_id", entry.path().stem().string());
            aliases[displayId].insert(displayId);
            aliases[displayId].insert(enemyId);
            aliases[enemyId].insert(displayId);
            aliases[enemyId].insert(enemyId);
        } catch (const std::exception&) {
        }
    }

    return aliases;
}

auto BuildRuntimeEnemyIds(const json& stage) -> std::unordered_set<std::string> {
    std::unordered_set<std::string> ids;
    if (!stage.contains("enemies") || !stage["enemies"].is_object()) {
        return ids;
    }

    const auto aliases = LoadEnemyAliases();
    for (auto it = stage["enemies"].begin(); it != stage["enemies"].end(); ++it) {
        const std::string key = it.key();
        ids.insert(key);

        if (const auto aliasIt = aliases.find(key); aliasIt != aliases.end()) {
            ids.insert(aliasIt->second.begin(), aliasIt->second.end());
        }

        const auto& enemy = it.value();
        if (enemy.is_object() && enemy.contains("enemy_id") && enemy["enemy_id"].is_string()) {
            const std::string enemyId = enemy["enemy_id"].get<std::string>();
            ids.insert(enemyId);
            if (const auto aliasIt = aliases.find(enemyId); aliasIt != aliases.end()) {
                ids.insert(aliasIt->second.begin(), aliasIt->second.end());
            }
        }
    }

    return ids;
}

} // namespace

void ValidateRootSchema(const json& stage, std::vector<std::string>& errors) {
    if (!stage.is_object()) {
        errors.push_back("stage root must be a JSON object");
        return;
    }

    if (!stage.contains("name") || !stage["name"].is_string()) {
        errors.push_back("missing or invalid field: name (string)");
    }
    if (!stage.contains("width") || !stage["width"].is_number_integer()) {
        errors.push_back("missing or invalid field: width (integer)");
    }
    if (!stage.contains("height") || !stage["height"].is_number_integer()) {
        errors.push_back("missing or invalid field: height (integer)");
    }
    if (!stage.contains("tiles") || !stage["tiles"].is_array()) {
        errors.push_back("missing or invalid field: tiles (2D array)");
    }
    if (!stage.contains("routes") || !stage["routes"].is_object()) {
        errors.push_back("missing or invalid field: routes (object)");
    }
    if (!stage.contains("enemies") || !stage["enemies"].is_object()) {
        errors.push_back("missing or invalid field: enemies (object)");
    }
    if (!stage.contains("waves") || !stage["waves"].is_array()) {
        errors.push_back("missing or invalid field: waves (array)");
    }
}

void ValidateTiles(const json& stage, std::vector<std::string>& errors) {
    if (!stage.contains("width") || !stage.contains("height") ||
        !stage.contains("tiles") || !stage["width"].is_number_integer() ||
        !stage["height"].is_number_integer() || !stage["tiles"].is_array()) {
        return;
    }

    const int width = stage["width"].get<int>();
    const int height = stage["height"].get<int>();
    if (width <= 0 || height <= 0) {
        errors.push_back("width and height must be positive");
        return;
    }

    const auto& tiles = stage["tiles"];
    if (static_cast<int>(tiles.size()) != height) {
        errors.push_back("tiles row count does not match height");
        return;
    }

    int spawnCount = 0;
    int goalCount = 0;
    for (int y = 0; y < height; ++y) {
        const auto& row = tiles[static_cast<size_t>(y)];
        if (!row.is_array()) {
            errors.push_back("tiles[" + std::to_string(y) + "] must be an array");
            continue;
        }
        if (static_cast<int>(row.size()) != width) {
            errors.push_back("tiles[" + std::to_string(y) +
                             "] length does not match width");
        }
        const int limit = std::min(static_cast<int>(row.size()), width);
        for (int x = 0; x < limit; ++x) {
            const auto& cell = row[static_cast<size_t>(x)];
            std::string tile;
            if (cell.is_string()) {
                tile = cell.get<std::string>();
            } else if (cell.is_object() && cell.contains("type") && cell["type"].is_string()) {
                tile = cell["type"].get<std::string>();
                if (cell.contains("image") && !cell["image"].is_string()) {
                    errors.push_back("tile image at (" + std::to_string(x) + "," +
                                     std::to_string(y) + ") must be string");
                }
            } else {
                errors.push_back("tile at (" + std::to_string(x) + "," +
                                 std::to_string(y) + ") must be string or object with type");
                continue;
            }
            if (!IsKnownTile(tile)) {
                errors.push_back("unknown tile type at (" + std::to_string(x) +
                                 "," + std::to_string(y) + "): " + tile);
            }
            if (tile == "spawn") {
                ++spawnCount;
            }
            if (tile == "goal") {
                ++goalCount;
            }
        }
    }

    if (stage.contains("tile_images")) {
        if (!stage["tile_images"].is_array()) {
            errors.push_back("tile_images must be a 2D array when present");
        } else if (static_cast<int>(stage["tile_images"].size()) != height) {
            errors.push_back("tile_images row count does not match height");
        } else {
            const auto& tileImages = stage["tile_images"];
            for (int y = 0; y < height; ++y) {
                const auto& row = tileImages[static_cast<size_t>(y)];
                if (!row.is_array()) {
                    errors.push_back("tile_images[" + std::to_string(y) + "] must be an array");
                    continue;
                }
                if (static_cast<int>(row.size()) != width) {
                    errors.push_back("tile_images[" + std::to_string(y) +
                                     "] length does not match width");
                    continue;
                }
                for (int x = 0; x < width; ++x) {
                    if (!row[static_cast<size_t>(x)].is_string()) {
                        errors.push_back("tile_images at (" + std::to_string(x) + "," +
                                         std::to_string(y) + ") must be string");
                    }
                }
            }
        }
    }

    if (spawnCount == 0) {
        errors.push_back("at least one spawn tile is required");
    }
    if (goalCount == 0) {
        errors.push_back("at least one goal tile is required");
    }
}

void ValidateRoutes(const json& stage, std::vector<std::string>& errors) {
    if (!stage.contains("width") || !stage["width"].is_number_integer() ||
        !stage.contains("height") || !stage["height"].is_number_integer() ||
        !stage.contains("routes") || !stage["routes"].is_object() ||
        !stage.contains("tiles") || !stage["tiles"].is_array()) {
        return;
    }

    const int width = stage["width"].get<int>();
    const int height = stage["height"].get<int>();
    const auto& routes = stage["routes"];

    for (auto it = routes.begin(); it != routes.end(); ++it) {
        const std::string routeId = it.key();
        const auto& route = it.value();
        if (!route.is_array() || route.empty()) {
            errors.push_back("route '" + routeId +
                             "' must be a non-empty array");
            continue;
        }

        std::optional<int> previousX;
        std::optional<int> previousY;
        for (size_t index = 0; index < route.size(); ++index) {
            const auto& node = route[index];
            if (!node.is_object()) {
                errors.push_back("route '" + routeId + "' node " +
                                 std::to_string(index) +
                                 " must be an object");
                continue;
            }

            if (!node.contains("x") || !node["x"].is_number_integer() ||
                !node.contains("y") || !node["y"].is_number_integer()) {
                errors.push_back("route '" + routeId + "' node " +
                                 std::to_string(index) +
                                 " must include integer x/y");
                continue;
            }
            if (!node.contains("wait") || !node["wait"].is_number()) {
                errors.push_back("route '" + routeId + "' node " +
                                 std::to_string(index) +
                                 " must include numeric wait");
                continue;
            }

            const int x = node["x"].get<int>();
            const int y = node["y"].get<int>();
            const double wait = node["wait"].get<double>();

            if (!IsTileInBounds(x, y, width, height)) {
                errors.push_back("route '" + routeId + "' node " +
                                 std::to_string(index) +
                                 " is out of bounds");
                continue;
            }
            if (wait < 0.0) {
                errors.push_back("route '" + routeId + "' node " +
                                 std::to_string(index) +
                                 " has negative wait");
            }

            const auto tile = GetTile(stage, x, y);
            if (!IsRouteTile(tile)) {
                errors.push_back("route '" + routeId + "' node " +
                                 std::to_string(index) +
                                 " must be on road/spawn/goal");
            }

            if (previousX.has_value() && previousY.has_value()) {
                const int distance =
                    std::abs(x - *previousX) + std::abs(y - *previousY);
                if (distance != 1) {
                    errors.push_back("route '" + routeId + "' nodes " +
                                     std::to_string(index - 1) + " and " +
                                     std::to_string(index) +
                                     " are not orthogonally adjacent");
                }
            }

            previousX = x;
            previousY = y;
        }

        if (!route.empty() && route.front().is_object() && route.back().is_object() &&
            route.front().contains("x") && route.front().contains("y") &&
            route.back().contains("x") && route.back().contains("y") &&
            route.front()["x"].is_number_integer() &&
            route.front()["y"].is_number_integer() &&
            route.back()["x"].is_number_integer() &&
            route.back()["y"].is_number_integer()) {
            const int startX = route.front()["x"].get<int>();
            const int startY = route.front()["y"].get<int>();
            const int endX = route.back()["x"].get<int>();
            const int endY = route.back()["y"].get<int>();
            if (IsTileInBounds(startX, startY, width, height) &&
                GetTile(stage, startX, startY) != "spawn") {
                errors.push_back("route '" + routeId +
                                 "' must start on a spawn tile");
            }
            if (IsTileInBounds(endX, endY, width, height) &&
                GetTile(stage, endX, endY) != "goal") {
                errors.push_back("route '" + routeId +
                                 "' must end on a goal tile");
            }
        }
    }
}

void ValidateEnemies(const json& stage, std::vector<std::string>& errors) {
    if (!stage.contains("enemies") || !stage["enemies"].is_object()) {
        return;
    }

    const auto& enemies = stage["enemies"];
    for (auto it = enemies.begin(); it != enemies.end(); ++it) {
        const std::string enemyId = it.key();
        const auto& enemy = it.value();
        if (!enemy.is_object()) {
            errors.push_back("enemy '" + enemyId + "' must be an object");
            continue;
        }

        if (!enemy.contains("hp") || !enemy["hp"].is_number()) {
            errors.push_back("enemy '" + enemyId + "' missing numeric hp");
        } else if (enemy["hp"].get<double>() <= 0.0) {
            errors.push_back("enemy '" + enemyId + "' hp must be > 0");
        }

        if (!enemy.contains("speed") || !enemy["speed"].is_number()) {
            errors.push_back("enemy '" + enemyId + "' missing numeric speed");
        } else if (enemy["speed"].get<double>() <= 0.0) {
            errors.push_back("enemy '" + enemyId + "' speed must be > 0");
        }
    }
}

void ValidateWaves(const json& stage, std::vector<std::string>& errors) {
    if (!stage.contains("waves") || !stage["waves"].is_array()) {
        return;
    }

    const bool hasEnemyMap =
        stage.contains("enemies") && stage["enemies"].is_object();
    const bool hasRouteMap =
        stage.contains("routes") && stage["routes"].is_object();
    const auto runtimeEnemyIds = BuildRuntimeEnemyIds(stage);

    const auto& waves = stage["waves"];
    for (size_t i = 0; i < waves.size(); ++i) {
        const auto& wave = waves[i];
        const auto waveLabel = "waves[" + std::to_string(i) + "]";
        if (!wave.is_object()) {
            errors.push_back(waveLabel + " must be an object");
            continue;
        }

        if (!wave.contains("enemy") || !wave["enemy"].is_string()) {
            errors.push_back(waveLabel + ".enemy must be string");
        }
        if (!wave.contains("route") || !wave["route"].is_string()) {
            errors.push_back(waveLabel + ".route must be string");
        }
        if (!wave.contains("count") || !wave["count"].is_number_integer()) {
            errors.push_back(waveLabel + ".count must be integer");
        } else if (wave["count"].get<int>() <= 0) {
            errors.push_back(waveLabel + ".count must be > 0");
        }
        if (!wave.contains("start") || !wave["start"].is_number()) {
            errors.push_back(waveLabel + ".start must be numeric");
        } else if (wave["start"].get<double>() < 0.0) {
            errors.push_back(waveLabel + ".start must be >= 0");
        }
        if (!wave.contains("interval") || !wave["interval"].is_number()) {
            errors.push_back(waveLabel + ".interval must be numeric");
        } else if (wave["interval"].get<double>() < 0.0) {
            errors.push_back(waveLabel + ".interval must be >= 0");
        }

        if (wave.contains("enemy") && wave["enemy"].is_string() && hasEnemyMap) {
            const auto enemyId = wave["enemy"].get<std::string>();
            if (runtimeEnemyIds.find(enemyId) == runtimeEnemyIds.end()) {
                errors.push_back(waveLabel + " references unknown enemy '" +
                                 enemyId + "'");
            }
        }

        if (wave.contains("route") && wave["route"].is_string() && hasRouteMap) {
            const auto routeId = wave["route"].get<std::string>();
            if (!stage["routes"].contains(routeId)) {
                errors.push_back(waveLabel + " references unknown route '" +
                                 routeId + "'");
            }
        }
    }
}

auto ValidateStage(const json& stage) -> std::vector<std::string> {
    std::vector<std::string> errors;
    ValidateRootSchema(stage, errors);
    ValidateTiles(stage, errors);
    ValidateRoutes(stage, errors);
    ValidateEnemies(stage, errors);
    ValidateWaves(stage, errors);
    return errors;
}

void PrintValidationResult(const std::vector<std::string>& errors) {
    if (errors.empty()) {
        std::cout << "Validation passed.\n";
        return;
    }

    std::cout << "Validation failed with " << errors.size() << " issue(s):\n";
    for (const auto& error : errors) {
        std::cout << "  - " << error << '\n';
    }
}

} // namespace ark_builder
