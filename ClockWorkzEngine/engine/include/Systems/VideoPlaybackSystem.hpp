/******************************************************************************/
/*!
\file           VideoPlaybackSystem.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon], 100%
\brief          Declares the ECS system responsible for decoding, timing, and
                rendering video playback.

                Updates VideoPlayer components, synchronizes decoded frames to
                playback time, and routes video output to scene sprites.

All content © 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

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
