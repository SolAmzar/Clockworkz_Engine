#include "States/Level1State.hpp"
#include "Components/PlayerController.hpp"
#include "Components/Light.hpp"
#include "Runtime/Scene.hpp"
#include <iostream>

namespace Runtime {

Level1State::Level1State(Platform::InputManager &input, GameStateManager &gsm)
    // Pass "level1" (or "level1.json") to PlayState constructor
    // Ensure "assets/scenes/level1.json" exists as created by Sandbox
    : PlayState("Level1", input, gsm) {}

void Level1State::onEnter() {
  PlayState::onEnter();

  auto *scene = m_scenes.current();
  if (!scene)
    return;

  // "Level_01.json" loads ECS entities directly into m_scene.GetWorld()
  // We need to find the Player entity and give it a Light component if it
  // doesn't have one.
  auto &world = scene->GetWorld();

  // Find player by name "Player" (assumes name was serialized, or just use
  // tag/component) Since we don't have a Tag component in this codebase yet
  // (implied by SceneSerializer), we can search by FragmentCollector which the
  // specific player has. Or just iterate and check for player-specific
  // components like PlayerController.

  world.each<Components::PlayerController>(
      [&](ECS::Entity entity, Components::PlayerController &) {
        if (!world.hasComponent<Components::Light>(entity)) {
          Components::Light torch;
          torch.active = true;
          torch.radius = 300.0f;
          torch.angle = 60.0f;
          torch.color = {1.0f, 0.9f, 0.6f};
          torch.intensity = 1.0f;
          torch.offset = {35.0f, -20.0f};
          world.addComponent(entity, torch);
          std::cout << "[Level1State] Added Light component to Player entity "
                    << entity.id << "\n";
        }
      });
}

} // namespace Runtime
