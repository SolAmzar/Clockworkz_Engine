/******************************************************************************/
/*!
\file           LevelMusicManager.cpp
\project        Lights Out
\author(s)      [Sol-Amzar] 100%
\brief          Implements level music context and layered music management.

All content © 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Runtime/LevelMusicManager.hpp"

#include "Assets/SoundManager.hpp"

#pragma warning(push, 0)
#include "Core/json.hpp"
#pragma warning(pop)

#include <algorithm>
#include <fstream>
#include <iomanip>

namespace Runtime {

using json = nlohmann::json;

namespace {
std::vector<MusicLayerConfig> SanitizeLayers(const std::vector<MusicLayerConfig> &layers) {
  std::vector<MusicLayerConfig> sanitized;
  sanitized.reserve(layers.size());
  for (const auto &layer : layers) {
    if (layer.track.empty()) {
      continue;
    }

    auto existing = std::find_if(sanitized.begin(), sanitized.end(),
                                 [&](const MusicLayerConfig &item) {
                                   return item.track == layer.track;
                                 });
    if (existing == sanitized.end()) {
      sanitized.push_back({layer.track, std::clamp(layer.volume, 0.0f, 1.0f)});
    }
  }
  return sanitized;
}
} // namespace

bool LevelMusicManager::s_loaded = false;
std::vector<MusicContextConfig> LevelMusicManager::s_contexts{};
std::string LevelMusicManager::s_currentContext{};
std::vector<std::string> LevelMusicManager::s_currentTracks{};
float LevelMusicManager::s_currentFadeOutSeconds = 0.5f;
std::vector<int> LevelMusicManager::s_currentChannels{};

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

      if (entry.contains("layers") && entry["layers"].is_array()) {
        for (const auto &layer : entry["layers"]) {
          if (!layer.is_object()) {
            continue;
          }
          MusicLayerConfig cfgLayer;
          cfgLayer.track = layer.value("track", "");
          cfgLayer.volume = std::clamp(layer.value("volume", 1.0f), 0.0f, 1.0f);
          if (!cfgLayer.track.empty()) {
            cfg.layers.push_back(cfgLayer);
          }
        }
      } else if (entry.contains("tracks") && entry["tracks"].is_array()) {
        for (const auto &track : entry["tracks"]) {
          if (track.is_string()) {
            cfg.layers.push_back({track.get<std::string>(), 1.0f});
          }
        }
      } else {
        const std::string legacyTrack = entry.value("track", "");
        if (!legacyTrack.empty()) {
          cfg.layers.push_back({legacyTrack, 1.0f});
        }
      }

      cfg.layers = SanitizeLayers(cfg.layers);
      cfg.fadeOutSeconds = std::max(0.0f, entry.value("fadeOutSeconds", 0.5f));
      if (!cfg.id.empty()) {
        s_contexts.push_back(std::move(cfg));
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

    json layers = json::array();
    for (const auto &layer : SanitizeLayers(ctx.layers)) {
      layers.push_back({
          {"track", layer.track},
          {"volume", std::clamp(layer.volume, 0.0f, 1.0f)},
      });
    }

    root["contexts"].push_back({
        {"id", ctx.id},
        {"layers", layers},
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
    ctx.layers = SanitizeLayers(ctx.layers);
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
      return !outConfig.layers.empty();
    }
  }

  if (!scene.getBGM().empty()) {
    outConfig.id = MakeLegacyContextId(scene.getBGM());
    outConfig.layers = {{scene.getBGM(), 1.0f}};
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

bool LevelMusicManager::AreLayersActive(const std::vector<MusicLayerConfig> &layers,
                                        Audio::AudioManager &audio) {
  const auto desired = SanitizeLayers(layers);
  if (desired.size() != s_currentTracks.size()) {
    return false;
  }

  for (const auto &layer : desired) {
    if (std::find(s_currentTracks.begin(), s_currentTracks.end(), layer.track) ==
            s_currentTracks.end() ||
        !audio.isPlaying(layer.track)) {
      return false;
    }
  }

  return true;
}

bool LevelMusicManager::ApplyContext(const MusicContextConfig &desired,
                                     const std::string &desiredContext,
                                     Audio::AudioManager &audio) {
  const auto desiredLayers = SanitizeLayers(desired.layers);
  if (desiredLayers.empty()) {
    return false;
  }

  if (AreLayersActive(desiredLayers, audio)) {
    s_currentContext = desiredContext;
    s_currentFadeOutSeconds = desired.fadeOutSeconds;
    return true;
  }

  for (int channel : s_currentChannels) {
    if (channel > 0) {
      audio.fadeOutChannel(channel, s_currentFadeOutSeconds);
    }
  }
  if (s_currentChannels.empty()) {
    for (const auto &track : s_currentTracks) {
      if (!track.empty()) {
        audio.stop(track);
      }
    }
  }

  std::vector<int> newChannels;
  std::vector<std::string> startedTracks;
  newChannels.reserve(desiredLayers.size());
  startedTracks.reserve(desiredLayers.size());

  for (const auto &layer : desiredLayers) {
    const int channel = Assets::SoundManager::Play(layer.track, layer.volume, 1.0f);
    if (channel > 0) {
      newChannels.push_back(channel);
      startedTracks.push_back(layer.track);
    }
  }

  if (startedTracks.empty()) {
    return false;
  }

  s_currentTracks = std::move(startedTracks);
  s_currentChannels = std::move(newChannels);
  s_currentContext = desiredContext;
  s_currentFadeOutSeconds = desired.fadeOutSeconds;
  return true;
}

void LevelMusicManager::ApplySceneMusic(const Scene &scene, Audio::AudioManager &audio) {
  MusicContextConfig desired{};
  std::string desiredContext;
  if (!ResolveTrackForScene(scene, desired, desiredContext)) {
    Stop(audio);
    return;
  }

  if (!ApplyContext(desired, desiredContext, audio)) {
    Stop(audio, desired.fadeOutSeconds);
  }
}

bool LevelMusicManager::RequestContext(const std::string &contextId,
                                       Audio::AudioManager &audio) {
  MusicContextConfig desired{};
  if (!ResolveTrackForContextId(contextId, desired) || desired.layers.empty()) {
    return false;
  }

  return ApplyContext(desired, desired.id, audio);
}

bool LevelMusicManager::HasContext(const std::string &contextId) {
  MusicContextConfig resolved{};
  return ResolveTrackForContextId(contextId, resolved);
}

void LevelMusicManager::Stop(Audio::AudioManager &audio, float fadeOutSeconds) {
  const float fade = std::max(0.0f, fadeOutSeconds);
  for (int channel : s_currentChannels) {
    if (channel > 0) {
      audio.fadeOutChannel(channel, fade);
    }
  }
  if (s_currentChannels.empty()) {
    for (const auto &track : s_currentTracks) {
      if (!track.empty()) {
        audio.stop(track);
      }
    }
  }

  s_currentChannels.clear();
  s_currentTracks.clear();
  s_currentContext.clear();
  s_currentFadeOutSeconds = fade;
}

std::string LevelMusicManager::CurrentContext() {
  return s_currentContext;
}

std::string LevelMusicManager::CurrentTrack() {
  return s_currentTracks.empty() ? std::string{} : s_currentTracks.front();
}

} // namespace Runtime
