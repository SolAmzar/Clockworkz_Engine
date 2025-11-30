/******************************************************************************/
/*!
\file           Serializer.cpp
\project        Lights Out
\author(s)      [Yusuf], 50% [Lebon], 30%  [Wei An], 20%
\brief          Implements full JSON serialization/deserialization for GameObjects,
                their components, and entire scenes, including prefab-based 
                construction, transform/collider parsing, and animation/audio loading.


All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/


#include "Gameplay\Serializer.hpp"
#include "Gameplay\GameObject.hpp"
#include "Gameplay\Components\Health.hpp"
#include "Gameplay\Components\DamageDealer.hpp"
#include "Gameplay\Components\Patrol.hpp"
#include "Gameplay\Collider.hpp"
#include "Gameplay\Prefab.hpp"
#include "Math/Vec2.hpp"
#include <fstream>
#include <iostream>
#include <Gameplay/Components/SpriteRenderer.hpp>
#include <Gameplay/Components/SpriteAnimator.hpp>
#include <Gameplay/Components/AudioSource.hpp>



namespace Gameplay {

    // ========================================================================
    // COMPONENT SERIALIZATION
    // ========================================================================

    json GameObjectSerializer::serializeTransform(const GameObject& obj) {
        json j;
        j["position"]["x"] = obj.transform().position.x;
        j["position"]["y"] = obj.transform().position.y;
        j["rotation"] = obj.transform().rotationRad;
        j["scale"]["x"] = obj.transform().scale.x;
        j["scale"]["y"] = obj.transform().scale.y;
        return j;
    }

    json GameObjectSerializer::serializeCollider(const GameObject& obj) {
        json j;

        // Common fields
        j["isSolid"] = obj.isSolid();
        j["layer"] = obj.layer();
        j["collisionMask"] = obj.collisionMask();
        j["moveable"] = obj.ismoveable();
        j["collisionCategory"] = obj.collisionCategory();
        j["blocksLight"] = obj.blocksLight();

        // Shape-specific: ask the colliders directly
        if (auto* circle = obj.getCollider<CircleCollider>()) {
            j["type"] = "Circle";
            j["radius"] = circle->radius();

            j["width"] = obj.size().x;   // optional, for editor display
            j["height"] = obj.size().y;
        }
        else if (auto* aabb = obj.getCollider<AABBCollider>()) {
            j["type"] = "AABB";
            j["width"] = obj.size().x;
            j["height"] = obj.size().y;
        }
        else {
            j["type"] = "None";
        }

        return j;
    }


    json GameObjectSerializer::serializeHealth(const GameObject& obj) {
        auto* health = obj.getComponent<HealthComponent>();
        if (!health) return json();

        json j;
        j["hp"] = health->hp();
        return j;
    }

    json GameObjectSerializer::serializePatrol(const GameObject& obj) {
        if (auto* pat = obj.getComponent<PatrolComponent>()) {
            json j;
            j["a"]["x"] = pat->a().x;
            j["a"]["y"] = pat->a().y;
            j["b"]["x"] = pat->b().x;
            j["b"]["y"] = pat->b().y;
            j["speed"] = pat->speed();
            return j;
        }
        return json(); // empty if not present
    }

    json GameObjectSerializer::serializeDamage(const GameObject& obj) {
        auto* damage = obj.getComponent<DamageDealerComponent>();
        if (!damage) return json();

        json j;
        j["damage"] = damage->damage();
        return j;
    }

    json GameObjectSerializer::serializeSpriteRenderer(const GameObject& obj) {
        json j;
        //j["textureId"] = obj.textureId();
        if (auto* sr = obj.getComponent<SpriteRendererComponent>()) {
            j["textureId"] = obj.textureId(); // or sr->textureAlias if you store it there

            auto uv = sr->uv();
            j["uv"] = { uv.x, uv.y, uv.z, uv.w };
            // (optionally tint too)
        }


        return j;
    }

    json GameObjectSerializer::serializeSpriteAnimator(const GameObject& obj) {
        if (auto* anim = obj.getComponent<SpriteAnimatorComponent>()) {
            json j;
            j["filename"] = anim->getFilename();
            return j;
        }
        return json();
    }

    json GameObjectSerializer::serializeAudioSource(const GameObject& obj) {
        json j;
        if (auto* audio = obj.getComponent<AudioSourceComponent>()) {
            j["soundId"] = audio->soundId;
            j["volume"] = audio->volume;
            j["loop"] = audio->loop;
            j["playOnStart"] = audio->playOnStart;
            j["group"] = static_cast<int>(audio->group);
        }
        return j;
    }

    // ========================================================================
    // COMPONENT DESERIALIZATION
    // ========================================================================

    void GameObjectSerializer::deserializeTransform(GameObject& obj, const json& j) {
        if (j.contains("position")) {
            obj.transform().position.x = j["position"].value("x", 0.0f);
            obj.transform().position.y = j["position"].value("y", 0.0f);
        }
        if (j.contains("rotation")) {
            obj.transform().rotationRad = j["rotation"].get<float>();
        }
        if (j.contains("scale")) {
            obj.transform().scale.x = j["scale"].value("x", 1.0f);
            obj.transform().scale.y = j["scale"].value("y", 1.0f);
        }
    }

    void GameObjectSerializer::deserializeCollider(GameObject& obj, const json& j) {
        if (j.contains("width") && j.contains("height")) {
            obj.setSize(vec2(j["width"].get<float>(), j["height"].get<float>()));
        }
        if (j.contains("isSolid")) {
            obj.setSolid(j["isSolid"].get<bool>());
        }
        if (j.contains("layer")) {
            obj.setLayer(j["layer"].get<uint32_t>());
        }
        if (j.contains("collisionMask")) {
            obj.setCollisionMask(j["collisionMask"].get<uint32_t>());
        }
        if (j.contains("collisionCategory")) {
            obj.setCollisionCategory(j["collisionCategory"].get<uint32_t>());
        }
        if (j.contains("blocksLight")) {
            obj.setBlocksLight(j["blocksLight"].get<bool>());
        }
        if (j.contains("layer")) {
            obj.setLayer(j["layer"].get<uint32_t>());
        }
        if (j.contains("type")) {
            std::string s = j["type"].get<std::string>();
            if (s == "AABB") { 
                auto* collide = obj.addCollider<AABBCollider>();
                collide->type = ColliderType::AABB;
                if (j.contains("blocksLight")) {
                    collide->blocksLight=((j["blocksLight"].get<bool>()));
                }
        /*        if (obj.name() == "Enemy" || obj.name() == "Enemy1" || obj.name() == "Enemy2") {
                    std::cout << "deserialize enemy" << std::endl;
                }*/

                if (obj.name() == "Wall") {
                    std::cout << "deserialize wall" << std::endl;
                }
                if (j.contains("moveable")) {
                    obj.setMoveable(j["moveable"].get<bool>());
                }
            }
            else if (s == "Circle") {
                auto* collide = obj.addCollider<CircleCollider>();
                collide->type = ColliderType::Circle;

                // Default: half of width
                float radius = obj.size().x * 0.5f;

                if (j.contains("radius")) {
                    radius = j["radius"].get<float>();
                }

                collide->setRadius(radius);

                if (j.contains("blocksLight")) {
                    collide->blocksLight = j["blocksLight"].get<bool>();
                }

                obj.setShape(Gameplay::GameObjectShape::Circle);
                obj.setSize(vec2(radius * 2.0f, radius * 2.0f));
   /*             if (obj.name() == "Enemy" || obj.name() == "Enemy1" || obj.name() == "Enemy2") {
                    std::cout << "deserialize enemy" << std::endl;
                }

                if (obj.name() == "Wall") {
                    std::cout << "deserialize wall" << std::endl;
                }*/
                if (j.contains("moveable")) {
                    obj.setMoveable(j["moveable"].get<bool>());
                }
            }
            else if (s == "None") {
                auto* collide = obj.addCollider<Collider>();
                collide->type = ColliderType::None;
            }
        }


      
    }

    void GameObjectSerializer::deserializeHealth(GameObject& obj, const json& j) {
        if (j.contains("hp")) {
            obj.addComponent<HealthComponent>(j["hp"].get<int>());
        }
    }

    void GameObjectSerializer::deserializeDamage(GameObject& obj, const json& j) {
        if (j.contains("damage")) {
            obj.addComponent<DamageDealerComponent>(j["damage"].get<int>());
        }
    }

    void GameObjectSerializer::deserializeSpriteRenderer(GameObject& obj, const json& j) {
        if (j.contains("textureId")) {
            std::string texId = j["textureId"].get<std::string>();
            obj.setTextureId(texId);

            auto* sr = obj.addComponent<SpriteRendererComponent>(texId);

            if (j.contains("uv") && j["uv"].is_array() && j["uv"].size() == 4) {
                auto arr = j["uv"];
                glm::vec4 uv{ arr[0].get<float>(), arr[1].get<float>(),
                              arr[2].get<float>(), arr[3].get<float>() };
               
                sr->setUV(uv);
            }
        }
     

    }

    void GameObjectSerializer::deserializePatrol(GameObject& obj, const json& j) {
        // Expected shape:
        if (!j.is_object()) return;

        vec2 a{ 0.f, 0.f };
        vec2 b{ 0.f, 0.f };
        float speed = 100.f;

        if (j.contains("a")) {
            a.x = j["a"].value("x", 0.f);
            a.y = j["a"].value("y", 0.f);
        }
        if (j.contains("b")) {
            b.x = j["b"].value("x", 0.f);
            b.y = j["b"].value("y", 0.f);
        }
        if (j.contains("speed")) {
            speed = j["speed"].get<float>();
        }

        obj.addComponent<PatrolComponent>(a, b, speed);
    }

    void GameObjectSerializer::deserializeSpriteAnimator(GameObject& obj, const std::string& jsonFileName) {
        std::string folder = "assets/animations/";
        std::string path = folder + std::string(jsonFileName);
        std::ifstream f(path);
        if (f.is_open()) {
            nlohmann::json j;
            f >> j;
            auto* comp = obj.addComponent<SpriteAnimatorComponent>();
            comp->loadFromJson(j);
            std::cout << "[SpriteState] Reloaded animation from: "
                << std::filesystem::absolute(path) << std::endl;
            if (auto* sr = obj.getComponent<SpriteRendererComponent>())
                comp->setSpriteRenderer(sr);
        }
    }

    void GameObjectSerializer::deserializeAudioSource(GameObject& obj, const json& j) {
        if (!j.is_object()) return;

        auto* audio = obj.addComponent<AudioSourceComponent>();
        audio->soundId = j.value("soundId", "");
        audio->volume = j.value("volume", 1.0f);
        audio->loop = j.value("loop", false);
        audio->playOnStart = j.value("playOnStart", false);
        audio->group = static_cast<Audio::Group>(
            j.value("group", static_cast<int>(Audio::Group::SFX))
            );
    }

    // ========================================================================
    // GAMEOBJECT SERIALIZATION
    // ========================================================================

    json GameObjectSerializer::serializeGameObject(const GameObject& obj) {
        json j;

        // Basic properties
        j["name"] = obj.name();
        j["active"] = obj.active();
        j["type"] = static_cast<int>(obj.type());

        // Save prefab name if the object was created from a prefab
        if (!obj.prefabName().empty()) {
            j["prefab"] = obj.prefabName();
        }

        // Components object
        json components;

        // Always include transform
        components["transform"] = serializeTransform(obj);

        // Optional collider (only if size is not default)
        if (obj.size().x != 1.0f || obj.size().y != 1.0f || obj.isSolid()) {
            components["collider"] = serializeCollider(obj);
        }

        // Optional health component
        if (obj.getComponent<HealthComponent>()) {
            components["health"] = serializeHealth(obj);
        }

        // Optional damage component
        if (obj.getComponent<DamageDealerComponent>()) {
            components["damage"] = serializeDamage(obj);
        }

        // Optional sprite renderer
        if (!obj.textureId().empty()) {
            components["spriteRenderer"] = serializeSpriteRenderer(obj);
        }
		// Optional patrol component
        if (obj.getComponent<PatrolComponent>()) {
            components["patrol"] = serializePatrol(obj);
        }

        if (obj.getComponent<SpriteAnimatorComponent>()) {
            components["spriteAnimator"] = serializeSpriteAnimator(obj);
        }

        if (obj.getComponent<AudioSourceComponent>()) {
            components["audio"] = serializeAudioSource(obj);
        }
            

        j["components"] = components;

        return j;
    }

 




    // ========================================================================
    // GAMEOBJECT DESERIALIZATION (WITH PREFAB SUPPORT)
    // ========================================================================

    std::unique_ptr<GameObject> GameObjectSerializer::deserializeGameObject(const json& j) {
        std::unique_ptr<GameObject> obj;

        // 1️⃣ Prefab-based creation
        if (j.contains("prefab")) {
            std::string prefabName = j["prefab"].get<std::string>();
            std::cout << "[Serializer] Loading prefab: " << prefabName << "\n";

            // Extract some basic overrides early (position, scale, texture, hp, dmg)
            vec2 pos{ 0.f }, scale{ 100.f, 100.f }, patrola{ 0.f, 0.f }, patrolb{ 0.f,0.f };
            std::string tex = "";
            std::string spriteAnimatorFileName;
            int hp = 3, dmg = 1;
			float speed = 100.f;

            if (j.contains("components")) {
                const auto& comps = j["components"];

                if (comps.contains("transform")) {
                    const auto& t = comps["transform"];
                    if (t.contains("position"))
                        pos = { t["position"].value("x", 0.f), t["position"].value("y", 0.f) };
                    if (t.contains("scale"))
                        scale = { t["scale"].value("x", 100.f), t["scale"].value("y", 100.f) };
                }

                if (comps.contains("spriteRenderer"))
                    tex = comps["spriteRenderer"].value("textureId", "");

                if (comps.contains("spriteAnimator")) {
                    spriteAnimatorFileName = comps["spriteAnimator"].value("filename", "");
                }
                if (comps.contains("audio")) {
                    deserializeAudioSource(*obj, comps["audio"]);
                }   

                if (comps.contains("health"))
                    hp = comps["health"].value("hp", 3);

                if (comps.contains("damage"))
                    dmg = comps["damage"].value("damage", 1);

                if (comps.contains("patrol")) {
                    const auto& p = comps["patrol"];
                    if (p.contains("a")) {
                        patrola.x = p["a"].value("x", 0.f);
                        patrola.y = p["a"].value("y", 0.f);
                    }
                    if (p.contains("b")) {
                        patrolb.x = p["b"].value("x", 0.f);
                        patrolb.y = p["b"].value("y", 0.f);
                    }
                    speed = p.value("speed", speed);
                }
            }

            // 2️⃣ Create prefab with correct factory (preserves colliders & masks)

            obj = PrefabFactory::get().createAt(prefabName, pos);

           /* if (prefabName == "Player")
                obj = GameObject::MakePlayer(pos, scale, tex);
            else if (prefabName == "Enemy")
                obj = GameObject::MakeEnemy(pos, scale, hp, tex);
            else if (prefabName == "Wall")
                obj = GameObject::MakeWall(pos, scale, tex);
            else if (prefabName == "Floor")
                obj = GameObject::MakeFloor(pos, scale, tex);
            else if (prefabName == "Trap")
                obj = GameObject::MakeTrap(pos, scale, dmg, tex);*/
            /*else if (prefabName == "Ball")
                obj = GameObject::MakeBall(pos, 50.f, scale, tex);*/
            if (!obj) {
                std::cerr << "[Serializer] Unknown prefab type: " << prefabName << "\n";
                obj = std::make_unique<GameObject>(GameObjectType::Unknown, prefabName);
            }

            // 3️⃣ Apply overrides AFTER prefab creation (so you don’t lose collider)
            if (j.contains("components")) {
                const auto& comps = j["components"];

                if (comps.contains("transform"))
                    deserializeTransform(*obj, comps["transform"]);
                if (comps.contains("health"))
                    deserializeHealth(*obj, comps["health"]);
                if (comps.contains("damage"))
                    deserializeDamage(*obj, comps["damage"]);
                /*if (comps.contains("spriteRenderer"))
                    deserializeSpriteRenderer(*obj, comps["spriteRenderer"]);*/
                if (comps.contains("collider"))
                    deserializeCollider(*obj, comps["collider"]);
                if (comps.contains("patrol"))
                    deserializePatrol(*obj, comps["patrol"]);
                if (comps.contains("spriteAnimator")) {
                    deserializeSpriteAnimator(*obj, comps["spriteAnimator"].value("filename", ""));
                }
            }

            // 4️⃣ Apply generic fields (name, active)
            if (j.contains("name"))
                obj->setName(j["name"].get<std::string>());
            if (j.contains("active"))
                obj->setActive(j["active"].get<bool>());
            obj->autoUpdateCollisionBaseOnScale();
            return obj;
        }

        // 5️. Fallback: Non-prefab legacy JSON
        std::string name = j.value("name", "Unnamed");
        GameObjectType type = GameObjectType::Unknown;
        if (j.contains("type")) {
            type = static_cast<GameObjectType>(j["type"].get<int>());
        }

        obj = std::make_unique<GameObject>(type, name);

        if (j.contains("active"))
            obj->setActive(j["active"].get<bool>());

        if (j.contains("components") && j["components"].is_object()) {
            const json& comps = j["components"];
            if (comps.contains("transform"))
                deserializeTransform(*obj, comps["transform"]);
            if (comps.contains("collider"))
                deserializeCollider(*obj, comps["collider"]);
            if (comps.contains("health"))
                deserializeHealth(*obj, comps["health"]);
            if (comps.contains("damage"))
                deserializeDamage(*obj, comps["damage"]);
            if (comps.contains("spriteRenderer"))
                deserializeSpriteRenderer(*obj, comps["spriteRenderer"]);
            if (comps.contains("spriteAnimator")) {
                std::string spriteAnimatorFilePath = comps["spriteAnimator"].value("filename", "");
                deserializeSpriteAnimator(*obj, spriteAnimatorFilePath);
            }
            if (comps.contains("audio"))
                deserializeAudioSource(*obj, comps["audio"]);
        }

        // Too many hard code below code to be remove later, the level json should have proper handling of the object colliders information.
        // Serializing from level editor should have fix it. make it such that it loads attributes from prefab, but things like transform/level state u can overwrite.
        using GT = Gameplay::GameObjectType;
        switch (obj->type()) {
        case GT::Wall:    obj->setMoveable(false); break;       // <-- walls are static
        case GT::Floor:   obj->setMoveable(false); break;         // optional: floors static too
        case GT::Trap:    obj->setMoveable(false); break;        // traps as triggers (no push)
        case GT::Box:	  obj->setMoveable(true);  break;        // boxes are movable
        case GT::Player:  obj->setMoveable(true);  break;
        case GT::Enemy:   obj->setMoveable(true);  break;
        case GT::Ball:    obj->setMoveable(false); break;        // dynamic bodies move
        default:
            obj->setMoveable(true);
        }

            

        return obj;
    }


    // ========================================================================
    // SCENE SERIALIZATION
    // ========================================================================

    json GameObjectSerializer::serializeScene(const std::string& sceneName,
        const std::vector<std::unique_ptr<GameObject>>& objects) {
        json j;
        j["sceneName"] = sceneName;

        json objectsArray = json::array();
        for (const auto& obj : objects) {
            if (obj) {
                objectsArray.push_back(serializeGameObject(*obj));
            }
        }
        j["objects"] = objectsArray;

        return j;
    }

    bool GameObjectSerializer::saveSceneToFile(const std::string& sceneName,
        const std::vector<std::unique_ptr<GameObject>>& objects,
        const std::string& filepath) {
        try {
            json sceneJson = serializeScene(sceneName, objects);

            std::ofstream file(filepath);
            if (!file.is_open()) {
                std::cerr << "[Serializer] Failed to open file for writing: " << filepath << "\n";
                return false;
            }

            // Write with pretty formatting (4 spaces indent)
            file << sceneJson.dump(4);
            file.close();

            std::cout << "[Serializer] Scene '" << sceneName << "' saved to: " << filepath << "\n";
            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "[Serializer] Error saving scene: " << e.what() << "\n";
            return false;
        }
    }

    bool GameObjectSerializer::loadSceneFromFile(const std::string& filepath,
        std::string& outSceneName,
        std::vector<std::unique_ptr<GameObject>>& outObjects) {
        try {
            std::ifstream file(filepath);
            if (!file.is_open()) {
                std::cerr << "[Serializer] Failed to open file for reading: " << filepath << "\n";
                return false;
            }

            json j;
            file >> j;
            file.close();

            // Parse scene name
            outSceneName = j.value("sceneName", "Unnamed Scene");

            // Parse objects
            outObjects.clear();
            if (j.contains("objects") && j["objects"].is_array()) {
                for (const auto& objJson : j["objects"]) {
                    auto obj = deserializeGameObject(objJson);
                    if (obj) {
                        outObjects.push_back(std::move(obj));
                    }
                }
            }

            std::cout << "[Serializer] Scene '" << outSceneName << "' loaded from: " << filepath
                << " (" << outObjects.size() << " objects)\n";
            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "[Serializer] Error loading scene: " << e.what() << "\n";
            return false;
        }
    }

} // namespace Gameplay