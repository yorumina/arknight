#pragma once

#include <stdexcept>
#include <string>

namespace ark_builder {

class CliError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct SimulationRecord {
    int waveIndex{};
    int unitIndex{};
    std::string enemyId;
    std::string routeId;
    double spawnTime{};
    double goalTime{};
};

} // namespace ark_builder
