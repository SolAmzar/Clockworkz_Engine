/******************************************************************************/
/*!
\file           World.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          ECS World coordinator - manages entities and components.

                World is the main interface for ECS operations:
                - Entity creation/destruction with ID reuse
                - Component registration and management
                - Query/iteration over entities with specific components

                This is the primary API that game code interacts with.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "ComponentArray.hpp"
#include "Entity.hpp"
#include <array>
#include <functional>
#include <memory>
#include <queue>
#include <typeindex>
#include <unordered_map>

namespace ECS {

/**
 * @brief Main ECS coordinator
 *
 * Manages entity lifecycle and component storage.
 * Provides the primary API for ECS operations.
 */
class World {
private:
  static constexpr size_t MAX_ENTITIES = 10000;

  // Entity management
  std::queue<uint32_t> m_availableIds;
  std::array<uint32_t, MAX_ENTITIES> m_generations;
  uint32_t m_livingEntityCount{0};
  uint32_t m_nextId{1}; // 0 is reserved for invalid

  // Component management
  std::unordered_map<std::type_index, std::shared_ptr<IComponentArray>>
      m_componentArrays;

public:
  World() { m_generations.fill(0); }

  ~World() = default;

  // ===== Entity Management =====

  /**
   * @brief Create a new entity
   * @return Entity handle
   */
  Entity createEntity() {
    uint32_t id;

    // Reuse ID from destroyed entities
    if (!m_availableIds.empty()) {
      id = m_availableIds.front();
      m_availableIds.pop();
    } else {
      id = m_nextId++;
      if (id >= MAX_ENTITIES) {
        // TODO: Handle overflow
        return Entity{0, 0}; // Invalid entity
      }
    }

    m_livingEntityCount++;
    return Entity{id, m_generations[id]};
  }

  /**
   * @brief Destroy an entity and all its components
   * @param entity The entity to destroy
   */
  void destroyEntity(Entity entity) {
    // Must check isValid() which verifies BOTH that ID is non-zero AND generation matches
    // This prevents stale references from corrupting valid entities
    if (!isValid(entity))
      return;

    // Notify all component arrays to remove this entity's components
    for (auto &[type, array] : m_componentArrays) {
      array->entityDestroyed(entity);
    }

    // Increment generation to invalidate old handles
    m_generations[entity.id]++;

    // Make ID available for reuse
    m_availableIds.push(entity.id);

    m_livingEntityCount--;
  }

  /**
   * @brief Check if an entity is valid
   * @param entity The entity to check
   * @return True if entity is valid
   */
  bool isValid(Entity entity) const {
    if (!entity.isValid())
      return false;
    if (entity.id >= MAX_ENTITIES)
      return false;
    return m_generations[entity.id] == entity.generation;
  }

  /**
   * @brief Get number of living entities
   * @return Entity count
   */
  uint32_t entityCount() const { return m_livingEntityCount; }

  /**
   * @brief Destroy all entities
   *
   * Clears all entities from the world. Useful for scene transitions.
   */
  void destroyAll() {
    // Clear the recycling queue first — these IDs are already dead.
    // Without this, destroyAll re-destroys dead entities (their current
    // generation always passes isValid), pushing duplicate IDs into the
    // queue. Later createEntity pops the same ID twice, giving two
    // entities the same handle, so creating one overwrites the other.
    while (!m_availableIds.empty()) {
      m_availableIds.pop();
    }

    // Destroy all entities and recycle their IDs
    for (uint32_t id = 1; id < m_nextId; ++id) {
      Entity entity{id, m_generations[id]};
      for (auto &[type, array] : m_componentArrays) {
        array->entityDestroyed(entity);
      }
      m_generations[id]++;
      m_availableIds.push(id);
    }

    m_livingEntityCount = 0;
  }

  // ===== Component Management =====

  /**
   * @brief Register a component type
   *
   * Must be called before using a component type.
   *
   * @tparam T Component type
   */
  template <typename T> void registerComponent() {
    std::type_index typeIndex(typeid(T));

    // Don't register twice
    if (m_componentArrays.find(typeIndex) != m_componentArrays.end()) {
      return;
    }

    m_componentArrays[typeIndex] = std::make_shared<ComponentArray<T>>();
  }

  /**
   * @brief Add a component to an entity
   * @tparam T Component type
   * @param entity The entity
   * @param component The component data
   */
  template <typename T> void addComponent(Entity entity, T component) {
    getComponentArray<T>()->insert(entity, component);
  }

  /**
   * @brief Remove a component from an entity
   * @tparam T Component type
   * @param entity The entity
   */
  template <typename T> void removeComponent(Entity entity) {
    getComponentArray<T>()->remove(entity);
  }

  /**
   * @brief Get a component from an entity
   * @tparam T Component type
   * @param entity The entity
   * @return Pointer to component, or nullptr if not found
   */
  template <typename T> T *getComponent(Entity entity) {
    return getComponentArray<T>()->get(entity);
  }

  /**
   * @brief Get a component from an entity (const version)
   * @tparam T Component type
   * @param entity The entity
   * @return Const pointer to component, or nullptr if not found
   */
  template <typename T> const T *getComponent(Entity entity) const {
    return getComponentArray<T>()->get(entity);
  }

  /**
   * @brief Check if entity has a component
   * @tparam T Component type
   * @param entity The entity
   * @return True if component exists
   */
  template <typename T> bool hasComponent(Entity entity) const {
    auto *array = getComponentArray<T>();
    if (!array)
      return false;
    return array->has(entity);
  }

  /**
   * @brief Get number of entities with a component type
   * @tparam T Component type
   * @return Component count
   */
  template <typename T> size_t componentCount() const {
    auto *array = getComponentArray<T>();
    return array ? array->size() : 0;
  }

  // ===== Query/Iteration =====

  /**
   * @brief Iterate over all entities with a single component type
   * @tparam T Component type
   * @param func Callback function(Entity, T&)
   */
  template <typename T> void each(std::function<void(Entity, T &)> func) {
    auto *array = getComponentArray<T>();
    if (!array)
      return;

    size_t count = array->size();
    T *components = array->data();

    for (size_t i = 0; i < count; ++i) {
      uint32_t entityId = array->entityAt(i);
      Entity entity{entityId, m_generations[entityId]};
      func(entity, components[i]);
    }
  }

  /**
   * @brief Iterate over entities with two component types
   * @tparam T1 First component type
   * @tparam T2 Second component type
   * @param func Callback function(Entity, T1&, T2&)
   */
  template <typename T1, typename T2>
  void each(std::function<void(Entity, T1 &, T2 &)> func) {
    auto *array1 = getComponentArray<T1>();
    auto *array2 = getComponentArray<T2>();
    if (!array1 || !array2)
      return;

    // Iterate over smaller array for efficiency
    size_t count = array1->size();
    T1 *components1 = array1->data();

    for (size_t i = 0; i < count; ++i) {
      uint32_t entityId = array1->entityAt(i);
      Entity entity{entityId, m_generations[entityId]};

      // Check if entity has second component
      T2 *comp2 = array2->get(entity);
      if (comp2) {
        func(entity, components1[i], *comp2);
      }
    }
  }

  /**
   * @brief Iterate over entities with three component types
   * @tparam T1 First component type
   * @tparam T2 Second component type
   * @tparam T3 Third component type
   * @param func Callback function(Entity, T1&, T2&, T3&)
   */
  template <typename T1, typename T2, typename T3>
  void each(std::function<void(Entity, T1 &, T2 &, T3 &)> func) {
    auto *array1 = getComponentArray<T1>();
    auto *array2 = getComponentArray<T2>();
    auto *array3 = getComponentArray<T3>();
    if (!array1 || !array2 || !array3)
      return;

    size_t count = array1->size();
    T1 *components1 = array1->data();

    for (size_t i = 0; i < count; ++i) {
      uint32_t entityId = array1->entityAt(i);
      Entity entity{entityId, m_generations[entityId]};

      T2 *comp2 = array2->get(entity);
      T3 *comp3 = array3->get(entity);

      if (comp2 && comp3) {
        func(entity, components1[i], *comp2, *comp3);
      }
    }
  }

private:
  // @brief Get component array for type T
  // @tparam T Component type
  // @return Pointer to component array
  template <typename T> ComponentArray<T> *getComponentArray() {
    std::type_index typeIndex(typeid(T));
    auto it = m_componentArrays.find(typeIndex);
    if (it == m_componentArrays.end())
      return nullptr;
    return static_cast<ComponentArray<T> *>(it->second.get());
  }

  // @brief Get component array for type T (const version)
  // @tparam T Component type
  // @return Const pointer to component array
  template <typename T> const ComponentArray<T> *getComponentArray() const {
    std::type_index typeIndex(typeid(T));
    auto it = m_componentArrays.find(typeIndex);
    if (it == m_componentArrays.end())
      return nullptr;
    return static_cast<const ComponentArray<T> *>(it->second.get());
  }
};

} // namespace ECS
