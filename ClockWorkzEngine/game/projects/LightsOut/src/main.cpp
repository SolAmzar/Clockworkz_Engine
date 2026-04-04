/******************************************************************************/
/*!
\file           main.cpp
\project        Lights Out
\author(s)      [Syafiq] 79% [Amzar] 21%
\brief          Entry point for CWZEditor.exe ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â editor
executable. Boots to EditorHubState. All game logic lives in Lua scripts.

All content (c) 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Assets/FontManager.hpp"
#include "Assets/ResourceManager.hpp"
#include "Assets/SoundManager.hpp"
#include "Assets/TextureManager.hpp"
#include "Core/Engine.hpp"
#include "Core/Profiler.hpp"
#include "ECS/World.hpp"
#include "Gameplay/Prefab.hpp"
#include "Graphics/Shader.hpp"
#include "Runtime/EditorHubState.hpp"
#include "Runtime/EditorPlayState.hpp"
#include "Runtime/EditorState.hpp"
#include "Runtime/SceneSerializer.hpp"
#include "Scripting/GameLuaBindings.hpp"
#include "Scripting/ScriptPropertyReader.hpp"

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
#include "Components/ParticleEmitter.hpp"
#include "Gameplay/Components/SurfaceComponent.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Runtime/LevelMusicManager.hpp"
#include "Runtime/Scene.hpp"
#include "Systems/CollisionSystem.hpp"
#include "Systems/LightRenderSystem.hpp"
#include <iostream>
#include <memory>

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <cstdlib>

#ifndef DBG_NEW
#define DBG_NEW new (_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DBG_NEW
#endif
#endif

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
  std::cout << "[Main] Booting ClockWorkz Engine...\n";
#ifdef _DEBUG
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF);
  _CrtMemState baseline;
  _CrtMemCheckpoint(&baseline);
#endif

  int _result;
  {
    Core::Engine engine;

    engine.setComponentRegistrar([](ECS::World &world) {
      world.registerComponent<Gameplay::SurfaceComponent>();
      world.registerComponent<Components::FootstepAudio>();
    });

    engine.setLuaExtensionRegistrar(Scripting::RegisterGameBindings);

    Runtime::SceneSerializer::setExtensions(
        [](ECS::Entity entity, ECS::World &world, void *rawComponents) {
          using json = nlohmann::json;
          auto &comps = *static_cast<json *>(rawComponents);
          if (auto *sc = world.getComponent<Gameplay::SurfaceComponent>(entity))
            comps["surface"] = {{"type", static_cast<int>(sc->type())}};
          if (auto *fa =
                  world.getComponent<Components::FootstepAudio>(entity)) {
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
            fa.woodPrefix = fac.value("woodPrefix", fa.woodPrefix);
            world.addComponent(entity, fa);
          }
        });

    engine.setSceneSetup([](Runtime::Scene &scene) {
      scene.setGameCallbacks(
          [](ECS::World &world, double dt, Systems::CollisionSystem &,
             Runtime::Scene &) {
            Audio::UpdateFootstepListener(world);
            Audio::UpdateFootstepAudio(world, dt);
          },
          [](ECS::World &world, Graphics::RenderingManager &renderer) {
            Systems::renderLightPass(world, renderer);
          },
          [](ECS::World &world, const vec2 &spawn, Runtime::Scene &scene) {
            const vec2 resolvedSpawn = resolvePlayerSpawn(world, spawn);
            world.each<Components::Tag, Components::Transform>(
                [&](ECS::Entity, Components::Tag &tag,
                    Components::Transform &t) {
                  if (tag.tag == "Player")
                    t.position = resolvedSpawn;
                });
            Runtime::LevelMusicManager::ApplySceneMusic(
                scene, Core::Engine::Get().audio());
          },
          [](ECS::World &world, const vec2 &spawn) {
            const vec2 resolvedSpawn = resolvePlayerSpawn(world, spawn);
            world.each<Components::Tag, Components::Transform,
                       Components::SpriteAnimator>(
                [&](ECS::Entity entity, Components::Tag &tag,
                    Components::Transform &t,
                    Components::SpriteAnimator &anim) {
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

    engine.setEditorFactory([&engine](const std::string &scenePath)
                                -> std::shared_ptr<Runtime::GameState> {
      return std::make_shared<Runtime::EditorState>(engine.input(),
                                                    engine.gsm(), scenePath);
    });
    engine.setPlayFactory([&engine](const std::string &scenePath)
                              -> std::shared_ptr<Runtime::GameState> {
      return std::make_shared<Runtime::EditorPlayState>(
          engine.input(), engine.gsm(), scenePath);
    });
    engine.setBootstrap(
        [](Core::Engine &e) -> std::shared_ptr<Runtime::GameState> {
          return std::make_shared<Runtime::EditorHubState>(e.gsm(), e.input());
        });

    _result = engine.Run();

    Assets::ResourceManager<Assets::FontManager>::Clear();
    Assets::ResourceManager<Graphics::Shader>::Clear();
    Assets::TextureManager::Clear();
    Assets::SoundManager::Clear();
    Runtime::SceneSerializer::setExtensions(nullptr, nullptr);
    Gameplay::PrefabFactory::get().clear();
    Scripting::ScriptPropertyReader::clearAll();

  } // engine destroyed here
#ifdef _DEBUG
  Core::Profiler::Destroy();
  Core::Engine::captureLeakCheckpoint();
  _CrtMemState after, diff;
  _CrtMemCheckpoint(&after);
  const auto *snap =
      static_cast<const _CrtMemState *>(Core::Engine::postLuaInitState());
  const _CrtMemState *useSnap = snap ? snap : &baseline;
  const char *snapLabel = snap ? "post-engine" : "baseline";
  if (_CrtMemDifference(&diff, useSnap, &after)) {
    std::cout << "[Memory] Leaks detected (relative to " << snapLabel
              << " snapshot):\n";
    _CrtMemDumpAllObjectsSince(useSnap);
  } else {
    std::cout << "[Memory] No leaks detected.\n";
  }
#endif
  return _result;
}
