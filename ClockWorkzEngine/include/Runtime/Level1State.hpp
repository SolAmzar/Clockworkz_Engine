
#pragma once
#include "Runtime/PlayState.hpp"

namespace Runtime {

class Level1State final : public PlayState {
public:
  Level1State(Platform::InputManager &input, GameStateManager &gsm);
  ~Level1State() = default;

  void onEnter() override;

  const char *name() const override { return "Level1State"; }
};

} // namespace Runtime
