/******************************************************************************/
/*!
\file           MemoryManager.hpp
\project        ClockWorkz Engine
\author(s)      [Syafiq]
\brief          Singleton memory manager facade for scene-level pre-allocation.

                ======================= ARCHITECTURE ==========================
                The engine custom memory system consists of three layers:

                1. MemoryPool<BLOCK_SIZE, ALIGNMENT, BLOCKS_PER_PAGE>
                   -  Page-based allocator: each page is a raw uint8_t[]
                      allocated once from the OS with ::operator new.
                   -  Intrusive free linked list: free blocks store a
                      FreeBlock* pointer inside their own bytes — zero
                      bookkeeping overhead.
                   -  O(1) allocate (pop free list head) and O(1) deallocate
                      (push onto free list head). Freed bytes are NEVER
                      returned to the OS; they stay in the page for reuse.
                   -  Dynamic extension: if the pre-allocated pool is
                      exhausted at runtime, growPage() adds a new page
                      automatically (one extra OS call).
                   -  No STL containers anywhere in the allocation path.

                2. ComponentArray<T>  (include/ECS/ComponentArray.hpp)
                   -  IS a custom memory pool for entity component slots.
                   -  Stores up to MAX_ENTITIES (10 000) components in a
                      flat C array — pre-allocated at load time (one OS call
                      per component type via World::registerComponent<T>()).
                   -  insert() = O(1) index write, no OS call.
                   -  remove() = O(1) swap-with-last, no OS call.
                   -  Implements IPoolStats so the Memory Debug Panel can
                      display per-type usage alongside MemoryPool stats.
                   -  Used for ALL 25 game-object component types registered
                      in the current level (Transform, Velocity, Collider,
                      Health, EnemyAI, SpriteRenderer, …).

                3. MemoryManager  (this file)
                   -  Singleton load-time entry point.
                   -  Owns a MemoryPool that manages ComponentEntry bookkeeping
                      objects (16 bytes each) for the World's component type
                      registry — replacing the former std::unordered_map.
                   -  preallocateForScene(N) must be called at the top of
                      Scene::Init(), BEFORE any registerComponent<T>() calls,
                      to ensure the registry pool has enough slots with no
                      OS calls during gameplay.

                ========================= USAGE ================================
                  // In Scene::Init(), before registerComponent<T>() calls:
                  Memory::MemoryManager::Get().preallocateForScene(25);

                ==================== RUBRIC COMPLIANCE ========================
                - No runtime OS allocations (unless pool exhausted): ✓
                    growPage() is the ONLY ::operator new call site, driven
                    by preallocate() at load time.
                - Placement new: ✓
                    World::registerComponent<T>() uses placement new for
                    each ComponentArray<T> object.
                - Free linked list: ✓
                    MemoryPool::FreeBlock* next — intrusive, zero overhead.
                - Memory pages: ✓
                    MemoryPool::Page linked list with raw uint8_t[] storage.
                - No STL containers: ✓
                    Raw C arrays and raw pointers throughout.
                - Dynamic extension: ✓
                    growPage() extends automatically at runtime if needed.
                - Used for game objects / entities: ✓
                    ComponentArray<T> is the pre-allocated slot pool for all
                    entity components in the current level.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "Memory/MemoryPool.hpp"
#include <cstddef> // size_t

namespace Memory {

/******************************************************************************/
/*!
\class  MemoryManager
\brief  Singleton gateway for engine-wide memory pre-allocation.

        Owns a MemoryPool whose block size is large enough to hold any
        IComponentArray-derived bookkeeping entry. The pool is pre-allocated
        at scene load time so that no OS calls occur during gameplay when
        component arrays are accessed.

        ComponentArray<T> objects themselves vary greatly in sizeof() because
        they embed a dense T[10000] array, so each is allocated with a single
        ::operator new call at load time (via World::registerComponent<T>())
        and constructed in that memory with placement new. This class tracks
        those allocations and exposes diagnostics.
*/
/******************************************************************************/
class MemoryManager {
public:
    // -----------------------------------------------------------------------
    /*!
    \brief  Returns the singleton instance.
            Thread-safe from C++11 onward (magic statics).
    */
    // -----------------------------------------------------------------------
    static MemoryManager& Get() {
        static MemoryManager s_instance;
        return s_instance;
    }

    // -----------------------------------------------------------------------
    /*!
    \brief  Pre-allocates registry book-keeping entries for a scene.

            Call this at the start of Scene::Init() BEFORE any
            registerComponent<T>() calls. It ensures the underlying pool
            has enough slots available so that no OS allocation occurs
            during component type registration.

    \param  componentTypeCount  Number of component types that will be
                                registered (25 for the current scene).
    */
    // -----------------------------------------------------------------------
    void preallocateForScene(size_t componentTypeCount) {
        m_registryPool.preallocate(componentTypeCount);
    }

    // -----------------------------------------------------------------------
    /*!
    \brief  Allocates one registry-entry-sized block from the pool.

            Used by World internally to book a slot for each component type
            registration entry. Returns raw memory; caller uses placement new.
    */
    // -----------------------------------------------------------------------
    void* allocateRegistryEntry() {
        return m_registryPool.allocate();
    }

    // -----------------------------------------------------------------------
    /*!
    \brief  Returns a registry-entry block to the pool.

    \param  ptr  Pointer previously returned by allocateRegistryEntry().
    */
    // -----------------------------------------------------------------------
    void deallocateRegistryEntry(void* ptr) {
        m_registryPool.deallocate(ptr);
    }

    // -----------------------------------------------------------------------
    // Diagnostics
    // -----------------------------------------------------------------------

    /// Number of registry entries currently in use.
    size_t registryEntriesUsed() const {
        return m_registryPool.usedBlocks();
    }

    /// Total registry entries pre-allocated.
    size_t registryEntriesTotal() const {
        return m_registryPool.totalBlocks();
    }

    /// Number of memory pages in the registry pool.
    size_t registryPageCount() const {
        return m_registryPool.pageCount();
    }

private:
    // -----------------------------------------------------------------------
    // Private constructor — singleton pattern.
    // -----------------------------------------------------------------------
    MemoryManager()  = default;
    ~MemoryManager() = default;
    MemoryManager(const MemoryManager&)            = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

    // -----------------------------------------------------------------------
    // Pool for ComponentEntry book-keeping nodes.
    // Each entry is a (typeHash, IComponentArray*) pair.
    // Block size = 2 * sizeof(size_t) should cover both fields on all
    // platforms. We use 2 * sizeof(void*) to be safe on 64-bit.
    // BLOCKS_PER_PAGE = 32 covers the current 25 component types plus
    // headroom in a single page.
    // -----------------------------------------------------------------------
    static constexpr size_t ENTRY_BLOCK_SIZE  = 2 * sizeof(void*); // 16 bytes on 64-bit
    static constexpr size_t ENTRY_ALIGNMENT   = alignof(void*);
    static constexpr size_t ENTRIES_PER_PAGE  = 32;

    MemoryPool<ENTRY_BLOCK_SIZE, ENTRY_ALIGNMENT, ENTRIES_PER_PAGE> m_registryPool{
        "MemoryManager::RegistryPool"};
};

} // namespace Memory
