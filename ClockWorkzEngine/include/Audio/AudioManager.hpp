/******************************************************************************/
/*!
\file           AudioManager.hpp
\project        Lights Out
\author(s)      [Amzar]
\brief          Handles Audio using FMOD API.
                Responsibilities include:
                - Initializing and shutting down the FMOD system
                - Loading and unloading sound resources
                - Playing sounds with options for volume, looping, and grouping (BGM, SFX)
                - Managing audio channels and groups for volume control and pausing
                - Providing 3D audio listener settings if needed

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/



#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include "Math/Vec3.hpp"


// Forward declare FMOD to avoid heavy headers leaking
namespace FMOD { class System; class Sound; class Channel; class ChannelGroup; }

namespace Audio {

    enum class Group { Master, BGM, SFX };

    struct SoundDesc {
		std::string name; // unique id
        std::string path = "";
        bool loop = false;
        bool stream = false; // stream large bgm from disk
        bool is3D = false;
    };

    class AudioManager {
    public:
        AudioManager() = default;
        ~AudioManager();

        bool init(int maxChannels = 64);
        void shutdown();
        void update();

        // Resources
        bool load(const SoundDesc& desc);
        void unload(const std::string& id);
        void unloadAll();

        // Playback
        int  play(const std::string& id, Group group = Group::SFX, float volume = 1.0f, bool paused = false);
        void playOneShot(const std::string& id, Group group = Group::SFX, float volume = 1.0f);

        // Stop control
        void stop(std::string const& name);      // stop a specific playing sound by key
        void stopGroup(Group g);                  // stop everything in a group
        void stopAll();                           // panic-stop everything


        // Channel control
        void stop(int channel);
        void setPaused(int channel, bool paused);
        void setVolume(int channel, float volume);

        // Groups
        void setGroupVolume(Group g, float volume);
        void setGroupPaused(Group g, bool paused);

        // Listener (for 3D if you want it)
        void setListener(const vec3& pos, const vec3& vel, const vec3& forward, const vec3& up);

        bool isReady() const noexcept { return m_system != nullptr; }

        FMOD::System* getSystem() const { return m_system; }
        static AudioManager& get();

    private:
        struct ChannelHandle { 
            FMOD::Channel* ch = nullptr;  
            std::string soundId{};
            Group group = Group::SFX;
        };

        FMOD::System* m_system = nullptr;
        FMOD::ChannelGroup* m_master = nullptr;
        FMOD::ChannelGroup* m_bgm = nullptr;
        FMOD::ChannelGroup* m_sfx = nullptr;

        std::unordered_map<std::string, FMOD::Sound*> m_sounds;
        std::unordered_map<int, ChannelHandle> m_channels;
        int m_nextChannelId = 1;

        FMOD::ChannelGroup* groupHandle(Group g) const;
        bool createSound(SoundDesc const& desc, FMOD::Sound** out);
    };

} // namespace Audio
