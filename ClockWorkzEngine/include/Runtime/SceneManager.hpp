#pragma once
#include <memory>
#include <string>
#include <unordered_map>

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
		void render();
		Scene* current();
		Scene const* current() const;
		std::shared_ptr<Scene> currentScene() const { return m_current; }

	private:
		std::unordered_map<std::string, std::shared_ptr<Scene>> m_cache;
		std::shared_ptr<Scene> m_current;
	};

} // namespace Runtime
