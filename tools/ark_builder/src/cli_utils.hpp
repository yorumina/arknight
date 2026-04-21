#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ark_builder {

auto Usage() -> std::string;

auto FindOption(const std::vector<std::string>& args,
                std::string_view flag) -> std::optional<std::string>;

auto ParseInt(const std::string& value, std::string_view name) -> int;
auto ParseDouble(const std::string& value, std::string_view name) -> double;

auto RequireIntOption(const std::vector<std::string>& args, std::string_view flag,
                      std::string_view name) -> int;
auto RequireDoubleOption(const std::vector<std::string>& args,
                         std::string_view flag,
                         std::string_view name) -> double;
auto RequireStringOption(const std::vector<std::string>& args,
                         std::string_view flag) -> std::string;

auto ResolveDataRoot() -> std::filesystem::path;
auto ResolveLevelFilePath(const std::string& userPath) -> std::filesystem::path;

} // namespace ark_builder
