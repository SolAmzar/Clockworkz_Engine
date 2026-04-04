/******************************************************************************/
/*!
\file           TilemapCollisionBuilder.hpp
\project        ClockWorkz Engine
\author(s)      [Syafiq] 94% [Amzar] 6%
\brief          Greedy-rectangle tilemap collision merging.

                Merges adjacent solid tiles into minimal rectangular colliders,
                eliminating internal edges that cause ghost collisions. Individual
                tile Collider.isSolid flags are disabled so only the merged
                rectangles participate in solid collision resolution.

All content (C) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#pragma once
#include "ECS/World.hpp"
#include "ECS/Entity.hpp"
#include "Math/Vec2.hpp"
#include <vector>

namespace Systems {

/**
 * @brief Entry describing one tile for the collision builder.
 */
struct TilemapTileEntry {
    vec2        position;        ///< World-space center of the tile (= Transform.position)
    ECS::Entity entity;          ///< The ECS entity for this tile
    int         layer;           ///< Tilemap layer index
    uint32_t    combinedGroupId; ///< Merge bucket: 0 normal, non-zero authored/composite isolation
};

/**
 * @brief Builds merged rectangular colliders from a flat tilemap.
 *
 * Usage:
 *   1. After loading tiles, call build() to generate merged colliders.
 *   2. Before clearing or reloading tiles, call clear() to remove merged
 *      entities and restore individual tile colliders.
 *
 * Merge criteria (all must be true):
 *   - Collider.isSolid == true
 *   - Collider.isTrigger == false
 *   - Collider.moveable == false
 */
class TilemapCollisionBuilder {
public:
    /**
     * @brief Build merged solid colliders from tilemap tile entries.
     *
     * Runs a greedy rectangle merge per layer. For each merged rect:
     *   - Creates an ECS entity with Transform + Collider + Tag("TilemapMergedCollider") + Layer
     *   - Sets Collider.isSolid = false on the original tile entities
     *
     * @param world   The ECS world
     * @param tiles   All tile entries (position, entity, layer)
     * @param tileW   Tile width in world units (= gridSize)
     * @param tileH   Tile height in world units (= gridSize)
     */
    static void build(ECS::World& world,
                      const std::vector<TilemapTileEntry>& tiles,
                      float tileW, float tileH);

    /**
     * @brief Remove all merged collider entities and restore individual tile colliders.
     *
     * Must be called before clearTilemap() or build() is called again.
     *
     * @param world The ECS world
     */
    static void clear(ECS::World& world);
};

} // namespace Systems
