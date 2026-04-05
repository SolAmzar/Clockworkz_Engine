/******************************************************************************/
/*!
\file           TilemapCollisionBuilder.cpp
\project        ClockWorkz Engine
\author(s)      [Syafiq] 90% [Amzar] 10%
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
#include <limits>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Systems {

// Tracks which tile entities had isSolid disabled so clear() can restore them.
static std::vector<ECS::Entity> s_disabledTileEntities;

struct ColliderBounds {
    float minX{0.f};
    float minY{0.f};
    float maxX{0.f};
    float maxY{0.f};
};

static ColliderBounds getColliderBounds(const Components::Transform& transform,
                                        const Components::Collider& collider) {
    const vec2 center{
        transform.position.x + collider.offset.x,
        transform.position.y + collider.offset.y
    };
    const vec2 halfExtents{
        std::abs(collider.size.x * transform.scale.x) * 0.5f,
        std::abs(collider.size.y * transform.scale.y) * 0.5f
    };

    return {
        center.x - halfExtents.x,
        center.y - halfExtents.y,
        center.x + halfExtents.x,
        center.y + halfExtents.y
    };
}

static bool intervalsOverlapOrTouch(float minA, float maxA,
                                    float minB, float maxB,
                                    float epsilon = 0.001f) {
    return !(maxA < minB - epsilon || maxB < minA - epsilon);
}

static bool boundsOverlapOrTouch(const ColliderBounds& a,
                                 const ColliderBounds& b,
                                 float epsilon = 0.001f) {
    return intervalsOverlapOrTouch(a.minX, a.maxX, b.minX, b.maxX, epsilon)
        && intervalsOverlapOrTouch(a.minY, a.maxY, b.minY, b.maxY, epsilon);
}

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

struct MergeBucketKey {
    int layer{0};
    uint32_t combinedGroupId{0};

    bool operator==(const MergeBucketKey& other) const {
        return layer == other.layer && combinedGroupId == other.combinedGroupId;
    }
};

struct MergeBucketKeyHash {
    std::size_t operator()(const MergeBucketKey& key) const {
        std::size_t h1 = std::hash<int>{}(key.layer);
        std::size_t h2 = std::hash<uint32_t>{}(key.combinedGroupId);
        return h1 ^ (h2 + 0x9e3779b9u + (h1 << 6) + (h1 >> 2));
    }
};

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
        uint32_t combinedGroupId = 0;
        if (auto* collider = world.getComponent<Collider>(entity))
            combinedGroupId = collider->combinedGroupId;
        entries.push_back({transform.position, entity, layerIdx, combinedGroupId});
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

    std::unordered_map<MergeBucketKey, std::vector<const TilemapTileEntry*>, MergeBucketKeyHash> tilesByLayer;
    for (const auto& tile : tiles) {
        if (!world.isValid(tile.entity)) continue;
        auto* tileCollider = world.getComponent<Collider>(tile.entity);
        if (!tileCollider) continue;
        if (!tileCollider->isSolid) continue;
        if (tileCollider->isTrigger) continue;
        if (tileCollider->moveable) continue;
        if (tileCollider->shape != Collider::Shape::Box) continue; // skip circles
        if (isMergedColliderExcludedEntity(world, tile.entity)) continue;
        tilesByLayer[{tile.layer, tile.combinedGroupId}].push_back(&tile);
    }

    int totalMergedRects = 0;

    for (auto& [bucketKey, tilesOnLayer] : tilesByLayer) {
        if (tilesOnLayer.empty()) continue;

        std::vector<std::vector<const TilemapTileEntry*>> tileGroups;
        if (bucketKey.combinedGroupId == 0) {
            tileGroups.push_back(tilesOnLayer);
        } else {
            std::vector<ColliderBounds> tileBounds(tilesOnLayer.size());
            for (int tileIdx = 0; tileIdx < static_cast<int>(tilesOnLayer.size()); ++tileIdx) {
                auto* transform = world.getComponent<Transform>(tilesOnLayer[tileIdx]->entity);
                auto* collider = world.getComponent<Collider>(tilesOnLayer[tileIdx]->entity);
                if (!transform || !collider) continue;
                tileBounds[tileIdx] = getColliderBounds(*transform, *collider);
            }

            std::vector<bool> visited(tilesOnLayer.size(), false);
            for (int startIdx = 0; startIdx < static_cast<int>(tilesOnLayer.size()); ++startIdx) {
                if (visited[startIdx]) continue;
                visited[startIdx] = true;
                std::vector<int> stack{startIdx};
                std::vector<const TilemapTileEntry*> connectedGroup;

                while (!stack.empty()) {
                    const int currentIdx = stack.back();
                    stack.pop_back();
                    connectedGroup.push_back(tilesOnLayer[currentIdx]);

                    for (int nextIdx = 0; nextIdx < static_cast<int>(tilesOnLayer.size()); ++nextIdx) {
                        if (visited[nextIdx]) continue;
                        if (!boundsOverlapOrTouch(tileBounds[currentIdx], tileBounds[nextIdx]))
                            continue;
                        visited[nextIdx] = true;
                        stack.push_back(nextIdx);
                    }
                }

                if (!connectedGroup.empty())
                    tileGroups.push_back(std::move(connectedGroup));
            }
        }

        for (const auto& tilesInGroup : tileGroups) {
            if (tilesInGroup.empty()) continue;

            // --- Find the world-space bounding box of all colliders in this group ---
            float leftmostX = std::numeric_limits<float>::max();
            float topmostY = std::numeric_limits<float>::max();
            float rightmostX = std::numeric_limits<float>::lowest();
            float bottommostY = std::numeric_limits<float>::lowest();
            for (const auto* tile : tilesInGroup) {
                auto* transform = world.getComponent<Transform>(tile->entity);
                auto* collider = world.getComponent<Collider>(tile->entity);
                if (!transform || !collider) continue;

                const ColliderBounds bounds = getColliderBounds(*transform, *collider);
                leftmostX = std::min(leftmostX, bounds.minX);
                topmostY = std::min(topmostY, bounds.minY);
                rightmostX = std::max(rightmostX, bounds.maxX);
                bottommostY = std::max(bottommostY, bounds.maxY);
            }

            // Convert bounding box to grid dimensions (number of columns and rows).
            int gridCols = std::max(1, static_cast<int>(std::ceil((rightmostX - leftmostX) / tileWidth)));
            int gridRows = std::max(1, static_cast<int>(std::ceil((bottommostY - topmostY) / tileHeight)));

            // Build a flat 2D array: each grid cell stores every tile index whose
            // collider bounds overlap that cell.
            std::vector<std::vector<int>> solidTileIndices(gridRows * gridCols);
            for (int tileIdx = 0; tileIdx < static_cast<int>(tilesInGroup.size()); ++tileIdx) {
                auto* transform = world.getComponent<Transform>(tilesInGroup[tileIdx]->entity);
                auto* collider = world.getComponent<Collider>(tilesInGroup[tileIdx]->entity);
                if (!transform || !collider) continue;

                const ColliderBounds bounds = getColliderBounds(*transform, *collider);
                const float epsilon = 0.001f;
                int startCol = static_cast<int>(std::floor((bounds.minX - leftmostX) / tileWidth));
                int endCol = static_cast<int>(std::floor(((bounds.maxX - leftmostX) - epsilon) / tileWidth));
                int startRow = static_cast<int>(std::floor((bounds.minY - topmostY) / tileHeight));
                int endRow = static_cast<int>(std::floor(((bounds.maxY - topmostY) - epsilon) / tileHeight));

                startCol = std::clamp(startCol, 0, gridCols - 1);
                endCol = std::clamp(endCol, 0, gridCols - 1);
                startRow = std::clamp(startRow, 0, gridRows - 1);
                endRow = std::clamp(endRow, 0, gridRows - 1);

                for (int row = startRow; row <= endRow; ++row) {
                    for (int col = startCol; col <= endCol; ++col) {
                        solidTileIndices[row * gridCols + col].push_back(tileIdx);
                    }
                }
            }

            // --- Greedy row-major rectangle merge ---
            // For each unmerged solid cell, extend right then down to form the
            // largest possible rectangle, then emit one merged collider for it.
            std::vector<bool> alreadyMerged(gridRows * gridCols, false);

            for (int startRow = 0; startRow < gridRows; ++startRow) {
                for (int startCol = 0; startCol < gridCols; ++startCol) {
                    // Skip empty cells and cells already consumed by a previous rect.
                    if (solidTileIndices[startRow * gridCols + startCol].empty()
                        || alreadyMerged[startRow * gridCols + startCol]) continue;

                    // Extend right: find how many consecutive solid, unmerged cells
                    // exist in this row starting at startCol.
                    int endCol = startCol;
                    while (endCol + 1 < gridCols
                        && !solidTileIndices[startRow * gridCols + (endCol + 1)].empty()
                        && !alreadyMerged[startRow * gridCols + (endCol + 1)])
                        ++endCol;

                    // Extend down: keep adding rows as long as every cell in the
                    // column range [startCol, endCol] is solid and unmerged.
                    int endRow = startRow;
                    while (endRow + 1 < gridRows) {
                        bool nextRowFullySolid = true;
                        for (int col = startCol; col <= endCol; ++col) {
                            if (solidTileIndices[(endRow + 1) * gridCols + col].empty()
                                || alreadyMerged[(endRow + 1) * gridCols + col]) {
                                nextRowFullySolid = false;
                                break;
                            }
                        }
                        if (!nextRowFullySolid) break;
                        ++endRow;
                    }

                    // Mark all cells in this rectangle as merged.
                    for (int row = startRow; row <= endRow; ++row)
                        for (int col = startCol; col <= endCol; ++col)
                            alreadyMerged[row * gridCols + col] = true;

                    // Compute the merged bounds from the actual source colliders so
                    // authored offsets and slight overhangs are preserved instead of
                    // snapping the merged result back to the tile grid.
                    float mergedMinX = std::numeric_limits<float>::max();
                    float mergedMinY = std::numeric_limits<float>::max();
                    float mergedMaxX = std::numeric_limits<float>::lowest();
                    float mergedMaxY = std::numeric_limits<float>::lowest();
                    std::unordered_set<int> mergedSourceTiles;

                    for (int row = startRow; row <= endRow; ++row) {
                        for (int col = startCol; col <= endCol; ++col) {
                            const int cellIndex = row * gridCols + col;
                            for (int tileIndex : solidTileIndices[cellIndex]) {
                                if (!mergedSourceTiles.insert(tileIndex).second) continue;

                                const ECS::Entity sourceEntity = tilesInGroup[tileIndex]->entity;
                                auto* sourceTransform = world.getComponent<Transform>(sourceEntity);
                                auto* sourceCollider  = world.getComponent<Collider>(sourceEntity);
                                if (!sourceTransform || !sourceCollider) continue;

                                const ColliderBounds bounds = getColliderBounds(*sourceTransform, *sourceCollider);
                                mergedMinX = std::min(mergedMinX, bounds.minX);
                                mergedMinY = std::min(mergedMinY, bounds.minY);
                                mergedMaxX = std::max(mergedMaxX, bounds.maxX);
                                mergedMaxY = std::max(mergedMaxY, bounds.maxY);
                            }
                        }
                    }

                    if (mergedMinX > mergedMaxX || mergedMinY > mergedMaxY)
                        continue;

                    const float rectCenterX = (mergedMinX + mergedMaxX) * 0.5f;
                    const float rectCenterY = (mergedMinY + mergedMaxY) * 0.5f;
                    const float rectPixelWidth  = mergedMaxX - mergedMinX;
                    const float rectPixelHeight = mergedMaxY - mergedMinY;

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
                    mergedCollider.isCombined        = bucketKey.combinedGroupId != 0;
                    mergedCollider.combinedGroupId   = bucketKey.combinedGroupId;
                    mergedCollider.collisionCategory = Gameplay::Layer::Wall;
                    mergedCollider.collisionMask     = 0xFFFFFFFF;
                    mergedCollider.mass              = 0.f; // immoveable
                    world.addComponent<Collider>(mergedEntity, mergedCollider);

                    world.addComponent<Tag>(mergedEntity, Tag{"TilemapMergedCollider"});
                    world.addComponent<Components::Layer>(mergedEntity, Components::Layer{bucketKey.layer});

                    // Disable isSolid on every original tile collider absorbed into
                    // this merged rect so only the merged collider stays active.
                    for (int tileIndex : mergedSourceTiles) {
                        ECS::Entity originalTileEntity = tilesInGroup[tileIndex]->entity;
                        auto* originalCollider = world.getComponent<Components::Collider>(originalTileEntity);
                        if (originalCollider && originalCollider->isSolid) {
                            originalCollider->isSolid = false;
                            s_disabledTileEntities.push_back(originalTileEntity);
                        }
                    }

                    ++totalMergedRects;
                }
            }
        }
    }

    std::cout << "[TilemapCollisionBuilder] " << totalMergedRects
              << " merged colliders built from " << tiles.size() << " tiles\n";
}

} // namespace Systems
