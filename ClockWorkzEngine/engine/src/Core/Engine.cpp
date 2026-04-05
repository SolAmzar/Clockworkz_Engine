/******************************************************************************/
/*!
\file           Engine.cpp
\project        Lights Out
\author(s)      [Lebon], 80% [Wei An], 5% [Amzar] 15%
\brief          Declares Engine, the central orchestrator of the game loop.
                                Responsibilities include:
                                - Initializing and shutting down all subsystems
                                  (Config, Logger, Time, Window, Input,
Graphics, Renderer, Camera)
                                - Managing the GameStateManager and
bootstrapping the MenuState
                                - Running the main loop (update, render, timing,
input polling)
                                - Providing Run() as the entry point to start
the application

                                Serves as the core runtime that coordinates
gameplay, rendering, and platform-level services.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#include "Core/Engine.hpp"
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>
static _CrtMemState s_postLuaInitState;
static bool s_hasPostLuaInitState = false;
#endif
#include "ECS/World.hpp"
#include "Runtime/Scene.hpp"
#include "Assets/SoundManager.hpp"
#include "Assets/TextureManager.hpp"
#include "Audio/AudioManager.hpp"
#include "Audio/SurfaceAudioSystem.hpp"
#include "Core/Config.hpp"
#include "Core/EventBus.hpp"
#include "Core/Logger.hpp"
#include "Core/Profiler.hpp"
#include "Core/Time.hpp"
#include "Assets/FontManager.hpp"
#include "Assets/ResourceManager.hpp"
#include "Gameplay/Prefab.hpp"
#include "Graphics/Camera2D.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "Graphics/ImGuiManager.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Shader.hpp"
#include "Platform/GamepadManager.hpp"
#include "Platform/InputManager.hpp"
#include "Platform/LevelEditor.hpp"
#include "Platform/WindowManager.hpp"
#include "Runtime/GameState.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Scripting/LuaManager.hpp"
#include "Scripting/ScriptPropertyReader.hpp"
#include <GLFW/glfw3.h>
#include <exception>
#include <iostream>
#include <string>


using namespace Core;

namespace Core {
Engine *Engine::s_instance = nullptr;

#ifdef _DEBUG
const void* Engine::postLuaInitState() {
    return s_hasPostLuaInitState ? &s_postLuaInitState : nullptr;
}
void Engine::captureLeakCheckpoint() {
    _CrtMemCheckpoint(&s_postLuaInitState);
    s_hasPostLuaInitState = true;
}
#endif
Engine::Engine() {
  s_instance = this; // register singleton instance
  m_accuml = 0.0;
  m_pausedByFocus = false;
}

Engine::~Engine() {
  s_instance = nullptr; // clear singleton instance
  shutdown();
}

void Engine::setBootstrap(BootstrapFn fn) {
  m_bootstrap = std::move(fn);
}

void Engine::setEditorFactory(EditorFactory fn) {
  m_editorFactory = std::move(fn);
}

void Engine::setPlayFactory(PlayFactory fn) {
  m_playFactory = std::move(fn);
}

void Engine::setComponentRegistrar(ComponentRegistrar fn) {
  m_componentRegistrar = std::move(fn);
}

void Engine::invokeComponentRegistrar(ECS::World& world) const {
  if (m_componentRegistrar)
    m_componentRegistrar(world);
}

void Engine::setSceneSetup(SceneSetupFn fn) {
  m_sceneSetup = std::move(fn);
}

void Engine::setLuaExtensionRegistrar(LuaExtensionFn fn) {
  m_luaExtensionFn = std::move(fn);
}

void Engine::invokeSceneSetup(Runtime::Scene& scene) const {
  if (m_sceneSetup)
    m_sceneSetup(scene);
}

const std::string& Engine::startState() const {
  return m_config->startState;
}

Engine &Engine::Get() {
  assert(s_instance &&
         "Engine::Get() called before engine instance was created!");
  return *s_instance;
}

Audio::AudioManager &Engine::audio() {
  assert(m_audio &&
         "Engine::audio() called before AudioManager is initialized");
  return *m_audio;
}

Audio::AudioManager const &Engine::audio() const {
  assert(m_audio &&
         "Engine::audio() called before AudioManager is initialized");
  return *m_audio;
}

/** Function header comment
 * @brief Engine class private function
 *
 * XXX
 *
 * @param XXX
 *
 * @return XXX
 */
bool Engine::init() {
  m_config = std::make_unique<Config>();
  m_logger = std::make_unique<Logger>();
  m_time = std::make_unique<Time>();
  m_window   = std::make_unique<Platform::WindowManager>();
  m_input    = std::make_unique<Platform::InputManager>();
  m_gamepad  = std::make_unique<Platform::GamepadManager>();
  m_editor   = std::make_unique<Platform::LevelEditor>();
  m_gfx = std::make_unique<Graphics::GraphicsManager>();
  m_renderer = std::make_unique<Graphics::RenderingManager>();
  m_camera = std::make_unique<Graphics::Camera2D>();
  m_imgui = std::make_unique<Graphics::ImGuiManager>();
  m_lua = std::make_unique<Scripting::LuaManager>();

  m_config->loadFromFile("assets/configs/engine_config.json");
  m_logger->init();
  m_logger->info("Engine init...");

  m_audio = std::make_unique<Audio::AudioManager>();
  m_sfAudio = std::make_unique<Audio::SurfaceAudioSystem>();

  if (!m_audio->init()) {
    std::cerr << "Failed to load AudioManager" << "\n";
    return false;
  }
  // m_sfAudio->init(*m_audio);

  if (!m_window->create(*m_config))
    return false;
  m_input->init(*m_window);
  m_gamepad->init(m_config->gamepadDeadzone, m_config->gamepadSprintThresh);
  if (!m_gfx->init(*m_window))
    return false;

  Assets::SoundManager::Init(*m_audio);

  if (!Assets::TextureManager::LoadFromConfig("assets/configs/Textures.json")) {
    m_logger->error("Some textures failed to load from config. "
                    "Game will still run; check log for details.");
    // Do NOT return false here — missing textures are non-fatal.
  }

  if (!Assets::SoundManager::LoadFromConfig("assets/configs/sounds.json")) {
    m_logger->error("Some sounds failed to load from sounds.json. "
                    "Game will still run; check log for details.");
    // Do NOT return false here.
  }

  // 1️ Register all prefab blueprints
  const std::string prefabFile = "assets/configs/Prefab.json";
  if (!Gameplay::PrefabFactory::get().loadPrefabFromFile(prefabFile)) {
    std::cerr << "Failed to load prefab from file: " << prefabFile << "\n";
    return false;
  }

  /* */
  if (!m_renderer->init())
    return false;

  if (!m_imgui->init(m_window->handle()))
    return false;

  // m_camera->setViewport((float)m_window->width(), (float)m_window->height());
  m_camera->init((int)m_window->width(), (int)m_window->height());

  m_lua->Initialize();
  if (m_luaExtensionFn) m_luaExtensionFn(*m_lua);

  m_gsm = std::make_unique<Runtime::GameStateManager>();

  /*
          m_audio->load({"ui_move", "assets/audio/ui_move.wav", false, false,
     false }); m_audio->load({"ui_select", "assets/audio/ui_select.wav", false,
     false, false });

          // BGM Test but it's just random royalty-free music atm
          m_audio->load({"menu_bgm", "assets/audio/menu_bgm.mp3", true, true,
     false});
          */
  m_running = true;
  events().subscribe<QuitGameEvent>(this, [this](const QuitGameEvent &) {
    this->m_quitRequested = true;
    this->m_quitFadeTimer = 0.5f;
    this->audio().fadeOutActive(0.5f);
  });

  events().subscribe<WindowFocusLostEvent>(
      this, [this](const WindowFocusLostEvent &) {
        // pause audio & gameplay
        this->audio().setGroupPaused(Audio::Group::BGM, true);
        this->audio().setGroupPaused(Audio::Group::SFX, true);
        this->m_pausedByFocus = true;
      });

  events().subscribe<WindowFocusGainedEvent>(
      this, [this](const WindowFocusGainedEvent &) {
        // resume audio
        this->audio().setGroupPaused(Audio::Group::BGM, false);
        this->audio().setGroupPaused(Audio::Group::SFX, false);
        this->m_pausedByFocus = false;
      });
  return true;
}

void Engine::shutdown() {
  if (m_shutdownCalled) return;
  m_shutdownCalled = true;
  std::cout << "Shutdown started" << std::endl;
  if (m_logger)
    m_logger->info("Engine shutdown.");

  // 1. Cleanup Game States first (they use ImGui in renderImGui/onExit)
  if (m_gsm) {
    while (m_gsm->top()) {
      m_gsm->pop();
      m_gsm->update(0.0);
    }
    m_gsm.reset();
  }

  // 2. Destroy ImGui AFTER all states are gone
  if (m_imgui) {
    m_imgui->shutdown();
    m_imgui.reset();
  }

  // 3. Camera must die BEFORE renderer + graphics
  if (m_camera) {
    m_camera.reset();
  }

  // 4. Free GPU-backed resources while the OpenGL context is still valid
  Assets::ResourceManager<Graphics::Shader>::Clear();
  Assets::ResourceManager<Assets::FontManager>::Clear();

  // 5. RenderingManager (OpenGL buffers)
  if (m_renderer) {
    m_renderer.reset();
  }

  // 6. GraphicsManager (viewport, GL state)
  if (m_gfx) {
    m_gfx.reset();
  }

  // 7. Gamepad + Input must die BEFORE window
  if (m_gamepad) {
    m_gamepad->shutdown();
    m_gamepad.reset();
  }
  if (m_input) {
    m_input.reset();
  }

  // 8. WindowManager (destroys GLFW window + context)
  if (m_window) {
    m_window.reset();
  }

  // 9. Audio shutdown
  if (m_audio) {
    Assets::SoundManager::Clear();
    m_audio.reset();
  }
  if (m_sfAudio) {
    m_sfAudio.reset();
  }

  // 10. Lua Shutdown
  Scripting::ScriptPropertyReader::clearAll();
  if (m_lua) {
    m_lua.reset();
  }

  // 11. Global Caches and Factories
  Assets::TextureManager::Clear();
  Gameplay::PrefabFactory::get().clear();

  // 12. Explicitly clear callback lambdas to release any captured shared_ptrs
  m_bootstrap = nullptr;
  m_editorFactory = nullptr;
  m_playFactory = nullptr;
  m_componentRegistrar = nullptr;
  m_sceneSetup = nullptr;

  // 13. Destroy Profiler — deletes the singleton so its unordered_map memory
  //     is freed before _CrtDumpMemoryLeaks() runs in main().
  Core::Profiler::Destroy();

  // 14. Basic engine services (safe anywhere below)
  m_time.reset();
  m_logger.reset();
  m_config.reset();

  std::cout << "Shutdown done" << std::endl;
}

/** Function header comment
 * @brief Engine::update(double dt) is called once per frame in the game loop.
 *
 * It updates all the engine systems and ensures the initial game state is set
 * up.
 *
 * @param XXX
 *
 * @return XXX
 */
void Engine::update(double dt) {
  if (m_quitRequested) {
    m_quitFadeTimer -= static_cast<float>(dt);
    if (m_quitFadeTimer <= 0.0f) {
      m_running = false;
      m_window->requestClose();
    }
  }
  // Check if Fixed Delta time (Uncomment if you want to check)
  /*std::cout << m_time->fixedDelta() << std::endl;*/

  // Initial state is pushed inside Run() via the bootstrap factory.
  m_audio->update(dt); // Update the audio manager class
  m_input->update();   // Update the input manager class
  m_gamepad->update(); // Pump SDL gamepad events + snapshot state
  if (m_input->wasKeyPressed(GLFW_KEY_F11)) {
    m_window->toggleFullscreen();
  }

  m_gsm->update(dt);
  // int steps = m_time->currentSteps();
  // for (int i = 0; i < steps; ++i) {
  //	m_gsm->update(m_time->fixedDelta());
  // }
}

/** Function header comment
 * @brief
 *
 * XXX
 *
 * @param XXX
 *
 * @return XXX
 */
void Engine::render() {
  // clear & present
  // (Clear happens inside RenderingManager::beginFrame)
  m_gsm->render(m_time.get());

  if (m_renderer && m_camera) {
    /*
    m_renderer->drawCircle(400.0f, 300.0f, 100.0f, 0.0f, 0.0f, 1.0f, 64);

    // Draw a red rectangle at (200, 150) with width 300 and height 200
    m_renderer->drawRect(200.0f, -300.0f, 300.0f, 200.0f, 1.0f, 0.0f, 0.0f);
    */
  }
}

/** Function header comment
 * @brief Engine class public function
 *
 * XXX
 *
 * @param XXX
 *
 * @return XXX
 */
int Engine::Run() {

  // Call the engine private function `init()`
  try {
    if (!init())
      return -1;

    // Push the initial game state via the bootstrap factory registered by main().
    if (m_bootstrap)
      m_gsm->push(m_bootstrap(*this));

    // `m_running` is a private data member of type bool, and was set to true by
    // `init()`.

    // `m_window` is a WindowManager class, and `shouldClose()` seems to be a
    // wrapper? not sure if it is the right term, i dont really understand
    // wrapper. [Done]
    // Q: What is a wrapper?
    // A: A wrapper is a function or class that “wraps around” another function,
    // library, or system to make it easier, safer, or cleaner to use. It hides
    // complexity of the underlying system. It can change the interface to be
    // more convenient or type - safe. The rest of your code doesn’t need to
    // know about the internal details.

    // `m_` seems to be a prefix for manager, not sure what it is suppose to
    // mean? [Done] Q: What does the m_ prefix mean ? A : In C++, m_ is a
    // convention for member variables. m_ stands for member. It tells you the
    // variable belongs to the class instance, not a local variable or
    // parameter. It has nothing to do with “manager”, even if it looks like
    // that.

    // Q: Is m_window->shouldClose() calling itself recursively?
    // A: No.Engine::m_window is a WindowManager*, and
    // WindowManager::shouldClose() calls glfwWindowShouldClose() on its GLFW
    // window member. They just happen to share the name m_window.
    while (m_running && !m_window->shouldClose()) {

      // PROFILE_SCOPE("Frame");

      {
        PROFILE_SCOPE("FrameSetup");
        m_time->beginFrame();
        m_window->poll();
      }

      {
        PROFILE_SCOPE("Update");
        if (!m_pausedByFocus) {
          update(m_time->delta());
        } else {
          // update(0.f);
        }
      }

      /*
                              m_renderer->beginFrame(*m_camera);
                              m_imgui->beginFrame();*/
      {
        PROFILE_SCOPE("Full Render");
        m_renderer->beginFrame();
        /*std::cout << "Core Render Begin: " << std::endl;*/
        render();
        /*std::cout << "Core Render End " << std::endl;*/
        m_renderer->endFrame();
      }
      /*m_imgui->endFrame();
      m_renderer->endFrame();*/

      {
        PROFILE_SCOPE("SwapBuffers");
        m_window->swap();
      }

      m_time->endFrame();

      m_accuml += m_time->delta();
      if (m_accuml >= 0.5) {
        m_accuml = 0.0;
        std::string title = "Lights Out";
#ifdef _DEBUG
        double fps = m_time->fps();
        double frameTime = m_time->frameTimeMs();
        title = "Lights Out  |  FPS: " + std::to_string((int)fps) + "  (" +
                std::to_string(frameTime).substr(0, 5) + " ms)";
#endif
        m_window->setTitle(title);
      }
    }
    std::cout << "Exiting main loop" << std::endl;
    shutdown();
    return 0;
  } catch (const std::exception &e) {
    // Log fatal exceptions with crash report
    if (m_logger) {
      m_logger->fatal("Fatal exception caught: " + std::string(e.what()));
    } else {
      std::cerr << "[FATAL] Exception before logger initialized: " << e.what()
                << std::endl;
    }

    shutdown();
    return -1;
  } catch (...) {
    // Log unknown fatal exceptions
    if (m_logger) {
      m_logger->fatal("Unknown fatal exception caught");
    } else {
      std::cerr << "[FATAL] Unknown exception before logger initialized"
                << std::endl;
    }

    shutdown();
    return -1;
  }
}
} // namespace Core
