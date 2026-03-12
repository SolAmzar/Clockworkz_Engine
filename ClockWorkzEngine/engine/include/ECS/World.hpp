/******************************************************************************/
/*!
\file           World.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon, Syafiq]
\brief          ECS World coordinator — manages entities and components.

                World is the main interface for all ECS operations:
                  - Entity creation / destruction with ID recycling.
                  - Component type registration and management.
                  - Query / iteration over entities with specific components.

                Memory management changes (custom Memory Manager integration):
                  - std::queue<uint32_t>   replaced by a fixed-size ring buffer
                    (m_recycleQueue[]) — no heap allocation.
                  - std::unordered_map + std::shared_ptr replaced by a flat
                    ComponentEntry registry array (m_componentRegistry[]) —
                    no heap allocation for the registry structure itself.
                  - Each ComponentArray<T> is allocated with ::operator new
                    at load time (one OS call per type) and constructed with
                    placement new. This is the "original allocation at load
                    time" described in the Memory Manager requirements.
                  - World::~World() explicitly calls ~IComponentArray() then
                    ::operator delete for every registered ComponentArray.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "ComponentArray.hpp"
#include "Entity.hpp"
#include "Memory/MemoryDebugRegistry.hpp"
#include <new>

namespace ECS {

/******************************************************************************/
/*!
\class  World
\brief  Main ECS coordinator.

        Manages entity lifecycle and component storage.
        Provides the primary API for game code to interact with the ECS.
*/
/******************************************************************************/
class World {
private:
    // -----------------------------------------------------------------------
    // Constants
    // -----------------------------------------------------------------------
    static constexpr size_t MAX_ENTITIES        = 10000;

    /*!
    \brief  Maximum number of distinct component types that can be registered.
            The engine currently registers 25 types; 32 provides headroom.
    */
    static constexpr size_t MAX_COMPONENT_TYPES = 32;

    // -----------------------------------------------------------------------
    // Entity management
    // -----------------------------------------------------------------------

    /*!
    \brief  Fixed-size ring buffer for recycled entity IDs.

            Replaces std::queue<uint32_t>. Supports push / pop / clear in
            O(1) with no heap allocation.
    */
    uint32_t m_recycleQueue[MAX_ENTITIES];
    size_t   m_recycleHead{0};   // Index of next ID to pop.
    size_t   m_recycleTail{0};   // Index where next ID will be pushed.
    size_t   m_recycleCount{0};  // Number of IDs currently in the queue.

    // Generation counter per entity slot for stale-handle detection.
    uint32_t m_generations[MAX_ENTITIES];

    uint32_t m_livingEntityCount{0};
    uint32_t m_nextId{1}; // 0 is reserved as the invalid entity ID.

    // -----------------------------------------------------------------------
    /*!
    \struct ComponentEntry
    \brief  Registry entry mapping a type hash to its ComponentArray.
    */
    // -----------------------------------------------------------------------
    struct ComponentEntry {
        size_t               typeHash{0};    // std::type_index::hash_code()
        IComponentArray*     array{nullptr}; // Raw pointer — no shared_ptr.
        Memory::IPoolStats*  stats{nullptr}; // Same object cast to IPoolStats for debug registry.
    };

    /// Flat array registry of all registered component types.
    ComponentEntry m_componentRegistry[MAX_COMPONENT_TYPES];

    /// Number of component types currently registered.
    size_t m_componentTypeCount{0};

public:
    // -----------------------------------------------------------------------
    // Constructor / Destructor
    // -----------------------------------------------------------------------

    World() {
        for (size_t i = 0; i < MAX_ENTITIES; ++i) { m_generations[i] = 0; }

        // Initialise registry to empty.
        for (size_t i = 0; i < MAX_COMPONENT_TYPES; ++i) {
            m_componentRegistry[i].typeHash = 0;
            m_componentRegistry[i].array    = nullptr;
        }

        // Initialise ring buffer storage.
        for (size_t i = 0; i < MAX_ENTITIES; ++i) {
            m_recycleQueue[i] = 0;
        }
    }

    /*!
    \brief  Destructor — explicitly destroys and frees every ComponentArray.
    */
    ~World() {
        // Ensure all entities are destroyed so component sizes become zero
        // and any heap resources in components are released before unregistering.
        destroyAll();

        for (size_t i = 0; i < m_componentTypeCount; ++i) {
            // Unregister from the debug registry before destruction so the
            // registry can record the final used-block count (leak detection).
            if (m_componentRegistry[i].stats) {
                Memory::MemoryDebugRegistry::Get().unregisterPool(
                    m_componentRegistry[i].stats);
                m_componentRegistry[i].stats = nullptr;
            }

            IComponentArray* arr = m_componentRegistry[i].array;
            if (arr) {
                arr->~IComponentArray();    // explicit virtual destructor call
                ::operator delete(arr);     // return raw memory to OS
                m_componentRegistry[i].array = nullptr;
            }
        }
    }

    // Non-copyable — World owns heap memory via ComponentArray pointers.
    World(const World&)            = delete;
    World& operator=(const World&) = delete;

    // ===== Entity Management =====

    // -----------------------------------------------------------------------
    /*!
    \brief  Create a new entity.

            Recycles a previously destroyed entity ID from the buffer
            if one is available; otherwise allocates a fresh ID.

    \return A valid Entity handle, or Entity{0,0} if MAX_ENTITIES exceeded.
    */
    // -----------------------------------------------------------------------
    Entity createEntity() {
        uint32_t id;

        if (!recycleEmpty()) {
            id = recyclePop();
        } else {
            id = m_nextId++;
            if (id >= MAX_ENTITIES) {
                return Entity{0, 0}; // invalid — overflow
            }
        }

        ++m_livingEntityCount;
        return Entity{id, m_generations[id]};
    }

    // -----------------------------------------------------------------------
    /*!
    \brief  Destroy an entity and remove all its components.

            Increments the entity's generation and pushes its ID back onto the recycled buffer.

    \param  entity  The entity to destroy.
    */
    // -----------------------------------------------------------------------
    void destroyEntity(Entity entity) {
        if (!isValid(entity)) return;

        for (size_t i = 0; i < m_componentTypeCount; ++i) {
            if (m_componentRegistry[i].array) {
                m_componentRegistry[i].array->entityDestroyed(entity);
            }
        }

        m_generations[entity.id]++;
        recyclePush(entity.id);
        --m_livingEntityCount;
    }

    // -----------------------------------------------------------------------
    /*!
    \brief  Check if an entity handle is currently valid.

    \param  entity  The entity to validate.
    \return True if the entity is alive and the generation matches.
    */
    // -----------------------------------------------------------------------
    bool isValid(Entity entity) const {
        if (!entity.isValid()) return false;
        if (entity.id >= MAX_ENTITIES) return false;
        return m_generations[entity.id] == entity.generation;
    }

    /*!
    \brief  Get the number of living entities.
    \return Current entity count.
    */
    uint32_t entityCount() const { return m_livingEntityCount; }

    // -----------------------------------------------------------------------
    /*!
    \brief  Destroy all entities (used on scene transitions).
    */
    // -----------------------------------------------------------------------
    void destroyAll() {
        // Clear the ring buffer — IDs in here are already dead.
        recycleClear();

        for (uint32_t id = 1; id < m_nextId; ++id) {
            Entity entity{id, m_generations[id]};
            for (size_t i = 0; i < m_componentTypeCount; ++i) {
                if (m_componentRegistry[i].array) {
                    m_componentRegistry[i].array->entityDestroyed(entity);
                }
            }
            m_generations[id]++;
            recyclePush(id);
        }

        m_livingEntityCount = 0;
    }

    // ===== Component Management =====

    // -----------------------------------------------------------------------
    /*!
    \brief  Register a component type with the World.

            Must be called once per component type (typically in Scene::Init)
            before any entity uses that component. Subsequent calls for the
            same type are silently ignored.

            Memory allocation strategy (load-time pre-allocation):
              1. ::operator new allocates raw memory for a ComponentArray<T>
                 object — this is the single OS-level allocation per type.
              2. Placement new constructs ComponentArray<T> in that memory.
              3. The raw pointer is stored in the flat m_componentRegistry[].
              4. No std::shared_ptr or std::make_shared is used.

    \tparam T  Component type to register.
    */
    // -----------------------------------------------------------------------
    template <typename T> void registerComponent() {
        size_t hash = typeid(T).hash_code();

        // Duplicate check — linear scan over at most MAX_COMPONENT_TYPES entries.
        for (size_t i = 0; i < m_componentTypeCount; ++i) {
            if (m_componentRegistry[i].typeHash == hash) return; // already registered
        }

        if (m_componentTypeCount >= MAX_COMPONENT_TYPES) {
            // Should never happen with MAX_COMPONENT_TYPES = 32 and 25 types.
            return;
        }

        // One OS-level allocation per component type.
        void* raw = ::operator new(sizeof(ComponentArray<T>));

        // Construct ComponentArray<T> in the pre-allocated raw memory.
        ComponentArray<T>* arr = new (raw) ComponentArray<T>();

        // Register with the Memory Debug Registry for the debug panel.
        // ComponentArray<T> implements both IComponentArray and IPoolStats.
        Memory::MemoryDebugRegistry::Get().registerPool(arr);

        m_componentRegistry[m_componentTypeCount].typeHash = hash;
        m_componentRegistry[m_componentTypeCount].array    = arr;
        m_componentRegistry[m_componentTypeCount].stats    = arr;
        ++m_componentTypeCount;
    }

    // -----------------------------------------------------------------------
    /*!
    \brief  Add (or update) a component on an entity.
    \tparam T  Component type.
    \param  entity     The target entity.
    \param  component  Component data to copy into the dense array.
    */
    // -----------------------------------------------------------------------
    template <typename T> void addComponent(Entity entity, T component) {
        auto* array = getComponentArray<T>();
        if (array) array->insert(entity, component);
    }

    // -----------------------------------------------------------------------
    /*!
    \brief  Remove a component from an entity.
    \tparam T  Component type.
    \param  entity  The entity to remove the component from.
    */
    // -----------------------------------------------------------------------
    template <typename T> void removeComponent(Entity entity) {
        auto* array = getComponentArray<T>();
        if (array) array->remove(entity);
    }

    // -----------------------------------------------------------------------
    /*!
    \brief  Get a pointer to an entity's component.
    \tparam T  Component type.
    \param  entity  The entity to query.
    \return Pointer to the component, or nullptr if not found.
    */
    // -----------------------------------------------------------------------
    template <typename T> T* getComponent(Entity entity) {
        auto* array = getComponentArray<T>();
        return array ? array->get(entity) : nullptr;
    }

    /*!
    \brief  Const overload of getComponent().
    */
    template <typename T> const T* getComponent(Entity entity) const {
        const auto* array = getComponentArray<T>();
        return array ? array->get(entity) : nullptr;
    }

    // -----------------------------------------------------------------------
    /*!
    \brief  Check whether an entity has a specific component.
    \tparam T  Component type.
    \param  entity  The entity to check.
    \return True if the entity has a live component of type T.
    */
    // -----------------------------------------------------------------------
    template <typename T> bool hasComponent(Entity entity) const {
        const auto* array = getComponentArray<T>();
        if (!array) return false;
        return array->has(entity);
    }

    // -----------------------------------------------------------------------
    /*!
    \brief  Get the number of entities that have component type T.
    \tparam T  Component type.
    \return Component count, or 0 if type is not registered.
    */
    // -----------------------------------------------------------------------
    template <typename T> size_t componentCount() const {
        const auto* array = getComponentArray<T>();
        return array ? array->size() : 0;
    }

    // ===== Query / Iteration =====

    // -----------------------------------------------------------------------
    /*!
    \brief  Iterate over all entities with a single component type.

    \tparam T     Component type to iterate.
    \tparam Func  Callable with signature void(Entity, T&).
    \param  func  Callback invoked once per entity that owns component T.
    */
    // -----------------------------------------------------------------------
    template <typename T, typename Func>
    void each(Func&& func) {
        auto* array = getComponentArray<T>();
        if (!array) return;

        size_t count = array->size();
        T*     comps = array->data();

        for (size_t i = 0; i < count; ++i) {
            uint32_t entityId = array->entityAt(i);
            Entity   entity{entityId, m_generations[entityId]};
            func(entity, comps[i]);
        }
    }

    // -----------------------------------------------------------------------
    /*!
    \brief  Iterate over entities that have two component types.

    \tparam T1    First component type.
    \tparam T2    Second component type.
    \tparam Func  Callable with signature void(Entity, T1&, T2&). Deduced from
                  the passed lambda — no std::function wrapping, no heap alloc.

    \param  func  Callback invoked for every entity that owns both T1 and T2.
    */
    // -----------------------------------------------------------------------
    template <typename T1, typename T2, typename Func>
    void each(Func&& func) {
        auto* array1 = getComponentArray<T1>();
        auto* array2 = getComponentArray<T2>();
        if (!array1 || !array2) return;

        size_t count   = array1->size();
        T1*    comps1  = array1->data();

        for (size_t i = 0; i < count; ++i) {
            uint32_t entityId = array1->entityAt(i);
            Entity   entity{entityId, m_generations[entityId]};

            T2* comp2 = array2->get(entity);
            if (comp2) {
                func(entity, comps1[i], *comp2);
            }
        }
    }

    // -----------------------------------------------------------------------
    /*!
    \brief  Iterate over entities that have three component types.

    \tparam T1    First component type.
    \tparam T2    Second component type.
    \tparam T3    Third component type.
    \tparam Func  Callable with signature void(Entity, T1&, T2&, T3&). Deduced
                  from the passed lambda — no std::function wrapping, no heap.

    \param  func  Callback invoked for every entity that owns T1, T2, and T3.
    */
    // -----------------------------------------------------------------------
    template <typename T1, typename T2, typename T3, typename Func>
    void each(Func&& func) {
        auto* array1 = getComponentArray<T1>();
        auto* array2 = getComponentArray<T2>();
        auto* array3 = getComponentArray<T3>();
        if (!array1 || !array2 || !array3) return;

        size_t count   = array1->size();
        T1*    comps1  = array1->data();

        for (size_t i = 0; i < count; ++i) {
            uint32_t entityId = array1->entityAt(i);
            Entity   entity{entityId, m_generations[entityId]};

            T2* comp2 = array2->get(entity);
            T3* comp3 = array3->get(entity);

            if (comp2 && comp3) {
                func(entity, comps1[i], *comp2, *comp3);
            }
        }
    }

private:
    // -----------------------------------------------------------------------
    // Ring buffer helpers (replace std::queue<uint32_t>)
    // -----------------------------------------------------------------------

    bool recycleEmpty() const { return m_recycleCount == 0; }

    /*!
    \brief  Push a recycled entity ID onto the ring buffer.
    \param  id  Entity ID to recycle.
    */
    void recyclePush(uint32_t id) {
        // Ring buffer cannot overflow if MAX_RECYCLE == MAX_ENTITIES.
        m_recycleQueue[m_recycleTail] = id;
        m_recycleTail = (m_recycleTail + 1) % MAX_ENTITIES;
        ++m_recycleCount;
    }

    /*!
    \brief  Pop and return the oldest recycled entity ID.
    \return The recycled entity ID.
    */
    uint32_t recyclePop() {
        uint32_t id   = m_recycleQueue[m_recycleHead];
        m_recycleHead = (m_recycleHead + 1) % MAX_ENTITIES;
        --m_recycleCount;
        return id;
    }

    /*!
    \brief  Clear the ring buffer (used by destroyAll()).
    */
    void recycleClear() {
        m_recycleHead  = 0;
        m_recycleTail  = 0;
        m_recycleCount = 0;
    }

    // -----------------------------------------------------------------------
    // Component array lookups (linear scan, max MAX_COMPONENT_TYPES entries)
    // -----------------------------------------------------------------------

    /*!
    \brief  Get the ComponentArray<T> for type T, or nullptr if unregistered.
    \tparam T  Component type.
    */
    template <typename T> ComponentArray<T>* getComponentArray() {
        size_t hash = typeid(T).hash_code();
        for (size_t i = 0; i < m_componentTypeCount; ++i) {
            if (m_componentRegistry[i].typeHash == hash) {
                return static_cast<ComponentArray<T>*>(m_componentRegistry[i].array);
            }
        }
        return nullptr;
    }

    /*!
    \brief  Const overload of getComponentArray().
    */
    template <typename T> const ComponentArray<T>* getComponentArray() const {
        size_t hash = typeid(T).hash_code();
        for (size_t i = 0; i < m_componentTypeCount; ++i) {
            if (m_componentRegistry[i].typeHash == hash) {
                return static_cast<const ComponentArray<T>*>(m_componentRegistry[i].array);
            }
        }
        return nullptr;
    }
};

} // namespace ECS
