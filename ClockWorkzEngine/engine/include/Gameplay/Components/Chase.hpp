/******************************************************************************/
/*!
\file           Chase.hpp
\project        Lights Out
\author(s)      [Sol-Amzar], 10%
\brief          Declares Chase component that makes a GameObject chase the player.
				Responsibilities include:
				- Moving the owner GameObject towards the player at a specified speed
				- Optionally only engaging when within a certain radius
				- Stopping when close enough to avoid jittering on top of the player
				Used to create enemy AI that pursues the player character.
				(Template for more complex AI behaviors)

All content ? 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/





// Gameplay/Components/Chase.hpp
#pragma once
#include "Gameplay/GameObject.hpp"
#include "Math/Vec2.hpp"

namespace Gameplay {

    class Chase : public Component {
    public:
        // Configuration constants
        static constexpr float DEFAULT_SPEED = 150.0f;
        static constexpr float DEFAULT_ENGAGE_RADIUS = 300.0f;
        static constexpr float DEFAULT_STOP_DISTANCE = 25.0f;
        static constexpr float DEFAULT_LIGHT_SLOWDOWN = 0.3f;  // 30% speed when in light

        // Constructor
        // speed: units/sec
        // engageRadius: only chase when player is within this distance (0 = always chase)
        explicit Chase(GameObject& enemy, GameObject* player,
            float speed = DEFAULT_SPEED,
            float engageRadius = DEFAULT_ENGAGE_RADIUS);

        // Component interface
        void onUpdate(GameObject& enemy, double dt) override;

        // Query current state
        bool isChasing() const { return m_isChasing; }
        bool isSlowedByLight() const { return m_isSlowedByLight; }
        float distanceToTarget() const { return m_lastDistance; }
        GameObject* getTarget() const { return m_player; }
        float getCurrentSpeed() const { return m_currentSpeed; }

        // Configuration setters
        void setSpeed(float s) { m_speed = s; }
        void setEngageRadius(float r) { m_engageRadius = r; }
        void setStopDistance(float r) { m_stopDist = r; }
        void setTarget(GameObject* target) { m_player = target; }

        // Wander behavior configuration
        void setWanderSpeed(float speed) { m_wanderSpeed = speed; }
        void setWanderInterval(float interval) { m_wanderChangeInterval = interval; }

        // Light slowdown configuration
        // factor: 0.0 = stop completely, 1.0 = no slowdown
        // Example: 0.3 means enemy moves at 30% speed when in light
        void setLightSlowdownFactor(float factor) { m_lightSlowdownFactor = factor; }
        float getLightSlowdownFactor() const { return m_lightSlowdownFactor; }

        // Configuration getters
        float getSpeed() const { return m_speed; }
        float getEngageRadius() const { return m_engageRadius; }
        float getStopDistance() const { return m_stopDist; }

    private:
        // Helper methods - encapsulated logic
        bool shouldEngageTarget(float distance) const;
        vec2 calculateDesiredVelocity(const vec2& from, const vec2& to) const;
        void applyVelocity(GameObject& enemy, const vec2& velocity, double dt);
        void updateWanderBehavior(GameObject& enemy, double dt);
        vec2 generateRandomDirection() const;
        float calculateEffectiveSpeed() const;

        // Configuration
        float m_speed;
        float m_engageRadius;
        float m_stopDist;
        float m_lightSlowdownFactor;  // How much to slow down in light (0.0-1.0)

        // References
        GameObject& m_enemy;
        GameObject* m_player;

        // Internal state - Chase component owns this!
        bool m_isChasing{ false };
        bool m_isSlowedByLight{ false };
        float m_lastDistance{ 0.0f };
        float m_currentSpeed{ 0.0f };

        // Wander/Idle behavior
        vec2 m_wanderDirection{ 1.0f, 0.0f };
        float m_wanderTimer{ 0.0f };
        float m_wanderChangeInterval{ 2.0f };  // Change direction every 2 seconds
        float m_wanderSpeed{ 50.0f };          // Slower than chase speed
    };

} // namespace Gameplay






















