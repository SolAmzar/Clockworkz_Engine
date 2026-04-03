/******************************************************************************/
/*!
\file           ParticleEmitter.hpp
\project        ClockWorkz Engine
\author(s)      [Sol-Amzar] 90% [Sol-Amzar] 10%
\brief          Declares an ECS particle emitter component that wraps the
                engine particle system and exposes editor-friendly controls.

All content Â© 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once

#include <cstddef>

#include "Gameplay/GameObject.hpp"
#include "Graphics/ParticleSystem.hpp"

namespace Graphics {
class RenderingManager;
}

namespace Components {

class ParticleEmitter : public Gameplay::Component {
public:
  explicit ParticleEmitter(std::size_t maxParticles = 1024);

  void setActive(bool on) { m_active = on; }
  bool active() const { return m_active; }

  void update(const ps::v2 &position, double dt);
  void render(Graphics::RenderingManager &renderer) const;
  void setRenderer(Graphics::RenderingManager *renderer) {
    m_renderer = renderer;
  }

  void onUpdate(Gameplay::GameObject &owner, double dt) override;
  void onRender(Gameplay::GameObject &owner) override;

  void burst(int count);
  void burst(const ps::v2 &position, int count);

  ParticleSystem &system() { return m_system; }
  const ParticleSystem &system() const { return m_system; }

  ps::v2 localOffset{0.f, 0.f};

private:
  ParticleSystem m_system;
  bool m_active{true};
  ps::v2 m_lastPosition{0.f, 0.f};
  bool m_hasLastPosition{false};
  Graphics::RenderingManager *m_renderer{nullptr};
};

} // namespace Components

