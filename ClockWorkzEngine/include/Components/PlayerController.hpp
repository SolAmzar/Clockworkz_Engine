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
#include "Components/LuaScript.hpp"

namespace Components {

struct PlayerController {
  bool initialized{false};

  // Speed multipliers
  float walkMultiplier{1.0f};   // Normal walking speed
  float sprintMultiplier{2.0f}; // Sprint speed multiplier

  // Footsteps
  // Footsteps
  float footstepTimer{0.0f};
  float footstepInterval{0.6f};

  // Torch Bobbing
  vec2 m_baseTorchOffset{-9999.0f, -9999.0f}; // Sentinel value
  float m_bobTimer{0.0f};
  float m_bobSpeed{10.0f};
  float m_bobAmount{2.0f};

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

    // Check if movement is handled by a Lua script
    auto *luaScript = world.getComponent<LuaScript>(entity);

    if (vel && trans) {
      if (luaScript && luaScript->enabled) {
        // --- Lua handles movement; infer animation state from velocity ---
        float velMag = std::sqrt(vel->velocity.x * vel->velocity.x +
                                 vel->velocity.y * vel->velocity.y);
        if (velMag > 0.01f) {
          bool isSprinting = input.isKeyDown(GLFW_KEY_LEFT_SHIFT) ||
                             input.isKeyDown(GLFW_KEY_RIGHT_SHIFT);
          currentState = isSprinting ? "run" : "walk";

          // Footstep Logic
          footstepTimer -= static_cast<float>(dt);
          if (footstepTimer <= 0.0f) {
            float interval = footstepInterval;
            if (isSprinting)
              interval *= 0.6f;
            footstepTimer = interval;

            std::string surfaceInfo = "concrete";
            if (col) {
              world.each<Transform, Collider, Gameplay::SurfaceComponent>(
                  [&](ECS::Entity e, Transform &tSurface, Collider &cSurface,
                      Gameplay::SurfaceComponent &surfComp) {
                    if (e.id == entity.id)
                      return;
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
            int variation = (std::rand() % 4) + 1;
            std::string soundName =
                "fs_" + surfaceInfo + "_" + std::to_string(variation);
            Assets::SoundManager::Play(soundName);
          }
        } else {
          currentState = "idle";
          footstepTimer = 0.05f;
        }
      } else {
        // --- Original C++ movement (fallback when no LuaScript) ---
        float forwardInput = 0.f;
        if (input.isKeyDown(GLFW_KEY_W))
          forwardInput += 1.f;
        if (input.isKeyDown(GLFW_KEY_S))
          forwardInput -= 1.f;

        if (forwardInput != 0.f) {
          bool isSprinting = input.isKeyDown(GLFW_KEY_LEFT_SHIFT) ||
                             input.isKeyDown(GLFW_KEY_RIGHT_SHIFT);

          float speedMultiplier =
              isSprinting ? sprintMultiplier : walkMultiplier;
          float currentSpeed = vel->moveSpeed * speedMultiplier;

          float playerAngle = trans->rotation;
          vec2 forward{std::cos(playerAngle), std::sin(playerAngle)};
          vec2 moveDir = forward * forwardInput;

          float moveLength =
              std::sqrt(moveDir.x * moveDir.x + moveDir.y * moveDir.y);
          if (moveLength > 0.f) {
            moveDir.x /= moveLength;
            moveDir.y /= moveLength;
          }

          vel->velocity = {moveDir.x * currentSpeed, moveDir.y * currentSpeed};

          if (isSprinting) {
            currentState = "run";
          } else {
            currentState = "walk";
          }

          footstepTimer -= static_cast<float>(dt);
          if (footstepTimer <= 0.0f) {
            float interval = footstepInterval;
            if (isSprinting)
              interval *= 0.6f;
            footstepTimer = interval;

            std::string surfaceInfo = "concrete";
            if (col) {
              world.each<Transform, Collider, Gameplay::SurfaceComponent>(
                  [&](ECS::Entity e, Transform &tSurface, Collider &cSurface,
                      Gameplay::SurfaceComponent &surfComp) {
                    if (e.id == entity.id)
                      return;
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
            int variation = (std::rand() % 4) + 1;
            std::string soundName =
                "fs_" + surfaceInfo + "_" + std::to_string(variation);
            Assets::SoundManager::Play(soundName);
          }

        } else {
          vel->velocity = {0.f, 0.f};
          currentState = "idle";
          footstepTimer = 0.05f;
        }
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

    // Dynamic Torch Sync
    if (torch) {
      // 1. Capture Base Offset (once, to respect editor settings)
      if (m_baseTorchOffset.x < -9000.0f) {
        m_baseTorchOffset = {torch->offset.x, torch->offset.y};
      }

      // 2. Determine State Modifiers
      float bobOffset = 0.0f;
      vec2 stateOffset = {0.0f, 0.0f};
      float currentBobSpeed = m_bobSpeed;

      if (currentState.find("run") != std::string::npos) {
        stateOffset = {10.0f, 0.0f};         // Shift forward when running
        currentBobSpeed = m_bobSpeed * 1.5f; // Faster bob
      } else if (currentState.find("walk") != std::string::npos) {
        stateOffset = {5.0f, 0.0f}; // Slight shift forward
      }

      // 3. Update Bobbing Timer if moving
      if (currentState != "idle" && currentState != "idle_torch") {
        m_bobTimer += currentBobSpeed * static_cast<float>(dt);
        // Sine wave bobbing (Vertical usually fits walking best)
        // Adjust phase or axis as needed
        bobOffset = std::sin(m_bobTimer) * m_bobAmount;
      } else {
        // Reset/Decay bobbing when idle? Or keep it?
        // Let's reset phase for consistency on start move
        m_bobTimer = 0.0f;
        bobOffset = 0.0f;
      }

      // 4. Apply Final Offset
      // Base + StateShift + (0, Bob)
      // Note: This overrides torch->offset every frame relative to base.
      // If user changes Torch Offset in editor *while playing*, it won't
      // reflect unless we detect change or re-capture. But for now, this is
      // safer than hardcoding.
      torch->offset.x = m_baseTorchOffset.x + stateOffset.x;
      torch->offset.y = m_baseTorchOffset.y + stateOffset.y + bobOffset;
    }
  }

  void OnExit(ECS::Entity entity, ECS::World &world) {
    (void)entity;
    (void)world;
    std::cout << "PlayerController Stopped!" << std::endl;
  }
};

} // namespace Components
