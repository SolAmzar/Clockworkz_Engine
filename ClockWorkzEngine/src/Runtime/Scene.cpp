/******************************************************************************/
/*!
\file           Scene.cpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares Scene, a container for organizing game objects,
                systems, and level data.
                Responsibilities include:
                - Holding and managing collections of GameObjects
                - Providing update and render hooks for all objects in the scene
                - Supporting scene-level setup and teardown

References:
- Scene management patterns: https://gameprogrammingpatterns.com/state.html
- Unity Scenes (conceptual reference): https://docs.unity3d.com/Manual/CreatingScenes.html

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/


#include "Runtime/Scene.hpp"
#include "Gameplay/GameObject.hpp"
#include "Gameplay/Physics2D.hpp"
#include "Gameplay\Serializer.hpp"  // ADD THIS
#include "Gameplay/Components/Prefab.hpp"  // ADD THIS
#include <iostream>
#include "Assets/ResourceManager.hpp"
#include <filesystem>

using namespace Runtime;
using namespace Gameplay;
//using ClockWorkz::Assets::ResourceManager;

Scene::Scene(const std::string& name) : m_name(name) {
    m_physics = std::make_unique<Physics2D>();  // Initialize physics
}

Scene::~Scene() = default;
const std::string& Scene::name() const { return m_name; }
std::vector<std::unique_ptr<Gameplay::GameObject>>& Scene::objects() { return m_objects; }

bool Scene::load() {
    std::cout << "[Scene] load: " << m_name << "\n";

    //try {
    //    ResourceManager<Graphics::Texture>::Load(
    //        "background",
    //        "assets/images/background.jpg"
    //    );
    //    std::cout << "[Scene] Background texture loaded successfully\n";
    //}
    //catch (const std::exception& e) {
    //    std::cerr << "[Scene] Failed to load background: " << e.what() << "\n";
    //}

    // TRY TO LOAD FROM JSON FILE FIRST
    std::string filepath = "assets/scenes/" + m_name + ".json";
    SerializedScene sceneData;

    objects().push_back(GameObject::MakeFloor({ 0,0 }, { 20,12 }, "tex_floor"));

    if (GameObjectSerializer::loadSceneFromFile(filepath, sceneData)) {
        // Deserialize all objects from the file
        std::cout << "[Scene] Loading from file: " << filepath << "\n";
        for (const auto& objData : sceneData.objects) {
            auto obj = GameObjectSerializer::deserialize(objData);
            if (obj) {
                objects().push_back(std::move(obj));
            }
        }
        std::cout << "[Scene] Loaded " << objects().size() << " objects from file\n";
        return true;
    }

    // FALLBACK: Create default test room if file doesn't exist
    std::cout << "[Scene] No scene file found, creating default test room\n";

    //creates a default json if file is not found uncomment once everything is ready

    //auto& factory = PrefabFactory::get();

    //// Create floor
    //auto floor = factory.createAt("Floor", glm::vec2(0, 0));
    //if (floor) {
    //    floor->setSize(glm::vec2(20, 12));
    //    objects().push_back(std::move(floor));
    //}

    //// Create walls
    //auto wall1 = factory.createAt("Wall", glm::vec2(-10, 0));
    //if (wall1) {
    //    wall1->setSize(glm::vec2(1, 12));
    //    objects().push_back(std::move(wall1));
    //}

    //auto wall2 = factory.createAt("Wall", glm::vec2(10, 0));
    //if (wall2) {
    //    wall2->setSize(glm::vec2(1, 12));
    //    objects().push_back(std::move(wall2));
    //}

    //auto wall3 = factory.createAt("Wall", glm::vec2(0, -6));
    //if (wall3) {
    //    wall3->setSize(glm::vec2(20, 1));
    //    objects().push_back(std::move(wall3));
    //}

    //auto wall4 = factory.createAt("Wall", glm::vec2(0, 6));
    //if (wall4) {
    //    wall4->setSize(glm::vec2(20, 1));
    //    objects().push_back(std::move(wall4));
    //}

    //// Create player
    //objects().push_back(factory.createAt("Player", glm::vec2(-6, -2)));

    //// Create enemies
    //objects().push_back(factory.createAt("Enemy", glm::vec2(3, 0)));
    //objects().push_back(factory.createAt("FastEnemy", glm::vec2(5, 2)));

    return true;
}

void Scene::unload() {
    std::cout << "[Scene] unload: " << m_name << "\n";

    std::string filepath = "assets/scenes/" + m_name + "_autosave.json";

    std::filesystem::path absPath = std::filesystem::absolute(filepath);
    std::cout << "[Scene] Attempting to save to: " << absPath << "\n";

    saveToFile(filepath);

    m_objects.clear();
}

bool Scene::saveToFile(const std::string& filepath) {
    SerializedScene sceneData;
    sceneData.sceneName = m_name;

    // Serialize all active objects
    for (const auto& obj : objects()) {
        if (obj) {
            sceneData.objects.push_back(GameObjectSerializer::serialize(*obj));
        }
    }

    return GameObjectSerializer::saveSceneToFile(sceneData, filepath);
}

//temp commented out for testing of serializer
//void Scene::update(double dt) {
//    for (auto& o : objects()) {
//        if (o->active()) o->update(dt);
//    }
//    if (m_physics) {
//        m_physics->step(objects(), dt);
//    }
//}

void Scene::update(double dt) {
    static double timer = 0.0;
    timer += dt;

    // Print positions every 2 seconds
    if (timer >= 2.0) {
        std::cout << "\n=== Object Positions ===\n";
        for (auto& o : objects()) {
            if (o->active()) {
                auto pos = o->transform().position;
                std::cout << o->name() << " at (" << pos.x << ", " << pos.y << ")\n";
            }
        }
        timer = 0.0;
    }

    for (auto& o : objects()) {
        if (o->active()) o->update(dt);
    }
    if (m_physics) {
        m_physics->step(objects(), dt);
    }
}

void Scene::render() {
    for (auto& o : objects()) {
        if (o->active()) o->render();
    }
}