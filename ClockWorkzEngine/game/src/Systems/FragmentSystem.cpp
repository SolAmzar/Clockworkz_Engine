/******************************************************************************/
/*!
\file           FragmentSystem.cpp
\project        ClockWorkz Engine
\author(s)      [Lebon], 100%
\brief          Implementation of FragmentSystem.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Systems/FragmentSystem.hpp"
#include "Components/Battery.hpp"
#include "Components/Fragment.hpp"
#include "Components/FragmentCollector.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/Light.hpp"
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

    // --- VISIBILITY LOGIC ---
    // Default target is invisible
    float targetAlpha = 0.0f;

    // If light is not required, always visible
    if (!frag.requiresLight) {
      targetAlpha = 1.0f;
    } else {
      // Check against all torches... (logic continues)

      // Check against all torches
      world.each<Components::Transform, Components::Light>(
          [&](ECS::Entity tEntity, Components::Transform &tTrans,
              Components::Light &torch) {
            if (!torch.active)
              return;
            if (targetAlpha >= 1.0f)
              return; // Already fully lit

            // Check battery if present
            auto *batt = world.getComponent<Components::Battery>(tEntity);
            if (batt && batt->charge <= 0.0f)
              return;

            // Calculate Torch World Position
            float rot = tTrans.rotation;
            // rot is in radians.

            // Rotate offset
            float c = std::cos(rot);
            float s = std::sin(rot);
            vec2 torchPos;
            torchPos.x =
                tTrans.position.x + (torch.offset.x * c - torch.offset.y * s);
            torchPos.y =
                tTrans.position.y + (torch.offset.x * s + torch.offset.y * c);

            // Vector to fragment
            vec2 toFrag(transform->position.x - torchPos.x,
                        transform->position.y - torchPos.y);
            float distSq = toFrag.x * toFrag.x + toFrag.y * toFrag.y;
            float radiusSq = torch.radius * torch.radius;

            if (distSq <= radiusSq) {
              // Check Angle
              // Torch Direction
              vec2 torchDir(std::cos(rot), std::sin(rot));
              vec2 fragDir = toFrag;

              // Normalize fragDir
              float dist = std::sqrt(distSq);
              if (dist > 0.0001f) {
                fragDir.x /= dist;
                fragDir.y /= dist;
              }

              float dot = torchDir.x * fragDir.x + torchDir.y * fragDir.y;
              // dot > cos(halfAngle)
              // torch.angle is full FOV in DEGREES. Convert to radians half
              // angle.
              float halfAngleRad = (torch.angle * 3.14159f / 180.0f) * 0.5f;
              if (dot >= std::cos(halfAngleRad)) {
                targetAlpha = 1.0f; // Visible!
              }
            }
          });
    }

    // Smooth Fade
    if (frag.currentAlpha < targetAlpha) {
      frag.currentAlpha += frag.fadeSpeed * fDt;
      if (frag.currentAlpha > targetAlpha)
        frag.currentAlpha = targetAlpha;
    } else if (frag.currentAlpha > targetAlpha) {
      frag.currentAlpha -= frag.fadeSpeed * fDt;
      if (frag.currentAlpha < targetAlpha)
        frag.currentAlpha = targetAlpha;
    }

    // Apply alpha to sprite
    auto *sprite = world.getComponent<Components::SpriteRenderer>(entity);
    if (sprite) {
      sprite->color.a = frag.currentAlpha;
      sprite->unlit = true; // Fragments should ignore global darkness
    }

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
              sys.lifeMin = 0.2f;
              sys.lifeMax = 0.4f;
              sys.colorStart = {1.f, 1.f, 0.2f, 1.f}; // Yellowish
              sys.colorEnd = {1.f, 1.f, 1.f, 0.f};    // Fade out white
              sys.sizeStart = 8.0f;
              sys.sizeEnd = 0.0f;
              sys.speedMin = 200.0f;
              sys.speedMax = 400.0f;
              sys.spreadDeg = 360.0f; // All directions

              world.addComponent(entity, emitter);

              // Burst!
              // We need to retrieve it to call burst, or do it above if
              // addComponent copies? addComponent copies the value. So we
              // need to get the pointer from world.
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
