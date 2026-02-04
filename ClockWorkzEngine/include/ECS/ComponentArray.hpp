/******************************************************************************/
/*!
\file           ComponentArray.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          Dense component storage for ECS architecture.

                ComponentArray stores components in a contiguous array for
                cache-friendly iteration. Uses sparse set pattern:
                - Dense array: Actual component data (cache-friendly)
                - Sparse map: Entity ID -> array index lookup

                This provides O(1) add/remove/get with excellent iteration
performance.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "Entity.hpp"
#include <array>
#include <cstddef>
#include <unordered_map>

namespace ECS {

/**
 * @brief Type-erased interface for component storage
 *
 * Allows World to manage different component types polymorphically.
 */
class IComponentArray {
public:
  virtual ~IComponentArray() = default;

  /**
   * @brief Called when an entity is destroyed
   * @param entity The entity being destroyed
   */
  virtual void entityDestroyed(Entity entity) = 0;

  /**
   * @brief Get number of components stored
   * @return Component count
   */
  virtual size_t size() const = 0;
};

/**
 * @brief Dense array storage for components of type T
 *
 * Uses sparse set pattern for O(1) operations with cache-friendly iteration.
 *
 * @tparam T Component type
 */
template <typename T> class ComponentArray : public IComponentArray {
private:
  static constexpr size_t MAX_ENTITIES = 10000;

  // Dense array of components (cache-friendly for iteration)
  std::array<T, MAX_ENTITIES> m_components;

  // Sparse maps for O(1) lookup
  std::unordered_map<uint32_t, size_t>
      m_entityToIndex; // Entity ID -> array index
  std::unordered_map<size_t, uint32_t>
      m_indexToEntity; // array index -> Entity ID

  // Track generation for each entity ID to detect stale references
  std::unordered_map<uint32_t, uint32_t> m_entityGeneration;

  size_t m_size{0};

public:
  /**
   * @brief Insert a component for an entity
   * @param entity The entity
   * @param component The component data
   */
  void insert(Entity entity, T component) {
    // Check if component already exists for this exact entity (same ID and generation)
    auto it = m_entityToIndex.find(entity.id);
    if (it != m_entityToIndex.end()) {
      // Verify generation matches - if not, the old entity was destroyed
      auto genIt = m_entityGeneration.find(entity.id);
      if (genIt != m_entityGeneration.end()) {
        if (genIt->second == entity.generation) {
          // Same entity, update component
          m_components[it->second] = component;
          return;
        }
        // Different generation - old entity was destroyed, remove stale data first
        remove(Entity{entity.id, genIt->second});
      } else {
        // No generation tracked but index exists - corrupted state, clean up
        // This shouldn't happen, but handle it defensively
        size_t removedIndex = it->second;
        size_t lastIndex = m_size - 1;
        if (removedIndex != lastIndex && m_size > 0) {
          m_components[removedIndex] = m_components[lastIndex];
          uint32_t lastEntityId = m_indexToEntity[lastIndex];
          m_entityToIndex[lastEntityId] = removedIndex;
          m_indexToEntity[removedIndex] = lastEntityId;
        }
        m_entityToIndex.erase(entity.id);
        if (m_size > 0) {
          m_indexToEntity.erase(lastIndex);
          m_size--;
        }
      }
    }

    if (m_size >= MAX_ENTITIES) {
      // TODO: Handle overflow (resize or error)
      return;
    }

    // Add to dense array
    m_entityToIndex[entity.id] = m_size;
    m_indexToEntity[m_size] = entity.id;
    m_entityGeneration[entity.id] = entity.generation;
    m_components[m_size] = component;
    m_size++;
  }

  /**
   * @brief Remove a component from an entity
   * @param entity The entity
   */
  void remove(Entity entity) {
    auto it = m_entityToIndex.find(entity.id);
    if (it == m_entityToIndex.end())
      return;

    // Verify generation matches to avoid removing wrong entity's component
    auto genIt = m_entityGeneration.find(entity.id);
    if (genIt != m_entityGeneration.end() && genIt->second != entity.generation) {
      return; // Stale reference, don't remove current entity's component
    }

    size_t removedIndex = it->second;
    size_t lastIndex = m_size - 1;

    // Swap with last element (maintains dense array)
    m_components[removedIndex] = m_components[lastIndex];

    // Update mappings for swapped element
    uint32_t lastEntityId = m_indexToEntity[lastIndex];
    m_entityToIndex[lastEntityId] = removedIndex;
    m_indexToEntity[removedIndex] = lastEntityId;

    // Remove old mappings
    m_entityToIndex.erase(entity.id);
    m_indexToEntity.erase(lastIndex);
    m_entityGeneration.erase(entity.id);

    m_size--;
  }

  /**
   * @brief Get a component for an entity
   * @param entity The entity
   * @return Pointer to component, or nullptr if not found or stale reference
   */
  T *get(Entity entity) {
    auto it = m_entityToIndex.find(entity.id);
    if (it == m_entityToIndex.end())
      return nullptr;

    // Verify generation to detect stale references
    auto genIt = m_entityGeneration.find(entity.id);
    if (genIt == m_entityGeneration.end() || genIt->second != entity.generation)
      return nullptr;

    return &m_components[it->second];
  }

  /**
   * @brief Get a component for an entity (const version)
   * @param entity The entity
   * @return Const pointer to component, or nullptr if not found or stale reference
   */
  const T *get(Entity entity) const {
    auto it = m_entityToIndex.find(entity.id);
    if (it == m_entityToIndex.end())
      return nullptr;

    // Verify generation to detect stale references
    auto genIt = m_entityGeneration.find(entity.id);
    if (genIt == m_entityGeneration.end() || genIt->second != entity.generation)
      return nullptr;

    return &m_components[it->second];
  }

  /**
   * @brief Check if entity has this component
   * @param entity The entity
   * @return True if component exists and generation matches
   */
  bool has(Entity entity) const {
    auto it = m_entityToIndex.find(entity.id);
    if (it == m_entityToIndex.end())
      return false;

    // Verify generation
    auto genIt = m_entityGeneration.find(entity.id);
    return genIt != m_entityGeneration.end() && genIt->second == entity.generation;
  }

  // IComponentArray interface
  void entityDestroyed(Entity entity) override { remove(entity); }

  size_t size() const override { return m_size; }

  /**
   * @brief Get direct access to dense component array
   * @return Pointer to component array
   */
  T *data() { return m_components.data(); }

  /**
   * @brief Get entity ID at given index in dense array
   * @param index Array index
   * @return Entity ID
   */
  uint32_t entityAt(size_t index) const {
    auto it = m_indexToEntity.find(index);
    return (it != m_indexToEntity.end()) ? it->second : 0;
  }
};

} // namespace ECS
