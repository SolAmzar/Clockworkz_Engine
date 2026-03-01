/******************************************************************************/
/*!
\file           AudioSource.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          AudioSource component - pure data for audio playback.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include <string>

namespace Components {

/**
 * @brief AudioSource component - audio playback data
 *
 * Pure data component for entities that emit sound.
 * Used by AudioSystem to manage sound playback.
 */
struct AudioSource {
  std::string soundId{};   ///< Sound asset ID
  float volume{1.f};       ///< Volume (0.0 to 1.0)
  bool loop{false};        ///< Loop playback
  bool playing{false};     ///< Currently playing
  bool playOnStart{false}; ///< Auto-play when entity is created
  float pitch{1.f};        ///< Pitch multiplier
};

} // namespace Components
