/******************************************************************************/
/*!
\file           SceneManager.hpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares SceneManager, a system for handling scene switching
				and lifecycle management.
				Responsibilities include:
				- Holding the currently active Scene
				- Loading, unloading, and transitioning between scenes
				- Exposing hooks for global scene control

References:
- Game loop & scene transitions (Simillar to GSM): https://gameprogrammingpatterns.com/state.html

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
#include <memory>
#include <string>
#include <unordered_map>

namespace Graphics { class RenderingManager; }
namespace Runtime {

	class Scene;

	class SceneManager {
	public:
		SceneManager() = default;

		// Load or switch to an already cached scene (creates & loads if not present)
		bool loadScene(const std::string& id);

		// Unload current scene (keeps it cached for quick return if you want)
		void unloadCurrent();

		void update(double dt);
		void render(Graphics::RenderingManager& m_renderer);
		Scene* current();
		Scene const* current() const;
		std::shared_ptr<Scene> currentScene() const { return m_current; }

		//Reload Content
		bool reloadCurrent();

	private:
		std::unordered_map<std::string, std::shared_ptr<Scene>> m_cache;
		std::shared_ptr<Scene> m_current;
	};

} // namespace Runtime
