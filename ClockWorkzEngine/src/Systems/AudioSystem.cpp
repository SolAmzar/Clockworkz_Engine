/******************************************************************************/
/*!
\file           AudioSystem.cpp
\project        ClockWorkz Engine
\author(s)      [Lebon], 100%
\brief          Implementation of AudioSystem.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Systems/AudioSystem.hpp"
#include "Audio/AudioManager.hpp"
#include "Components/AudioSource.hpp"
#include "Components/Transform.hpp"
#include "Components/Velocity.hpp"
#include "Core/Engine.hpp"
#include <cmath>

namespace Systems {

void AudioSystem::update(ECS::World &world) {
  auto &audioManager = Core::Engine::Get().audio();

  world.each<Components::AudioSource>(
      [&](ECS::Entity entity, Components::AudioSource &audio) {
        if (audio.soundId.empty()) {
          if (audio.channelId != 0) {
            audioManager.stop(audio.channelId);
            audio.channelId = 0;
          }
          audio.playing = false;
          return;
        }

        const bool wantsPlayback = audio.playing || audio.playOnStart;

        if (!wantsPlayback) {
          if (audio.channelId != 0) {
            audioManager.stop(audio.channelId);
            audio.channelId = 0;
          }
          return;
        }

        if (audio.channelId == 0) {
          audio.channelId =
              audioManager.play(audio.soundId, Audio::Group::SFX, audio.volume, false);
          if (audio.channelId == 0) {
            audio.playing = false;
            return;
          }
          audio.playing = true;
          audio.playOnStart = false;
        }

        if (!audioManager.isChannelPlaying(audio.channelId)) {
          audio.channelId = 0;
          audio.playing = false;
          return;
        }

        audioManager.setVolume(audio.channelId, audio.volume);
        audioManager.setPitch(audio.channelId, audio.pitch);

        if (!audio.spatial) {
          return;
        }

        vec3 emitterPos(0.0f, 0.0f, audio.height);
        vec3 emitterVel(0.0f, 0.0f, 0.0f);
        vec3 forward(1.0f, 0.0f, 0.0f);

        if (auto *transform = world.getComponent<Components::Transform>(entity)) {
          emitterPos.x = transform->position.x;
          emitterPos.y = transform->position.y;
          forward = vec3(std::cos(transform->rotation), std::sin(transform->rotation), 0.0f);
        }

        if (auto *velocity = world.getComponent<Components::Velocity>(entity)) {
          emitterVel.x = velocity->velocity.x;
          emitterVel.y = velocity->velocity.y;
        }

        audioManager.setChannel3DPosition(audio.channelId, emitterPos, emitterVel);
        audioManager.setChannel3DDistance(audio.channelId, audio.minDistance,
                                          audio.maxDistance);

        if (audio.directional) {
          audioManager.setChannel3DCone(audio.channelId, forward,
                                        audio.innerConeAngleDeg,
                                        audio.outerConeAngleDeg,
                                        audio.outerConeVolume);
        }
      });
}

} // namespace Systems
