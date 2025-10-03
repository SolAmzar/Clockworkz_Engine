#include "Gameplay/Serializer.hpp"
#include "Gameplay/GameObject.hpp"
#include "Gameplay/Components/Prefab.hpp"
#include "Gameplay/Components/Health.hpp"
#include "Gameplay/Components/DamageDealer.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cctype>

/******************************************************************************/
/*!
\file           Serializer.cpp
\project        Lights Out
\author(s)      [Yusuf], 100%
\brief          Declares Serializer, a system for converting GameObjects to/from
                JSON format and managing scene persistence.
                Responsibilities include:
                - Serializing GameObject instances to JSON-compatible data structures
                - Deserializing JSON data back into GameObjects using PrefabFactory
                - Reading and writing scene files in JSON format
                - Converting between in-memory SerializedScene structures and JSON strings
                - Handling data validation and error recovery during load operations
                - Supporting save/load functionality for game state persistence
                - Enabling level design through human-readable JSON files
                - Managing blueprint-based instantiation with property overrides

                The Serializer acts as the bridge between runtime game objects
                and persistent storage, allowing scenes to be saved when exiting
                and restored when loading, as well as enabling autosave functionality.

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

namespace Gameplay {

    SerializedGameObject GameObjectSerializer::serialize(const GameObject& obj) {
        SerializedGameObject data;

        // Store transform data
        data.position = obj.transform().position;
        data.scale = obj.transform().scale;
        data.rotation = obj.transform().rotationDeg;
        data.active = obj.active();

        // Store texture if set
        if (!obj.textureId().empty()) {
            data.textureOverride = obj.textureId();
        }

        // Store component data
        if (auto* hp = obj.getComponent<HealthComponent>()) {
            data.healthOverride = hp->hp();
        }
        if (auto* dmg = obj.getComponent<DamageDealerComponent>()) {
            data.damageOverride = dmg->damage();
        }

        // Determine blueprint name based on type
        // This is a reverse lookup - you might want to store this in GameObject
        switch (obj.type()) {
        case GameObjectType::Floor:  data.blueprintName = "Floor"; break;
        case GameObjectType::Wall:   data.blueprintName = "Wall"; break;
        case GameObjectType::Player: data.blueprintName = "Player"; break;
        case GameObjectType::Enemy:  data.blueprintName = "Enemy"; break;
        case GameObjectType::Trap:   data.blueprintName = "Trap"; break;
        default: data.blueprintName = "Unknown"; break;
        }

        return data;
    }

    std::unique_ptr<GameObject> GameObjectSerializer::deserialize(const SerializedGameObject& data) {
        auto& factory = PrefabFactory::get();

        // Create from blueprint
        auto obj = factory.createAt(data.blueprintName, data.position);
        if (!obj) {
            std::cerr << "[Serializer] Failed to create object from blueprint: "
                << data.blueprintName << "\n";
            return nullptr;
        }

        // Apply transform overrides
        obj->transform().scale = data.scale;
        obj->transform().rotationDeg = data.rotation;
        obj->setActive(data.active);

        // Apply texture override
        if (!data.textureOverride.empty()) {
            obj->setTextureId(data.textureOverride);
        }

        // Apply component overrides
        if (data.healthOverride >= 0) {
            if (auto* hp = obj->getComponent<HealthComponent>()) {
                // Health exists but we can't change it easily - would need to add setter
                // For now, recreate component if value differs
            }
            else {
                obj->addComponent<HealthComponent>(data.healthOverride);
            }
        }

        if (data.damageOverride >= 0) {
            if (auto* dmg = obj->getComponent<DamageDealerComponent>()) {
                dmg->setDamage(data.damageOverride);
            }
            else {
                obj->addComponent<DamageDealerComponent>(data.damageOverride);
            }
        }

        return obj;
    }

    std::string GameObjectSerializer::toJSON(const SerializedScene& scene) {
        std::ostringstream oss;
        oss << "{\n";
        oss << "  \"sceneName\": \"" << escapeJSON(scene.sceneName) << "\",\n";
        oss << "  \"objects\": [\n";

        for (size_t i = 0; i < scene.objects.size(); ++i) {
            const auto& obj = scene.objects[i];
            oss << "    {\n";
            oss << "      \"blueprint\": \"" << escapeJSON(obj.blueprintName) << "\",\n";
            oss << "      \"position\": [" << obj.position.x << ", " << obj.position.y << "],\n";
            oss << "      \"scale\": [" << obj.scale.x << ", " << obj.scale.y << "],\n";
            oss << "      \"rotation\": " << obj.rotation << ",\n";
            oss << "      \"active\": " << (obj.active ? "true" : "false");

            if (obj.healthOverride >= 0) {
                oss << ",\n      \"health\": " << obj.healthOverride;
            }
            if (obj.damageOverride >= 0) {
                oss << ",\n      \"damage\": " << obj.damageOverride;
            }
            if (obj.speedOverride >= 0.0f) {
                oss << ",\n      \"speed\": " << obj.speedOverride;
            }
            if (!obj.textureOverride.empty()) {
                oss << ",\n      \"texture\": \"" << escapeJSON(obj.textureOverride) << "\"";
            }

            oss << "\n    }";
            if (i < scene.objects.size() - 1) oss << ",";
            oss << "\n";
        }

        oss << "  ]\n";
        oss << "}\n";
        return oss.str();
    }

    bool GameObjectSerializer::fromJSON(const std::string& json, SerializedScene& outScene) {
        // Parse sceneName
        outScene.sceneName = extractJSONValue(json, "sceneName");

        // Find objects array
        size_t objsStart = json.find("\"objects\"");
        if (objsStart == std::string::npos) {
            std::cerr << "[Serializer] No 'objects' array found in JSON\n";
            return false;
        }

        size_t arrayStart = json.find("[", objsStart);
        size_t arrayEnd = json.rfind("]");

        if (arrayStart == std::string::npos || arrayEnd == std::string::npos) {
            std::cerr << "[Serializer] Invalid objects array\n";
            return false;
        }

        // Extract objects array content
        std::string arrayContent = json.substr(arrayStart + 1, arrayEnd - arrayStart - 1);

        // Parse individual objects
        size_t pos = 0;
        int braceDepth = 0;
        size_t objStart = std::string::npos;

        for (size_t i = 0; i < arrayContent.length(); ++i) {
            char c = arrayContent[i];

            if (c == '{') {
                if (braceDepth == 0) {
                    objStart = i;
                }
                braceDepth++;
            }
            else if (c == '}') {
                braceDepth--;
                if (braceDepth == 0 && objStart != std::string::npos) {
                    // Extract this object
                    std::string objStr = arrayContent.substr(objStart, i - objStart + 1);
                    SerializedGameObject obj;
                    if (parseJSONObject(objStr, obj)) {
                        outScene.objects.push_back(obj);
                    }
                    objStart = std::string::npos;
                }
            }
        }

        std::cout << "[Serializer] Parsed " << outScene.objects.size() << " objects from JSON\n";
        return true;
    }

    bool GameObjectSerializer::parseJSONObject(const std::string& objStr, SerializedGameObject& outObj) {
        outObj.blueprintName = extractJSONValue(objStr, "blueprint");
        extractJSONVec2(objStr, "position", outObj.position);
        extractJSONVec2(objStr, "scale", outObj.scale);
        outObj.rotation = extractJSONFloat(objStr, "rotation", 0.0f);
        outObj.active = extractJSONBool(objStr, "active", true);
        outObj.healthOverride = extractJSONInt(objStr, "health", -1);
        outObj.damageOverride = extractJSONInt(objStr, "damage", -1);
        outObj.speedOverride = extractJSONFloat(objStr, "speed", -1.0f);
        outObj.textureOverride = extractJSONValue(objStr, "texture");

        return !outObj.blueprintName.empty();
    }

    std::string GameObjectSerializer::extractJSONValue(const std::string& json, const std::string& key) {
        std::string searchKey = "\"" + key + "\"";
        size_t keyPos = json.find(searchKey);
        if (keyPos == std::string::npos) return "";

        size_t colonPos = json.find(":", keyPos);
        if (colonPos == std::string::npos) return "";

        size_t valueStart = json.find("\"", colonPos);
        if (valueStart == std::string::npos) return "";

        size_t valueEnd = json.find("\"", valueStart + 1);
        if (valueEnd == std::string::npos) return "";

        return unescapeJSON(json.substr(valueStart + 1, valueEnd - valueStart - 1));
    }

    float GameObjectSerializer::extractJSONFloat(const std::string& json, const std::string& key, float defaultVal) {
        std::string searchKey = "\"" + key + "\"";
        size_t keyPos = json.find(searchKey);
        if (keyPos == std::string::npos) return defaultVal;

        size_t colonPos = json.find(":", keyPos);
        if (colonPos == std::string::npos) return defaultVal;

        // Skip whitespace
        size_t valueStart = colonPos + 1;
        while (valueStart < json.length() && std::isspace(json[valueStart])) valueStart++;

        // Find end of number
        size_t valueEnd = valueStart;
        while (valueEnd < json.length() &&
            (std::isdigit(json[valueEnd]) || json[valueEnd] == '.' ||
                json[valueEnd] == '-' || json[valueEnd] == 'e' || json[valueEnd] == 'E')) {
            valueEnd++;
        }

        if (valueStart == valueEnd) return defaultVal;

        std::string valueStr = json.substr(valueStart, valueEnd - valueStart);
        return std::stof(valueStr);
    }

    int GameObjectSerializer::extractJSONInt(const std::string& json, const std::string& key, int defaultVal) {
        return static_cast<int>(extractJSONFloat(json, key, static_cast<float>(defaultVal)));
    }

    bool GameObjectSerializer::extractJSONBool(const std::string& json, const std::string& key, bool defaultVal) {
        std::string searchKey = "\"" + key + "\"";
        size_t keyPos = json.find(searchKey);
        if (keyPos == std::string::npos) return defaultVal;

        size_t colonPos = json.find(":", keyPos);
        if (colonPos == std::string::npos) return defaultVal;

        size_t truePos = json.find("true", colonPos);
        size_t falsePos = json.find("false", colonPos);

        // Make sure we find the closest one after the colon
        if (truePos != std::string::npos && truePos < colonPos + 10) return true;
        if (falsePos != std::string::npos && falsePos < colonPos + 10) return false;

        return defaultVal;
    }

    void GameObjectSerializer::extractJSONVec2(const std::string& json, const std::string& key, glm::vec2& outVec) {
        std::string searchKey = "\"" + key + "\"";
        size_t keyPos = json.find(searchKey);
        if (keyPos == std::string::npos) return;

        size_t bracketStart = json.find("[", keyPos);
        if (bracketStart == std::string::npos) return;

        size_t bracketEnd = json.find("]", bracketStart);
        if (bracketEnd == std::string::npos) return;

        std::string arrayStr = json.substr(bracketStart + 1, bracketEnd - bracketStart - 1);

        // Parse two floats separated by comma
        size_t commaPos = arrayStr.find(",");
        if (commaPos != std::string::npos) {
            std::string xStr = arrayStr.substr(0, commaPos);
            std::string yStr = arrayStr.substr(commaPos + 1);

            // Trim whitespace
            xStr.erase(0, xStr.find_first_not_of(" \t\n\r"));
            xStr.erase(xStr.find_last_not_of(" \t\n\r") + 1);
            yStr.erase(0, yStr.find_first_not_of(" \t\n\r"));
            yStr.erase(yStr.find_last_not_of(" \t\n\r") + 1);

            outVec.x = std::stof(xStr);
            outVec.y = std::stof(yStr);
        }
    }

    bool GameObjectSerializer::saveSceneToFile(const SerializedScene& scene, const std::string& filepath) {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "[Serializer] Failed to open file for writing: " << filepath << "\n";
            return false;
        }

        std::string json = toJSON(scene);
        file << json;
        file.close();

        std::cout << "[Serializer] Scene saved to: " << filepath << "\n";
        return true;
    }

    bool GameObjectSerializer::loadSceneFromFile(const std::string& filepath, SerializedScene& outScene) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "[Serializer] Failed to open file for reading: " << filepath << "\n";
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string json = buffer.str();
        file.close();

        bool success = fromJSON(json, outScene);
        if (success) {
            std::cout << "[Serializer] Scene loaded from: " << filepath << "\n";
        }
        return success;
    }

    std::string GameObjectSerializer::escapeJSON(const std::string& str) {
        std::ostringstream oss;
        for (char c : str) {
            switch (c) {
            case '\"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default: oss << c; break;
            }
        }
        return oss.str();
    }

    std::string GameObjectSerializer::unescapeJSON(const std::string& str) {
        std::ostringstream oss;
        for (size_t i = 0; i < str.length(); ++i) {
            if (str[i] == '\\' && i + 1 < str.length()) {
                switch (str[i + 1]) {
                case '\"': oss << '\"'; ++i; break;
                case '\\': oss << '\\'; ++i; break;
                case 'n': oss << '\n'; ++i; break;
                case 'r': oss << '\r'; ++i; break;
                case 't': oss << '\t'; ++i; break;
                default: oss << str[i]; break;
                }
            }
            else {
                oss << str[i];
            }
        }
        return oss.str();
    }

} // namespace Gameplay