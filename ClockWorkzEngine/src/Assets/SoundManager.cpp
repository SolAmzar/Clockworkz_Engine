/******************************************************************************/
/*!
\file           SoundManager.cpp
\project        Lights Out
\author(s)      [Amzar], 100%
\brief          Implementation of Sound Manager for data-driven sound loading and management.
                Does not manage playback, only loading and unloading sound assets.

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "Assets/SoundManager.hpp"
#include "Audio/AudioManager.hpp"
#include <core/json.hpp> 
#include <fstream>

namespace Assets {
    Audio::AudioManager* SoundManager::s_audio = nullptr;

    void SoundManager::Init(Audio::AudioManager& audio) {
        s_audio = &audio;
	}

    std::unordered_set<std::string> SoundManager::s_loadedNames{};

    std::shared_ptr<Audio::AudioClip> SoundManager::LoadSound(
        const std::string& name,
        const std::string& path,
        bool loop,
        bool stream,
        bool is3D)
    {
        // Build SoundDesc
        Audio::SoundDesc desc{};
        desc.name = name;
        desc.path = path;
        desc.loop = loop;
        desc.stream = stream;
        desc.is3D = is3D;

        if (!s_audio) {
            // Audio system not initialised – you might want an assert or log here
            std::cout << "SoundManager::Init must be called before LoadSound\n";
            return nullptr;
        }

        auto clip = Assets::ResourceManager<Audio::AudioClip>::Load(
            name,
            *s_audio,   // AudioManager&
            desc        // SoundDesc
        );
        if (clip) {
            s_loadedNames.insert(name);
        }
        return clip;
    }

    std::shared_ptr<Audio::AudioClip> SoundManager::Get(const std::string& name) {
        return ResourceManager<Audio::AudioClip>::Get(name);
    }

    void SoundManager::Unload(const std::string& name) {
        // ResourceManager doesn't have Unload per-resource,
        // so the usual pattern is:
        //   - either add Unload to ResourceManager
        //   - or ignore per-sound unload and just Clear() (simpler)
        //
        // If you want proper Unload(name), you can extend ResourceManager.
        // For now, you might just leave this empty or implement a custom
        // ResourceManager specialization.


        s_loadedNames.erase(name);
    }

    void SoundManager::Clear() {
        ResourceManager<Audio::AudioClip>::Clear();
        s_loadedNames.clear();
    }

    bool SoundManager::IsLoaded(const std::string& name) {
        return s_loadedNames.find(name) != s_loadedNames.end();
    }

    std::vector<std::string> SoundManager::GetLoadedSoundNames() {
        return { s_loadedNames.begin(), s_loadedNames.end() };
    }

    bool SoundManager::LoadFromConfig(const std::string& configPath) {
        auto configs = ParseConfig(configPath);
        bool ok = true;
        for (const auto& cfg : configs) {
            std::cout << "[SoundManager]Loading Soundfile: " << cfg.name
                      << " from path" << cfg.path << std::endl;
            auto clip = LoadSound(cfg.name, cfg.path, cfg.loop, cfg.stream, cfg.is3D);
            if (!clip) {
                ok = false;
                std::cerr << "[SoundManager] Failed to load: " << cfg.name << std::endl;
            } else
            {
                std::cout << "[TextureManager] Successfully loaded: " << cfg.name
                    << " ( Loop: " << cfg.loop << ", Stream: " << cfg.stream << " Is3D: " << cfg.is3D << ")" << std::endl;
            }
        }
        return ok;
    }

    std::vector<SoundConfig> SoundManager::ParseConfig(const std::string& configPath) {
        std::vector<SoundConfig> result;

        std::ifstream file(configPath);
        if (!file) return result;

        nlohmann::json j;
        file >> j;

        if (!j.contains("sounds") || !j["sounds"].is_array()) return result;

        for (auto& entry : j["sounds"]) {
            SoundConfig cfg;
            cfg.name = entry.value("name", "");
            cfg.path = entry.value("path", "");
            cfg.loop = entry.value("loop", false);
            cfg.stream = entry.value("stream", false);
            cfg.is3D = entry.value("is3D", false);
            if (!cfg.name.empty() && !cfg.path.empty())
                result.push_back(cfg);
        }

        return result;
    }

} // namespace Assets
