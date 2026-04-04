/******************************************************************************/
/*!
\file           GlobalAudioSettings.hpp
\project        Lights Out
\author(s)      [Amzar] 100%
\brief          Declares helpers for applying and persisting global audio
                settings for the game.

All content © 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once

#include "Audio/AudioManager.hpp"

namespace Runtime {

class GlobalAudioSettings {
public:
  static void Load();
  static void Save();
  static void Apply(Audio::AudioManager &audio);

  static float GetMasterVolume();
  static float GetBGMVolume();
  static float GetSFXVolume();
  static void SetMasterVolume(float volume);
  static void SetBGMVolume(float volume);
  static void SetSFXVolume(float volume);

private:
  static const char *ConfigPath();
  static float Clamp(float value);
  static bool s_loaded;
  static float s_masterVolume;
  static float s_bgmVolume;
  static float s_sfxVolume;
};

} // namespace Runtime
