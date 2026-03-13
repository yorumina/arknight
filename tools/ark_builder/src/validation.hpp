#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace ark_builder {

using json = nlohmann::json;

void ValidateRootSchema(const json& stage, std::vector<std::string>& errors);
void ValidateTiles(const json& stage, std::vector<std::string>& errors);
void ValidateRoutes(const json& stage, std::vector<std::string>& errors);
void ValidateEnemies(const json& stage, std::vector<std::string>& errors);
void ValidateWaves(const json& stage, std::vector<std::string>& errors);

auto ValidateStage(const json& stage) -> std::vector<std::string>;
void PrintValidationResult(const std::vector<std::string>& errors);

} // namespace ark_builder
