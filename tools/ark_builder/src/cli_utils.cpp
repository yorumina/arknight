#include "cli_utils.hpp"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <vector>

#include "types.hpp"

namespace ark_builder {
namespace {

void EnsureDataFolders(const std::filesystem::path& dataRoot) {
    std::filesystem::create_directories(dataRoot / "enemy");
    std::filesystem::create_directories(dataRoot / "levels");
    std::filesystem::create_directories(dataRoot / "operators");
}

auto StripKnownLevelPrefix(const std::filesystem::path& raw) -> std::filesystem::path {
    std::vector<std::string> parts;
    parts.reserve(16);
    for (const auto& piece : raw) {
        const std::string token = piece.generic_string();
        if (token.empty() || token == "." || token == "/") continue;
        parts.push_back(token);
    }

    const std::array<std::vector<std::string>, 3> knownPrefixes{
        std::vector<std::string>{"tools", "ark_builder", "levels"},
        std::vector<std::string>{"data", "levels"},
        std::vector<std::string>{"levels"},
    };

    for (const auto& prefix : knownPrefixes) {
        if (parts.size() < prefix.size()) continue;
        for (std::size_t start = 0; start + prefix.size() <= parts.size(); ++start) {
            bool match = true;
            for (std::size_t i = 0; i < prefix.size(); ++i) {
                if (parts[start + i] != prefix[i]) {
                    match = false;
                    break;
                }
            }
            if (!match) continue;

            std::filesystem::path stripped;
            for (std::size_t i = start + prefix.size(); i < parts.size(); ++i) {
                if (parts[i] == "..") continue;
                stripped /= parts[i];
            }
            return stripped;
        }
    }

    if (raw.is_absolute()) {
        return raw.filename();
    }

    std::filesystem::path sanitized;
    for (const auto& piece : raw.lexically_normal()) {
        const std::string token = piece.generic_string();
        if (token.empty() || token == "." || token == ".." || token == "/") continue;
        sanitized /= token;
    }
    return sanitized;
}

} // namespace

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
  empty | road | ground | highground | unusablehighground | spawn | goal

Path behavior:
  All stage file arguments are automatically mapped to data/levels/.
  Old prefixes like tools/ark_builder/levels/ are accepted for compatibility.
)";
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

auto ResolveDataRoot() -> std::filesystem::path {
    if (const char* envRoot = std::getenv("ARKNIGHT_DATA_ROOT");
        envRoot != nullptr && envRoot[0] != '\0') {
        const std::filesystem::path fromEnv = std::filesystem::path(envRoot).lexically_normal();
        try {
            EnsureDataFolders(fromEnv);
            return fromEnv;
        } catch (const std::filesystem::filesystem_error& e) {
            throw CliError("failed to prepare ARKNIGHT_DATA_ROOT '" + fromEnv.string() + "': " + e.what());
        }
    }

    const std::array<std::filesystem::path, 4> candidates{
        std::filesystem::path("data"),
        std::filesystem::path("../data"),
        std::filesystem::path("../../data"),
        std::filesystem::path("../../../data"),
    };

    for (const auto& candidate : candidates) {
        if (!std::filesystem::exists(candidate) || !std::filesystem::is_directory(candidate)) continue;
        const auto resolved = candidate.lexically_normal();
        try {
            EnsureDataFolders(resolved);
            return resolved;
        } catch (const std::filesystem::filesystem_error& e) {
            throw CliError("failed to prepare data folders under '" + resolved.string() + "': " + e.what());
        }
    }

    const auto fallback = std::filesystem::path("data");
    try {
        EnsureDataFolders(fallback);
    } catch (const std::filesystem::filesystem_error& e) {
        throw CliError("failed to create fallback data folders under '" + fallback.string() + "': " + e.what());
    }
    return fallback.lexically_normal();
}

auto ResolveLevelFilePath(const std::string& userPath) -> std::filesystem::path {
    if (userPath.empty()) {
        throw CliError("stage file cannot be empty");
    }

    auto relative = StripKnownLevelPrefix(std::filesystem::path(userPath));
    if (relative.empty()) {
        throw CliError("invalid stage file path: " + userPath);
    }

    if (relative.extension().empty()) {
        relative += ".json";
    }

    return (ResolveDataRoot() / "levels" / relative).lexically_normal();
}

} // namespace ark_builder
