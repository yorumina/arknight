#pragma once

#include <string>
#include <vector>

namespace ark_builder {

void RunNew(const std::vector<std::string>& args);
void RunPaint(const std::vector<std::string>& args);
void RunRouteSet(const std::vector<std::string>& args);
void RunEnemySet(const std::vector<std::string>& args);
void RunSpawnAdd(const std::vector<std::string>& args);
void RunValidate(const std::vector<std::string>& args);
void RunSimulate(const std::vector<std::string>& args);
void RunShow(const std::vector<std::string>& args);
void RunCalibrate(const std::vector<std::string>& args);
void RunMenuCalibrate(const std::vector<std::string>& args);
void RunOpeningCalibrate(const std::vector<std::string>& args);

} // namespace ark_builder
