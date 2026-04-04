/******************************************************************************/
/*!
\file           SoundManager.hpp
\project        Lights Out
\author(s)      [Amzar], 100%
\brief          Declaration of Sound Manager for data-driven sound loading and
management. Does not manage playback, only loading and unloading sound assets.

All content ? 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "Assets/ResourceManager.hpp"
#include "Audio/AudioClip.hpp"

namespace Assets {

struct SoundConfig {
  std::string name; // unique id, e.g. "menu_bgm"
  std::string path; // e.g. "assets/audio/menu_bgm.mp3"
  bool loop = false;
  bool stream = false;
  bool is3D = false;
  float volume = 1.0f;
  float pitch = 1.0f;
  std::string group = "SFX";
  float minDistance = 24.0f;
  float maxDistance = 320.0f;
  float falloffStart = 40.0f;
  float falloffAmount = 0.62f;
  float filterInStart = 48.0f;
  float filterPeak = 140.0f;
  float filterReleaseEnd = 260.0f;
};

class SoundManager {
public:
  // Optional: data-driven loading from JSON, like TextureManager
  static bool LoadFromConfig(const std::string &configPath);
  static void Init(Audio::AudioManager &audio);

  // Manually load a single sound
  static std::shared_ptr<Audio::AudioClip>
  LoadSound(const std::string &name, const std::string &path, bool loop = false,
            bool stream = false, bool is3D = false);

  // Retrieve sound by name
  static std::shared_ptr<Audio::AudioClip> Get(const std::string &name);

  // Play sound by name using its config values
  // Returns the channel ID
  static int Play(const std::string &name);

  // Manual override
  static int Play(const std::string &name, float volume, float pitch = 1.0f);

  // Stop a currently playing sound by name
  static void Stop(const std::string &name);

  // Returns true if the sound is currently playing
  static bool IsPlaying(const std::string &name);

  // Unload one sound
  static void Unload(const std::string &name);

  // Clear all sounds
  static void Clear();

  // Helpers similar to TextureManager
  static std::vector<std::string> GetLoadedSoundNames();
  static bool IsLoaded(const std::string &name);
  static bool RemoveFromConfig(const std::string &name,
                               const std::string &configPath =
                                   "assets/configs/sounds.json");
  static bool UpdateVolumesInConfig(
      const std::unordered_map<std::string, float> &volumes,
      const std::string &configPath = "assets/configs/sounds.json");

  /**
   * @brief Import a sound file into the assets folder and register it
   *
   * @param sourcePath Path to the source audio file
   * @param name Unique name for the sound
   * @param config Sound configuration (loop, stream, volume, etc.)
   * @param configPath Path to JSON config file
   * @return true if import successful, false otherwise
   */
  static bool ImportSound(const std::string &sourcePath, const std::string &name,
                          const SoundConfig &config,
                          const std::string &configPath =
                              "assets/configs/sounds.json");

  /**
   * @brief Delete sound from memory, config, AND physical file from disk
   *
   * @param name Sound name to delete
   * @param configPath Path to JSON config file
   * @return true if fully deleted, false otherwise
   *
   * WARNING: This permanently deletes the file from disk!
   */
  static bool DeleteFromDisk(const std::string &name,
                             const std::string &configPath =
                                 "assets/configs/sounds.json");

  /**
   * @brief Get the file path for a sound from config
   */
  static std::string GetFilePath(const std::string &name,
                                 const std::string &configPath =
                                     "assets/configs/sounds.json");
  static std::string GetNameForPath(const std::string &path,
                                    const std::string &configPath =
                                        "assets/configs/sounds.json");
  static bool GetConfig(const std::string &name, SoundConfig &outConfig,
                        const std::string &configPath =
                            "assets/configs/sounds.json");
  static bool UpdateConfig(const std::string &name, const SoundConfig &config,
                           const std::string &configPath =
                               "assets/configs/sounds.json");
  static bool UpdatePathInConfig(const std::string &name,
                                 const std::string &newPath,
                                 const std::string &configPath =
                                     "assets/configs/sounds.json");

private:
  static std::vector<SoundConfig> ParseConfig(const std::string &configPath);
  static Audio::AudioManager *s_audio;

  static std::unordered_map<std::string, SoundConfig> s_configs;
  // Local bookkeeping for names (since ResourceManager's map is private)
  static std::unordered_set<std::string> s_loadedNames;
};

} // namespace Assets




