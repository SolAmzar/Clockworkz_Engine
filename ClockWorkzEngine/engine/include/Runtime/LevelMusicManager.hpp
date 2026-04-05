/******************************************************************************/
/*!
\file           LevelMusicManager.hpp
\project        Lights Out
\author(s)      [Amzar] 100%
\brief          Declares level music context and layered music management
                helpers for Lights Out.

All content © 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once

#include "Audio/AudioManager.hpp"
#include "Runtime/Scene.hpp"

#include <string>
#include <vector>

namespace Runtime {

struct MusicLayerConfig {
  std::string track;
  float volume{1.0f};
};

struct MusicContextConfig {
  std::string id;
  std::vector<MusicLayerConfig> layers;
  float fadeOutSeconds{0.5f};
};

class LevelMusicManager {
public:
  static void Load();
  static bool Save();

  static std::vector<MusicContextConfig> GetContexts();
  static bool SetContexts(const std::vector<MusicContextConfig> &contexts);

  static void ApplySceneMusic(const Scene &scene, Audio::AudioManager &audio);
  static bool RequestContext(const std::string &contextId, Audio::AudioManager &audio);
  static bool HasContext(const std::string &contextId);
  static void Stop(Audio::AudioManager &audio, float fadeOutSeconds = 0.5f);

  static std::string CurrentContext();
  static std::string CurrentTrack();

private:
  static const char *ConfigPath();
  static std::string MakeLegacyContextId(const std::string &track);
  static bool ResolveTrackForScene(const Scene &scene, MusicContextConfig &outConfig,
                                   std::string &outContextId);
  static bool ResolveTrackForContextId(const std::string &contextId,
                                       MusicContextConfig &outConfig);
  static bool AreLayersActive(const std::vector<MusicLayerConfig> &layers,
                              Audio::AudioManager &audio);
  static bool ApplyContext(const MusicContextConfig &desired,
                           const std::string &desiredContext,
                           Audio::AudioManager &audio);

  static bool s_loaded;
  static std::vector<MusicContextConfig> s_contexts;
  static std::string s_currentContext;
  static std::vector<std::string> s_currentTracks;
  static float s_currentFadeOutSeconds;
  static std::vector<int> s_currentChannels;
};

} // namespace Runtime
