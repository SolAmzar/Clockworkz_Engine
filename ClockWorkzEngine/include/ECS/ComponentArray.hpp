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

  size_t m_size{0};

public:
  /**
   * @brief Insert a component for an entity
   * @param entity The entity
   * @param component The component data
   */
  void insert(Entity entity, T component) {
    if (m_size >= MAX_ENTITIES) {
      // TODO: Handle overflow (resize or error)
      return;
    }

    // Add to dense array
    m_entityToIndex[entity.id] = m_size;
    m_indexToEntity[m_size] = entity.id;
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

    m_size--;
  }

  /**
   * @brief Get a component for an entity
   * @param entity The entity
   * @return Pointer to component, or nullptr if not found
   */
  T *get(Entity entity) {
    auto it = m_entityToIndex.find(entity.id);
    if (it == m_entityToIndex.end())
      return nullptr;
    return &m_components[it->second];
  }

  /**
   * @brief Get a component for an entity (const version)
   * @param entity The entity
   * @return Const pointer to component, or nullptr if not found
   */
  const T *get(Entity entity) const {
    auto it = m_entityToIndex.find(entity.id);
    if (it == m_entityToIndex.end())
      return nullptr;
    return &m_components[it->second];
  }

  /**
   * @brief Check if entity has this component
   * @param entity The entity
   * @return True if component exists
   */
  bool has(Entity entity) const {
    return m_entityToIndex.find(entity.id) != m_entityToIndex.end();
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
