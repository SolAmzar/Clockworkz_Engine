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
#include "Gameplay/Serializer.hpp" // ADD THIS
#include "Gameplay/Prefab.hpp"     // ADD THIS
#include <iostream>
#include <exception>
#include "Assets/ResourceManager.hpp"
#include <filesystem>
#include "Graphics/Texture2D.hpp"
#include "Core/Engine.hpp"
#include <Core/Profiler.hpp>
#include <Assets/TextureManager.hpp>

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
    try
    {
        // Preload common textures
        std::cout << "[Scene] Loading textures...\n";

        auto floorTex = Assets::TextureManager::Get("floor_texture");
        auto wallTex = Assets::TextureManager::Get("wall_texture");
        auto enemyTex = Assets::TextureManager::Get("enemy_texture");
        auto playerTex = Assets::TextureManager::Get("player_texture");
        auto ballTex = Assets::TextureManager::Get("ball_texture");

        if (!playerTex)
        {
            Core::Engine::Get().logger().warn("Failed to get player_texture");
        }
        if (!enemyTex)
        {
            Core::Engine::Get().logger().warn("Failed to get enemyTex_texture");
        }
        if (!floorTex)
        {
            Core::Engine::Get().logger().warn("Failed to get floor_texture");
        }

        if (!wallTex)
        {
            Core::Engine::Get().logger().warn("Failed to get wall_texture");
        }
        if (!ballTex)
        {
            Core::Engine::Get().logger().warn("Failed to get ball_texture");
        }

        std::cout << "[Scene] load: " << m_name << "\n";

        std::string filepath = "assets/scenes/" + m_name + ".json";

        if (!std::filesystem::exists(filepath))
        {
            std::cerr << "Scene file does not exist: " << filepath << "\n";

            if (Core::Engine::IsInitialized())
            {
                Core::Engine::Get().logger().warn("Scene file not found: " + filepath);
            }

            return false;
        }

        std::string loadedSceneName;
        if (GameObjectSerializer::loadSceneFromFile(filepath, loadedSceneName, m_objects))
        {
            std::cout << "[Scene] Loaded scene '" << loadedSceneName << "' from file: " << filepath << "\n";
            std::cout << "[Scene] Loaded " << m_objects.size() << " objects\n";
            return true;
        }

        return true;
    }
    catch (const std::exception &e)
    {
        std::string errorMsg = std::string("[Scene] CRITICAL ERROR loading '") + m_name + "': " + e.what();
        std::cerr << errorMsg << "\n";

        if (Core::Engine::IsInitialized())
        {
            Core::Engine::Get().logger().error(errorMsg);
        }

        return false;
    }
    catch (...)
    {
        std::string errorMsg = std::string("[Scene] UNKNOWN ERROR loading scene: ") + m_name;
        std::cerr << errorMsg << "\n";

        if (Core::Engine::IsInitialized())
        {
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
    // ⭐ NEW API: Save using the new serializer
    return GameObjectSerializer::saveSceneToFile(m_name, m_objects, filepath);
}

Gameplay::Physics2D *Scene::getPhysics()
{
    return m_physics.get();
}

const Gameplay::Physics2D *Scene::getPhysics() const
{
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
    {
        PROFILE_SCOPE("GameObjectUpdates");
        std::vector<std::unique_ptr<Gameplay::GameObject>>& objs = objects();
        for (auto& o : objs){
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

void Scene::render(Graphics::RenderingManager &renderer)
{
    PROFILE_SCOPE("GameObjectRender");
    std::vector<std::unique_ptr<Gameplay::GameObject>>& objs = objects();
    for (auto &o : objs)
    {
        if (o->active())
            o->render(renderer);
    }
}