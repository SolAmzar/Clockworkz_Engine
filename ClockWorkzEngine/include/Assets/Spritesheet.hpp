// Lebon - This is incomplete, intention is to do the spritesheet frame change
#pragma once
#include <vector>
#include <glm/vec4.hpp> // x,y,w,h in pixels
namespace  ClockWorkz::Assets {
  struct Frame { glm::vec4 rectPx; float duration = 0.1f; }; // seconds
  struct Spritesheet {
    std::vector<Frame> frames;
  };
}

