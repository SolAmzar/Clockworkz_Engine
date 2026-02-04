/******************************************************************************/
/*!
\file           AudioSystem.cpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          Implementation of AudioSystem.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Systems/AudioSystem.hpp"
#include "Audio/AudioManager.hpp"
#include "Components/AudioSource.hpp"
#include "Core/Engine.hpp"


namespace Systems {

void AudioSystem::update(ECS::World &world) {
  world.each<Components::AudioSource>(
      [&](ECS::Entity entity, Components::AudioSource &audio) {
        (void)entity;
        // Logic for auto-play or management could go here.
        // For now, we purely rely on the UI or other systems to set state,
        // or we handle playOnStart.

        if (audio.playOnStart && !audio.playing) {
          // Simple one-off trigger logic could be added here
          // But 'playing' is a state.
          // If we play, we should set playing = true?
          // This requires tracking.
          // For this refactor, we will drive it mainly from UI for testing.
        }
      });
}

} // namespace Systems
