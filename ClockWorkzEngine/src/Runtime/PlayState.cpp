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
        m_scenes.update(dt);
    }

    void PlayState::render(Core::Time* timer) {
        if (!m_loaded) return;
        m_scenes.render();
        if (m_debug) {
            /*Implement the IMGUI*/
            ImGui::Begin("Game State");
            ImGui::Text("Testing Window");
            if (ImGui::Button("Back to Menu (ESC)")) {
                m_gsm.pop();
            }
            ImGui::End();
        }
    }

} // namespace Runtime
