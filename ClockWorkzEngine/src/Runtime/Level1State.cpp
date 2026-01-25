
#include "Runtime/Level1State.hpp"

namespace Runtime {

Level1State::Level1State(Platform::InputManager &input, GameStateManager &gsm)
    // Pass "level1" (or "level1.json") to PlayState constructor
    // Ensure "assets/scenes/level1.json" exists as created by Sandbox
    : PlayState("level_01", input, gsm) {}

} // namespace Runtime
