/******************************************************************************/
/*!
\file           Serializer.hpp
\project        Lights Out
\author(s)      [Yusuf], 100%
\brief          Declares Serializer data structures and utility functions for
                converting GameObjects to/from JSON format using nlohmann/json.
                Responsibilities include:
                - Component-based serialization/deserialization
                - Transform, collider, health, and damage component support
                - Scene-level save/load functionality
                - Extensible component registration system

                This header provides the data structures and function signatures
                needed to persist game state to disk and restore it, enabling
                save/load functionality and level design workflows.

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
#include <string>
#include <vector>
#include <memory>
#include <glm/vec2.hpp>
#pragma warning(push, 0)
#include "Core/json.hpp" // nlohmann::json
#pragma warning(pop)


namespace Gameplay {
    class GameObject;
    using json = nlohmann::json;

    class GameObjectSerializer {
    public:
        // Serialize a GameObject to JSON
        static json serializeGameObject(const GameObject& obj);

        // Deserialize JSON to create a new GameObject
        static std::unique_ptr<GameObject> deserializeGameObject(const json& j);

        // Scene-level serialization
        static json serializeScene(const std::string& sceneName,
            const std::vector<std::unique_ptr<GameObject>>& objects);

        static bool saveSceneToFile(const std::string& sceneName,
            const std::vector<std::unique_ptr<GameObject>>& objects,
            const std::string& filepath);

        static bool loadSceneFromFile(const std::string& filepath,
            std::string& outSceneName,
            std::vector<std::unique_ptr<GameObject>>& outObjects);

    private:
        // Component serialization helpers
        static json serializeTransform(const GameObject& obj);
        static json serializeCollider(const GameObject& obj);
        static json serializeHealth(const GameObject& obj);
        static json serializeDamage(const GameObject& obj);
        static json serializePatrol(const GameObject& obj);
        static json serializeSpriteRenderer(const GameObject& obj);
        static json serializeSpriteAnimator(const GameObject& obj);

        static json serializeFragment(const GameObject& obj);
        static json serializeFragmentCollector(const GameObject& obj);

        // Component deserialization helpers
        static void deserializeTransform(GameObject& obj, const json& j);
        static void deserializeCollider(GameObject& obj, const json& j);
        static void deserializeHealth(GameObject& obj, const json& j);
        static void deserializeDamage(GameObject& obj, const json& j);
        static void deserializePatrol(GameObject& obj, const json& j);
        static void deserializeSpriteRenderer(GameObject& obj, const json& j);
        static void deserializeSpriteAnimator(GameObject& obj, const std::string& j);

        static void deserializeFragment(GameObject& obj, const json& j);
        static void deserializeFragmentCollector(GameObject& obj, const json& j);
		// Audio Source component serialization/deserialization
        static json serializeAudioSource(const GameObject& obj);
        static void deserializeAudioSource(GameObject& obj, const json& j);
    };

} // namespace Gameplay