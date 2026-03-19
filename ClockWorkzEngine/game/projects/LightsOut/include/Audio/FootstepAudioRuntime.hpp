/******************************************************************************/
/*!
\file           FootstepAudioRuntime.hpp
\project        Lights Out
\author(s)      [Sol-Amzar] 100%
\brief          Declares runtime helpers for footstep surface audio playback in
                Lights Out.

All content © 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once

#include "Audio/AudioManager.hpp"
#include "Components/Collider.hpp"
#include "Components/FootstepAudio.hpp"
#include "Components/Tag.hpp"
#include "Components/Transform.hpp"
#include "Components/Velocity.hpp"
#include "Core/Engine.hpp"
#include "Gameplay/Components/SurfaceComponent.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <string>

namespace Audio {
namespace detail {

inline std::mt19937 &FootstepRng() {
  static std::mt19937 rng{std::random_device{}()};
  return rng;
}

inline float FootstepSpeed(const vec2 &velocity) {
  return std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
}

inline vec3 ToVec3(const vec2 &value, float z = 0.0f) {
  return vec3{value.x, value.y, z};
}

inline bool PointInCollider(const vec2 &point, const Components::Transform &transform,
                            const Components::Collider &collider) {
  const vec2 worldPos = transform.position + collider.offset;
  if (collider.shape == Components::Collider::Shape::Circle) {
    const float dx = point.x - worldPos.x;
    const float dy = point.y - worldPos.y;
    return (dx * dx + dy * dy) <= (collider.radius * collider.radius);
  }

  const vec2 half{(collider.size.x * transform.scale.x) * 0.5f,
                  (collider.size.y * transform.scale.y) * 0.5f};
  return point.x >= worldPos.x - half.x && point.x <= worldPos.x + half.x &&
         point.y >= worldPos.y - half.y && point.y <= worldPos.y + half.y;
}

inline Gameplay::SurfaceType ResolveSurfaceType(ECS::World &world,
                                                const vec2 &position) {
  Gameplay::SurfaceType resolved = Gameplay::SurfaceType::Default;
  world.each<Gameplay::SurfaceComponent, Components::Transform, Components::Collider>(
      [&](ECS::Entity, Gameplay::SurfaceComponent &surface,
          Components::Transform &transform, Components::Collider &collider) {
        if (resolved != Gameplay::SurfaceType::Default)
          return;
        if (PointInCollider(position, transform, collider))
          resolved = surface.type();
      });
  return resolved;
}

inline const std::string &PrefixForSurface(const Components::FootstepAudio &footsteps,
                                           Gameplay::SurfaceType surface) {
  if (!footsteps.useSurfaceSounds)
    return footsteps.soundPrefix;

  switch (surface) {
  case Gameplay::SurfaceType::Tile:
    return footsteps.tilePrefix.empty() ? footsteps.soundPrefix : footsteps.tilePrefix;
  case Gameplay::SurfaceType::Wood:
    return footsteps.woodPrefix.empty() ? footsteps.soundPrefix : footsteps.woodPrefix;
  case Gameplay::SurfaceType::Concrete:
    return footsteps.concretePrefix.empty() ? footsteps.soundPrefix : footsteps.concretePrefix;
  default:
    return footsteps.soundPrefix;
  }
}

} // namespace detail

inline void UpdateFootstepListener(ECS::World &world) {
  auto &audio = Core::Engine::Get().audio();
  bool listenerSet = false;

  world.each<Components::Tag, Components::Transform>(
      [&](ECS::Entity entity, Components::Tag &tag,
          Components::Transform &transform) {
        if (listenerSet || tag.tag != "Player")
          return;

        vec3 velocity{0.0f, 0.0f, 0.0f};
        if (auto *vel = world.getComponent<Components::Velocity>(entity))
          velocity = detail::ToVec3(vel->velocity);

        const vec3 position = detail::ToVec3(transform.position);
        const vec3 forward{std::cos(transform.rotation),
                           std::sin(transform.rotation), 0.0f};
        const vec3 up{0.0f, 0.0f, 1.0f};
        audio.setListener(position, velocity, forward, up);
        listenerSet = true;
      });
}

inline void UpdateFootstepAudio(ECS::World &world, double dt) {
  auto &audio = Core::Engine::Get().audio();
  const float dtf = static_cast<float>(dt);

  world.each<Components::Transform, Components::Velocity, Components::FootstepAudio>(
      [&](ECS::Entity entity, Components::Transform &transform,
          Components::Velocity &velocity, Components::FootstepAudio &footsteps) {
        const float speed = detail::FootstepSpeed(velocity.velocity);
        if (speed > footsteps.minSpeed && footsteps.variationCount > 0) {
          footsteps.timerRemaining -= dtf;
          if (footsteps.timerRemaining <= 0.0f) {
            float interval = footsteps.interval;
            if (footsteps.sprintSpeedThreshold > footsteps.minSpeed) {
              const float normalizedSpeed =
                  std::clamp((speed - footsteps.minSpeed) /
                                 (footsteps.sprintSpeedThreshold - footsteps.minSpeed),
                             0.0f, 1.0f);
              const float intervalScale =
                  1.0f + (footsteps.sprintIntervalScale - 1.0f) * normalizedSpeed;
              interval *= intervalScale;
            } else {
              interval *= footsteps.sprintIntervalScale;
            }
            footsteps.timerRemaining = std::max(interval, 0.01f);

            const Gameplay::SurfaceType surface =
                detail::ResolveSurfaceType(world, transform.position);
            const std::string &prefix = detail::PrefixForSurface(footsteps, surface);
            if (prefix.empty())
              return;

            std::uniform_int_distribution<int> dist(1,
                                                    footsteps.variationCount);
            const int variation = dist(detail::FootstepRng());
            const std::string soundId = prefix + std::to_string(variation);

            bool isPlayerEmitter = false;
            if (auto *tag = world.getComponent<Components::Tag>(entity))
              isPlayerEmitter = (tag->tag == "Player");

            if (isPlayerEmitter) {
              audio.playOneShotLocal(soundId, Group::SFX, footsteps.volume);
            } else {
              audio.playOneShotAt(soundId, detail::ToVec3(transform.position),
                                  Group::SFX, footsteps.volume);
            }
          }
        } else {
          footsteps.timerRemaining = footsteps.resetDelay;
        }
      });
}

} // namespace Audio

