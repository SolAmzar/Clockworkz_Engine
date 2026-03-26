/******************************************************************************/
/*!
\file           GamepadManager.hpp
\project        ClockWorkz
\author         [Syafiq] 100%
\brief          Declares GamepadManager, a subsystem for SDL3-based gamepad
                input. Responsibilities include:
                - Initialising SDL3 with SDL_INIT_GAMEPAD only (GLFW owns the
                  window and event loop)
                - Hot-plug: publishing GamepadConnectedEvent /
                  GamepadDisconnectedEvent through Core::EventBus so game code
                  never needs to poll isConnected() every frame
                - Per-frame edge detection: wasButtonPressed / wasButtonReleased
                  (mirroring Platform::InputManager)
                - Normalised analog axes in [-1, 1]
                - Up to MAX_GAMEPADS (4) simultaneous controllers

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
#include <array>
#include <string>
#include <unordered_map>
#include <SDL3/SDL.h>

namespace Platform {

class GamepadManager {
public:
    GamepadManager()  = default;
    ~GamepadManager() = default;

    // Called once during Engine::init(), after InputManager::init().
    // deadzone and sprintThreshold are read from engine_config.json.
    void init(float deadzone, float sprintThreshold);

    // Called once per frame during Engine::update(), after InputManager::update().
    // Pumps SDL events, processes connect/disconnect, then snapshots button + axis state.
    void update();

    // Called during Engine::shutdown(), before InputManager is destroyed.
    void shutdown();

    // ---------- Connection ----------
    // Prefer subscribing to GamepadConnectedEvent / GamepadDisconnectedEvent via
    // Core::EventBus for runtime changes. Use isConnected() for startup queries.
    bool isConnected(int index = 0) const;

    // ---------- Buttons (edge detection) ----------
    bool isButtonDown(SDL_GamepadButton btn, int index = 0) const;
    bool wasButtonPressed(SDL_GamepadButton btn, int index = 0) const;   // up -> down this frame
    bool wasButtonReleased(SDL_GamepadButton btn, int index = 0) const;  // down -> up this frame

    // ---------- Analog ----------
    // Returns a normalised value in [-1.0, 1.0].
    // Triggers (SDL_GAMEPAD_AXIS_LEFT_TRIGGER / RIGHT_TRIGGER) return [0.0, 1.0].
    float getAxis(SDL_GamepadAxis axis, int index = 0) const;

    // ---------- Named action API (preferred for Lua and game code) ----------
    // Actions driven from slot 0: "move_x", "move_y", "sprint", "flashlight", "escape"
    bool  isActionDown   (const std::string& name) const;
    bool  isActionPressed(const std::string& name) const;  // true one frame only
    float getActionAxis  (const std::string& name) const;

private:
    static constexpr int MAX_GAMEPADS = 4;

    struct GamepadState {
        SDL_Gamepad*   handle{ nullptr };
        SDL_JoystickID sdlId{ 0 };

        std::array<bool,  SDL_GAMEPAD_BUTTON_COUNT> prev{};
        std::array<bool,  SDL_GAMEPAD_BUTTON_COUNT> curr{};
        std::array<float, SDL_GAMEPAD_AXIS_COUNT>   axes{};
    };

    GamepadState m_pads[MAX_GAMEPADS]{};

    // Action layer — driven from slot 0 each frame
    struct ActionState {
        bool  prev  = false;
        bool  curr  = false;
        float value = 0.0f;
    };
    std::unordered_map<std::string, ActionState> m_actions;
    float m_deadzone     = 0.15f;
    float m_sprintThresh = 0.50f;
    void computeActions(int slot);

    // Returns the slot index [0, MAX_GAMEPADS) assigned to this SDL joystick ID,
    // or -1 if not found.
    int slotOf(SDL_JoystickID id) const;

    // Returns the first free slot index, or -1 if all slots are occupied.
    int firstFreeSlot() const;

    void onConnected(SDL_JoystickID id);
    void onDisconnected(SDL_JoystickID id);
};

} // namespace Platform
