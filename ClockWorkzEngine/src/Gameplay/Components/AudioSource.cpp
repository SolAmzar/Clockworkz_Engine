/******************************************************************************/
/*!
\file			AudioSource.cpp
\project        Lights Out
\author(s)      [Amzar], 100%
\brief          Defines the AudioSourceComponent for attaching audio playback
                capabilities to GameObjects.
                Responsibilities include:
                - Storing sound ID, volume, loop, and play-on-start settings
                - Playing audio via AudioManager during updates
				- Serialization to/from JSON for level saving/loading
All content ? 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "Gameplay/Components/AudioSource.hpp"
#include "Core/Engine.hpp" // for Engine::Get().audio()

namespace Gameplay {

    void AudioSourceComponent::onUpdate(GameObject& owner, double /*dt*/) {
        (void)owner;
        if (playOnStart && !m_started) {
            // AudioManager uses soundId, no file paths here
            Core::Engine::Get().audio().play(soundId, group, volume, false);
            m_started = true;
        }
    }

    nlohmann::json AudioSourceComponent::toJson() const {
        nlohmann::json j;
        j["soundId"] = soundId;
        j["volume"] = volume;
        j["loop"] = loop;
        j["playOnStart"] = playOnStart;
        j["group"] = static_cast<int>(group); // store as int
        return j;
    }

    AudioSourceComponent* AudioSourceComponent::fromJson(GameObject& owner,
        nlohmann::json const& j) {
        auto* comp = owner.addComponent<AudioSourceComponent>();

        comp->soundId = j.value("soundId", "");
        comp->volume = j.value("volume", 1.0f);
        comp->loop = j.value("loop", false);
        comp->playOnStart = j.value("playOnStart", false);
        comp->group = static_cast<Audio::Group>(j.value("group",
            static_cast<int>(Audio::Group::SFX)));
        return comp;
    }

} // namespace Gameplay
