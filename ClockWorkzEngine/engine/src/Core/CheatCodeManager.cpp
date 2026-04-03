/******************************************************************************/
/*!
\file           CheatCodeManager.cpp
\project        ClockWorkz
\author         [Gemini]
\brief          Implements CheatCodeManager. See header for full documentation.
*/
/******************************************************************************/

#include "Core/CheatCodeManager.hpp"
#include "Platform/GamepadManager.hpp"
#include "Platform/InputManager.hpp"
#include "Core/Logger.hpp"
#include "Core/Engine.hpp"
#include <GLFW/glfw3.h>
#include <iostream>

namespace Core {

CheatCodeManager::CheatCodeManager(Platform::GamepadManager& gamepad, Platform::InputManager& input)
    : m_gamepad(gamepad), m_input(input) {
}

void CheatCodeManager::registerCheat(const std::vector<CheatInput>& sequence, CheatCallback callback, const std::string& name) {
    m_cheats.push_back({sequence, callback, name});
}

void CheatCodeManager::update(float dt) {
    m_timer += dt;

    if (m_timer > m_inputTimeout) {
        resetBuffer();
    }

    CheatInput gamepadInput = getGamepadInput();
    CheatInput keyboardInput = getKeyboardInput();

    CheatInput newInput = CheatInput::None;
    if (gamepadInput != CheatInput::None) {
        newInput = gamepadInput;
    } else if (keyboardInput != CheatInput::None) {
        newInput = keyboardInput;
    }

    if (newInput != CheatInput::None) {
        m_buffer.push_back(newInput);
        if (m_buffer.size() > m_maxBufferSize) {
            m_buffer.erase(m_buffer.begin());
        }
        m_timer = 0.0f;
        checkBuffer();
    }
}

void CheatCodeManager::resetBuffer() {
    if (!m_buffer.empty()) {
        m_buffer.clear();
    }
}

void CheatCodeManager::checkBuffer() {
    for (const auto& cheat : m_cheats) {
        if (m_buffer.size() < cheat.sequence.size()) {
            continue;
        }

        bool match = true;
        size_t bufferOffset = m_buffer.size() - cheat.sequence.size();
        for (size_t i = 0; i < cheat.sequence.size(); ++i) {
            if (m_buffer[bufferOffset + i] != cheat.sequence[i]) {
                match = false;
                break;
            }
        }

        if (match) {
            Core::Engine::Get().logger().info("Cheat Activated: " + (cheat.name.empty() ? "Unnamed Cheat" : cheat.name));
            if (cheat.callback) {
                cheat.callback();
            }
            resetBuffer();
            break; // Only trigger one cheat at a time
        }
    }
}

CheatInput CheatCodeManager::getGamepadInput() const {
    if (m_gamepad.wasButtonPressed(SDL_GAMEPAD_BUTTON_DPAD_UP)) return CheatInput::Up;
    if (m_gamepad.wasButtonPressed(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) return CheatInput::Down;
    if (m_gamepad.wasButtonPressed(SDL_GAMEPAD_BUTTON_DPAD_LEFT)) return CheatInput::Left;
    if (m_gamepad.wasButtonPressed(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) return CheatInput::Right;
    if (m_gamepad.wasButtonPressed(SDL_GAMEPAD_BUTTON_SOUTH)) return CheatInput::A; // Xbox A / PS Cross
    if (m_gamepad.wasButtonPressed(SDL_GAMEPAD_BUTTON_EAST)) return CheatInput::B;  // Xbox B / PS Circle
    if (m_gamepad.wasButtonPressed(SDL_GAMEPAD_BUTTON_WEST)) return CheatInput::X;  // Xbox X / PS Square
    if (m_gamepad.wasButtonPressed(SDL_GAMEPAD_BUTTON_NORTH)) return CheatInput::Y; // Xbox Y / PS Triangle
    if (m_gamepad.wasButtonPressed(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) return CheatInput::L1;
    if (m_gamepad.wasButtonPressed(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) return CheatInput::R1;
    // For L2/R2, they are usually axes in SDL, but some controllers have them as buttons too.
    // SDL_GAMEPAD_BUTTON_BACK is Select/Share
    if (m_gamepad.wasButtonPressed(SDL_GAMEPAD_BUTTON_BACK)) return CheatInput::Select;
    if (m_gamepad.wasButtonPressed(SDL_GAMEPAD_BUTTON_START)) return CheatInput::Start;
    
    return CheatInput::None;
}

CheatInput CheatCodeManager::getKeyboardInput() const {
    if (m_input.wasKeyPressed(GLFW_KEY_UP)) return CheatInput::Up;
    if (m_input.wasKeyPressed(GLFW_KEY_DOWN)) return CheatInput::Down;
    if (m_input.wasKeyPressed(GLFW_KEY_LEFT)) return CheatInput::Left;
    if (m_input.wasKeyPressed(GLFW_KEY_RIGHT)) return CheatInput::Right;
    if (m_input.wasKeyPressed(GLFW_KEY_Z)) return CheatInput::A;
    if (m_input.wasKeyPressed(GLFW_KEY_X)) return CheatInput::B;
    if (m_input.wasKeyPressed(GLFW_KEY_C)) return CheatInput::X;
    if (m_input.wasKeyPressed(GLFW_KEY_V)) return CheatInput::Y;
    if (m_input.wasKeyPressed(GLFW_KEY_1)) return CheatInput::L1;
    if (m_input.wasKeyPressed(GLFW_KEY_2)) return CheatInput::R1;
    if (m_input.wasKeyPressed(GLFW_KEY_BACKSPACE)) return CheatInput::Select;
    if (m_input.wasKeyPressed(GLFW_KEY_ENTER)) return CheatInput::Start;

    return CheatInput::None;
}

} // namespace Core
