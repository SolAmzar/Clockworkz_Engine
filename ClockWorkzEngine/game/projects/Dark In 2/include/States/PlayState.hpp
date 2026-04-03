/******************************************************************************/
/*!
\file           PlayState.hpp
\project        Dark In 2
\author(s)      [Syafiq] 100%
\brief          Declares the Dark In 2 PlayState game state.

All content © 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#pragma once
#include "Runtime/GameState.hpp"
#include "Runtime/Scene.hpp"
#include <string>

namespace Platform { class InputManager; }
namespace Runtime  { class GameStateManager; }
namespace Audio    { class AudioManager; }
namespace Core     { class Time; }

namespace Runtime {

class PlayState final : public GameState {
public:
    PlayState(GameStateManager& gsm, Platform::InputManager& input,
              Audio::AudioManager& audio, const std::string& scenePath);

    const char* name() const override { return "PlayState"; }
    void onEnter()  override;
    void onExit()   override;
    void update(double dt) override;
    void render(Core::Time* timer) override;
    void renderImGui(Core::Time* timer) override;

private:
    GameStateManager&       m_gsm;
    Platform::InputManager& m_input;
    Audio::AudioManager&    m_audio;
    std::string             m_scenePath;
    Runtime::Scene          m_scene;
};

} // namespace Runtime
