/******************************************************************************/
/*!
\file           ParticleEmitter.hpp
\project        Lights Out
\author(s)      [Amzar], 100%
\brief          Declares ParticleEmitter, a component for emitting particles from a GameObject.
				Responsibilities include:
				- Managing a ParticleSystem instance
				- Updating and rendering particles each frame
				- Allowing bursts of particles to be emitted on demand
				- Supporting local offsets and world-space emission

All content ? 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
#include "math/ps_math.hpp"
#include "graphics/ParticleSystem.hpp"
#include "Gameplay/GameObject.hpp"

namespace Gameplay {
	// class GameObject;

	class ParticleEmitter : public Component {
	public:
		explicit ParticleEmitter(std::size_t max = 1024);
		void setActive(bool on) { m_active = on; }
		bool active() const { return m_active; }

		void setRenderer(Graphics::RenderingManager* r) { m_renderer = r; }

		void onUpdate(GameObject& owner, double dt) override;
		void onRender(GameObject& owner) override;
		void burst(int count);

		ps::v2 localOffset{ 0.f, 0.f };
		bool   worldSpace = true;

		ParticleSystem& system() { return m_system; }

	private:
		ParticleSystem m_system;
		bool m_active{ true };
		Graphics::RenderingManager* m_renderer{ nullptr };
	};
}
