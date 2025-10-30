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
#include <Gameplay/Prefab.hpp>
#include "Core/Profiler.hpp" 
#include "Graphics/RenderingManager.hpp"


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
		case T::Ball:   return "Ball";
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

        ResourceManager<Shader>::Load(
            "light",
            "assets/shaders/light.vert",
            "assets/shaders/light.frag"
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
            std::cout << "[PlayState] Returning to menu...\n";
            m_shouldReturnToMenu = true;  //Returns to menu
        }
    }

    void PlayState::onExit() {
        // Only unload if a scene was actually loaded successfully
        if (m_loaded) {
            m_scenes.unloadCurrent();
            m_scenes.unloadCurrent();
        }
        m_loaded = false;

    }

    void PlayState::update(double dt) {

        if (m_shouldReturnToMenu) {
            m_gsm.pop();
            return;
        }

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

            // Syafiq - need find a way to cache the player.or do a unordered_map with object unique name to array of objects.
            // objMap["Player"][0] <- since only 1 player. index 0 will work.
            for (auto& obj : scene->objects()) {
                if (obj->type() == Gameplay::GameObjectType::Player) {
                    float moveSpeed = 100.0f;
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

                    if (m_input.isKeyDown(GLFW_KEY_G)) {
                        obj->transform().scale.x += 10*dt;
                        obj->transform().scale.y += 10 * dt;
                    }


                    Core::Engine& engine = Core::Engine::Get();
                    Platform::InputManager& input = engine.input();
                    Graphics::Camera2D& cam = engine.camera();
                    Platform::WindowManager& window = engine.window();

                    glm::vec2 mouseWorld = input.getMouseWorldPos(cam,
                        static_cast<float>(window.width()),
                        static_cast<float>(window.height()));

                    // Player world position
                    glm::vec2 playerPos = obj->transform().position;

                    // Direction vector from player to mouse
                    glm::vec2 dir = mouseWorld - playerPos;

                    // Distance check to prevent jitter
                    float dist = glm::length(dir);
                    if (dist > 10.0f) { // 10 units = threshold distance
                        // Normalize direction
                        dir = glm::normalize(dir);

                        // Compute angle in radians — atan2(y, x)
                        float angleRad = std::atan2(dir.y, dir.x);

                        // Convert to degrees if your transform uses degrees
                        obj->transform().rotationRad = angleRad;

                    }
                    if (obj->type() == Gameplay::GameObjectType::Player && !obj->active()) {
						obj->respawnAt(glm::vec2(0.0f, 0.0f), true, true);
                    }

                }
                if (obj->type() == Gameplay::GameObjectType::Ball) {
                    obj->velocity().y = 0.0f;
                    obj->velocity().x += m_ballDir * m_ballSpeed;

                    // Predict next X and flip if we would cross a bound
                    float nextX = obj->transform().position.x + obj->velocity().x * static_cast<float>(dt);
                    if (nextX < m_leftBound) { nextX = m_leftBound;  m_ballDir = +1; }
                    if (nextX > m_rightBound) { nextX = m_rightBound; m_ballDir = -1; }

                    obj->transform().position.x = nextX; // snap to keep within bounds (optional)
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

                    auto obj = factory.createAt("Wall", glm::vec2(x, y));
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
        // why render twice. not required
       /* if (auto* scene = m_scenes.current()) {
            scene->render(*m_renderer);
        }*/

		// Use SceneManager's render to render the current scene
        m_scenes.render(*m_renderer);

        // ==================== Torch Light Rendering ====================
        Core::Engine& engine = Core::Engine::Get();
        auto* scene = m_scenes.current();
        if (!scene) return;

        auto* physics = scene->getPhysics();
        if (physics) {
            std::vector<std::unique_ptr<Gameplay::GameObject>>& objs = scene->objects();

            // Find the player (you can cache this later)
            Gameplay::GameObject* player = nullptr;
            for (auto& o : objs) {
                if (o->type() == Gameplay::GameObjectType::Player) {
                    player = o.get();
                    break;
                }
            }
            if (!player) return;

            glm::vec2 origin = player->transform().position;
            float angle = player->transform().rotationRad;

            // Compute visibility polygon
            std::vector<glm::vec2> lightPoly = m_renderer->computeVisibilityPolygon(
                origin,
                angle,
                glm::radians(60.0f), // FOV in radians (torch cone width)
                300.0f,              // radius
                *physics,
                objs
            );

            // Draw the light cone (via RenderingManager)
            auto shader = Assets::ResourceManager<Graphics::Shader>::Get("light");
            if (!shader) return;

            shader->use();
            shader->setVec3("uColor", glm::vec3(1.0f, 0.9f, 0.6f)); // optional
            shader->setVec2("uLightPos", origin);
            shader->setFloat("uRadius", 600.0f);
            shader->setMat4("uProjection", engine.camera().viewProj());
            m_renderer->drawLightCone(origin, lightPoly, glm::vec3(1.0f, 0.9f, 0.6f));
        }
        // ===============================================================


        // In PlayState::render() method:
        if (m_debug) {
            ImGui::Begin("Game State");
            ImGui::Text("Testing Window");

            if (timer) {
                double fps = timer->fps();;
                double frameTime = timer->delta() * 1000.0;

                ImGui::Text("FPS: %.1f", fps);
                ImGui::Text("Frame Time: %.2f ms", frameTime);
            }

            ImGui::Separator();

            if (ImGui::Button("Show Performance Profile")) {
                Core::Profiler::Get().print();
            }

            if (ImGui::Button("Reset Profiler")) {
                Core::Profiler::Get().reset();
            }


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

            if (ImGui::Button("Spawn 2500 Objects Gradually")) {
                m_spawning = true;
                m_spawnedCount = 0;
            }

            if (ImGui::Button("Back to Menu (ESC)")) {
                m_gsm.pop();
            }

            if (ImGui::Button("Reset Scene")) {
                resetScene();
            }

            ImGui::End();
        }


        //ImDrawList* dl = ImGui::GetForegroundDrawList();

        //const ImVec2 center{ 640.0f, 360.0f };
        //const float radius = 60.0f;
        //const int segments = 48; // More segments = smoother circle
        //const float thickness = 3.0f;

        //dl->AddCircle(center, radius, IM_COL32(255, 0, 0, 255), segments, thickness);
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
