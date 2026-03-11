#include "GlobalAudioSettings.hpp"

#include "Core/json.hpp"
#include <fstream>

namespace Runtime {

bool GlobalAudioSettings::s_loaded = false;
float GlobalAudioSettings::s_bgmVolume = 1.0f;
float GlobalAudioSettings::s_sfxVolume = 1.0f;

const char *GlobalAudioSettings::ConfigPath() {
  return "assets/configs/audio_settings.json";
}

float GlobalAudioSettings::Clamp(float value) {
  if (value < 0.0f) return 0.0f;
  if (value > 1.0f) return 1.0f;
  return value;
}

void GlobalAudioSettings::Load() {
  if (s_loaded) {
    return;
  }

  std::ifstream file(ConfigPath());
  if (!file.is_open()) {
    s_loaded = true;
    Save();
    return;
  }

  nlohmann::json root;
  try {
    file >> root;
    s_bgmVolume = Clamp(root.value("bgmVolume", 1.0f));
    s_sfxVolume = Clamp(root.value("sfxVolume", 1.0f));
  } catch (...) {
    s_bgmVolume = 1.0f;
    s_sfxVolume = 1.0f;
  }

  s_loaded = true;
}

void GlobalAudioSettings::Save() {
  nlohmann::json root;
  root["bgmVolume"] = s_bgmVolume;
  root["sfxVolume"] = s_sfxVolume;

  std::ofstream file(ConfigPath());
  if (file.is_open()) {
    file << root.dump(2) << '\n';
  }
}

void GlobalAudioSettings::Apply(Audio::AudioManager &audio) {
  Load();
  audio.setGroupVolume(Audio::Group::BGM, s_bgmVolume);
  audio.setGroupVolume(Audio::Group::SFX, s_sfxVolume);
}

float GlobalAudioSettings::GetBGMVolume() {
  Load();
  return s_bgmVolume;
}

float GlobalAudioSettings::GetSFXVolume() {
  Load();
  return s_sfxVolume;
}

void GlobalAudioSettings::SetBGMVolume(float volume) {
  s_bgmVolume = Clamp(volume);
  s_loaded = true;
}

void GlobalAudioSettings::SetSFXVolume(float volume) {
  s_sfxVolume = Clamp(volume);
  s_loaded = true;
}

} // namespace Runtime
