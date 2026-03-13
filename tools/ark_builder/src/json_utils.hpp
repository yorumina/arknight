#pragma once

#include <filesystem>
#include <nlohmann/json.hpp>

namespace ark_builder {

using json = nlohmann::json;

auto LoadJson(const std::filesystem::path& path) -> json;
void SaveJson(const std::filesystem::path& path, const json& doc);

} // namespace ark_builder
