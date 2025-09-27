#pragma once
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <cstdint>
#include <glm/vec2.hpp>

struct GLFWwindow;

namespace Platform {

    class WindowManager; // fwd

    class InputManager {
    public:
        InputManager();

        // Preferred: grab GLFWwindow* from WindowManager
        // Returns false if window handle is null.
        bool init(WindowManager& wm);

        // Call once per frame AFTER WindowManager::poll()
        void update();

        // ---------- Keyboard ----------
        void trackKey(int key);                // begin polling this key every frame
        void untrackKey(int key);              // stop polling
        bool isKeyTracked(int key) const;

        bool isKeyDown(int key) const;         // held right now
        bool wasKeyPressed(int key) const;     // edge: up -> down this frame
        bool wasKeyTriggered(int key) const { return wasKeyPressed(key); } // alias
        bool wasKeyReleased(int key) const;    // edge: down -> up this frame

        // ---------- Mouse ----------
        void trackMouseButton(int button);
        void untrackMouseButton(int button);
        bool isMouseButtonTracked(int button) const;

        bool isMouseDown(int button) const;        // held
        bool wasMousePressed(int button) const;    // edge: up -> down
        bool wasMouseReleased(int button) const;   // edge: down -> up
        bool wasMouseClicked(int button) const;    // press->release within thresholds

        // TODO swap to own math vec2
        glm::vec2 mousePosition() const { return { static_cast<float>(m_mouseX), static_cast<float>(m_mouseY) }; }
        void getMousePosition(double& x, double& y) const { x = m_mouseX; y = m_mouseY; }

        // Click detection thresholds (defaults: 0.25s, 6px)
        void setClickThresholds(double maxSeconds, double maxPixels) {
            if (maxSeconds >= 0.0) m_clickTimeMax = maxSeconds;
            if (maxPixels >= 0.0) m_clickMoveMax = maxPixels;
        }

    private:
        // Internals
        void ensureKeySlot(int key);
        void ensureMouseSlot(int button);

        bool keyCurr(int key)   const;
        bool keyPrev(int key)   const;
        bool mouseCurr(int btn) const;
        bool mousePrev(int btn) const;

        static bool isPressedUChar(unsigned char v) { return v != 0; }

    private:
        GLFWwindow* m_window{ nullptr };

        // Which inputs to poll each frame
        std::unordered_set<int> m_trackedKeys;
        std::unordered_set<int> m_trackedMouse;

        // State maps (GLFW_PRESS = 1, GLFW_RELEASE = 0 copied as unsigned char)
        mutable std::unordered_map<int, unsigned char> m_prevKeys;
        mutable std::unordered_map<int, unsigned char> m_currKeys;
        mutable std::unordered_map<int, unsigned char> m_prevMouse;
        mutable std::unordered_map<int, unsigned char> m_currMouse;

        // Mouse position (window client coords)
        double m_mouseX{ 0.0 }, m_mouseY{ 0.0 };
        double m_prevMouseX{ 0.0 }, m_prevMouseY{ 0.0 };

        // Click bookkeeping
        struct PressInfo { double t{ 0.0 }; double x{ 0.0 }; double y{ 0.0 }; bool valid{ false }; };
        std::unordered_map<int, PressInfo> m_mousePressInfo;
        std::unordered_map<int, bool>      m_mouseClickedThisFrame;

        // Click thresholds
        double m_clickTimeMax{ 0.25 }; // seconds
        double m_clickMoveMax{ 6.0 };  // pixels
    };

} // namespace Platform
