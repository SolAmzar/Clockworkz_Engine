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
        

        m_scenes.update(dt);
    }

    void PlayState::render(Core::Time* timer) {
        if (!m_loaded || !m_renderer) return;

        //Syafiq
        // why render twice. not required
       /* if (auto* scene = m_scenes.current()) {
            scene->render(*m_renderer);
        }*/

        m_scenes.render(*m_renderer);

        if (m_debug) {
            /*Implement the IMGUI*/
            ImGui::Begin("Game State");
            ImGui::Text("Testing Window");

            if (ImGui::Button("Reset Scene")) {
                resetScene();
            }

            if (ImGui::Button("Back to Menu (ESC)")) {
                m_gsm.pop();
            }

            ImGui::End();
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
