/******************************************************************************/
/*!
\file           SpriteAnimator.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          SpriteAnimator component - data for frame-based animation.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#pragma warning(push, 0)
#include "Core/json.hpp"
#pragma warning(pop)
#include <fstream>
#include <glm/vec4.hpp>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace Components {

struct FrameData {
  glm::vec4 uv;   // UV coordinates (x, y, w, h)
  float duration; // Duration of this frame in seconds
};

struct AnimationClip {
  std::string name;
  std::vector<FrameData> frames;
  bool loop = true;
};

/**
 * @brief SpriteAnimator component
 *
 * Holds animation state and clips.
 * The AnimationSystem uses this data to update the SpriteRenderer's UVs.
 */
struct SpriteAnimator {
  std::unordered_map<std::string, AnimationClip> clips;
  std::string currentClip;

  float timer{0.f};
  size_t frameIndex{0};

  bool playing{false};
  bool paused{false};
  float playbackSpeed{1.0f}; // Multiplier applied to frame timer (1 = authored speed)

  std::string filename; // Path to loaded animation file

  // Helper to add a clip easily
  void addClip(const AnimationClip &clip) {
    clips[clip.name] = clip;
    if (currentClip.empty())
      currentClip = clip.name;
  }

  AnimationClip &createClip(const std::string &name, bool loop = true) {
    AnimationClip clip;
    clip.name = name;
    clip.loop = loop;
    clips[name] = clip;
    if (currentClip.empty())
      currentClip = name;
    return clips[name];
  }

  void play(const std::string &name, bool force = false) {
    if (clips.find(name) == clips.end())
      return;
    if (!force && currentClip == name && playing)
      return;
    currentClip = name;
    playing = true;
    frameIndex = 0;
    timer = 0.0f;
  }

  void removeClip(const std::string &name) {
    clips.erase(name);
    if (currentClip == name) {
      currentClip = "";
      playing = false;
      if (!clips.empty())
        currentClip = clips.begin()->first;
    }
  }

  AnimationClip *getClip(const std::string &name) {
    auto it = clips.find(name);
    if (it != clips.end())
      return &it->second;
    return nullptr;
  }

  void clearAll() {
    clips.clear();
    currentClip = "";
    playing = false;
  }

  nlohmann::json serialize(const std::string &fname) {
    nlohmann::json j;
    j["name"] = fname;
    std::vector<nlohmann::json> anims;
    for (const auto &[name, clip] : clips) {
      nlohmann::json c;
      c["name"] = clip.name;
      c["loop"] = clip.loop;
      std::vector<nlohmann::json> frames;
      for (const auto &f : clip.frames) {
        nlohmann::json fr;
        fr["duration"] = f.duration;
        fr["u"] = f.uv.x;
        fr["v"] = f.uv.y;
        fr["w"] = f.uv.z;
        fr["h"] = f.uv.w;
        frames.push_back(fr);
      }
      c["frames"] = frames;
      anims.push_back(c);
    }
    j["animations"] = anims;
    return j;
  }

  void loadFromJson(const nlohmann::json &j) {
    clearAll();
    if (j.contains("animations")) {
      for (const auto &animJson : j["animations"]) {
        AnimationClip clip;
        clip.name = animJson.value("name", "untitled");
        clip.loop = animJson.value("loop", true);
        if (animJson.contains("frames")) {
          for (const auto &frameJson : animJson["frames"]) {
            FrameData frame;
            frame.duration = frameJson.value("duration", 0.1f);
            frame.uv.x = frameJson.value("u", 0.0f);
            frame.uv.y = frameJson.value("v", 0.0f);
            frame.uv.z = frameJson.value("w", 0.0f);
            frame.uv.w = frameJson.value("h", 0.0f);
            clip.frames.push_back(frame);
          }
        }
        addClip(clip);
      }
    }
  }

  void loadFromPath(const std::string &path) {
    filename = path;
    std::ifstream file(path);
    if (!file.is_open()) {
      std::cerr << "Failed to open animation file: " << path << std::endl;
      return;
    }

    nlohmann::json j;
    file >> j;
    loadFromJson(j);
  }
};

} // namespace Components
