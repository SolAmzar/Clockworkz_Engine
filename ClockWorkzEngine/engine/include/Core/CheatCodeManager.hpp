/******************************************************************************/
/*!
\file           CheatCodeManager.hpp
\project        ClockWorkz
\author         [Gemini]
\brief          Declares CheatCodeManager, a system for tracking and triggering
                cheat codes from gamepad or keyboard input.
*/
/******************************************************************************/

#pragma once
#include <vector>
#include <string>
#include <functional>
#include <memory>

namespace Platform {
    class GamepadManager;
    class InputManager;
}

namespace Core {

/**
 * @brief Unified input enum for cheat codes.
 */
enum class CheatInput {
    Up, Down, Left, Right,
    A, B, X, Y,
    L1, R1, L2, R2,
    Select, Start,
    None
};

class CheatCodeManager {
public:
    using CheatCallback = std::function<void()>;

    CheatCodeManager(Platform::GamepadManager& gamepad, Platform::InputManager& input);
    ~CheatCodeManager() = default;

    /**
     * @brief Registers a cheat code sequence.
     * @param sequence The sequence of inputs required to trigger the cheat.
     * @param callback The function to call when the cheat is activated.
     * @param name Optional name for the cheat (for logging).
     */
    void registerCheat(const std::vector<CheatInput>& sequence, CheatCallback callback, const std::string& name = "");

    /**
     * @brief Updates the cheat code manager, polling for new inputs.
     * @param dt Delta time since last frame.
     */
    void update(float dt);

    /**
     * @brief Resets the current input buffer.
     */
    void resetBuffer();

private:
    Platform::GamepadManager& m_gamepad;
    Platform::InputManager& m_input;

    struct Cheat {
        std::vector<CheatInput> sequence;
        CheatCallback callback;
        std::string name;
    };

    std::vector<Cheat> m_cheats;
    std::vector<CheatInput> m_buffer;
    
    float m_timer = 0.0f;
    const float m_inputTimeout = 1.0f; // Seconds of inactivity before buffer resets
    const size_t m_maxBufferSize = 32;

    void checkBuffer();
    CheatInput getGamepadInput() const;
    CheatInput getKeyboardInput() const;
};

} // namespace Core
