/******************************************************************************/
/*!
\file			WallTiles.hpp
\project        Lights Out
\author(s)      [Sol-Amzar], 100%
\brief          Declares helper functions for wall tile UV mapping
                using a 3x3 texture atlas.
                Responsibilities include:
                - Atlas3x3(): Compute UV coordinates for a given column/row in a 3x3 grid
                - WallTile enum: Define tile types (corners, edges, center)
				- GetWallTileUV(): Retrieve UVs for specific wall tile types
All content ? 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
#include <glm/vec4.hpp>
#include <Math/Vec4.hpp>

namespace Gameplay {

    // 3x3 atlas helper: col/row in [0,2]
    inline vec4 Atlas3x3(int col, int row) {
        constexpr float step = 1.0f / 3.0f; // 3x3 grid
        return vec4(col * step, row * step, step, step);  // x,y = start, z,w = size
    }

    enum class WallTile {
        CornerTL,
        EdgeTop,
        CornerTR,
        EdgeLeft,
        Center,
        EdgeRight,
        CornerBL,
        EdgeBottom,
        CornerBR
    };

    inline vec4 GetWallTileUV(WallTile t) {
        switch (t) {
        case WallTile::CornerTL:   return Atlas3x3(0, 2);
        case WallTile::EdgeTop:    return Atlas3x3(1, 2);
        case WallTile::CornerTR:   return Atlas3x3(2, 2);

        case WallTile::EdgeLeft:   return Atlas3x3(0, 1);
        case WallTile::Center:     return Atlas3x3(1, 1);
        case WallTile::EdgeRight:  return Atlas3x3(2, 1);

        case WallTile::CornerBL:   return Atlas3x3(0, 0);
        case WallTile::EdgeBottom: return Atlas3x3(1, 0);
        case WallTile::CornerBR:   return Atlas3x3(2, 0);
        }
        // default: center
        return Atlas3x3(1, 1);
    }

} // namespace Gameplay
