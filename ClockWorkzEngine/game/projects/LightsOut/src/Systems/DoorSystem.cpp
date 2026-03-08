/******************************************************************************/
/*!
\file           DoorSystem.cpp
\project        ClockWorkz Engine
\author(s)      [Lebon], 100%
\brief          Implementation of DoorSystem.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Systems/DoorSystem.hpp"
#include "Components/Collider.hpp"
#include "Components/Door.hpp"
#include "Components/FragmentCollector.hpp"
#include "Components/SpriteRenderer.hpp"

namespace Systems {

void DoorSystem::update(ECS::World &world, double dt) {
  (void)dt;

  // 1. Find Total Collected Fragments (Assuming Single Player for now)
  int totalCollected = 0;

  world.each<Components::FragmentCollector>(
      [&](ECS::Entity, Components::FragmentCollector &collector) {
        totalCollected = collector.collectedCount;
      });

  // 2. Update Doors
  world
      .each<Components::Door, Components::Collider, Components::SpriteRenderer>(
          [&](ECS::Entity, Components::Door &door, Components::Collider &col,
              Components::SpriteRenderer &sprite) {
            if (!door.isOpen) {
              // Check Condition
              if (totalCollected >= door.requiredFragments) {
                // UNLOCK!
                door.isOpen = true;

                // Update Visuals
                sprite.textureId = door.openTexture;

                // Update Physics
                col.moveable = true;
                col.isSolid = true; // Still solid, but moveable
              } else {
                // Ensure it stays closed if not met (in case of reset/init)
                sprite.textureId = door.closedTexture;
                col.moveable = false;
              }
            }
          });
}

} // namespace Systems
