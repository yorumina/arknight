#include <iostream>
#include <string>
#include <vector>

#include "cli_utils.hpp"
#include "commands.hpp"
#include "types.hpp"

auto main(int argc, char** argv) -> int {
    try {
        std::vector<std::string> args;
        for (int i = 1; i < argc; ++i) {
            args.emplace_back(argv[i]);
        }

        if (args.empty() || args[0] == "--help" || args[0] == "-h") {
            std::cout << ark_builder::Usage();
            return 0;
        }

        const auto& command = args[0];
        if (command == "new") {
            ark_builder::RunNew(args);
        } else if (command == "paint") {
            ark_builder::RunPaint(args);
        } else if (command == "route-set") {
            ark_builder::RunRouteSet(args);
        } else if (command == "enemy-set") {
            ark_builder::RunEnemySet(args);
        } else if (command == "spawn-add") {
            ark_builder::RunSpawnAdd(args);
        } else if (command == "validate") {
            ark_builder::RunValidate(args);
        } else if (command == "simulate") {
            ark_builder::RunSimulate(args);
        } else if (command == "show") {
            ark_builder::RunShow(args);
        } else if (command == "calibrate") {
            ark_builder::RunCalibrate(args);
        } else if (command == "menu-calibrate") {
            ark_builder::RunMenuCalibrate(args);
        } else if (command == "opening-calibrate") {
            ark_builder::RunOpeningCalibrate(args);
        } else {
            throw ark_builder::CliError("unknown command: " + command);
        }

        return 0;
    } catch (const ark_builder::CliError& error) {
        std::cerr << "error: " << error.what() << "\n\n" << ark_builder::Usage();
    } catch (const std::exception& error) {
        std::cerr << "fatal: " << error.what() << '\n';
    }
    return 1;
}
