#pragma once
#include "Runtime/GameState.hpp"
#include "Runtime/SceneManager.hpp"
#include <memory>
#include <string>

namespace Runtime {

	class PlayState final : public GameState {
	public:
		explicit PlayState(std::string initialSceneId);

		const char* name() const override;

		void onEnter() override;
		void onExit()  override;
		void update(double dt) override;
		void render() override;

	private:
		std::string m_initialSceneId;
		SceneManager m_scenes;   // your existing scene manager
		bool m_loaded{ false };
	};

} // namespace Runtime
