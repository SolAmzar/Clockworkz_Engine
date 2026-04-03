/******************************************************************************/
/*!
\file           ParticleEmitter.cpp
\project        ClockWorkz Engine
\author(s)      [Sol-Amzar] 100%
\brief          Implements the engine-owned particle emitter component.

All content Â© 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Components/ParticleEmitter.hpp"

#include "Graphics/RenderingManager.hpp"

namespace Components {

ParticleEmitter::ParticleEmitter(std::size_t maxParticles)
    : m_system(maxParticles) {}

void ParticleEmitter::update(const ps::v2 &position, double dt) {
  m_lastPosition = position;
  m_hasLastPosition = true;

  const ps::v2 origin = position + localOffset;
  if (m_active && m_system.rate > 0.f) {
    m_system.emitContinuous(m_system.worldSpace ? origin : ps::v2{0.f, 0.f},
                            dt);
  }
  m_system.update(dt);
}

void ParticleEmitter::render(Graphics::RenderingManager &renderer) const {
  if (!m_active && m_system.aliveCount() == 0)
    return;

  m_system.render(renderer);
}

void ParticleEmitter::onUpdate(Gameplay::GameObject &owner, double dt) {
  update(ps::v2{owner.transform().position.x, owner.transform().position.y},
         dt);
}

void ParticleEmitter::onRender(Gameplay::GameObject & /*owner*/) {
  if (m_renderer != nullptr)
    render(*m_renderer);
}

void ParticleEmitter::burst(const ps::v2 &position, int count) {
  const ps::v2 origin = position + localOffset;
  m_system.emitBurst(m_system.worldSpace ? origin : ps::v2{0.f, 0.f}, count);
}

void ParticleEmitter::burst(int count) {
  if (!m_hasLastPosition && owner() != nullptr) {
    m_lastPosition =
        ps::v2{owner()->transform().position.x, owner()->transform().position.y};
    m_hasLastPosition = true;
  }
  if (!m_hasLastPosition)
    return;

  burst(m_lastPosition, count);
}

} // namespace Components
