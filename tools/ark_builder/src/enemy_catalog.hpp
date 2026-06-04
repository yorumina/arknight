#pragma once

#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <nlohmann/json.hpp>

namespace ark_builder {

using json = nlohmann::json;
using EnemyAliasMap = std::unordered_map<std::string, std::unordered_set<std::string>>;

inline auto ResolveEnemyDir() -> std::filesystem::path {
    const std::filesystem::path candidates[]{
        std::filesystem::path("data/enemy"),
        std::filesystem::path("../data/enemy"),
        std::filesystem::path("../../data/enemy"),
        std::filesystem::path("../../../data/enemy"),
    };

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate) &&
            std::filesystem::is_directory(candidate)) {
            return candidate.lexically_normal();
        }
    }
    return {};
}

inline void AddEnemyAlias(EnemyAliasMap& aliases,
                          const std::string& canonical,
                          const std::string& alias) {
    if (canonical.empty() || alias.empty()) return;
    aliases[canonical].insert(alias);
}

inline auto LoadEnemyAliases() -> EnemyAliasMap {
    EnemyAliasMap aliases;
    const auto enemyDir = ResolveEnemyDir();
    if (enemyDir.empty()) return aliases;

    for (const auto& entry : std::filesystem::directory_iterator(enemyDir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;

        try {
            std::ifstream file(entry.path());
            json enemy;
            file >> enemy;
            const std::string fallbackId = entry.path().stem().string();
            const std::string displayId = enemy.value("id", fallbackId);
            const std::string enemyId = enemy.value("enemy_id", fallbackId);

            AddEnemyAlias(aliases, displayId, displayId);
            AddEnemyAlias(aliases, displayId, enemyId);
            AddEnemyAlias(aliases, enemyId, displayId);
            AddEnemyAlias(aliases, enemyId, enemyId);
        } catch (const std::exception&) {
        }
    }

    return aliases;
}

inline void InsertEnemyAliases(std::unordered_set<std::string>& ids,
                               const EnemyAliasMap& aliases,
                               const std::string& id) {
    if (id.empty()) return;
    ids.insert(id);
    if (const auto it = aliases.find(id); it != aliases.end()) {
        ids.insert(it->second.begin(), it->second.end());
    }
}

inline auto BuildRuntimeEnemyIds(const json& stage) -> std::unordered_set<std::string> {
    std::unordered_set<std::string> ids;
    if (!stage.contains("enemies") || !stage["enemies"].is_object()) {
        return ids;
    }

    const auto aliases = LoadEnemyAliases();
    for (auto it = stage["enemies"].begin(); it != stage["enemies"].end(); ++it) {
        InsertEnemyAliases(ids, aliases, it.key());

        const auto& enemy = it.value();
        if (enemy.is_object() && enemy.contains("enemy_id") && enemy["enemy_id"].is_string()) {
            InsertEnemyAliases(ids, aliases, enemy["enemy_id"].get<std::string>());
        }
    }

    return ids;
}

inline auto FindStageEnemyByRuntimeId(const json& stage,
                                      const EnemyAliasMap& aliases,
                                      const std::string& enemyId) -> const json* {
    if (!stage.contains("enemies") || !stage["enemies"].is_object()) {
        return nullptr;
    }

    const auto& enemies = stage["enemies"];
    auto findByKey = [&](const std::string& key) -> const json* {
        if (!key.empty() && enemies.contains(key)) {
            return &enemies.at(key);
        }
        return nullptr;
    };

    if (const json* exact = findByKey(enemyId)) {
        return exact;
    }
    if (const auto aliasIt = aliases.find(enemyId); aliasIt != aliases.end()) {
        for (const auto& alias : aliasIt->second) {
            if (const json* aliased = findByKey(alias)) {
                return aliased;
            }
        }
    }

    for (auto it = enemies.begin(); it != enemies.end(); ++it) {
        const auto& enemy = it.value();
        if (!enemy.is_object()) continue;

        if (enemy.contains("enemy_id") && enemy["enemy_id"].is_string()) {
            const auto declaredEnemyId = enemy["enemy_id"].get<std::string>();
            if (declaredEnemyId == enemyId) {
                return &enemy;
            }
            if (const auto aliasIt = aliases.find(enemyId); aliasIt != aliases.end() &&
                aliasIt->second.find(declaredEnemyId) != aliasIt->second.end()) {
                return &enemy;
            }
        }
    }

    return nullptr;
}

inline auto FindStageEnemyByRuntimeId(const json& stage,
                                      const std::string& enemyId) -> const json* {
    return FindStageEnemyByRuntimeId(stage, LoadEnemyAliases(), enemyId);
}

} // namespace ark_builder
