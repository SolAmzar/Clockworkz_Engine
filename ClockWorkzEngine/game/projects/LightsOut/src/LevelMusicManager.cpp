#include "LevelMusicManager.hpp"

#include "Assets/SoundManager.hpp"

#pragma warning(push, 0)
#include "Core/json.hpp"
#pragma warning(pop)

#include <algorithm>
#include <fstream>
#include <iomanip>

namespace Runtime {

using json = nlohmann::json;

bool LevelMusicManager::s_loaded = false;
std::vector<MusicContextConfig> LevelMusicManager::s_contexts{};
std::string LevelMusicManager::s_currentContext{};
std::string LevelMusicManager::s_currentTrack{};
float LevelMusicManager::s_currentFadeOutSeconds = 0.5f;
int LevelMusicManager::s_currentChannel = 0;

const char *LevelMusicManager::ConfigPath() {
  return "assets/configs/music_contexts.json";
}

std::string LevelMusicManager::MakeLegacyContextId(const std::string &track) {
  return "__legacy__:" + track;
}

void LevelMusicManager::Load() {
  if (s_loaded) {
    return;
  }

  s_contexts.clear();

  std::ifstream file(ConfigPath());
  if (!file) {
    s_loaded = true;
    return;
  }

  json root;
  try {
    file >> root;
  } catch (...) {
    s_loaded = true;
    return;
  }

  if (root.contains("contexts") && root["contexts"].is_array()) {
    for (const auto &entry : root["contexts"]) {
      MusicContextConfig cfg;
      cfg.id = entry.value("id", "");
      cfg.track = entry.value("track", "");
      cfg.fadeOutSeconds = std::max(0.0f, entry.value("fadeOutSeconds", 0.5f));
      if (!cfg.id.empty()) {
        s_contexts.push_back(cfg);
      }
    }
  }

  s_loaded = true;
}

bool LevelMusicManager::Save() {
  json root;
  root["contexts"] = json::array();
  for (const auto &ctx : s_contexts) {
    if (ctx.id.empty()) {
      continue;
    }
    root["contexts"].push_back({
        {"id", ctx.id},
        {"track", ctx.track},
        {"fadeOutSeconds", std::max(0.0f, ctx.fadeOutSeconds)},
    });
  }

  std::ofstream out(ConfigPath());
  if (!out) {
    return false;
  }
  out << std::setw(4) << root << "\n";
  return true;
}

std::vector<MusicContextConfig> LevelMusicManager::GetContexts() {
  Load();
  return s_contexts;
}

bool LevelMusicManager::SetContexts(const std::vector<MusicContextConfig> &contexts) {
  Load();
  s_contexts.clear();
  s_contexts.reserve(contexts.size());
  for (auto ctx : contexts) {
    if (ctx.id.empty()) {
      continue;
    }
    ctx.fadeOutSeconds = std::max(0.0f, ctx.fadeOutSeconds);
    s_contexts.push_back(std::move(ctx));
  }

  std::sort(s_contexts.begin(), s_contexts.end(),
            [](const MusicContextConfig &a, const MusicContextConfig &b) {
              return a.id < b.id;
            });
  return Save();
}

bool LevelMusicManager::ResolveTrackForScene(const Scene &scene,
                                             MusicContextConfig &outConfig,
                                             std::string &outContextId) {
  Load();

  const std::string sceneContext = scene.getMusicContext();
  if (!sceneContext.empty()) {
    if (ResolveTrackForContextId(sceneContext, outConfig)) {
      outContextId = outConfig.id;
      return !outConfig.track.empty();
    }
  }

  if (!scene.getBGM().empty()) {
    outConfig.id = MakeLegacyContextId(scene.getBGM());
    outConfig.track = scene.getBGM();
    outConfig.fadeOutSeconds = 0.5f;
    outContextId = outConfig.id;
    return true;
  }

  outConfig = {};
  outContextId.clear();
  return false;
}

bool LevelMusicManager::ResolveTrackForContextId(const std::string &contextId,
                                                 MusicContextConfig &outConfig) {
  Load();

  for (const auto &ctx : s_contexts) {
    if (ctx.id == contextId) {
      outConfig = ctx;
      return true;
    }
  }

  outConfig = {};
  return false;
}

void LevelMusicManager::ApplySceneMusic(const Scene &scene, Audio::AudioManager &audio) {
  MusicContextConfig desired{};
  std::string desiredContext;
  if (!ResolveTrackForScene(scene, desired, desiredContext)) {
    Stop(audio);
    return;
  }

  if (!desired.track.empty() && desired.track == s_currentTrack &&
      audio.isPlaying(desired.track)) {
    s_currentContext = desiredContext;
    s_currentFadeOutSeconds = desired.fadeOutSeconds;
    return;
  }

  if (s_currentChannel > 0) {
    audio.fadeOutChannel(s_currentChannel, s_currentFadeOutSeconds);
  } else if (!s_currentTrack.empty()) {
    audio.stop(s_currentTrack);
  }

  s_currentTrack = desired.track;
  s_currentContext = desiredContext;
  s_currentFadeOutSeconds = desired.fadeOutSeconds;
  s_currentChannel = Assets::SoundManager::Play(desired.track);
}

bool LevelMusicManager::RequestContext(const std::string &contextId,
                                       Audio::AudioManager &audio) {
  MusicContextConfig desired{};
  if (!ResolveTrackForContextId(contextId, desired) || desired.track.empty()) {
    return false;
  }

  if (desired.track == s_currentTrack && audio.isPlaying(desired.track)) {
    s_currentContext = desired.id;
    s_currentFadeOutSeconds = desired.fadeOutSeconds;
    return true;
  }

  if (s_currentChannel > 0) {
    audio.fadeOutChannel(s_currentChannel, s_currentFadeOutSeconds);
  } else if (!s_currentTrack.empty()) {
    audio.stop(s_currentTrack);
  }

  s_currentTrack = desired.track;
  s_currentContext = desired.id;
  s_currentFadeOutSeconds = desired.fadeOutSeconds;
  s_currentChannel = Assets::SoundManager::Play(desired.track);
  return s_currentChannel > 0;
}

bool LevelMusicManager::HasContext(const std::string &contextId) {
  MusicContextConfig resolved{};
  return ResolveTrackForContextId(contextId, resolved);
}

void LevelMusicManager::Stop(Audio::AudioManager &audio, float fadeOutSeconds) {
  const float fade = std::max(0.0f, fadeOutSeconds);
  if (s_currentChannel > 0) {
    audio.fadeOutChannel(s_currentChannel, fade);
  } else if (!s_currentTrack.empty()) {
    audio.stop(s_currentTrack);
  }

  s_currentChannel = 0;
  s_currentTrack.clear();
  s_currentContext.clear();
  s_currentFadeOutSeconds = fade;
}

std::string LevelMusicManager::CurrentContext() {
  return s_currentContext;
}

std::string LevelMusicManager::CurrentTrack() {
  return s_currentTrack;
}

} // namespace Runtime
