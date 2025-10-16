#pragma once
#include <memory>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace Graphics { class Texture2D; }
namespace Gameplay { class GameObject; }

namespace  ClockWorkz::Assets {
  struct SpriteRenderer {
    std::shared_ptr<Graphics::Texture2D> texture;
    // UV in pixels (spritesheet) or normalized if you prefer; we’ll treat as pixels + use tex size
    glm::vec4 uvPx {0,0,-1,-1}; // x,y,w,h; w<0 -> full texture
    glm::vec4 tint {1,1,1,1};
    int layer = 0;
  };
}
