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
#include "Components/FragmentCollector.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/Transform.hpp"
#include "Gameplay/Components/ParticleEmitter.hpp"
#include "Math/Vec2.hpp" // Use custom Math
#include <cmath>         // for sin


namespace Systems {

void FragmentSystem::update(ECS::World &world, double dt) {
  float fDt = static_cast<float>(dt);

  std::vector<ECS::Entity> toDestroy;

  // Iterate entities that have BOTH Fragment and Transform
  world.each<Components::Fragment>([&](ECS::Entity entity,
                                       Components::Fragment &frag) {
    // --- COLLECTED STATE LOGIC ---
    if (frag.collected) {
      // Use animationTimer as "Death Timer"
      frag.animationTimer += fDt;
      if (frag.animationTimer > 1.0f) { // 1 second for particles to play
        toDestroy.push_back(entity);
      }
      return; // Skip animation and re-collection
    }

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
    transform->position.y += (newOffset - oldOffset);

    // --- COLLECTION LOGIC ---
    // Iterate all Collectors (Players)
    world.each<Components::FragmentCollector, Components::Transform>(
        [&](ECS::Entity, Components::FragmentCollector &collector,
            Components::Transform &colTrans) {
          // Calculate Distance using Custom Math
          vec2 fragPos(transform->position.x, transform->position.y);
          vec2 colPos(colTrans.position.x, colTrans.position.y);

          // dist = magnitude(p2 - p1)
          float dist = magnitude(fragPos - colPos);

          // Check Radius
          if (dist <= collector.pickupRadius) {
            // Collect!
            frag.collected = true;
            frag.animationTimer = 0.0f; // Reset timer for death tracking
            collector.collectedCount++;

            // 1. Remove Sprite (Hide Fragment)
            if (world.hasComponent<Components::SpriteRenderer>(entity)) {
              world.removeComponent<Components::SpriteRenderer>(entity);
            }

            // 2. Add Particle Emitter (Sparkle)
            // Note: Assuming we can add component on the fly
            if (!world.hasComponent<Components::ParticleEmitter>(entity)) {
              Components::ParticleEmitter emitter(128); // Max particles

              // Config
              auto &sys = emitter.system();
              sys.lifeMin = 0.5f;
              sys.lifeMax = 1.0f;
              sys.colorStart = {1.f, 1.f, 0.2f, 1.f}; // Yellowish
              sys.colorEnd = {1.f, 1.f, 1.f, 0.f};    // Fade out white
              sys.sizeStart = 8.0f;
              sys.sizeEnd = 0.0f;
              sys.speedMin = 50.0f;
              sys.speedMax = 150.0f;
              sys.spreadDeg = 180.0f; // All directions

              world.addComponent(entity, emitter);

              // Burst!
              // We need to retrieve it to call burst, or do it above if
              // addComponent copies? addComponent copies the value. So we need
              // to get the pointer from world.
              auto *e = world.getComponent<Components::ParticleEmitter>(entity);
              if (e)
                e->burst(30);
            }
          }
        });
  });

  // Cleanup
  for (auto e : toDestroy) {
    world.destroyEntity(e);
  }
}

} // namespace Systems
