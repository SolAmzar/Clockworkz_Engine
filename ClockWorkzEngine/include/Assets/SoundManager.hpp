/******************************************************************************/
/*!
\file           SoundManager.hpp
\project        Lights Out
\author(s)      [Amzar], 100%
\brief          Declaration of Sound Manager for data-driven sound loading and management.
                Does not manage playback, only loading and unloading sound assets.

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
#include <string>
#include <memory>
#include <vector>
#include <unordered_set>

#include "Audio/AudioClip.hpp"
#include "Assets/ResourceManager.hpp"

namespace Assets {

    struct SoundConfig {
        std::string name;   // unique id, e.g. "menu_bgm"
        std::string path;   // e.g. "assets/audio/menu_bgm.mp3"
        bool loop = false;
        bool stream = false;
        bool is3D = false;
    };

    class SoundManager {
    public:
        // Optional: data-driven loading from JSON, like TextureManager
        static bool LoadFromConfig(const std::string& configPath);
        static void Init(Audio::AudioManager& audio);

        // Manually load a single sound
        static std::shared_ptr<Audio::AudioClip> LoadSound(
            const std::string& name,
            const std::string& path,
            bool loop = false,
            bool stream = false,
            bool is3D = false);

        // Retrieve sound by name
        static std::shared_ptr<Audio::AudioClip> Get(const std::string& name);

        // Unload one sound
        static void Unload(const std::string& name);

        // Clear all sounds
        static void Clear();

        // Helpers similar to TextureManager
        static std::vector<std::string> GetLoadedSoundNames();
        static bool IsLoaded(const std::string& name);

    private:
        static std::vector<SoundConfig> ParseConfig(const std::string& configPath);
        static Audio::AudioManager* s_audio;

        // Local bookkeeping for names (since ResourceManager's map is private)
        static std::unordered_set<std::string> s_loadedNames;
    };

} // namespace Assets
