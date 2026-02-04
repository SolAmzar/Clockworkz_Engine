/******************************************************************************/
/*!
\file           PlayerController.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon] (100%)
\brief          PlayerController component - manages player input and movement
logic.

All content © 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once

#include "Components/Collider.hpp"
#include "Components/SpriteAnimator.hpp"
#include "Components/StateMachine.hpp"
#include "Components/Tag.hpp"
#include "Components/Torch.hpp"
#include "Components/Transform.hpp"
#include "Components/Velocity.hpp"
#include "Core/Engine.hpp"
#include "ECS/World.hpp"
#include "Gameplay/Components/SurfaceComponent.hpp"
#include "Math/Vec2.hpp"
#include "Platform/InputManager.hpp"
#include "Platform/WindowManager.hpp"
#include <GLFW/glfw3.h>
#include <cctype>
#include <cmath>
#include <cstdlib> // for rand()
#include <iostream>
#include <string>

#include "Assets/SoundManager.hpp"
#include "Components/Battery.hpp"

namespace Components {

struct PlayerController {
  bool initialized{false};

  // Speed multipliers
  float walkMultiplier{1.0f};   // Normal walking speed
  float sprintMultiplier{2.0f}; // Sprint speed multiplier

  // Footsteps
  float footstepTimer{0.0f};
  float footstepInterval{0.4f};

  void OnEnter(ECS::Entity entity, ECS::World &world) {
    (void)entity;
    (void)world;
    std::cout << "PlayerController Attached/Started!" << std::endl;
  }

  void Update(ECS::Entity entity, ECS::World &world, double dt,
              Platform::InputManager &input) {
    (void)dt;
    auto *vel = world.getComponent<Velocity>(entity);
    auto *trans = world.getComponent<Transform>(entity);
    auto *sm = world.getComponent<StateMachine>(entity);
    auto *anim = world.getComponent<SpriteAnimator>(entity);
    auto *torch = world.getComponent<TorchComponent>(entity);
    auto *col = world.getComponent<Components::Collider>(entity);
    auto *batt = world.getComponent<Components::Battery>(entity);

    // Disable control if dead
    if (anim && anim->currentClip == "death") {
      if (vel)
        vel->velocity = {0.f, 0.f};
      return;
    }

    // Enforce Physics Interaction: Player must be moveable to collide with
    // walls
    if (col) {
      col->moveable = true;
    }

    // --- Flashlight Logic ---
    if (torch) {
      // Toggle Flashlight with 'F'
      if (input.wasKeyPressed(GLFW_KEY_F)) {
        if (batt && batt->charge > 0.0f) {
          torch->active = !torch->active;
          std::cout << "Flashlight Toggled: " << (torch->active ? "ON" : "OFF")
                    << "\n";

          if (torch->active) {
            Assets::SoundManager::Play("flashlight_on");
          } else {
            Assets::SoundManager::Play("flashlight_off");
          }
        } else if (batt && batt->charge <= 0.0f && !torch->active) {
          // Optional: Play "click" sound for empty battery?
          Assets::SoundManager::Play("flashlight_off");
        }
      }

      // Battery Management
      if (batt) {
        if (torch->active) {
          // Drain Battery
          batt->isCharging = false;
          batt->charge -=
              batt->drainRate * static_cast<float>(dt); // Drain per second

          // Force off if empty
          if (batt->charge <= 0.0f) {
            batt->charge = 0.0f;
            torch->active = false;
            Assets::SoundManager::Play("flashlight_off");
            std::cout << "Flashlight Drained!\n";
          }
        } else {
          // Recharge Battery
          batt->isCharging = true;
          batt->charge +=
              batt->chargeRate * static_cast<float>(dt); // Charge per second
          if (batt->charge > batt->maxCharge) {
            batt->charge = batt->maxCharge;
            // batt->isCharging = false; // Keep charging status true while
            // torch is off
          }
        }
      }
    }

    std::string currentState = "idle";

    if (vel && trans) {
      // Get forward/backward input (W/S)
      float forwardInput = 0.f;
      if (input.isKeyDown(GLFW_KEY_W))
        forwardInput += 1.f;
      if (input.isKeyDown(GLFW_KEY_S))
        forwardInput -= 1.f;

      if (forwardInput != 0.f) {
        // Determine speed multiplier
        bool isSprinting = input.isKeyDown(GLFW_KEY_LEFT_SHIFT) ||
                           input.isKeyDown(GLFW_KEY_RIGHT_SHIFT);

        float speedMultiplier = isSprinting ? sprintMultiplier : walkMultiplier;
        float currentSpeed = vel->moveSpeed * speedMultiplier;

        // Calculate movement direction based on player's rotation
        float playerAngle = trans->rotation;

        // Forward direction (along player's facing)
        vec2 forward{std::cos(playerAngle), std::sin(playerAngle)};

        // Combine forward movement
        vec2 moveDir = forward * forwardInput;

        // Normalize if moving diagonally
        float moveLength =
            std::sqrt(moveDir.x * moveDir.x + moveDir.y * moveDir.y);
        if (moveLength > 0.f) {
          moveDir.x /= moveLength;
          moveDir.y /= moveLength;
        }

        vel->velocity = {moveDir.x * currentSpeed, moveDir.y * currentSpeed};

        // Update animation state
        if (isSprinting) {
          currentState = "run";
        } else {
          currentState = "walk";
        }

        // Footstep Logic
        footstepTimer -= static_cast<float>(dt);
        if (footstepTimer <= 0.0f) {
          float interval = footstepInterval;
          if (isSprinting)
            interval *= 0.6f; // Faster footsteps when running
          footstepTimer = interval;

          std::string surfaceInfo = "concrete"; // Default fallback

          // Naive Surface Detection: Check overlap with SurfaceComponent
          // entities
          if (col) {
            world.each<Transform, Collider, Gameplay::SurfaceComponent>(
                [&](ECS::Entity e, Transform &tSurface, Collider &cSurface,
                    Gameplay::SurfaceComponent &surfComp) {
                  if (e.id == entity.id)
                    return; // Skip self

                  // Simple AABB Overlap Check
                  vec2 posA = trans->position + col->offset;
                  vec2 posB = tSurface.position + cSurface.offset;
                  vec2 halfA = col->size * 0.5f;
                  vec2 halfB = cSurface.size * 0.5f;

                  bool overlap = (posA.x - halfA.x < posB.x + halfB.x &&
                                  posA.x + halfA.x > posB.x - halfB.x &&
                                  posA.y - halfA.y < posB.y + halfB.y &&
                                  posA.y + halfA.y > posB.y - halfB.y);

                  if (overlap) {
                    switch (surfComp.type()) {
                    case Gameplay::SurfaceType::Tile:
                      surfaceInfo = "tile";
                      break;
                    case Gameplay::SurfaceType::Wood:
                      surfaceInfo = "wood";
                      break;
                    case Gameplay::SurfaceType::Concrete:
                      surfaceInfo = "concrete";
                      break;
                    default:
                      surfaceInfo = "concrete";
                      break;
                    }
                  }
                });
          }

          // Randomize variation (1 to 4)
          int variation = (std::rand() % 4) + 1;
          std::string soundName =
              "fs_" + surfaceInfo + "_" + std::to_string(variation);
          Assets::SoundManager::Play(soundName);
        }

      } else {
        vel->velocity = {0.f, 0.f};
        currentState = "idle";
        footstepTimer = 0.05f; // Ready to step as soon as we move
      }
    }

    // Mouse-facing rotation
    if (trans) {
      // Get mouse world position
      auto &engine = Core::Engine::Get();
      auto &window = engine.window();
      auto &camera = engine.camera();

      vec2 mouseWorld =
          input.getMouseWorldPos(camera, static_cast<float>(window.width()),
                                 static_cast<float>(window.height()));

      // Calculate direction from player to mouse
      vec2 playerPos = trans->position;
      vec2 dir = mouseWorld - playerPos;

      // Only rotate if mouse is far enough to avoid jitter
      float dist = std::sqrt(dir.x * dir.x + dir.y * dir.y);
      if (dist > 10.0f) {
        // Normalize direction
        dir.x /= dist;
        dir.y /= dist;

        // Calculate angle in radians
        float angleRad = std::atan2(dir.y, dir.x);
        trans->rotation = angleRad;
      }
    }

    // Append _torch if torch component is present and active
    if (torch && torch->active) {
      currentState += "_torch"; // e.g. "idle_torch", "walk_torch", "run_torch"
    }

    // Update StateMachine if it exists (optional)
    if (sm) {
      sm->currentState = currentState;
    }

    // Sync Animation
    if (anim) {
      std::string targetClip = (sm) ? sm->currentState : currentState;

      if (anim->currentClip != targetClip) {
        bool clipExists = false;
        for (const auto &clip : anim->clips) {
          if (clip.first == targetClip) {
            clipExists = true;
            break;
          }
        }

        if (clipExists) {
          anim->play(targetClip);
        }
      }
    }

    // Update torch offset based on animation state
    if (torch) {
      // Adjust offset based on animation state for better visual alignment
      if (currentState.find("idle") != std::string::npos) {
        torch->offset = vec2{35.0f, -20.0f}; // Default idle pose
      } else if (currentState.find("walk") != std::string::npos) {
        torch->offset = vec2{40.0f, -15.0f}; // Walking pose - slightly forward
      } else if (currentState.find("run") != std::string::npos) {
        torch->offset = vec2{45.0f, -10.0f}; // Running pose - more forward
      }
    }
  }

  void OnExit(ECS::Entity entity, ECS::World &world) {
    (void)entity;
    (void)world;
    std::cout << "PlayerController Stopped!" << std::endl;
  }
};

} // namespace Components
