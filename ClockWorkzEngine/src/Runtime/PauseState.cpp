/******************************************************************************/
/*!
\file           PauseState.cpp
\project        Lights Out
\author(s)      [Syafiq], 100%
\brief          Basic pause menu implementation

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "Runtime/PauseState.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Runtime/PlayState.hpp"
#include "Platform/InputManager.hpp"
#include "Platform/WindowManager.hpp"
#include "Core/AudioManager.hpp"
#include "Assets/FontManager.hpp"
#include "Assets/ResourceManager.hpp"
#include "Graphics/Shader.hpp"
#include "Core/Engine.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Camera2D.hpp"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "IMGUI/imgui.h"
#include <iostream>

using Assets::ResourceManager;
using Graphics::Shader;

namespace Runtime {

    PauseState::PauseState(GameStateManager& gsm,Platform::InputManager& input)
        : m_gsm(gsm), m_input(input)
    {
    }

    void PauseState::onEnter()
    {
        m_selected = 0;
        std::cout << "[PauseState] Entered\n";

        Core::Engine& engine = Core::Engine::Get();
        m_renderer = &engine.rendering();
        m_cam = &engine.camera();


        std::cout << "[PauseState] Ready\n";
    }

    void PauseState::moveUp()
    {
        if (--m_selected < 0)
            m_selected = static_cast<int>(m_items.size()) - 1;
    }

    void PauseState::moveDown()
    {
        if (++m_selected >= static_cast<int>(m_items.size()))
            m_selected = 0;
    }

    void PauseState::select()
    {
        const std::string& choice = m_items[m_selected];

        if (choice == "Resume Game")
        {
            std::cout << "[PauseState] Resuming...\n";
            m_gsm.pop();
            return;
        }

        if (choice == "Main Menu")
        {
            std::cout << "[PauseState] Exiting...\n";
            m_gsm.pop();
            m_gsm.pop();
        }

        std::cout << "[PauseState] Selected: " << choice << "\n";


    }

    void PauseState::update(double dt)
    {
        (void)dt;

        // ESC to resume
        if (m_input.wasKeyPressed(GLFW_KEY_ESCAPE))
        {
            std::cout << "[PauseState] ESC pressed - resuming\n";
            m_gsm.pop();
            return;
        }

        // Keyboard navigation
        if (m_input.wasKeyPressed(GLFW_KEY_UP) || m_input.wasKeyPressed(GLFW_KEY_W))
        {
            moveUp();
        }
        if (m_input.wasKeyPressed(GLFW_KEY_DOWN) || m_input.wasKeyPressed(GLFW_KEY_S))
        {
            moveDown();
        }

        if (m_input.wasKeyPressed(GLFW_KEY_ENTER) || m_input.wasKeyPressed(GLFW_KEY_KP_ENTER))
        {
            select();
        }

        if (m_input.wasKeyPressed(GLFW_KEY_F1))
        {
            m_debug = !m_debug;
        }
    }

    void PauseState::render(Core::Time* timer)
    {
        Core::Engine& engine = Core::Engine::Get();
        Platform::WindowManager& window = engine.window();
        Graphics::RenderingManager& renderer = engine.rendering();

        int w = window.width();
        int h = window.height();

        glm::mat4 projection = glm::ortho(0.0f, (float)w, 0.0f, (float)h);

        m_renderer->beginFontPass(projection);

        auto shader = ResourceManager<Shader>::Get("text");
        if (!shader) {
            m_renderer->endFontPass();
            return;
        }
        auto pauseFont = ResourceManager<Assets::FontManager>::Get("PauseMenu");
        if (!pauseFont) {
            Assets::ResourceManager<Assets::FontManager>::Load(
                "PauseMenu", "assets/fonts/ANUNEDW_.TTF", 48);
            pauseFont = ResourceManager<Assets::FontManager>::Get("PauseMenu");
        }

        if (pauseFont) {
            pauseFont->RenderText("PAUSED", w / 2.0f - 100.0f, h - 150.0f,
                1.0f, glm::vec3(1.0f, 1.0f, 0.0f), *shader);
        }

        // Render menu options
        glm::vec3 normalCol(0.6f, 0.6f, 0.6f);
        glm::vec3 selectedCol(1.0f, 1.0f, 1.0f);

        float startY = h / 2.0f + 100.0f;

        if (pauseFont) {
            for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
            {
                const std::string& item = m_items[i];
                glm::vec3 color = (i == m_selected) ? selectedCol : normalCol;

                float yPos = startY - (i * 80.0f);
                float textWidth = item.length() * 20.0f;
                float xPos = (w / 2.0f) - (textWidth / 2.0f);

                // ACTUALLY RENDER THE TEXT
                pauseFont->RenderText(item, xPos, yPos, 0.8f, color, *shader);
            }
        }

        renderer.endFontPass();

        (void)timer;
    }

    void PauseState::renderImGui(Core::Time* timer)
    {
        if (!m_debug) return;

        ImGui::Begin("Pause State Debug");
        ImGui::Text("Press ESC to Resume");
        ImGui::Text("Selected: %d - %s", m_selected, m_items[m_selected].c_str());
        ImGui::End();

        (void)timer;
    }

    void PauseState::onExit()
    {
        std::cout << "[PauseState] Exiting\n";
        std::cout << "[PauseState] Exit complete\n";
    }

} // namespace Runtime