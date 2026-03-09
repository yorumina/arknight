#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace {
using json = nlohmann::json;

class CliError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct SimulationRecord {
    int waveIndex{};
    int unitIndex{};
    std::string enemyId;
    std::string routeId;
    double spawnTime{};
    double goalTime{};
};

auto Usage() -> std::string {
    return R"(ArknightBuilder - Proposal Stage Builder (PTSD)

Commands:
  new <file> --name <name> --width <w> --height <h>
  paint <file> <x> <y> <tile> [--rect-width <w>] [--rect-height <h>]
  route-set <file> <route-id> <x,y[:wait]>...
  enemy-set <file> <enemy-id> --hp <value> --speed <value>
  spawn-add <file> --enemy <enemy-id> --route <route-id> --count <n> --start <sec> --interval <sec>
  validate <file>
  simulate <file> [--duration <sec>]
  show <file>

Tiles:
  empty | road | ground | highground | spawn | goal
)";
}

auto IsKnownTile(const std::string& tile) -> bool {
    static const std::vector<std::string> kAllowedTiles{
        "empty", "road", "ground", "highground", "spawn", "goal",
    };
    return std::find(kAllowedTiles.begin(), kAllowedTiles.end(), tile) !=
           kAllowedTiles.end();
}

auto IsRouteTile(const std::string& tile) -> bool {
    return tile == "ground" || tile == "road" || tile == "spawn" || tile == "goal";
}

auto FindOption(const std::vector<std::string>& args,
                std::string_view flag) -> std::optional<std::string> {
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == flag) {
            if (i + 1 >= args.size()) {
                throw CliError("missing value after option " +
                               std::string(flag));
            }
            return args[i + 1];
        }
    }
    return std::nullopt;
}

auto ParseInt(const std::string& value, std::string_view name) -> int {
    try {
        size_t index = 0;
        const int parsed = std::stoi(value, &index);
        if (index != value.size()) {
            throw CliError("invalid integer for " + std::string(name) + ": " +
                           value);
        }
        return parsed;
    } catch (const std::invalid_argument&) {
        throw CliError("invalid integer for " + std::string(name) + ": " +
                       value);
    } catch (const std::out_of_range&) {
        throw CliError("integer out of range for " + std::string(name) + ": " +
                       value);
    }
}

auto ParseDouble(const std::string& value, std::string_view name) -> double {
    try {
        size_t index = 0;
        const double parsed = std::stod(value, &index);
        if (index != value.size()) {
            throw CliError("invalid number for " + std::string(name) + ": " +
                           value);
        }
        return parsed;
    } catch (const std::invalid_argument&) {
        throw CliError("invalid number for " + std::string(name) + ": " +
                       value);
    } catch (const std::out_of_range&) {
        throw CliError("number out of range for " + std::string(name) + ": " +
                       value);
    }
}

auto RequireIntOption(const std::vector<std::string>& args, std::string_view flag,
                      std::string_view name) -> int {
    const auto option = FindOption(args, flag);
    if (!option.has_value()) {
        throw CliError("missing required option " + std::string(flag));
    }
    return ParseInt(*option, name);
}

auto RequireDoubleOption(const std::vector<std::string>& args,
                         std::string_view flag,
                         std::string_view name) -> double {
    const auto option = FindOption(args, flag);
    if (!option.has_value()) {
        throw CliError("missing required option " + std::string(flag));
    }
    return ParseDouble(*option, name);
}

auto RequireStringOption(const std::vector<std::string>& args,
                         std::string_view flag) -> std::string {
    const auto option = FindOption(args, flag);
    if (!option.has_value()) {
        throw CliError("missing required option " + std::string(flag));
    }
    return *option;
}

auto LoadJson(const std::filesystem::path& path) -> json {
    std::ifstream input(path);
    if (!input) {
        throw CliError("unable to open file: " + path.string());
    }

    json doc;
    try {
        input >> doc;
    } catch (const json::parse_error& error) {
        throw CliError("invalid JSON in " + path.string() + ": " +
                       std::string(error.what()));
    }
    return doc;
}

void SaveJson(const std::filesystem::path& path, const json& doc) {
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream output(path);
    if (!output) {
        throw CliError("unable to write file: " + path.string());
    }
    output << std::setw(2) << doc << '\n';
}

auto BuildBlankStage(const std::string& name, int width, int height) -> json {
    if (width <= 0 || height <= 0) {
        throw CliError("width and height must be positive");
    }

    json tiles = json::array();
    for (int y = 0; y < height; ++y) {
        json row = json::array();
        for (int x = 0; x < width; ++x) {
            row.push_back("empty");
        }
        tiles.push_back(std::move(row));
    }

    return json{
        {"name", name},
        {"width", width},
        {"height", height},
        {"tiles", std::move(tiles)},
        {"routes", json::object()},
        {"enemies", json::object()},
        {"waves", json::array()},
    };
}

auto IsTileInBounds(int x, int y, int width, int height) -> bool {
    return x >= 0 && y >= 0 && x < width && y < height;
}

auto GetTile(const json& stage, int x, int y) -> std::string {
    return stage.at("tiles").at(static_cast<size_t>(y)).at(static_cast<size_t>(x))
        .get<std::string>();
}

void SetTile(json& stage, int x, int y, const std::string& tile) {
    stage["tiles"][static_cast<size_t>(y)][static_cast<size_t>(x)] = tile;
}

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
            if (!cell.is_string()) {
                errors.push_back("tile at (" + std::to_string(x) + "," +
                                 std::to_string(y) + ") must be string");
                continue;
            }
            const std::string tile = cell.get<std::string>();
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
            if (!stage["enemies"].contains(enemyId)) {
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

struct ParsedRouteNode {
    int x{};
    int y{};
    double wait{};
};

auto ParseRouteNodeToken(const std::string& token) -> ParsedRouteNode {
    const auto commaPos = token.find(',');
    if (commaPos == std::string::npos) {
        throw CliError("invalid route node token: " + token +
                       " (expected x,y[:wait])");
    }

    const std::string xPart = token.substr(0, commaPos);
    const std::string rest = token.substr(commaPos + 1);
    const auto colonPos = rest.find(':');

    std::string yPart = rest;
    std::string waitPart = "0";
    if (colonPos != std::string::npos) {
        yPart = rest.substr(0, colonPos);
        waitPart = rest.substr(colonPos + 1);
    }

    ParsedRouteNode node{};
    node.x = ParseInt(xPart, "route node x");
    node.y = ParseInt(yPart, "route node y");
    node.wait = ParseDouble(waitPart, "route node wait");
    if (node.wait < 0.0) {
        throw CliError("route node wait must be >= 0");
    }
    return node;
}

void RunNew(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        throw CliError("new requires <file>");
    }
    const std::filesystem::path file = args[1];
    const std::string name = RequireStringOption(args, "--name");
    const int width = RequireIntOption(args, "--width", "width");
    const int height = RequireIntOption(args, "--height", "height");

    const auto stage = BuildBlankStage(name, width, height);
    SaveJson(file, stage);
    std::cout << "Created stage: " << file.string() << '\n';
}

void RunPaint(const std::vector<std::string>& args) {
    if (args.size() < 5) {
        throw CliError("paint requires <file> <x> <y> <tile>");
    }

    const std::filesystem::path file = args[1];
    const int x = ParseInt(args[2], "x");
    const int y = ParseInt(args[3], "y");
    const std::string tile = args[4];
    if (!IsKnownTile(tile)) {
        throw CliError("unknown tile type: " + tile);
    }

    int rectWidth = 1;
    int rectHeight = 1;
    if (const auto widthOption = FindOption(args, "--rect-width")) {
        rectWidth = ParseInt(*widthOption, "rect-width");
    }
    if (const auto heightOption = FindOption(args, "--rect-height")) {
        rectHeight = ParseInt(*heightOption, "rect-height");
    }
    if (rectWidth <= 0 || rectHeight <= 0) {
        throw CliError("rect-width and rect-height must be positive");
    }

    auto stage = LoadJson(file);
    if (!stage.contains("width") || !stage.contains("height") ||
        !stage["width"].is_number_integer() ||
        !stage["height"].is_number_integer()) {
        throw CliError("stage missing valid width/height");
    }
    const int stageWidth = stage["width"].get<int>();
    const int stageHeight = stage["height"].get<int>();

    for (int dy = 0; dy < rectHeight; ++dy) {
        for (int dx = 0; dx < rectWidth; ++dx) {
            const int px = x + dx;
            const int py = y + dy;
            if (!IsTileInBounds(px, py, stageWidth, stageHeight)) {
                throw CliError("paint region is out of bounds");
            }
            SetTile(stage, px, py, tile);
        }
    }

    SaveJson(file, stage);
    std::cout << "Painted tile(s) in " << file.string() << '\n';
}

void RunRouteSet(const std::vector<std::string>& args) {
    if (args.size() < 5) {
        throw CliError("route-set requires <file> <route-id> <x,y[:wait]>...");
    }

    const std::filesystem::path file = args[1];
    const std::string routeId = args[2];
    if (routeId.empty()) {
        throw CliError("route-id cannot be empty");
    }

    json route = json::array();
    for (size_t i = 3; i < args.size(); ++i) {
        const auto parsed = ParseRouteNodeToken(args[i]);
        route.push_back(
            {{"x", parsed.x}, {"y", parsed.y}, {"wait", parsed.wait}});
    }

    auto stage = LoadJson(file);
    stage["routes"][routeId] = route;
    SaveJson(file, stage);
    std::cout << "Updated route '" << routeId << "' in " << file.string()
              << '\n';
}

void RunEnemySet(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        throw CliError("enemy-set requires <file> <enemy-id>");
    }

    const std::filesystem::path file = args[1];
    const std::string enemyId = args[2];
    if (enemyId.empty()) {
        throw CliError("enemy-id cannot be empty");
    }

    const double hp = RequireDoubleOption(args, "--hp", "hp");
    const double speed = RequireDoubleOption(args, "--speed", "speed");
    if (hp <= 0.0 || speed <= 0.0) {
        throw CliError("hp and speed must be > 0");
    }

    auto stage = LoadJson(file);
    stage["enemies"][enemyId] = {{"hp", hp}, {"speed", speed}};
    SaveJson(file, stage);
    std::cout << "Updated enemy '" << enemyId << "' in " << file.string()
              << '\n';
}

void RunSpawnAdd(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        throw CliError("spawn-add requires <file>");
    }

    const std::filesystem::path file = args[1];
    const std::string enemy = RequireStringOption(args, "--enemy");
    const std::string route = RequireStringOption(args, "--route");
    const int count = RequireIntOption(args, "--count", "count");
    const double start = RequireDoubleOption(args, "--start", "start");
    const double interval =
        RequireDoubleOption(args, "--interval", "interval");
    if (count <= 0) {
        throw CliError("count must be > 0");
    }
    if (start < 0.0 || interval < 0.0) {
        throw CliError("start and interval must be >= 0");
    }

    auto stage = LoadJson(file);
    stage["waves"].push_back({
        {"enemy", enemy},
        {"route", route},
        {"count", count},
        {"start", start},
        {"interval", interval},
    });
    SaveJson(file, stage);
    std::cout << "Added spawn wave in " << file.string() << '\n';
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

void RunValidate(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        throw CliError("validate requires <file>");
    }

    const auto stage = LoadJson(args[1]);
    const auto errors = ValidateStage(stage);
    PrintValidationResult(errors);
    if (!errors.empty()) {
        throw CliError("stage is invalid");
    }
}

auto ComputeRouteDuration(const json& route, double speed) -> double {
    double time = 0.0;
    for (size_t i = 0; i < route.size(); ++i) {
        const auto& node = route[i];
        if (i > 0) {
            const auto& prev = route[i - 1];
            const int dx = std::abs(node["x"].get<int>() - prev["x"].get<int>());
            const int dy = std::abs(node["y"].get<int>() - prev["y"].get<int>());
            const int manhattanDistance = dx + dy;
            time += static_cast<double>(manhattanDistance) / speed;
        }
        time += node["wait"].get<double>();
    }
    return time;
}

void RunSimulate(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        throw CliError("simulate requires <file>");
    }

    const std::filesystem::path file = args[1];
    const auto stage = LoadJson(file);
    const auto errors = ValidateStage(stage);
    if (!errors.empty()) {
        PrintValidationResult(errors);
        throw CliError("cannot simulate invalid stage");
    }

    double duration = 60.0;
    if (const auto durationOption = FindOption(args, "--duration")) {
        duration = ParseDouble(*durationOption, "duration");
    }
    if (duration <= 0.0) {
        throw CliError("duration must be > 0");
    }

    std::vector<SimulationRecord> records;
    const auto& waves = stage["waves"];
    for (size_t waveIndex = 0; waveIndex < waves.size(); ++waveIndex) {
        const auto& wave = waves[waveIndex];
        const std::string enemyId = wave["enemy"].get<std::string>();
        const std::string routeId = wave["route"].get<std::string>();
        const int count = wave["count"].get<int>();
        const double start = wave["start"].get<double>();
        const double interval = wave["interval"].get<double>();
        const double speed =
            stage["enemies"][enemyId]["speed"].get<double>();
        const auto& route = stage["routes"][routeId];
        const double routeDuration = ComputeRouteDuration(route, speed);

        for (int unitIndex = 0; unitIndex < count; ++unitIndex) {
            const double spawnTime = start + interval * static_cast<double>(unitIndex);
            records.push_back(SimulationRecord{
                static_cast<int>(waveIndex),
                unitIndex,
                enemyId,
                routeId,
                spawnTime,
                spawnTime + routeDuration,
            });
        }
    }

    std::sort(records.begin(), records.end(), [](const SimulationRecord& lhs,
                                                 const SimulationRecord& rhs) {
        if (lhs.spawnTime == rhs.spawnTime) {
            if (lhs.waveIndex == rhs.waveIndex) {
                return lhs.unitIndex < rhs.unitIndex;
            }
            return lhs.waveIndex < rhs.waveIndex;
        }
        return lhs.spawnTime < rhs.spawnTime;
    });

    std::cout << "Simulation (duration=" << std::fixed << std::setprecision(2)
              << duration << "s):\n";
    std::cout << "wave unit enemy route spawn goal status\n";
    int escapedCount = 0;
    for (const auto& record : records) {
        const bool reachedGoalWithinDuration = record.goalTime <= duration;
        if (reachedGoalWithinDuration) {
            ++escapedCount;
        }
        std::cout << std::setw(4) << (record.waveIndex + 1) << " "
                  << std::setw(4) << (record.unitIndex + 1) << " "
                  << std::setw(8) << record.enemyId << " "
                  << std::setw(8) << record.routeId << " "
                  << std::setw(6) << record.spawnTime << " "
                  << std::setw(6) << record.goalTime << " "
                  << (reachedGoalWithinDuration ? "GOAL" : "ACTIVE") << '\n';
    }

    std::cout << "Total units: " << records.size()
              << ", reached goal within duration: " << escapedCount << '\n';
}

void RunShow(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        throw CliError("show requires <file>");
    }

    const auto stage = LoadJson(args[1]);
    const auto errors = ValidateStage(stage);

    std::cout << "Stage: " << stage.value("name", "<unnamed>") << '\n'
              << "Size : " << stage.value("width", 0) << "x"
              << stage.value("height", 0) << '\n'
              << "Routes: " << stage["routes"].size() << '\n'
              << "Enemies: " << stage["enemies"].size() << '\n'
              << "Waves: " << stage["waves"].size() << '\n';

    if (errors.empty()) {
        std::cout << "Status: valid\n";
    } else {
        std::cout << "Status: invalid (" << errors.size() << " issue(s))\n";
    }
}

} // namespace

auto main(int argc, char** argv) -> int {
    try {
        std::vector<std::string> args;
        for (int i = 1; i < argc; ++i) {
            args.emplace_back(argv[i]);
        }

        if (args.empty() || args[0] == "--help" || args[0] == "-h") {
            std::cout << Usage();
            return 0;
        }

        const auto& command = args[0];
        if (command == "new") {
            RunNew(args);
        } else if (command == "paint") {
            RunPaint(args);
        } else if (command == "route-set") {
            RunRouteSet(args);
        } else if (command == "enemy-set") {
            RunEnemySet(args);
        } else if (command == "spawn-add") {
            RunSpawnAdd(args);
        } else if (command == "validate") {
            RunValidate(args);
        } else if (command == "simulate") {
            RunSimulate(args);
        } else if (command == "show") {
            RunShow(args);
        } else {
            throw CliError("unknown command: " + command);
        }

        return 0;
    } catch (const CliError& error) {
        std::cerr << "error: " << error.what() << "\n\n" << Usage();
    } catch (const std::exception& error) {
        std::cerr << "fatal: " << error.what() << '\n';
    }
    return 1;
}
