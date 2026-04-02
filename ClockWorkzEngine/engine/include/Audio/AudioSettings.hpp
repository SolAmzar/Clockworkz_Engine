/******************************************************************************/
/*!
\file           AudioSettings.hpp
\project        ClockWorkz Engine
\author         [Sol-Amzar]
\brief          Engine-level singleton storing user-desired audio group volumes.
                Persists across state transitions (static storage).
                Any game state can read/write via AudioSettings::Get().

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#pragma once

namespace Audio {

struct AudioSettings {
    float master = 1.0f;
    float bgm    = 0.8f;
    float sfx    = 1.0f;

    static AudioSettings& Get() {
        static AudioSettings s;
        return s;
    }

private:
    AudioSettings() = default;
};

} // namespace Audio
