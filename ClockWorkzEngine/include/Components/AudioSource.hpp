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
  std::string soundId{};          ///< Sound asset ID
  float volume{1.f};              ///< Volume (0.0 to 1.0)
  bool loop{false};               ///< Loop playback
  bool playing{false};            ///< Currently playing
  bool playOnStart{false};        ///< Auto-play when entity is created
  float pitch{1.f};               ///< Pitch multiplier
  bool spatial{false};            ///< Enable 3D positioning/attenuation
  bool directional{false};        ///< Enable directional cone attenuation
  float minDistance{100.f};       ///< 3D min attenuation distance
  float maxDistance{1000.f};      ///< 3D max attenuation distance
  float innerConeAngleDeg{360.f}; ///< Full-volume cone angle
  float outerConeAngleDeg{360.f}; ///< Outer attenuation cone angle
  float outerConeVolume{1.f};     ///< Volume outside outer cone
  float height{0.f};              ///< Optional 3D Z offset in world units
  int channelId{0};               ///< Runtime channel handle (not serialized)
};

} // namespace Components
