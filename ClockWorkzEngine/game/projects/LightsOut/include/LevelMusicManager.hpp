#pragma once

#include "Audio/AudioManager.hpp"
#include "Runtime/Scene.hpp"

#include <string>
#include <vector>

namespace Runtime {

struct MusicContextConfig {
  std::string id;
  std::string track;
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

  static bool s_loaded;
  static std::vector<MusicContextConfig> s_contexts;
  static std::string s_currentContext;
  static std::string s_currentTrack;
  static float s_currentFadeOutSeconds;
  static int s_currentChannel;
};

} // namespace Runtime
