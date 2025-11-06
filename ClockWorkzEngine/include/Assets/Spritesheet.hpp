/******************************************************************************/
/*!
\file           Spritesheet.hpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares simple data structures representing frame-based
                spritesheet animation data.

                Responsibilities:
                - Frame: Defines a single animation frame containing a pixel
                  rectangle (x, y, width, height) and display duration in seconds.
                - Spritesheet: Stores an ordered list of Frames used by Animator
                  for time-based sprite animation.

                These structs provide lightweight data containers that can be
                serialized or loaded from JSON and are referenced by the
                Animator system to drive 2D animated sprites.

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
#include <vector>
#include <glm/vec4.hpp> // x,y,w,h in pixels
namespace  ClockWorkz::Assets {
  struct Frame { glm::vec4 rectPx; float duration = 0.1f; }; // seconds
  struct Spritesheet {
    std::vector<Frame> frames;
  };
}

