#pragma once

#include "Audio/AudioManager.hpp"
#include "Components/FootstepAudio.hpp"
#include "Components/Tag.hpp"
#include "Components/Transform.hpp"
#include "Components/Velocity.hpp"
#include "Core/Engine.hpp"

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
      [&](ECS::Entity, Components::Transform &transform,
          Components::Velocity &velocity, Components::FootstepAudio &footsteps) {
        const float speed = detail::FootstepSpeed(velocity.velocity);
        if (speed > footsteps.minSpeed && footsteps.variationCount > 0 &&
            !footsteps.soundPrefix.empty()) {
          footsteps.timerRemaining -= dtf;
          if (footsteps.timerRemaining <= 0.0f) {
            float interval = footsteps.interval;
            if (speed >= footsteps.sprintSpeedThreshold)
              interval *= footsteps.sprintIntervalScale;
            footsteps.timerRemaining = std::max(interval, 0.01f);

            std::uniform_int_distribution<int> dist(1,
                                                    footsteps.variationCount);
            const int variation = dist(detail::FootstepRng());
            audio.playOneShotAt(
                footsteps.soundPrefix + std::to_string(variation),
                detail::ToVec3(transform.position), Group::SFX,
                footsteps.volume);
          }
        } else {
          footsteps.timerRemaining = footsteps.resetDelay;
        }
      });
}

} // namespace Audio
