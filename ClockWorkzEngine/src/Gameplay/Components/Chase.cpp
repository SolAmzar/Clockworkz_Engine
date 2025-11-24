/******************************************************************************/
/*!
\file           Chase.cpp
\project        Lights Out
\author(s)      [Amzar], 70% [Syafiq] 30%
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

// Gameplay/Components/Chase.cpp
#include "Gameplay/Components/Chase.hpp"
#include "Gameplay/GameObject.hpp"
#include <iostream> 
#include <Gameplay/Components/SpriteAnimator.hpp>

namespace Gameplay {

    namespace {
        // Helper: project vector v onto plane with normal n (remove into-
        // component)
            inline vec2 slideAgainst(vec2 v, vec2 n) {
            float intoWall = dot(v, n);
            if (intoWall > 0.0f) return v;        // moving away from wall already
            return v - intoWall * n;              // subtract the blocked component
        }
            inline float cleanFloat(float value) {
                if (std::abs(value) < 1e-6f) return 0.0f;
                return value;
            }
    }

    Chase::Chase(GameObject& enemy, GameObject* player, float speed, float engageRadius)
        : m_enemy(enemy),m_player(player), m_speed(speed), m_engageRadius(engageRadius) {
    }



    void Chase::onUpdate(GameObject& enemy, double dt)
    {
        (void)dt;
        // 1) Locate the player. Use whatever lookup you already have.
        // Examples you can swap depending on your engine:
        // auto* player = m_owner.world().FindWithTag("Player");
        /*auto* player = m_enemy.world().FindFirstOfType(GameObjectType::Player);*/


        GameObject* player =  m_player;
        if (!player) {
            std::cout << "Chase: No player found!\n";
            return;
        }

        // 2) Compute desired direction toward player
        vec2 from = m_enemy.transform().position;
        vec2 to = player->transform().position;
        vec2 d = to - from;
        float d2 = dot(d, d);

        if (d2 <= m_stopDist * m_stopDist) {
            // close enough; optionally zero velocity
            if (m_enemy.hasVelocity()) m_enemy.velocity() = { 0.f, 0.f };
            enemy.setChase(false);
            return;
        }

        float dist = std::sqrt(d2);

        static float lastPrintTime = 0;
        static float printInterval = 1.0f; // Print every 1 second
        lastPrintTime += static_cast<f32>(dt);
        if (lastPrintTime >= printInterval) {
            lastPrintTime = 0;
        }
        vec2 dir = (1.0f / dist) * d;
        dir.x = cleanFloat(dir.x);
        dir.y = cleanFloat(dir.y);
        
        // 3) Optional �only chase if near enough�
        if (m_engageRadius > 0.0f && dist > m_engageRadius) {
            enemy.setChase(false);
            if (m_enemy.hasVelocity())
            {
                m_enemy.velocity() = { 0.f, 0.f };
            }
        }
        else {
            enemy.setChase(true);
        }

        SpriteAnimatorComponent* spriteAnimator = enemy.getComponent<SpriteAnimatorComponent>();
        if (enemy.checkChase() && spriteAnimator) {
            spriteAnimator->play("walk");
        }

        if (!enemy.checkChase()) {
            return;
        }

        // 4) Respect walls: slide along the last collision normal your Physics2D stored
        // Physics step writes setLastHitNormal(...) when it resolves a solid pair. :contentReference[oaicite:3]{index=3}
        vec2 desiredVel = dir * m_speed;
        desiredVel.x = cleanFloat(desiredVel.x);
        desiredVel.y = cleanFloat(desiredVel.y);
        vec2 n = m_enemy.lastHitNormal(); // {0,0} if no recent hit
        if (n.x != 0.f || n.y != 0.f) {
            desiredVel = slideAgainst(desiredVel, n);
        }
        // 5) Apply movement�use the pathway your engine already uses:
        // (A) If you have a velocity field integrated elsewhere each frame:
        if (m_enemy.hasVelocity()) {
            m_enemy.velocity() = desiredVel;

        }
        else {
            // (B) Fallback: move directly (simple kinematic)
            m_enemy.transform().position += desiredVel * static_cast<f32>(dt);
            m_enemy.markDirty();
        }
    }
} // namespace Gameplay
