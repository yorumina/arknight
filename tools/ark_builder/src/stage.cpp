#include "stage.hpp"

#include <algorithm>
#include <vector>

#include "types.hpp"

namespace ark_builder {

auto IsKnownTile(const std::string& tile) -> bool {
    static const std::vector<std::string> kAllowedTiles{
        "empty", "road", "ground", "highground", "unusablehighground", "spawn", "goal",
    };
    return std::find(kAllowedTiles.begin(), kAllowedTiles.end(), tile) !=
           kAllowedTiles.end();
}

auto IsRouteTile(const std::string& tile) -> bool {
    return tile == "ground" || tile == "road" || tile == "spawn" || tile == "goal";
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
    const auto& cell =
        stage.at("tiles").at(static_cast<size_t>(y)).at(static_cast<size_t>(x));
    if (cell.is_object()) {
        return cell.value("type", std::string{"empty"});
    }
    return cell.get<std::string>();
}

void SetTile(json& stage, int x, int y, const std::string& tile) {
    stage["tiles"][static_cast<size_t>(y)][static_cast<size_t>(x)] = tile;
}

} // namespace ark_builder
