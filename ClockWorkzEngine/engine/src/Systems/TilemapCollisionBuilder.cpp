/******************************************************************************/
/*!
\file           TilemapCollisionBuilder.cpp
\project        ClockWorkz Engine
\brief          Greedy-rectangle tilemap collision merging implementation.

All content (C) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#include "Systems/TilemapCollisionBuilder.hpp"
#include "ECS/World.hpp"
#include "Components/Transform.hpp"
#include "Components/Collider.hpp"
#include "Components/Tag.hpp"
#include "Components/Layer.hpp"
#include "Physics/Collision.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <unordered_map>
#include <vector>

namespace Systems {

// Tracks which tile entities had isSolid disabled so clear() can restore them.
static std::vector<ECS::Entity> s_disabledTileEntities;

// ---------------------------------------------------------------------------

void TilemapCollisionBuilder::clear(ECS::World& world) {
    // Re-enable isSolid on individual tile colliders that were disabled during build().
    for (const auto& tileEntity : s_disabledTileEntities) {
        if (world.isValid(tileEntity)) {
            auto* tileCollider = world.getComponent<Components::Collider>(tileEntity);
            if (tileCollider) tileCollider->isSolid = true;
        }
    }
    s_disabledTileEntities.clear();

    // Destroy all merged collider entities (tagged "TilemapMergedCollider").
    std::vector<ECS::Entity> mergedEntitiesToDestroy;
    world.each<Components::Tag>([&](ECS::Entity entity, Components::Tag& tag) {
        if (tag.tag == "TilemapMergedCollider")
            mergedEntitiesToDestroy.push_back(entity);
    });
    for (const auto& mergedEntity : mergedEntitiesToDestroy)
        world.destroyEntity(mergedEntity);
}

// ---------------------------------------------------------------------------

void TilemapCollisionBuilder::build(ECS::World& world,
                                     const std::vector<TilemapTileEntry>& tiles,
                                     float tileWidth, float tileHeight) {
    using namespace Components; // brings Transform, Collider, Tag into scope

    if (tiles.empty() || tileWidth <= 0.f || tileHeight <= 0.f) return;

    // Group qualifying tiles by layer.
    // A tile qualifies if it is a static solid wall: isSolid, not a trigger,
    // not moveable, and not a player/enemy entity.
    std::unordered_map<int, std::vector<const TilemapTileEntry*>> tilesByLayer;
    for (const auto& tile : tiles) {
        if (!world.isValid(tile.entity)) continue;
        auto* tileCollider = world.getComponent<Collider>(tile.entity);
        if (!tileCollider)               continue;
        if (!tileCollider->isSolid)                      continue;
        if (tileCollider->isTrigger)                     continue;
        if (tileCollider->moveable)                      continue;
        if (tileCollider->shape != Collider::Shape::Box) continue; // skip circles
        tilesByLayer[tile.layer].push_back(&tile);
    }

    int totalMergedRects = 0;

    for (auto& [layerIndex, tilesOnLayer] : tilesByLayer) {
        if (tilesOnLayer.empty()) continue;

        // --- Find the world-space bounding box of all tiles on this layer ---
        float leftmostX  = tilesOnLayer[0]->position.x;
        float topmostY   = tilesOnLayer[0]->position.y;
        float rightmostX = leftmostX;
        float bottommostY = topmostY;
        for (const auto* tile : tilesOnLayer) {
            leftmostX  = std::min(leftmostX,  tile->position.x);
            topmostY   = std::min(topmostY,   tile->position.y);
            rightmostX = std::max(rightmostX, tile->position.x);
            bottommostY = std::max(bottommostY, tile->position.y);
        }

        // Convert bounding box to grid dimensions (number of columns and rows).
        int gridCols = static_cast<int>(std::round((rightmostX - leftmostX) / tileWidth))  + 1;
        int gridRows = static_cast<int>(std::round((bottommostY - topmostY) / tileHeight)) + 1;

        // Build a flat 2D array: solidTileIndex[row * gridCols + col] = index into
        // tilesOnLayer, or -1 if that grid cell is empty.
        std::vector<int> solidTileIndex(gridRows * gridCols, -1);
        for (int tileIdx = 0; tileIdx < static_cast<int>(tilesOnLayer.size()); ++tileIdx) {
            int col = static_cast<int>(std::round((tilesOnLayer[tileIdx]->position.x - leftmostX) / tileWidth));
            int row = static_cast<int>(std::round((tilesOnLayer[tileIdx]->position.y - topmostY)  / tileHeight));
            if (col >= 0 && col < gridCols && row >= 0 && row < gridRows)
                solidTileIndex[row * gridCols + col] = tileIdx;
        }

        // --- Greedy row-major rectangle merge ---
        // For each unmerged solid cell, extend right then down to form the
        // largest possible rectangle, then emit one merged collider for it.
        std::vector<bool> alreadyMerged(gridRows * gridCols, false);

        for (int startRow = 0; startRow < gridRows; ++startRow) {
            for (int startCol = 0; startCol < gridCols; ++startCol) {
                // Skip empty cells and cells already consumed by a previous rect.
                if (solidTileIndex[startRow * gridCols + startCol] < 0
                    || alreadyMerged[startRow * gridCols + startCol]) continue;

                // Extend right: find how many consecutive solid, unmerged cells
                // exist in this row starting at startCol.
                int endCol = startCol;
                while (endCol + 1 < gridCols
                    && solidTileIndex[startRow * gridCols + (endCol + 1)] >= 0
                    && !alreadyMerged[startRow * gridCols + (endCol + 1)])
                    ++endCol;
                int rectWidthInTiles = endCol - startCol + 1;

                // Extend down: keep adding rows as long as every cell in the
                // column range [startCol, endCol] is solid and unmerged.
                int endRow = startRow;
                while (endRow + 1 < gridRows) {
                    bool nextRowFullySolid = true;
                    for (int col = startCol; col <= endCol; ++col) {
                        if (solidTileIndex[(endRow + 1) * gridCols + col] < 0
                            || alreadyMerged[(endRow + 1) * gridCols + col]) {
                            nextRowFullySolid = false;
                            break;
                        }
                    }
                    if (!nextRowFullySolid) break;
                    ++endRow;
                }
                int rectHeightInTiles = endRow - startRow + 1;

                // Mark all cells in this rectangle as merged.
                for (int row = startRow; row <= endRow; ++row)
                    for (int col = startCol; col <= endCol; ++col)
                        alreadyMerged[row * gridCols + col] = true;

                // Compute the world-space center and pixel size of the merged rect.
                // Tile positions are their centers, so we average across the span.
                float rectCenterX = leftmostX + (startCol + (rectWidthInTiles  - 1) * 0.5f) * tileWidth;
                float rectCenterY = topmostY  + (startRow + (rectHeightInTiles - 1) * 0.5f) * tileHeight;
                float rectPixelWidth  = static_cast<float>(rectWidthInTiles)  * tileWidth;
                float rectPixelHeight = static_cast<float>(rectHeightInTiles) * tileHeight;

                // Create the merged collider entity (no sprite, physics-only).
                ECS::Entity mergedEntity = world.createEntity();

                Transform mergedTransform;
                mergedTransform.position = {rectCenterX, rectCenterY};
                world.addComponent<Transform>(mergedEntity, mergedTransform);

                Collider mergedCollider;
                mergedCollider.shape             = Collider::Shape::Box;
                mergedCollider.size              = {rectPixelWidth, rectPixelHeight};
                mergedCollider.isSolid           = true;
                mergedCollider.isTrigger         = false;
                mergedCollider.moveable          = false;
                mergedCollider.blocksLight       = false;
                mergedCollider.collisionCategory = Gameplay::Layer::Wall;
                mergedCollider.collisionMask     = 0xFFFFFFFF;
                mergedCollider.mass              = 0.f; // immoveable
                world.addComponent<Collider>(mergedEntity, mergedCollider);

                world.addComponent<Tag>(mergedEntity, Tag{"TilemapMergedCollider"});
                world.addComponent<Components::Layer>(mergedEntity, Components::Layer{layerIndex});

                ++totalMergedRects;
            }
        }

        // Disable isSolid on every individual tile entity that was absorbed into
        // a merged rect, so it no longer participates in solid collision.
        for (int cellIndex = 0; cellIndex < gridRows * gridCols; ++cellIndex) {
            if (!alreadyMerged[cellIndex] || solidTileIndex[cellIndex] < 0) continue;
            ECS::Entity originalTileEntity = tilesOnLayer[solidTileIndex[cellIndex]]->entity;
            auto* originalCollider = world.getComponent<Components::Collider>(originalTileEntity);
            if (originalCollider && originalCollider->isSolid) {
                originalCollider->isSolid = false;
                s_disabledTileEntities.push_back(originalTileEntity);
            }
        }
    }

    std::cout << "[TilemapCollisionBuilder] " << totalMergedRects
              << " merged colliders built from " << tiles.size() << " tiles\n";
}

} // namespace Systems
