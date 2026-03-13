#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace ark_builder {

using json = nlohmann::json;

auto IsKnownTile(const std::string& tile) -> bool;
auto IsRouteTile(const std::string& tile) -> bool;

auto BuildBlankStage(const std::string& name, int width, int height) -> json;
auto IsTileInBounds(int x, int y, int width, int height) -> bool;
auto GetTile(const json& stage, int x, int y) -> std::string;
void SetTile(json& stage, int x, int y, const std::string& tile);

} // namespace ark_builder
