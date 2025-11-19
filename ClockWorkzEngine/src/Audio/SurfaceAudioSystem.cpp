#include "Audio/SurfaceAudioSystem.hpp"
#include <random>


namespace Audio {
    void SurfaceAudioSystem::playFootstep(Audio::AudioManager& audio, Gameplay::SurfaceType surface, float speed) {
        // choose id list from surface type
        const char* id = "fs_default";
        switch (surface) {
        case Gameplay::SurfaceType::Wood:  id = "fs_tiles_01"; break;
        case Gameplay::SurfaceType::Tile:  id = "fs_tiles_02"; break;
			// ...to add extensions for other surface types
        default: 
        id = "fs_tiles_01"; break;
        }
		// volume scales with speed (clamped)
        float volume = std::clamp(0.4f + 0.6f * (speed / 200.0f), 0.3f, 1.0f);
        audio.playOneShot(id, Audio::Group::SFX, volume);
    }
} // namespace Audio