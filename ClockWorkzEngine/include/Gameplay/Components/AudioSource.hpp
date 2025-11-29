// Gameplay/Components/AudioSource.hpp
#pragma once
#include <string>
#include "Gameplay/GameObject.hpp"
#include "Audio/AudioManager.hpp"

#pragma warning(push, 0)
#include "Core/json.hpp" // nlohmann::json
#pragma warning(pop)


namespace Gameplay {

    class AudioSourceComponent : public Component {
    public:
        std::string soundId;        // name used with AudioManager / SoundManager
        float       volume = 1.0f;
        bool        loop = false;
        bool        playOnStart = false;
        Audio::Group group = Audio::Group::SFX;

        AudioSourceComponent() = default;
        AudioSourceComponent(std::string id, float vol = 1.0f,
            bool loop_ = false, bool playOnStart_ = false,
            Audio::Group g = Audio::Group::SFX)
            : soundId(std::move(id)), volume(vol),
            loop(loop_), playOnStart(playOnStart_), group(g)
        {
        }

        void onUpdate(GameObject& owner, double /*dt*/) override;

        // ---- serialization helpers ----
        nlohmann::json toJson() const;
        static AudioSourceComponent* fromJson(GameObject& owner,
            nlohmann::json const& j);

    private:
        bool m_started = false;   // for playOnStart
    };

} // namespace Gameplay
