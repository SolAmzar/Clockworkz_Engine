/******************************************************************************/
/*!
\file           ParticleEmitter.cpp
\project        Lights Out
\author(s)      [Amzar], 100%
\brief          Definines ParticleEmitter, a component for emitting particles
from a GameObject. Responsibilities include:
                - Managing a ParticleSystem instance
                - Updating and rendering particles each frame
                - Allowing bursts of particles to be emitted on demand
                - Supporting local offsets and world-space emission

All content ? 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Gameplay/Components/ParticleEmitter.hpp"
#include "Gameplay/GameObject.hpp"
namespace Components {
using namespace Gameplay;

ParticleEmitter::ParticleEmitter(std::size_t max) : m_system(max) {}

void ParticleEmitter::update(const ps::v2 &position, double dt) {
  ps::v2 origin = position + localOffset;
  if (m_active && m_system.rate > 0.f) {
    m_system.emitContinuous(m_system.worldSpace ? origin : ps::v2{0, 0}, dt);
  }
  m_system.update(dt);
}

void ParticleEmitter::onUpdate(GameObject &owner, double dt) {
  // Legacy hook
  update(ps::v2{owner.transform().position.x, owner.transform().position.y},
         dt);
}

void ParticleEmitter::onRender(GameObject & /*owner*/) {
  if (m_renderer) {
    m_system.render(*m_renderer);
  };
}

void ParticleEmitter::burst(const ps::v2 &position, int count) {
  m_system.emitBurst(position + localOffset, count);
}

void ParticleEmitter::burst(int count) {
  if (owner()) {
    ps::v2 origin{owner()->transform().position.x,
                  owner()->transform().position.y};
    burst(origin, count);
  }
}

} // namespace Components