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
#include "Audio/AudioManager.hpp"
#include "Assets/FontManager.hpp"
#include "Assets/ResourceManager.hpp"
#include "Graphics/Shader.hpp"
#include "Core/Engine.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Camera2D.hpp"
#include <Gameplay/Components/SpriteRenderer.hpp>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "IMGUI/imgui.h"
#include <iostream>

using Assets::ResourceManager;
using Graphics::Shader;
using namespace Gameplay;

namespace Runtime {

    PauseState::PauseState(GameStateManager& gsm, Platform::InputManager& input)
        : m_gsm(gsm),
        m_input(input),
        m_background(GameObjectType::Unknown, "PauseBG", GameObjectShape::Rectangle)
    {
    }

    void PauseState::onEnter()
    {
        m_selected = 0;
        std::cout << "[PauseState] Entered\n";

        Core::Engine& engine = Core::Engine::Get();
        m_renderer = &engine.rendering();
        m_cam = &engine.camera();

        m_background.addComponent<SpriteRendererComponent>("pause_bg_texture");
        m_background.setSize({ 1.f, 1.f });                    // Proper dimensions
        m_background.transform().scale = { 1920.0f, 1080.0f }; // Keep scale neutral
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
        // Get window reference for mouse position
        Core::Engine& engine = Core::Engine::Get();
        Platform::WindowManager& window = engine.window();

        // Mouse hover detection
        double mouseX, mouseY;
        glfwGetCursorPos(window.getWindow(), &mouseX, &mouseY);

        int w = window.width();
        int h = window.height();
        float glMouseY = h - static_cast<float>(mouseY);  // Convert to OpenGL coordinates

        // Check if mouse is hovering over any menu item
        bool isHover = false;
        float startY = h / 2.0f + 80.0f;  // Same as render function

        for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
        {
            float scale = 0.55f;
            float yPos = startY - (i * 55.0f);  // Same as render function
            float textWidth = m_items[i].length() * 48.0f * scale * 0.55f;  // Same calculation as render
            float xPos = (w / 2.0f) - (textWidth / 2.0f);

            float textHeight = 48.0f * scale;  // Approximate height based on font size and scale

            // Check if mouse is within this menu item's bounds
            if (mouseX >= xPos && mouseX <= (xPos + textWidth) &&
                glMouseY >= yPos && glMouseY <= (yPos + textHeight))
            {
                isHover = true;
                if (m_selected != i)
                {
                    m_selected = i;
                    // Optional: Add sound effect here if you have audio manager
                    // m_audio.playOneShot("ui_move", Audio::Group::SFX, 0.9f);
                }
                break;
            }
        }

        // Mouse click to select
        if (isHover && m_input.wasMouseClicked(GLFW_MOUSE_BUTTON_LEFT))
        {
            // Optional: Add sound effect here
            // m_audio.playOneShot("ui_select", Audio::Group::SFX, 1.0f);
            select();
        }

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

        m_renderer->beginScenePass(m_cam->viewProj());
        m_background.render(*m_renderer);
        m_renderer->endScenePass();

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

        // Colors matching the scribble/sticky note aesthetic
        glm::vec3 normalCol(0.7f, 0.5f, 0.5f);      // Medium brown (unselected)
        glm::vec3 selectedCol(0.20f, 0.15f, 0.12f);    // Dark brown (selected - like pen ink)

        if (pauseFont) {
            // "PAUSED" title - brown color to match scribbles, smaller and centered
            std::string title = "PAUSED";
            float titleScale = 1.0f;
            float titleWidth = title.length() * 48.0f * titleScale * 0.55f;  // Better estimation
            float titleX = (w / 2.0f) - (titleWidth / 2.0f)- 10.0f;
            pauseFont->RenderText(title, titleX, h - 150.0f, titleScale, glm::vec3(0.5f, 0.4f, 0.3f), *shader);
        }

        // Render menu options - smaller, centered, matching aesthetic
        float startY = h / 2.0f + 80.0f;

        if (pauseFont) {
            for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
            {
                const std::string& item = m_items[i];
                glm::vec3 color = (i == m_selected) ? selectedCol : normalCol;

                float scale = 0.55f;  // Smaller text
                float yPos = startY - (i * 55.0f);
                float textWidth = item.length() * 48.0f * scale * 0.55f;  // Better width estimation
                float xPos = (w / 2.0f) - (textWidth / 2.0f);

                pauseFont->RenderText(item, xPos, yPos, scale, color, *shader);
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