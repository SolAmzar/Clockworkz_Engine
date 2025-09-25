#include "Runtime/MenuState.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Platform/InputManager.hpp"
#include "Runtime/PlayState.hpp"
#include <GLFW/glfw3.h>
#include <iostream>
#include <memory>

namespace Runtime {

    static void ClearConsole() {
        std::cout << "\x1B[2J\x1B[H"; // ANSI clear + home
    }

    MenuState::MenuState(GameStateManager& gsm, Platform::InputManager& input, std::function<void()> quitFn)
        : m_gsm(gsm), m_input(input), m_quitFn(std::move(quitFn)) {

    }

    void MenuState::onEnter() {
        m_selected = 0;
        m_dirty = true;
        printMenu();
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

} // namespace Runtime
