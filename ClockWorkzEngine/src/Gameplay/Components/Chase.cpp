/******************************************************************************/
/*!
\file           Chase.cpp
\project        Lights Out
\author(s)      [Amzar], 10%
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

//// Gameplay/Components/Chase.cpp
//#include "Gameplay/Components/Chase.hpp"
//#include "Gameplay/GameObject.hpp"
//
//namespace Gameplay {
//
//    namespace {
//        // Helper: project vector v onto plane with normal n (remove into-wall component)
//        inline vec2 slideAgainst(vec2 v, vec2 n) {
//            float intoWall = dot(v, n);
//            if (intoWall > 0.0f) return v;        // moving away from wall already
//            return v - intoWall * n;              // subtract the blocked component
//        }
//    }
//
//    Chase::Chase(GameObject& owner, float speed, float engageRadius)
//        : m_owner(owner), m_speed(speed), m_engageRadius(engageRadius) {
//    }
//
//    void Chase::onUpdate(GameObject& owner, double dt)
//    {
//        (void)dt;
//        // 1) Locate the player. Use whatever lookup you already have.
//        // Examples you can swap depending on your engine:
//        // auto* player = m_owner.world().FindWithTag("Player");
//        // auto* player = m_owner.world().FindFirstOfType(GameObjectType::Player);
//        GameObject* player =  
//        if (!player || !player->active()) return;
//
//        // 2) Compute desired direction toward player
//        vec2 from = m_owner.transform().position;
//        vec2 to = player->transform().position;
//        vec2 d = to - from;
//        float d2 = dot(d, d);
//        if (d2 <= m_stopDist * m_stopDist) {
//            // close enough; optionally zero velocity
//            if (m_owner.hasVelocity()) m_owner.velocity() = { 0.f, 0.f };
//            return;
//        }
//
//        float dist = std::sqrt(d2);
//        vec2 dir = (1.0f / dist) * d;
//
//        // 3) Optional “only chase if near enough”
//        if (m_engageRadius > 0.0f && dist > m_engageRadius) {
//            return;
//        }
//
//        // 4) Respect walls: slide along the last collision normal your Physics2D stored
//        // Physics step writes setLastHitNormal(...) when it resolves a solid pair. :contentReference[oaicite:3]{index=3}
//        vec2 desiredVel = dir * m_speed;
//        vec2 n = m_owner.lastHitNormal(); // {0,0} if no recent hit
//        if (n.x != 0.f || n.y != 0.f) {
//            desiredVel = slideAgainst(desiredVel, n);
//        }
//
//        // 5) Apply movement—use the pathway your engine already uses:
//        // (A) If you have a velocity field integrated elsewhere each frame:
//        if (m_owner.hasVelocity()) {
//            m_owner.velocity() = desiredVel;
//        }
//        else {
//            // (B) Fallback: move directly (simple kinematic)
//            m_owner.transform().position += desiredVel * dt;
//            m_owner.markDirty();
//        }
//    }
//
//} // namespace Gameplay
