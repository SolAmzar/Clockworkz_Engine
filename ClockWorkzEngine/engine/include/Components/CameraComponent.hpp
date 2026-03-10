/******************************************************************************/
/*!
\file           CameraComponent.hpp
\project        ClockWorkz Engine
\brief          ECS data component for controlling the global Camera2D.
                Attach to any entity (e.g. the player) to make it the scene
                camera.  CameraSystem reads this each frame and drives
                Camera2D accordingly.

                isMain  – only one entity should be the primary camera.
                zoom    – overrides the global camera zoom.
                offset  – world-space offset from the entity's transform.

All content (C) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#pragma once
#include <glm/vec2.hpp>

namespace Components {

struct CameraComponent {
    bool      isMain = true;       // drives the global Camera2D when true
    float     zoom   = 1.0f;       // zoom level (1.0 = no zoom, 0.5 = zoomed out)
    glm::vec2 offset = {0.f, 0.f}; // world-space offset from entity transform
};

} // namespace Components
