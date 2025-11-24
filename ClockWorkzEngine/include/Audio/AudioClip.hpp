/******************************************************************************/
/*!
\file			AudioClip.hpp
\project        Lights Out
\author(s)      [Amzar], 100%
\brief          Definition of AudioClip class that wraps FMOD::Sound for sound resource management.
                Responsibilities include:
                - Load(): Load sound data from file using AudioManager and SoundDesc
                - Unload(): Release FMOD resources
                - Move semantics for safe resource transfer
				- Provides low-level access to FMOD::Sound if needed
All content ? 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

// AudioClip.hpp
#pragma once
#include <string>
#include <fmod.hpp>
#include <iostream>
#include "Audio/AudioManager.hpp"

namespace Audio {
    class AudioClip {
    public:
        AudioClip() = default;
        ~AudioClip() = default;

        AudioClip(const AudioClip&) = delete;
        AudioClip& operator=(const AudioClip&) = delete;

        AudioClip(AudioClip&& o) noexcept : m_sound(o.m_sound) { o.m_sound = nullptr; }
        AudioClip& operator=(AudioClip&& o) noexcept {
            if (this != &o) {
                Unload();
                m_sound = o.m_sound;
                o.m_sound = nullptr;
            }
            return *this;
        }

        // Called by ResourceManager or manually to load sound data
        bool Load(AudioManager& audio, SoundDesc file) {
            Unload();
            FMOD::System* system = audio.getSystem();   // <-- you'll add this getter
            if (!system) {
                std::cout << "[AudioClip] No FMOD system when loading '"
                    << file.path << "'\n";
                return false;
            }

            FMOD_MODE mode = FMOD_DEFAULT;
            if (file.loop)   mode |= FMOD_LOOP_NORMAL; else mode |= FMOD_LOOP_OFF;
            if (file.stream) mode |= FMOD_CREATESTREAM; else mode |= FMOD_CREATESAMPLE;
            if (file.is3D)   mode |= FMOD_3D; else       mode |= FMOD_2D;

            FMOD_RESULT result = system->createSound(file.path.c_str(), mode, nullptr, &m_sound);
            if (result != FMOD_OK) {
                std::cout << "[AudioClip] Failed to create sound for '"
                    << file.path << "' (FMOD error " << result << ")\n";
                m_sound = nullptr;
                return false;
            }
            return true;
        }

        // Free FMOD resources if loaded
        inline void Unload() {
            if (m_sound) {
                // Only try to release if we still have a system pointer
                // (i.e., AudioManager hasn’t been shut down yet).
                if (m_system) {
                    FMOD_RESULT r = m_sound->release();
                    if (r != FMOD_OK) {
                        std::cout << "[AudioClip] FMOD::Sound::release failed ("
                            << (int)r << ")\n";
                    }
                }
                m_sound = nullptr;
                m_system = nullptr;
            }
        }

        // Low-level access if needed
        FMOD::Sound* GetRaw() const { return m_sound; }

    private:
        FMOD::Sound* m_sound = nullptr;  // owned FMOD sound resource
        FMOD::System* m_system = nullptr;
    };

} // namespace Audio
