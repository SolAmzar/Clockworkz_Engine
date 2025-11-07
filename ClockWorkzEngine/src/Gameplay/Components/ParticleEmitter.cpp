/******************************************************************************/
/*!
\file           ParticleEmitter.cpp
\project        Lights Out
\author(s)      [Amzar], 100%
\brief          Definines ParticleEmitter, a component for emitting particles from a GameObject.
                Responsibilities include:
                - Managing a ParticleSystem instance
                - Updating and rendering particles each frame
                - Allowing bursts of particles to be emitted on demand
                - Supporting local offsets and world-space emission

All content ? 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "Gameplay/Components/ParticleEmitter.hpp"
#include "Gameplay/GameObject.hpp"
using namespace Gameplay;

ParticleEmitter::ParticleEmitter(GameObject& owner, std::size_t max)
    : m_owner(owner), m_system(max) {
}

void ParticleEmitter::update(float dt) {
    // adapt to your Transform API if different
    ps::v2 origin{ m_owner.transform().position.x, m_owner.transform().position.y };
    origin = origin + localOffset;

    if (m_active && m_system.rate > 0.f) {
        m_system.emitContinuous(m_system.worldSpace ? origin : ps::v2{ 0,0 }, dt);
    }
    m_system.update(dt);
}

void ParticleEmitter::render() const {
    if (m_renderer) {
        m_system.render(*m_renderer);
    }; 
}


void ParticleEmitter::burst(int count) {
    ps::v2 origin{ m_owner.transform().position.x, m_owner.transform().position.y };
    m_system.emitBurst(origin + localOffset, count);
}
