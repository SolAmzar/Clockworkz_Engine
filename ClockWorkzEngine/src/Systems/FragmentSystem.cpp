/******************************************************************************/
/*!
\file           FragmentSystem.cpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          Implementation of FragmentSystem.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Systems/FragmentSystem.hpp"
#include "Components/Fragment.hpp"
#include "Components/Transform.hpp"
#include <cmath> // for sin

namespace Systems {

void FragmentSystem::update(ECS::World &world, double dt) {
  float fDt = static_cast<float>(dt);

  // Iterate entities that have BOTH Fragment and Transform
  // Note: We currently don't have a multi-component view helper in World yet,
  // so we have to check for Transform inside the Fragment loop or use a view if
  // available. Given current World implementation, we check existence.

  world.each<Components::Fragment>(
      [&](ECS::Entity entity, Components::Fragment &frag) {
        if (frag.collected)
          return; // Don't animate collected fragments

        auto *transform = world.getComponent<Components::Transform>(entity);
        if (!transform)
          return; // Needs transform to move

        // Calculate previous offset based on old timer
        float oldOffset =
            std::sin(frag.animationTimer * frag.bobSpeed) * frag.bobAmount;

        // Update timer
        frag.animationTimer += fDt;

        // Calculate new offset
        float newOffset =
            std::sin(frag.animationTimer * frag.bobSpeed) * frag.bobAmount;

        // Apply delta to position
        // This ensures we don't drift or snap if bobAmount changes
        transform->position.y += (newOffset - oldOffset);
      });
}

} // namespace Systems
