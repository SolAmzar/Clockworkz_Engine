/******************************************************************************/
/*!
\file           GamepadManager.cpp
\project        ClockWorkz
\author         [Syafiq] 100%
\brief          Implements GamepadManager. See header for full documentation.

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "Platform/GamepadManager.hpp"
#include "Core/Engine.hpp"
#include "Core/Event.hpp"
#include "Core/EventBus.hpp"
#include <cmath>
#include <iostream>

namespace Platform {

// SDL Sint16 range is -32768 .. 32767; normalise to [-1, 1]
static constexpr float SDL_AXIS_NORM = 1.0f / 32767.0f;

// --------------------------------------------------------------------------
// init
// --------------------------------------------------------------------------
void GamepadManager::init(float deadzone, float sprintThreshold) {
    m_deadzone     = deadzone;
    m_sprintThresh = sprintThreshold;

    if (SDL_Init(SDL_INIT_GAMEPAD) < 0) {
        std::cerr << "[GamepadManager] SDL_Init(SDL_INIT_GAMEPAD) failed: "
                  << SDL_GetError() << "\n";
        return;
    }

    // Open any controllers that are already connected at startup
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (ids) {
        for (int i = 0; i < count; ++i)
            onConnected(ids[i]);
        SDL_free(ids);
    }
}

// --------------------------------------------------------------------------
// update  (called once per frame, after InputManager::update)
// --------------------------------------------------------------------------
void GamepadManager::update() {
    // 1. Pump SDL's internal event queue so connect/disconnect events are ready
    SDL_PumpEvents();

    // 2. Drain only gamepad-relevant events — everything else is ignored so
    //    SDL does not compete with GLFW's own event handling
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_EVENT_GAMEPAD_ADDED)
            onConnected(ev.gdevice.which);
        else if (ev.type == SDL_EVENT_GAMEPAD_REMOVED)
            onDisconnected(ev.gdevice.which);
        // All other SDL events are intentionally discarded here
    }

    // 3. Snapshot current button + axis state for every active slot, then
    //    compute the named action layer from slot 0
    for (int slot = 0; slot < MAX_GAMEPADS; ++slot) {
        GamepadState& pad = m_pads[slot];
        if (!pad.handle) continue;

        // Swap buffers
        pad.prev = pad.curr;

        // Read buttons
        for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; ++b) {
            pad.curr[b] = SDL_GetGamepadButton(
                pad.handle, static_cast<SDL_GamepadButton>(b));
        }

        // Read axes (normalise Sint16 -> float)
        for (int a = 0; a < SDL_GAMEPAD_AXIS_COUNT; ++a) {
            Sint16 raw = SDL_GetGamepadAxis(
                pad.handle, static_cast<SDL_GamepadAxis>(a));
            pad.axes[a] = static_cast<float>(raw) * SDL_AXIS_NORM;
        }
    }

    computeActions(0);   // drive named action layer from slot 0
}

// --------------------------------------------------------------------------
// shutdown
// --------------------------------------------------------------------------
void GamepadManager::shutdown() {
    for (int slot = 0; slot < MAX_GAMEPADS; ++slot) {
        if (m_pads[slot].handle) {
            SDL_CloseGamepad(m_pads[slot].handle);
            m_pads[slot] = GamepadState{};
        }
    }
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
}

// --------------------------------------------------------------------------
// Public queries
// --------------------------------------------------------------------------
bool GamepadManager::isConnected(int index) const {
    if (index < 0 || index >= MAX_GAMEPADS) return false;
    return m_pads[index].handle != nullptr;
}

bool GamepadManager::isButtonDown(SDL_GamepadButton btn, int index) const {
    if (index < 0 || index >= MAX_GAMEPADS) return false;
    if (!m_pads[index].handle) return false;
    return m_pads[index].curr[static_cast<int>(btn)];
}

bool GamepadManager::wasButtonPressed(SDL_GamepadButton btn, int index) const {
    if (index < 0 || index >= MAX_GAMEPADS) return false;
    if (!m_pads[index].handle) return false;
    int b = static_cast<int>(btn);
    return m_pads[index].curr[b] && !m_pads[index].prev[b];
}

bool GamepadManager::wasButtonReleased(SDL_GamepadButton btn, int index) const {
    if (index < 0 || index >= MAX_GAMEPADS) return false;
    if (!m_pads[index].handle) return false;
    int b = static_cast<int>(btn);
    return !m_pads[index].curr[b] && m_pads[index].prev[b];
}

float GamepadManager::getAxis(SDL_GamepadAxis axis, int index) const {
    if (index < 0 || index >= MAX_GAMEPADS) return 0.0f;
    if (!m_pads[index].handle) return 0.0f;
    return m_pads[index].axes[static_cast<int>(axis)];
}

// --------------------------------------------------------------------------
// Private helpers
// --------------------------------------------------------------------------
int GamepadManager::slotOf(SDL_JoystickID id) const {
    for (int i = 0; i < MAX_GAMEPADS; ++i)
        if (m_pads[i].sdlId == id && m_pads[i].handle)
            return i;
    return -1;
}

int GamepadManager::firstFreeSlot() const {
    for (int i = 0; i < MAX_GAMEPADS; ++i)
        if (!m_pads[i].handle)
            return i;
    return -1;
}

void GamepadManager::onConnected(SDL_JoystickID id) {
    if (slotOf(id) >= 0) return; // already tracked

    int slot = firstFreeSlot();
    if (slot < 0) {
        std::cerr << "[GamepadManager] All " << MAX_GAMEPADS
                  << " gamepad slots are occupied; ignoring controller.\n";
        return;
    }

    SDL_Gamepad* handle = SDL_OpenGamepad(id);
    if (!handle) {
        std::cerr << "[GamepadManager] SDL_OpenGamepad failed: "
                  << SDL_GetError() << "\n";
        return;
    }

    m_pads[slot].handle = handle;
    m_pads[slot].sdlId  = id;
    m_pads[slot].prev.fill(false);
    m_pads[slot].curr.fill(false);
    m_pads[slot].axes.fill(0.0f);

    std::cout << "[GamepadManager] Controller connected in slot " << slot << "\n";
    Core::Engine::Get().events().publish(Core::GamepadConnectedEvent{ slot });
}

void GamepadManager::onDisconnected(SDL_JoystickID id) {
    int slot = slotOf(id);
    if (slot < 0) return;

    SDL_CloseGamepad(m_pads[slot].handle);
    m_pads[slot] = GamepadState{};

    std::cout << "[GamepadManager] Controller disconnected from slot " << slot << "\n";
    Core::Engine::Get().events().publish(Core::GamepadDisconnectedEvent{ slot });
}

// --------------------------------------------------------------------------
// Named action layer
// --------------------------------------------------------------------------
void GamepadManager::computeActions(int slot) {
    // Swap prev before writing curr for all actions
    for (auto& [name, a] : m_actions)
        a.prev = a.curr;

    if (slot < 0 || slot >= MAX_GAMEPADS || !m_pads[slot].handle) {
        // No controller — zero everything
        for (auto& [name, a] : m_actions) {
            a.curr  = false;
            a.value = 0.0f;
        }
        return;
    }

    // --- Left stick with radial deadzone + Y-flip ---
    float lx = m_pads[slot].axes[SDL_GAMEPAD_AXIS_LEFTX];
    float ly = m_pads[slot].axes[SDL_GAMEPAD_AXIS_LEFTY];

    float mag = std::sqrt(lx * lx + ly * ly);
    if (mag < m_deadzone) {
        lx = 0.0f;
        ly = 0.0f;
    } else {
        float scale = (mag - m_deadzone) / (1.0f - m_deadzone);
        lx = lx * scale / mag;
        ly = ly * scale / mag;
    }
    ly = -ly;   // SDL positive-Y = down; game positive-Y = up

    ActionState& mx = m_actions["move_x"];
    mx.curr  = (lx != 0.0f);
    mx.value = lx;

    ActionState& my = m_actions["move_y"];
    my.curr  = (ly != 0.0f);
    my.value = ly;

    // --- Left stick + D-pad → UI navigation ---
    bool dpadUp   = SDL_GetGamepadButton(m_pads[slot].handle, SDL_GAMEPAD_BUTTON_DPAD_UP);
    bool dpadDown = SDL_GetGamepadButton(m_pads[slot].handle, SDL_GAMEPAD_BUTTON_DPAD_DOWN);

    ActionState& uiUp   = m_actions["ui_up"];
    uiUp.curr  = dpadUp || (ly >  0.5f);
    uiUp.value = uiUp.curr ? 1.f : 0.f;

    ActionState& uiDown = m_actions["ui_down"];
    uiDown.curr  = dpadDown || (ly < -0.5f);
    uiDown.value = uiDown.curr ? 1.f : 0.f;

    // --- Right stick → look (same radial deadzone, Y-flipped) ---
    float rx = m_pads[slot].axes[SDL_GAMEPAD_AXIS_RIGHTX];
    float ry = m_pads[slot].axes[SDL_GAMEPAD_AXIS_RIGHTY];

    float rmag = std::sqrt(rx * rx + ry * ry);
    if (rmag < m_deadzone) {
        rx = 0.0f;
        ry = 0.0f;
    } else {
        float rscale = (rmag - m_deadzone) / (1.0f - m_deadzone);
        rx = rx * rscale / rmag;
        ry = ry * rscale / rmag;
    }
    ry = -ry;

    ActionState& lookX = m_actions["look_x"];
    lookX.curr  = (rx != 0.0f);
    lookX.value = rx;

    ActionState& lookY = m_actions["look_y"];
    lookY.curr  = (ry != 0.0f);
    lookY.value = ry;

    // --- Right trigger → sprint ---
    float triggerRaw = m_pads[slot].axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER];
    // Trigger axis is already [0, 1] after SDL_AXIS_NORM (Sint16 0..32767)
    float trigger = (triggerRaw < 0.0f) ? 0.0f : triggerRaw;

    ActionState& sprint = m_actions["sprint"];
    sprint.curr  = (trigger > m_sprintThresh);
    sprint.value = trigger;

    // --- X button (Xbox X = SDL_GAMEPAD_BUTTON_WEST) → flashlight ---
    bool xBtn = SDL_GetGamepadButton(m_pads[slot].handle, SDL_GAMEPAD_BUTTON_WEST);
    ActionState& flashlight = m_actions["flashlight"];
    flashlight.curr  = xBtn;
    flashlight.value = xBtn ? 1.0f : 0.0f;

    // --- Options / Start button (DualSense B9) → escape ---
    bool startBtn = SDL_GetGamepadButton(m_pads[slot].handle, SDL_GAMEPAD_BUTTON_START);
    ActionState& escapeAction = m_actions["escape"];
    escapeAction.curr  = startBtn;
    escapeAction.value = startBtn ? 1.0f : 0.0f;
}

bool GamepadManager::isActionDown(const std::string& name) const {
    auto it = m_actions.find(name);
    return it != m_actions.end() && it->second.curr;
}

bool GamepadManager::isActionPressed(const std::string& name) const {
    auto it = m_actions.find(name);
    return it != m_actions.end() && it->second.curr && !it->second.prev;
}

float GamepadManager::getActionAxis(const std::string& name) const {
    auto it = m_actions.find(name);
    return it != m_actions.end() ? it->second.value : 0.0f;
}

} // namespace Platform
