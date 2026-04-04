/******************************************************************************/
/*!
\file           SoundManager.cpp
\project        Lights Out
\author(s)      [Amzar] 100%
\brief          Implementation of Sound Manager for data-driven sound loading
and management. Does not manage playback, only loading and unloading sound
assets.

All content ? 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Assets/SoundManager.hpp"
#include "Audio/AudioManager.hpp"
#pragma warning(push, 0)
#include "Core/json.hpp" // nlohmann::json
#pragma warning(pop)
#include <filesystem>
#include <fstream>
#include <iomanip>

namespace fs = std::filesystem;

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

void SoundManager::Stop(const std::string &name) {
  if (s_audio)
    s_audio->stop(name);
}

bool SoundManager::IsPlaying(const std::string &name) {
  return s_audio && s_audio->isPlaying(name);
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
  s_loadedNames = {};
  s_configs = {};
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
        profile.minDistance = cfg.minDistance;
        profile.maxDistance = cfg.maxDistance;
        profile.falloffStart = cfg.falloffStart;
        profile.falloffAmount = cfg.falloffAmount;
        profile.filterInStart = cfg.filterInStart;
        profile.filterPeak = cfg.filterPeak;
        profile.filterReleaseEnd = cfg.filterReleaseEnd;
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
    cfg.minDistance = entry.value("minDistance", 80.0f);
    cfg.maxDistance = entry.value("maxDistance", 520.0f);
    cfg.falloffStart = entry.value("falloffStart", 120.0f);
    cfg.falloffAmount = entry.value("falloffAmount", 0.25f);
    cfg.filterInStart = entry.value("filterInStart", 180.0f);
    cfg.filterPeak = entry.value("filterPeak", 320.0f);
    cfg.filterReleaseEnd = entry.value("filterReleaseEnd", 480.0f);

    if (!cfg.name.empty() && !cfg.path.empty())
      result.push_back(cfg);
  }

  return result;
}

std::string SoundManager::GetFilePath(const std::string &name,
                                      const std::string &configPath) {
  std::ifstream file(configPath);
  if (!file)
    return "";

  nlohmann::json j;
  try {
    file >> j;
  } catch (...) {
    return "";
  }

  if (!j.contains("sounds") || !j["sounds"].is_array())
    return "";

  for (const auto &entry : j["sounds"]) {
    if (entry.value("name", "") == name) {
      return entry.value("path", "");
    }
  }

  return "";
}

std::string SoundManager::GetNameForPath(const std::string &path,
                                         const std::string &configPath) {
  std::ifstream file(configPath);
  if (!file)
    return "";

  nlohmann::json j;
  try {
    file >> j;
  } catch (...) {
    return "";
  }

  if (!j.contains("sounds") || !j["sounds"].is_array())
    return "";

  const fs::path target = fs::path(path).lexically_normal();
  for (const auto &entry : j["sounds"]) {
    const fs::path entryPath =
        fs::path(entry.value("path", "")).lexically_normal();
    if (entryPath == target)
      return entry.value("name", "");
  }

  return "";
}
bool SoundManager::GetConfig(const std::string &name, SoundConfig &outConfig,
                             const std::string &configPath) {
  std::ifstream file(configPath);
  if (!file)
    return false;

  nlohmann::json j;
  try {
    file >> j;
  } catch (...) {
    return false;
  }

  if (!j.contains("sounds") || !j["sounds"].is_array())
    return false;

  for (const auto &entry : j["sounds"]) {
    if (entry.value("name", "") != name)
      continue;

    outConfig.name = entry.value("name", "");
    outConfig.path = entry.value("path", "");
    outConfig.loop = entry.value("loop", false);
    outConfig.stream = entry.value("stream", false);
    outConfig.is3D = entry.value("is3D", false);
    outConfig.volume = entry.value("volume", 1.0f);
    outConfig.pitch = entry.value("pitch", 1.0f);
    outConfig.group = entry.value("group", "SFX");
    outConfig.minDistance = entry.value("minDistance", 80.0f);
    outConfig.maxDistance = entry.value("maxDistance", 520.0f);
    outConfig.falloffStart = entry.value("falloffStart", 120.0f);
    outConfig.falloffAmount = entry.value("falloffAmount", 0.25f);
    outConfig.filterInStart = entry.value("filterInStart", 180.0f);
    outConfig.filterPeak = entry.value("filterPeak", 320.0f);
    outConfig.filterReleaseEnd = entry.value("filterReleaseEnd", 480.0f);
    return true;
  }

  return false;
}

bool SoundManager::UpdateConfig(const std::string &name,
                                const SoundConfig &config,
                                const std::string &configPath) {
  std::ifstream file(configPath);
  if (!file)
    return false;

  nlohmann::json j;
  try {
    file >> j;
  } catch (...) {
    return false;
  }

  if (!j.contains("sounds") || !j["sounds"].is_array())
    return false;

  bool changed = false;
  for (auto &entry : j["sounds"]) {
    if (entry.value("name", "") != name)
      continue;

    entry["name"] = config.name;
    entry["path"] = config.path;
    entry["loop"] = config.loop;
    entry["stream"] = config.stream;
    entry["is3D"] = config.is3D;
    entry["volume"] = config.volume;
    entry["pitch"] = config.pitch;
    entry["group"] = config.group;
    entry["minDistance"] = config.minDistance;
    entry["maxDistance"] = config.maxDistance;
    entry["falloffStart"] = config.falloffStart;
    entry["falloffAmount"] = config.falloffAmount;
    entry["filterInStart"] = config.filterInStart;
    entry["filterPeak"] = config.filterPeak;
    entry["filterReleaseEnd"] = config.filterReleaseEnd;
    changed = true;
    break;
  }

  if (!changed)
    return false;

  std::ofstream out(configPath);
  if (!out)
    return false;
  out << std::setw(4) << j << "\n";
  out.close();

  s_configs[name] = config;

  if (s_audio) {
    Audio::AudioManager::SoundProfile profile;
    profile.volume = config.volume;
    profile.pitch = config.pitch;
    profile.group = (config.group == "BGM") ? Audio::Group::BGM : Audio::Group::SFX;
    profile.minDistance = config.minDistance;
    profile.maxDistance = config.maxDistance;
    profile.falloffStart = config.falloffStart;
    profile.falloffAmount = config.falloffAmount;
    profile.filterInStart = config.filterInStart;
    profile.filterPeak = config.filterPeak;
    profile.filterReleaseEnd = config.filterReleaseEnd;
    s_audio->registerSoundProfile(name, profile);
  }

  if (IsLoaded(name)) {
    Unload(name);
    auto clip = LoadSound(name, config.path, config.loop, config.stream, config.is3D);
    if (!clip)
      return false;
    if (s_audio) {
      Audio::AudioManager::SoundProfile profile;
      profile.volume = config.volume;
      profile.pitch = config.pitch;
      profile.group = (config.group == "BGM") ? Audio::Group::BGM : Audio::Group::SFX;
      profile.minDistance = config.minDistance;
      profile.maxDistance = config.maxDistance;
      profile.falloffStart = config.falloffStart;
      profile.falloffAmount = config.falloffAmount;
      profile.filterInStart = config.filterInStart;
      profile.filterPeak = config.filterPeak;
      profile.filterReleaseEnd = config.filterReleaseEnd;
      s_audio->registerSoundProfile(name, profile);
    }
    s_configs[name] = config;
  }

  return true;
}

bool SoundManager::UpdatePathInConfig(const std::string &name,
                                      const std::string &newPath,
                                      const std::string &configPath) {
  std::ifstream file(configPath);
  if (!file)
    return false;

  nlohmann::json j;
  try {
    file >> j;
  } catch (...) {
    return false;
  }

  if (!j.contains("sounds") || !j["sounds"].is_array())
    return false;

  bool changed = false;
  for (auto &entry : j["sounds"]) {
    if (entry.value("name", "") == name) {
      entry["path"] = newPath;
      changed = true;
      break;
    }
  }

  if (!changed)
    return false;

  std::ofstream out(configPath);
  if (!out)
    return false;

  out << std::setw(4) << j << "\n";

  auto it = s_configs.find(name);
  if (it != s_configs.end())
    it->second.path = newPath;

  return true;
}

bool SoundManager::ImportSound(const std::string &sourcePath,
                               const std::string &name,
                               const SoundConfig &config,
                               const std::string &configPath) {
  // 1. Validate source file exists
  if (!fs::exists(sourcePath)) {
    std::cerr << "[SoundManager] Source file not found: " << sourcePath
              << std::endl;
    return false;
  }

  // 2. Determine destination path
  fs::path source(sourcePath);
  std::string filename = source.filename().string();
  std::string destPath = "assets/audio/" + filename;

  // 3. Create destination directory if needed
  fs::create_directories("assets/audio");

  // 4. Copy file to assets folder
  try {
    fs::copy_file(sourcePath, destPath, fs::copy_options::overwrite_existing);
    std::cout << "[SoundManager] Copied file to: " << destPath << std::endl;
  } catch (const fs::filesystem_error &e) {
    std::cerr << "[SoundManager] Failed to copy file: " << e.what()
              << std::endl;
    return false;
  }

  // 5. Add entry to JSON config
  std::ifstream inFile(configPath);
  nlohmann::json j;
  if (inFile.is_open()) {
    try {
      inFile >> j;
    } catch (...) {
      j = nlohmann::json::object();
    }
    inFile.close();
  }

  if (!j.contains("sounds") || !j["sounds"].is_array()) {
    j["sounds"] = nlohmann::json::array();
  }

  // Check if name already exists
  for (const auto &entry : j["sounds"]) {
    if (entry.value("name", "") == name) {
      std::cerr << "[SoundManager] Sound name already exists: " << name
                << std::endl;
      return false;
    }
  }

  // Add new entry
  nlohmann::json newEntry;
  newEntry["name"] = name;
  newEntry["path"] = destPath;
  newEntry["loop"] = config.loop;
  newEntry["stream"] = config.stream;
  newEntry["is3D"] = config.is3D;
  newEntry["volume"] = config.volume;
  newEntry["pitch"] = config.pitch;
  newEntry["group"] = config.group;
  j["sounds"].push_back(newEntry);

  // Write back to config
  std::ofstream outFile(configPath);
  if (!outFile.is_open()) {
    std::cerr << "[SoundManager] Failed to write config: " << configPath
              << std::endl;
    return false;
  }
  outFile << std::setw(4) << j << std::endl;
  outFile.close();

  // 6. Load the sound into memory
  auto clip = LoadSound(name, destPath, config.loop, config.stream, config.is3D);
  if (!clip) {
    std::cerr << "[SoundManager] Failed to load imported sound" << std::endl;
    return false;
  }

  // Register profile
  if (s_audio) {
    Audio::AudioManager::SoundProfile profile;
    profile.volume = config.volume;
    profile.pitch = config.pitch;
    profile.group = (config.group == "BGM") ? Audio::Group::BGM : Audio::Group::SFX;
    profile.minDistance = config.minDistance;
    profile.maxDistance = config.maxDistance;
    profile.falloffStart = config.falloffStart;
    profile.falloffAmount = config.falloffAmount;
    profile.filterInStart = config.filterInStart;
    profile.filterPeak = config.filterPeak;
    profile.filterReleaseEnd = config.filterReleaseEnd;
    s_audio->registerSoundProfile(name, profile);
  }

  s_configs[name] = config;
  s_configs[name].path = destPath;

  std::cout << "[SoundManager] Successfully imported: " << name << std::endl;
  return true;
}

bool SoundManager::DeleteFromDisk(const std::string &name,
                                  const std::string &configPath) {
  // 1. Get the file path before removing from config
  std::string filePath = GetFilePath(name, configPath);
  if (filePath.empty()) {
    std::cerr << "[SoundManager] Sound not found in config: " << name
              << std::endl;
    return false;
  }

  // 2. Unload from memory
  Unload(name);

  // 3. Remove from JSON config
  if (!RemoveFromConfig(name, configPath)) {
    std::cerr << "[SoundManager] Failed to remove from config: " << name
              << std::endl;
    return false;
  }

  // 4. Delete physical file from disk
  try {
    if (fs::exists(filePath)) {
      fs::remove(filePath);
      std::cout << "[SoundManager] Deleted file from disk: " << filePath
                << std::endl;
    } else {
      std::cerr << "[SoundManager] File not found on disk: " << filePath
                << std::endl;
    }
  } catch (const fs::filesystem_error &e) {
    std::cerr << "[SoundManager] Failed to delete file: " << e.what()
              << std::endl;
    return false;
  }

  std::cout << "[SoundManager] Successfully deleted: " << name << std::endl;
  return true;
}

} // namespace Assets



