/******************************************************************************/
/*!
\file           AISystem.cpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          Implementation of AISystem state machine.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Systems/AISystem.hpp"
#include "Components/EnemyAI.hpp"
#include "Components/Health.hpp" // Added
#include "Components/LightExposure.hpp"
#include "Components/PlayerController.hpp"
#include "Components/Transform.hpp"
#include "Components/Velocity.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace Systems {

// Helper to calculate distance
static float distance(const vec2 &a, const vec2 &b) {
  float dx = a.x - b.x;
  float dy = a.y - b.y;
  return std::sqrt(dx * dx + dy * dy);
}

// Helper to normalize vector
static vec2 normalize(const vec2 &v) {
  float len = std::sqrt(v.x * v.x + v.y * v.y);
  if (len == 0.0f)
    return {0.0f, 0.0f};
  return {v.x / len, v.y / len};
}

// Helper: random float between -1 and 1
static float randomFloat() {
  return (static_cast<float>(rand()) / RAND_MAX) * 2.0f - 1.0f;
}

void AISystem::update(ECS::World &world, double dt) {
  float fDt = static_cast<float>(dt);

  // 1. Find Player Position
  vec2 playerPos{0, 0};
  bool playerFound = false;

  world.each<Components::PlayerController, Components::Transform>(
      [&](ECS::Entity, Components::PlayerController &,
          Components::Transform &t) {
        playerPos = t.position;
        playerFound = true;
      });

  // 2. Update AI Entities
  world.each<Components::EnemyAI, Components::Transform, Components::Velocity>(
      [&](ECS::Entity e, Components::EnemyAI &ai, Components::Transform &t,
          Components::Velocity &vel) {
        // Dead Check
        if (ai.state == Components::AIState::Death) {
          vel.velocity = {0.f, 0.f};
          return;
        }

        // --- Light Sensitivity Check ---
        float speedMultiplier = 1.0f;
        auto *lightExp = world.getComponent<Components::LightExposure>(e);

        // Check if in light (immediate)
        if (lightExp && lightExp->inLight) {
          speedMultiplier = ai.lightSensitivity;

          // Damage over time
          auto *health = world.getComponent<Components::Health>(e);
          if (health && !health->isDead) {
            const float DAMAGE_RATE = 20.0f; // DPS
            health->hp -= DAMAGE_RATE * fDt;

            if (health->hp <= 0.0f) {
              health->hp = 0.0f;
              health->isDead = true;
              ai.state = Components::AIState::Death;
            }
          }
        }
        // Fallback: if exposureTime > 0 but not currently in light directly?
        // Original logic checked exposureTime. Let's keep exposureTime check
        // for consistency if needed, but 'inLight' is accurate for cones.
        else if (lightExp && lightExp->exposureTime > 0.0f) {
          speedMultiplier = ai.lightSensitivity;
        }

        // Re-Check Death after damage
        if (ai.state == Components::AIState::Death) {
          vel.velocity = {0.f, 0.f};
          return;
        }

        // --- State Machine ---
        switch (ai.state) {

        // --- PATROL STATE ---
        case Components::AIState::Patrol: {
          // Check for Player
          if (playerFound) {
            float distToPlayer = distance(t.position, playerPos);
            if (distToPlayer < ai.detectionRadius) {
              ai.state = Components::AIState::Chase;
              // Reset Chase params if needed
              return;
            }
          }

          // Wander Logic
          ai.wanderTimer -= fDt;
          if (ai.wanderTimer <= 0.0f) {
            // New random direction
            ai.wanderDirection = normalize({randomFloat(), randomFloat()});
            ai.wanderTimer = ai.wanderInterval;
          }

          // Set Velocity
          vel.moveSpeed = ai.patrolSpeed * speedMultiplier;
          vel.velocity = ai.wanderDirection;
          break;
        }

        // --- CHASE STATE ---
        case Components::AIState::Chase: {
          // Lost Player?
          if (!playerFound) {
            ai.state = Components::AIState::Alert;
            ai.stateTimer = ai.alertDuration;
            ai.targetPosition = t.position; // Stay put or search here
            return;
          }

          float distToPlayer = distance(t.position, playerPos);
          // Hysteresis: needs to go further to lose sight (1.5x)
          if (distToPlayer > ai.detectionRadius * 1.5f) {
            ai.state = Components::AIState::Alert;
            ai.stateTimer = ai.alertDuration;
            ai.targetPosition = playerPos; // Go to where we last saw them
            return;
          }

          // Move towards player
          vec2 dir = normalize(
              {playerPos.x - t.position.x, playerPos.y - t.position.y});

          vel.moveSpeed = ai.chaseSpeed * speedMultiplier;
          vel.velocity = dir;

          // Update known position
          ai.targetPosition = playerPos;
          break;
        }

        // --- ALERT STATE ---
        case Components::AIState::Alert: {
          // Timer
          ai.stateTimer -= fDt;
          if (ai.stateTimer <= 0.0f) {
            ai.state = Components::AIState::Patrol;
            ai.wanderTimer = 0.0f; // Force new direction immediately
            return;
          }

          // Check for Player
          if (playerFound) {
            if (distance(t.position, playerPos) < ai.detectionRadius) {
              ai.state = Components::AIState::Chase;
              return;
            }
          }

          // Behavior: Go to target position, then wait
          float distToTarget = distance(t.position, ai.targetPosition);
          if (distToTarget > 20.0f) {
            // Move to target
            vec2 dir = normalize({ai.targetPosition.x - t.position.x,
                                  ai.targetPosition.y - t.position.y});
            vel.moveSpeed = ai.chaseSpeed * speedMultiplier; // Alert runs fast
            vel.velocity = dir;
          } else {
            // Arrived, look around (stand still)
            vel.velocity = {0.f, 0.f};
          }
          break;
        }

        // --- OTHER STATES ---
        default:
          vel.velocity = {0.f, 0.f};
          break;
        }
      });
}

} // namespace Systems
