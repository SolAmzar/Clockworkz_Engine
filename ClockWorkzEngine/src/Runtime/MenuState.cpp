/******************************************************************************/
/*!
\file           MenuState.cpp
\project        Lights Out
\author(s)      [Lebon], 70%, [Amirul Syafiq] 30%
\brief          Declares MenuState, the state representing the game�s main menu.
                Responsibilities include:
                - Displaying menu options such as Start, Options, Credits, Exit
                - Handling keyboard and mouse input for menu navigation
                - Triggering state transitions (e.g., moving from menu to PlayState)
                - Rendering UI text using FontManager and Shader resources

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "Runtime/MenuState.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Platform/InputManager.hpp"
#include "Platform/WindowManager.hpp"
#include "Runtime/PlayState.hpp"
#include "Audio/AudioManager.hpp"
#include <GLFW/glfw3.h>
#include "Assets/FontManager.hpp"
#include "Assets/ResourceManager.hpp"
#include "Graphics/Shader.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "IMGUI/imgui.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <memory>
#include <sstream>
#include "Core/Time.hpp"
#include <Core/Engine.hpp>
#include <Graphics/Renderer2D.hpp>
#include <Graphics/Camera2D.hpp>
#include <Graphics/GraphicsManager.hpp>
#include "Graphics/RenderingManager.hpp"

#include <Gameplay/Components/SpriteRenderer.hpp>
#include <Runtime/SpriteState.hpp>

using Assets::ResourceManager;
using Graphics::Shader;
using namespace Gameplay;

namespace Runtime
{

    static void ClearConsole()
    {
        std::cout << "\x1B[2J\x1B[H"; // ANSI clear + home
    }

    MenuState::MenuState(GameStateManager &gsm, Platform::InputManager &input, Platform::WindowManager &window, Audio::AudioManager &audio)
        : m_gsm(gsm), m_input(input), m_window(window), m_audio(audio),
        m_background(GameObjectType::Unknown, "MenuBG", GameObjectShape::Rectangle), m_logo(GameObjectType::Unknown, "Logo", GameObjectShape::Rectangle)
    {
    }

    void MenuState::onEnter()
    {
        m_selected = 0;
        m_dirty = true;

        Core::Engine &engine = Core::Engine::Get();
        m_renderer = &engine.rendering();
        m_cam = &engine.camera();
        if (m_items.empty())
        {
            m_items = {"Start Game",
                       "Load Game",
                       "Option",
                       "Credits",
                       "Quit Game"};
        }

        // m_audio.stopGroup(Audio::Group::Music);
        // printMenu();

        Assets::ResourceManager<Assets::FontManager>::Load(
            "Title", "assets/fonts/MANIC1.ttf", 48);

        Assets::ResourceManager<Assets::FontManager>::Load(
            "Subtitle", "assets/fonts/MANIC2.TTF", 72);

        Assets::ResourceManager<Assets::FontManager>::Load(
            "Debug", "assets/fonts/MANIC1.TTF", 24);

        /* Syafiq*/
        Assets::ResourceManager<Assets::FontManager>::Load(
            "Selection", "assets/fonts/MANIC1.TTF", 60);

        auto textShader = ResourceManager<Shader>::Load(
            "text",
            "assets/shaders/text.vert",
            "assets/shaders/text.frag");

        auto textureShader = ResourceManager<Shader>::Load(
            "texture",
            "assets/shaders/texture.vert",
            "assets/shaders/texture.frag");
        // Amzar
        m_bgmChannel = m_audio.play("menu_bgm", Audio::Group::BGM, m_bgmVol, false);
        
  
        
        m_background.addComponent<SpriteRendererComponent>("menu_bg_texture");
        SpriteRendererComponent* bgSprite = m_background.getComponent<SpriteRendererComponent>();
        bgSprite->setIsUI(true);
        m_background.setSize({1.f, 1.f});                    // Proper dimensions

        m_logo.addComponent<SpriteRendererComponent>("logo_texture");
        SpriteRendererComponent* logoSprite = m_logo.getComponent<SpriteRendererComponent>();
        logoSprite->setIsUI(true);
        m_logo.setSize({ 1.f, 1.f });


        updateLayout(m_window.width(), m_window.height());

        // Update layout when window size change
        Core::Engine::Get().events().subscribe<Core::WindowResizeEvent>(
            this,
            [this](const Core::WindowResizeEvent& e) {
                if (e.width > 0 && e.height > 0) {
                    updateLayout(e.width, e.height);
                }
            }
        );
    }

    void MenuState::printMenu()
    {
        ClearConsole();
        std::cout << "Menu:\n";
        for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
        {
            std::cout << (i == m_selected ? "> " : "  ") << m_items[i] << "\n";
        }
        std::cout.flush();
        m_dirty = false;
    }

    void MenuState::moveUp()
    {
        if (--m_selected < 0)
            m_selected = static_cast<int>(m_items.size()) - 1;
        m_dirty = true;
    }
    void MenuState::moveDown()
    {
        if (++m_selected >= static_cast<int>(m_items.size()))
            m_selected = 0;
        m_dirty = true;
    }

    void MenuState::select()
    {
        const std::string &choice = m_items[m_selected];

        if (choice == "Start Game")
        {
            //*** DO NOT REMOVE! Working on SwitchTo GSM*****
            //m_gsm.switchTo(std::make_shared<PlayState>("Level_01", m_input, m_gsm));

            m_gsm.push(std::make_shared<PlayState>("Level_01", m_input, m_gsm));
            return;
        }
        if (choice == "Exit Game")
        {
            std::cout << "Quiting game...\n";

            //**** Exit game needs work *****
            Core::Engine::Get().events().publish(Core::QuitGameEvent());
            return;
        }
        // yusuf
        if (choice == "Load Game")
        {
            m_gsm.push(std::make_shared<PlayState>("Level_01_autosave", m_input, m_gsm));
            // might want to create a text to say that there is no autosavefile
            return;
        }

        std::cout << "[" << choice << "] selected (no state change).\n";
    }

    void MenuState::updateLayout(int w, int h)
    {
        // Resize background to fill screen
        float targetW = (float)w;
        float targetH = (float)h;

        float scaleX = (float)w / 1920.0f;
        float scaleY = (float)h / 1080.0f;

        m_positions[0] = { 390.0f * scaleX, h - (340.0f * scaleY), scaleX * 1.f };
        m_positions[1] = { 200.0f * scaleX, h - (480.0f * scaleY), scaleX * 1.f };
        m_positions[2] = { 230.0f * scaleX, h - (660.0f * scaleY), scaleX * 1.f };
        m_positions[3] = { 520.0f * scaleX, h - (700.0f * scaleY), scaleX * 0.6f };
        m_positions[4] = { 270.0f * scaleX, h - (830.0f * scaleY), scaleX * 0.6f };



        // center it (optional)
        m_background.transform().position = {
            0.0f,
            0.0f
        };

        m_background.transform().scale.x = targetW;
        m_background.transform().scale.y = targetH;
        std::cout << "bgX: " << m_background.transform().scale.x << ", bgY: " << m_background.transform().scale.y << std::endl;

        // === LOGO SCALING ===
        float desiredLogoHeight = 400.0f;      // You can change this anytime
        float desiredLogoWidth = desiredLogoHeight; // square logo

        // Center horizontally
        float logoX = (w * 0.5f) - (desiredLogoWidth * 0.5f);

        // Place near top
        float topPadding = -50.0f; // distance from top edge
        float logoY = h - desiredLogoHeight - topPadding;

        // Apply transform
        m_logo.transform().position = { logoX, logoY };
        m_logo.transform().scale = { desiredLogoWidth, desiredLogoHeight };
    }

    void MenuState::update(double /*dt*/)
    {

        double mouseX, mouseY;
        glfwGetCursorPos(m_window.getWindow(), &mouseX, &mouseY);

        //int w = m_window.width();
        int h = m_window.height();
        float glMouseY = h - static_cast<float>(mouseY);

        bool isHover = false;



        for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
        {
            float scale = m_positions[i].scale;
            float textWidth = m_items[i].length() * 50.0f * scale * 0.5f;
            float xPos = m_positions[i].centerX - (textWidth / 2.0f);  // Center calculation
            float yPos = m_positions[i].centerY;
            float textHeight = 48.0f * scale;

            if (mouseX >= xPos && mouseX <= (xPos + textWidth) &&
                glMouseY >= yPos && glMouseY <= (yPos + textHeight))
            {
                isHover = true;
                if (m_selected != i)
                {
                    m_selected = i;
                    m_audio.playOneShot("ui_move", Audio::Group::SFX, 0.9f);
                    m_dirty = true;
                }
                break;
            }
        }

        if (isHover && m_input.wasMouseClicked(GLFW_MOUSE_BUTTON_LEFT))
        {
            m_audio.playOneShot("ui_select", Audio::Group::SFX, 1.0f);
            /*std::this_thread::sleep_for(std::chrono::milliseconds(120));*/
            select();
        }

        // Keys: arrows or WASD
        if (m_input.wasKeyPressed(GLFW_KEY_UP) || m_input.wasKeyPressed(GLFW_KEY_W))
        {
            m_audio.playOneShot("ui_move", Audio::Group::SFX, 0.9f);
            moveUp();
        }
        if (m_input.wasKeyPressed(GLFW_KEY_DOWN) || m_input.wasKeyPressed(GLFW_KEY_S))
        {
            m_audio.playOneShot("ui_move", Audio::Group::SFX, 0.9f);
            moveDown();
        }

        // Enter / KP_Enter to select
        if (m_input.wasKeyPressed(GLFW_KEY_ENTER) || m_input.wasKeyPressed(GLFW_KEY_KP_ENTER))
        {
            m_audio.playOneShot("ui_select", Audio::Group::SFX, 1.0f), /*std::this_thread::sleep_for(std::chrono::milliseconds(120));*/
            select();
        }
#ifdef _DEBUG
        if (m_input.wasKeyPressed(GLFW_KEY_P))
        {

            m_debug = !m_debug;
            if (m_debug)
            {
                std::cout << "Debug is pressed\n";
            }

            return;
        }


        if (m_input.wasKeyTriggered(GLFW_KEY_M)) {
            m_gsm.push(std::make_shared<Runtime::SpriteState>(
                "SpriteEditor",
                m_input,
                m_gsm));

        }
#endif
        // Optional: mouse click to select as well
        // if (m_input.wasMouseClicked(GLFW_MOUSE_BUTTON_LEFT)) select();
        if (m_input.wasKeyTriggered(GLFW_KEY_V))
        {
            std::cout << "V pressed -> toggling BGM\n";
            m_bgmPaused = !m_bgmPaused;
            if (m_bgmChannel)
                m_audio.setPaused(m_bgmChannel, m_bgmPaused);
        }
        if (m_input.wasKeyPressed(GLFW_KEY_M))
            m_audio.setGroupVolume(Audio::Group::BGM, 0.0f);
        if (m_input.wasKeyPressed(GLFW_KEY_U))
            m_audio.setGroupVolume(Audio::Group::BGM, 0.7f);

        /* if (m_dirty) {
             printMenu();
         }*/
    }

    void MenuState::render(Core::Time *timer)
    {
        int w = m_window.width();
        int h = m_window.height();
        //glm::mat4 projection = glm::ortho(
        //    0.0f, (float)w,
        //    0.0f, (float)h);
        //m_renderer->beginScenePass(m_cam->viewProj());
        //m_background.render(*m_renderer);
        //m_renderer->endScenePass();
        glm::mat4 uiProj = glm::ortho(0.f, (float)w, 0.f, (float)h);
        m_renderer->beginScenePass(uiProj);
        m_background.render(*m_renderer);
        m_logo.render(*m_renderer);
        m_renderer->endScenePass();

        //m_renderer->beginScenePass(m_cam->viewProj());
        //m_logo.render(*m_renderer);
        //m_renderer->endScenePass();

        // Later, retrieve them anywhere:

        m_renderer->beginFontPass(uiProj);

        auto shader = Assets::ResourceManager<Shader>::Get("text");
        auto titleFont = Assets::ResourceManager<Assets::FontManager>::Get("Title");
        auto subFont = Assets::ResourceManager<Assets::FontManager>::Get("Subtitle");
        auto debugFont = Assets::ResourceManager<Assets::FontManager>::Get("Debug");

        auto selectFont = Assets::ResourceManager<Assets::FontManager>::Get("Selection");

        if (titleFont)
        {
            //titleFont->RenderText("Lights Out", w / 2.f - 250.f, h - 300.0f, 1.0f, glm::vec3(1.0f, 1.0f, 1.0f), *shader);
        }

        if (subFont)
        {
            //subFont->RenderText("A Series of Unfortunate Events", w / 2.f - 650.f, h - 450.0f, 1.0f, glm::vec3(0.8f, 0.8f, 1.0f), *shader);
        }
#ifdef _DEBUG
        if (debugFont)
        {
            if (timer)
            {
                std::stringstream ss{};
                ss << " screenW: "  << m_window.width() << " screenH: " << m_window.height() << " FPS: " << timer->fps();
                debugFont->RenderText(ss.str(), 10.f, h - 40.f, 1.0f, glm::vec3(0.8f, 0.8f, 1.0f), *shader);

            }
        }
#endif

        // Start: Syafiq
        if (selectFont)
        {
            // Much darker colors for better visibility
            glm::vec3 normalCol(0.01f, 0.01f, 0.01f);
            glm::vec3 selectedCol(1.f, 1.f, 1.f);

            //// Individual positions for each sticky note
            //struct MenuPosition {
            //    float centerX, centerY, scale;
            //};

            //MenuPosition positions[5] = {
            //    {390.0f, h - 340.0f, 1.f},
            //    {200.0f, h - 480.0f, 1.f},
            //    {230.0f, h - 660.0f, 1.f},
            //    {500.0f, h - 700.0f, 0.6f},
            //    {270.0f, h - 830.0f, 0.6f}
            //};

            for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
            {
                const std::string& item = m_items[i];
                glm::vec3 color = (i == m_selected) ? selectedCol : normalCol;

                // Center the text on the sticky note
                float scale = m_positions[i].scale;
                float textWidth = item.length() * 50.0f * scale * 0.5f;
                float xPos = m_positions[i].centerX - (textWidth / 2.0f);

                selectFont->RenderText(item, xPos, m_positions[i].centerY, scale, color, *shader);
            }
        }
        m_renderer->endFontPass();
    }

    // Implementation of ImGui
    void Runtime::MenuState::renderImGui(Core::Time *timer)
    {
#ifdef _DEBUG
        if (!m_debug)
            return;

        if (m_debug)
        {
            /*Implement the IMGUI*/
            ImGui::Begin("Welcome to Lights Out");
            ImGui::Text("Press [Down] & [UP] to select");
            ImGui::Text("[Enter] to select");
            ImGui::End();
        }
        (void)timer;
#endif
    }

    void MenuState::onExit()
    {
        /*if (m_loaded)
        {
            m_scenes.unloadCurrent();
        }
        m_loaded = false;*/
        if (m_bgmChannel != 0)
        {
            m_audio.stop(m_bgmChannel);
            m_bgmChannel = 0;
        }
        // m_audio.stopAll();
    }

} // namespace Runtime
