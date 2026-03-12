/******************************************************************************/
/*!
\file           VideoPlaybackSystem.cpp
\project        ClockWorkz Engine
\author(s)      [Lebon], 100%
\brief          Implements timed ECS video playback and rendering.

                Initializes decoder contexts on demand, presents frames using
                playback timestamps, and updates SpriteRenderer targets with the
                latest video texture.

All content © 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Systems/VideoPlaybackSystem.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/Transform.hpp"
#include "Components/VideoPlayer.hpp"
#include "Graphics/OpenGLVideoTexture.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Video/FFmpegDecoder.hpp"
#include <algorithm>
#include <chrono>


namespace Systems {

struct VideoDecoderContext {
  Video::FFmpegDecoder decoder;
  Graphics::OpenGLVideoTexture texture;
  std::string currentLoadedPath;
  std::chrono::steady_clock::time_point playbackStartTime{};
  double playbackStartPts{0.0};
  double presentedPts{0.0};
  bool playbackClockStarted{false};
  bool wasPlayingLastFrame{false};
};

VideoPlaybackSystem::VideoPlaybackSystem() {}

VideoPlaybackSystem::~VideoPlaybackSystem() {}

void VideoPlaybackSystem::cleanup(ECS::World &world) {
  world.each<Components::VideoPlayer>(
      [&](ECS::Entity entity, Components::VideoPlayer &) {
        cleanupEntity(world, entity);
      });
}

void VideoPlaybackSystem::cleanupEntity(ECS::World &world, ECS::Entity entity) {
  if (auto *player = world.getComponent<Components::VideoPlayer>(entity)) {
    if (player->internalDecoderHandle) {
      delete static_cast<VideoDecoderContext *>(player->internalDecoderHandle);
      player->internalDecoderHandle = nullptr;
    }
  }
}

void VideoPlaybackSystem::update(ECS::World &world,
                                 Graphics::RenderingManager &renderer) {
  world.each<Components::VideoPlayer>([&](ECS::Entity entity,
                                          Components::VideoPlayer &player) {
    using Clock = std::chrono::steady_clock;

    if (!player.internalDecoderHandle) {
      player.internalDecoderHandle = new VideoDecoderContext();
    }
    auto *ctx =
        static_cast<VideoDecoderContext *>(player.internalDecoderHandle);

    if (player.filepath != ctx->currentLoadedPath && !player.filepath.empty()) {
      if (ctx->decoder.load(player.filepath)) {
        ctx->currentLoadedPath = player.filepath;
        ctx->decoder.startWorkerThread();
        ctx->playbackStartPts = 0.0;
        ctx->presentedPts = 0.0;
        ctx->playbackStartTime = Clock::now();
        ctx->playbackClockStarted = false;
        ctx->wasPlayingLastFrame = false;
        if (player.autoplay) {
          player.isPlaying = true;
          ctx->decoder.play();
        } else {
          player.isPlaying = false;
        }
      } else {
        ctx->currentLoadedPath = "";
      }
    }

    player.playbackState = ctx->decoder.getPlaybackState();
    player.duration = ctx->decoder.getDuration();
    player.currentTime = ctx->decoder.getCurrentTime();
    player.width = ctx->decoder.getWidth();
    player.height = ctx->decoder.getHeight();
    player.fps = ctx->decoder.getFps();
    ctx->decoder.setPlaybackSpeed(player.playbackSpeed);

    if (player.isPlaying && player.playbackState == "Paused") {
      ctx->decoder.play();
    } else if (!player.isPlaying && player.playbackState == "Playing") {
      ctx->decoder.pause();
    }

    const bool isActivelyPlaying = player.isPlaying &&
                                   player.playbackState == "Playing";
    if (isActivelyPlaying && !ctx->wasPlayingLastFrame) {
      ctx->playbackStartTime = Clock::now();
      ctx->playbackStartPts = ctx->presentedPts;
      ctx->playbackClockStarted = true;
    } else if (!isActivelyPlaying && ctx->wasPlayingLastFrame &&
               ctx->playbackClockStarted) {
      const auto elapsed = std::chrono::duration<double>(
          Clock::now() - ctx->playbackStartTime);
      ctx->presentedPts = ctx->playbackStartPts +
                          elapsed.count() * std::max(player.playbackSpeed, 0.01f);
    }
    ctx->wasPlayingLastFrame = isActivelyPlaying;

    if (player.loop && player.playbackState == "Finished") {
      ctx->decoder.restart();
      player.isPlaying = true;
      ctx->playbackStartPts = 0.0;
      ctx->presentedPts = 0.0;
      ctx->playbackStartTime = Clock::now();
      ctx->playbackClockStarted = true;
    } else if (player.playbackState == "Finished") {
      player.isPlaying = false;
    }

    double playbackTime = ctx->presentedPts;
    if (isActivelyPlaying && ctx->playbackClockStarted) {
      const auto elapsed = std::chrono::duration<double>(
          Clock::now() - ctx->playbackStartTime);
      playbackTime = ctx->playbackStartPts +
                     elapsed.count() * std::max(player.playbackSpeed, 0.01f);
    }

    const double frameLeadTolerance =
        1.0 / std::max(player.fps > 0.0f ? player.fps : 30.0f, 1.0f);

    Video::FrameData nextFrame;
    while (ctx->decoder.getFrameQueue().tryPeek(nextFrame) &&
           (!player.isTextureReady ||
            nextFrame.pts <= playbackTime + frameLeadTolerance)) {
      Video::FrameData frame;
      if (!ctx->decoder.getFrameQueue().tryPop(frame)) {
        break;
      }
      if (ctx->texture.getWidth() != frame.width ||
          ctx->texture.getHeight() != frame.height) {
        ctx->texture.init(frame.width, frame.height);
      }
      ctx->texture.updatePixels(frame.rgbPixels.data());
      player.isTextureReady = true;
      ctx->presentedPts = frame.pts;
      if (!ctx->playbackClockStarted) {
        ctx->playbackStartTime = Clock::now();
        ctx->playbackStartPts = ctx->presentedPts;
        ctx->playbackClockStarted = true;
      }
    }

    player.currentTime = static_cast<float>(std::min(
        playbackTime, static_cast<double>(player.duration)));

    if (player.targetMode == Components::VideoTargetMode::SpriteRenderer &&
        player.isTextureReady) {
      auto *transform = world.getComponent<Components::Transform>(entity);
      if (transform) {
        auto *sprite = world.getComponent<Components::SpriteRenderer>(entity);
        if (sprite && sprite->visible) {
          sprite->visible = false;
        }

        glm::vec2 position(transform->position.x, transform->position.y);
        float sizeX = sprite ? sprite->size.x : 100.f;
        float sizeY = sprite ? sprite->size.y : 100.f;
        glm::vec2 size(sizeX * transform->scale.x, sizeY * transform->scale.y);

        float rotation = transform->rotation;
        glm::vec4 color(1.0f);
        glm::vec4 uv(0, 1, 1, -1);
        glm::vec2 pivot(transform->pivot.x, transform->pivot.y);

        renderer.drawSprite(ctx->texture.getRendererId(), position, size,
                            rotation, uv, color, false, pivot);
      }
    }
  });
}
} // namespace Systems
