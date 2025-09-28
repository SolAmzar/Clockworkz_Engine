#include "Runtime/MenuState.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Platform/InputManager.hpp"
#include "Platform/WindowManager.hpp"
#include "Runtime/PlayState.hpp"
#include <GLFW/glfw3.h>
#include "Assets/FontManager.hpp"
#include "Assets/ResourceManager.hpp"
#include "Graphics/Shader.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <memory>

using ClockWorkz::Assets::ResourceManager;
using Graphics::Shader;



namespace Runtime {

    static void ClearConsole() {
        std::cout << "\x1B[2J\x1B[H"; // ANSI clear + home
    }

    MenuState::MenuState(GameStateManager& gsm, Platform::InputManager& input, Platform::WindowManager& window, std::function<void()> quitFn)
        : m_gsm(gsm), m_input(input), m_window(window), m_quitFn(std::move(quitFn)) {

    }

    void MenuState::onEnter() {
        m_selected = 0;
        m_dirty = true;
        printMenu();

        // 
        ClockWorkz::Assets::ResourceManager<ClockWorkz::Assets::FontManager>::Load(
            "Title", "assets/fonts/ANUNE___.TTF", 48
        );

        ClockWorkz::Assets::ResourceManager<ClockWorkz::Assets::FontManager>::Load(
            "Subtitle", "assets/fonts/ANUNEDW_.TTF", 72
        );

        auto textShader = ResourceManager<Shader>::Load(
            "text",
            "assets/shaders/text.vert",
            "assets/shaders/text.frag"
        );
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
            m_gsm.switchTo(std::make_shared<PlayState>("Level_01"));
            return;
        }
        if (choice == "Exit Game") {
            std::cout << "Exiting game...\n";
            if (m_quitFn) m_quitFn();
            return;
        }

        std::cout << "[" << choice << "] selected (no state change).\n";
    }

    void MenuState::update(double /*dt*/) {
        // Keys: arrows or WASD
        if (m_input.wasKeyPressed(GLFW_KEY_UP) || m_input.wasKeyPressed(GLFW_KEY_W)) moveUp();
        if (m_input.wasKeyPressed(GLFW_KEY_DOWN) || m_input.wasKeyPressed(GLFW_KEY_S)) moveDown();

        // Enter / KP_Enter to select
        if (m_input.wasKeyPressed(GLFW_KEY_ENTER) || m_input.wasKeyPressed(GLFW_KEY_KP_ENTER)) select();

        // Optional: mouse click to select as well
        // if (m_input.wasMouseClicked(GLFW_MOUSE_BUTTON_LEFT)) select();

        if (m_dirty) printMenu();
    }

    void MenuState::render() {
        // Later, retrieve them anywhere:
        auto shader = ResourceManager<Shader>::Get("text");
        shader->use();
        shader->setVec3("textColor", glm::vec3(1, 1, 1));

        // projection setup (window width/height)
        int w = m_window.width();
        int h = m_window.height();

        glm::mat4 projection = glm::ortho(
            0.0f, (float)w,
            0.0f, (float)h
        );

        shader->setMat4("projection", projection);

        auto titleFont = ClockWorkz::Assets::ResourceManager<ClockWorkz::Assets::FontManager>::Get("Title");
        auto subFont = ClockWorkz::Assets::ResourceManager<ClockWorkz::Assets::FontManager>::Get("Subtitle");
        if (titleFont) {
            titleFont->RenderText("Lights Out", w/2.f - 250.f, h - 300.0f, 1.0f, glm::vec3(1.0f, 1.0f, 1.0f));
        }
            
        if (subFont) {
            subFont->RenderText("A Series of Unfortunate Events", w / 2.f - 650.f, h - 450.0f, 1.0f, glm::vec3(0.8f, 0.8f, 1.0f));
        }
            
    }

    void MenuState::onExit() {
    }


} // namespace Runtime
