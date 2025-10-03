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

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
#include "Runtime/GameState.hpp"
#include "Runtime/SceneManager.hpp"
#include <memory>
#include <string>
namespace Core {
	class Logger; class Time;
}
namespace Runtime {
	
	class PlayState final : public GameState {
	public:
		explicit PlayState(std::string initialSceneId);

		const char* name() const override;

		void onEnter() override;
		void onExit()  override;
		void update(double dt) override;
		void render(Core::Time* timer) override;

	private:
		std::string m_initialSceneId;
		SceneManager m_scenes;   // your existing scene manager
		bool m_loaded{ false };
	};

} // namespace Runtime
