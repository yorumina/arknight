// StageLoader.cpp - Ark engine stage/entity loader
#include "Ark/StageLoader.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <tuple>
#include <unordered_map>

#include "pch.hpp"
#include "Util/Logger.hpp"

namespace {

const std::array<std::string, 8> BASE_CANDIDATES{
    "data",
    ".",
    "../data",
    "..",
    "../../data",
    "../..",
    "../../../data",
    "../../..",
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

std::optional<Ark::EnemySpriteDirection> ParseRouteSpriteDirection(const std::string& value) {
    if (value == "normal" || value == "default") {
        return Ark::EnemySpriteDirection::NORMAL;
    }
    if (value == "flip" || value == "flipped") {
        return Ark::EnemySpriteDirection::FLIP;
    }
    return std::nullopt;
}

std::optional<Ark::EnemySpriteDirection> ReadRouteSpriteDirection(const nlohmann::json& node,
                                                                  std::string& fieldName,
                                                                  std::string& invalidValue) {
    for (const char* key : {"direction", "model", "facing"}) {
        if (!node.contains(key)) continue;
        fieldName = key;
        const auto& value = node[key];
        if (value.is_string()) {
            invalidValue = value.get<std::string>();
            return ParseRouteSpriteDirection(invalidValue);
        }
        if (value.is_boolean()) {
            return value.get<bool>() ? Ark::EnemySpriteDirection::FLIP
                                     : Ark::EnemySpriteDirection::NORMAL;
        }
        invalidValue = value.dump();
        return std::nullopt;
    }
    return std::nullopt;
}

Ark::TileType ParseTile(const std::string& s) {
    if (s == "road")       return Ark::TileType::ROAD;
    if (s == "ground")     return Ark::TileType::GROUND;
    if (s == "highground") return Ark::TileType::HIGHGROUND;
    if (s == "unusablehighground") return Ark::TileType::UNUSABLE_HIGHGROUND;
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

void LogStageLoadErrors(const std::string& stageFile, const std::vector<std::string>& errors) {
    if (errors.empty()) return;

    LOG_ERROR("Failed to load stage '{}'", stageFile);
    for (const auto& error : errors) {
        LOG_WARN("  {}", error);
    }
}

// Build a map: display-name -> EnemyTemplate (reads all *.json in data/enemy/)
// File names are the enemy_id (B2, 01, 02, etc.). The "id" field inside JSON
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
        } catch (const std::exception& e) {
            LOG_WARN("Skipping enemy definition '{}': {}", entry.path().string(), e.what());
        }
    }
    return reg;
}

} // namespace

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

// Load all operator definitions from data/operators/
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
            t.magicResistance = j.value("magicResistance", 0);
            t.className       = j.value("class", std::string{});
            if (j.contains("skill") && j["skill"].is_object()) {
                const auto& skill = j["skill"];
                t.skillName = skill.value("name", std::string{});
                t.skillDescription = skill.value("description", std::string{});
            }
            const std::string dt = j.value("deployType","ground");
            t.deployType = (dt == "highground") ? DeployType::HIGHGROUND_ONLY : DeployType::GROUND_ONLY;
            auto it = OP_COLORS.find(t.id);
            t.color = (it != OP_COLORS.end()) ? it->second : IM_COL32(200,200,200,255);
            if (!t.id.empty()) result.push_back(t);
        } catch (const std::exception& e) {
            LOG_WARN("Skipping operator definition '{}': {}", p.string(), e.what());
        }
    }
    return result;
}

std::vector<OperatorTemplate> LoadOperatorsWithFallback() {
    auto operators = LoadOperators();
    if (!operators.empty()) {
        return operators;
    }

    auto makeOperator = [](std::string id,
                           std::string name,
                           int cost,
                           float hp,
                           float damage,
                           float def,
                           float attackIntervalMs,
                           int blockCount,
                           float maxSp,
                           float initialSp,
                           float skillDuration,
                           ImU32 color,
                           DeployType deployType,
                           bool isVanguard) {
        OperatorTemplate op;
        op.id = std::move(id);
        op.name = std::move(name);
        op.cost = cost;
        op.hp = hp;
        op.damage = damage;
        op.def = def;
        op.attackIntervalMs = attackIntervalMs;
        op.blockCount = blockCount;
        op.maxSp = maxSp;
        op.initialSp = initialSp;
        op.skillDuration = skillDuration;
        op.color = color;
        op.deployType = deployType;
        op.isVanguard = isVanguard;
        return op;
    };

    return {
        makeOperator("Bagpipe", "風笛", 11, 2664.0F, 769.0F, 382.0F, 1000.0F, 1, 4.0F, 2.0F, 1000.0F,
                     IM_COL32(225,120,80,255), DeployType::GROUND_ONLY, true),
        makeOperator("Sniper", "Sniper", 11, 1200.0F, 500.0F, 150.0F, 1000.0F, 0, 0.0F, 0.0F, 0.0F,
                     IM_COL32(255,196,66,255), DeployType::HIGHGROUND_ONLY, false),
        makeOperator("Myrtle", "桃金娘", 8, 1654.0F, 508.0F, 400.0F, 1000.0F, 1, 22.0F, 13.0F, 8000.0F,
                     IM_COL32(255,215,0,255), DeployType::GROUND_ONLY, true),
    };
}

// Load a full stage from JSON, wiring in enemies from the data/enemy directory
StageLoadResult LoadStageFromJsonDetailed(const std::string& stageFile) {
    StageLoadResult result;
    auto fail = [&](std::string error) {
        result.errors.push_back(std::move(error));
        LogStageLoadErrors(stageFile, result.errors);
        return result;
    };

    const auto stagePath = ResolveStagePath(stageFile);
    if (!stagePath.has_value()) {
        return fail("stage file not found in data/levels search paths");
    }

    try {
        std::ifstream file(*stagePath);
        if (!file.is_open()) {
            return fail("unable to open " + stagePath->string());
        }

        nlohmann::json stage;
        file >> stage;
        if (!stage.is_object()) {
            return fail("stage root must be a JSON object: " + stagePath->string());
        }

        const int W = stage.value("width",  0);
        const int H = stage.value("height", 0);
        if (W <= 0 || H <= 0) {
            return fail("stage width and height must be positive");
        }
        if (!stage.contains("tiles") || !stage["tiles"].is_array() ||
            static_cast<int>(stage["tiles"].size()) != H) {
            return fail("tiles must be an array with exactly height rows");
        }

        StageData data;
        data.name       = stage.value("name", "unnamed_stage");
        data.sourceFile = stagePath->lexically_normal().string();
        data.width  = W;
        data.height = H;
        if (stage.contains("camera") && stage["camera"].is_object()) {
            const auto& cam = stage["camera"];
            data.camera.projectionScaleX = cam.value("projection_scale_x", data.camera.projectionScaleX);
            data.camera.projectionScaleY = cam.value("projection_scale_y", data.camera.projectionScaleY);
            data.camera.projectionSkewX = cam.value("projection_skew_x", data.camera.projectionSkewX);
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
        bool seedBoardArtCellsFromCorners = false;
        if (stage.contains("board_art") && stage["board_art"].is_object()) {
            const auto& boardArt = stage["board_art"];
            auto readPoint = [&](const char* key) -> std::optional<glm::vec2> {
                if (!boardArt.contains(key) || !boardArt[key].is_array() || boardArt[key].size() != 2) {
                    return std::nullopt;
                }
                const auto& point = boardArt[key];
                if (!point[0].is_number() || !point[1].is_number()) {
                    return std::nullopt;
                }
                return glm::vec2{point[0].get<float>(), point[1].get<float>()};
            };
            auto readReferenceSize = [&]() -> glm::vec2 {
                if (!boardArt.contains("reference_size") || !boardArt["reference_size"].is_array() ||
                    boardArt["reference_size"].size() != 2) {
                    return {0.0F, 0.0F};
                }
                const auto& size = boardArt["reference_size"];
                if (!size[0].is_number() || !size[1].is_number()) {
                    return {0.0F, 0.0F};
                }
                return {std::max(0.0F, size[0].get<float>()), std::max(0.0F, size[1].get<float>())};
            };

            const auto topLeft = readPoint("top_left");
            const auto topRight = readPoint("top_right");
            const auto bottomRight = readPoint("bottom_right");
            const auto bottomLeft = readPoint("bottom_left");
            const auto referenceSize = readReferenceSize();
            const bool hasCellOverrides = boardArt.contains("cells") && boardArt["cells"].is_array();
            if (topLeft && topRight && bottomRight && bottomLeft) {
                data.boardArt.enabled = true;
                data.boardArt.corners = {*topLeft, *topRight, *bottomRight, *bottomLeft};
                data.boardArt.referenceSize = referenceSize;
                seedBoardArtCellsFromCorners = true;
            } else if (hasCellOverrides && referenceSize.x > 0.0F && referenceSize.y > 0.0F) {
                data.boardArt.enabled = true;
                data.boardArt.corners = {
                    glm::vec2{0.0F, 0.0F},
                    glm::vec2{referenceSize.x, 0.0F},
                    glm::vec2{referenceSize.x, referenceSize.y},
                    glm::vec2{0.0F, referenceSize.y}
                };
                data.boardArt.referenceSize = referenceSize;
            } else if (hasCellOverrides) {
                result.errors.push_back("board_art cells require reference_size when top-level corners are omitted");
            } else {
                result.errors.push_back("board_art requires top_left, top_right, bottom_right, and bottom_left point arrays");
            }
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
        data.tileImages.assign(static_cast<std::size_t>(H),
                               std::vector<std::string>(static_cast<std::size_t>(W)));

        for (int y = 0; y < H; ++y) {
            const auto& row = stage["tiles"][static_cast<std::size_t>(y)];
            if (!row.is_array() || static_cast<int>(row.size()) != W) {
                return fail("tiles[" + std::to_string(y) + "] must be an array with exactly width cells");
            }
            for (int x = 0; x < W; ++x) {
                const auto& cell = row[static_cast<std::size_t>(x)];
                if (cell.is_string()) {
                    data.tileMap[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                        ParseTile(cell.get<std::string>());
                } else if (cell.is_object()) {
                    data.tileMap[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                        ParseTile(cell.value("type", std::string{"empty"}));
                    const auto imagePath = ResolveAssetPath(*stagePath, cell.value("image", std::string{}));
                    if (!imagePath.empty()) {
                        data.tileImages[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = imagePath.string();
                    }
                } else {
                    return fail("tile at (" + std::to_string(x) + "," + std::to_string(y) +
                                ") must be a string or object");
                }
            }
        }
        if (stage.contains("tile_images") && stage["tile_images"].is_array() &&
            static_cast<int>(stage["tile_images"].size()) == H) {
            for (int y = 0; y < H; ++y) {
                const auto& row = stage["tile_images"][static_cast<std::size_t>(y)];
                if (!row.is_array() || static_cast<int>(row.size()) != W) continue;
                for (int x = 0; x < W; ++x) {
                    const auto& cell = row[static_cast<std::size_t>(x)];
                    if (!cell.is_string()) continue;
                    const auto imagePath = ResolveAssetPath(*stagePath, cell.get<std::string>());
                    if (!imagePath.empty()) {
                        data.tileImages[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = imagePath.string();
                    }
                }
            }
        }
        if (data.boardArt.enabled) {
            auto boardArtPoint = [&](float x, float y) {
                const float u = std::clamp(x / static_cast<float>(std::max(1, W)), 0.0F, 1.0F);
                const float v = std::clamp(y / static_cast<float>(std::max(1, H)), 0.0F, 1.0F);
                const glm::vec2 top = data.boardArt.corners[0] + (data.boardArt.corners[1] - data.boardArt.corners[0]) * u;
                const glm::vec2 bottom = data.boardArt.corners[3] + (data.boardArt.corners[2] - data.boardArt.corners[3]) * u;
                return top + (bottom - top) * v;
            };
            data.boardArt.cells.assign(static_cast<std::size_t>(H),
                                       std::vector<BoardArtCell>(static_cast<std::size_t>(W)));
            if (seedBoardArtCellsFromCorners) {
                for (int y = 0; y < H; ++y) {
                    for (int x = 0; x < W; ++x) {
                        if (data.tileMap[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] ==
                            TileType::EMPTY) {
                            continue;
                        }
                        auto& cell = data.boardArt.cells[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
                        cell.enabled = true;
                        cell.corners = {
                            boardArtPoint(static_cast<float>(x), static_cast<float>(y)),
                            boardArtPoint(static_cast<float>(x + 1), static_cast<float>(y)),
                            boardArtPoint(static_cast<float>(x + 1), static_cast<float>(y + 1)),
                            boardArtPoint(static_cast<float>(x), static_cast<float>(y + 1))
                        };
                    }
                }
            }

            const auto& boardArt = stage["board_art"];
            if (boardArt.contains("cells") && boardArt["cells"].is_array()) {
                auto readPoint = [](const nlohmann::json& owner, const char* key) -> std::optional<glm::vec2> {
                    if (!owner.contains(key) || !owner[key].is_array() || owner[key].size() != 2) {
                        return std::nullopt;
                    }
                    const auto& point = owner[key];
                    if (!point[0].is_number() || !point[1].is_number()) {
                        return std::nullopt;
                    }
                    return glm::vec2{point[0].get<float>(), point[1].get<float>()};
                };
                for (const auto& overrideCell : boardArt["cells"]) {
                    if (!overrideCell.is_object()) continue;
                    const int x = overrideCell.value("x", -1);
                    const int y = overrideCell.value("y", -1);
                    if (x < 0 || x >= W || y < 0 || y >= H) continue;
                    if (data.tileMap[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] ==
                        TileType::EMPTY) {
                        continue;
                    }
                    const auto topLeft = readPoint(overrideCell, "top_left");
                    const auto topRight = readPoint(overrideCell, "top_right");
                    const auto bottomRight = readPoint(overrideCell, "bottom_right");
                    const auto bottomLeft = readPoint(overrideCell, "bottom_left");
                    if (!(topLeft && topRight && bottomRight && bottomLeft)) continue;
                    auto& cell = data.boardArt.cells[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
                    cell.enabled = true;
                    cell.corners = {*topLeft, *topRight, *bottomRight, *bottomLeft};
                }
            }
        }

        // Routes
        if (!stage.contains("routes") || !stage["routes"].is_object()) {
            return fail("missing or invalid routes object");
        }
        auto boardCenter = [](int x, int y) -> glm::vec2 {
            return {static_cast<float>(x)+0.5F, static_cast<float>(y)+0.5F};
        };
        for (const auto& [routeId, nodes] : stage["routes"].items()) {
            if (!nodes.is_array() || nodes.empty()) {
                result.errors.push_back("route '" + routeId + "' must be a non-empty array");
                continue;
            }
            Route r; r.id = routeId;
            for (std::size_t index = 0; index < nodes.size(); ++index) {
                const auto& nd = nodes[index];
                if (!nd.is_object()) {
                    result.errors.push_back("route '" + routeId + "' node " +
                                            std::to_string(index) + " must be an object");
                    continue;
                }
                int nx = nd.value("x", -1), ny = nd.value("y", -1);
                if (nx < 0 || nx >= W || ny < 0 || ny >= H) {
                    result.errors.push_back("route '" + routeId + "' node " +
                                            std::to_string(index) + " is out of bounds");
                    continue;
                }
                std::string directionField;
                std::string invalidDirection;
                const bool hasDirectionField =
                    nd.contains("direction") || nd.contains("model") || nd.contains("facing");
                auto spriteDirection =
                    ReadRouteSpriteDirection(nd, directionField, invalidDirection);
                if (index == 0 && !hasDirectionField) {
                    result.errors.push_back("route '" + routeId +
                                            "' first node must include direction: normal or flip");
                } else if (hasDirectionField && !spriteDirection.has_value()) {
                    result.errors.push_back("route '" + routeId + "' node " +
                                            std::to_string(index) + " has invalid " +
                                            directionField + ": " + invalidDirection);
                }
                r.nodes.push_back(RouteNode{
                    boardCenter(nx, ny),
                    nd.value("wait", 0.0F),
                    spriteDirection.value_or(EnemySpriteDirection::INHERIT)
                });
            }
            if (r.nodes.size() >= 2) {
                data.routes.push_back(std::move(r));
            } else {
                result.errors.push_back("route '" + routeId + "' has fewer than two valid nodes");
            }
        }
        if (data.routes.empty()) {
            return fail("stage has no valid routes");
        }

        // Collect enemy display names from level "enemies" section
        std::vector<std::string> enemyNames;
        if (stage.contains("enemies") && stage["enemies"].is_object())
            for (const auto& [eid, _] : stage["enemies"].items())
                enemyNames.push_back(eid);
        if (enemyNames.empty()) {
            return fail("stage enemies object is missing or empty");
        }

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
        if (data.enemyTemplates.empty()) {
            return fail("stage has no enemy templates after loading enemies");
        }

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
        if (!stage.contains("waves") || !stage["waves"].is_array()) {
            return fail("missing or invalid waves array");
        }
        data.totalWaves = static_cast<int>(stage["waves"].size());
        for (std::size_t wi = 0; wi < stage["waves"].size(); ++wi) {
            const auto& wave = stage["waves"][wi];
            const std::string waveLabel = "waves[" + std::to_string(wi) + "]";
            if (!wave.is_object()) {
                result.errors.push_back(waveLabel + " must be an object");
                continue;
            }
            int   ei    = findEnemy(wave.value("enemy",""));
            int   ri    = findRoute(wave.value("route",""));
            int   cnt   = wave.value("count",    0);
            float start = wave.value("start",    0.0F);
            float inter = wave.value("interval", 0.0F);
            if (ei < 0 || ri < 0 || cnt <= 0) {
                result.errors.push_back(waveLabel + " references invalid enemy/route or has non-positive count");
                continue;
            }
            for (int u = 0; u < cnt; ++u)
                data.wavePlans.push_back(WavePlan{static_cast<int>(wi)+1, ei, ri, start+inter*u});
        }
        if (data.wavePlans.empty()) {
            return fail("stage has no valid wave plans");
        }

        std::sort(data.wavePlans.begin(), data.wavePlans.end(),
            [](const WavePlan& a, const WavePlan& b){
                if (a.spawnTimeSec == b.spawnTimeSec)
                    return std::tie(a.waveIndex,a.routeIndex,a.enemyTypeIndex) <
                           std::tie(b.waveIndex,b.routeIndex,b.enemyTypeIndex);
                return a.spawnTimeSec < b.spawnTimeSec;
            });
        if (!result.errors.empty()) {
            for (const auto& error : result.errors) {
                LOG_WARN("Stage '{}' loaded with warning: {}", stageFile, error);
            }
        }
        result.data = std::move(data);
        return result;
    } catch (const std::exception& e) {
        return fail(std::string("exception while loading stage: ") + e.what());
    }
}

std::optional<StageData> LoadStageFromJson(const std::string& stageFile) {
    return LoadStageFromJsonDetailed(stageFile).data;
}

} // namespace Ark
