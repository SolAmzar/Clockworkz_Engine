/******************************************************************************/
/*!
\file           Prefab.hpp
\project        Lights Out
\author(s)      [Amirul Syafiq], 100%
\brief          Defines GameObjectBlueprint and PrefabFactory for creating
                reusable, configurable GameObjects.

GameObjectBlueprint stores type, texture, transform,
                collision, and basic gameplay attributes (HP, damage, etc.).
PrefabFactory saves and retrieves blueprints, and instantiates
              new GameObjects with predefined properties via create() or
              createAt().
Provides setupGameBlueprints() to register standard prefabs.

                Supports a "recipe book" system to easily spawn consistent
                objects in levels.

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "Gameplay/Prefab.hpp"
#include "Gameplay/GameObject.hpp"
#include "Gameplay/Serializer.hpp"
#include "Gameplay/Components/Health.hpp"
#include "Gameplay/Components/DamageDealer.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>

namespace Gameplay
{
    PrefabFactory &PrefabFactory::get()
    {
        static PrefabFactory instance;
        return instance;
    }

    // ========================================================================
    // PREFAB FILE I/O
    // ========================================================================

    static GameObjectType getTypeFromPrefabName(const std::string &name)
    {
        if (name == "Player")
            return GameObjectType::Player;
        if (name == "Enemy")
            return GameObjectType::Enemy;
        if (name == "Wall"
            || name == "WallCornerTL"
            || name == "WallCornerTR"
            || name == "WallCornerBL"
            || name == "WallCornerBR"
            || name == "WallEdgeTop"
            || name == "WallEdgeBottom"
            || name == "WallEdgeLeft"
            || name == "WallEdgeRight"
            || name == "WallCenter")
            return GameObjectType::Wall;
        if (name == "Floor")
            return GameObjectType::Floor;
        if (name == "Trap")
            return GameObjectType::Trap;
        if (name == "Ball")
			return GameObjectType::Ball;
		if (name == "Box")
			return GameObjectType::Box;
        return GameObjectType::Unknown;
    }

    bool PrefabFactory::loadPrefabFromFile(const std::string &filepath)
    {
        try
        {
            std::ifstream file(filepath);
            if (!file.is_open())
            {
                std::cerr << "[PrefabFactory] Failed to open prefab file: " << filepath << "\n";
                return false;
            }

            json j;
            file >> j;
            file.close();

            // Support two formats:
            // Format 1 (Single prefab):
            // {
            //   "prefabName": "PlayerPrefab",
            //   "components": { ... }
            // }
            //
            // Format 2 (Multiple prefabs):
            // {
            //   "prefabs": [
            //     { "prefabName": "Player", "components": {...} },
            //     { "prefabName": "Enemy", "components": {...} }
            //   ]
            // }

            int loadedCount = 0;

            // Check if it's the multiple prefabs format
            if (j.contains("prefabs") && j["prefabs"].is_array())
            {
                for (const auto &prefabJson : j["prefabs"])
                {
                    if (!prefabJson.contains("prefabName") || !prefabJson.contains("components"))
                    {
                        std::cerr << "[PrefabFactory] Invalid prefab entry in: " << filepath << "\n";
                        continue;
                    }

                    std::string prefabName = prefabJson["prefabName"].get<std::string>();
                    registerPrefab(prefabName, prefabJson["components"]);
                    loadedCount++;
                }

                std::cout << "[PrefabFactory] Loaded " << loadedCount << " prefabs from: " << filepath << "\n";
                return loadedCount > 0;
            }
            // Single prefab format
            else if (j.contains("prefabName") && j.contains("components"))
            {
                std::string prefabName = j["prefabName"].get<std::string>();
                registerPrefab(prefabName, j["components"]);

                std::cout << "[PrefabFactory] Loaded prefab '" << prefabName << "' from: " << filepath << "\n";
                return true;
            }
            else
            {
                std::cerr << "[PrefabFactory] Invalid prefab format in: " << filepath << "\n";
                return false;
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "[PrefabFactory] Error loading prefab: " << e.what() << "\n";
            return false;
        }
    }

    bool PrefabFactory::savePrefabToFile(const std::string &prefabName, const std::string &filepath)
    {
        auto it = prefabs_.find(prefabName);
        if (it == prefabs_.end())
        {
            std::cerr << "[PrefabFactory] Prefab '" << prefabName << "' not found\n";
            return false;
        }

        try
        {
            json j;
            j["prefabName"] = prefabName;
            j["components"] = it->second.componentData;

            std::ofstream file(filepath);
            if (!file.is_open())
            {
                std::cerr << "[PrefabFactory] Failed to open file for writing: " << filepath << "\n";
                return false;
            }

            file << j.dump(4);
            file.close();

            std::cout << "[PrefabFactory] Saved prefab '" << prefabName << "' to: " << filepath << "\n";
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[PrefabFactory] Error saving prefab: " << e.what() << "\n";
            return false;
        }
    }

    bool PrefabFactory::loadAllPrefabsFromDirectory(const std::string &directory)
    {
        namespace fs = std::filesystem;

        if (!fs::exists(directory) || !fs::is_directory(directory))
        {
            std::cerr << "[PrefabFactory] Directory not found: " << directory << "\n";
            return false;
        }

        int loadedCount = 0;
        for (const auto &entry : fs::directory_iterator(directory))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".json")
            {
                if (loadPrefabFromFile(entry.path().string()))
                {
                    loadedCount++;
                }
            }
        }

        std::cout << "[PrefabFactory] Loaded " << loadedCount << " prefabs from: " << directory << "\n";
        return loadedCount > 0;
    }

    // ========================================================================
    // PREFAB REGISTRATION
    // ========================================================================

    void PrefabFactory::registerPrefab(const std::string &name, const json &componentData)
    {
        PrefabData prefab;
        prefab.name = name;
        prefab.componentData = componentData;
        prefabs_[name] = prefab;

        std::cout << "[PrefabFactory] Registered prefab: " << name << "\n";
    }

    // ========================================================================
    // GAMEOBJECT CREATION
    // ========================================================================

    std::unique_ptr<GameObject> PrefabFactory::create(const std::string &prefabName)
    {
        auto it = prefabs_.find(prefabName);
        if (it == prefabs_.end())
        {
            std::cerr << "[PrefabFactory] Prefab not found: " << prefabName << "\n";
            return nullptr;
        }

        // Create a temporary JSON object with the prefab data
        json objJson;
        objJson["name"] = prefabName;
        objJson["type"] = static_cast<int>(getTypeFromPrefabName(prefabName));
        objJson["active"] = true;
        objJson["components"] = it->second.componentData;


        // Use the serializer to create the GameObject
        auto obj = GameObjectSerializer::deserializeGameObject(objJson);
        if (obj) {
            obj->setPrefabName(prefabName);  // Remember which prefab this came from
        }
        return obj;
    }

    std::unique_ptr<GameObject> PrefabFactory::createAt(const std::string& prefabName, vec2 position)
    {
        auto obj = create(prefabName);
        if (obj)
        {
            obj->transform().position = position;
            obj->setPrefabName(prefabName);
        }
        return obj;
    }

    std::unique_ptr<GameObject> PrefabFactory::createWithOverrides(
        const std::string &prefabName,
        const json &overrides)
    {
        auto it = prefabs_.find(prefabName);
        if (it == prefabs_.end())
        {
            std::cerr << "[PrefabFactory] Prefab not found: " << prefabName << "\n";
            return nullptr;
        }

        // Start with prefab data
        json objJson;
        objJson["name"] = prefabName;
        objJson["type"] = static_cast<int>(getTypeFromPrefabName(prefabName));
        objJson["active"] = true;
        objJson["components"] = it->second.componentData;

        // Apply overrides (merge with prefab data)
        if (overrides.contains("name"))
        {
            objJson["name"] = overrides["name"];
        }
        if (overrides.contains("active"))
        {
            objJson["active"] = overrides["active"];
        }
        if (overrides.contains("components"))
        {
            // Merge component overrides
            for (auto &[key, value] : overrides["components"].items())
            {
                objJson["components"][key] = value;
            }
        }
        if (overrides.contains("patrol")) {
            objJson["components"]["patrol"] = {
            {"a", {{"x", 100.f}, {"y", 400.f}}},
            {"b", {{"x", 300.f}, {"y", 400.f}}},
            {"speed", 150.f}
            };
        }

        auto obj = GameObjectSerializer::deserializeGameObject(objJson);
        if (obj) {
            obj->setPrefabName(prefabName);  // Add this line
        }
        return obj;
    }

    // ========================================================================
    // UTILITY
    // ========================================================================

    bool PrefabFactory::hasPrefab(const std::string &name) const
    {
        return prefabs_.find(name) != prefabs_.end();
    }

    std::vector<std::string> PrefabFactory::getPrefabNames() const
    {
        std::vector<std::string> names;
        for (const auto &[name, _] : prefabs_)
        {
            names.push_back(name);
        }
        return names;
    }

    // ========================================================================
    // SETUP BUILT-IN PREFABS (Optional - for backwards compatibility)
    // ========================================================================

    void setupGameBlueprints()
    {
        auto &factory = PrefabFactory::get();

        // Player Prefab
        {
            json components;

            components["transform"]["position"]["x"] = 0.0f;
            components["transform"]["position"]["y"] = 0.0f;
            components["transform"]["rotation"] = 0.0f;
            components["transform"]["scale"]["x"] = 100.0f;
            components["transform"]["scale"]["y"] = 100.0f;

            components["collider"]["width"] = 0.8f;
            components["collider"]["height"] = 0.8f;
            components["collider"]["isSolid"] = true;
            components["collider"]["layer"] = 2;
            components["collider"]["collisionMask"] = 0xFFFFFFFFu;

            components["health"]["hp"] = 10;

            components["spriteRenderer"]["textureId"] = "player_texture";

            factory.registerPrefab("Player", components);
        }

        // Enemy Prefab
        {
            json components;

            components["transform"]["position"]["x"] = 0.0f;
            components["transform"]["position"]["y"] = 0.0f;
            components["transform"]["rotation"] = 0.0f;
            components["transform"]["scale"]["x"] = 100.0f;
            components["transform"]["scale"]["y"] = 100.0f;

            components["collider"]["width"] = 0.9f;
            components["collider"]["height"] = 0.9f;
            components["collider"]["isSolid"] = true;
            components["collider"]["layer"] = 2;
            components["collider"]["collisionMask"] = 0xFFFFFFFFu;

            components["health"]["hp"] = 3;
            components["damage"]["damage"] = 2;

            components["spriteRenderer"]["textureId"] = "enemy_texture";

            factory.registerPrefab("Enemy", components);
        }

        // Wall Prefab
        {
            json components;

            components["transform"]["position"]["x"] = 0.0f;
            components["transform"]["position"]["y"] = 0.0f;
            components["transform"]["rotation"] = 0.0f;
            components["transform"]["scale"]["x"] = 1.0f;
            components["transform"]["scale"]["y"] = 1.0f;

            components["collider"]["width"] = 1.0f;
            components["collider"]["height"] = 1.0f;
            components["collider"]["isSolid"] = true;
            components["collider"]["layer"] = 1;
            components["collider"]["collisionMask"] = 0xFFFFFFFFu;

            components["spriteRenderer"]["textureId"] = "wall_texture";

            factory.registerPrefab("Wall", components);
        }

        // Floor Prefab
        {
            json components;

            components["transform"]["position"]["x"] = 0.0f;
            components["transform"]["position"]["y"] = 0.0f;
            components["transform"]["rotation"] = 0.0f;
            components["transform"]["scale"]["x"] = 1.0f;
            components["transform"]["scale"]["y"] = 1.0f;

            components["collider"]["width"] = 1.0f;
            components["collider"]["height"] = 1.0f;
            components["collider"]["isSolid"] = false;
            components["collider"]["layer"] = 0;
            components["collider"]["collisionMask"] = 0xFFFFFFFFu;

            components["spriteRenderer"]["textureId"] = "floor_texture";

            factory.registerPrefab("Floor", components);
        }

        std::cout << "[PrefabFactory] Built-in prefabs registered\n";
    }

} // namespace Gameplay