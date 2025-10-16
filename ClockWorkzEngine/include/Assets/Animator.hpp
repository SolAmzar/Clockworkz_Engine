#pragma once
#include "Assets/Spritesheet.hpp"
namespace  ClockWorkz::Assets {
  struct Animator {
    const Spritesheet* sheet = nullptr; // owned elsewhere (e.g., SpriteManager)
    float time = 0.f;
    int frame = 0;
    bool loop = true;

    void update(float dt) {
      if (!sheet || sheet->frames.empty()) return;
      time += dt;
      while (time >= sheet->frames[frame].duration) {
        time -= sheet->frames[frame].duration;
        frame++;
        if (frame >= (int)sheet->frames.size()) frame = loop ? 0 : (int)sheet->frames.size()-1;
      }
    }
    glm::vec4 currentRectPx() const {
      if (!sheet || sheet->frames.empty()) return {0,0,-1,-1};
      return sheet->frames[frame].rectPx;
    }
  };
}
