/******************************************************************************/
/*!
\file           InputManager.hpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares InputManager, a system for handling user input with GLFW.
                Responsibilities include:
                - Tracking specific keys and mouse buttons
                - Polling current and previous states to detect presses, releases,
                  and clicks
                - Storing mouse position and simple click detection thresholds
                - Providing queries such as isKeyDown(), wasKeyPressed(),
                  wasMouseClicked(), etc.

                Integrates with WindowManager by attaching to a GLFW window.

References:
- GLFW Input Guide: https://www.glfw.org/docs/latest/input_guide.html
- GLFW Mouse Input: https://www.glfw.org/docs/latest/input_guide.html#input_mouse
- GLFW Keyboard Input: https://www.glfw.org/docs/latest/input_guide.html#input_keyboard

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/
#include "Platform/InputManager.hpp"
#include "Platform/WindowManager.hpp"
#include "Graphics/Camera2D.hpp"
#include <GLFW/glfw3.h>
#include <cmath>

namespace Platform {

    InputManager::InputManager() {
        // Tracking defaults set in init(), once we know GLFW is ready.
    }

    bool InputManager::init(WindowManager& wm) {
        m_window = wm.handle();
        if (!m_window) return false;

        // Reasonable defaults for menus + gameplay
        trackKey(GLFW_KEY_UP);
        trackKey(GLFW_KEY_DOWN);
        trackKey(GLFW_KEY_LEFT);
        trackKey(GLFW_KEY_RIGHT);

        trackKey(GLFW_KEY_W);
        trackKey(GLFW_KEY_S);
        trackKey(GLFW_KEY_A);
        trackKey(GLFW_KEY_D);

        trackKey(GLFW_KEY_Q);
        trackKey(GLFW_KEY_E);
        trackKey(GLFW_KEY_G);
   
        trackKey(GLFW_KEY_ENTER);
        trackKey(GLFW_KEY_KP_ENTER);
        trackKey(GLFW_KEY_ESCAPE);
        trackKey(GLFW_KEY_SPACE);
        trackKey(GLFW_KEY_L);


        trackKey(GLFW_KEY_R); //For Flashlight
        trackKey(GLFW_KEY_Z); //
        trackKey(GLFW_KEY_X);
        trackKey(GLFW_KEY_P); //For debugging
		trackKey(GLFW_KEY_M); //Mute BGM
		trackKey(GLFW_KEY_U); //Unmute BGM
        trackKey(GLFW_KEY_LEFT_SHIFT);

        trackMouseButton(GLFW_MOUSE_BUTTON_LEFT);
        trackMouseButton(GLFW_MOUSE_BUTTON_RIGHT);

        // Seed mouse pos
        glfwGetCursorPos(m_window, &m_mouseX, &m_mouseY);
        m_prevMouseX = m_mouseX; m_prevMouseY = m_mouseY;

        return true;
    }


    void InputManager::trackKey(int key) {
        m_trackedKeys.insert(key);
        ensureKeySlot(key);
    }

    void InputManager::untrackKey(int key) {
        m_trackedKeys.erase(key);
        m_prevKeys.erase(key);
        m_currKeys.erase(key);
    }

    bool InputManager::isKeyTracked(int key) const {
        return m_trackedKeys.find(key) != m_trackedKeys.end();
    }

    void InputManager::trackMouseButton(int button) {
        m_trackedMouse.insert(button);
        ensureMouseSlot(button);
    }

    void InputManager::untrackMouseButton(int button) {
        m_trackedMouse.erase(button);
        m_prevMouse.erase(button);
        m_currMouse.erase(button);
        m_mouseClickedThisFrame.erase(button);
        m_mousePressInfo.erase(button);
    }

    bool InputManager::isMouseButtonTracked(int button) const {
        return m_trackedMouse.find(button) != m_trackedMouse.end();
    }

    void InputManager::ensureKeySlot(int key) {
        if (!m_prevKeys.count(key)) m_prevKeys[key] = 0;
        if (!m_currKeys.count(key)) m_currKeys[key] = 0;
    }

    void InputManager::ensureMouseSlot(int button) {
        if (!m_prevMouse.count(button)) m_prevMouse[button] = 0;
        if (!m_currMouse.count(button)) m_currMouse[button] = 0;
        if (!m_mouseClickedThisFrame.count(button)) m_mouseClickedThisFrame[button] = false;
        if (!m_mousePressInfo.count(button)) m_mousePressInfo[button] = {};
    }

    bool InputManager::keyCurr(int key) const { auto it = m_currKeys.find(key);  return it != m_currKeys.end() && isPressedUChar(it->second); }
    bool InputManager::keyPrev(int key) const { auto it = m_prevKeys.find(key);  return it != m_prevKeys.end() && isPressedUChar(it->second); }
    bool InputManager::mouseCurr(int b) const { auto it = m_currMouse.find(b);   return it != m_currMouse.end() && isPressedUChar(it->second); }
    bool InputManager::mousePrev(int b) const { auto it = m_prevMouse.find(b);   return it != m_prevMouse.end() && isPressedUChar(it->second); }

    void InputManager::update() {
        if (!m_window) return;

        // Swap states
        m_prevKeys = m_currKeys;
        m_prevMouse = m_currMouse;
        m_prevMouseX = m_mouseX; m_prevMouseY = m_mouseY;

        // Reset "clicked this frame" flags
        for (auto& kv : m_mouseClickedThisFrame) kv.second = false;

        // Poll keyboard states only for tracked keys
        for (int key : m_trackedKeys) {
            ensureKeySlot(key);
            m_currKeys[key] = static_cast<unsigned char>(glfwGetKey(m_window, key));
        }

        // Poll mouse states and position
        for (int b : m_trackedMouse) {
            ensureMouseSlot(b);
            m_currMouse[b] = static_cast<unsigned char>(glfwGetMouseButton(m_window, b));
        }
        glfwGetCursorPos(m_window, &m_mouseX, &m_mouseY);

        // Click detection (press->release within thresholds)
        const double now = glfwGetTime();
        for (int b : m_trackedMouse) {
            const bool pressedNow = (m_currMouse[b] == GLFW_PRESS);
            const bool pressedPrev = (m_prevMouse[b] == GLFW_PRESS);

            // Edge: pressed this frame -> store press time & pos
            if (pressedNow && !pressedPrev) {
                auto& pi = m_mousePressInfo[b];
                pi.t = now; pi.x = m_mouseX; pi.y = m_mouseY; pi.valid = true;
            }

            // Edge: released this frame -> check click thresholds
            if (!pressedNow && pressedPrev) {
                auto& pi = m_mousePressInfo[b];
                bool clicked = false;
                if (pi.valid) {
                    const double dt = now - pi.t;
                    const double dx = m_mouseX - pi.x;
                    const double dy = m_mouseY - pi.y;
                    const double dist = std::sqrt(dx * dx + dy * dy);
                    clicked = (dt <= m_clickTimeMax) && (dist <= m_clickMoveMax);
                }
                m_mouseClickedThisFrame[b] = clicked;
                pi.valid = false;
            }
        }
    }

    // -------- Keyboard queries --------
    bool InputManager::isKeyDown(int key) const {
        auto it = m_currKeys.find(key);
        return it != m_currKeys.end() && (it->second == GLFW_PRESS);
    }
    bool InputManager::wasKeyPressed(int key) const {
        const bool c = keyCurr(key);
        const bool p = keyPrev(key);
        return (c && !p);
    }
    bool InputManager::wasKeyReleased(int key) const {
        const bool c = keyCurr(key);
        const bool p = keyPrev(key);
        return (!c && p);
    }

    // -------- Mouse queries --------
    bool InputManager::isMouseDown(int button) const {
        auto it = m_currMouse.find(button);
        return it != m_currMouse.end() && (it->second == GLFW_PRESS);
    }
    bool InputManager::wasMousePressed(int button) const {
        const bool c = mouseCurr(button);
        const bool p = mousePrev(button);
        return (c && !p);
    }
    bool InputManager::wasMouseReleased(int button) const {
        const bool c = mouseCurr(button);
        const bool p = mousePrev(button);
        return (!c && p);
    }
    bool InputManager::wasMouseClicked(int button) const {
        auto it = m_mouseClickedThisFrame.find(button);
        return it != m_mouseClickedThisFrame.end() && it->second;
    }


    vec2 InputManager::getMouseWorldPos(const Graphics::Camera2D& camera,
        float windowWidth,
        float windowHeight) const {
        // Convert window-space (pixels) to Normalized Device Coordinates (NDC)
        float ndcX = (2.0f * static_cast<float>(m_mouseX) / windowWidth) - 1.0f;
        float ndcY = 1.0f - (2.0f * static_cast<float>(m_mouseY) / windowHeight);

        glm::vec4 clipCoords(ndcX, ndcY, 0.0f, 1.0f);

        // Transform back into world space using inverse of (Projection * View)
        glm::mat4 invVP = glm::inverse(camera.viewProj());
        glm::vec4 world = invVP * clipCoords;

        return vec2(world.x, world.y);
    }

} // namespace Platform
