/******************************************************************************/
/*!
\file           main.cpp
\project        Lights Out
\author(s)      [Lebon], [Syafiq]
\brief          Entry point for CWZEditor.exe — editor executable.
                Boots to EditorHubState. All game logic lives in Lua scripts.

All content (c) 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "Assets/FontManager.hpp"
#include "Assets/ResourceManager.hpp"
#include "Core/Engine.hpp"
#include "Core/Profiler.hpp"
#include "Assets/SoundManager.hpp"
#include "Assets/TextureManager.hpp"
#include "Graphics/Shader.hpp"
#include "Runtime/EditorHubState.hpp"
#include "Runtime/EditorState.hpp"
#include "Runtime/EditorPlayState.hpp"
#include "Runtime/SceneSerializer.hpp"
#include "Gameplay/Prefab.hpp"
#include "Scripting/ScriptPropertyReader.hpp"
#include "Scripting/GameLuaBindings.hpp"
#include "ECS/World.hpp"
#pragma warning(push, 0)
#include "Core/json.hpp"
#pragma warning(pop)
#include "Runtime/Scene.hpp"
#include "Systems/CollisionSystem.hpp"
#include "Components/Collider.hpp"
#include "Components/SpriteAnimator.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/Transform.hpp"
#include "Components/Tag.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Systems/LightRenderSystem.hpp"
#include "Gameplay/Components/ParticleEmitter.hpp"
#include "Gameplay/Components/SurfaceComponent.hpp"
#include "Components/FootstepAudio.hpp"
#include "Audio/FootstepAudioRuntime.hpp"
#include <memory>
#include <iostream>
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>
#ifndef DBG_NEW
#define DBG_NEW new (_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DBG_NEW
#endif
#endif

int main()
{
    std::cout << "[Main] Booting ClockWorkz Engine...\n";
#ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF);
    _CrtMemState baseline;
    _CrtMemCheckpoint(&baseline);
#endif

    int _result;
    {
    Core::Engine engine;

    engine.setComponentRegistrar([](ECS::World& world) {
        world.registerComponent<Components::ParticleEmitter>();
        world.registerComponent<Gameplay::SurfaceComponent>();
        world.registerComponent<Components::FootstepAudio>();
    });

    engine.setLuaExtensionRegistrar(Scripting::RegisterGameBindings);

    Runtime::SceneSerializer::setExtensions(
        [](ECS::Entity entity, ECS::World& world, void* rawComponents) {
            using json = nlohmann::json;
            auto& comps = *static_cast<json*>(rawComponents);
            if (auto* pe = world.getComponent<Components::ParticleEmitter>(entity)) {
                auto& sys = pe->system();
                comps["particleEmitter"] = {
                    {"active", pe->active()}, {"rate", sys.rate},
                    {"lifeMin", sys.lifeMin}, {"lifeMax", sys.lifeMax},
                    {"speedMin", sys.speedMin}, {"speedMax", sys.speedMax},
                    {"sizeStart", sys.sizeStart}, {"sizeEnd", sys.sizeEnd},
                    {"colorStart", {{"r",sys.colorStart.r},{"g",sys.colorStart.g},{"b",sys.colorStart.b},{"a",sys.colorStart.a}}},
                    {"colorEnd",   {{"r",sys.colorEnd.r},  {"g",sys.colorEnd.g},  {"b",sys.colorEnd.b},  {"a",sys.colorEnd.a}}},
                    {"spreadDeg", sys.spreadDeg},
                    {"direction", {{"x", sys.dir.x}, {"y", sys.dir.y}}},
                    {"acceleration", {{"x", sys.acceleration.x}, {"y", sys.acceleration.y}}},
                    {"looping", sys.looping}, {"worldSpace", sys.worldSpace},
                    {"localOffset", {{"x", pe->localOffset.x}, {"y", pe->localOffset.y}}}};
            }
            if (auto* sc = world.getComponent<Gameplay::SurfaceComponent>(entity))
                comps["surface"] = {{"type", static_cast<int>(sc->type())}};
            if (auto* fa = world.getComponent<Components::FootstepAudio>(entity)) {
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
        [](ECS::Entity entity, ECS::World& world, const void* rawComponents) {
            using json = nlohmann::json;
            const auto& c = *static_cast<const json*>(rawComponents);
            if (c.contains("particleEmitter")) {
                const auto& pec = c["particleEmitter"];
                Components::ParticleEmitter pe;
                pe.setActive(pec.value("active", true));
                pe.localOffset.x = pec.contains("localOffset") ? pec["localOffset"].value("x", 0.f) : 0.f;
                pe.localOffset.y = pec.contains("localOffset") ? pec["localOffset"].value("y", 0.f) : 0.f;
                auto& sys = pe.system();
                sys.rate = pec.value("rate", 200.f); sys.lifeMin = pec.value("lifeMin", 0.5f);
                sys.lifeMax = pec.value("lifeMax", 1.2f); sys.speedMin = pec.value("speedMin", 20.f);
                sys.speedMax = pec.value("speedMax", 120.f); sys.sizeStart = pec.value("sizeStart", 2.25f);
                sys.sizeEnd = pec.value("sizeEnd", 0.f); sys.spreadDeg = pec.value("spreadDeg", 45.f);
                sys.looping = pec.value("looping", true); sys.worldSpace = pec.value("worldSpace", true);
                if (pec.contains("colorStart")) { sys.colorStart.r = pec["colorStart"].value("r",1.f); sys.colorStart.g = pec["colorStart"].value("g",1.f); sys.colorStart.b = pec["colorStart"].value("b",1.f); sys.colorStart.a = pec["colorStart"].value("a",1.f); }
                if (pec.contains("colorEnd"))   { sys.colorEnd.r   = pec["colorEnd"].value("r",1.f);   sys.colorEnd.g   = pec["colorEnd"].value("g",1.f);   sys.colorEnd.b   = pec["colorEnd"].value("b",1.f);   sys.colorEnd.a   = pec["colorEnd"].value("a",0.f); }
                if (pec.contains("direction"))    { sys.dir.x = pec["direction"].value("x",1.f);    sys.dir.y = pec["direction"].value("y",0.f); }
                if (pec.contains("acceleration")) { sys.acceleration.x = pec["acceleration"].value("x",0.f); sys.acceleration.y = pec["acceleration"].value("y",-200.f); }
                world.addComponent(entity, pe);
            }
            if (c.contains("surface")) {
                Gameplay::SurfaceComponent sc;
                sc.setType(static_cast<Gameplay::SurfaceType>(c["surface"].value("type", 0)));
                world.addComponent(entity, sc);
                if (!world.getComponent<Components::Collider>(entity)) {
                    Components::Collider col;
                    if (auto* spr = world.getComponent<Components::SpriteRenderer>(entity)) col.size = spr->size;
                    col.isSolid = false; col.isTrigger = true;
                    world.addComponent(entity, col);
                }
            }
            if (c.contains("footstepAudio")) {
                const auto& fac = c["footstepAudio"];
                Components::FootstepAudio fa;
                fa.interval = fac.value("interval", fa.interval);
                fa.sprintIntervalScale = fac.value("sprintIntervalScale", fa.sprintIntervalScale);
                fa.sprintSpeedThreshold = fac.value("sprintSpeedThreshold", fa.sprintSpeedThreshold);
                fa.minSpeed = fac.value("minSpeed", fa.minSpeed);
                fa.resetDelay = fac.value("resetDelay", fa.resetDelay);
                fa.volume = fac.value("volume", fa.volume);
                fa.variationCount = fac.value("variationCount", fa.variationCount);
                fa.useSurfaceSounds = fac.value("useSurfaceSounds", fa.useSurfaceSounds);
                fa.soundPrefix = fac.value("soundPrefix", fa.soundPrefix);
                fa.concretePrefix = fac.value("concretePrefix", fa.concretePrefix);
                fa.tilePrefix = fac.value("tilePrefix", fa.tilePrefix);
                fa.woodPrefix = fac.value("woodPrefix", fa.woodPrefix);
                world.addComponent(entity, fa);
            }
        }
    );

    engine.setSceneSetup([](Runtime::Scene& scene) {
        scene.setGameCallbacks(
            [](ECS::World& world, double dt, Systems::CollisionSystem&, Runtime::Scene&) {
                world.each<Components::Transform, Components::ParticleEmitter>(
                    [dt](ECS::Entity, Components::Transform& t, Components::ParticleEmitter& pe) {
                        if (pe.active()) {
                            ps::v2 pos{t.position.x, t.position.y};
                            pe.update(pos, dt);
                        }
                    });
                Audio::UpdateFootstepListener(world);
                Audio::UpdateFootstepAudio(world, dt);
            },
            [](ECS::World& world, Graphics::RenderingManager& renderer) {
                world.each<Components::ParticleEmitter>(
                    [&renderer](ECS::Entity, Components::ParticleEmitter& pe) {
                        if (pe.active()) {
                            pe.setRenderer(&renderer);
                            pe.system().render(renderer);
                        }
                    });
                Systems::renderLightPass(world, renderer);
            },
            [](ECS::World& world, const vec2& spawn) {
                world.each<Components::Tag, Components::Transform>(
                    [&](ECS::Entity, Components::Tag& tag, Components::Transform& t) {
                        if (tag.tag == "Player") t.position = spawn;
                    });
            },
            [](ECS::World& world, const vec2& spawn) {
                world.each<Components::Tag, Components::Transform, Components::SpriteAnimator>(
                    [&](ECS::Entity entity, Components::Tag& tag,
                        Components::Transform& t, Components::SpriteAnimator& anim) {
                        if (tag.tag == "Player") {
                            t.position = spawn;
                            anim.play("idle");
                            if (auto* col = world.getComponent<Components::Collider>(entity))
                                col->isSolid = true;
                        }
                    });
            }
        );
    });

    engine.setEditorFactory([&engine](const std::string& scenePath) -> std::shared_ptr<Runtime::GameState> {
        return std::make_shared<Runtime::EditorState>(engine.input(), engine.gsm(), scenePath);
    });
    engine.setPlayFactory([&engine](const std::string& scenePath) -> std::shared_ptr<Runtime::GameState> {
        return std::make_shared<Runtime::EditorPlayState>(engine.input(), engine.gsm(), scenePath);
    });
    engine.setBootstrap([](Core::Engine& e) -> std::shared_ptr<Runtime::GameState> {
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
    const auto* snap = static_cast<const _CrtMemState*>(Core::Engine::postLuaInitState());
    const _CrtMemState* useSnap = snap ? snap : &baseline;
    const char* snapLabel = snap ? "post-engine" : "baseline";
    if (_CrtMemDifference(&diff, useSnap, &after)) {
        std::cout << "[Memory] Leaks detected (relative to " << snapLabel << " snapshot):\n";
        _CrtMemDumpAllObjectsSince(useSnap);
    } else {
        std::cout << "[Memory] No leaks detected.\n";
    }
#endif
    return _result;
}






