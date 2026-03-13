#include "cli_utils.hpp"

#include "types.hpp"

namespace ark_builder {

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

} // namespace ark_builder
