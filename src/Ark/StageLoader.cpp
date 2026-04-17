// ????????????????????????????????????????????????????????????????
// StageLoader.cpp  ?? Ark engine stage/entity loader
// ????????????????????????????????????????????????????????????????
#include "Ark/StageLoader.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <tuple>
#include <unordered_map>

#include "pch.hpp"

// ?? helpers ????????????????????????????????????????????????????
namespace {

const std::array<std::string, 4> BASE_CANDIDATES{
    ".", "tools/ark_builder", "../tools/ark_builder", "../../tools/ark_builder"
};

std::filesystem::path FindDir(const std::string& sub) {
    for (const auto& base : BASE_CANDIDATES) {
        auto p = std::filesystem::path(base) / sub;
        if (std::filesystem::exists(p) && std::filesystem::is_directory(p)) return p;
    }
    return {};
}

static const ImU32 PALETTE[6]{
    IM_COL32(220, 87,  92, 255), IM_COL32(236,142, 75, 255),
    IM_COL32(227,205, 88, 255), IM_COL32(122,219,154, 255),
    IM_COL32(117,179,238, 255), IM_COL32(193,135,236, 255),
};
inline ImU32 PaletteColor(std::size_t i) { return PALETTE[i % 6]; }

Ark::TileType ParseTile(const std::string& s) {
    if (s == "road")       return Ark::TileType::ROAD;
    if (s == "ground")     return Ark::TileType::GROUND;
    if (s == "highground") return Ark::TileType::HIGHGROUND;
    if (s == "spawn")      return Ark::TileType::SPAWN;
    if (s == "goal")       return Ark::TileType::GOAL;
    return Ark::TileType::EMPTY;
}

float Clamp01(float v) {
    return std::clamp(v, 0.0F, 1.0F);
}

std::filesystem::path ResolveAssetPath(const std::filesystem::path& stagePath, const std::string& value) {
    if (value.empty()) return {};

    const auto direct = std::filesystem::path(value);
    if (std::filesystem::exists(direct)) return direct.lexically_normal();

    if (stagePath.has_parent_path()) {
        const auto relative = stagePath.parent_path() / direct;
        if (std::filesystem::exists(relative)) return relative.lexically_normal();
    }
    return {};
}

// Build a map: display-name ??EnemyTemplate  (reads all *.json in enemy/)
// File names are the enemy_id (B2, 01, 02, ??.  The "id" field inside JSON
// is the display name used in level "enemies" sections.
std::unordered_map<std::string, Ark::EnemyTemplate> BuildEnemyRegistry(const std::filesystem::path& dir) {
    std::unordered_map<std::string, Ark::EnemyTemplate> reg;
    if (dir.empty()) return reg;
    std::size_t colorIdx = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() != ".json") continue;
        try {
            std::ifstream f(entry.path());
            nlohmann::json j; f >> j;
            Ark::EnemyTemplate tmpl;
            // "id" is the display name (used in level files & in-game labels)
            // "enemy_id" is the short code (equals filename stem)
            tmpl.id               = j.value("id", entry.path().stem().string());
            tmpl.enemyId          = j.value("enemy_id", entry.path().stem().string());
            tmpl.hp               = j.value("hp",               100.0F);
            tmpl.speed            = j.value("speed",            1.0F);
            tmpl.damage           = j.value("damage",           200.0F);
            tmpl.attackIntervalMs = j.value("attackIntervalMs", 2000.0F);
            tmpl.def              = j.value("def",              0.0F);
            tmpl.attackRange      = j.value("attackRange",      0.0F);
            tmpl.isRanged         = j.value("isRanged",         false);
            tmpl.canAttackOperator= j.value("canAttackOperator",true);
            tmpl.color = PaletteColor(colorIdx++);
            // Register by display name AND enemy_id so both look-ups work
            reg[tmpl.id]     = tmpl;
            reg[tmpl.enemyId]= tmpl;
        } catch (...) {}
    }
    return reg;
}

} // namespace

// ?? public API ?????????????????????????????????????????????????
namespace Ark {

std::optional<std::filesystem::path> ResolveStagePath(const std::string& level) {
    if (!level.empty()) {
        for (const auto& base : BASE_CANDIDATES) {
            for (const auto& ext : {std::string(""), std::string(".json")}) {
                auto p = std::filesystem::path(base) / "levels" / (level + ext);
                if (std::filesystem::exists(p)) return p;
            }
        }
        if (std::filesystem::exists(level)) return level;
    }
    for (const auto& base : BASE_CANDIDATES) {
        for (const char* f : {"levels/test.json", "levels/tutorial_1.json"}) {
            auto p = std::filesystem::path(base) / f;
            if (std::filesystem::exists(p)) return p;
        }
    }
    return std::nullopt;
}

std::filesystem::path ResolveEnemyDir()    { return FindDir("enemy"); }
std::filesystem::path ResolveOperatorDir() { return FindDir("operators"); }

// Load enemies by the display-name IDs used in the level file.
// Looks up each in the enemy registry (keyed by display name OR enemy_id).
std::vector<EnemyTemplate> LoadEnemies(const std::vector<std::string>& displayNames) {
    const auto reg = BuildEnemyRegistry(ResolveEnemyDir());
    std::vector<EnemyTemplate> result;
    std::size_t colorIdx = 0;
    for (const auto& name : displayNames) {
        auto it = reg.find(name);
        if (it != reg.end()) {
            EnemyTemplate t = it->second;
            t.color = PaletteColor(colorIdx++);
            result.push_back(t);
        } else {
            // Fallback: basic template
            EnemyTemplate t;
            t.id = name; t.enemyId = name;
            t.hp = 500; t.speed = 1; t.damage = 200; t.attackIntervalMs = 2000;
            t.canAttackOperator = true;
            t.color = PaletteColor(colorIdx++);
            result.push_back(t);
        }
    }
    return result;
}

// Load all operator definitions from operators/
std::vector<OperatorTemplate> LoadOperators() {
    const auto opDir = ResolveOperatorDir();
    std::vector<OperatorTemplate> result;

    static const std::unordered_map<std::string, ImU32> OP_COLORS{
        {"Vanguard", IM_COL32(68, 160, 255, 255)},
        {"Sniper",   IM_COL32(255, 196,  66, 255)},
        {"Myrtle",   IM_COL32(255, 215,   0, 255)},
    };

    if (opDir.empty()) return result;

    std::vector<std::filesystem::path> files;
    for (const auto& e : std::filesystem::recursive_directory_iterator(opDir))
        if (e.is_regular_file() && e.path().extension() == ".json") files.push_back(e.path());
    std::sort(files.begin(), files.end());

    for (const auto& p : files) {
        try {
            std::ifstream f(p);
            nlohmann::json j; f >> j;
            OperatorTemplate t;
            t.id              = j.value("id",              p.stem().string());
            t.name            = j.value("name",            t.id);
            t.cost            = j.value("cost",            10);
            t.hp              = j.value("hp",              1000.0F);
            t.damage          = j.value("damage",          300.0F);
            t.def             = j.value("def",             200.0F);
            t.attackIntervalMs= j.value("attackIntervalMs",1000.0F);
            t.blockCount      = j.value("blockCount",      0);
            t.maxSp           = j.value("maxSp",           0.0F);
            t.initialSp       = j.value("initialSp",       0.0F);
            t.skillDuration   = j.value("skillDuration",   0.0F);
            t.isVanguard      = j.value("isVanguard",      false);
            const std::string dt = j.value("deployType","ground");
            t.deployType = (dt == "highground") ? DeployType::HIGHGROUND_ONLY : DeployType::GROUND_ONLY;
            auto it = OP_COLORS.find(t.id);
            t.color = (it != OP_COLORS.end()) ? it->second : IM_COL32(200,200,200,255);
            if (!t.id.empty()) result.push_back(t);
        } catch (...) {}
    }
    return result;
}

// Load a full stage from JSON, wiring in enemies from the enemy/ directory
std::optional<StageData> LoadStageFromJson(const std::string& stageFile) {
    const auto stagePath = ResolveStagePath(stageFile);
    if (!stagePath.has_value()) return std::nullopt;

    try {
        std::ifstream file(*stagePath);
        if (!file.is_open()) return std::nullopt;
        nlohmann::json stage;
        file >> stage;
        if (!stage.is_object()) return std::nullopt;

        const int W = stage.value("width",  0);
        const int H = stage.value("height", 0);
        if (W <= 0 || H <= 0) return std::nullopt;
        if (!stage.contains("tiles") || !stage["tiles"].is_array() ||
            static_cast<int>(stage["tiles"].size()) != H) return std::nullopt;

        StageData data;
        data.name       = stage.value("name", "unnamed_stage");
        data.sourceFile = stagePath->lexically_normal().string();
        data.width  = W;
        data.height = H;
        if (stage.contains("camera") && stage["camera"].is_object()) {
            const auto& cam = stage["camera"];
            data.camera.projectionScaleX = cam.value("projection_scale_x", data.camera.projectionScaleX);
            data.camera.projectionScaleY = cam.value("projection_scale_y", data.camera.projectionScaleY);
            data.camera.zoom = cam.value("zoom", data.camera.zoom);
            data.camera.minZoom = cam.value("min_zoom", data.camera.minZoom);
            data.camera.maxZoom = cam.value("max_zoom", data.camera.maxZoom);
            data.camera.panX = cam.value("pan_x", data.camera.panX);
            data.camera.panY = cam.value("pan_y", data.camera.panY);
        }
        if (stage.contains("board_layout") && stage["board_layout"].is_object()) {
            const auto& boardLayout = stage["board_layout"];
            data.boardLayoutOverride.cellSize = boardLayout.value("cell_size", data.boardLayoutOverride.cellSize);
            data.boardLayoutOverride.topLeftX = boardLayout.value("top_left_x", data.boardLayoutOverride.topLeftX);
            data.boardLayoutOverride.topLeftY = boardLayout.value("top_left_y", data.boardLayoutOverride.topLeftY);
            data.hasBoardLayoutOverride = data.boardLayoutOverride.cellSize > 0.0F;
        }
        if (stage.contains("background") && stage["background"].is_object()) {
            const auto& background = stage["background"];
            const auto imagePath = ResolveAssetPath(*stagePath, background.value("image", std::string{}));
            if (!imagePath.empty()) data.backgroundImage = imagePath.string();
            data.backgroundAlpha = Clamp01(background.value("alpha", data.backgroundAlpha));
        }
        if (stage.contains("loading") && stage["loading"].is_object()) {
            const auto& loading = stage["loading"];
            const auto imagePath = ResolveAssetPath(*stagePath, loading.value("image", std::string{}));
            if (!imagePath.empty()) data.loadingImage = imagePath.string();
            data.loadingAlpha = Clamp01(loading.value("alpha", data.loadingAlpha));
        }
        if (stage.contains("finish") && stage["finish"].is_object()) {
            const auto& finish = stage["finish"];
            const auto imagePath = ResolveAssetPath(*stagePath, finish.value("image", std::string{}));
            if (!imagePath.empty()) data.finishImage = imagePath.string();
            data.finishAlpha = Clamp01(finish.value("alpha", data.finishAlpha));
        }
        data.tileMap.assign(static_cast<std::size_t>(H),
                            std::vector<TileType>(static_cast<std::size_t>(W), TileType::EMPTY));

        for (int y = 0; y < H; ++y) {
            const auto& row = stage["tiles"][static_cast<std::size_t>(y)];
            if (!row.is_array() || static_cast<int>(row.size()) != W) return std::nullopt;
            for (int x = 0; x < W; ++x) {
                const auto& cell = row[static_cast<std::size_t>(x)];
                if (cell.is_string())
                    data.tileMap[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                        ParseTile(cell.get<std::string>());
            }
        }

        // Routes
        if (!stage.contains("routes") || !stage["routes"].is_object()) return std::nullopt;
        auto boardCenter = [](int x, int y) -> glm::vec2 {
            return {static_cast<float>(x)+0.5F, static_cast<float>(y)+0.5F};
        };
        for (const auto& [routeId, nodes] : stage["routes"].items()) {
            if (!nodes.is_array() || nodes.empty()) continue;
            Route r; r.id = routeId;
            for (const auto& nd : nodes) {
                if (!nd.is_object()) continue;
                int nx = nd.value("x", -1), ny = nd.value("y", -1);
                if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
                r.nodes.push_back(RouteNode{boardCenter(nx, ny), nd.value("wait", 0.0F)});
            }
            if (r.nodes.size() >= 2) data.routes.push_back(std::move(r));
        }
        if (data.routes.empty()) return std::nullopt;

        // Collect enemy display names from level "enemies" section
        std::vector<std::string> enemyNames;
        if (stage.contains("enemies") && stage["enemies"].is_object())
            for (const auto& [eid, _] : stage["enemies"].items())
                enemyNames.push_back(eid);
        if (enemyNames.empty()) return std::nullopt;

        data.enemyTemplates = LoadEnemies(enemyNames);
        // Apply or override stats from level "enemies" section.
        for (auto& tmpl : data.enemyTemplates) {
            const nlohmann::json* ed = nullptr;
            if (stage["enemies"].contains(tmpl.id)) {
                ed = &stage["enemies"][tmpl.id];
            } else if (stage["enemies"].contains(tmpl.enemyId)) {
                ed = &stage["enemies"][tmpl.enemyId];
            }
            if (ed != nullptr && ed->is_object()) {
                tmpl.hp = ed->value("hp", tmpl.hp);
                tmpl.speed = ed->value("speed", tmpl.speed);
                tmpl.damage = ed->value("damage", tmpl.damage);
                tmpl.attackIntervalMs = ed->value("attackIntervalMs", tmpl.attackIntervalMs);
                tmpl.def = ed->value("def", tmpl.def);
                tmpl.attackRange = ed->value("attackRange", tmpl.attackRange);
                tmpl.isRanged = ed->value("isRanged", tmpl.isRanged);
                tmpl.canAttackOperator = ed->value("canAttackOperator", tmpl.canAttackOperator);
            }
        }
        if (data.enemyTemplates.empty()) return std::nullopt;

        // Index helpers
        auto findEnemy = [&](const std::string& name) -> int {
            for (std::size_t i = 0; i < data.enemyTemplates.size(); ++i)
                if (data.enemyTemplates[i].id == name || data.enemyTemplates[i].enemyId == name)
                    return static_cast<int>(i);
            return -1;
        };
        auto findRoute = [&](const std::string& id) -> int {
            for (std::size_t i = 0; i < data.routes.size(); ++i)
                if (data.routes[i].id == id) return static_cast<int>(i);
            return -1;
        };

        // Waves
        if (!stage.contains("waves") || !stage["waves"].is_array()) return std::nullopt;
        data.totalWaves = static_cast<int>(stage["waves"].size());
        for (std::size_t wi = 0; wi < stage["waves"].size(); ++wi) {
            const auto& wave = stage["waves"][wi];
            if (!wave.is_object()) continue;
            int   ei    = findEnemy(wave.value("enemy",""));
            int   ri    = findRoute(wave.value("route",""));
            int   cnt   = wave.value("count",    0);
            float start = wave.value("start",    0.0F);
            float inter = wave.value("interval", 0.0F);
            if (ei < 0 || ri < 0 || cnt <= 0) continue;
            for (int u = 0; u < cnt; ++u)
                data.wavePlans.push_back(WavePlan{static_cast<int>(wi)+1, ei, ri, start+inter*u});
        }
        if (data.wavePlans.empty()) return std::nullopt;

        std::sort(data.wavePlans.begin(), data.wavePlans.end(),
            [](const WavePlan& a, const WavePlan& b){
                if (a.spawnTimeSec == b.spawnTimeSec)
                    return std::tie(a.waveIndex,a.routeIndex,a.enemyTypeIndex) <
                           std::tie(b.waveIndex,b.routeIndex,b.enemyTypeIndex);
                return a.spawnTimeSec < b.spawnTimeSec;
            });
        return data;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace Ark
