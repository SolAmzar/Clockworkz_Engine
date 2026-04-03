/******************************************************************************/
/*!
\file           Collision.hpp
\project        Lights Out
\author(s)      [Sol-Amzar] 100%
\brief          Establishes basic 2D AABB collision detection functions.
                Responsibilities include:
                - Defining an AABB struct with min and max points
                - Implementing static overlap test for two AABBs
                - Implementing swept AABB test for moving AABBs over a time interval
                - Providing accurate collision timing and detection for 2D games
				- Serving as a lightweight, header-only utility for AABB collision detection
All content ï¿½ 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/

#pragma once
#include <glm/vec2.hpp>
#include <cstdint>

namespace Gameplay {

    enum Layer : uint32_t {
        Player = 1u << 0,
        Enemy = 1u << 1,
        Wall = 1u << 2,
        Trap = 1u << 3,
        Ball = 1u << 4,
    };


    namespace LayerBits {
        constexpr uint32_t Background = 1u << 8;
        constexpr uint32_t World = 1u << 9;   // walls, floors
        constexpr uint32_t Actors = 1u << 10;  // player, enemies
        constexpr uint32_t FX = 1u << 11;  // particles, lights meshes
        constexpr uint32_t UI = 1u << 12;
    }

    // convenience: OR a list of layers into a mask
    template <typename... Bits>
    constexpr uint32_t Mask(Bits... bits) {
        return (0u | ... | bits);
    }

}