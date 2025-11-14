#pragma once
#include "Gameplay/Surface.hpp"
#include "Audio/AudioManager.hpp"
#include "math/vec2.hpp"

namespace Audio {
	class  SurfaceAudioSystem {	
	public:
		void init(Audio::AudioManager& audio);
		void playFootstep(Audio::AudioManager& audio, Gameplay::SurfaceType surface, float speed);

	private:
		const float m_stepTimer{ 0.0f }; // Time interval between footsteps
	};
}