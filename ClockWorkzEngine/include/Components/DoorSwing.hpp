/******************************************************************************/
/*!
\file           DoorSwing.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          DoorSwing component - data for rotating doors/objects.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once

namespace Components {

/**
 * @brief DoorSwing component - interpolates rotation for open/close states
 */
struct DoorSwing {
  bool isOpen{false};         ///< Target state
  bool initialized{false};    ///< Have we captured the initial rotation?
  float initialRotation{0.f}; ///< Captured initial rotation (closed state)

  float swingAngle{90.f}; ///< How many degrees to swing (absolute value)
  bool clockwise{true};   ///< Direction of swing
  float speed{90.f};      ///< Degrees per second
};

} // namespace Components
