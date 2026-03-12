#pragma once
#include <string>

namespace Components {

enum class VideoTargetMode {
  SpriteRenderer, // Render using the entity's SpriteRenderer transform
  UIQuad, // Render on UI Layer (to be implemented later based on UI system)
  MaterialSlot // Custom material rendering
};

struct VideoPlayer {
  std::string filepath{""};
  bool isPlaying{false};
  bool loop{false};
  bool autoplay{true};
  bool mute{true};
  float playbackSpeed{1.0f};

  VideoTargetMode targetMode{VideoTargetMode::SpriteRenderer};

  // Read-only status info exposed to Editor
  float currentTime{0.0f};
  float duration{0.0f};
  int width{0};
  int height{0};
  float fps{0.0f};
  std::string playbackState{"Stopped"};
  bool isTextureReady{false};

  // Internal opaque pointer to the engine side decoder/texture manager.
  // Stored as void* to keep component as plain old data/headers free.
  void *internalDecoderHandle{nullptr};
};

} // namespace Components
