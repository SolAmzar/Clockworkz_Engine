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
#include "Components/Transform.hpp"
#include "Components/Velocity.hpp"
#include <cmath>
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

void AISystem::update(ECS::World &world, double dt) {
  float fDt = static_cast<float>(dt);

  // For this simple implementation, we assume the player is entity 0 for
  // "Chase" target In a real game, we'd look up the player entity by Tag or
  // Component. We'll try to find a "Player" entity if possible, or just use
  // Entity 0 as a placeholder if active. Actually, let's just use mouse
  // position or a known entity for testing. For now: Assume the FIRST entity is
  // the Player.

  vec2 playerPos{0, 0};
  bool playerFound = false;

  // Find a target: look for an entity with Transform + Velocity but NO EnemyAI
  world.each<Components::Transform>([&](ECS::Entity e,
                                        Components::Transform &t) {
    if (playerFound)
      return; // Optimization: just find the first one

    bool isEnemy = (world.getComponent<Components::EnemyAI>(e) != nullptr);
    bool hasVelocity = (world.getComponent<Components::Velocity>(e) != nullptr);

    if (!isEnemy && hasVelocity) {
      playerPos = t.position;
      playerFound = true;
    }
  });

  world.each<Components::EnemyAI>([&](ECS::Entity entity,
                                      Components::EnemyAI &ai) {
    auto *transform = world.getComponent<Components::Transform>(entity);
    auto *velocity = world.getComponent<Components::Velocity>(entity);

    if (!transform || !velocity)
      return;

    switch (ai.state) {
    // -----------------------------------------------------------------
    // PATROL STATE
    // -----------------------------------------------------------------
    case Components::AIState::Patrol: {
      // Check for Player
      if (playerFound) {
        float distToPlayer = distance(transform->position, playerPos);
        if (distToPlayer < ai.detectionRadius) {
          ai.state = Components::AIState::Chase;
          // std::cout << "Enemy detected player! Switching to CHASE." <<
          // std::endl;
          return;
        }
      }

      // Patrol Logic
      if (ai.waypoints.empty()) {
        velocity->velocity = {0.f, 0.f}; // No waypoints, stand still
        return;
      }

      vec2 target = ai.waypoints[ai.currentWaypoint];
      float dist = distance(transform->position, target);

      if (dist < ai.waypointRadius) {
        // Reached waypoint, next one
        if (ai.loop) {
          ai.currentWaypoint = (ai.currentWaypoint + 1) % ai.waypoints.size();
        } else {
          // Should reverse logic here if implemented, for now just wrap or stop
          ai.currentWaypoint = (ai.currentWaypoint + 1) % ai.waypoints.size();
        }
      } else {
        // Move towards waypoint
        vec2 dir = normalize({target.x - transform->position.x,
                              target.y - transform->position.y});
        velocity->velocity.x = dir.x * ai.patrolSpeed;
        velocity->velocity.y = dir.y * ai.patrolSpeed;
      }
      break;
    }

    // -----------------------------------------------------------------
    // CHASE STATE
    // -----------------------------------------------------------------
    case Components::AIState::Chase: {
      if (!playerFound) {
        ai.state = Components::AIState::Alert;
        ai.alertTimer = ai.searchDuration;
        return;
      }

      float distToPlayer = distance(transform->position, playerPos);
      if (distToPlayer >
          ai.detectionRadius *
              1.5f) { // Hysteresis: needs to go further to lose sight
        ai.state = Components::AIState::Alert;
        ai.alertTimer = ai.searchDuration;
        // std::cout << "Lost sight of player. Switching to ALERT." <<
        // std::endl;
      } else {
        // Move towards player
        vec2 dir = normalize({playerPos.x - transform->position.x,
                              playerPos.y - transform->position.y});
        velocity->velocity.x = dir.x * ai.chaseSpeed;
        velocity->velocity.y = dir.y * ai.chaseSpeed;
      }
      break;
    }

    // -----------------------------------------------------------------
    // ALERT STATE
    // -----------------------------------------------------------------
    case Components::AIState::Alert: {
      // Stop moving
      velocity->velocity = {0.f, 0.f};

      // Countdown
      ai.alertTimer -= fDt;
      if (ai.alertTimer <= 0.f) {
        ai.state = Components::AIState::Patrol;
        // std::cout << "Given up search. Returning to PATROL." << std::endl;
      }

      // Optional: Could still spot player here immediately
      if (playerFound &&
          distance(transform->position, playerPos) < ai.detectionRadius) {
        ai.state = Components::AIState::Chase;
      }
      break;
    }

    // -----------------------------------------------------------------
    // DEATH STATE
    // -----------------------------------------------------------------
    case Components::AIState::Death: {
      // Stop moving
      velocity->velocity = {0.f, 0.f};
      // Do nothing else. Logic for cleanup or animation would go here.
      break;
    }
    }
  });
}

} // namespace Systems
