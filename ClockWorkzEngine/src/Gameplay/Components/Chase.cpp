/******************************************************************************/
/*!
\file           Chase.cpp (DEBUG VERSION)
\project        Lights Out
\author(s)      [Amzar], 50% [Lebon], 30% [Syafiq], 20%
\brief          DEBUG VERSION with extensive logging to diagnose light slowdown

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "Gameplay/Components/Chase.hpp"
#include "Gameplay/GameObject.hpp"
#include "Gameplay/Components/SpriteAnimator.hpp"
#include <cmath>
#include <cstdlib>  // For rand()
#include <iostream> // For debug output

namespace Gameplay {

    namespace {
        // Helper: project vector v onto plane with normal n (slide along walls)
        inline vec2 slideAgainst(vec2 v, vec2 n) {
            float intoWall = dot(v, n);
            if (intoWall > 0.0f) return v;  // Moving away from wall
            return v - intoWall * n;         // Subtract blocked component
        }

        // Helper: clean up floating point noise
        inline float cleanFloat(float value) {
            return (std::abs(value) < 1e-6f) ? 0.0f : value;
        }

        // Helper: random float between -1 and 1
        inline float randomFloat() {
            return (static_cast<float>(rand()) / RAND_MAX) * 2.0f - 1.0f;
        }
    }

    Chase::Chase(GameObject& enemy, GameObject* player, float speed, float engageRadius)
        : m_enemy(enemy)
        , m_player(player)
        , m_speed(speed)
        , m_engageRadius(engageRadius)
        , m_stopDist(DEFAULT_STOP_DISTANCE)
        , m_lightSlowdownFactor(DEFAULT_LIGHT_SLOWDOWN)
        , m_isChasing(false)
        , m_isSlowedByLight(false)
        , m_lastDistance(0.0f)
        , m_currentSpeed(speed)
    {
        // DEBUG: Log construction
        std::cout << "[Chase DEBUG] Created for '" << m_enemy.name()
            << "' | Speed: " << m_speed
            << " | Slowdown Factor: " << m_lightSlowdownFactor << std::endl;

        // Update animation if sprite animator exists
        if (auto* spriteAnimator = enemy.getComponent<SpriteAnimatorComponent>()) {
                spriteAnimator->play("walk");
        }
    }

    bool Chase::shouldEngageTarget(float distance) const {
        // No engage radius means always chase
        if (m_engageRadius <= 0.0f) {
            return true;
        }

        if (auto* anim = m_enemy.getComponent<SpriteAnimatorComponent>()) {
            if (anim->currentState() == "attack" && anim->isPlaying()) {
                return true;
            }
        }

        SpriteAnimatorComponent* playerAnim = m_player->getComponent<SpriteAnimatorComponent>();
        if (playerAnim->currentState() == "death") {
            return false;
        }

        // Only chase if within engage radius
        return distance <= m_engageRadius;
    }

    float Chase::calculateEffectiveSpeed() const {
        // Check if enemy is exposed to light
        float lightExposure = m_enemy.lightExposureTime();

        if (lightExposure > 0.0f) {
            // Enemy is in light - apply slowdown
            float slowedSpeed = m_speed * m_lightSlowdownFactor;
            return slowedSpeed;
        }

        // Not in light - full speed
        return m_speed;
    }

    vec2 Chase::calculateDesiredVelocity(const vec2& from, const vec2& to) const {
        vec2 d = to - from;
        float dist = magnitude(d);  // Using magnitude() instead of length()

        // Avoid division by zero
        if (dist < 1e-6f) {
            return { 0.0f, 0.0f };
        }

        // Normalize direction
        vec2 dir = d / dist;
        dir.x = cleanFloat(dir.x);
        dir.y = cleanFloat(dir.y);

        // Calculate desired velocity with effective speed (considering light slowdown)
        float effectiveSpeed = calculateEffectiveSpeed();

        vec2 desiredVel = dir * effectiveSpeed;
        desiredVel.x = cleanFloat(desiredVel.x);
        desiredVel.y = cleanFloat(desiredVel.y);

        // Respect collision normal - slide along walls
        vec2 hitNormal = m_enemy.lastHitNormal();
        if (hitNormal.x != 0.0f || hitNormal.y != 0.0f) {
            desiredVel = slideAgainst(desiredVel, hitNormal);
        }

        return desiredVel;
    }

    void Chase::applyVelocity(GameObject& enemy, const vec2& velocity, double dt) {

        if (enemy.hasVelocity()) {
            // Use velocity-based movement (integrated by physics system)
            enemy.velocity() = velocity;
        }
        else {
            // Fallback: direct kinematic movement
            enemy.transform().position += velocity * static_cast<f32>(dt);
            enemy.markDirty();
        }

        if (magnitude(velocity) > 0.001f) {
            float angleRad = atan2(velocity.y, velocity.x);  // radians
            enemy.transform().rotationRad = angleRad;
        }

    }

    vec2 Chase::generateRandomDirection() const {
        // Generate a random angle
        float angle = randomFloat() * 3.14159f;  // -PI to PI
        return vec2{ std::cos(angle), std::sin(angle) };
    }

    void Chase::updateWanderBehavior(GameObject& enemy, double dt) {

        if (auto* anim = enemy.getComponent<SpriteAnimatorComponent>()) {
            if (anim->currentState() == "attack" && anim->isPlaying()) {
                return;
            }
            else {
                anim->play("walk");
            }
        }
        // Update wander timer
        m_wanderTimer += static_cast<float>(dt);

        // Change direction periodically
        if (m_wanderTimer >= m_wanderChangeInterval) {
            m_wanderDirection = generateRandomDirection();
            m_wanderTimer = 0.0f;
            std::cout << "[Chase DEBUG] Wander direction changed for '" << m_enemy.name() << "'" << std::endl;
        }

        // Calculate wander velocity
        vec2 wanderVel = m_wanderDirection * m_wanderSpeed;

        // Respect collision normal - slide along walls
        vec2 hitNormal = m_enemy.lastHitNormal();
        if (hitNormal.x != 0.0f || hitNormal.y != 0.0f) {
            wanderVel = slideAgainst(wanderVel, hitNormal);
            // Also change direction when hitting a wall
            m_wanderDirection = generateRandomDirection();
            m_wanderTimer = 0.0f;
        }
        

        // Apply wander movement
        applyVelocity(enemy, wanderVel, dt);
    }

    void Chase::onUpdate(GameObject& enemy, double dt) {

        // Validate target exists
        if (!m_player) {
            m_isChasing = false;
            m_isSlowedByLight = false;
            m_currentSpeed = 0.0f;
            return;
        }



        // Calculate distance to target
        vec2 from = m_enemy.transform().position;
        vec2 to = m_player->transform().position;
        vec2 d = to - from;
        float distSquared = dot(d, d);
        m_lastDistance = std::sqrt(distSquared);


        // Check if too close - stop to avoid jitter
        /*if (distSquared <= m_stopDist * m_stopDist) {
            m_isChasing = false;
            m_isSlowedByLight = false;
            m_currentSpeed = 0.0f;


            if (m_enemy.hasVelocity()) {
                m_enemy.velocity() = { 0.0f, 0.0f };
            }
            return;
        }*/
        // Determine if should engage target
        m_isChasing = shouldEngageTarget(m_lastDistance);

        // Check if slowed by light - THIS IS THE KEY CHECK
        float currentLightExposure = m_enemy.lightExposureTime();
        m_isSlowedByLight = (currentLightExposure > 0.0f);
        m_currentSpeed = calculateEffectiveSpeed();

        

        // Stop movement if not chasing - switch to wander behavior
        if (!m_isChasing) {
            updateWanderBehavior(enemy, dt);
            return;
        }
        else {
            // Calculate and apply chase movement (with light slowdown applied)
            vec2 desiredVel = calculateDesiredVelocity(from, to);
            applyVelocity(enemy, desiredVel, dt);
        }


    }

} // namespace Gameplay