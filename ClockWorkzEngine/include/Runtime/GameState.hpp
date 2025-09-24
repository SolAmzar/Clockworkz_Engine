#pragma once

namespace Runtime {

	class GameState {
	public:
		virtual ~GameState() = default;
		virtual void onEnter() {}
		virtual void onExit() {}
		virtual void update(double dt) = 0;
		virtual void render() = 0;
		virtual const char* name() const { return "GameState"; }
	};

} // namespace Runtime
