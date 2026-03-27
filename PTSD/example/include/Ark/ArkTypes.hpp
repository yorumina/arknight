#pragma once
#include "pch.hpp"
#include <string>
#include <vector>

namespace Ark {

// ── Tile / Deploy ───────────────────────────────────────────
enum class TileType { EMPTY = 0, ROAD, GROUND, HIGHGROUND, SPAWN, GOAL };
enum class DeployType { GROUND_ONLY = 0, HIGHGROUND_ONLY };

// ── Stage data ───────────────────────────────────────────────
struct RouteNode { glm::vec2 boardPos{0,0}; float waitSec = 0; };
struct Route { std::string id; std::vector<RouteNode> nodes; };

// ── Enemy ────────────────────────────────────────────────────
struct EnemyTemplate {
    std::string id;        // display name (e.g. "獵狗")
    std::string enemyId;   // short code   (e.g. "01")
    float hp = 0; float speed = 0;
    float damage = 200; float attackIntervalMs = 2000;
    float def = 0;          // enemy's own defense (reduces operator damage)
    float attackRange = 0;  // > 0 = ranged; attacks operators in this radius
    bool  isRanged = false; // true = can attack operators without being blocked
    bool canAttackOperator = true;
    ImU32 color = IM_COL32(220, 87, 92, 255);
};

struct Enemy {
    int id = 0;
    glm::vec2 boardPos{0,0};
    int routeIndex = 0;
    std::size_t nodeIndex = 0;
    float waitSec = 0;
    float hp = 0; float maxHp = 0;
    float speed = 0;
    float def = 0;           // reduces operator damage
    float damage = 0; float attackIntervalMs = 0;
    float attackCooldownMs = 0;
    float attackRange = 0;   // >0 = ranged attacker radius in tiles
    bool  isRanged = false;
    int blockedByOperatorId = -1;
    bool canAttackOperator = true;
    ImU32 color = IM_COL32(220, 87, 92, 255);
    bool alive = true;
};

// ── Operator ──────────────────────────────────────────────────
struct OperatorTemplate {
    std::string id;
    std::string name;
    int cost = 0;
    float hp = 0; float damage = 0; float def = 0;
    float attackIntervalMs = 0;
    int blockCount = 0;
    float maxSp = 0; float initialSp = 0; float skillDuration = 0;
    ImU32 color = IM_COL32_WHITE;
    DeployType deployType = DeployType::GROUND_ONLY;
    bool isVanguard = false;
};

struct Operator {
    int id = 0;
    int typeIndex = 0;
    glm::ivec2 cell{0, 0};
    glm::ivec2 direction{1, 0};
    float cooldownMs = 0;
    float hp = 0; float maxHp = 0; float def = 0;
    int blockedEnemyCount = 0;
    float sp = 0;
    bool skillActive = false;
    float skillTimerMs = 0;
};

// ── Misc ──────────────────────────────────────────────────────
struct WavePlan { int waveIndex=0; int enemyTypeIndex=0; int routeIndex=0; float spawnTimeSec=0; };
struct AttackBeam { glm::vec2 from{0,0}; glm::vec2 to{0,0}; float ttlMs=0; };

struct BoardLayout {
    float cellSize = 60.0F;
    float topLeftX = -300.0F;
    float topLeftY =  200.0F;
};

} // namespace Ark
