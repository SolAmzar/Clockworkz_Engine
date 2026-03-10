/******************************************************************************/
/*!
\file           CameraSystem.hpp
\project        ClockWorkz Engine
\brief          Drives the global Camera2D from ECS data each frame.

                Priority order:
                  1. Entity with CameraComponent (isMain=true) – uses zoom
                     and offset from the component.
                  2. Entity tagged "Player" (no CameraComponent needed) –
                     engine default follow; preserves current zoom, no offset.
                  3. Nothing found – camera stays at its current state.

All content (C) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#pragma once

namespace ECS { class World; }

namespace Systems {

class CameraSystem {
public:
    void update(ECS::World& world);
};

} // namespace Systems
