#include "Gameplay/Components/VideoPlayerComponent.hpp"
#include "Core/Engine.hpp"
#include "Gameplay/Components/SpriteRenderer.hpp"
#include "Graphics/RenderingManager.hpp"
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>


namespace Gameplay {

VideoPlayerComponent::VideoPlayerComponent() {}

VideoPlayerComponent::~VideoPlayerComponent() { m_decoder.stopWorkerThread(); }

void VideoPlayerComponent::loadVideo(const std::string &path) {
  if (m_decoder.load(path)) {
    m_data.filepath = path;
    m_decoder.startWorkerThread();
    if (m_data.autoplay) {
      play();
    }
  }
}

void VideoPlayerComponent::play() {
  m_data.isPlaying = true;
  m_decoder.play();
}

void VideoPlayerComponent::pause() {
  m_data.isPlaying = false;
  m_decoder.pause();
}

void VideoPlayerComponent::stop() {
  m_data.isPlaying = false;
  m_decoder.stop();
}

void VideoPlayerComponent::restart() {
  m_data.isPlaying = true;
  m_decoder.restart();
}

void VideoPlayerComponent::seek(float timeInSeconds) {
  m_decoder.seek(timeInSeconds);
}

void VideoPlayerComponent::onUpdate(GameObject &owner, double dt) {
  (void)owner;
  (void)dt;

  if (m_data.filepath != m_decoder.getFilepath() && !m_data.filepath.empty()) {
    loadVideo(m_data.filepath);
  }

  m_data.playbackState = m_decoder.getPlaybackState();
  m_data.duration = m_decoder.getDuration();
  m_data.currentTime = m_decoder.getCurrentTime();
  m_data.width = m_decoder.getWidth();
  m_data.height = m_decoder.getHeight();
  m_data.fps = m_decoder.getFps();
  m_decoder.setPlaybackSpeed(m_data.playbackSpeed);

  if (m_data.isPlaying && m_data.playbackState == "Paused") {
    m_decoder.play();
  } else if (!m_data.isPlaying && m_data.playbackState == "Playing") {
    m_decoder.pause();
  }

  if (m_data.loop && m_data.playbackState == "Finished") {
    restart();
  } else if (m_data.playbackState == "Finished") {
    m_data.isPlaying = false;
  }

  Video::FrameData frame;
  if (m_decoder.getFrameQueue().tryPop(frame)) {
    if (m_texture.getWidth() != frame.width ||
        m_texture.getHeight() != frame.height) {
      m_texture.init(frame.width, frame.height);
    }
    m_texture.updatePixels(frame.rgbPixels.data());
    m_data.isTextureReady = true;
  }
}

void VideoPlayerComponent::onRender(GameObject &owner) {
  if (m_data.isTextureReady) {
    Graphics::RenderingManager &renderer = Core::Engine::Get().rendering();

    glm::vec2 pos(owner.transform().position.x, owner.transform().position.y);
    glm::vec2 size(owner.size().x * owner.transform().scale.x,
                   owner.size().y * owner.transform().scale.y);

    if (m_data.targetMode == Components::VideoTargetMode::SpriteRenderer) {
      if (auto *sprite = owner.getComponent<SpriteRendererComponent>()) {
        sprite->setTint(
            glm::vec4(1.f, 1.f, 1.f, 0.f)); // hide underlying sprite color
      }
      renderer.drawSprite(m_texture.getRendererId(), pos, size,
                          owner.transform().rotationRad,
                          glm::vec4(0.f, 0.f, 1.f, 1.f), glm::vec4(1.f), false);
    }
  }
}

} // namespace Gameplay
