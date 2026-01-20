/******************************************************************************/
/*!
\file           AnimationSystem.cpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          Implementation of AnimationSystem.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Systems/AnimationSystem.hpp"
#include "Components/SpriteAnimator.hpp"
#include "Components/SpriteRenderer.hpp"
#include <iostream>

namespace Systems {

void AnimationSystem::update(ECS::World &world, double dt) {
  world.each<Components::SpriteAnimator, Components::SpriteRenderer>(
      [&](ECS::Entity entity, Components::SpriteAnimator &anim,
          Components::SpriteRenderer &sprite) {
        (void)entity;

        // If not playing or paused, skip
        if (!anim.playing || anim.paused)
          return;

        // Check if current clip exists
        auto it = anim.clips.find(anim.currentClip);
        if (it == anim.clips.end())
          return;

        const auto &clip = it->second;
        if (clip.frames.empty())
          return;

        // Validation: Ensure frameIndex is within bounds
        if (anim.frameIndex >= clip.frames.size()) {
          anim.frameIndex = 0;
        }

        // Update timer
        anim.timer += static_cast<float>(dt);

        // Check frame duration
        float duration = clip.frames[anim.frameIndex].duration;

        while (anim.timer >= duration && duration > 0.f) {
          anim.timer -= duration;
          anim.frameIndex++;

          // Handle end of clip
          if (anim.frameIndex >= clip.frames.size()) {
            if (clip.loop) {
              anim.frameIndex = 0;
            } else {
              anim.frameIndex = clip.frames.size() - 1;
              anim.playing = false; // Stop playing
              // anim.onComplete()? // callbacks not supported in pure ECS data
              // yet
            }
          }

          // Update duration for the new frame (for the while loop check)
          duration = clip.frames[anim.frameIndex].duration;
        }

        // Apply UVs to SpriteRenderer
        sprite.uv = clip.frames[anim.frameIndex].uv;
      });
}

} // namespace Systems
