/******************************************************************************/
/*!
\file           AISystem.cpp
\project        ClockWorkz Engine
\author(s)      [Lebon] [Syafiq][Amzar]
\brief          Implementation of AISystem state machine.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Systems/AISystem.hpp"
#include "Assets/SoundManager.hpp"
#include "Components/Collider.hpp"
#include "Components/EnemyAI.hpp"
#include "Components/Health.hpp" // Added
#include "Components/Layer.hpp"
#include "Components/LightExposure.hpp"
#include "Components/Tag.hpp"
#include "Components/SpriteAnimator.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/Transform.hpp"
#include "Components/Velocity.hpp"
#include "Components/Torch.hpp"
#include "Components/Battery.hpp"
#include "Gameplay/Components/ParticleEmitter.hpp" // Added
#include <array>
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

void AISystem::update(ECS::World &world, double dt,
                      const std::vector<bool> *layerEnabled) {
  float fDt = static_cast<float>(dt);

  // 1. Find Player Position
  vec2 playerPos{0, 0};
  bool playerFound = false;
  bool playerDead = false;

  world.each<Components::Tag, Components::Transform>(
      [&](ECS::Entity e, Components::Tag &tag,
          Components::Transform &t) {
        if (tag.tag != "Player") return;
        playerPos = t.position;
        playerFound = true;
        if (auto *anim = world.getComponent<Components::SpriteAnimator>(e)) {
          if (anim->currentClip == "death")
            playerDead = true;
        }
      });
  if (playerDead)
    playerFound = false;

  // 1b. Find Player Torch info (for light-attracted enemies)
  vec2 torchBeamTarget{0, 0};
  bool torchActive = false;

  world.each<Components::PlayerController, Components::Transform,
             Components::TorchComponent>(
      [&](ECS::Entity torchEnt, Components::PlayerController &,
          Components::Transform &tTrans, Components::TorchComponent &torch) {
        if (!torch.active)
          return;
        // Check battery
        if (auto *batt = world.getComponent<Components::Battery>(torchEnt)) {
          if (batt->charge <= 0.0f)
            return;
        }
        torchActive = true;
        float angle = tTrans.rotation;
        float c = std::cos(angle);
        float s = std::sin(angle);
        // Compute torch world position (matching LightExposureSystem)
        vec2 localOffset = {torch.offset.x * tTrans.scale.x,
                            torch.offset.y * tTrans.scale.y};
        vec2 worldOffset = {localOffset.x * c - localOffset.y * s,
                            localOffset.x * s + localOffset.y * c};
        vec2 torchPos = tTrans.position + worldOffset;
        vec2 torchDir = {c, s};
        torchBeamTarget = torchPos + torchDir * (torch.radius * 0.7f);
      });

  // Helper lambda to check if entity's layer is enabled
  auto isLayerActive = [&](ECS::Entity e) -> bool {
    if (!layerEnabled)
      return true;
    if (auto *layer = world.getComponent<Components::Layer>(e)) {
      int idx = layer->index;
      if (idx >= 0 && idx < static_cast<int>(layerEnabled->size()) &&
          !(*layerEnabled)[idx]) {
        return false;
      }
    }
    return true;
  };

  auto disableColliderForDeath = [&](ECS::Entity e, Components::EnemyAI &ai) {
    auto *col = world.getComponent<Components::Collider>(e);
    if (!col)
      return;
    if (!ai.colliderCached) {
      ai.colliderWasSolid = col->isSolid;
      ai.colliderWasTrigger = col->isTrigger;
      ai.colliderCached = true;
    }
    col->isSolid = false;
    col->isTrigger = false;
  };

  auto restoreColliderFromDeath = [&](ECS::Entity e, Components::EnemyAI &ai) {
    auto *col = world.getComponent<Components::Collider>(e);
    if (!col || !ai.colliderCached)
      return;
    col->isSolid = ai.colliderWasSolid;
    col->isTrigger = ai.colliderWasTrigger;
  };

  // 2. Update AI Entities
  world.each<Components::EnemyAI, Components::Transform, Components::Velocity>(
      [&](ECS::Entity e, Components::EnemyAI &ai, Components::Transform &t,
          Components::Velocity &vel) {
        // Ensure enemy colliders are moveable so they get pushed out of walls
        if (auto *col = world.getComponent<Components::Collider>(e)) {
          if (!col->moveable) col->moveable = true;
        }
        // Skip entities on disabled layers
        if (!isLayerActive(e))
          return;

        Components::AIState prevState = ai.previousState;
        // Check for Revive (Transition from Death -> !Death)
        if (prevState == Components::AIState::Death &&
            ai.state != Components::AIState::Death) {
          // Restore Alpha
          auto *sprite = world.getComponent<Components::SpriteRenderer>(e);
          if (sprite) {
            sprite->color.a = 1.0f;
          }

          // Restore Health
          auto *health = world.getComponent<Components::Health>(e);
          if (health) {
            health->hp = health->maxHp;
            health->isDead = false;
          }

          // Restore Collider
          restoreColliderFromDeath(e, ai);
        }

        // Update previous state at the start/end of the loop
        ai.previousState = ai.state;

        auto playDeathSfxIfNeeded = [&]() {
          if (ai.state != Components::AIState::Death ||
              prevState == Components::AIState::Death) {
            return;
          }
          static const std::array<const char *, 3> deathIds = {
              "enemy_death_1", "enemy_death_2", "enemy_death_3"};
          const char *id =
              deathIds[static_cast<size_t>(rand()) % deathIds.size()];
          Assets::SoundManager::Play(id);
        };

        // Dead Check
        if (ai.state == Components::AIState::Death) {
          playDeathSfxIfNeeded();
          vel.velocity = {0.f, 0.f};
          disableColliderForDeath(e, ai);

          // 1. Sprite Fading
          auto *sprite = world.getComponent<Components::SpriteRenderer>(e);
          bool fullyFaded = false;
          if (sprite) {
            float fadeSpeed = 2.0f; // Fade out over 0.5s
            sprite->color.a -= fadeSpeed * fDt;
            if (sprite->color.a <= 0.0f) {
              sprite->color.a = 0.0f;
              fullyFaded = true;
            }

            // 2. Particle Burst on Death Start
            // Using alpha as a flag: if alpha is just starting to fade, trigger
            // burst Or use a flag in AI/Health? Using alpha > 0.9 check is a
            // cheap hack, but let's assume we just entered death. Better: Check
            // if we haven't burst yet. But we don't have a flag. WORKAROUND:
            // Trigger burst if alpha is still high (e.g. > 0.9) AND we are in
            // Death state. Since we subtract alpha immediately, we can use a
            // range. Actually, best might be to use a static set or component
            // flag. For now, let's use the alpha check: if (oldAlpha > 0 &&
            // newAlpha <= 0) -> burst? No, burst at start. If alpha is near 1.0
            // (start of death), trigger burst.
            if (sprite->color.a > 0.8f) { // Trigger early
              auto *emitter =
                  world.getComponent<Components::ParticleEmitter>(e);
              if (emitter) {
                // Multiple bursts around center
                for (int i = 0; i < 5; ++i) {
                  float rx = (rand() % 100 - 50) / 10.0f; // -5 to 5 offset
                  float ry = (rand() % 100 - 50) / 10.0f;
                  emitter->burst(t.position + vec2{rx, ry}, 5);
                }
              }
            }
          }
          return;
        }

        if (playerDead && ai.state == Components::AIState::Chase) {
          ai.state = Components::AIState::Patrol;
          ai.wanderTimer = 0.0f;
          vel.velocity = {0.f, 0.f};
        }

        // --- Light Sensitivity Check ---
        // Light-attracted enemies are immune to light damage/slowdown
        float speedMultiplier = 1.0f;
        auto *lightExp = world.getComponent<Components::LightExposure>(e);

        if (!ai.attractedToLight) {
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
        }

        // Re-Check Death after damage
        if (ai.state == Components::AIState::Death) {
          playDeathSfxIfNeeded();
          vel.velocity = {0.f, 0.f};
          disableColliderForDeath(e, ai);
          return;
        }

        // --- Animation Control ---
        auto *animPtr = world.getComponent<Components::SpriteAnimator>(e);
        if (animPtr) {
          auto &anim = *animPtr;
          // Threshold for movement
          if (std::abs(vel.velocity.x) > 0.1f ||
              std::abs(vel.velocity.y) > 0.1f) {
            anim.play("walk");

            // Sprite Flipping (Flip X based on movement direction)
            // Assuming default sprite faces right
            if (vel.velocity.x > 0.1f) {
              if (t.scale.x < 0)
                t.scale.x = -t.scale.x; // Make positive
            } else if (vel.velocity.x < -0.1f) {
              if (t.scale.x > 0)
                t.scale.x = -t.scale.x; // Make negative
            }
          } else {
            // If not moving, stop animation (or play idle if available)
            // For now, just stop playing walk to freeze frame or reset
            anim.playing = false;
          }
        }

        // --- State Machine ---
        switch (ai.state) {

        // --- PATROL STATE ---
        case Components::AIState::Patrol: {
          // Light-attracted enemies only react when actually hit by the light
          if (ai.attractedToLight) {
            if (torchActive && playerFound && lightExp && lightExp->inLight) {
              ai.state = Components::AIState::FlashlightChase;
              return;
            }
          } else {
            // Normal enemies chase the player
            if (playerFound) {
              float distToPlayer = distance(t.position, playerPos);
              if (distToPlayer < ai.detectionRadius) {
                ai.state = Components::AIState::Chase;
                return;
              }
            }
          }

          // Light-attracted enemies stand still when no light
          if (ai.attractedToLight) {
            vel.velocity = {0.f, 0.f};
            break;
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

        // --- FLASHLIGHT CHASE STATE ---
        case Components::AIState::FlashlightChase: {
          // Stop chasing if light is off or no longer hitting this enemy
          if (!torchActive || !playerFound || !lightExp || !lightExp->inLight) {
            ai.state = Components::AIState::Patrol;
            vel.velocity = {0.f, 0.f};
            return;
          }

          // Move toward the player (attracted by their light)
          vec2 dir = normalize(
              {playerPos.x - t.position.x, playerPos.y - t.position.y});

          vel.moveSpeed = ai.chaseSpeed * speedMultiplier;
          vel.velocity = dir;

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

          // Re-acquire target
          if (ai.attractedToLight) {
            if (torchActive && playerFound && lightExp && lightExp->inLight) {
              ai.state = Components::AIState::FlashlightChase;
              return;
            }
          } else {
            if (playerFound && distance(t.position, playerPos) < ai.detectionRadius) {
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
