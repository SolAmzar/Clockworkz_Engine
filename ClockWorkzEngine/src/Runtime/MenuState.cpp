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
#include "Core/AudioManager.hpp"
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

using Assets::ResourceManager;
using Graphics::Shader;



namespace Runtime {

    static void ClearConsole() {
        std::cout << "\x1B[2J\x1B[H"; // ANSI clear + home
    }




    MenuState::MenuState(GameStateManager& gsm, Platform::InputManager& input, Platform::WindowManager& window, Audio::AudioManager& audio, std::function<void()> quitFn)
        : m_gsm(gsm), m_input(input), m_window(window), m_audio(audio), m_quitFn(std::move(quitFn))
    {
    }

    void MenuState::onEnter() {
        m_selected = 0;
        m_dirty = true;

        if (m_items.empty()) {
            m_items = { "Start Game",
                        "Load Game",
                        "Option",
                        "Credits",
                        "Exit Game" };
        }

        // m_audio.stopGroup(Audio::Group::Music);
        //printMenu();

        Assets::ResourceManager<Assets::FontManager>::Load(
            "Title", "assets/fonts/ANUNE___.TTF", 48
        );

        Assets::ResourceManager<Assets::FontManager>::Load(
            "Subtitle", "assets/fonts/ANUNEDW_.TTF", 72
        );

        Assets::ResourceManager<Assets::FontManager>::Load(
            "Debug", "assets/fonts/ANUNEDW_.TTF", 24
        );

        /* Syafiq*/
        Assets::ResourceManager<Assets::FontManager>::Load(
            "Selection", "assets/fonts/ANUNEDW_.TTF", 50
        );

        auto textShader = ResourceManager<Shader>::Load(
            "text",
            "assets/shaders/text.vert",
            "assets/shaders/text.frag"
        );
       // Amzar
        m_bgmChannel = m_audio.play("menu_bgm", Audio::Group::BGM, m_bgmVol, false);
    }

    void MenuState::printMenu() {
        ClearConsole();
        std::cout << "Menu:\n";
        for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
            std::cout << (i == m_selected ? "> " : "  ") << m_items[i] << "\n";
        }
        std::cout.flush();
        m_dirty = false;
    }

    void MenuState::moveUp() { if (--m_selected < 0) m_selected = static_cast<int>(m_items.size()) - 1; m_dirty = true; }
    void MenuState::moveDown() { if (++m_selected >= static_cast<int>(m_items.size())) m_selected = 0;   m_dirty = true; }

    void MenuState::select() {
        const std::string& choice = m_items[m_selected];

        if (choice == "Start Game") {
            m_gsm.push(std::make_shared<PlayState>("Level_01" , m_input, m_gsm));
            return;
        }
        if (choice == "Exit Game") {
            std::cout << "Exiting game...\n";
            if (m_quitFn) m_quitFn();
            return;
        }
        //yusuf
        if (choice == "Load Game")
        {
            m_gsm.switchTo(std::make_shared<PlayState>("Level_01_autosave", m_input, m_gsm));
            return;
        }

        std::cout << "[" << choice << "] selected (no state change).\n";
    }

    void MenuState::update(double /*dt*/) {
		
        // Keys: arrows or WASD
        if (m_input.wasKeyPressed(GLFW_KEY_UP) || m_input.wasKeyPressed(GLFW_KEY_W)) {
            m_audio.playOneShot("ui_move", Audio::Group::SFX, 0.9f);
            moveUp();
            
        }
        if (m_input.wasKeyPressed(GLFW_KEY_DOWN) || m_input.wasKeyPressed(GLFW_KEY_S)) {
            m_audio.playOneShot("ui_move", Audio::Group::SFX, 0.9f);
            moveDown();
            
        }

        // Enter / KP_Enter to select
        if (m_input.wasKeyPressed(GLFW_KEY_ENTER) || m_input.wasKeyPressed(GLFW_KEY_KP_ENTER)) {
            m_audio.playOneShot("ui_select", Audio::Group::SFX, 1.0f), std::this_thread::sleep_for(std::chrono::milliseconds(120));
            select();
            
        }

        if (m_input.wasKeyPressed(GLFW_KEY_0)) {
            m_debug = !m_debug;
            if (m_debug) {
                std::cout << "Debug is pressed\n";
            }
            return;
        }

        // Optional: mouse click to select as well
        // if (m_input.wasMouseClicked(GLFW_MOUSE_BUTTON_LEFT)) select();
        if (m_input.wasKeyTriggered(GLFW_KEY_P)) {
            std::cout << "P pressed -> toggling BGM\n";
			m_bgmPaused = !m_bgmPaused;
            if (m_bgmChannel) m_audio.setPaused(m_bgmChannel, m_bgmPaused);
        }
        if (m_input.wasKeyPressed(GLFW_KEY_M)) m_audio.setGroupVolume(Audio::Group::BGM, 0.0f);
        if (m_input.wasKeyPressed(GLFW_KEY_U)) m_audio.setGroupVolume(Audio::Group::BGM, 0.7f);

       /* if (m_dirty) {
            printMenu();
        }*/
    }

    void MenuState::render(Core::Time* timer) {
        // Later, retrieve them anywhere:
        auto shader = ResourceManager<Shader>::Get("text");
        shader->use();
        shader->setVec3("textColor", glm::vec3(1.0f, 1.0f, 1.0f));

        // projection setup (window width/height)
        int w = m_window.width();
        int h = m_window.height();

        glm::mat4 projection = glm::ortho(
            0.0f, (float)w,
            0.0f, (float)h
        );

        shader->setMat4("projection", projection);

        auto titleFont = Assets::ResourceManager<Assets::FontManager>::Get("Title");
        auto subFont = Assets::ResourceManager<Assets::FontManager>::Get("Subtitle");
        auto debugFont = Assets::ResourceManager<Assets::FontManager>::Get("Debug");
        
        auto selectFont = Assets::ResourceManager<Assets::FontManager>::Get("Selection");

        if (titleFont) {
            titleFont->RenderText("Lights Out", w / 2.f - 250.f, h - 300.0f, 1.0f, glm::vec3(1.0f, 1.0f, 1.0f));
        }

        if (subFont) {
            subFont->RenderText("A Series of Unfortunate Events", w / 2.f - 650.f, h - 450.0f, 1.0f, glm::vec3(0.8f, 0.8f, 1.0f));
        }

        if (debugFont) {
            if (timer) {
                std::stringstream ss{ };
                ss << "FPS: " << timer->fps();
                debugFont->RenderText(ss.str(), 10.f, h-40.f, 1.0f, glm::vec3(0.8f, 0.8f, 1.0f));
            }
           
        }

        //Start: Syafiq
        if (selectFont) {

            glm::vec3 normalCol(0.5f, 0.5f, 0.5f);
            glm::vec3 selectedCol(1.0f, 1.0f, 1.0f);

            for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
                const std::string& item = m_items[i];


                glm::vec3 selCol = (i == m_selected) ? selectedCol : normalCol;

                shader->setVec3("textColor", selCol);


                float yPos = (h - 550.0f) - (i * 80.0f);
                float textWidth = item.length() * 25.0f;
                float xPos = (w / 2.0f) - (textWidth / 2.0f);

                selectFont->RenderText(item, xPos, yPos, 0.8f, selCol);

            }
        }

        if (m_debug) {
            /*Implement the IMGUI*/
            ImGui::Begin("Welcome to Lights Out");
            ImGui::Text("Press [Down] & [UP] to select");
            ImGui::Text("[Enter] to select");
            ImGui::End();
        }
        //End: Syafiq

    }

    void MenuState::onExit() {
        if (m_bgmChannel != 0) {
            m_audio.stop(m_bgmChannel);
            m_bgmChannel = 0;
        }
    }


} // namespace Runtime
