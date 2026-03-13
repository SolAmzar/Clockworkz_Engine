#pragma once
#include "ECS/World.hpp"

namespace Graphics {
class RenderingManager;
}

namespace Systems {

class VideoPlaybackSystem {
public:
  VideoPlaybackSystem();
  ~VideoPlaybackSystem();

  void update(ECS::World &world, Graphics::RenderingManager &renderer);
  void cleanup(ECS::World &world);

private:
  void cleanupEntity(ECS::World &world, ECS::Entity entity);
};

} // namespace Systems
