/******************************************************************************/
/*!
\file           SpriteState.cpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          

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
#include "Runtime/SpriteState.hpp"
#include "Audio/AudioManager.hpp"
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
#include "Gameplay/Components/chase.hpp"
#include <unordered_map>
#include <vector>
#include <string>
#include <Gameplay/Components/SpriteAnimator.hpp>
#include <filesystem>
#include <Assets/TextureManager.hpp>
namespace fs = std::filesystem;

// Website to see how sprite work
// https://codeshack.io/images-sprite-sheet-generator/
// https://ezgif.com/sprite-cutter/ezgif-4d563db71b384c6a.png.html

// UV
// 0,0 is at bottom left.
// 1,1 is at top right

using namespace Graphics;
using namespace Assets;

namespace Runtime
{

	//std::unordered_map<std::string, std::vector<glm::vec4>> playerSprite;
	//const float changeFrameTime = 0.3f;
	//float frameTimer = 0.f;
	//size_t frameCount = 0;

	//void ResetFrame()
	//{
	//	frameTimer = 0.f;
	//	frameCount = 0;
	//}

	//void SpriteState::rebindLightsAndPlayerAfterReload()
	//{
	//	auto* scene = m_scenes.current();
	//	
	//}

	/*std::vector<std::string> SpriteState::GetTextureList(const std::string& folder)
	{
		
		std::vector<std::string> textures;
		for (auto& entry : std::filesystem::directory_iterator(folder))
		{
			if (!entry.is_regular_file()) continue;
			auto path = entry.path().string();

			auto hasExtension = [](const std::string& str, const std::string& ext) {
				if (str.size() < ext.size()) return false;
				return std::equal(ext.rbegin(), ext.rend(), str.rbegin(),
					[](char a, char b) { return std::tolower(a) == std::tolower(b); });
				};

			if (hasExtension(path, ".png") || hasExtension(path, ".jpg") || hasExtension(path, ".jpeg"))
				textures.push_back(path);
		}
		return textures;
	}*/

	void SpriteState::InitPlayerSpriteUVs()
	{
		Gameplay::SpriteAnimatorComponent* animatorComponent = m_sprite_obj->getComponent<Gameplay::SpriteAnimatorComponent>();
		if (animatorComponent == nullptr) return;

		const float frameW = 1.0f / 8.0f; // 0.125
		const float frameH = 1.0f / 8.0f; // 0.125

		// Create (or fetch) the "walk" animation
		Gameplay::AnimationClip& deathClip = animatorComponent->createClip("death", true);
		for (int i = 0; i < 21; ++i)
		{
			int row = i / 8;
			int col = i % 8;
			glm::vec4 uv = { col*frameW, (7-row) * frameH, frameW, frameH };
			// for example group rows by animation name
			//playerSprite["walk"].push_back(uv);
			deathClip.frames.push_back({ uv, 0.2f });
		}

		Gameplay::AnimationClip& idleClip = animatorComponent->createClip("idle", true);
		for (int i = 21; i < 25; ++i)
		{
			int row = i / 8;
			int col = i % 8;
			glm::vec4 uv = { col * frameW, (7 - row) * frameH, frameW, frameH };
			// for example group rows by animation name
			//playerSprite["idle"].push_back(uv);
			idleClip.frames.push_back({ uv, 0.1f });
		}

		Gameplay::AnimationClip& idleTorchClip = animatorComponent->createClip("idle_torch", true);
		for (int i = 53; i < 57; ++i)  // Adjust frame indices based on your spritesheet
		{
			int row = i / 8;
			int col = i % 8;
			glm::vec4 uv = { col * frameW, (7 - row) * frameH, frameW, frameH };
			idleTorchClip.frames.push_back({ uv, 0.1f });
		}

		Gameplay::AnimationClip& runClip = animatorComponent->createClip("run", true);
		for (int i = 25; i < 31; ++i)
		{
			int row = i / 8;
			int col = i % 8;
			glm::vec4 uv = { col * frameW, (7 - row) * frameH, frameW, frameH };
			// for example group rows by animation name
			//playerSprite["run"].push_back(uv);
			runClip.frames.push_back({ uv, 0.1f });
		}

		Gameplay::AnimationClip& runTorchClip = animatorComponent->createClip("run_torch", true);
		for (int i = 31; i < 37; ++i)
		{
			int row = i / 8;
			int col = i % 8;
			glm::vec4 uv = { col * frameW, (7 - row) * frameH, frameW, frameH };
			// for example group rows by animation name
			//playerSprite["run_torch"].push_back(uv);
			runTorchClip.frames.push_back({ uv, 0.1f });
		}


		// Create (or fetch) the "walk" animation
		Gameplay::AnimationClip& walkClip = animatorComponent->createClip("walk", true);
		for (int i = 37; i < 45; ++i)
		{
			int row = i / 8;
			int col = i % 8;
			glm::vec4 uv = { col * frameW, (7 - row) * frameH, frameW, frameH };
			// for example group rows by animation name
			//playerSprite["walk"].push_back(uv);
			walkClip.frames.push_back({ uv, 0.1f });
		}


		Gameplay::AnimationClip& walkTorchClip = animatorComponent->createClip("walk_torch", true);
		for (int i = 45; i < 53; ++i)
		{
			int row = i / 8;
			int col = i % 8;
			glm::vec4 uv = { col * frameW, (7 - row) * frameH, frameW, frameH };
			// for example group rows by animation name
			//playerSprite["walk_torch"].push_back(uv);
			walkTorchClip.frames.push_back({ uv, 0.1f });
		}

		

		animatorComponent->play("death");
	}

	SpriteState::SpriteState(std::string initialSceneId, Platform::InputManager &input, GameStateManager &gsm)
		: m_initialSceneId(std::move(initialSceneId)), m_input(input), m_gsm(gsm), m_sprite_obj(nullptr)
	{
	}

	const char *SpriteState::name() const { return "SpriteEditor"; }


	void SpriteState::onEnter()
	{
		std::cout << "[SpriteState] Enter, loading scene: " << m_initialSceneId << "\n";
		Core::Engine &engine = Core::Engine::Get();
		m_renderer = &engine.rendering();
		
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
					m_sprite_obj = &*u;
					auto p = o.transform().position;
					auto s = o.size();
					std::cout << " - id=" << o.id()
										<< " name=\"" << o.name() << "\""
										<< " pos=(" << p.x << "," << p.y << ")"
										<< " size=(" << s.x << "," << s.y << ")"
										<< (o.active() ? "" : " [inactive]") << "\n";
					m_sprite_obj->addComponent<Gameplay::SpriteAnimatorComponent>();

				}
				// ******** Chase function to be implemented in M3 ********
				std::cout.flush();
			}
		}
		else
		{
			std::cout << "[SpriteState] Scene load FAILED.\n";
			std::cout << "[SpriteState] Returning to menu...\n";
			m_shouldReturnToMenu = true; // Returns to menu
		}

		//InitPlayerSpriteUVs();
		m_debug = true;
	}

	void SpriteState::onExit()
	{
		// Only unload if a scene was actually loaded successfully
		if (m_loaded)
		{
			m_scenes.unloadCurrent();
		}
		m_loaded = false;
	}

	void SpriteState::update(double dt)
	{

		Core::Engine &engine = Core::Engine::Get();
		//Platform::InputManager &input = engine.input();
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

		//if (m_input.wasKeyPressed(GLFW_KEY_P))
		//{
		//	m_debug = !m_debug;
		//	if (m_debug)
		//	{
		//		std::cout << "Debug is pressed\n";
		//	}
		//	return;
		//}

		//// Syafiq - Player movement
		//if (auto *scene = m_scenes.current())
		//{

		//	// Syafiq - need find a way to cache the player.or do a unordered_map with object unique name to array of objects.
		//	// objMap["Player"][0] <- since only 1 player. index 0 will work.
		//	for (auto &obj : scene->objects())
		//	{

		//		
		//	}
		//}


		//if (m_input.isKeyDown(GLFW_KEY_Q))
		//{
		//	cam.setZoom(cam.zoom() * 1.02f);
		//}
		//if (m_input.isKeyDown(GLFW_KEY_E))
		//{
		//	cam.setZoom(cam.zoom() / 1.02f);
		//}


		m_scenes.update(dt);
	}

	void SpriteState::render(Core::Time *timer)
	{
		if (!m_loaded || !m_renderer)
			return;

		Core::Engine &engine = Core::Engine::Get();

		// Use SceneManager's render to render the current scene
		// --- Pass: Scene ---
		// Batch rendering
		m_renderer->beginScenePass(engine.camera().viewProj());
		m_scenes.render(*m_renderer);
		// Custom render for seeing bigger spritesheet
		Gameplay::SpriteRendererComponent* spriteImageRenderer = m_sprite_obj->getComponent<Gameplay::SpriteRendererComponent>();
		if (spriteImageRenderer) {
			// Resolve texture alias (local override or GameObject's textureId)
			std::string texAlias = spriteImageRenderer->textureAlias();
			if (texAlias.empty()) return;

			auto texture = ResourceManager<Texture2D>::Get(texAlias);
			if (!texture) {
				std::cerr << "[SpriteRenderer] Missing texture: " << texAlias << "\n";
				return;
			}

			// Bind texture
			//texture->bind(0);

			// Access central RenderingManager (from Engine)
			Graphics::RenderingManager& renderer = Core::Engine::Get().rendering();
			glm::vec2 size = glm::vec2(m_sprite_obj->size().x, m_sprite_obj->size().y);
			glm::vec2 pos = glm::vec2(m_sprite_obj->transform().position.x+550, m_sprite_obj->transform().position.y+100);
			glm::vec2 scale = glm::vec2(m_sprite_obj->transform().scale.x * 7, m_sprite_obj->transform().scale.y * 7);

			renderer.drawSprite(texture->id(),
				pos,
				size * scale, m_sprite_obj->transform().rotationRad,
				glm::vec4{ 0.0f, 0.0f, 1.0f, 1.0f },
				glm::vec4{ 1.0f, 1.0f, 1.0f, 1.0f }, false);
		}

		m_renderer->endScenePass();

		(void)timer;
	}

	void SpriteState::renderImGui(Core::Time* timer)
	{

		if (!m_debug || !m_sprite_obj) return;
		using namespace Gameplay;
		ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);
		ImGui::Begin("Sprite Animator Editor", nullptr, ImGuiWindowFlags_MenuBar);

		SpriteAnimatorComponent* anim = m_sprite_obj->getComponent<SpriteAnimatorComponent>();
		if (!anim) {
			ImGui::Text("No SpriteAnimatorComponent found.");
			ImGui::End();
			return;
		}

		// --- Texture Browser ---
		if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen)) {
			//static std::vector<std::string> texList = GetTextureList("assets/textures");
			static std::vector<std::string> texList = Assets::TextureManager::GetLoadedTextureNames();
			for (auto& tex : texList) {
				if (ImGui::Selectable(tex.c_str(), tex == m_selectedTexture)) {
					m_selectedTexture = tex;
					SpriteRendererComponent* spriteImageRenderer = m_sprite_obj->getComponent<SpriteRendererComponent>();
					if (spriteImageRenderer) {
						spriteImageRenderer->setTexture(tex);
					}
				}
			}
		}

		// --- Animation Selection ---
		if (ImGui::CollapsingHeader("Animations", ImGuiTreeNodeFlags_DefaultOpen)) {
			std::vector<std::string> animNames;
			for (auto& [name, clip] : anim->clips()) // we'll expose clips() later
				animNames.push_back(name);

			if (!animNames.empty()) {
				const char* current = m_selectedAnimation.empty() ? animNames[0].c_str() : m_selectedAnimation.c_str();
				if (ImGui::BeginCombo("Animation", current)) {
					for (auto& a : animNames) {
						bool selected = (a == m_selectedAnimation);
						if (ImGui::Selectable(a.c_str(), selected)) {
							m_selectedAnimation = a;
						}
					}
					ImGui::EndCombo();
				}
			}
			if (m_selectedAnimation != m_lastSelectedAnimation && !m_selectedAnimation.empty()) {
				anim->play(m_selectedAnimation, true);
				m_playing = true;
				m_lastSelectedAnimation = m_selectedAnimation;
			}
			if (auto* clip = anim->getClip(m_selectedAnimation)) {
				m_playLoop = clip->loop;
			}
			ImGui::Separator();

			// --- Playback Controls ---
			if (ImGui::Button("Play")) {
				if (!m_selectedAnimation.empty()) {
					anim->play(m_selectedAnimation, true);
					m_playing = true;
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Reset")) {
				anim->play(m_selectedAnimation, true);
				m_playing = false;
			}
			ImGui::SameLine();
			bool loopValue = m_playLoop;
			if (ImGui::Checkbox("Loop", &loopValue)) {
				m_playLoop = loopValue;
				// Update actual animation clip
				if (!m_selectedAnimation.empty()) {
					if (auto* clip = anim->getClip(m_selectedAnimation)) {
						clip->loop = m_playLoop;
						// Optional: restart animation if needed
						anim->play(m_selectedAnimation, true);
					}
				}
			}

			ImGui::Separator();
			if (!m_selectedAnimation.empty()) {
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
				if (ImGui::Button("Delete Animation")) {
					ImGui::OpenPopup("ConfirmDelete");
				}
				if (ImGui::BeginPopupModal("ConfirmDelete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
					ImGui::Text("Delete animation '%s'?\nThis cannot be undone.", m_selectedAnimation.c_str());
					ImGui::Separator();
					if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
						anim->removeClip(m_selectedAnimation);
						m_selectedAnimation.clear();
						m_lastSelectedAnimation.clear();
						m_selectedFrame = -1;
						ImGui::CloseCurrentPopup();
					}
					ImGui::SameLine();
					if (ImGui::Button("Cancel", ImVec2(120, 0))) {
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndPopup();
				}
				ImGui::PopStyleColor();
			}

			(void)timer;

		}

		// --- Frame Editor ---
		if (!m_selectedAnimation.empty()) {
			AnimationClip* clip = anim->getClip(m_selectedAnimation);
			if (clip) {
				ImGui::Separator();
				ImGui::Text("Editing Animation: %s (%zu frames)", m_selectedAnimation.c_str(), clip->frames.size());
	
				// --- Bulk Duration Editor ---
				static float bulkDuration = 0.1f;
				ImGui::DragFloat("Bulk Duration", &bulkDuration, 0.01f, 0.01f, 5.0f);
				ImGui::SameLine();
				if (ImGui::Button("Apply to All Frames")) {
					for (auto& frame : clip->frames)
						frame.duration = bulkDuration;
				}
				ImGui::Separator();

				for (size_t i = 0; i < clip->frames.size(); ++i) {
					auto& frame = clip->frames[i];
					ImGui::PushID(static_cast<int>(i));
					if (ImGui::Selectable(("Frame " + std::to_string(i)).c_str(), i == m_selectedFrame))
						m_selectedFrame = (int)i;

					if (i == m_selectedFrame) {
						ImGui::DragFloat4("UV (x,y,w,h)", &frame.uv.x, 0.001f, 0.0f, 1.0f);
						ImGui::DragFloat("Duration", &frame.duration, 0.01f, 0.01f, 2.0f);
					}
					ImGui::PopID();
				}
			}
		}

		// --- Grid-Based Animation Creator ---
		if (ImGui::CollapsingHeader("Generate Animation From Sheet", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::InputInt("Sheet Width", &m_sheetWidth);
			ImGui::InputInt("Sheet Height", &m_sheetHeight);
			ImGui::InputInt("Cell Width", &m_cellWidth);
			ImGui::InputInt("Cell Height", &m_cellHeight);
			ImGui::InputInt("Start Frame", &m_startFrame);
			ImGui::InputInt("End Frame", &m_endFrame);
			ImGui::InputText("New Animation Name", m_newAnimName, sizeof(m_newAnimName));

			if (ImGui::Button("Generate Animation")) {
				int cols = m_sheetWidth / m_cellWidth;
				//int rows = m_sheetHeight / m_cellHeight;
				AnimationClip& clip = anim->createClip(m_newAnimName, m_playLoop);

				for (int i = m_startFrame; i <= m_endFrame; ++i) {
					int row = i / cols;
					int col = i % cols;
					float u = static_cast<float>(col * m_cellWidth) / m_sheetWidth;
					float v = 1.0f - static_cast<float>((row + 1) * m_cellHeight) / m_sheetHeight;
					float w = static_cast<float>(m_cellWidth) / m_sheetWidth;
					float h = static_cast<float>(m_cellHeight) / m_sheetHeight;
					clip.frames.push_back({ glm::vec4(u, v, w, h), 0.1f });
				}

				m_selectedAnimation = m_newAnimName;
			}
		}

		// --- Save / Load JSON Section ---
		static std::string jsonFileName = "";

		ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackResize;
		if (ImGui::InputText("JSON File Name", jsonFileName.data(), jsonFileName.capacity() + 1, flags,
			[](ImGuiInputTextCallbackData* data) -> int {
				std::string* str = reinterpret_cast<std::string*>(data->UserData);
				if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
					str->resize(data->BufTextLen);
					data->Buf = str->data();
				}
				return 0;
			}, &jsonFileName))
		{
			// Optionally react to enter key here
		}

		std::string folder = "assets/animations/";
		std::string folder2 = "../assets/animations/";
		std::string path = folder + jsonFileName;
		std::string path2 = folder2 + jsonFileName;
		if (ImGui::Button("Save Animation JSON")) {
			std::filesystem::create_directories(folder);
			std::filesystem::create_directories(folder2);
			if (std::filesystem::exists(path)) {
				ImGui::OpenPopup("ConfirmOverwrite");
			}
			else {
				nlohmann::json j = anim->serialize(jsonFileName);
				std::ofstream(path) << j.dump(4);
				std::ofstream(path2) << j.dump(4);
				std::cout << "[SpriteState] Saved animation JSON to: "
					<< std::filesystem::absolute(path) << std::endl;
			}
		}

		if (ImGui::BeginPopupModal("ConfirmOverwrite", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("File '%s' already exists.\nDo you want to overwrite it?", jsonFileName.c_str());
			ImGui::Separator();
			if (ImGui::Button("Yes, Overwrite", ImVec2(150, 0))) {
				nlohmann::json j = anim->serialize(jsonFileName);
				std::ofstream(path) << j.dump(4);
				std::ofstream(path2) << j.dump(4);
				std::cout << "[SpriteState] Overwrote: "
					<< std::filesystem::absolute(path) << std::endl;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(150, 0))) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		ImGui::SameLine();

		if (ImGui::Button("Reset JSON")) {
			anim->clearAll();
			jsonFileName = "";
			SpriteRendererComponent* spriteImageRenderer = m_sprite_obj->getComponent<SpriteRendererComponent>();
			if (spriteImageRenderer) {
				spriteImageRenderer->resetAll();
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Reload JSON")) {
			std::string folder1 = "assets/animations/";
			std::string path1 = folder1 + std::string(jsonFileName);
			std::ifstream f(path1);
			if (f.is_open()) {
				nlohmann::json j;
				f >> j;
				anim->loadFromJson(j);
				std::cout << "[SpriteState] Reloaded animation from: "
					<< std::filesystem::absolute(path1) << std::endl;
			}
		}

		ImGui::End();
	}


	void SpriteState::resetScene()
	{

	}

} // namespace Runtime
