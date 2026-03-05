#include "Core/Engine.hpp"
#include "Runtime/EditorHubState.hpp"
#include "Runtime/EditorState.hpp"
#include "Runtime/EditorPlayState.hpp"
#include "Runtime/SceneSerializer.hpp"
#include "ECS/World.hpp"
#pragma warning(push, 0)
#include "Core/json.hpp"
#pragma warning(pop)
// Game-specific components
// Engine types needed by scene setup callbacks
#include "Runtime/Scene.hpp"
#include "Systems/CollisionSystem.hpp"
#include "Components/Collider.hpp"
#include "Components/SpriteAnimator.hpp"
#include "Components/Transform.hpp"
#include "Graphics/RenderingManager.hpp"
// Game-specific components
#include "Components/ActivationZone.hpp"
#include "Components/Battery.hpp"
#include "Components/Door.hpp"
#include "Components/DoorSwing.hpp"
#include "Components/EnemyAI.hpp"
#include "Components/Fragment.hpp"
#include "Components/FragmentCollector.hpp"
#include "Components/Health.hpp"
#include "Components/LightExposure.hpp"
#include "Components/PlayerController.hpp"
#include "Components/Portal.hpp"
#include "Components/Torch.hpp"
#include "Gameplay/Components/ParticleEmitter.hpp"
#include "Gameplay/Components/SurfaceComponent.hpp"
#include "Systems/PlayerSystem.hpp"
#include "Systems/AISystem.hpp"
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
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	Core::Engine engine;

	// Game component registrar: called by every Scene::Init() so game-specific
	// components are available in all scenes (editor, play, sandbox, etc.).
	engine.setComponentRegistrar([](ECS::World& world) {
		world.registerComponent<Components::Health>();
		world.registerComponent<Components::Battery>();
		world.registerComponent<Components::LightExposure>();
		world.registerComponent<Components::Fragment>();
		world.registerComponent<Components::EnemyAI>();
		world.registerComponent<Components::PlayerController>();
		world.registerComponent<Components::FragmentCollector>();
		world.registerComponent<Components::TorchComponent>();
		world.registerComponent<Components::Door>();
		world.registerComponent<Components::DoorSwing>();
		world.registerComponent<Components::ActivationZone>();
		world.registerComponent<Components::Portal>();
		world.registerComponent<Components::ParticleEmitter>();
		world.registerComponent<Gameplay::SurfaceComponent>();
	});

	// Scene serializer extensions: teach the engine serializer how to
	// read/write game-specific components without the engine knowing about them.
	Runtime::SceneSerializer::setExtensions(
		// Serialize: write game components into the per-entity "components" JSON object
		[](ECS::Entity entity, ECS::World& world, void* rawComponents) {
			using json = nlohmann::json;
			auto& comps = *static_cast<json*>(rawComponents);

			if (auto* h = world.getComponent<Components::Health>(entity))
				comps["health"] = {{"hp", h->hp}, {"maxHp", h->maxHp},
				                   {"regenRate", h->regenRate}, {"invulnerable", h->invulnerable},
				                   {"isDead", h->isDead}};

			if (auto* b = world.getComponent<Components::Battery>(entity))
				comps["battery"] = {{"charge", b->charge}, {"maxCharge", b->maxCharge},
				                    {"isCharging", b->isCharging}};

			if (auto* le = world.getComponent<Components::LightExposure>(entity))
				comps["lightExposure"] = {{"exposureTime", le->exposureTime},
				                          {"maxExposureTime", le->maxExposureTime},
				                          {"inLight", le->inLight}, {"isDangerous", le->isDangerous}};

			if (auto* fr = world.getComponent<Components::Fragment>(entity))
				comps["fragment"] = {{"fragmentId", fr->fragmentId}, {"collected", fr->collected}};

			if (auto* fc = world.getComponent<Components::FragmentCollector>(entity))
				comps["fragmentCollector"] = {{"requiredFragments", fc->requiredFragments},
				                              {"collectedCount", fc->collectedCount},
				                              {"pickupRadius", fc->pickupRadius}};

			if (auto* ai = world.getComponent<Components::EnemyAI>(entity))
				comps["enemyAI"] = {{"detectionRadius", ai->detectionRadius},
				                    {"chaseSpeed", ai->chaseSpeed}, {"patrolSpeed", ai->patrolSpeed}};

			if (auto* pc = world.getComponent<Components::PlayerController>(entity))
				comps["playerController"] = {{"walkMultiplier", pc->walkMultiplier},
				                             {"sprintMultiplier", pc->sprintMultiplier}};

			if (auto* t = world.getComponent<Components::TorchComponent>(entity))
				comps["torch"] = {{"active", t->active},
				                  {"offset", {{"x", t->offset.x}, {"y", t->offset.y}}},
				                  {"color", {{"r", t->color.r}, {"g", t->color.g}, {"b", t->color.b}}},
				                  {"radius", t->radius}, {"angle", t->angle}, {"intensity", t->intensity}};

			if (auto* d = world.getComponent<Components::Door>(entity))
				comps["door"] = {{"requiredFragments", d->requiredFragments}, {"isOpen", d->isOpen},
				                 {"openTexture", d->openTexture}, {"closedTexture", d->closedTexture}};

			if (auto* ds = world.getComponent<Components::DoorSwing>(entity))
				comps["doorSwing"] = {{"isOpen", ds->isOpen}, {"swingAngle", ds->swingAngle},
				                      {"clockwise", ds->clockwise}, {"speed", ds->speed},
				                      {"initialRotation", ds->initialRotation},
				                      {"initialized", ds->initialized}};

			if (auto* az = world.getComponent<Components::ActivationZone>(entity))
				comps["activationZone"] = {{"active", az->active},
				                           {"actionType", (int)az->actionType},
				                           {"payload", az->payload}, {"oneShot", az->oneShot},
				                           {"triggered", az->triggered}};

			if (auto* po = world.getComponent<Components::Portal>(entity))
				comps["portal"] = {{"levelName", po->levelName}};

			if (auto* sc = world.getComponent<Gameplay::SurfaceComponent>(entity))
				comps["surface"] = {{"type", static_cast<int>(sc->type())}};

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
		},
		// Deserialize: read game components from the per-entity "components" JSON object
		[](ECS::Entity entity, ECS::World& world, const void* rawComponents) {
			using json = nlohmann::json;
			const auto& c = *static_cast<const json*>(rawComponents);

			if (c.contains("health")) {
				Components::Health h;
				h.hp = c["health"].value("hp", 10.f); h.maxHp = c["health"].value("maxHp", 10.f);
				h.regenRate = c["health"].value("regenRate", 0.f);
				h.invulnerable = c["health"].value("invulnerable", false);
				h.isDead = c["health"].value("isDead", false);
				world.addComponent(entity, h);
			}
			if (c.contains("battery")) {
				Components::Battery b;
				b.charge = c["battery"].value("charge", 100.f);
				b.maxCharge = c["battery"].value("maxCharge", 100.f);
				b.isCharging = c["battery"].value("isCharging", false);
				world.addComponent(entity, b);
			}
			if (c.contains("lightExposure")) {
				Components::LightExposure le;
				le.exposureTime = c["lightExposure"].value("exposureTime", 0.f);
				le.maxExposureTime = c["lightExposure"].value("maxExposureTime", 10.f);
				le.inLight = c["lightExposure"].value("inLight", false);
				le.isDangerous = c["lightExposure"].value("isDangerous", false);
				world.addComponent(entity, le);
			}
			if (c.contains("fragment")) {
				Components::Fragment fr;
				if (c["fragment"]["fragmentId"].is_string()) {
					std::string fid = c["fragment"]["fragmentId"];
					auto pos = fid.find("_");
					try { fr.fragmentId = pos != std::string::npos ? std::stoi(fid.substr(pos + 1)) : 0; }
					catch (...) { fr.fragmentId = 0; }
				} else { fr.fragmentId = c["fragment"].value("fragmentId", 0); }
				fr.collected = c["fragment"].value("collected", false);
				world.addComponent(entity, fr);
			}
			if (c.contains("fragmentCollector")) {
				Components::FragmentCollector fc;
				fc.requiredFragments = c["fragmentCollector"].value("requiredFragments", 1);
				fc.collectedCount = c["fragmentCollector"].value("collectedCount", 0);
				fc.pickupRadius = c["fragmentCollector"].value("pickupRadius", 50.f);
				world.addComponent(entity, fc);
			}
			if (c.contains("enemyAI")) {
				Components::EnemyAI ai;
				ai.detectionRadius = c["enemyAI"].value("detectionRadius", 300.f);
				ai.chaseSpeed = c["enemyAI"].value("chaseSpeed", 100.f);
				ai.patrolSpeed = c["enemyAI"].value("patrolSpeed", 50.f);
				world.addComponent(entity, ai);
			}
			if (c.contains("playerController")) {
				Components::PlayerController pc;
				pc.walkMultiplier = c["playerController"].value("walkMultiplier", 1.0f);
				pc.sprintMultiplier = c["playerController"].value("sprintMultiplier", 2.0f);
				world.addComponent(entity, pc);
			}
			if (c.contains("torch")) {
				Components::TorchComponent t;
				t.active = c["torch"].value("active", true);
				if (c["torch"].contains("offset")) { t.offset.x = c["torch"]["offset"].value("x", 35.f); t.offset.y = c["torch"]["offset"].value("y", -20.f); }
				if (c["torch"].contains("color"))  { t.color.r  = c["torch"]["color"].value("r", 1.f);   t.color.g  = c["torch"]["color"].value("g", 0.9f);  t.color.b  = c["torch"]["color"].value("b", 0.6f); }
				t.radius = c["torch"].value("radius", 300.f); t.angle = c["torch"].value("angle", 60.f);
				t.intensity = c["torch"].value("intensity", 1.f);
				world.addComponent(entity, t);
			}
			if (c.contains("door")) {
				Components::Door d;
				d.requiredFragments = c["door"].value("requiredFragments", 1);
				d.isOpen = c["door"].value("isOpen", false);
				d.openTexture = c["door"].value("openTexture", "door_open");
				d.closedTexture = c["door"].value("closedTexture", "door_close");
				world.addComponent(entity, d);
			}
			if (c.contains("doorSwing")) {
				Components::DoorSwing ds;
				ds.isOpen = c["doorSwing"].value("isOpen", false);
				ds.swingAngle = c["doorSwing"].value("swingAngle", 90.f);
				ds.clockwise = c["doorSwing"].value("clockwise", true);
				ds.speed = c["doorSwing"].value("speed", 90.f);
				ds.initialRotation = c["doorSwing"].value("initialRotation", 0.f);
				ds.initialized = c["doorSwing"].value("initialized", false);
				world.addComponent(entity, ds);
			}
			if (c.contains("activationZone")) {
				Components::ActivationZone az;
				az.active = c["activationZone"].value("active", true);
				az.actionType = (Components::ActivationZone::ActionType)c["activationZone"].value("actionType", 0);
				az.payload = c["activationZone"].value("payload", "");
				az.oneShot = c["activationZone"].value("oneShot", false);
				az.triggered = c["activationZone"].value("triggered", false);
				world.addComponent(entity, az);
			}
			if (c.contains("portal")) {
				Components::Portal po;
				po.levelName = c["portal"].value("levelName", "");
				world.addComponent(entity, po);
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
		}
	);

	// Scene setup: called after Scene::Init() to attach game-specific callbacks
	// (particle update/render, player spawn reset, death/portal/respawn logic).
	engine.setSceneSetup([](Runtime::Scene& scene) {
		scene.setGameCallbacks(
			// onUpdate: particles + collision death + portal transitions
			[](ECS::World& world, double dt, Systems::CollisionSystem& cols, Runtime::Scene& scene) {
				// PlayerSystem: input, animation, torch, battery, footsteps
				static Systems::PlayerSystem playerSystem;
				playerSystem.update(world, dt, Core::Engine::Get().input(), nullptr);

				// AISystem: enemy state machine (uses enemy_ai.lua internally)
				static Systems::AISystem aiSystem;
				aiSystem.update(world, dt, nullptr);

				// Particle emitter update
				world.each<Components::Transform, Components::ParticleEmitter>(
					[dt](ECS::Entity, Components::Transform& t, Components::ParticleEmitter& pe) {
						if (pe.active()) {
							ps::v2 pos{t.position.x, t.position.y};
							pe.update(pos, dt);
						}
					});

				// Player vs Enemy collision → death
				auto& collisions = cols.getCollisions();
				for (const auto& pair : collisions) {
					ECS::Entity eA = pair.first, eB = pair.second;
					if (!world.isValid(eA) || !world.isValid(eB)) continue;
					bool playerA = world.hasComponent<Components::PlayerController>(eA);
					bool playerB = world.hasComponent<Components::PlayerController>(eB);
					bool enemyA  = world.hasComponent<Components::EnemyAI>(eA);
					bool enemyB  = world.hasComponent<Components::EnemyAI>(eB);
					if ((playerA && enemyB) || (playerB && enemyA)) {
						ECS::Entity pe2 = playerA ? eA : eB;
						auto* pAnim = world.getComponent<Components::SpriteAnimator>(pe2);
						if (pAnim && pAnim->currentClip == "death") continue;
						std::cout << "[Scene] Player hit by enemy!\n";
						if (pAnim) pAnim->play("death");
						if (auto* col = world.getComponent<Components::Collider>(pe2))
							col->isSolid = false;
					}
				}

				// Portal collision → set pending level via Tag lookup or Portal component
				for (const auto& pair : collisions) {
					ECS::Entity eA = pair.first, eB = pair.second;
					if (!world.isValid(eA) || !world.isValid(eB)) continue;
					bool portalA = world.hasComponent<Components::Portal>(eA);
					bool portalB = world.hasComponent<Components::Portal>(eB);
					bool playerA = world.hasComponent<Components::PlayerController>(eA);
					bool playerB = world.hasComponent<Components::PlayerController>(eB);
					ECS::Entity portalEnt = {0, 0};
					if (playerA && portalB) portalEnt = eB;
					else if (playerB && portalA) portalEnt = eA;
					if (portalEnt.isValid()) {
						auto* portal = world.getComponent<Components::Portal>(portalEnt);
						if (portal && !portal->levelName.empty()) {
							std::cout << "[Scene] Transitioning to level: " << portal->levelName << "\n";
							scene.SetPendingLevel(portal->levelName);
							break;
						}
					}
				}
			},
			// onRender: particle emitters
			[](ECS::World& world, Graphics::RenderingManager& renderer) {
				world.each<Components::ParticleEmitter>(
					[&renderer](ECS::Entity, Components::ParticleEmitter& pe) {
						if (pe.active()) {
							pe.setRenderer(&renderer);
							pe.system().render(renderer);
						}
					});
			},
			// onLoad: reset player to spawn point
			[](ECS::World& world, const vec2& spawn) {
				bool found = false;
				world.each<Components::PlayerController, Components::Transform>(
					[&](ECS::Entity, Components::PlayerController&, Components::Transform& t) {
						t.position = spawn;
						found = true;
					});
				if (!found)
					std::cout << "[Scene] Warning: No Player found to reset!\n";
				else
					std::cout << "[Scene] Player reset to spawn: (" << spawn.x << ", " << spawn.y << ")\n";
			},
			// onRespawn: restore player state
			[](ECS::World& world, const vec2& spawn) {
				world.each<Components::PlayerController, Components::Transform,
				            Components::SpriteAnimator>(
					[&](ECS::Entity entity, Components::PlayerController&,
					    Components::Transform& t, Components::SpriteAnimator& anim) {
						t.position = spawn;
						anim.play("idle");
						if (auto* col = world.getComponent<Components::Collider>(entity))
							col->isSolid = true;
					});
			}
		);
	});

	// Editor factory: called by EditorHubState when opening a project.
	engine.setEditorFactory([&engine](const std::string& scenePath) -> std::shared_ptr<Runtime::GameState> {
		return std::make_shared<Runtime::EditorState>(engine.input(), engine.gsm(), scenePath);
	});

	// Play factory: called by EditorState when Play is pressed.
	// Creates EditorPlayState — runs the scene with all systems active.
	// Stop button returns to EditorState via Engine::editorFactory().
	engine.setPlayFactory([&engine](const std::string& scenePath) -> std::shared_ptr<Runtime::GameState> {
		return std::make_shared<Runtime::EditorPlayState>(engine.input(), engine.gsm(), scenePath);
	});

	// Engine always boots to the EditorHubState (Unity-style project launcher).
	engine.setBootstrap([](Core::Engine& e) -> std::shared_ptr<Runtime::GameState> {
		return std::make_shared<Runtime::EditorHubState>(e.gsm(), e.input());
	});

	return engine.Run();
}
