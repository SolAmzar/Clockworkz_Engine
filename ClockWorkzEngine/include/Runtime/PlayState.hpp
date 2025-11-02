/******************************************************************************/
/*!
\file           PlayState.hpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares PlayState, the main gameplay loop state.
				Responsibilities include:
				- Loading and managing the current Scene and its GameObjects
				- Updating gameplay logic such as movement, collisions, and AI
				- Rendering the game world each frame
				- Transitioning back to other states (e.g., pause or menu)

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
#include "Runtime/GameState.hpp"
#include "Runtime/SceneManager.hpp"
#include "Gameplay/GameObject.hpp"
#include <memory>
#include <string>
#include <vector>


namespace Core {
	class Logger; class Time;
}

namespace Graphics {
	class RenderingManager;
}

namespace Runtime {
	
	class PlayState final : public GameState {
	public:
		explicit PlayState(std::string initialSceneId, Platform::InputManager& input, GameStateManager& gsm);

		const char* name() const override;

		void onEnter() override;
		void onExit()  override;
		void update(double dt) override;
		void render(Core::Time* timer) override;
		bool m_spawning = false;
		int m_spawnedCount = 0;
		const int SPAWN_PER_FRAME = 100;
		bool m_shouldReturnToMenu{ false };

		void renderImGui(Core::Time* timer) override; //Imgui for playstate

	private:
		std::string m_initialSceneId;
		SceneManager m_scenes;   // your existing scene manager
		Platform::InputManager& m_input;
		GameStateManager& m_gsm;
		bool m_spawnonclick{ false };
		bool m_loaded{ false };
		bool m_debug{ false };
		bool m_flashlightOn{ true };
		Graphics::RenderingManager* m_renderer{ nullptr };
		float m_leftBound{ 100.f };
		float m_rightBound{ 700.f };
		float m_ballSpeed{ 220.f };
		int   m_ballDir{ +1 };
		void resetScene();
		std::vector<std::unique_ptr<Gameplay::GameObject>> gameObjects;

	};

} // namespace Runtime
