#pragma once
#include <vector>
#include "math/ps_math.hpp"
#include "Graphics/Particle.hpp"

namespace Graphics
{
	class RenderingManager;
}


class ParticleSystem {
public:
	explicit ParticleSystem(std::size_t max = 2048);

	void update(double dt);
	void render(Graphics::RenderingManager& renderer) const;

	void emitBurst(ps::v2 origin, int count);
	void emitContinuous(ps::v2 origin, double dt);

	// Tunables
	float rate = 200.f;
	float lifeMin = 0.5f, lifeMax = 1.2f;
	float speedMin = 20.f, speedMax = 120.f;
	float spreadDeg = 45.f;
	ps::v2 dir = { 1.f, 0.f };              // should be normalized
	ps::v2 acceleration = { 0.f, -200.f };
	float sizeStart = 2.25f, sizeEnd = 0.f;
	ps::Color colorStart = { 1,1,1,1 };
	ps::Color colorEnd = { 1,1,1,0 };
	bool  worldSpace = true;
	bool  looping = true;

	void setSeed(uint32_t s);
	void clear();
	std::size_t aliveCount() const { return m_alive; }

private:
	std::vector<Particle> m_pool;
	std::size_t m_firstDead = 0;
	std::size_t m_alive = 0;
	float m_emitAcc = 0.f;
	uint32_t m_rng = 0x12345678u;

	float frand01();
	float frand(float a, float b);
	ps::v2 randDirInCone(ps::v2 basis, float halfAngleDeg);
	void revive(Particle& p, ps::v2 origin);
};
