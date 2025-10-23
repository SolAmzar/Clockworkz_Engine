/******************************************************************************/
/*!
\file           PlayState.cpp
\project        Lights Out
\author(s)      [Lebon], 75% [Syafiq] 15%
\brief          Declares PlayState, the main gameplay loop state.
                Responsibilities include:
                - Loading and managing the current Scene and its GameObjects
                - Updating gameplay logic such as movement, collisions, and AI
                - Rendering the game world each frame (Game Object Render not done yet)
                - Transitioning back to other states (e.g., pause or menu)

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "Runtime/SceneManager.hpp"
#include "Runtime/Scene.hpp"
#include "Gameplay/GameObject.hpp"
#include "IMGUI/imgui.h"
#include "Runtime/MenuState.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Platform/InputManager.hpp"
#include "Platform/WindowManager.hpp"
#include "Runtime/PlayState.hpp"
#include "Core/AudioManager.hpp"
#include <GLFW/glfw3.h>
#include "Assets/FontManager.hpp"
#include "Assets/ResourceManager.hpp"
#include "Graphics/Shader.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <memory>
#include <Core/Time.hpp>
#include <Graphics/Renderer2D.hpp>
#include <Graphics/Camera2D.hpp>
#include <Graphics/GraphicsManager.hpp>
#include <Core/Engine.hpp>
#include <Gameplay/Components/Prefab.hpp>


using namespace Graphics;
using namespace Assets;

namespace Runtime {
    PlayState::PlayState(std::string initialSceneId, Platform::InputManager& input, GameStateManager& gsm)
        : m_initialSceneId(std::move(initialSceneId)), m_input(input), m_gsm(gsm) {
    }

    const char* PlayState::name() const { return "PlayState"; }

    static const char* toString(Gameplay::GameObjectType t) {
        using T = Gameplay::GameObjectType;
        switch (t) {
        case T::Floor:  return "Floor";
        case T::Wall:   return "Wall";
        case T::Player: return "Player";
        case T::Enemy:  return "Enemy";
        case T::Trap:   return "Trap";
        default:        return "Unknown";
        }
    }

    void PlayState::onEnter() {
        std::cout << "[PlayState] Enter, loading scene: " << m_initialSceneId << "\n";

        //Syafiq - 
        Core::Engine& engine = Core::Engine::Get();
        m_renderer = &engine.rendering();

        auto textureShader = ResourceManager<Shader>::Load(
            "texture",
            "assets/shaders/texture.vert",
            "assets/shaders/texture.frag"
        );
        if (m_scenes.loadScene(m_initialSceneId)) {
            m_loaded = true;

            // Print all GameObjects created by the loaded scene
            if (auto* scene = m_scenes.current()) {
                auto& objs = scene->objects();
                std::cout << "GameObjects created (" << objs.size() << "):\n";
                for (auto const& u : objs) {
                    auto const& o = *u;
                    auto p = o.transform().position;
                    auto s = o.size();
                    std::cout << " - id=" << o.id()
                        << " type=" << toString(o.type())
                        << " name=\"" << o.name() << "\""
                        << " pos=(" << p.x << "," << p.y << ")"
                        << " size=(" << s.x << "," << s.y << ")"
                        << (o.active() ? "" : " [inactive]") << "\n";
                }
                std::cout.flush();
            }
        }
        else {
            std::cout << "[PlayState] Scene load FAILED.\n";
        }
    }

    void PlayState::onExit() {
        m_scenes.unloadCurrent();
        m_loaded = false;
    }

    void PlayState::update(double dt) {
        if (!m_loaded) return;

        if (m_input.wasKeyPressed(GLFW_KEY_ESCAPE)) {
            m_gsm.pop();
            return;
        }
        if (m_input.wasKeyPressed(GLFW_KEY_0)) {
            m_debug = !m_debug;
            if (m_debug) {
                std::cout << "Debug is pressed\n";
            }
            return;
        }

        //Syafiq - Player movement
        if (auto* scene = m_scenes.current()){
            for (auto& obj : scene->objects()) {
                if (obj->type() == Gameplay::GameObjectType::Player) {
                    float moveSpeed = 10.0f;
                    float moveAmount = moveSpeed * static_cast<float>(dt);

                    if (m_input.isKeyDown(GLFW_KEY_W)) {
                        obj->velocity().y = moveSpeed;
                    }
                    else if(m_input.isKeyDown(GLFW_KEY_S)) {
                        obj->velocity().y = -moveSpeed;
                    }
                    else {
                        obj->velocity().y = 0;
                    }

                    if (m_input.isKeyDown(GLFW_KEY_A)) {
                        obj->velocity().x = -moveSpeed;
                    }
                    else if (m_input.isKeyDown(GLFW_KEY_D)) {
                        obj->velocity().x = moveSpeed;
                    }
                    else {
                        obj->velocity().x = 0;
                    }


                    break;
                }
            }
        }


        Core::Engine& engine = Core::Engine::Get();
        Graphics::Camera2D& cam = engine.camera();

        if (m_input.isKeyDown(GLFW_KEY_Q)) {
            cam.setZoom(cam.zoom() * 1.02f);
        }
        if (m_input.isKeyDown(GLFW_KEY_E)) {
            cam.setZoom(cam.zoom() / 1.02f);
        }

        if (m_spawning && m_spawnedCount < 2500) {
            auto& factory = Gameplay::PrefabFactory::get();
            auto currentScene = m_scenes.currentScene();

            if (currentScene) {
                int toSpawn = std::min(SPAWN_PER_FRAME, 2500 - m_spawnedCount);

                for (int i = 0; i < toSpawn; ++i) {
                    int gridSize = 50;
                    float spacing = 2.0f;
                    int idx = m_spawnedCount + i;
                    int row = idx / gridSize;
                    int col = idx % gridSize;

                    float x = -gridSize * spacing / 2.0f + col * spacing;
                    float y = -gridSize * spacing / 2.0f + row * spacing;

                    auto obj = factory.createAt("Enemy", glm::vec2(x, y));
                    if (obj) {
                        currentScene->objects().push_back(std::move(obj));
                    }
                }

                m_spawnedCount += toSpawn;
                if (m_spawnedCount >= 2500) {
                    m_spawning = false;
                    std::cout << "Finished spawning 2500 objects!\n";
                }
            }
        }
        

        m_scenes.update(dt);
    }

    void PlayState::render(Core::Time* timer) {
        if (!m_loaded || !m_renderer) return;

        //Syafiq
        if (auto* scene = m_scenes.current()) {
            scene->render(*m_renderer);
        }

        m_scenes.render(*m_renderer);

        // In PlayState::render() method:
        if (m_debug) {
            ImGui::Begin("Game State");
            ImGui::Text("Testing Window");

            // Add button to spawn 2500 objects
            if (ImGui::Button("Spawn 2500 Objects")) {
                // Get the current scene using the existing method
                auto& factory = Gameplay::PrefabFactory::get();
                auto currentScene = m_scenes.currentScene(); // Returns std::shared_ptr<Scene>
                if (currentScene) {
                    // Spawn objects in a grid pattern
                    int gridSize = 50; // 50x50 = 2500 objects
                    float spacing = 2.0f;
                    float startX = -gridSize * spacing / 2.0f;
                    float startY = -gridSize * spacing / 2.0f;

                    for (int i = 0; i < gridSize; ++i) {
                        for (int j = 0; j < gridSize; ++j) {
                            float x = startX + i * spacing;
                            float y = startY + j * spacing;

                            // Create an object using your prefab system
                            auto obj = factory.createAt("Enemy", glm::vec2(x, y));
                            if (obj) {
                                currentScene->objects().push_back(std::move(obj));
                            }
                        }
                    }
                    std::cout << "Spawned 2500 objects!\n";
                }
            }

            if (ImGui::Button("Back to Menu (ESC)")) {
                m_gsm.pop();
            }
            ImGui::End();
        }

        if (ImGui::Button("Spawn 2500 Objects Gradually")) {
            m_spawning = true;
            m_spawnedCount = 0;
        }
    }

    void PlayState::resetScene() {
        if (m_scenes.reloadCurrent()) {
            std::cout << "[PlayState] Scene reset Success\n";
        }
        else {
            m_scenes.loadScene(m_initialSceneId);
        }
        
    }
} // namespace Runtime
