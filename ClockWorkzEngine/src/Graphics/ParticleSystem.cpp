/******************************************************************************/
/*!
\file           ParticleSystem.cpp
\project        Lights Out
\author(s)      [Amzar], 100%
\brief          Declares ParticleSystem, a simple 2D particle system for effects
                like smoke, fire, explosions, etc.
                Responsibilities include:
                - Managing a pool of particles with properties like position,
                  velocity, life, size, and color.
                - Emitting particles in bursts or continuously from an origin
                  point with configurable direction and spread.
                - Updating particle states over time (movement, life decay,
                  size/color interpolation).
				- Rendering particles (hook to your renderer in render()).

All content ? 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "Graphics/ParticleSystem.hpp"
#include "Graphics/RenderingManager.hpp"
#include <algorithm>
#include <cmath>

ParticleSystem::ParticleSystem(std::size_t max) : m_pool(max) {}

void ParticleSystem::setSeed(uint32_t s) { m_rng = s ? s : 0xabcdefu; }
float ParticleSystem::frand01() { m_rng = 1664525u * m_rng + 1013904223u; return (m_rng >> 8) * (1.0f / 16777216.f); }
float ParticleSystem::frand(float a, float b) { return a + (b - a) * frand01(); }

ps::v2 ParticleSystem::randDirInCone(ps::v2 basis, float halfAngleDeg) {
    ps::v2 d = ps::normalize(basis);
    float a = ps::radians(frand(-halfAngleDeg, halfAngleDeg));
    return ps::rotate(d, a);
}

void ParticleSystem::revive(Particle& p, ps::v2 origin) {
    p.alive = true;
    p.life = frand(lifeMin, lifeMax);
    p.invLife = 1.0f / p.life;
    ps::v2 vdir = randDirInCone(dir, spreadDeg);
    float spd = frand(speedMin, speedMax);
    p.pos = origin;
    p.vel = vdir * spd;
    p.acc = acceleration;
    p.rotation = 0.f;
    p.size = sizeStart;
    p.color = colorStart;
}

void ParticleSystem::emitBurst(ps::v2 origin, int count) {
    for (int i = 0; i < count && m_firstDead < m_pool.size(); ++i) {
        revive(m_pool[m_firstDead++], origin);
        ++m_alive;
    }
}

void ParticleSystem::emitContinuous(ps::v2 origin, float dt) {
    m_emitAcc += rate * dt;
    int toEmit = static_cast<int>(m_emitAcc);
    m_emitAcc -= toEmit;
    emitBurst(origin, toEmit);
}

void ParticleSystem::update(float dt) {
    std::size_t write = 0;
    for (std::size_t read = 0; read < m_firstDead; ++read) {
        Particle& p = m_pool[read];
        if (!p.alive) continue;

        p.life -= dt;
        if (p.life <= 0.f) { p.alive = false; continue; }

        p.vel = p.vel + p.acc * dt;
        p.pos = p.pos + p.vel * dt;

        const float t = 1.f - p.life * p.invLife; // 0..1
        p.size = sizeStart + (sizeEnd - sizeStart) * t;
        p.color = {
          colorStart.r + (colorEnd.r - colorStart.r) * t,
          colorStart.g + (colorEnd.g - colorStart.g) * t,
          colorStart.b + (colorEnd.b - colorStart.b) * t,
          colorStart.a + (colorEnd.a - colorStart.a) * t
        };

        m_pool[write++] = p; // compact alive
    }
    m_firstDead = write;
    m_alive = write;
}

void ParticleSystem::render(Graphics::RenderingManager& renderer) const {
    for (size_t i = 0; i < m_firstDead; ++i) {
        const Particle& p = m_pool[i];

        // Your API expects center + radius, no alpha
        const float radius = p.size * 0.5f;
        renderer.drawCircle(
            p.pos.x, p.pos.y,
            radius,
            p.color.r, p.color.g, p.color.b
        );
    }
}

void ParticleSystem::clear() {
    m_firstDead = 0; m_alive = 0;
    for (auto& p : m_pool) p.alive = false;
}
