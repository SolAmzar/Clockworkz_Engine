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
#include "Gameplay/Serializer.hpp"        // ADD THIS
#include "Gameplay/Prefab.hpp" // ADD THIS
#include <iostream>
#include <exception>
#include "Assets/ResourceManager.hpp"
#include <filesystem>
#include "Graphics/Texture2D.hpp"
#include "Core/Engine.hpp"
#include <Core/Profiler.hpp>

using namespace Runtime;
using namespace Gameplay;
using Assets::ResourceManager;

Scene::Scene(const std::string &name) : m_name(name)
{
    m_physics = std::make_unique<Physics2D>(); // Initialize physics
}

Scene::~Scene() = default;
const std::string &Scene::name() const { return m_name; }
std::vector<std::unique_ptr<Gameplay::GameObject>> &Scene::objects() { return m_objects; }

bool Scene::load()
{
    try {
        std::cout << "[Scene] load: " << m_name << "\n";

        // try {
        //     ResourceManager<Graphics::Texture>::Load(
        //         "background",
        //         "assets/images/background.jpg"
        //     );
        //     std::cout << "[Scene] Background texture loaded successfully\n";
        // }
        // catch (const std::exception& e) {
        //     std::cerr << "[Scene] Failed to load background: " << e.what() << "\n";
        // }
        std::cout << "[Scene] load: " << m_name << "\n";

        std::string filepath = "assets/scenes/" + m_name + ".json";
        SerializedScene sceneData;

        // Preload common textures
        std::cout << "[Scene] Loading textures...\n";

        auto playerTex = ResourceManager<Graphics::Texture2D>::Load("player_texture", "assets/textures/player.png");
        if (!playerTex) {
            throw std::runtime_error("Failed to load critical texture: player_texture\n");
        }

        auto enemyTex = ResourceManager<Graphics::Texture2D>::Load("enemy_texture", "assets/textures/enemy.png");
        if (!enemyTex) {
            throw std::runtime_error("[Scene] Warning: Failed to load enemy_texture\n");
        }

        auto floorTex = ResourceManager<Graphics::Texture2D>::Load("floor_texture", "assets/textures/floor.png");
        if (!floorTex) {
            throw std::runtime_error("[Scene] Warning: Failed to load floor_texture\n");
        }

        auto wallTex = ResourceManager<Graphics::Texture2D>::Load("wall_texture", "assets/textures/wall.png");
        if (!wallTex) {
            throw std::runtime_error("[Scene] Warning: Failed to load wall_texture\n");
        }

        auto ballTex = ResourceManager<Graphics::Texture2D>::Load("ball_texture", "assets/textures/ball.png");
        if (!ballTex){
            throw std::runtime_error("[Scene] Warning: Failed to load ball_texture\n");
        }

        if (!std::filesystem::exists(filepath)) {
            std::cerr << "Scene file does not exist: " << filepath << "\n";

            if (Core::Engine::IsInitialized()) {
                Core::Engine::Get().logger().warn("Scene file not found: " + filepath);
            }

            return false;
        }

        if (GameObjectSerializer::loadSceneFromFile(filepath, sceneData))
        {
            std::cout << "[Scene] Loading from file: " << filepath << "\n";
            for (const auto& objData : sceneData.objects)
            {
                auto obj = GameObjectSerializer::deserialize(objData);
                if (obj)
                {
                    m_objects.push_back(std::move(obj));
                }
            }
            std::cout << "[Scene] Loaded " << m_objects.size() << " objects from file\n";
            return true;
        }

        // FALLBACK: Create default test room if file doesn't exist
        std::cout << "[Scene] No scene file found, creating default test room\n";

        // creates a default json if file is not found uncomment once everything is ready

        // auto& factory = PrefabFactory::get();

        //// Create floor
        // auto floor = factory.createAt("Floor", glm::vec2(0, 0));
        // if (floor) {
        //     floor->setSize(glm::vec2(20, 12));
        //     objects().push_back(std::move(floor));
        // }

        //// Create walls
        // auto wall1 = factory.createAt("Wall", glm::vec2(-10, 0));
        // if (wall1) {
        //     wall1->setSize(glm::vec2(1, 12));
        //     objects().push_back(std::move(wall1));
        // }

        // auto wall2 = factory.createAt("Wall", glm::vec2(10, 0));
        // if (wall2) {
        //     wall2->setSize(glm::vec2(1, 12));
        //     objects().push_back(std::move(wall2));
        // }

        // auto wall3 = factory.createAt("Wall", glm::vec2(0, -6));
        // if (wall3) {
        //     wall3->setSize(glm::vec2(20, 1));
        //     objects().push_back(std::move(wall3));
        // }

        // auto wall4 = factory.createAt("Wall", glm::vec2(0, 6));
        // if (wall4) {
        //     wall4->setSize(glm::vec2(20, 1));
        //     objects().push_back(std::move(wall4));
        // }

        //// Create player
        // objects().push_back(factory.createAt("Player", glm::vec2(-6, -2)));

        //// Create enemies
        // objects().push_back(factory.createAt("Enemy", glm::vec2(3, 0)));
        // objects().push_back(factory.createAt("FastEnemy", glm::vec2(5, 2)));

        return true;
    }
    catch (const std::exception& e) {
        std::string errorMsg = std::string("[Scene] CRITICAL ERROR loading '") + m_name + "': " + e.what();
        std::cerr << errorMsg << "\n";

        if (Core::Engine::IsInitialized()) {
            Core::Engine::Get().logger().error(errorMsg);
        }

        return false;
    }
    catch (...) {
        std::string errorMsg = std::string("[Scene] UNKNOWN ERROR loading scene: ") + m_name;
        std::cerr << errorMsg << "\n";

        if (Core::Engine::IsInitialized()) {
            Core::Engine::Get().logger().error(errorMsg);
        }

        return false;
    }
}

void Scene::unload()
{
    std::cout << "[Scene] unload: " << m_name << "\n";

    std::string filepath = "assets/scenes/" + m_name + "_autosave.json";

    std::filesystem::path absPath = std::filesystem::absolute(filepath);
    std::cout << "[Scene] Attempting to save to: " << absPath << "\n";

    saveToFile(filepath);

    m_objects.clear();
}

bool Scene::saveToFile(const std::string &filepath)
{
    SerializedScene sceneData;
    sceneData.sceneName = m_name;

    // Serialize all active objects
    for (const auto &obj : objects())
    {
        if (obj)
        {
            sceneData.objects.push_back(GameObjectSerializer::serialize(*obj));
        }
    }

    return GameObjectSerializer::saveSceneToFile(sceneData, filepath);
}

Gameplay::Physics2D* Scene::getPhysics() {
    return m_physics.get();
}

const Gameplay::Physics2D* Scene::getPhysics() const {
    return m_physics.get();
}

// temp commented out for testing of serializer
// void Scene::update(double dt) {
//     for (auto& o : objects()) {
//         if (o->active()) o->update(dt);
//     }
//     if (m_physics) {
//         m_physics->step(objects(), dt);
//     }
// }

void Scene::update(double dt)
{
    static double timer = 0.0;
    timer += dt;

    // Print positions every 2 seconds
    //if (timer >= 2.0)
    //{
    //    std::cout << "\n=== Object Positions ===\n";
    //    for (auto &o : objects())
    //    {
    //        if (o->active())
    //        {
    //            auto pos = o->transform().position;
    //            std::cout << o->name() << " at (" << pos.x << ", " << pos.y << ")\n";
    //        }
    //    }
    //    timer = 0.0;
    //}
    {
        PROFILE_SCOPE("GameObjectUpdates");
        for (auto& o : objects())
        {
            if (o->active())
                o->update(dt);
        }
    }

    if (m_physics)
    {
        PROFILE_SCOPE("Physics");
        // collision detection
        m_physics->step(objects(), dt);
    }
}

void Scene::render(Graphics::RenderingManager& renderer)
{
    PROFILE_SCOPE("GameObjectRender");
    for (auto &o : objects())
    {
        if (o->active())
            o->render(renderer);
    }
}