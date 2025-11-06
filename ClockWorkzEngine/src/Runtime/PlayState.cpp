/******************************************************************************/
/*!
\file           PlayState.cpp
\project        Lights Out
\author(s)      [Lebon], 75% [Syafiq] 15%
\brief          Declares PlayState, the main gameplay loop state.
				Responsibilities include:
				- Loading and managing the current Scene and its GameObjects
				- Updating gameplay logic such as movement, collisions, and AI
				- Rendering the game world each frame (Game Object Render not done yet)
				- Transitioning back to other states (e.g., pause or menu)

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "Runtime/SceneManager.hpp"
#include "Runtime/Scene.hpp"
#include "Gameplay/GameObject.hpp"
#include "IMGUI/imgui.h"
#include "Runtime/MenuState.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Platform/InputManager.hpp"
#include "Platform/WindowManager.hpp"
#include "Runtime/PlayState.hpp"
#include "Core/AudioManager.hpp"
#include <GLFW/glfw3.h>
#include "Assets/FontManager.hpp"
#include "Assets/ResourceManager.hpp"
#include "Graphics/Shader.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <memory>
#include <Core/Time.hpp>
#include <Graphics/Renderer2D.hpp>
#include <Graphics/Camera2D.hpp>
#include <Graphics/GraphicsManager.hpp>
#include <Core/Engine.hpp>
#include <Gameplay/Prefab.hpp>
#include "Core/Profiler.hpp"
#include "Graphics/RenderingManager.hpp"
#include <Platform/LevelEditorState.hpp>
#include <Gameplay/Components/SpriteRenderer.hpp>
#include <unordered_map>
#include <vector>
#include <string>

using namespace Graphics;
using namespace Assets;

namespace Runtime
{

	std::unordered_map<std::string, std::vector<glm::vec4>> playerSprite;
	const float changeFrameTime = 0.3;
	float frameTimer = 0.f;
	size_t frameCount = 0;

	void ResetFrame()
	{
		float frameTimer = 0.f;
		size_t frameCount = 0;
	}

	void InitPlayerSpriteUVs()
	{
		const float frameW = 1.0f / 8.0f; // 0.125
		const float frameH = 1.0f / 8.0f; // 0.125

		for (int i = 1; i <= 8; ++i)
		{
			glm::vec4 uv = {frameW * 3.f, 1.0 - (i * frameH), frameW, frameH};
			// for example group rows by animation name
			playerSprite["walk"].push_back(uv);
		}

		for (int i = 1; i <= 4; ++i)
		{
			glm::vec4 uv = {frameW * 4.f, 1.0 - (i * frameH), frameW, frameH};
			// for example group rows by animation name
			playerSprite["idle"].push_back(uv);
		}

		for (int i = 1; i <= 6; ++i)
		{
			glm::vec4 uv = {frameW * 5.f, 1.0 - (i * frameH), frameW, frameH};
			// for example group rows by animation name
			playerSprite["run_torch"].push_back(uv);
		}

		for (int i = 1; i <= 8; ++i)
		{
			glm::vec4 uv = {frameW * 6.f, 1.0 - (i * frameH), frameW, frameH};
			// for example group rows by animation name
			playerSprite["walk_torch"].push_back(uv);
		}

		for (int i = 1; i <= 6; ++i)
		{
			glm::vec4 uv = {frameW * 7.f, 1.0 - (i * frameH), frameW, frameH};
			// for example group rows by animation name
			playerSprite["run"].push_back(uv);
		}
	}
	PlayState::PlayState(std::string initialSceneId, Platform::InputManager &input, GameStateManager &gsm)
			: m_initialSceneId(std::move(initialSceneId)), m_input(input), m_gsm(gsm)
	{
	}

	const char *PlayState::name() const { return "PlayState"; }

	static const char *toString(Gameplay::GameObjectType t)
	{
		using T = Gameplay::GameObjectType;
		switch (t)
		{
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
		default:
			return "Unknown";
		}
	}

	void PlayState::onEnter()
	{
		std::cout << "[PlayState] Enter, loading scene: " << m_initialSceneId << "\n";

		// Syafiq -
		Core::Engine &engine = Core::Engine::Get();
		m_renderer = &engine.rendering();
		InitPlayerSpriteUVs();
		auto textureShader = ResourceManager<Shader>::Load(
				"texture",
				"assets/shaders/texture.vert",
				"assets/shaders/texture.frag");

		ResourceManager<Shader>::Load(
				"light",
				"assets/shaders/light.vert",
				"assets/shaders/light.frag");
		if (m_scenes.loadScene(m_initialSceneId))
		{
			m_loaded = true;

			// Print all GameObjects created by the loaded scene
			if (auto *scene = m_scenes.current())
			{
				auto &objs = scene->objects();
				std::cout << "GameObjects created (" << objs.size() << "):\n";
				for (auto &u : objs)
				{
					auto const &o = *u;
					auto p = o.transform().position;
					auto s = o.size();
					std::cout << " - id=" << o.id()
										<< " type=" << toString(o.type())
										<< " name=\"" << o.name() << "\""
										<< " pos=(" << p.x << "," << p.y << ")"
										<< " size=(" << s.x << "," << s.y << ")"
										<< (o.active() ? "" : " [inactive]") << "\n";

					if (u->type() == Gameplay::GameObjectType::Player)
					{
						m_player = &(*u);
						Gameplay::SpriteRendererComponent *playerSprite = u->getComponent<Gameplay::SpriteRendererComponent>();
						if (playerSprite)
						{

							playerSprite->setUV(glm::vec4{0, 0.875, 0.125, 0.125});
						}
					}
				}
				std::cout.flush();
			}
		}
		else
		{
			std::cout << "[PlayState] Scene load FAILED.\n";
			std::cout << "[PlayState] Returning to menu...\n";
			m_shouldReturnToMenu = true; // Returns to menu
		}
	}

	void PlayState::onExit()
	{
		// Only unload if a scene was actually loaded successfully
		if (m_loaded)
		{
			m_scenes.unloadCurrent();
		}
		m_loaded = false;
	}

	void PlayState::update(double dt)
	{

		Core::Engine &engine = Core::Engine::Get();
		Platform::InputManager &input = engine.input();
		Graphics::Camera2D &cam = engine.camera();
		Platform::WindowManager &window = engine.window();
		vec2 mouseWorld = m_input.getMouseWorldPos(
				cam,
				static_cast<float>(window.width()),
				static_cast<float>(window.height()));

		if (m_shouldReturnToMenu)
		{
			m_gsm.pop();
			return;
		}

		if (!m_loaded)
			return;

		if (m_input.wasKeyPressed(GLFW_KEY_ESCAPE))
		{
			m_gsm.pop();
			return;
		}

		if (m_input.wasKeyPressed(GLFW_KEY_P))
		{
			m_debug = !m_debug;
			if (m_debug)
			{
				std::cout << "Debug is pressed\n";
			}
			return;
		}

		if (m_input.wasKeyPressed(GLFW_KEY_L))
		{
			m_spawnonclick = !m_spawnonclick;
			std::cout << "Spawn on click is "
								<< (m_spawnonclick ? "enabled" : "disabled")
								<< std::endl;
			return;
		}

		// Havign issue with the camera
		if (m_input.wasKeyPressed(GLFW_KEY_R))
		{
			m_flashlightOn = !m_flashlightOn;
			std::cout << "Flashlight " << (m_flashlightOn ? "ON" : "OFF") << "\n";
		}

		if (m_input.wasKeyPressed(GLFW_KEY_Z))
		{
			m_gsm.push(std::make_shared<EditorState>(m_input, m_gsm));
		}

		if (m_input.wasMouseClicked(GLFW_MOUSE_BUTTON_LEFT) && m_spawnonclick)
		{
			mouseWorld = m_input.getMouseWorldPos(cam,
																						static_cast<float>(window.width()),
																						static_cast<float>(window.height()));

			bool wallFound = false; // flag

			// Check for an existing wall
			for (auto it = gameObjects.begin(); it != gameObjects.end();)
			{
				auto &obj = *it;
				if (obj->type() == Gameplay::GameObjectType::Wall)
				{
					vec2 pos = obj->transform().position;
					vec2 size = obj->size();
					if (mouseWorld.x >= pos.x - size.x / 2 && mouseWorld.x <= pos.x + size.x / 2 &&
							mouseWorld.y >= pos.y - size.y / 2 && mouseWorld.y <= pos.y + size.y / 2)
					{
						// Found wall, delete it
						std::cout << "Deleted " << toString(obj->type()) << " at (" << pos.x << ", " << pos.y << ")\n";
						it = gameObjects.erase(it);
						wallFound = true;
						break; // Only delete one wall per click
					}
					else
					{
						++it;
					}
				}
				else
				{
					++it;
				}

				if (!wallFound)
				{
					// If no wall was found, spawn one
					vec2 wallSize{100.f, 100.f};
					auto wall = Gameplay::PrefabFactory::get().create("Wall");
					if (wall)
					{
						wall->setPosition(mouseWorld);
						wall->setSize(wallSize);
						gameObjects.push_back(std::move(wall));
						std::cout << "Spawned Wall at (" << mouseWorld.x << ", " << mouseWorld.y << ")\n";
					}
				}
			}
		}

		// function to delete objects on right click
		if ((m_input.wasMouseClicked(GLFW_MOUSE_BUTTON_LEFT) && !m_spawnonclick))
		{
			for (auto it = gameObjects.begin(); it != gameObjects.end();)
			{
				auto &obj = *it;
				// Simple AABB check
				vec2 pos = obj->transform().position;
				vec2 size = obj->size();
				if (mouseWorld.x >= pos.x - size.x / 2 && mouseWorld.x <= pos.x + size.x / 2 &&
						mouseWorld.y >= pos.y - size.y / 2 && mouseWorld.y <= pos.y + size.y / 2)
				{
					std::cout << "Deleted " << toString(obj->type()) << " at (" << pos.x << ", " << pos.y << ")\n";
					it = gameObjects.erase(it); // Remove object and get new iterator
				}
				else
				{
					++it; // Only increment if not erased
				}
			}
		}

		// Syafiq - Player movement
		if (auto *scene = m_scenes.current())
		{

			// Syafiq - need find a way to cache the player.or do a unordered_map with object unique name to array of objects.
			// objMap["Player"][0] <- since only 1 player. index 0 will work.
			for (auto &obj : scene->objects())
			{

				if (obj->type() == Gameplay::GameObjectType::Player)
				{
					float moveSpeed = 100.0f;
					if (m_input.isKeyDown(GLFW_KEY_LEFT_SHIFT))
					{
						moveSpeed *= 2;
					}
					//float moveAmount = moveSpeed * static_cast<float>(dt);

					if (m_input.isKeyDown(GLFW_KEY_W))
					{
						obj->velocity().y = moveSpeed;
					}
					else if (m_input.isKeyDown(GLFW_KEY_S))
					{
						obj->velocity().y = -moveSpeed;
					}
					else
					{
						obj->velocity().y = 0;
					}

					if (m_input.isKeyDown(GLFW_KEY_A))
					{
						obj->velocity().x = -moveSpeed;
					}
					else if (m_input.isKeyDown(GLFW_KEY_D))
					{
						obj->velocity().x = moveSpeed;
					}
					else
					{
						obj->velocity().x = 0;
					}

					if (m_input.isKeyDown(GLFW_KEY_G))
					{
						obj->transform().scale.x += static_cast<f32>(10 * dt);
						obj->transform().scale.y += static_cast<f32>(10 * dt);
					}

					vec2 mouseWorld = input.getMouseWorldPos(cam, static_cast<float>(window.width()), static_cast<float>(window.height()));

					// Player world position
					vec2 playerPos = obj->transform().position;

					// Direction vector from player to mouse
					vec2 dir = mouseWorld - playerPos;

					// Distance check to prevent jitter
					float dist = magnitude(dir);
					if (dist > 10.0f)
					{ // 10 units = threshold distance
						// Normalize direction
						dir = normalize(dir);

						// Compute angle in radians — atan2(y, x)
						float angleRad = std::atan2(dir.y, dir.x);

						// Convert to degrees if your transform uses degrees
						obj->transform().rotationRad = angleRad;
					}
					if (obj->type() == Gameplay::GameObjectType::Player && !obj->active())
					{
						obj->respawnAt(vec2(0.0f, 0.0f), true, true);
					}

					// --- Player Animation Control ---
					Gameplay::SpriteRendererComponent *playerSpriteComp = obj->getComponent<Gameplay::SpriteRendererComponent>();
					if (playerSpriteComp)
					{
						// Determine animation state
						std::string animState = "idle";

						bool isMoving = (obj->velocity().x != 0 || obj->velocity().y != 0);

						if (isMoving)
						{
							if (m_flashlightOn)
							{
								animState = "walk_torch";
							}
							else
							{
								animState = "walk";
							}

							// You can decide if shift key means run
							if (m_input.isKeyDown(GLFW_KEY_LEFT_SHIFT))
							{
								animState = m_flashlightOn ? "run_torch" : "run";
							}
						}

						// Update animation frame timer
						frameTimer += static_cast<float>(dt);
						if (frameTimer >= changeFrameTime)
						{
							frameTimer = 0.0f;
							frameCount++;
						}

						// Get current animation frames safely
						auto it = playerSprite.find(animState);
						if (it != playerSprite.end() && !it->second.empty())
						{
							const auto &frames = it->second;
							frameCount %= frames.size(); // wrap around
							playerSpriteComp->setUV(frames[frameCount]);
						}
					}
				}

				if (obj->type() == Gameplay::GameObjectType::Ball)
				{
					obj->velocity().y = 0.0f;
					obj->velocity().x += m_ballDir * m_ballSpeed;

					// Predict next X and flip if we would cross a bound
					float nextX = obj->transform().position.x + obj->velocity().x * static_cast<float>(dt);
					if (nextX < m_leftBound)
					{
						nextX = m_leftBound;
						m_ballDir = +1;
					}
					if (nextX > m_rightBound)
					{
						nextX = m_rightBound;
						m_ballDir = -1;
					}

					obj->transform().position.x = nextX; // snap to keep within bounds (optional)
				}
			}
		}

		if (m_input.isKeyDown(GLFW_KEY_Z))
		{
			m_gsm.push(std::make_shared<EditorState>(m_input, m_gsm));
		}
		if (m_input.isKeyDown(GLFW_KEY_Q))
		{
			cam.setZoom(cam.zoom() * 1.02f);
		}
		if (m_input.isKeyDown(GLFW_KEY_E))
		{
			cam.setZoom(cam.zoom() / 1.02f);
		}

		if (m_spawning && m_spawnedCount < 2500)
		{
			auto &factory = Gameplay::PrefabFactory::get();
			auto currentScene = m_scenes.currentScene();

			if (currentScene)
			{
				int toSpawn = std::min(SPAWN_PER_FRAME, 2500 - m_spawnedCount);

				for (int i = 0; i < toSpawn; ++i)
				{
					int gridSize = 50;
					float spacing = 50.f;
					int idx = m_spawnedCount + i;
					int row = idx / gridSize;
					int col = idx % gridSize;

					float x = -gridSize * spacing / 2.0f + col * spacing;
					float y = -gridSize * spacing / 2.0f + row * spacing;

					auto obj = factory.createAt("Wall", vec2(x, y));
					if (obj)
					{
						currentScene->objects().push_back(std::move(obj));
					}
				}

				m_spawnedCount += toSpawn;
				if (m_spawnedCount >= 2500)
				{
					m_spawning = false;
					std::cout << "Finished spawning 2500 objects!\n";
				}
			}
		}
		m_scenes.update(dt);
	}

	void PlayState::render(Core::Time *timer)
	{
		if (!m_loaded || !m_renderer)
			return;

		Core::Engine &engine = Core::Engine::Get();

		// Use SceneManager's render to render the current scene
		// --- Pass: Scene ---
		// Batch rendering
		m_renderer->beginScenePass(engine.camera().viewProj());
		m_scenes.render(*m_renderer);
		m_renderer->endScenePass();

		// ==================== Torch Light Rendering ====================
		auto *scene = m_scenes.current();
		if (!scene)
			return;

		auto *physics = scene->getPhysics();
		if (m_flashlightOn && physics && m_player && m_player->active())
		{

			vec2 origin = m_player->transform().position;
			float angle = m_player->transform().rotationRad;
			std::vector<std::unique_ptr<Gameplay::GameObject>> &objs = scene->objects();
			std::vector<vec2> lightPoly = m_renderer->computeVisibilityPolygon(
					origin, angle, glm::radians(60.0f), 300.0f, *physics, objs);
			glm::vec2 originGLM = glm::vec2(origin.x, origin.y);
			std::vector<glm::vec2> lightPolyGLM;
			for (const auto &point : lightPoly)
			{
				lightPolyGLM.emplace_back(point.x, point.y);
			}

			m_renderer->beginLightPass(engine.camera().viewProj(), originGLM, glm::vec3(1.0f, 0.9f, 0.6f), 300.0f, angle, glm::radians(60.0f));
			m_renderer->drawLightCone(originGLM, lightPolyGLM, {1.0f, 0.9f, 0.6f});
			m_renderer->endLightPass();

			// need refactor, something like EntityManager.GetAll<PlayerComponent>();
			// Another way is to cache the player gameobject when LoadScene from file.
			/*for (auto& o : objs)
			{
				if (o->type() == Gameplay::GameObjectType::Player)
				{

					vec2 origin = o->transform().position;
					float angle = o->transform().rotationRad;
					std::vector<vec2> lightPoly = m_renderer->computeVisibilityPolygon(
						origin, angle, glm::radians(60.0f), 300.0f, *physics, objs);
						glm::vec2 originGLM = glm::vec2(origin.x, origin.y);
						std::vector<glm::vec2> lightPolyGLM;
						for (const auto& point : lightPoly) {
							lightPolyGLM.emplace_back(point.x, point.y);
						}


						m_renderer->beginLightPass(engine.camera().viewProj(), originGLM, glm::vec3(1.0f, 0.9f, 0.6f), 300.0f, angle, glm::radians(60.0f));
						m_renderer->drawLightCone(originGLM, lightPolyGLM, { 1.0f, 0.9f, 0.6f });
						m_renderer->endLightPass();
						break;
				}
			}*/
		}

		// std::vector<std::unique_ptr<Gameplay::GameObject>>& objs = scene->objects();
		// m_renderer->beginDebugPass(engine.camera().viewProj());
		// for (auto& o : objs)
		//{
		//	if (o->active())
		//		o->debugRender(*m_renderer);
		// }
		// m_renderer->endDebugPass();
	}

	// test for IMGUI (still broken)
	const char *defaultTextures[] = {
			"player_texture",
			"enemy.png",
			"assets/textures/wall.png",
			"assets/textures/floor.png",
			"assets/textures/trap.png"};

	void PlayState::renderImGui(Core::Time *timer)
	{
		if (!m_debug)
			return;

		if (m_debug)
		{
			ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);
			ImGui::Begin("Game State", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

			// ******* Object Creation **********
			static int selectedType = 0;
			const char *types[] = {"Player", "Enemy", "Wall", "Floor", "Trap"};
			ImGui::Combo("Type", &selectedType, types, IM_ARRAYSIZE(types));

			static float pos[2] = {0, 0};
			static float size[2] = {100, 100};
			ImGui::InputFloat2("Position", pos);
			ImGui::InputFloat2("Size", size);

			static int hp = 10, damage = 1;
			static float ms = 100.0f;
			if (selectedType == 0 || selectedType == 1) // if its enemy or player, set health
				ImGui::InputInt("HP", &hp);
			if (selectedType == 1 || selectedType == 4) // if its enemy or trap, set damage
				ImGui::InputInt("Damage", &damage);

			static char texture[64] = "";
			ImGui::InputText("Texture", texture, IM_ARRAYSIZE(texture));

			if (ImGui::Button("Create Object"))
			{

				const char *texToUse = texture[0] ? texture : defaultTextures[selectedType];
				std::unique_ptr<Gameplay::GameObject> obj = nullptr;

				// Supposedly spawn the corresponding gameobject (texture and logic not fixed)
				if (selectedType == 0)
				{
					obj = Gameplay::PrefabFactory::get().create("Player");
					if (obj)
					{
						obj->setMoveSpeed(ms);
						obj->setPosition(vec2(pos[0], pos[1]));
						obj->setSize(vec2(size[0], size[1]));
						obj->autoUpdateCollisionBaseOnScale();
						obj->setTextureId(texToUse);
						auto scene = m_scenes.current();
						if (scene)
						{
							scene->objects().push_back(std::move(obj));
						}
						std::cout << "Player created\n";
					}
				}
				else if (selectedType == 1)
				{
					obj = Gameplay::PrefabFactory::get().create("Enemy");
					if (obj)
					{
						obj->setMoveSpeed(ms);
						obj->setPosition(vec2(pos[0], pos[1]));
						obj->setSize(vec2(size[0], size[1]));
						obj->autoUpdateCollisionBaseOnScale();
						auto scene = m_scenes.current();
						if (scene)
						{
							scene->objects().push_back(std::move(obj));
						}
						std::cout << "Enemy created\n";
					}
				}
				else if (selectedType == 2)
				{
					obj = Gameplay::PrefabFactory::get().create("Wall");
					if (obj)
					{
						obj->setPosition(vec2(pos[0], pos[1]));
						obj->setSize(vec2(size[0], size[1]));
						obj->autoUpdateCollisionBaseOnScale();
						auto scene = m_scenes.current();
						if (scene)
						{
							scene->objects().push_back(std::move(obj));
						}
						std::cout << "Wall created\n";
					}
				}
				else if (selectedType == 3)
				{
					obj = Gameplay::PrefabFactory::get().create("Floor");
					if (obj)
					{
						obj->setPosition(vec2(pos[0], pos[1]));
						obj->setSize(vec2(size[0], size[1]));
						auto scene = m_scenes.current();
						if (scene)
						{
							scene->objects().push_back(std::move(obj));
						}
						std::cout << "Floor created\n";
					}
				}
				else if (selectedType == 4)
				{
					obj = Gameplay::PrefabFactory::get().create("Trap");
					if (obj)
					{
						obj->setPosition(vec2(pos[0], pos[1]));
						obj->setSize(vec2(size[0], size[1]));
						gameObjects.push_back(std::move(obj));
					}
				}
			}

			if (ImGui::Button("Delete Object"))
			{
				auto scene = m_scenes.current();
				if (!scene->objects().empty())
				{
					scene->objects().pop_back();
				}
			}
			ImGui::Text("Object counter: %d ", m_scenes.current()->objects().size());

			ImGui::Separator();

			// ******* Testing Window **********
			ImGui::Text("Testing Window");

			if (timer)
			{
				double fps = timer->fps();
				;
				double frameTime = timer->delta() * 1000.0;

				ImGui::Text("FPS: %.1f", fps);
				ImGui::Text("Frame Time: %.2f ms", frameTime);
			}
			ImGui::Separator();

			ImGui::Text("Performance Profiler (Live)");
			ImGui::Spacing();

			auto &profiler = Core::Profiler::Get();
			const auto &data = profiler.data();

			if (data.empty())
			{
				ImGui::TextDisabled("No profiling data recorded yet...");
			}
			else
			{
				for (const auto &[scope, info] : data)
				{
					double avg = info.avgTime();
					double total = info.totalTime;
					int count = info.callCount;

					// Color code performance (green=fast, red=slow)
					ImVec4 color;
					if (avg < 1.0)
						color = ImVec4(0.3f, 1.0f, 0.3f, 1.0f); // fast (<1ms)
					else if (avg < 8.0)
						color = ImVec4(1.0f, 1.0f, 0.3f, 1.0f); // moderate
					else
						color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); // slow

					ImGui::TextColored(color, "%-18s | Avg: %.3f ms | Calls: %d | Total: %.2f ms",
														 scope.c_str(), avg, count, total);
				}
			}

			ImGui::Spacing();

			/*if (ImGui::Button("Show Performance Profile"))
			{
				Core::Profiler::Get().print();
			}*/

			if (ImGui::Button("Reset Profiler"))
			{
				Core::Profiler::Get().reset();
			}

			ImGui::Checkbox("Activate OnClickSpawn", &m_spawnonclick);

			// Add button to spawn 2500 objects
			if (ImGui::Button("Spawn 2500 Objects"))
			{
				// Get the current scene using the existing method
				auto &factory = Gameplay::PrefabFactory::get();
				auto currentScene = m_scenes.currentScene(); // Returns std::shared_ptr<Scene>
				if (currentScene)
				{
					// Spawn objects in a grid pattern
					int gridSize = 50; // 50x50 = 2500 objects
					float spacing = 20.f;
					float startX = -gridSize * spacing / 2.0f;
					float startY = -gridSize * spacing / 2.0f;

					for (int i = 0; i < gridSize; ++i)
					{
						for (int j = 0; j < gridSize; ++j)
						{
							float x = startX + i * spacing;
							float y = startY + j * spacing;

							// Create an object using your prefab system
							auto obj = factory.createAt("Floor", vec2(x, y));
							if (obj)
							{
								currentScene->objects().push_back(std::move(obj));
							}
						}
					}
					std::cout << "Spawned 2500 objects!\n";
				}
			}

			if (ImGui::Button("Spawn 2500 Objects Gradually"))
			{
				m_spawning = true;
				m_spawnedCount = 0;
			}

			if (ImGui::Button("Back to Menu (ESC)"))
			{
				m_gsm.pop();
			}

			if (ImGui::Button("Reset Scene"))
			{
				resetScene();
			}

			ImGui::End();
		}

		// ImDrawList* dl = ImGui::GetForegroundDrawList();

		// const ImVec2 center{ 640.0f, 360.0f };
		// const float radius = 60.0f;
		// const int segments = 48; // More segments = smoother circle
		// const float thickness = 3.0f;

		// dl->AddCircle(center, radius, IM_COL32(255, 0, 0, 255), segments, thickness);
	}

	void PlayState::resetScene()
	{
		if (m_scenes.reloadCurrent())
		{
			std::cout << "[PlayState] Scene reset Success\n";
		}
		else
		{
			m_scenes.loadScene(m_initialSceneId);
		}
	}

} // namespace Runtime
