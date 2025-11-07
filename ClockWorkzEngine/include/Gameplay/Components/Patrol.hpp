/******************************************************************************/
/*!
\file           Patrol.hpp
\project        Lights Out
\author(s)      [Amzar], 100%
\brief          Declares PatrolComponent, a component of a GameObject that makes it
                patrol between two points A and B at a specified speed.
                Responsibilities include:
                - Moving the GameObject back and forth between points A and B
                - Updating the GameObject's velocity each frame to achieve patrol behavior
				Used to create patrolling enemies or moving platforms in the game.
             

All content ? 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
#include "Gameplay/GameObject.hpp"
#include "Math/Vec2.hpp"
#include "Math/config.hpp"
namespace Gameplay {
    class PatrolComponent : public Component {
    public:
        enum class Mode { PingPong, Loop, OneShot };

        PatrolComponent(vec2 a, vec2 b, float speed, Mode m = Mode::PingPong)
            : m_a(a), m_b(b), m_speed(speed), m_mode(m) {
        }

        vec2 a() const { return m_a; }
        vec2 b() const { return m_b; }
        float speed() const { return m_speed; }
        bool goingToB() const { return m_toB; }
        Mode mode() const { return m_mode; }

        void setSpeed(float s) { m_speed = s < 0.f ? 0.f : s; }
        void setA(vec2 A, GameObject& owner, bool preserveProgress = true) { setTargets(A, m_b, owner, preserveProgress); }
        void setB(vec2 B, GameObject& owner, bool preserveProgress = true) { setTargets(m_a, B, owner, preserveProgress); }
        void setTargets(vec2 A, vec2 B, GameObject& owner, bool preserveProgress = true) {
            if (!preserveProgress) { m_a = A; m_b = B; return; }
            float t = projectParam(owner.transform().position, currentStart(), currentEnd());
            m_a = A; m_b = B;
            owner.transform().position = lerp(currentStart(), currentEnd(), t); // smooth remap
        }
        void setMode(Mode m) { m_mode = m; }
        void flipDirection() { m_toB = !m_toB; }
        void setPaused(bool p) { m_paused = p; }

        // Pre-physics: best path (velocity -> physics resolver)
        void onBeforePhysics(GameObject& owner, double dt) override {
            m_ranThisFrame = true;                 // mark so onUpdate won't re-run
            applyDesiredVelocity(owner, dt);
        }

        // Post-physics: fallback if your engine doesn't have the pre-physics hook
        // (movement will start next frame because integration already happened)
        void onUpdate(GameObject& owner, double dt) override {
            if (m_ranThisFrame) {                  // already handled in pre-physics this frame
                m_ranThisFrame = false;            // reset for next frame
                return;
            }
            applyDesiredVelocity(owner, dt);
            // no reset here; we reset at the start of the next frame when pre-physics runs
        }

        

    private:
        // one implementation used by both hooks
        void applyDesiredVelocity(GameObject& owner, double dt) {
            vec2 p = owner.transform().position;
            vec2 target = m_toB ? m_b : m_a;
            vec2 toT = target - p;
            float dist = magnitude(toT);

            vec2 bias = normalize(vec2{ target.x - p.x, target.y - p.y });
            target += bias * 4.f; // 2–4 units to avoid scraping exact corners

            // arrival
            if (dist <= m_arriveEps) { m_toB = !m_toB; owner.setVelocity({ 0,0 }); m_stuckTime = 0.f; m_lastDist = FLT_MAX; return; }

            // desired straight-line velocity
            vec2 v_des = (dist > 1e-4f) ? (toT / dist) * m_speed : vec2{ 0,0 };

            // --- slide along wall if we touched something this frame ---
            vec2 n = owner.lastHitNormal(); // requires step() to set it (see #1)
            if (n.x != 0.f || n.y != 0.f) {
                // remove normal component -> keep tangent
                vec2 v_slide = v_des - n * dot(v_des, n);
                float ls = magnitude(v_slide);
                if (ls > 1e-3f) {
                    owner.setVelocity((v_slide / ls) * m_speed);
                }
                else {
                    // pick a perpendicular to n (choose side that best aligns with v_des)
                    vec2 t1 = vec2{ -n.y, n.x };
                    vec2 t2 = vec2{ n.y,-n.x };
                    vec2 t = (dot(t1, v_des) > dot(t2, v_des)) ? t1 : t2;
                    owner.setVelocity(t * m_speed * 0.9f);
                }
                // continue; physics will integrate and keep us hugging the wall
                return;
            }

            // --- simple stuck detection & nudge ---
            if (dist < m_lastDist - 0.5f) { m_stuckTime = 0.f; m_lastDist = dist; }
            else { m_stuckTime += (float)dt; }

            if (m_stuckTime >= 0.4f) {
                m_stuckTime = 0.f;
                // nudge sideways (perp to desired) to round corners
                vec2 perp = vec2{ -toT.y, toT.x };
                perp = (magnitude(perp) > 1e-4f) ? (perp / magnitude(perp)) : vec2{ 0,1 };
                owner.setVelocity(perp * m_speed * 0.8f);
                return;
            }

            // default: go straight
            owner.setVelocity(v_des);
        }
    
		// Local helpers
        static float clamp01(float t) { return t < 0.f ? 0.f : (t > 1.f ? 1.f : t); }
        static float projectParam(vec2 P, vec2 A, vec2 B) {
            vec2 AB = B - A;
            float denom = dot(AB, AB);                 // your dot()
            if (denom <= 1e-12f) return 0.f;
            return clamp01(dot(P - A, AB) / denom);
        }
        vec2 currentStart() const { return m_toB ? m_a : m_b; }
        vec2 currentEnd()   const { return m_toB ? m_b : m_a; }

        // --- existing members ---
        vec2  m_a{}, m_b{};
        float m_speed{ 120.f };
		float m_speedunstuck{ 200.f };
        bool  m_toB{ true };
        bool  m_paused{ false };
        Mode  m_mode{ Mode::PingPong };

        // --- new: guards double execution when both hooks are called in the same frame
        bool  m_ranThisFrame{ false };

		// --- wall-follow / unstick state ---
        float m_arriveEps = 8.0f;     // arrival radius
        float m_stuckTime = 0.0f;
        float m_stuckThresh = 0.35f;    // seconds without progress => stuck
        float m_lastDist = FLT_MAX;
        bool  m_wallFollow = false;
        vec2  m_wallTangent = { 0.f, 0.f };
        int   m_trySide = +1;       // +1 right-hand, -1 left-hand
    };
}