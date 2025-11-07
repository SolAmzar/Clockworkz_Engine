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
#include "Graphics/LayerManager.hpp"
#include "Gameplay/Physics2D.hpp"
#include "Gameplay/Serializer.hpp" // ADD THIS
#include "Gameplay/Prefab.hpp"     // ADD THIS
#include <iostream>
#include <exception>
#include <cassert>
#include "Assets/ResourceManager.hpp"
#include <filesystem>
#include "Graphics/Texture2D.hpp"
#include "Core/Engine.hpp"
#include <Core/Profiler.hpp>
#include <Assets/TextureManager.hpp>
#include <filesystem>

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

static void DebugDumpLayerCounts(Gameplay::LayerManager const& layers) {
    for (uint32_t bit : Gameplay::kOrderedBits) {
        auto const& v = layers.objects_in(bit);
        std::cout << "[Layer] bit=" << bit << " count=" << v.size() << "\n";
    }
}

Gameplay::GameObject* Scene::findByName(std::string const& name) {
    for (auto& u : m_objects) if (u && u->name() == name) return u.get();
    return nullptr;
}
Gameplay::GameObject* Scene::findFirstByLayer(int layer) {
    for (auto& u : m_objects) if (u && static_cast<int>(u->layer()) == layer) return u.get();
    return nullptr;
}

void Scene::wireLayerCallback(GameObject* o) {
    // When the object's draw layer changes, we reindex it in LayerManager.
    // LayerManager::onLayerChanged expects the GameObject* and the new layer value.
    o->setLayerChangedCallback([this](GameObject* who) {
        // Provide the current/new layer as the second argument.
        m_layers.onLayerChanged(who, who->layer());
    });
}

void Scene::registerExistingObjectsToLayers() {
    m_layers.clear();
    for (auto& up : m_objects) {
        // Ensure the object notifies the LayerManager when its layer changes.
        wireLayerCallback(up.get());
        // Add the object to the LayerManager with its current layer.
        m_layers.add(up.get(), up->layer());
    }
}

void Scene::addObject(std::unique_ptr<GameObject> go) {
    // Wire the layer-changed callback before adding so LayerManager stays consistent.
    wireLayerCallback(go.get());
    // Add to LayerManager with the object's current layer.
    m_layers.add(go.get(), go->layer());
    // Transfer ownership into the scene.
    m_objects.push_back(std::move(go));
}

void Scene::destroyObject(GameObject* go) {
    if (!go) return;
    m_layers.remove(go);
    // also erase from owning vector
    auto it = std::remove_if(m_objects.begin(), m_objects.end(),
        [&](std::unique_ptr<GameObject>& p) { return p.get() == go; });
    m_objects.erase(it, m_objects.end());
}

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

#ifndef _DEBUG
        filepath = "assets/scenes/" + m_name + ".json";
#endif
        std::cout << std::filesystem::current_path().string() << "\n";
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
            registerExistingObjectsToLayers();
            DebugDumpLayerCounts(m_layers);
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
 //void Scene::update(double dt) {
 //    for (auto& o : objects()) {
 //        if (o->active()) o->update(dt);
 //    }
 //    if (m_physics) {
 //        m_physics->step(objects(), dt);
 //    }
 //}

void Scene::update(double dt)
{
    {
        PROFILE_SCOPE("GameObjectUpdates");
        m_layers.for_each_layered_update([&](Gameplay::GameObject* o, uint32_t bit) {
            if (bit != Gameplay::DrawLayer::UI && o->active())
                o->update(dt);
            });
    }

    if (m_physics)
    {
        PROFILE_SCOPE("Physics");
        m_physics->step(objects(), dt); // or m_layers.all_objects()
    }
}

void Scene::render(Graphics::RenderingManager& renderer)
{
    uint32_t lastBit = 0;
    PROFILE_SCOPE("GameObjectRender");
    m_layers.for_each_layered_render([&](Gameplay::GameObject* o, uint32_t bit) {
        assert(bit >= lastBit && "LayerManager: render order regressed!");
        lastBit = bit;
        if (o->active()) o->render(renderer);
        });
}





//void Scene::update(double dt)
//{
//    {
//        PROFILE_SCOPE("GameObjectUpdates");
//        std::vector<std::unique_ptr<Gameplay::GameObject>>& objs = objects();
//        for (auto& o : objs) {
//            if (o->active())
//                o->update(dt);
//        }
//    }
//
//    if (m_physics)
//    {
//        PROFILE_SCOPE("Physics");
//        // collision detection
//        m_physics->step(objects(), dt);
//    }
//}
//
//void Scene::render(Graphics::RenderingManager& renderer)
//{
//    PROFILE_SCOPE("GameObjectRender");
//    std::vector<std::unique_ptr<Gameplay::GameObject>>& objs = objects();
//    for (auto& o : objs)
//    {
//        if (o->active())
//            o->render(renderer);
//    }
//}