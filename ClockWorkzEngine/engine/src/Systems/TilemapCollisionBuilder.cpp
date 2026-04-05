/******************************************************************************/
/*!
\file           TilemapCollisionBuilder.cpp
\project        ClockWorkz Engine
\author(s)      [Syafiq] 99% [Sol-Amzar] 1%
\brief          Greedy-rectangle tilemap collision merging implementation.

All content © 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#include "Systems/TilemapCollisionBuilder.hpp"
#include "ECS/World.hpp"
#include "Components/Transform.hpp"
#include "Components/Collider.hpp"
#include "Components/LuaScript.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/Tag.hpp"
#include "Components/Layer.hpp"
#include "Physics/Collision.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Systems {

// Tracks which tile entities had isSolid disabled so clear() can restore them.
static std::vector<ECS::Entity> s_disabledTileEntities;

namespace {

struct MergeCandidate {
    ECS::Entity entity{};
    int layer{0};
    vec2 snappedPosition{};
};

static float snapToGridCenter(float value, float gridSize) {
    const float half = gridSize * 0.5f;
    return std::round((value - half) / gridSize) * gridSize + half;
}

static bool isNearGridCenter(float value, float gridSize, float tolerance) {
    return std::abs(value - snapToGridCenter(value, gridSize)) <= tolerance;
}

static bool isAxisAlignedRotation(float radians, float tolerance) {
    const float pi = 3.14159265358979323846f;
    const float wrapped = std::remainder(radians, pi);
    return std::abs(wrapped) <= tolerance;
}

static bool isLikelyWallTexture(std::string_view textureId) {
    return textureId.find("wall") != std::string_view::npos
        || textureId.rfind("corridor wall", 0) == 0
        || textureId.rfind("hse c", 0) == 0
        || textureId == "house entrance";
}

static bool isLikelyWallTag(std::string_view tagName) {
    return tagName.find("wall") != std::string_view::npos;
}

static bool hasWallMergeIdentity(std::string_view tagName, std::string_view textureId) {
    return isLikelyWallTag(tagName) || isLikelyWallTexture(textureId);
}

} // namespace

static bool isMergedColliderExcludedEntity(ECS::World& world, ECS::Entity entity) {
    if (auto* tag = world.getComponent<Components::Tag>(entity)) {
        if (tag->tag == "Player" || tag->tag == "Enemy"
            || tag->tag == "Enemy (Clone)"
            || tag->tag == "TilemapMergedCollider")
            return true;
    }

    if (auto* lua = world.getComponent<Components::LuaScript>(entity)) {
        for (const auto& script : lua->scripts) {
            if (script.scriptPath == "player_controller.lua"
                || script.scriptPath == "components/enemy_ai.lua"
                || script.scriptPath == "components/moth_ai.lua") {
                return true;
            }
        }
    }

    return false;
}

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

bool TilemapCollisionBuilder::shouldMergeEntity(ECS::World& world,
                                                ECS::Entity entity,
                                                float tileWidth,
                                                float tileHeight,
                                                float positionTolerance,
                                                float rotationTolerance) {
    using namespace Components;

    if (!world.isValid(entity) || tileWidth <= 0.f || tileHeight <= 0.f) return false;

    auto* transform = world.getComponent<Transform>(entity);
    auto* collider = world.getComponent<Collider>(entity);
    if (!transform || !collider) return false;

    if (!collider->isSolid || collider->isTrigger || collider->moveable) return false;
    if (collider->shape != Collider::Shape::Box) return false;
    if (!collider->blocksLight) return false;
    if (!isAxisAlignedRotation(transform->rotation, rotationTolerance)) return false;
    if (std::abs(collider->size.x - tileWidth) > positionTolerance
        || std::abs(collider->size.y - tileHeight) > positionTolerance) {
        return false;
    }
    if (!isNearGridCenter(transform->position.x, tileWidth, positionTolerance)
        || !isNearGridCenter(transform->position.y, tileHeight, positionTolerance)) {
        return false;
    }
    if (isMergedColliderExcludedEntity(world, entity)) return false;

    const auto* tag = world.getComponent<Tag>(entity);
    const auto* sprite = world.getComponent<SpriteRenderer>(entity);
    const std::string_view tagName = tag ? std::string_view(tag->tag) : std::string_view{};
    const std::string_view textureId = sprite ? std::string_view(sprite->textureId) : std::string_view{};

    return hasWallMergeIdentity(tagName, textureId);
}

// ---------------------------------------------------------------------------

void TilemapCollisionBuilder::normalizePersistedMergedScene(ECS::World& world,
                                                            float tileWidth,
                                                            float tileHeight,
                                                            float positionTolerance,
                                                            float rotationTolerance) {
    using namespace Components;

    bool hasMergedColliders = false;
    world.each<Tag>([&](ECS::Entity, Tag& tag) {
        if (tag.tag == "TilemapMergedCollider")
            hasMergedColliders = true;
    });
    if (!hasMergedColliders || tileWidth <= 0.f || tileHeight <= 0.f)
        return;

    // Restore likely wall-source tiles that were serialized with isSolid disabled
    // because a previous save had already emitted merged colliders.
    world.each<Transform>([&](ECS::Entity entity, Transform& transform) {
        auto* collider = world.getComponent<Collider>(entity);
        if (!collider) return;
        if (collider->isTrigger || collider->moveable) return;
        if (collider->shape != Collider::Shape::Box) return;
        if (!collider->blocksLight) return;
        if (std::abs(collider->size.x - tileWidth) > positionTolerance
            || std::abs(collider->size.y - tileHeight) > positionTolerance) {
            return;
        }
        if (!isAxisAlignedRotation(transform.rotation, rotationTolerance)) return;
        if (!isNearGridCenter(transform.position.x, tileWidth, positionTolerance)
            || !isNearGridCenter(transform.position.y, tileHeight, positionTolerance)) {
            return;
        }
        if (isMergedColliderExcludedEntity(world, entity)) return;

        const auto* tag = world.getComponent<Tag>(entity);
        const auto* sprite = world.getComponent<SpriteRenderer>(entity);
        const std::string_view tagName = tag ? std::string_view(tag->tag) : std::string_view{};
        const std::string_view textureId = sprite ? std::string_view(sprite->textureId) : std::string_view{};
        if (!hasWallMergeIdentity(tagName, textureId)) return;

        collider->isSolid = true;
    });

    clear(world);

    std::vector<TilemapTileEntry> entries;
    world.each<Transform>([&](ECS::Entity entity, Transform& transform) {
        if (!shouldMergeEntity(world, entity, tileWidth, tileHeight, positionTolerance, rotationTolerance))
            return;

        int layerIdx = 0;
        if (auto* layer = world.getComponent<Layer>(entity))
            layerIdx = layer->index;
        entries.push_back({transform.position, entity, layerIdx});
    });

    if (!entries.empty())
        build(world, entries, tileWidth, tileHeight);
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
    std::unordered_map<int, std::vector<MergeCandidate>> tilesByLayer;
    for (const auto& tile : tiles) {
        if (!shouldMergeEntity(world, tile.entity, tileWidth, tileHeight)) continue;

        tilesByLayer[tile.layer].push_back({
            tile.entity,
            tile.layer,
            {
                snapToGridCenter(tile.position.x, tileWidth),
                snapToGridCenter(tile.position.y, tileHeight)
            }
        });
    }

    int totalMergedRects = 0;

    for (auto& [layerIndex, tilesOnLayer] : tilesByLayer) {
        if (tilesOnLayer.empty()) continue;

        // --- Find the world-space bounding box of all tiles on this layer ---
        float leftmostX  = tilesOnLayer[0].snappedPosition.x;
        float topmostY   = tilesOnLayer[0].snappedPosition.y;
        float rightmostX = leftmostX;
        float bottommostY = topmostY;
        for (const auto& tile : tilesOnLayer) {
            leftmostX  = std::min(leftmostX,  tile.snappedPosition.x);
            topmostY   = std::min(topmostY,   tile.snappedPosition.y);
            rightmostX = std::max(rightmostX, tile.snappedPosition.x);
            bottommostY = std::max(bottommostY, tile.snappedPosition.y);
        }

        // Convert bounding box to grid dimensions (number of columns and rows).
        int gridCols = static_cast<int>(std::round((rightmostX - leftmostX) / tileWidth))  + 1;
        int gridRows = static_cast<int>(std::round((bottommostY - topmostY) / tileHeight)) + 1;

        // Build a flat 2D array: solidTileIndex[row * gridCols + col] = index into
        // tilesOnLayer, or -1 if that grid cell is empty.
        std::vector<int> solidTileIndex(gridRows * gridCols, -1);
        for (int tileIdx = 0; tileIdx < static_cast<int>(tilesOnLayer.size()); ++tileIdx) {
            int col = static_cast<int>(std::round((tilesOnLayer[tileIdx].snappedPosition.x - leftmostX) / tileWidth));
            int row = static_cast<int>(std::round((tilesOnLayer[tileIdx].snappedPosition.y - topmostY)  / tileHeight));
            if (col >= 0 && col < gridCols && row >= 0 && row < gridRows
                && solidTileIndex[row * gridCols + col] < 0) {
                solidTileIndex[row * gridCols + col] = tileIdx;
            }
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
                mergedTransform.snapToGrid = true;
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
            ECS::Entity originalTileEntity = tilesOnLayer[solidTileIndex[cellIndex]].entity;
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
