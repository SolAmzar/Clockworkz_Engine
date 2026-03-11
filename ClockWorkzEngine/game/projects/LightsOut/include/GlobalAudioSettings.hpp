#pragma once

#include "Audio/AudioManager.hpp"

namespace Runtime {

class GlobalAudioSettings {
public:
  static void Load();
  static void Save();
  static void Apply(Audio::AudioManager &audio);

  static float GetBGMVolume();
  static float GetSFXVolume();
  static void SetBGMVolume(float volume);
  static void SetSFXVolume(float volume);

private:
  static const char *ConfigPath();
  static float Clamp(float value);
  static bool s_loaded;
  static float s_bgmVolume;
  static float s_sfxVolume;
};

} // namespace Runtime
