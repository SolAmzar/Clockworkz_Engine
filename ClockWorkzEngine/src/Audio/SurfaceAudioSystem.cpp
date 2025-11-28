/******************************************************************************/
/*!
\file			SurfaceAudioSystem.cpp
\project        Lights Out
\author(s)      [Amzar], 100%
\brief          Definition of SurfaceAudioSystem for playing footstep sounds
                based on surface type and movement speed.
                Responsibilities include:
                - playFootstep(): Play appropriate footstep sound for given surface and speed
                - Managing timing between footsteps to avoid overlap
All content ? 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "Audio/SurfaceAudioSystem.hpp"
#include <random>
#include <algorithm>
#include <array>

namespace
{
    // Shared RNG for all footsteps
    std::mt19937& FootstepRng()
    {
        static std::mt19937 rng{ std::random_device{}() };
        return rng;
    }

    template<std::size_t N>
    const char* pickRandom(const std::array<const char*, N>& ids)
    {
        std::uniform_int_distribution<std::size_t> dist(0, N - 1);
        return ids[dist(FootstepRng())];
    }
}

namespace Audio {
    void SurfaceAudioSystem::playFootstep(Audio::AudioManager& audio,
        Gameplay::SurfaceType surface,
        float speed)
    {
        const char* id = "fs_default";

        switch (surface) {
        case Gameplay::SurfaceType::Concrete:
        {
            // 3 variations for cement
            static const std::array<const char*, 3> concreteIds = {
                "fs_concrete_1",
                "fs_concrete_2",
                "fs_concrete_3"
            };
            id = pickRandom(concreteIds);
            break;
        }

        case Gameplay::SurfaceType::Wood:
        {
            static const std::array<const char*, 2> woodIds = {
                "Wood_1",
                "Wood_2"
            };
            id = pickRandom(woodIds);
            break;
        }

        case Gameplay::SurfaceType::Tile:
        {
            static const std::array<const char*, 2> tileIds = {
                "Tile_1",
                "Tile_2"
            };
            id = pickRandom(tileIds);
            break;
        }

        // add other surfaces here…

        default:
        {
            static const std::array<const char*, 3> defaultIds = {
                "fs_concrete_1",
                "fs_concrete_2",
                "fs_concrete_3"
            };
            id = pickRandom(defaultIds);
            break;
        }
        }

        // volume scales with speed (clamped)
        float volume = std::clamp(0.4f + 0.6f * (speed / 200.0f), 0.3f, 1.0f);
        audio.playOneShot(id, Audio::Group::SFX, volume);
    }
} // namespace Audio