/******************************************************************************/
/*!
\file           Animator.hpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares the Animator struct responsible for controlling
                frame-based sprite animations using data from a Spritesheet.

                Responsibilities:
                - Tracks current frame, elapsed time, and looping behavior.
                - Updates animation state based on frame duration timing.
                - Provides currentRectPx() to fetch the active frame’s pixel
                  rectangle for rendering.
                - Integrates with SpriteRenderer and Spritesheet to enable
                  animated textures in-game.

                The Animator operates on non-owned Spritesheet data, allowing
                multiple entities to share the same spritesheet efficiently.

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

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
