/******************************************************************************/
/*!
\file           UISlider.hpp
\project        ClockWorkz Engine
\author(s)      [Syafiq]
\brief          UISlider component for editor-authored audio volume sliders.
                Attach to any entity with Transform to make it an interactive
                slider. The 'settingKey' maps to an AudioSettings field
                ("master", "bgm", "sfx"). Visual properties (colors, sizes)
                are all data-driven via the scene JSON.

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#pragma once
#include <string>
#include <glm/vec4.hpp>

namespace Components {

struct UISlider {
    std::string settingKey;          // "master", "bgm", or "sfx"
    float minValue    = 0.f;
    float maxValue    = 1.f;
    float value       = 1.f;
    float trackWidth  = 400.f;       // total track length in world units
    float trackHeight = 18.f;        // track bar thickness
    float handleSize  = 22.f;        // handle square side length
    glm::vec4 trackColor  = {0.25f, 0.25f, 0.25f, 1.f};
    glm::vec4 fillColor   = {0.85f, 0.55f, 0.10f, 1.f};
    glm::vec4 handleColor = {1.f,   1.f,   1.f,   1.f};
    bool dragging = false;           // runtime only — not serialised
};

} // namespace Components
