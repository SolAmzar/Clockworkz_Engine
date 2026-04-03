/******************************************************************************/
/*!
\file           NotificationSystem.hpp
\project        ClockWorkz
\author         [Gemini]
\brief          Declares NotificationSystem, a global system for displaying
                toast messages using text rendering.
*/
/******************************************************************************/

#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <memory>

namespace Assets {
    class FontManager;
}

namespace Graphics {
    class Shader;
}

namespace Core {

struct Notification {
    std::string message;
    glm::vec3 color;
    float duration;
    float maxDuration;
    float alpha;
};

class NotificationSystem {
public:
    NotificationSystem();
    ~NotificationSystem() = default;

    /**
     * @brief Pushes a new notification to the screen.
     */
    void pushNotification(const std::string& message, glm::vec3 color, float duration = 3.0f);

    /**
     * @brief Updates active notifications, handling timers and fading.
     */
    void update(float dt);

    /**
     * @brief Renders all active notifications using FontManager.
     */
    void render();

private:
    std::vector<Notification> m_notifications;
    const float m_fadeSpeed = 2.0f;
};

} // namespace Core
