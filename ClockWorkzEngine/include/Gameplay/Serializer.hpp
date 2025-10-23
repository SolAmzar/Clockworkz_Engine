#pragma once
#include <string>
#include <vector>
#include <memory>
#include <glm/vec2.hpp>

/******************************************************************************/
/*!
\file           Serializer.hpp
\project        Lights Out
\author(s)      [Yusuf], 100%
\brief          Declares Serializer data structures and utility functions for
                converting GameObjects to/from JSON format.
                Responsibilities include:
                - Defining SerializedGameObject structure to store GameObject state
                - Defining SerializedScene structure to store complete scene data
                - Declaring GameObjectSerializer class with static serialization methods
                - Providing interfaces for serialize/deserialize operations
                - Declaring file I/O functions for scene persistence (save/load)
                - Declaring JSON conversion utilities (toJSON/fromJSON)
                - Supporting blueprint-based object recreation with property overrides

                This header provides the data structures and function signatures
                needed to persist game state to disk and restore it, enabling
                save/load functionality and level design workflows.

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

namespace Gameplay {
    class GameObject;
    struct GameObjectBlueprint;

    // Serialized representation of a GameObject instance
    struct SerializedGameObject {
        std::string blueprintName;  // Which prefab to use
        glm::vec2 position{ 0.0f, 0.0f };
        glm::vec2 scale{ 1.0f, 1.0f };
        float rotation{ 0.0f };
        bool active{ true };

        // Optional overrides (if different from blueprint defaults)
        int healthOverride{ -1 };     // -1 means use blueprint default
        int damageOverride{ -1 };
        float speedOverride{ -1.0f };
        std::string textureOverride;  // empty means use blueprint default
    };

    // Scene data container
    struct SerializedScene {
        std::string sceneName;
        std::vector<SerializedGameObject> objects;
    };

    class GameObjectSerializer {
    public:
        // Serialize a GameObject to data structure
        static SerializedGameObject serialize(const GameObject& obj);

        // Deserialize data into a new GameObject using PrefabFactory
        static std::unique_ptr<GameObject> deserialize(const SerializedGameObject& data);

        // Scene-level serialization
        static bool saveSceneToFile(const SerializedScene& scene, const std::string& filepath);
        static bool loadSceneFromFile(const std::string& filepath, SerializedScene& outScene);

        // JSON conversion helpers
        static std::string toJSON(const SerializedScene& scene);
        static bool fromJSON(const std::string& json, SerializedScene& outScene);

    private:
        static std::string escapeJSON(const std::string& str);
        static std::string unescapeJSON(const std::string& str);

        // Helper to parse a single JSON object
        static bool parseJSONObject(const std::string& objStr, SerializedGameObject& outObj);

        // Helper to extract value from JSON key-value pair
        static std::string extractJSONValue(const std::string& json, const std::string& key);
        static float extractJSONFloat(const std::string& json, const std::string& key, float defaultVal = 0.0f);
        static int extractJSONInt(const std::string& json, const std::string& key, int defaultVal = -1);
        static bool extractJSONBool(const std::string& json, const std::string& key, bool defaultVal = true);
        static void extractJSONVec2(const std::string& json, const std::string& key, glm::vec2& outVec);
    };

} // namespace Gameplay