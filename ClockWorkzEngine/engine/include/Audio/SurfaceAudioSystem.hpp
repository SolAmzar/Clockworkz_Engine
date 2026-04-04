/******************************************************************************/
/*!
\file			SurfaceAudioSystem.hpp
\project        Lights Out
\author(s)      [Amzar], 100%
\brief          Declaration of SurfaceAudioSystem for playing footstep sounds
				based on surface type and movement speed.
				Responsibilities include:
				- playFootstep(): Play appropriate footstep sound for given surface and speed
				- Managing timing between footsteps to avoid overlap
All content ? 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
#include "Gameplay/Surface.hpp"
#include "Audio/AudioManager.hpp"
#include "math/vec2.hpp"

namespace Audio {
	class  SurfaceAudioSystem {
	public:
		void playFootstep(Audio::AudioManager& audio, Gameplay::SurfaceType surface, float speed);

	private:
		const float m_stepTimer{ 0.0f }; // Time interval between footsteps
	};
}
