/******************************************************************************/
/*!
\file           GameState.hpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares GameState, the abstract base class for different
				game states (menu, play, pause, etc.).
				Responsibilities include:
				- Defining a consistent interface with onEnter(), update(),
				  render(), and onExit()
				- Allowing states to control their own initialization and cleanup
				- Providing a foundation for extending concrete states

Reference: GIT

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
namespace Core {
	class Logger; class Time;
}
namespace Runtime {
	class GameState {
	public:
		virtual ~GameState() = default;
		virtual void onEnter() {}
		virtual void onExit() {}
		virtual void update(double dt) = 0;
		virtual void render(Core::Time* timer) = 0;
		virtual const char* name() const { return "GameState"; }
	};

} // namespace Runtime
