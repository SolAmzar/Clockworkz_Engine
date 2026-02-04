/******************************************************************************/
/*!
\file           SoundManager.cpp
\project        Lights Out
\author(s)      [Amzar], 100%
\brief          Implementation of Sound Manager for data-driven sound loading
and management. Does not manage playback, only loading and unloading sound
assets.

All content � 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Assets/SoundManager.hpp"
#include "Audio/AudioManager.hpp"
#pragma warning(push, 0)
#include "Core/json.hpp" // nlohmann::json
#pragma warning(pop)
#include <fstream>
#include <iomanip>

namespace Assets {
Audio::AudioManager *SoundManager::s_audio = nullptr;
std::unordered_set<std::string> SoundManager::s_loadedNames{};
std::unordered_map<std::string, SoundConfig> SoundManager::s_configs{};

void SoundManager::Init(Audio::AudioManager &audio) { s_audio = &audio; }

std::shared_ptr<Audio::AudioClip>
SoundManager::LoadSound(const std::string &name, const std::string &path,
                        bool loop, bool stream, bool is3D) {
  // Build SoundDesc
  Audio::SoundDesc desc{};
  desc.name = name;
  desc.path = path;
  desc.loop = loop;
  desc.stream = stream;
  desc.is3D = is3D;

  if (!s_audio || !s_audio->isReady()) {
    std::cout << "[SoundManager] AudioManager not ready while loading '" << name
              << "' (" << path << ")\n";
    return nullptr;
  }

  auto clip =
      Assets::ResourceManager<Audio::AudioClip>::Load(name,
                                                      *s_audio, // AudioManager&
                                                      desc      // SoundDesc
      );
  if (!clip) {
    std::cout << "[SoundManager] Failed to load sound '" << name << "' from '"
              << path << "'\n";
    return nullptr;
  }
  s_loadedNames.insert(name);
  return clip;
}

std::shared_ptr<Audio::AudioClip> SoundManager::Get(const std::string &name) {
  return ResourceManager<Audio::AudioClip>::Get(name);
}

int SoundManager::Play(const std::string &name) {
  if (!s_audio) {
    std::cerr << "[SoundManager] Cannot play '" << name
              << "': Audio system not initialized.\n";
    return -1;
  }

  // Delegate to AudioManager which now handles profiles
  return s_audio->play(name);
}

int SoundManager::Play(const std::string &name, float volume, float pitch) {
  if (!s_audio) {
    std::cerr << "[SoundManager] Cannot play '" << name
              << "': Audio system not initialized.\n";
    return -1;
  }
  return s_audio->play(name, volume, pitch);
}

void SoundManager::Unload(const std::string &name) {
  if (s_audio) {
    s_audio->stop(name);
    s_audio->unload(name);
    s_audio->unregisterSoundProfile(name);
  }
  ResourceManager<Audio::AudioClip>::Unload(name);
  s_loadedNames.erase(name);
  // s_configs is now local to LoadFromConfig or we can keep it if needed,
  // but AudioManager has the profiles now.
  // If we want to be clean, we might need an UnloadProfile in AudioManager,
  // but for now just erasing here is fine.
  s_configs.erase(name);
}

void SoundManager::Clear() {
  ResourceManager<Audio::AudioClip>::Clear();
  s_loadedNames.clear();
  s_configs.clear();
  s_audio = nullptr;
}

bool SoundManager::IsLoaded(const std::string &name) {
  return s_loadedNames.find(name) != s_loadedNames.end();
}

std::vector<std::string> SoundManager::GetLoadedSoundNames() {
  return {s_loadedNames.begin(), s_loadedNames.end()};
}

bool SoundManager::RemoveFromConfig(const std::string &name,
                                    const std::string &configPath) {
  std::ifstream file(configPath);
  if (!file)
    return false;

  nlohmann::json j;
  file >> j;

  if (!j.contains("sounds") || !j["sounds"].is_array())
    return false;

  auto &arr = j["sounds"];
  bool removed = false;
  for (auto it = arr.begin(); it != arr.end();) {
    const std::string entryName = it->value("name", "");
    if (entryName == name) {
      it = arr.erase(it);
      removed = true;
    } else {
      ++it;
    }
  }

  if (!removed)
    return false;

  std::ofstream out(configPath);
  if (!out)
    return false;

  out << std::setw(4) << j << "\n";
  return true;
}

bool SoundManager::UpdateVolumesInConfig(
    const std::unordered_map<std::string, float> &volumes,
    const std::string &configPath) {
  std::ifstream file(configPath);
  if (!file)
    return false;

  nlohmann::json j;
  file >> j;

  if (!j.contains("sounds") || !j["sounds"].is_array())
    return false;

  bool changed = false;
  for (auto &entry : j["sounds"]) {
    const std::string entryName = entry.value("name", "");
    auto it = volumes.find(entryName);
    if (it != volumes.end()) {
      entry["volume"] = it->second;
      changed = true;
    }
  }

  if (!changed)
    return false;

  std::ofstream out(configPath);
  if (!out)
    return false;

  out << std::setw(4) << j << "\n";
  return true;
}

bool SoundManager::LoadFromConfig(const std::string &configPath) {
  auto configs = ParseConfig(configPath);
  bool ok = true;
  for (const auto &cfg : configs) {
    std::cout << "[SoundManager]Loading Soundfile: " << cfg.name << " from path"
              << cfg.path << std::endl;
    auto clip = LoadSound(cfg.name, cfg.path, cfg.loop, cfg.stream, cfg.is3D);
    if (!clip) {
      ok = false;
      std::cerr << "[SoundManager] Failed to load: " << cfg.name << std::endl;
    } else {
      // Register profile with AudioManager
      if (s_audio) {
        Audio::AudioManager::SoundProfile profile;
        profile.volume = cfg.volume;
        profile.pitch = cfg.pitch;
        profile.group =
            (cfg.group == "BGM") ? Audio::Group::BGM : Audio::Group::SFX;
        s_audio->registerSoundProfile(cfg.name, profile);
      }

      s_configs[cfg.name] = cfg; // Keep local copy if we want, or remove
      std::cout << "[SoundManager] Successfully loaded: " << cfg.name
                << " ( Loop: " << cfg.loop << ", Stream: " << cfg.stream
                << " Is3D: " << cfg.is3D << " Vol: " << cfg.volume
                << " Group: " << cfg.group << ")" << std::endl;
    }
  }
  return ok;
}

std::vector<SoundConfig>
SoundManager::ParseConfig(const std::string &configPath) {
  std::vector<SoundConfig> result;

  std::ifstream file(configPath);
  if (!file)
    return result;

  nlohmann::json j;
  file >> j;

  if (!j.contains("sounds") || !j["sounds"].is_array())
    return result;

  for (auto &entry : j["sounds"]) {
    SoundConfig cfg;
    cfg.name = entry.value("name", "");
    cfg.path = entry.value("path", "");
    cfg.loop = entry.value("loop", false);
    cfg.stream = entry.value("stream", false);
    cfg.is3D = entry.value("is3D", false);
    cfg.volume = entry.value("volume", 1.0f);
    cfg.pitch = entry.value("pitch", 1.0f);
    cfg.group = entry.value("group", "SFX");

    if (!cfg.name.empty() && !cfg.path.empty())
      result.push_back(cfg);
  }

  return result;
}

} // namespace Assets
