#pragma once

#include "App.hpp"

class App;

namespace Ark {

class GameLogic {
public:
    GameLogic(App& app) : m_App(app) {}

    void InitializeStage();
    void UpdateGame(float dt);
    void StartWave();
    void ResetDemo();

    void UpdateWave(float deltaTimeSec);
    void UpdateEnemies(float deltaTimeSec);
    void UpdateOperators(float deltaTimeMs);
    void UpdateBeams(float deltaTimeMs);
    void SpawnEnemy(const WavePlan& plan);
    void CleanupDefeatedEnemies();

private:
    App& m_App;
};

} // namespace Ark
