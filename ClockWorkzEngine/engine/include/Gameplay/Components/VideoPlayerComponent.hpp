#pragma once
#include "Components/VideoPlayer.hpp"
#include "Gameplay/GameObject.hpp"
#include "Graphics/OpenGLVideoTexture.hpp"
#include "Video/FFmpegDecoder.hpp"


namespace Gameplay {

class VideoPlayerComponent : public Component {
public:
  VideoPlayerComponent();
  ~VideoPlayerComponent() override;

  // Component overrides
  void onUpdate(GameObject &owner, double dt) override;
  void onRender(GameObject &owner) override;

  // Access to the ECS-style data struct for easy editor property binding
  Components::VideoPlayer &data() { return m_data; }
  const Components::VideoPlayer &data() const { return m_data; }

  void loadVideo(const std::string &path);
  void play();
  void pause();
  void stop();
  void restart();
  void seek(float timeInSeconds);

private:
  Components::VideoPlayer m_data;
  Video::FFmpegDecoder m_decoder;
  Graphics::OpenGLVideoTexture m_texture;
};

} // namespace Gameplay
