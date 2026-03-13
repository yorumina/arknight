#include "json_utils.hpp"

#include <fstream>
#include <iomanip>

#include "types.hpp"

namespace ark_builder {

auto LoadJson(const std::filesystem::path& path) -> json {
    std::ifstream input(path);
    if (!input) {
        throw CliError("unable to open file: " + path.string());
    }

    json doc;
    try {
        input >> doc;
    } catch (const json::parse_error& error) {
        throw CliError("invalid JSON in " + path.string() + ": " +
                       std::string(error.what()));
    }
    return doc;
}

void SaveJson(const std::filesystem::path& path, const json& doc) {
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream output(path);
    if (!output) {
        throw CliError("unable to write file: " + path.string());
    }
    output << std::setw(2) << doc << '\n';
}

} // namespace ark_builder
