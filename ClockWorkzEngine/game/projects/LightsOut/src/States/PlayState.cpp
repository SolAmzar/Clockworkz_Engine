#if 1
/******************************************************************************/
/*!
\file           PlayState.cpp
\project        Lights Out
\author(s)      [Lebon], 70% [Syafiq] 20%
\brief          Declares PlayState, the main gameplay loop state.
                                Responsibilities include:
                                - Loading and managing the current Scene and its
GameObjects
                                - Updating gameplay logic such as movement,
collisions, and AI
                                - Rendering the game world each frame (Game
Object Render not done yet)
                                - Transitioning back to other states (e.g.,
pause or menu)

All content � 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "States/PlayState.hpp"
#include "Assets/FontManager.hpp"
#include "Assets/ResourceManager.hpp"
#include "Assets/SoundManager.hpp"
#include "Audio/AudioManager.hpp"
#include "Audio/SurfaceAudioSystem.hpp"
#include "Components/Battery.hpp"
#include "Components/EnemyAI.hpp"
#include "Components/LightExposure.hpp"
#include "Components/Light.hpp"
#include "Components/Transform.hpp"
#include "Components/Velocity.hpp"
#include "Core/Profiler.hpp"
#include "Gameplay/Components/Fragment.hpp"
#include "Gameplay/Components/ParticleEmitter.hpp"
#include "Gameplay/Components/SurfaceComponent.hpp"
#include "Gameplay/Components/chase.hpp"
#include "Gameplay/GameObject.hpp"
#include "Graphics/ParticleSystem.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Shader.hpp"
#include "IMGUI/imgui.h"
#include "Platform/InputManager.hpp"
#include "Platform/LevelEditor.hpp"
#include "Platform/WindowManager.hpp"
#include "States/GameOverState.hpp"
#include "Runtime/GameStateManager.hpp"
#include "States/MenuState.hpp"
#include "States/PauseState.hpp"
#include "Runtime/Scene.hpp"
#include "Runtime/SceneManager.hpp"
#include "Runtime/SceneSerializer.hpp"
#include <Assets/TextureManager.hpp>
#include <Components/Collider.hpp>
#include <Components/Tag.hpp>
#include <Core/Engine.hpp>
#include <Core/Time.hpp>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <Gameplay/Components/SpriteAnimator.hpp>
#include <Gameplay/Components/SpriteRenderer.hpp>
#include <Gameplay/Prefab.hpp>
#include <Scripting/LuaScriptComponent.hpp>
#include <Gameplay/WallTiles.hpp>
#include <Graphics/Camera2D.hpp>
#include <Graphics/GraphicsManager.hpp>
#include <Graphics/Renderer2D.hpp>
#include <Math/VecCompatGLM.hpp>
#include <Platform/LevelEditorState.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace Graphics;
using namespace Assets;

namespace {
// Shared RNG for all footsteps
std::mt19937 &FootstepRng() {
  static std::mt19937 rng{std::random_device{}()};
  return rng;
}

template <std::size_t N>
const char *pickRandom(const std::array<const char *, N> &ids) {
  std::uniform_int_distribution<std::size_t> dist(0, N - 1);
  return ids[dist(FootstepRng())];
}
} // namespace

namespace Runtime {
static Gameplay::GameObject *findFirstPlayer(Runtime::Scene *scene) {
  if (!scene)
    return nullptr;
  for (auto &u : scene->objects()) {
    if (u && u->type() == Gameplay::GameObjectType::Player)
      return u.get();
  }
  return nullptr;
}

Gameplay::SurfaceType PlayState::getSurfaceUnder(Gameplay::GameObject *player) {
  auto *scene = m_scenes.current();
  if (!scene)
    return Gameplay::SurfaceType::Default;

  vec2 pos = player->transform().position;
  for (auto &obj : scene->objects()) {
    if (!obj || !obj->active())
      continue;
    if (auto *surf = obj->getComponent<Gameplay::SurfaceComponent>()) {
      vec2 half = obj->size() * 0.5f;
      if (abs(pos.x - obj->transform().position.x) <= half.x &&
          abs(pos.y - obj->transform().position.y) <= half.y) {
        return surf->type();
      }
    }
  }
  return Gameplay::SurfaceType::Default;
}

void PlayState::rebindLightsAndPlayerAfterReload() {
  auto *scene = m_scenes.current();
  // 1) Rebind player pointer to the NEW player object created by reload
  m_player = findFirstPlayer(scene);
}

PlayState::PlayState(std::string initialSceneId, Platform::InputManager &input,
                     GameStateManager &gsm)
    : m_initialSceneId(std::move(initialSceneId)), m_input(input), m_gsm(gsm),
      m_player(nullptr), m_levelEditor(nullptr), m_editorInitialized(false),
      currentBattery(100.0f), maxBattery(100.0f) {}

PlayState::~PlayState() {}
const char *PlayState::name() const { return "PlayState"; }

const char *PlayState::toString(Gameplay::GameObjectType t) {
  using T = Gameplay::GameObjectType;
  switch (t) {
  case T::Floor:
    return "Floor";
  case T::Wall:
    return "Wall";
  case T::Player:
    return "Player";
  case T::Enemy:
    return "Enemy";
  case T::Trap:
    return "Trap";
  case T::Ball:
    return "Ball";
  case T::Fragment:
    return "Fragment";
  default:
    return "Unknown";
  }
}

void PlayState::onEnter() {
  std::cout << "[PlayState] Enter, loading scene: " << m_initialSceneId << "\n";
  // Create an empty game object you can see in the scene

  // 1) Make the GO and keep a raw pointer before moving it into the vector
  auto fxUP = std::make_unique<Gameplay::GameObject>(
      Gameplay::GameObjectType::Wall, "ParticleTest");
  Gameplay::GameObject *fx = fxUP.get();

  fx->transform().position = {5.0f, 3.0f};
  // fx->setLayer(Gameplay::DrawLayer::FX);

  // 2) Transfer ownership
  if (auto *scene = m_scenes.current()) {
    scene->addObject(std::move(fxUP));
  }
  m_fx = fx;

  // 3) Construct the component in-place (NO new). Pass the ctor args.
  // ParticleEmitter(GameObject& owner, std::size_t max)
  auto *emitter = fx->addComponent<Components::ParticleEmitter>(1000);

  emitter->setRenderer(m_renderer);

  // 4) Configure
  auto &s = emitter->system();
  s.dir = normalize(vec2{1, 0});
  s.spreadDeg = 40.f;
  s.acceleration = {0.f, -50.f};
  s.sizeStart = 0.2f;
  s.sizeEnd = 0.0f;
  s.colorStart = {1.f, 0.6f, 0.1f, 1.f};
  s.colorEnd = {1.f, 0.1f, 0.0f, 0.f};
  s.rate = 120.f;

  // Initialize level editor
  Core::Engine &engine = Core::Engine::Get();

  //auto &audio = engine.audio(); 

  // play the gameplay BGM (make sure "level_bgm" is loaded via SoundManager)
  m_bgmChannel = Assets::SoundManager::Play("level_bgm");
  Assets::SoundManager::Play("level_ambience");

  m_levelEditor = std::make_unique<Platform::LevelEditor>();

  if (m_levelEditor->init(engine.window().handle(), engine.rendering(), m_input,
                          engine.window())) {

    // Create level editor and this will load
    // Scene 'level_01' loaded from : assets / scenes / EditorScene.json
    // Load current scene into editor
    if (auto *scene = m_scenes.current()) {
      m_levelEditor->loadSceneFromPlayState(scene);
    }

    m_levelEditor->setActive(false); // Start inactive
    m_editorInitialized = true;
    std::cout << "[PlayState] Level editor initialized\n";
  } else {
    std::cerr << "[PlayState] Failed to initialize level editor\n";
  }

  currentBattery = 100.0f;
  maxBattery = 100.0f;
  currentBatteryTexture = "battery_full";

  // Syafiq -
  m_renderer = &engine.rendering();

  auto textureShader = ResourceManager<Shader>::Load(
      "texture", "assets/shaders/texture.vert", "assets/shaders/texture.frag");

  ResourceManager<Shader>::Load("light", "assets/shaders/light.vert",
                                "assets/shaders/light.frag");
  if (m_scenes.loadScene(m_initialSceneId)) {
    m_loaded = true;

    // Attempt to load scene content from JSON if exists
    if (auto *scene = m_scenes.current()) {
      std::string filename = m_initialSceneId;
      // Check if path is relative or needs extension
      if (filename.find(".json") == std::string::npos) {
        filename += ".json";
      }
      std::string path = "assets/scenes/" + filename;

      if (std::filesystem::exists(path)) {
        std::cout << "[PlayState] Loading level file: " << path << "\n";
        SceneSerializer::Deserialize(*scene, path);
        scene->registerExistingObjectsToLayers();
      } else {
        std::cout << "[PlayState] Level file not found: " << path
                  << " (starting empty)\n";
      }
    }

    // Print all GameObjects created by the loaded scene
    if (auto *scene = m_scenes.current()) {
      auto &objs = scene->objects();
      std::cout << "GameObjects created (" << objs.size() << "):\n";

      for (auto &u : objs) {
        auto const &o = *u;
        auto p = o.transform().position;
        auto z = o.size();
        std::cout << " - id=" << o.id() << " type=" << toString(o.type())
                  << " name=\"" << o.name() << "\""
                  << " pos=(" << p.x << "," << p.y << ")"
                  << " size=(" << z.x << "," << z.y << ")"
                  << (o.active() ? "" : " [inactive]") << "\n";

        /*if (u->type() == Gameplay::GameObjectType::Player)
        {
                m_player = &(*u);

                Gameplay::SpriteRendererComponent* playerSprite2 =
        u->getComponent<Gameplay::SpriteRendererComponent>(); if (playerSprite2)
                {
                        playerSprite2->setUV(glm::vec4{ 0, 0.875, 0.125, 0.125
        });
                }
        }*/

        /*	if (u->type() == Gameplay::GameObjectType::Wall) {
                        auto* sprite =
           u->getComponent<Gameplay::SpriteRendererComponent>(); if (sprite) {
                                sprite->setUV(compat::to_glm(Gameplay::GetWallTileUV(Gameplay::WallTile::CornerTL)));
                        }

                }*/
      }

      for (auto &obj : scene->objects()) {
        if (obj && obj->type() == Gameplay::GameObjectType::Player) {
          m_player = obj.get();
          m_spawnPoint = scene->getSpawnPoint();
          m_player->transform().position = m_spawnPoint;
          break;
        }
      }

      initializeEnemyAI(scene);

      std::cout.flush();
    }
  } else {
    std::cout << "[PlayState] Scene load FAILED.\n";
    std::cout << "[PlayState] Returning to menu...\n";
    m_shouldReturnToMenu = true; // Returns to menu
  }

  Assets::ResourceManager<Assets::FontManager>::Load(
      "Subtitle", "assets/fonts/MANIC2.TTF", 100);

  auto textShader = ResourceManager<Shader>::Load(
      "text", "assets/shaders/text.vert", "assets/shaders/text.frag");

  Assets::ResourceManager<Assets::FontManager>::Load(
      "FragmentUI", "assets/fonts/MANIC1.TTF", 32);

  // Initialize fragment system
  m_showFragmentNotification = false;
  m_fragmentNotificationTimer = 0.0f;
  m_lastCollectedFragmentId = "";
  m_fragmentIconTexture = "fragment_texture";

  std::cout << "[PlayState] Fragment system initialized\n";
}

void PlayState::onExit() {
  // Shutdown level editor FIRST
  if (m_levelEditor && m_editorInitialized) {
    if (auto *scene = m_scenes.current()) {
      m_levelEditor->syncSceneBackToPlayState(scene);
    }
    m_levelEditor->shutdown();
    m_levelEditor.reset();
    m_editorInitialized = false;
  }

  // Then unload scene
  if (m_loaded) {
    m_scenes.unloadCurrent();
  }
  m_loaded = false;

  if (m_bgmPlaying) {
    auto &audio = Core::Engine::Get().audio();
    audio.fadeOutChannel(m_bgmChannel, 0.5f);
    m_bgmPlaying = false;
  }

  clearEnemyAI();
}

void PlayState::update(double dt) {
  // ESC = Exit editor (or pause if not in editor)
  if (m_input.wasKeyPressed(GLFW_KEY_Z) && m_levelEditor->isActive()) {
    if (m_levelEditor && m_levelEditor->isActive()) {
      // Close editor
      m_levelEditor->setActive(false);
      if (auto *scene = m_scenes.current()) {
        m_levelEditor->syncSceneBackToPlayState(scene);
        scene->registerExistingObjectsToLayers();
      }
      return;
    } else {
      // Normal pause
      m_pauseRequested = true;
      return;
    }
  }

#ifdef _DEBUG
  // F10 = Open editor
  if (m_input.wasKeyPressed(GLFW_KEY_F10)) {
    if (m_levelEditor && !m_levelEditor->isActive()) {
      m_levelEditor->setActive(true);
      if (auto *scene = m_scenes.current()) {
        m_levelEditor->loadSceneFromPlayState(scene);
      }
    }
  }
#endif

  if (m_returnToMenuRequested) {
    std::cout << "[PlayState] Return to menu requested\n";
    m_gsm.pop();
    return;
  }

  if (m_pauseRequested) {
    m_pauseRequested = false;
    std::cout << "[PlayState] Processing pause request\n";
    m_gsm.push(std::make_shared<PauseState>(m_gsm, m_input));
    return;
  }

  if (!m_loaded)
    return;

  // Check for pause input
  if (m_input.wasKeyPressed(GLFW_KEY_ESCAPE)) {
    std::cout << "[PlayState] ESC pressed - requesting pause\n";
    m_pauseRequested = true;
    return; // Don't continue this frame
  }

  Core::Engine &engine = Core::Engine::Get();
  // Platform::InputManager &input = engine.input();
  Graphics::Camera2D &cam = engine.camera();
  Platform::WindowManager &window = engine.window();
  auto &audio = engine.audio();
  auto &steps = engine.surfaceAudio();
  vec2 mouseWorld =
      m_input.getMouseWorldPos(cam, static_cast<float>(window.width()),
                               static_cast<float>(window.height()));

  if (m_shouldReturnToMenu) {
    m_gsm.pop();
    return;
  }

  if (!m_loaded)
    return;

  if (m_input.wasKeyPressed(GLFW_KEY_ESCAPE)) {
    m_gsm.pop();
    return;
  }
#ifdef _DEBUG
  if (m_input.wasKeyPressed(GLFW_KEY_P)) {
    m_debug = !m_debug;
    if (m_debug) {
      std::cout << "Debug is pressed\n";
    }
    return;
  }
#endif
  // Havign issue with the camera
  if (m_input.wasKeyPressed(GLFW_KEY_R)) {
    m_flashlightOn = !m_flashlightOn;
    std::cout << "Flashlight " << (m_flashlightOn ? "ON" : "OFF") << "\n";

    // Audio::AudioManager &sfx_audio = Core::Engine::Get().audio();

    if (m_flashlightOn == true) {
      // sfx_audio.playOneShot("flashlight_on", Audio::Group::SFX, 0.5f);
      Assets::SoundManager::Play("flashlight_on");
    } else {
      // sfx_audio.playOneShot("flashlight_off", Audio::Group::SFX, 0.5f);
      Assets::SoundManager::Play("flashlight_off");
    }
  }

  if (m_input.wasKeyPressed(GLFW_KEY_Z)) {
    // Toggle generic ImGui overlay
    m_showImGui = !m_showImGui;
    std::cout << "[PlayState] ImGui " << (m_showImGui ? "ENABLED" : "DISABLED")
              << "\n";

    /*
    // Decoupled Level Editor from Z key locally to prioritize ImGui toggle
    request.
    // Editor can still be toggled via F10 (if logic exists) or we can rebind
    it. if (m_levelEditor && m_editorInitialized) { bool newState =
    !m_levelEditor->isActive(); m_levelEditor->setActive(newState); std::cout <<
    "[PlayState] Level editor "
                << (newState ? "activated" : "deactivated") << "\n";

      if (auto *scene = m_scenes.current()) {
        if (newState) {
          m_levelEditor->loadSceneFromPlayState(scene);
        } else {
          m_levelEditor->syncSceneBackToPlayState(scene);
        }
      }
    }
    */
  }

  if (m_levelEditor && m_levelEditor->isActive()) {
    m_levelEditor->update(dt);
    return; // Don't update game while editor is active
  }

  // Syafiq - Player movement
  if (auto *scene = m_scenes.current()) {
    m_scenes.update(dt, m_input);

    // --- Game-specific systems (moved from Scene::Update) ---
    {
      auto &world = scene->GetWorld();
      auto &collisions = scene->GetCollisionSystem();
      const std::vector<bool> *layerFlags =
          scene->m_layerEnabled.empty() ? nullptr : &scene->m_layerEnabled;

      m_activationSystem.update(world, collisions);
      m_batterySystem.update(world, dt);
      m_lightSystem.update(world, dt);
      m_fragmentSystem.update(world, dt);
      m_aiSystem.update(world, dt);
      m_doorSystem.update(world, dt);
      m_doorSwingSystem.update(world, dt);
    }

    // ---- CAMERA FOLLOW (ECS Based) ----
    // This ensures camera tracks player even if "m_player" GameObject pointer
    // is stale
    scene->GetWorld().each<Components::Tag, Components::Transform>(
        [&](ECS::Entity, Components::Tag &tag,
            Components::Transform &t) {
          if (tag.tag != "Player") return;
          auto &camera = Core::Engine::Get().camera();
          camera.setPosition(glm::vec2(t.position.x, t.position.y));
        });

    for (auto &obj : scene->objects()) {

      if (obj->type() == Gameplay::GameObjectType::Player) {
        // ---- CAMERA FOLLOW ----
        if (m_player && m_player->active()) {
          vec2 p = m_player->transform().position;

          // Instantly follow
          cam.setPosition(glm::vec2(p.x, p.y));
        }

        // --- Player Animation Control ---
        Gameplay::SpriteAnimatorComponent *playerSpriteComp =
            obj->getComponent<Gameplay::SpriteAnimatorComponent>();
        if (playerSpriteComp->currentState() != "death") {
          // ---- Movement FOLLOW ----
          // float moveSpeed = static_cast<float>(obj->moveSpeed());
          bool isRunning = false;
          // float maxSpeed = obj->moveSpeed();
          // float maxRunSpeed = maxSpeed*2.0f;

          m_timeSinceStep += (float)dt;
          auto surfaceType = getSurfaceUnder(obj.get());
          vec2 curPos = obj->transform().position;
          float speed =
              magnitude(obj->velocity() * 0.3f); // use your magnitude/length
          bool moving = speed > 5.0f;

          if (moving) {
            if (!m_haveLastFootPos) {
              m_lastFootPos = curPos;
              m_haveLastFootPos = true;
            }
            vec2 delta = curPos - m_lastFootPos;
            float frameDist = magnitude(delta);
            m_distanceSinceStep += frameDist;
            m_lastFootPos = curPos;

            if (m_distanceSinceStep >= m_stepDistance &&
                m_timeSinceStep >= m_minStepInterval) {
              // Time for exactly ONE footsteps
              steps.playFootstep(audio, surfaceType, speed);

              m_distanceSinceStep = 0.0f; // reset distance
              m_timeSinceStep = 0.0f;
            }
          } else {
            // not moving → reset so we don’t “owe” steps
            m_distanceSinceStep = 0.0f;
            m_timeSinceStep = 0.0f;
            m_haveLastFootPos = false;
          }

          vec2 move{0, 0};

          // Skip C++ movement if Lua script handles it
          bool hasLuaScript = obj->getComponent<Scripting::LuaScriptComponent>() != nullptr;
          if (!hasLuaScript) {
            // WASD input → movement vector
            if (m_input.isKeyDown(GLFW_KEY_W))
              move.y += 1;
            if (m_input.isKeyDown(GLFW_KEY_S))
              move.y -= 1;
            if (m_input.isKeyDown(GLFW_KEY_A))
              move.x -= 1;
            if (m_input.isKeyDown(GLFW_KEY_D))
              move.x += 1;

            // normalize diagonal
            if (move.x != 0 || move.y != 0)
              move = normalize(move);

            // apply speed
            float speedMove = obj->moveSpeed();

            // shift to run
            if (m_input.isKeyDown(GLFW_KEY_LEFT_SHIFT)) {
              speedMove *= 2.0f;
              isRunning = true;
            }

            // final velocity
            obj->velocity() = move * speedMove;
          }

          ////not used
          // if (m_input.isKeyDown(GLFW_KEY_LEFT_SHIFT))
          //{
          //	moveSpeed *= 2;
          //	isRunning = true;
          // }

          ////Gameplay::GameObject* fx = nullptr;
          ////for (auto& up : gameObjects) {
          ////	if (up->name() == "ParticleTest") { fx = up.get(); break; }
          ////}
          ////if (fx) {
          ////	if (m_input.isKeyDown(GLFW_KEY_SPACE)) {
          ////		if (auto* em =
          /// fx->getComponent<Gameplay::ParticleEmitter>()) /
          /// em->burst(80); // 80-particle puff /	}
          ////}
          if (m_fx) {
            if (auto *em = m_fx->getComponent<Components::ParticleEmitter>()) {
              if (!em->active()) {
                em->setActive(true);
              }
            }
          }
          ////float moveAmount = moveSpeed * static_cast<float>(dt);

          // if (m_input.isKeyDown(GLFW_KEY_W))
          //{
          //	obj->velocity().y += moveSpeed * static_cast<f32>(dt);
          //	if ((!isRunning) && (obj->velocity().y > maxSpeed)) {
          //		obj->velocity().y = maxSpeed;
          //	}
          //	else if (obj->velocity().y > maxRunSpeed){
          //		obj->velocity().y = maxRunSpeed;
          //	}

          //}
          // else if (m_input.isKeyDown(GLFW_KEY_S))
          //{
          //	obj->velocity().y -= moveSpeed * static_cast<f32>(dt);
          //	if ((!isRunning) && (obj->velocity().y < -maxSpeed)) {
          //		obj->velocity().y = -maxSpeed;
          //	}
          //	else if (obj->velocity().y > -maxRunSpeed) {
          //		obj->velocity().y = -maxRunSpeed;
          //	}
          //}
          // else
          //{
          //	obj->velocity().y = 0;
          //}

          // if (m_input.isKeyDown(GLFW_KEY_A))
          //{
          //	obj->velocity().x -= moveSpeed * static_cast<f32>(dt);
          //	if ((!isRunning) && (obj->velocity().x < -maxSpeed)) {
          //		obj->velocity().x = -maxSpeed;
          //	}
          //	else if (obj->velocity().x > -maxRunSpeed) {
          //		obj->velocity().x = -maxRunSpeed;
          //	}
          // }
          // else if (m_input.isKeyDown(GLFW_KEY_D))
          //{
          //	obj->velocity().x += moveSpeed * static_cast<f32>(dt);
          //	if ((!isRunning) && (obj->velocity().x > maxSpeed)) {
          //		obj->velocity().x = maxSpeed;
          //	}
          //	else if (obj->velocity().y > maxRunSpeed) {
          //		obj->velocity().x = maxRunSpeed;
          //	}
          // }
          // else
          //{
          //	obj->velocity().x = 0;
          // }

          if (m_input.isKeyDown(GLFW_KEY_G)) {
            obj->transform().scale.x += static_cast<f32>(10 * dt);
            obj->transform().scale.y += static_cast<f32>(10 * dt);
          }

          // Player world position
          vec2 playerPos = obj->transform().position;

          // Direction vector from player to mouse
          vec2 dir = mouseWorld - playerPos;

          // Distance check to prevent jitter
          float dist = magnitude(dir);
          if (dist > 10.0f) { // 10 units = threshold distance
            // Normalize direction
            dir = normalize(dir);

            // Compute target angle in radians — atan2(y, x)
            float targetAngle = std::atan2(dir.y, dir.x);
            float currentAngle = obj->transform().rotationRad;

            // Calculate shortest angle difference
            float angleDiff = targetAngle - currentAngle;

            // Normalize angle difference to [-PI, PI] for shortest path
            const float PI = 3.14159265358979f;
            while (angleDiff > PI) angleDiff -= 2.0f * PI;
            while (angleDiff < -PI) angleDiff += 2.0f * PI;

            // Apply rotation (can add smoothing here if desired)
            obj->transform().rotationRad = currentAngle + angleDiff;
          }
          if (obj->type() == Gameplay::GameObjectType::Player &&
              !obj->active()) {
            obj->respawnAt(vec2(0.0f, 0.0f), true, true);
          }

          if (playerSpriteComp) {
            // Determine animation state
            std::string animState = m_flashlightOn ? "idle_torch" : "idle";
            // Check velocity for movement (works with both C++ and Lua movement)
            vec2 vel = obj->velocity();
            bool isMoving = (vel.x != 0 || vel.y != 0);

            if (isMoving) {
              animState = m_flashlightOn ? "walk_torch" : "walk";

              // You can decide if shift key means run
              if (m_input.isKeyDown(GLFW_KEY_LEFT_SHIFT)) {
                animState = m_flashlightOn ? "run_torch" : "run";
              }
            }

            playerSpriteComp->play(animState);
          }
        } else {
          obj->velocity() = vec2();
        }
      }

      // if (obj->type() == Gameplay::GameObjectType::Ball)
      //{
      //	obj->velocity().y = 0.0f;
      //	obj->velocity().x += m_ballDir * m_ballSpeed;

      //	// Predict next X and flip if we would cross a bound
      //	float nextX = obj->transform().position.x + obj->velocity().x *
      // static_cast<float>(dt); 	if (nextX < m_leftBound)
      //	{
      //		nextX = m_leftBound;
      //		m_ballDir = +1;
      //	}
      //	if (nextX > m_rightBound)
      //	{
      //		nextX = m_rightBound;
      //		m_ballDir = -1;
      //	}

      //	obj->transform().position.x = nextX; // snap to keep within
      // bounds (optional)
      //}
    }
  }

  if (m_fx && m_input.isKeyDown(GLFW_KEY_SPACE)) {
    if (auto *pe = m_fx->getComponent<Components::ParticleEmitter>())
      pe->burst(80);
  }

  if (m_input.wasKeyPressed(GLFW_KEY_Z)) {
    if (m_levelEditor && m_editorInitialized) {
      bool newState = !m_levelEditor->isActive();
      m_levelEditor->setActive(newState);
      std::cout << "[PlayState] Level editor "
                << (newState ? "activated" : "deactivated") << "\n";

      // Sync scene data when toggling
      if (auto *scene = m_scenes.current()) {
        if (newState) {
          // Activating: load current state
          m_levelEditor->loadSceneFromPlayState(scene);
        } else {
          // Deactivating: save changes back
          m_levelEditor->syncSceneBackToPlayState(scene);
        }
      }
    }
  }

  if (m_input.isKeyDown(GLFW_KEY_Q)) {
    cam.setZoom(cam.zoom() * 1.02f);
  }
  if (m_input.isKeyDown(GLFW_KEY_E)) {
    cam.setZoom(cam.zoom() / 1.02f);
  }

  if (m_input.wasKeyPressed(GLFW_KEY_F1)) {
    m_spawning = true;
    m_spawnedCount = 0;
    std::cout << "[PlayState] Started spawning objects (F1)\n";
  }

  // Update level editor if active
  if (m_levelEditor && m_levelEditor->isActive()) {
    m_levelEditor->update(dt);
    return; // Don't update game while editor is active
  }

  if (m_spawning && m_spawnedCount < 2500) {
    auto &factory = Gameplay::PrefabFactory::get();
    auto currentScene = m_scenes.currentScene();

    if (currentScene) {
      int toSpawn = std::min(SPAWN_PER_FRAME, 2500 - m_spawnedCount);

      for (int i = 0; i < toSpawn; ++i) {
        int gridSize = 50;
        float spacing = 50.f;
        int idx = m_spawnedCount + i;
        int row = idx / gridSize;
        int col = idx % gridSize;

        float x = -gridSize * spacing / 2.0f + col * spacing;
        float y = -gridSize * spacing / 2.0f + row * spacing;

        auto obj = factory.createAt("Wall", vec2(x, y));
        if (obj) {
          currentScene->addObject(std::move(obj));
        }
      }

      m_spawnedCount += toSpawn;
      if (m_spawnedCount >= 2500) {
        m_spawning = false;
        std::cout << "Finished spawning 2500 objects!\n";
      }
    }
  }

  if (m_player) {
    Gameplay::SpriteAnimatorComponent *playerSpriteComp =
        m_player->getComponent<Gameplay::SpriteAnimatorComponent>();
    const std::string &playerState = playerSpriteComp->currentState();
    if (playerState != "death") {
      auto *scene = m_scenes.current();
      for (auto &obj : scene->objects()) {
        if (!obj || !obj->active())
          continue;

        if (obj->type() == Gameplay::GameObjectType::Enemy) {
          vec2 playerPos = m_player->transform().position;
          vec2 enemyPos = obj->transform().position;

          vec2 playerHalf = m_player->size() * 0.5f * 2.0f;
          vec2 enemyHalf = obj->size() * 0.5f * 2.0f;

          // Check if bounding boxes overlap

          if (abs(playerPos.x - enemyPos.x) < (playerHalf.x + enemyHalf.x) &&
              abs(playerPos.y - enemyPos.y) < (playerHalf.y + enemyHalf.y)) {

            std::cout << "Player hit by enemy: " << obj->name() << "!\n";

            // Kill the player, killing at Physics2D, need to use event bus
            // later on.
            playerSpriteComp->play("death");
            m_playerRespawning = 7.0f;
            // m_player->setActive(false);
            break;
          }
        }
      }
    } else {
      m_flashlightOn = false;
      m_playerRespawning -= (float)dt;

      // Immediately trigger Game Over state after a short delay (e.g. 1 second
      // of death animation)
      if (m_playerRespawning <= 6.0f) { // Assuming starts at 7.0f, wait 1s
        m_GameOver = true;
        std::cout << "[PlayState] Transitioning to GameOverState\n";
        m_gsm.push(
            std::make_shared<GameOverState>(m_gsm, m_input, m_initialSceneId));
        return; // Stop updating PlayState
      }

      // Old auto-respawn logic removed
      /*
      if (m_playerRespawning <= 5.f) {
        m_GameOver = true;
      }
      if (m_playerRespawning <= 0) {
        m_playerRespawning = 7.f;
        m_GameOver = false;
        m_player->respawnAt(m_spawnPoint, true, true);
        std::cout << "Player respawned!\n";
        resetScene();
        return;
      }
      */
    }
  }

  updateFragmentCollection(dt);
  m_scenes.update(dt, m_input);
  syncEnemyTransforms();
  updateLightExposure(dt);
  updateBatteryDisplay();
}

void PlayState::updateBatteryDisplay() {
  float batteryPercent = currentBattery / maxBattery;

  // Select appropriate texture based on battery level
  if (batteryPercent > 0.875f) {
    currentBatteryTexture = "battery_full";
  } else if (batteryPercent > 0.625f) {
    currentBatteryTexture = "battery_3/4";
  } else if (batteryPercent > 0.375f) {
    currentBatteryTexture = "battery_2/4";
  } else if (batteryPercent > 0.125f) {
    currentBatteryTexture = "battery_1/4";
  } else {
    currentBatteryTexture = "battery_empty";
  }
}

// Helper: Check if point is inside polygon
bool isPointInPolygon(const glm::vec2 &point,
                      const std::vector<glm::vec2> &polygon) {
  int intersections = 0;
  size_t n = polygon.size();

  for (size_t i = 0; i < n; i++) {
    glm::vec2 p1 = polygon[i];
    glm::vec2 p2 = polygon[(i + 1) % n];

    if ((p1.y > point.y) != (p2.y > point.y)) {
      float xIntersect =
          p1.x + (point.y - p1.y) * (p2.x - p1.x) / (p2.y - p1.y);
      if (point.x < xIntersect) {
        intersections++;
      }
    }
  }

  return (intersections % 2) == 1;
}

void PlayState::updateLightExposure(double dt) {
  auto *scene = m_scenes.current();
  if (!scene)
    return;

  auto *physics = scene->getLegacyPhysics();

  // Only check if flashlight is on
  if (!m_flashlightOn || !physics || !m_player || !m_player->active() ||
      m_player->checkBatt() <= 0.0f) {
    // Flashlight is off - reset all enemy exposures
    for (auto &obj : scene->objects()) {
      if (obj && obj->type() == Gameplay::GameObjectType::Enemy) {
        obj->resetLightExposure();
      }
    }
    return;
  }

  // Calculate flashlight position and angle
  glm::vec2 torchOffsetLocal(35.0f, -20.0f);
  vec2 origin = m_player->transform().position;
  float angle = m_player->transform().rotationRad;
  glm::vec2 torchOffsetWorld(
      torchOffsetLocal.x * cos(angle) - torchOffsetLocal.y * sin(angle),
      torchOffsetLocal.x * sin(angle) + torchOffsetLocal.y * cos(angle));
  glm::vec2 originGLM = glm::vec2(origin.x, origin.y) + torchOffsetWorld;

  // Calculate light direction vector
  glm::vec2 lightDir(cos(angle), sin(angle));

  // Compute visibility polygon
  std::vector<std::unique_ptr<Gameplay::GameObject>> &objs = scene->objects();
  vec2 originVec2 = {originGLM.x, originGLM.y};
  std::vector<vec2> lightPoly = m_renderer->computeVisibilityPolygon(
      originVec2, angle, glm::radians(60.0f), 300.0f, *physics, objs);

  std::vector<glm::vec2> lightPolyGLM;
  for (const auto &point : lightPoly) {
    lightPolyGLM.emplace_back(point.x, point.y);
  }

  // if (lightPolyGLM.size() > 0) {
  //	for (size_t i = 0; i < std::min(size_t(5), lightPolyGLM.size()); i++) {
  //		std::cout << "  [" << i << "] (" << lightPolyGLM[i].x << ", " <<
  // lightPolyGLM[i].y << ")" << std::endl;
  //	}
  // }
  //  ✓ Check enemies against light polygon
  const float KILL_THRESHOLD = 3.0f;
  const float DECAY_RATE = 2.0f;
  auto audio = Core::Engine::Get().audio();

  for (auto &obj : objs) {
    if (!obj || !obj->active())
      continue;

    if (obj->type() == Gameplay::GameObjectType::Enemy) {
      glm::vec2 enemyPos(obj->transform().position.x,
                         obj->transform().position.y);

      // FORCE TRUE for testing
      bool inLight = isPointInPolygon(enemyPos, lightPolyGLM);

      if (inLight) {
        obj->addLightExposure(static_cast<float>(dt));
      } else {
        if (obj->lightExposureTime() > 0.0f) {
        }
        obj->resetLightExposure();
      }

      if (isPointInPolygon(enemyPos, lightPolyGLM)) {
        // Enemy is in light
        obj->addLightExposure(static_cast<float>(dt));

        if (obj->lightExposureTime() >= KILL_THRESHOLD) {
          std::cout << ">>> Enemy '" << obj->name() << "' KILLED!" << std::endl;
          static const std::array<const char *, 3> defaultIds = {
              "enemy_death_1", "enemy_death_2", "enemy_death_3"};
          std::string id = pickRandom(defaultIds);

          audio.playOneShot(id, Audio::Group::SFX, 0.7f);
          obj->setActive(false);
        }
      } else {
        // Enemy not in light - reset exposure
        if (obj->lightExposureTime() > 0.0f) {
          std::cout << "Enemy '" << obj->name() << "' left light" << std::endl;
        }
        // float oldExposure = obj->lightExposureTime();
        obj->decreaseLightExposure(static_cast<float>(dt) * DECAY_RATE);
        // float newExposure = obj->lightExposureTime();
      }
    }
  }
}

void PlayState::renderImGui(Core::Time *timer) {
  (void)timer;

#ifdef _DEBUG
  // Toggleable Debug UI
  if (m_showImGui) {
    ImGui::Begin("PlayState Debug");
    ImGui::Text("Play State ImGui Enabled");
    ImGui::Checkbox("Show ImGui (Z)", &m_showImGui);

    if (ImGui::Button("Return to Menu")) {
      m_shouldReturnToMenu = true;
    }
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::End();
  }

  // Only render editor UI if editor is active
  if (m_levelEditor && m_levelEditor->isActive()) {
    Core::Engine &engine = Core::Engine::Get();
    Graphics::ImGuiManager &imgui = engine.imguiManager();

    imgui.beginDockspace();
    m_levelEditor->renderUI();
    imgui.endDockspace();
  }
#endif
}

void PlayState::render(Core::Time *timer) {
  if (!m_loaded || !m_renderer)
    return;

  Core::Engine &engine = Core::Engine::Get();

  // Editor rendering mode
  if (m_levelEditor && m_levelEditor->isActive()) {
    Graphics::FrameBuffer &editorFB = m_levelEditor->getFramebuffer();
    editorFB.bind();

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_renderer->beginScenePass(m_levelEditor->getCamera().viewProj());
    m_scenes.render(*m_renderer);
    m_renderer->endScenePass();

    editorFB.unbind();

    m_levelEditor->render();
    return;
  }

  // Normal game rendering (keep all your existing code below)
  m_renderer->beginScenePass(engine.camera().viewProj());
  m_scenes.render(*m_renderer);
  m_renderer->endScenePass();

  // // ==================== Torch Light Rendering & Enemy Interaction
  // // ====================
  // auto *scene = m_scenes.current();
  // if (!scene)
  //   return;

  // auto *physics = scene->getLegacyPhysics();

  // // Battery management code (keep all existing flashlight code)
  // if (m_player) {
  //   float dt = static_cast<float>(timer->delta());

  //   if (m_flashlightOn) {
  //     currentBattery = m_player->checkBatt();
  //     currentBattery -= m_player->checkDrain() * dt * 4.f;
  //     m_player->setBatt(currentBattery);

  //     if (currentBattery <= 0.0f) {
  //       m_player->setBatt(0.0f);
  //       m_flashlightOn = false;
  //     }
  //   } else {
  //     currentBattery = m_player->checkBatt();
  //     currentBattery += m_player->checkCharge() * dt * 2.f;
  //     m_player->setBatt(std::min(100.0f, currentBattery));
  //   }
  // }

  // // Render flashlight (keep all existing flashlight rendering code)
  // if (m_flashlightOn && physics && m_player && m_player->active() &&
  //     m_player->checkBatt() > 0.0f) {
  //   glm::vec2 torchOffsetLocal(35.0f, -20.0f);
  //   vec2 origin = m_player->transform().position;
  //   float angle = m_player->transform().rotationRad;
  //   glm::vec2 torchOffsetWorld(
  //       torchOffsetLocal.x * cos(angle) - torchOffsetLocal.y * sin(angle),
  //       torchOffsetLocal.x * sin(angle) + torchOffsetLocal.y * cos(angle));
  //   glm::vec2 originGLM = glm::vec2(origin.x, origin.y) + torchOffsetWorld;

  //   std::vector<std::unique_ptr<Gameplay::GameObject>> &objs =
  //   scene->objects(); vec2 originVec2 = {originGLM.x, originGLM.y};
  //   std::vector<vec2> lightPoly = m_renderer->computeVisibilityPolygon(
  //       originVec2, angle, glm::radians(60.0f), 300.0f, *physics, objs);

  //   std::vector<glm::vec2> lightPolyGLM;
  //   for (const auto &point : lightPoly) {
  //     lightPolyGLM.emplace_back(point.x, point.y);
  //   }

  //   // Render the light cone
  //   float batteryPercent = currentBattery / maxBattery;
  //   m_renderer->beginLightPass(engine.camera().viewProj(), originGLM,
  //                              glm::vec3(1.0f, 0.9f, 0.6f), 300.0f, angle,
  //                              glm::radians(60.0f), batteryPercent);
  //   m_renderer->drawLightCone(originGLM, lightPolyGLM, {1.0f, 0.9f, 0.6f});
  //   m_renderer->endLightPass();
  // } else {
  //   // Flashlight is off - reset all enemy exposure times
  //   for (auto &obj : scene->objects()) {
  //     if (obj && obj->type() == Gameplay::GameObjectType::Enemy) {
  //       obj->resetLightExposure();
  //     }
  //   }
  // }

  if (m_flashlightOn) {
    renderTorchPass(*m_renderer);
  }

  // ==================== Battery UI Rendering ====================
  if (m_player) {
    // Update battery display based on current battery level
    float batteryPercent =
        m_player->checkBatt() / 100.0f; // Assuming max battery is 100

    // Select appropriate texture based on battery level
    if (batteryPercent > 0.875f) {
      currentBatteryTexture = "battery_full";
    } else if (batteryPercent > 0.625f) {
      currentBatteryTexture = "battery_3/4";
    } else if (batteryPercent > 0.375f) {
      currentBatteryTexture = "battery_2/4";
    } else if (batteryPercent > 0.125f) {
      currentBatteryTexture = "battery_1/4";
    } else {
      currentBatteryTexture = "battery_empty";
    }

    // Get the texture from TextureManager
    auto batteryTex = Assets::TextureManager::Get(currentBatteryTexture);

    if (batteryTex) {
      // Get the OpenGL texture ID
      unsigned int batteryTexId = batteryTex->id();

      // Get window size from engine
      int windowWidth = engine.window().width();
      int windowHeight = engine.window().height();

      glm::vec2 screenSize(static_cast<float>(windowWidth),
                           static_cast<float>(windowHeight));
      glm::vec2 batteryPos(screenSize.x - 100.0f,
                           screenSize.y - 100.0f); // Top-right corner
      glm::vec2 batterySize(64.0f, 64.0f); // Adjust to your texture size

      // Use an orthographic projection for UI (screen space)
      glm::mat4 uiProjection =
          glm::ortho(0.0f, screenSize.x, 0.0f, screenSize.y);

      m_renderer->beginScenePass(uiProjection);
      m_renderer->drawSprite(
          batteryTexId, batteryPos, batterySize,
          0.0f,                              // no rotation
          glm::vec4(0.0f, 0.0f, 1.0f, 1.0f), // full UV rect
          glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)  // white tint, full opacity
      );
      m_renderer->endScenePass();
    }
  }

  renderFragmentUI();
  Platform::WindowManager &window = engine.window();
  int w = window.width();
  int h = window.height();
  glm::mat4 uiProj = glm::ortho(0.f, (float)w, 0.f, (float)h);
  m_renderer->beginFontPass(uiProj);

  if (m_GameOver) {
    auto shader = Assets::ResourceManager<Shader>::Get("text");
    auto subFont =
        Assets::ResourceManager<Assets::FontManager>::Get("Subtitle");
    subFont->RenderText("YOU DIED!", w * 0.40f, h * 0.6f, 1.f,
                        glm::vec3{1, 0.2, 0.2}, *shader);
    std::stringstream ss;
    ss << "Restarting in " << int(m_playerRespawning);
    subFont->RenderText(ss.str(), w * 0.32f, h * 0.4f, 1.f,
                        glm::vec3{1, 0.2, 0.2}, *shader);
  }

  m_renderer->endFontPass();

  (void)timer;
}

// test for IMGUI (still broken)
const char *defaultTextures[] = {
    "player_texture", "enemy.png", "assets/textures/wall.png",
    "assets/textures/floor.png", "assets/textures/trap.png"};

void PlayState::initializeEnemyAI(Runtime::Scene *scene) {
  if (!scene || !m_player) {
    std::cout
        << "[PlayState] Cannot initialize enemy AI: scene or player missing\n";
    return;
  }

  // Clear any existing ECS mapping
  clearEnemyAI();

  auto &world = scene->GetWorld();
  auto &objs = scene->objects();

  for (auto &obj : objs) {
    if (obj && obj->type() == Gameplay::GameObjectType::Enemy &&
        obj->active()) {

      // Create ECS Entity
      ECS::Entity e = world.createEntity();

      // Add Transform (Sync from GameObject)
      Components::Transform t;
      t.position = obj->transform().position;
      t.rotation = obj->transform().rotationRad;
      t.scale = obj->transform().scale;
      world.addComponent(e, t);

      // Add Velocity (Default)
      Components::Velocity v;
      v.moveSpeed = 150.0f; // Default chase speed
      v.velocity = {0.f, 0.f};
      world.addComponent(e, v);

      // Add EnemyAI (Default)
      Components::EnemyAI ai;
      ai.state = Components::AIState::Patrol;
      ai.patrolSpeed = 50.0f;
      ai.chaseSpeed = 150.0f;
      ai.detectionRadius = 300.0f;
      ai.lightSensitivity = 0.3f; // Slow down to 30% in light
      world.addComponent(e, ai);

      // Add LightExposure
      world.addComponent(e, Components::LightExposure{});

      // Map ID to Entity
      m_enemyMap[obj->id()] = e;

      std::cout << "[PlayState] Initialized ECS AI for enemy: " << obj->name()
                << " (ID: " << obj->id() << ")\n";
    }
  }
}

void PlayState::clearEnemyAI() { m_enemyMap.clear(); }

void PlayState::syncEnemyTransforms() {
  auto *scene = m_scenes.current();
  if (!scene)
    return;

  auto &world = scene->GetWorld();

  // Iterate through GameObjects and sync from ECS if mapped
  for (auto &obj : scene->objects()) {
    if (!obj || !obj->active())
      continue;

    auto it = m_enemyMap.find(obj->id());
    if (it != m_enemyMap.end()) {
      ECS::Entity e = it->second;

      // Check if entity still valid (in a real ECS, this might be complex, but
      // here we assume sync)
      auto *t = world.getComponent<Components::Transform>(e);
      auto *v = world.getComponent<Components::Velocity>(e);
      auto *light = world.getComponent<Components::LightExposure>(e);

      if (t) {
        obj->transform().position = t->position;
        // obj->transform().rotationRad = t->rotation; // Should AI rotate? Yes.
      }

      if (v) {
        // GameObject uses velocity per second for integration
        // ECS velocity is direction * speed?
        // PhysicsSystem: pos += vel.vel * vel.speed * dt
        // GameObject: pos += velocity * dt
        // So GameObject velocity = velocity * moveSpeed
        obj->velocity() = v->velocity * v->moveSpeed;

        // Rotation from velocity
        if (v->velocity.x != 0 || v->velocity.y != 0) {
          float angle = std::atan2(v->velocity.y, v->velocity.x);
          obj->transform().rotationRad = angle;
        }
      }

      if (light) {
        obj->lightExposureTime(); // Getter
        if (light) {
          light->exposureTime = obj->lightExposureTime();
          light->inLight = (light->exposureTime > 0.0f);
        }
      }
    }
  }
}

// void PlayState::renderImGui(Core::Time *timer)
//{
//	if (!m_debug)
//		return;

//	if (m_debug)
//	{
//		ImGui::SetNextWindowSize(ImVec2(700, 500),
// ImGuiCond_FirstUseEver); 		ImGui::Begin("Game State", nullptr,
// ImGuiWindowFlags_AlwaysAutoResize);

//		// ******* Object Creation **********
//		static int selectedType = 0;
//		const char *types[] = {"Player", "Enemy", "Wall", "Floor",
//"Trap"}; 		ImGui::Combo("Type", &selectedType, types,
// IM_ARRAYSIZE(types));

//		static float pos[2] = {0, 0};
//		static float size[2] = {1, 1};
//		ImGui::InputFloat2("Position", pos);
//		ImGui::InputFloat2("Size", size);

//		static int hp = 10, damage = 1;
//		static float ms = 100.0f;
//		if (selectedType == 0 || selectedType == 1) { // if its enemy or
// player, set health 			ImGui::InputInt("HP", &hp);
//		}

//		if (selectedType == 1 || selectedType == 4) { // if its enemy or
// trap, set damage 			ImGui::InputInt("Damage", &damage);
//		}

//		static char texture[64] = "";
//		ImGui::InputText("Texture", texture, IM_ARRAYSIZE(texture));

//		if (ImGui::Button("Create Object"))
//		{

//			const char *texToUse = texture[0] ? texture :
// defaultTextures[selectedType];
// std::unique_ptr<Gameplay::GameObject> obj = nullptr;

//			// Supposedly spawn the corresponding gameobject
//(texture and logic not fixed) 			if (selectedType == 0)
//			{
//				obj =
// Gameplay::PrefabFactory::get().create("Player"); if (obj)
//				{
//					obj->setMoveSpeed(ms);
//					obj->setPosition(vec2(pos[0], pos[1]));
//					obj->setSize(vec2(size[0], size[1]));
//					obj->autoUpdateCollisionBaseOnScale();
//					obj->setTextureId(texToUse);
//					auto scene = m_scenes.current();
//					if (scene)
//					{
//						obj->setLayer(Gameplay::DrawLayer::Actors);
//						scene->addObject(std::move(obj));
//					}
//					std::cout << "Player created\n";
//				}
//			}
//			else if (selectedType == 1)
//			{
//				obj =
// Gameplay::PrefabFactory::get().create("Enemy"); if (obj)
//				{
//					obj->setMoveSpeed(ms);
//					obj->setPosition(vec2(pos[0], pos[1]));
//					obj->setSize(vec2(size[0], size[1]));
//					obj->autoUpdateCollisionBaseOnScale();
//					auto scene = m_scenes.current();
//					if (scene)
//					{
//						obj->setLayer(Gameplay::DrawLayer::Actors);
//						scene->addObject(std::move(obj));
//					}
//					std::cout << "Enemy created\n";
//				}
//			}
//			else if (selectedType == 2)
//			{
//				obj =
// Gameplay::PrefabFactory::get().create("Wall"); if (obj)
//				{
//					obj->setPosition(vec2(pos[0], pos[1]));
//					obj->setSize(vec2(size[0], size[1]));
//					obj->autoUpdateCollisionBaseOnScale();
//					auto scene = m_scenes.current();
//					if (scene)
//					{
//						obj->setLayer(Gameplay::DrawLayer::Background);
//						scene->addObject(std::move(obj));
//					}
//					std::cout << "Wall created\n";
//				}
//			}
//			else if (selectedType == 3)
//			{
//				obj =
// Gameplay::PrefabFactory::get().create("Floor"); if (obj)
//				{
//					obj->setPosition(vec2(pos[0], pos[1]));
//					obj->setSize(vec2(size[0], size[1]));
//					auto scene = m_scenes.current();
//					if (scene)
//					{
//						obj->setLayer(Gameplay::DrawLayer::World);
//						scene->addObject(std::move(obj));
//					}
//					std::cout << "Floor created\n";
//				}
//			}
//			else if (selectedType == 4)
//			{
//				obj =
// Gameplay::PrefabFactory::get().create("Trap"); if (obj)
//				{
//					obj->setPosition(vec2(pos[0], pos[1]));
//				obj->setSize(vec2(size[0], size[1]));
//					gameObjects.push_back(std::move(obj));
//				}
//			}
//		}

//		if (ImGui::Button("Delete Object"))
//		{
//			auto scene = m_scenes.current();
//			auto& v = scene->objects();
//			if (!v.empty())
//			{
//				Gameplay::GameObject* doomed = v.back().get();
//				scene->destroyObject(doomed);
//			}
//		}
//		ImGui::Text("Object counter: %d ",
// m_scenes.current()->objects().size());

//		ImGui::Separator();

//		// ******* Testing Window **********
//		ImGui::Text("Testing Window");

//		if (timer)
//		{
//			double fps = timer->fps();
//			;
//			double frameTime = timer->delta() * 1000.0;

//			ImGui::Text("FPS: %.1f", fps);
//			ImGui::Text("Frame Time: %.2f ms", frameTime);
//		}
//		ImGui::Separator();

//		ImGui::Text("Performance Profiler (Live)");
//		ImGui::Spacing();

//		auto &profiler = Core::Profiler::Get();
//		const auto &data = profiler.data();

//		if (data.empty())
//		{
//			ImGui::TextDisabled("No profiling data recorded
// yet...");
//		}
//		else
//		{
//			for (const auto &[scope, info] : data)
//			{
//				double avg = info.avgTime();
//				double total = info.totalTime;
//				int count = info.callCount;

//				// Color code performance (green=fast, red=slow)
//				ImVec4 color;
//				if (avg < 1.0)
//					color = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
//// fast (<1ms) 				else if (avg < 8.0)
/// color = ImVec4(1.0f, 1.0f, 0.3f, 1.0f); //
// moderate 				else
// color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); // slow

//				ImGui::TextColored(color, "%-18s | Avg: %.3f ms
//| Calls: %d | Total: %.2f ms",
// scope.c_str(), avg, count, total);
//			}
//		}

//		ImGui::Spacing();

//		if (ImGui::Button("Show Performance Profile"))
//		{
//			Core::Profiler::Get().print();
//		}

//		if (ImGui::Button("Reset Profiler"))
//		{
//			Core::Profiler::Get().reset();
//		}

//		if (ImGui::CollapsingHeader("Layers Debug",
// ImGuiTreeNodeFlags_DefaultOpen)) { 			auto* scene =
// m_scenes.current(); 			if (scene) {
//				// 1) Count objects per DrawLayer bit from the
// scene's objects() 				std::size_t counts[5] = {
// 0,0,0,0,0 }; 				static const uint32_t bits[5] =
// { 					Gameplay::DrawLayer::Background,
// Gameplay::DrawLayer::World,
// Gameplay::DrawLayer::Actors,
//					Gameplay::DrawLayer::FX,
//					Gameplay::DrawLayer::UI
//				};
//				static const char* names[5] = { "Background
//(1)","World (2)","Actors (4)","FX (8)","UI (16)" };

//				auto& objs = scene->objects();
//				for (auto& u : objs) {
//					if (!u) continue;
//					uint32_t L = u->layer();
//					for (int i = 0; i < 5; ++i) {
//						if (L & bits[i]) { counts[i]++;
//}
//					}
//				}

//				ImGui::TextDisabled("Per-layer object counts:");
//				for (int i = 0; i < 5; ++i) {
//					ImGui::BulletText("%s : %zu", names[i],
// counts[i]);
//				}

//				ImGui::Separator();

//				// 2) Pick an object (by index) and move it to
// another layer live 				static int chosen = 0; // object
// index in scene->objects() 				static int
// targetLayerIdx = 2;               // default "Actors"

//				if (!objs.empty() && ImGui::Button("Delete
// Chosen Object"))
//				{
//					if (auto scene1 = m_scenes.current())
//					{
//						Gameplay::GameObject* doomed =
// objs[chosen].get();
// scene1->destroyObject(doomed);
// if (chosen >= (int)scene1->objects().size()) chosen =
//(int)scene1->objects().size() - 1;
//					}
//				}

//				// clamp if list shrank
//				if (chosen < 0) chosen = 0;
//				if (chosen >= (int)objs.size()) chosen =
//(int)objs.size() - 1;

//

//				ImGui::TextDisabled("Move one object to a new
// draw layer:"); 				ImGui::SliderInt("Object Index",
// &chosen, 0, (int)objs.size() > 0 ? (int)objs.size() - 1 : 0);

//				if (!objs.empty()) {
//					auto* o = objs[chosen].get();
//					ImGui::Text("Chosen: id=%u name=\"%s\"
// type=%d currentBit=%u", 						o->id(),
// o->name().c_str(), (int)o->type(), o->layer());

//					ImGui::RadioButton("Background",
//&targetLayerIdx, 0); ImGui::SameLine();
// ImGui::RadioButton("World", &targetLayerIdx, 1); ImGui::SameLine();
// ImGui::RadioButton("Actors", &targetLayerIdx, 2); ImGui::SameLine();
// ImGui::RadioButton("FX", &targetLayerIdx, 3); ImGui::SameLine();
// ImGui::RadioButton("UI", &targetLayerIdx, 4);

//					if (ImGui::Button("Apply New Layer")) {
//						o->setLayer(bits[targetLayerIdx]);
//// IMPORTANT: use setter so Scene’s callback re-buckets
//					}
//				}

//				ImGui::Separator();

//				// 3) Optional: assert pass ordering once per
// frame (toggleable) 				static bool assertOrder = false;
// ImGui::Checkbox("Assert render order (debug)", &assertOrder);
// if (assertOrder) {
//					// Lightweight check: next frame
// Scene::render will hit the assert if order regresses.
//					// (Put the actual assert in
// Scene::render as shown earlier.)
//				}
//			}
//			else {
//				ImGui::TextDisabled("No scene loaded.");
//			}
//		}

//		ImGui::Checkbox("Activate OnClickSpawn", &m_spawnonclick);

//		// Add button to spawn 2500 objects
//		if (ImGui::Button("Spawn 2500 Objects"))
//		{
//			// Get the current scene using the existing method
//			auto &factory = Gameplay::PrefabFactory::get();
//			auto currentScene = m_scenes.currentScene(); // Returns
// std::shared_ptr<Scene> 			if (currentScene)
//			{
//				// Spawn objects in a grid pattern
//				int gridSize = 50; // 50x50 = 2500 objects
//				float spacing = 20.f;
//				float startX = -gridSize * spacing / 2.0f;
//				float startY = -gridSize * spacing / 2.0f;

//				for (int i = 0; i < gridSize; ++i)
//				{
//					for (int j = 0; j < gridSize; ++j)
//					{
//						float x = startX + i * spacing;
//						float y = startY + j * spacing;

//						// Create an object using your
// prefab system 						auto obj =
// factory.createAt("Floor", vec2(x, y));
// if (obj)
//						{
//							obj->setLayer(Gameplay::DrawLayer::Background);
//							currentScene->addObject(std::move(obj));
//						}
//					}
//				}
//				std::cout << "Spawned 2500 objects!\n";
//			}
//		}

//		if (ImGui::Button("Spawn 2500 Objects Gradually"))
//		{
//			m_spawning = true;
//			m_spawnedCount = 0;
//		}

//		if (ImGui::Button("Back to Menu (ESC)"))
//		{
//			m_gsm.pop();
//		}

//		if (ImGui::Button("Reset Scene"))
//		{
//			resetScene();
//		}

//		ImGui::Separator();

//		ImGui::Text("Press [W][A][S][D] to move!");
//		ImGui::Text("Press [Shift] to run!");
//		ImGui::Text("Press [Z] to open the level editor!");

//		ImGui::End();
//	}

//	// ImDrawList* dl = ImGui::GetForegroundDrawList();

//	// const ImVec2 center{ 640.0f, 360.0f };
//	// const float radius = 60.0f;
//	// const int segments = 48; // More segments = smoother circle
//	// const float thickness = 3.0f;

//	// dl->AddCircle(center, radius, IM_COL32(255, 0, 0, 255), segments,
// thickness);
//}

void PlayState::resetScene() {
  if (m_scenes.reloadCurrent()) {
    std::cout << "[PlayState] Scene reset Success\n";

    rebindLightsAndPlayerAfterReload();

    if (auto *scene = m_scenes.current()) {
      scene->registerExistingObjectsToLayers();
      m_player = findFirstPlayer(scene);

      initializeEnemyAI(scene);
    }

  } else {
    m_scenes.loadScene(m_initialSceneId);
    rebindLightsAndPlayerAfterReload();
    if (auto *scene = m_scenes.current()) {
      scene->registerExistingObjectsToLayers();
      m_player = findFirstPlayer(scene);

      initializeEnemyAI(scene);
    }
  }

  m_player->setMoveable(true);
}

} // namespace Runtime

void Runtime::PlayState::updateFragmentCollection(double dt) {
  auto *scene = m_scenes.current();
  if (!scene) {
    return;
  }

  // Find player
  Gameplay::GameObject *player = nullptr;
  for (auto &obj : scene->objects()) {
    if (obj && obj->type() == Gameplay::GameObjectType::Player &&
        obj->active()) {
      player = obj.get();
      break;
    }
  }

  if (!player) {
    return;
  }

  // Get player's fragment collector component
  auto *collector =
      player->getComponent<Gameplay::FragmentCollectorComponent>();
  if (!collector) {
    return;
  }

  // Get pickup radius from collector (JSON-configurable!)
  float pickupRadius = collector->pickupRadius();
  float pickupRadiusSquared = pickupRadius * pickupRadius;

  vec2 playerPos = player->transform().position;

  // Check for fragment collisions
  for (auto &obj : scene->objects()) {
    if (!obj || !obj->active()) {
      continue;
    }

    // Check if this is a Fragment GameObject
    if (obj->type() != Gameplay::GameObjectType::Fragment) {
      continue;
    }

    // Check if already collected
    auto *fragmentComp = obj->getComponent<Gameplay::FragmentComponent>();
    if (fragmentComp && fragmentComp->isCollected()) {
      continue;
    }

    // Calculate distance
    vec2 fragmentPos = obj->transform().position;
    float dx = playerPos.x - fragmentPos.x;
    float dy = playerPos.y - fragmentPos.y;
    float distSquared = dx * dx + dy * dy;

    // Check if within pickup radius
    if (distSquared <= pickupRadiusSquared) {
      handleFragmentPickup(player, obj.get());
    }
  }

  // Update notification timer
  if (m_showFragmentNotification) {
    m_fragmentNotificationTimer -= static_cast<float>(dt);
    if (m_fragmentNotificationTimer <= 0.0f) {
      m_showFragmentNotification = false;
    }
  }
}

void Runtime::PlayState::handleFragmentPickup(Gameplay::GameObject *player,
                                              Gameplay::GameObject *fragment) {

  if (fragment->type() != Gameplay::GameObjectType::Fragment) {
    return;
  }

  auto *collector =
      player->getComponent<Gameplay::FragmentCollectorComponent>();
  if (!collector) {
    return;
  }

  // Get optional fragment component for metadata
  auto *fragmentComp = fragment->getComponent<Gameplay::FragmentComponent>();

  // Check if already collected
  if (fragmentComp && fragmentComp->isCollected()) {
    return;
  }

  // Get fragment value (default 1 if no component)
  int value = fragmentComp ? fragmentComp->value() : 1;

  // Get fragment ID for notification
  std::string fragmentId =
      fragmentComp ? fragmentComp->fragmentId() : fragment->name();

  // Mark as collected
  if (fragmentComp) {
    fragmentComp->markCollected();
  }

  // Update collector
  collector->collectFragment(value);

  // Deactivate the fragment
  fragment->setActive(false);

  // Show notification (3 seconds)
  m_showFragmentNotification = true;
  m_fragmentNotificationTimer = 3.0f;
  m_lastCollectedFragmentId = fragmentId;

  // Optional: Play sound
  // auto& audio = Core::Engine::Get().audio();
  // audio.playSound("fragment_collect");

  // Check completion
  if (collector->canEscape()) {
    // Optional: Trigger escape door, next level, etc.
  }
}

void Runtime::PlayState::renderFragmentUI() {

  auto *scene = m_scenes.current();
  if (!scene) {
    // std::cout << "[FRAGMENT UI] No scene!\n";
    return;
  }

  // Find player
  Gameplay::GameObject *player = nullptr;
  for (auto &obj : scene->objects()) {
    if (obj && obj->type() == Gameplay::GameObjectType::Player &&
        obj->active()) {
      player = obj.get();
      break;
    }
  }

  if (!player) {
    // std::cout << "[FRAGMENT UI] No player found!\n";
    return;
  }

  auto *collector =
      player->getComponent<Gameplay::FragmentCollectorComponent>();
  if (!collector) {
    // std::cout << "[FRAGMENT UI] !!! PLAYER HAS NO FragmentCollectorComponent
    // !!!\n";
    return;
  }

  Core::Engine &engine = Core::Engine::Get();
  int windowWidth = engine.window().width();
  int windowHeight = engine.window().height();
  glm::vec2 screenSize(static_cast<float>(windowWidth),
                       static_cast<float>(windowHeight));
  glm::mat4 uiProjection = glm::ortho(0.0f, screenSize.x, 0.0f, screenSize.y);

  m_renderer->beginFontPass(uiProjection);

  auto shader = Assets::ResourceManager<Graphics::Shader>::Get("text");
  auto font = Assets::ResourceManager<Assets::FontManager>::Get("Debug");

  if (!shader) {
    // std::cout << "[FRAGMENT UI] Text shader not found!\n";
  }
  if (!font) {
    // std::cout << "[FRAGMENT UI] Debug font not found!\n";
  }

  if (font && shader) {

    std::string text = std::to_string(collector->collectedCount()) + " / " +
                       std::to_string(collector->requiredFragments());

    // font->RenderText("FRAGMENT TEST", 20.0f, screenSize.y - 80.0f, 1.0f,
    //	glm::vec3(1.0f, 1.0f, 0.0f), *shader);

    // font->RenderText(text, 20.0f, screenSize.y - 120.0f, 1.0f,
    //	glm::vec3(1.0f, 1.0f, 1.0f), *shader);

    // Load fragment icon texture
    auto iconTex = Assets::TextureManager::Get(m_fragmentIconTexture);
    if (iconTex) {
      // Render icon + text
    } else {
      // Fallback text
    }

    // Simple text rendering for now
    font->RenderText("Fragments: " + text, 20.0f, screenSize.y - 60.0f, 0.8f,
                     glm::vec3(1.0f, 0.8f, 0.0f), *shader);
  }

  m_renderer->endFontPass();
}
#endif

// Helper for ECS Raycasting (AABB only for now, matching Components::Collider)
static std::optional<glm::vec2> raycastLightECS(const glm::vec2 &origin,
                                                const glm::vec2 &dir,
                                                float maxDist,
                                                ECS::World &world) {
  glm::vec2 closestHit = origin + dir * maxDist;
  float closestDist = maxDist;
  bool hitFound = false;

  // Iterate all entities with Transform and Collider
  world.each<Components::Transform, Components::Collider>(
      [&](ECS::Entity, Components::Transform &t, Components::Collider &c) {
        if (!c.blocksLight)
          return;

        // AABB World Bounds
        // boxMin = pos + offset - size/2
        // boxMax = pos + offset + size/2
        // NOTE: Considering scale
        float hx = c.size.x * 0.5f * t.scale.x;
        float hy = c.size.y * 0.5f * t.scale.y;
        glm::vec2 center = glm::vec2(t.position.x, t.position.y) +
                           glm::vec2(c.offset.x, c.offset.y);
        glm::vec2 boxMin = center - glm::vec2(hx, hy);
        glm::vec2 boxMax = center + glm::vec2(hx, hy);

        // Ray vs AABB (Slab method)
        // Avoid div by zero
        glm::vec2 invDir;
        invDir.x = (std::abs(dir.x) < 1e-6f) ? 1e6f : 1.0f / dir.x;
        invDir.y = (std::abs(dir.y) < 1e-6f) ? 1e6f : 1.0f / dir.y;

        glm::vec2 t1 = (boxMin - origin) * invDir;
        glm::vec2 t2 = (boxMax - origin) * invDir;

        float tmin = std::max(
            0.0f, std::max(std::min(t1.x, t2.x), std::min(t1.y, t2.y)));
        float tmax = std::min(std::max(t1.x, t2.x), std::max(t1.y, t2.y));

        if (tmax >= tmin && tmin < closestDist) {
          closestDist = tmin;
          closestHit = origin + dir * tmin;
          hitFound = true;
        }
      });

  if (hitFound || closestDist < maxDist)
    return closestHit;
  return std::nullopt;
}

static std::vector<glm::vec2>
computeVisibilityPolygonECS(const glm::vec2 &origin, float angleCenter,
                            float fov, float radius, ECS::World &world) {
  std::vector<glm::vec2> points;
  points.push_back(origin);

  const int numRays = 100; // Adjust for quality vs perf
  float halfFov = fov * 0.5f;
  float startAngle = angleCenter - halfFov;

  for (int i = 0; i < numRays; ++i) {
    float angle = startAngle + fov * (float(i) / (numRays - 1));
    glm::vec2 dir = {std::cos(angle), std::sin(angle)};
    auto hit = raycastLightECS(origin, dir, radius, world);
    if (hit) {
      points.push_back(*hit);
    } else {
      points.push_back(origin + dir * radius);
    }
  }
  return points;
}

void Runtime::PlayState::renderTorchPass(Graphics::RenderingManager &renderer) {
  auto *scene = m_scenes.current();
  if (!scene)
    return;

  auto &world = scene->GetWorld();
  Graphics::Camera2D &camera = Core::Engine::Get().camera();

  world.each<Components::Transform, Components::Light>(
      [&](ECS::Entity entity, Components::Transform &trans,
          Components::Light &torch) {
        if (!torch.active)
          return;

        // Check battery if present
        float batteryFactor = 1.0f;
        auto *batt = world.getComponent<Components::Battery>(entity);
        if (batt) {
          batteryFactor = batt->charge / batt->maxCharge;
          if (batt->charge <= 0.0f)
            return;
        }

        // Calculate Torch World Position
        float angle = trans.rotation;

        vec2 torchOffsetLocal(torch.offset.x, torch.offset.y);
        vec2 torchOffsetWorld(torchOffsetLocal.x * std::cos(angle) -
                                  torchOffsetLocal.y * std::sin(angle),
                              torchOffsetLocal.x * std::sin(angle) +
                                  torchOffsetLocal.y * std::cos(angle));

        glm::vec2 originGLM =
            glm::vec2((trans.position.x + torchOffsetWorld.x),
                      (trans.position.y + torchOffsetWorld.y));

        // Use Raytracing for visibility
        float fovRad = glm::radians(torch.angle);
        std::vector<glm::vec2> lightPoly = computeVisibilityPolygonECS(
            originGLM, angle, fovRad, torch.radius, world);

        // Render Light Pass
        renderer.beginLightPass(camera.viewProj(), originGLM, torch.color,
                                torch.radius, angle, torch.angle, batteryFactor,
                                torch.intensity);
        // Note: drawLightCone expects polygon points
        renderer.drawLightCone(originGLM, lightPoly, torch.color);
        renderer.endLightPass();
      });
}
namespace Runtime {} // namespace Runtime
