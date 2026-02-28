/******************************************************************************/
/*!
\file           ComponentArray.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon, Syafiq]
\brief          Dense component storage for ECS architecture.

                ComponentArray stores components in a contiguous array for
                cache-friendly iteration. Uses the sparse set pattern:
                  - Dense array  : Actual component data (cache-friendly).
                  - Sparse arrays: O(1) entity-ID -> dense-index lookup.

                All index bookkeeping uses plain fixed-size uint32_t arrays
                instead of std::unordered_map, eliminating all heap
                allocations from the component storage layer. Memory for
                ComponentArray objects themselves is allocated via placement
                new inside World::registerComponent<T>() — see World.hpp.

                IPoolStats integration:
                  - ComponentArray<T> implements Memory::IPoolStats so it
                    can be registered with MemoryDebugRegistry and shown in
                    the Memory Debug Panel.
                  - Peak usage (m_peakSize) is tracked in insert().
                  - Registration / unregistration is handled by World, NOT
                    by ComponentArray's constructor/destructor, because the
                    component type name (typeid(T).name()) is used as the
                    pool name and the registry needs it at registration time.

                Allocation summary:
                  - m_components[]    : inline dense array, no heap
                  - m_sparse[]        : inline sparse array, no heap
                  - m_dense[]         : inline reverse-lookup array, no heap
                  - m_generationArr[] : inline generation cache, no heap

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "Entity.hpp"
#include "Memory/IPoolStats.hpp"
#include <climits>    // UINT32_MAX
#include <cstddef>
#include <typeinfo>   // typeid

namespace ECS {

/******************************************************************************/
/*!
\class  IComponentArray
\brief  Type-erased interface for component storage.

        Allows World to manage different component types polymorphically
        without knowing the concrete component type T.
*/
/******************************************************************************/
class IComponentArray {
public:
    virtual ~IComponentArray() = default;

    //Called when an entity is destroyed.
    virtual void entityDestroyed(Entity entity) = 0;

    // Get number of active components stored.
    virtual size_t size() const = 0;
};

/******************************************************************************/
/*!
\class  ComponentArray
\brief  Dense array storage for components of type T.
\tparam T  Component type (must be copy-assignable).
*/
/******************************************************************************/
template <typename T>
class ComponentArray : public IComponentArray, public Memory::IPoolStats {
private:
    // -----------------------------------------------------------------------
    // Constants
    // -----------------------------------------------------------------------
    static constexpr size_t   MAX_ENTITIES  = 10000;

    // UINT32_MAX is safe because valid entity IDs are in [1, MAX_ENTITIES).
    static constexpr uint32_t INVALID_INDEX = UINT32_MAX;

    // Entity ID 0 is reserved as invalid by World.

    static constexpr uint32_t INVALID_ID    = 0;

    // -----------------------------------------------------------------------
    // Component data (dense array — cache-friendly for iteration)
    // -----------------------------------------------------------------------

    /// Actual component values, packed from index 0 to m_size-1.
    T m_components[MAX_ENTITIES];

    // -----------------------------------------------------------------------
    // Sparse-set index arrays (inline, no heap)
    // -----------------------------------------------------------------------

    /// entity ID -> index in the dense arrays. INVALID_INDEX if absent.
    uint32_t m_sparse[MAX_ENTITIES];

    /// Dense reverse-lookup: dense index -> entity ID.
    uint32_t m_dense[MAX_ENTITIES];

    /// Generation cache: entity ID -> generation at insert time.
    uint32_t m_generationArr[MAX_ENTITIES];

    /// Number of active components currently stored.
    size_t m_size{0};

    /// Peak number of simultaneously active components (for debug panel).
    size_t m_peakSize{0};

public:
    // -----------------------------------------------------------------------
    /*!
    \brief  Constructor — initialises all index arrays to sentinel values.
    */
    // -----------------------------------------------------------------------
    ComponentArray() : m_size(0), m_peakSize(0) {
        for (size_t i = 0; i < MAX_ENTITIES; ++i) {
            m_sparse[i]        = INVALID_INDEX;
            m_dense[i]         = INVALID_ID;
            m_generationArr[i] = 0;
        }
    }

    // -----------------------------------------------------------------------
    /*!
    \brief  Insert (or update) a component for an entity.

    \param  entity     The owning entity.
    \param  component  Component data (copied into the dense array).
    */
    // -----------------------------------------------------------------------
    void insert(Entity entity, T component) {
        if (entity.id == 0 || entity.id >= MAX_ENTITIES) return;

        if (m_sparse[entity.id] != INVALID_INDEX) {
            if (m_generationArr[entity.id] == entity.generation) {
                // Same generation -> update in-place.
                m_components[m_sparse[entity.id]] = component;
                return;
            }
            // Stale slot from a recycled entity ID — remove before re-inserting.
            Entity stale{entity.id, m_generationArr[entity.id]};
            remove(stale);
        }

        // overflow guard
        if (m_size >= MAX_ENTITIES) return; 

        size_t newIdx                  = m_size;
        m_sparse[entity.id]            = static_cast<uint32_t>(newIdx);
        m_dense[newIdx]                = entity.id;
        m_generationArr[entity.id]     = entity.generation;
        m_components[newIdx]           = component;
        ++m_size;

        // Track peak usage for the debug panel.
        if (m_size > m_peakSize) {
            m_peakSize = m_size;
        }
    }

    // -----------------------------------------------------------------------
    /*!
    \brief  Remove a component from an entity

    \param  entity  The entity whose component should be removed.
    */
    // -----------------------------------------------------------------------
    void remove(Entity entity) {
        if (entity.id == 0 || entity.id >= MAX_ENTITIES)       return;
        if (m_sparse[entity.id] == INVALID_INDEX)              return;
        if (m_generationArr[entity.id] != entity.generation)   return;

        size_t removedIdx = m_sparse[entity.id];
        size_t lastIdx    = m_size - 1;

        if (removedIdx != lastIdx && m_size > 0) {
            m_components[removedIdx] = m_components[lastIdx];
            uint32_t movedId         = m_dense[lastIdx];
            m_dense[removedIdx]      = movedId;
            m_sparse[movedId]        = static_cast<uint32_t>(removedIdx);
        }

        m_sparse[entity.id] = INVALID_INDEX;
        m_dense[lastIdx]    = INVALID_ID;
        --m_size;
    }

    // -----------------------------------------------------------------------
    /*!
    \brief  Get a pointer to the component for an entity.
    \return Pointer to the component, or nullptr if not found / stale.
    */
    // -----------------------------------------------------------------------
    T* get(Entity entity) {
        if (entity.id == 0 || entity.id >= MAX_ENTITIES)     return nullptr;
        if (m_sparse[entity.id] == INVALID_INDEX)            return nullptr;
        if (m_generationArr[entity.id] != entity.generation) return nullptr;
        return &m_components[m_sparse[entity.id]];
    }

    const T* get(Entity entity) const {
        if (entity.id == 0 || entity.id >= MAX_ENTITIES)     return nullptr;
        if (m_sparse[entity.id] == INVALID_INDEX)            return nullptr;
        if (m_generationArr[entity.id] != entity.generation) return nullptr;
        return &m_components[m_sparse[entity.id]];
    }

    // -----------------------------------------------------------------------
    /*!
    \brief  Check if an entity has this component type.
    */
    // -----------------------------------------------------------------------
    bool has(Entity entity) const {
        if (entity.id == 0 || entity.id >= MAX_ENTITIES)    return false;
        if (m_sparse[entity.id] == INVALID_INDEX)           return false;
        return m_generationArr[entity.id] == entity.generation;
    }

    // -----------------------------------------------------------------------
    // IComponentArray interface
    // -----------------------------------------------------------------------

    void   entityDestroyed(Entity entity) override { remove(entity); }
    size_t size()                   const override { return m_size; }

    // -----------------------------------------------------------------------
    /*!
    \brief  Direct access to the dense component array (for systems).
    */
    // -----------------------------------------------------------------------
    T*       data()       { return m_components; }
    const T* data() const { return m_components; }

    // -----------------------------------------------------------------------
    /*!
    \brief  Entity ID at a given dense array index (for World::each<>()).
    */
    // -----------------------------------------------------------------------
    uint32_t entityAt(size_t index) const {
        if (index >= m_size) return 0;
        return m_dense[index];
    }

    // -----------------------------------------------------------------------
    // IPoolStats interface — exposes component-slot usage to the debug panel.
    // -----------------------------------------------------------------------

    /*!
    \brief  Pool name: RTTI type name of T (e.g. "struct Components::Transform").
            typeid(T).name() returns a static string on MSVC that is
            human-readable without demangling.
    */
    const char* poolName()       const override { return typeid(T).name(); }

    /// Byte size of one component slot.
    size_t      blockSize()      const override { return sizeof(T); }

    /// Number of active component slots (live entities with this component).
    size_t      usedBlocks()     const override { return m_size; }

    /// Number of available (not yet used) component slots.
    size_t      freeBlocks()     const override { return MAX_ENTITIES - m_size; }

    /// Total component slots in the dense array.
    size_t      totalBlocks()    const override { return MAX_ENTITIES; }

    /// Highest number of simultaneously active slots ever recorded.
    size_t      peakUsedBlocks() const override { return m_peakSize; }
};

} // namespace ECS
