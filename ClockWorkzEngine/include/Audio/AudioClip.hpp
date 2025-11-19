// AudioClip.hpp
#pragma once
#include <string>
#include <fmod.hpp>
#include "Audio/AudioManager.hpp"

namespace Audio {
    class AudioClip {
    public:
        AudioClip() = default;
        ~AudioClip() { /*Unload(); */ }

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
            if (!system) return false;

            FMOD_MODE mode = FMOD_DEFAULT;
            if (file.loop)   mode |= FMOD_LOOP_NORMAL; else mode |= FMOD_LOOP_OFF;
            if (file.stream) mode |= FMOD_CREATESTREAM; else mode |= FMOD_CREATESAMPLE;
            if (file.is3D)   mode |= FMOD_3D; else       mode |= FMOD_2D;

            FMOD_RESULT result = system->createSound(file.path.c_str(), mode, nullptr, &m_sound);
            if (result != FMOD_OK) {
                m_sound = nullptr;
                return false;
            }
            return true;
        }

        // Free FMOD resources if loaded
        inline void Unload() {
            if (m_sound) {
                m_sound->release();   // free FMOD sound
                m_sound = nullptr;
            }
        }

        // Low-level access if needed
        FMOD::Sound* GetRaw() const { return m_sound; }

    private:
        FMOD::Sound* m_sound = nullptr;  // owned FMOD sound resource

    };

} // namespace Audio
