/******************************************************************************/
/*!
\file           main_game.cpp
\project        Lights Out
\author(s)      [Syafiq] 71% [Sol-Amzar] 27% [Lebon] 2%
\brief          Entry point for LightsOut.exe
ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â
game executable. Boots straight to MenuState. All game logic lives in Lua
scripts. No C++ game systems needed.

All content (c) 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Core/Engine.hpp"
#include "ECS/World.hpp"
#include "Runtime/SceneSerializer.hpp"
#include "Scripting/GameLuaBindings.hpp"

#pragma warning(push, 0)
#include "Core/json.hpp"
#pragma warning(pop)
#include "Audio/FootstepAudioRuntime.hpp"
#include "Components/Collider.hpp"
#include "Components/FootstepAudio.hpp"
#include "Components/SpriteAnimator.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/Tag.hpp"
#include "Components/Transform.hpp"
#include "Components/FragmentCollector.hpp"
#include "Components/LuaScript.hpp"
#include "Components/ParticleEmitter.hpp"
#include "Gameplay/Components/SurfaceComponent.hpp"
#include "GlobalAudioSettings.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Runtime/LevelMusicManager.hpp"
#include "Runtime/Scene.hpp"
#include "States/IntroCutsceneState.hpp"
#include "States/MenuState.hpp"
#include "Systems/CollisionSystem.hpp"
#include "Systems/LightRenderSystem.hpp"
#include <iostream>
#include <memory>


namespace {
vec2 resolvePlayerSpawn(ECS::World &world, const vec2 &fallback) {
  vec2 resolved = fallback;
  bool foundMarker = false;
  world.each<Components::Tag, Components::Transform>(
      [&](ECS::Entity, Components::Tag &tag, Components::Transform &transform) {
        if (foundMarker || tag.tag != "PlayerSpawn")
          return;
        resolved = transform.position;
        foundMarker = true;
      });
  return resolved;
}
} // namespace

int main() {
  std::cout << "[Main] Booting LightsOut...\n";

  Core::Engine engine;

  // Register components that have no Lua equivalent yet (SurfaceComponent,
  // FootstepAudio). All other game components (Battery, Door,
  // Fragment, etc.) live purely in Lua scripts.
  engine.setComponentRegistrar([](ECS::World &world) {
    world.registerComponent<Gameplay::SurfaceComponent>();
    world.registerComponent<Components::FootstepAudio>();
    world.registerComponent<Components::FragmentCollector>();
  });

  // Extend EntityRef with game-component accessors backed by Lua script
  // properties.
  engine.setLuaExtensionRegistrar(Scripting::RegisterGameBindings);

  // Serializer extensions for components that still live in C++.
  Runtime::SceneSerializer::setExtensions(
      [](ECS::Entity entity, ECS::World &world, void *rawComponents) {
        using json = nlohmann::json;
        auto &comps = *static_cast<json *>(rawComponents);
        if (auto *sc = world.getComponent<Gameplay::SurfaceComponent>(entity))
          comps["surface"] = {{"type", static_cast<int>(sc->type())}};
        if (auto *fa = world.getComponent<Components::FootstepAudio>(entity)) {
          comps["footstepAudio"] = {
              {"interval", fa->interval},
              {"sprintIntervalScale", fa->sprintIntervalScale},
              {"sprintSpeedThreshold", fa->sprintSpeedThreshold},
              {"minSpeed", fa->minSpeed},
              {"resetDelay", fa->resetDelay},
              {"volume", fa->volume},
              {"variationCount", fa->variationCount},
              {"useSurfaceSounds", fa->useSurfaceSounds},
              {"soundPrefix", fa->soundPrefix},
              {"concretePrefix", fa->concretePrefix},
              {"tilePrefix", fa->tilePrefix},
              {"woodPrefix", fa->woodPrefix}};
        }
      },
      [](ECS::Entity entity, ECS::World &world, const void *rawComponents) {
        using json = nlohmann::json;
        const auto &c = *static_cast<const json *>(rawComponents);
        if (c.contains("surface")) {
          Gameplay::SurfaceComponent sc;
          sc.setType(static_cast<Gameplay::SurfaceType>(
              c["surface"].value("type", 0)));
          world.addComponent(entity, sc);
          if (!world.getComponent<Components::Collider>(entity)) {
            Components::Collider col;
            if (auto *spr =
                    world.getComponent<Components::SpriteRenderer>(entity))
              col.size = spr->size;
            col.isSolid = false;
            col.isTrigger = true;
            world.addComponent(entity, col);
          }
        }
        if (c.contains("footstepAudio")) {
          const auto &fac = c["footstepAudio"];
          Components::FootstepAudio fa;
          fa.interval = fac.value("interval", fa.interval);
          fa.sprintIntervalScale =
              fac.value("sprintIntervalScale", fa.sprintIntervalScale);
          fa.sprintSpeedThreshold =
              fac.value("sprintSpeedThreshold", fa.sprintSpeedThreshold);
          fa.minSpeed = fac.value("minSpeed", fa.minSpeed);
          fa.resetDelay = fac.value("resetDelay", fa.resetDelay);
          fa.volume = fac.value("volume", fa.volume);
          fa.variationCount = fac.value("variationCount", fa.variationCount);
          fa.useSurfaceSounds =
              fac.value("useSurfaceSounds", fa.useSurfaceSounds);
          fa.soundPrefix = fac.value("soundPrefix", fa.soundPrefix);
          fa.concretePrefix = fac.value("concretePrefix", fa.concretePrefix);
          fa.tilePrefix = fac.value("tilePrefix", fa.tilePrefix);
          world.addComponent(entity, fa);
        }

        if (c.contains("luaScript")) {
          for (auto &lsj : c["luaScript"]) {
            if (lsj.value("scriptPath", "") == "components/fragment_collector.lua") {
              if (lsj.contains("properties")) {
                const auto &p = lsj["properties"];
                Components::FragmentCollector fc;
                fc.requiredFragments = p.value("requiredFragments", 0);
                fc.collectedCount = p.value("collectedCount", 0);
                fc.pickupRadius = p.value("pickupRadius", 50.f);
                world.addComponent(entity, fc);
              }
            }
          }
        }
      });

  // Scene setup: footstep audio + darkness light pass.
  // Player/AI/portal/door logic all live in Lua scripts now.
  engine.setSceneSetup([](Runtime::Scene &scene) {
    scene.setGameCallbacks(
        // onUpdate: footstep audio
        [](ECS::World &world, double dt, Systems::CollisionSystem &,
           Runtime::Scene &) {
          Audio::UpdateFootstepListener(world);
          Audio::UpdateFootstepAudio(world, dt);
        },
        // onRender: darkness/light pass
        [](ECS::World &world, Graphics::RenderingManager &renderer) {
          Systems::renderLightPass(world, renderer);
        },
        // onLoad: reset player position to spawn
        [](ECS::World &world, const vec2 &spawn, Runtime::Scene &scene) {
          const vec2 resolvedSpawn = resolvePlayerSpawn(world, spawn);
          world.each<Components::Tag, Components::Transform>(
              [&](ECS::Entity, Components::Tag &tag, Components::Transform &t) {
                if (tag.tag == "Player")
                  t.position = resolvedSpawn;
              });
          Runtime::GlobalAudioSettings::Apply(Core::Engine::Get().audio());
          Runtime::LevelMusicManager::ApplySceneMusic(
              scene, Core::Engine::Get().audio());
        },
        // onRespawn: restore player state
        [](ECS::World &world, const vec2 &spawn) {
          const vec2 resolvedSpawn = resolvePlayerSpawn(world, spawn);
          world.each<Components::Tag, Components::Transform,
                     Components::SpriteAnimator>(
              [&](ECS::Entity entity, Components::Tag &tag,
                  Components::Transform &t, Components::SpriteAnimator &anim) {
                if (tag.tag == "Player") {
                  t.position = resolvedSpawn;
                  anim.play("idle");
                  if (auto *col =
                          world.getComponent<Components::Collider>(entity))
                    col->isSolid = true;
                }
              });
        });
  });

  // Boot to a skippable intro cutscene before the menu.
  engine.setBootstrap(
      [](Core::Engine &e) -> std::shared_ptr<Runtime::GameState> {
        return std::make_shared<Runtime::IntroCutsceneState>(
            e.gsm(), e.input(), e.window(), e.audio());
      });

  return engine.Run();
}
