#include "commands.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>

#include "cli_utils.hpp"
#include "json_utils.hpp"
#include "stage.hpp"
#include "types.hpp"
#include "validation.hpp"

namespace ark_builder {

namespace {

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

} // namespace

void RunNew(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        throw CliError("new requires <file>");
    }
    const std::filesystem::path file = ResolveLevelFilePath(args[1]);
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

    const std::filesystem::path file = ResolveLevelFilePath(args[1]);
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

    const std::filesystem::path file = ResolveLevelFilePath(args[1]);
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

    const std::filesystem::path file = ResolveLevelFilePath(args[1]);
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

    const std::filesystem::path file = ResolveLevelFilePath(args[1]);
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

void RunValidate(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        throw CliError("validate requires <file>");
    }

    const auto stage = LoadJson(ResolveLevelFilePath(args[1]));
    const auto errors = ValidateStage(stage);
    PrintValidationResult(errors);
    if (!errors.empty()) {
        throw CliError("stage is invalid");
    }
}

void RunSimulate(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        throw CliError("simulate requires <file>");
    }

    const std::filesystem::path file = ResolveLevelFilePath(args[1]);
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

    const auto stage = LoadJson(ResolveLevelFilePath(args[1]));
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

} // namespace ark_builder
