/******************************************************************************/
/*!
\file           MemoryDebugRegistry.hpp
\project        ClockWorkz Engine
\author(s)      [Syafiq]
\brief          Global registry that aggregates stats from all memory pools.

                Pools (MemoryPool<>, ComponentArray<T>) call registerPool()
                when constructed and unregisterPool() when destroyed.
                The registry never owns any pool — it only holds raw pointers
                to IPoolStats interfaces.

                The Memory Debug Panel queries this registry every frame to
                display aggregate and per-pool statistics.

                Implementation constraints:
                  - Singleton via Meyers static (thread-safe from C++11).
                  - Fixed-size pointer array — no heap allocation in the
                    registry itself.
                  - Leak detection: if a pool unregisters with usedBlocks > 0,
                    m_leakDetected is set and a console warning is printed.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "Memory/IPoolStats.hpp"
#include <cstddef>  // size_t
#include <cstdio>   // fprintf / stderr

namespace Memory {

/******************************************************************************/
/*!
\class  MemoryDebugRegistry
\brief  Singleton registry for all IPoolStats instances.

        Usage:
        \code
          // In pool constructor:
          MemoryDebugRegistry::Get().registerPool(this);

          // In pool destructor:
          MemoryDebugRegistry::Get().unregisterPool(this);

          // In ImGui panel:
          auto agg = MemoryDebugRegistry::Get().aggregate();
        \endcode
*/
/******************************************************************************/
class MemoryDebugRegistry {
public:
    /// Maximum pools that can be registered simultaneously.
    static constexpr size_t MAX_POOLS = 64;

    // -----------------------------------------------------------------------
    /*!
    \struct AggregateStats
    \brief  Sum of all registered pool statistics.
    */
    // -----------------------------------------------------------------------
    struct AggregateStats {
        size_t usedBytes{0};   ///< Sum of usedBlocks * blockSize for all pools.
        size_t totalBytes{0};  ///< Sum of totalBlocks * blockSize for all pools.
        size_t peakBytes{0};   ///< Sum of peakUsedBlocks * blockSize for all pools.
        size_t usedBlocks{0};  ///< Total blocks in use across all pools.
        size_t freeBlocks{0};  ///< Total free blocks across all pools.
    };

    // -----------------------------------------------------------------------
    /*!
    \brief  Singleton accessor. Thread-safe from C++11 (magic static).
    */
    // -----------------------------------------------------------------------
    static MemoryDebugRegistry& Get() {
        static MemoryDebugRegistry s_instance;
        return s_instance;
    }

    // -----------------------------------------------------------------------
    /*!
    \brief  Register a pool with the registry.

            Call from the pool's constructor. The registry stores a raw
            non-owning pointer — the pool manages its own lifetime.

    \param  pool  Non-null pointer to the pool's IPoolStats interface.
    */
    // -----------------------------------------------------------------------
    void registerPool(const IPoolStats* pool) {
        if (!pool || m_count >= MAX_POOLS) return;
        m_pools[m_count++] = pool;
    }

    // -----------------------------------------------------------------------
    /*!
    \brief  Unregister a pool from the registry.

            Call from the pool's destructor. If the pool still has live
            blocks (usedBlocks > 0), a leak warning is printed to stderr
            and m_leakDetected is set to true.

    \param  pool  Pointer previously passed to registerPool().
    */
    // -----------------------------------------------------------------------
    void unregisterPool(const IPoolStats* pool) {
        if (!pool) return;

        for (size_t i = 0; i < m_count; ++i) {
            if (m_pools[i] != pool) continue;

            // Leak detection: non-zero used blocks at unregister time = leak.
            if (pool->usedBlocks() > 0) {
                m_leakDetected = true;
                fprintf(stderr,
                    "[MemoryDebugRegistry] LEAK: pool '%s' unregistered "
                    "with %zu block(s) still in use!\n",
                    pool->poolName(), pool->usedBlocks());
            }

            // Remove by shifting remaining entries left (max 64 entries).
            for (size_t j = i; j < m_count - 1; ++j) {
                m_pools[j] = m_pools[j + 1];
            }
            m_pools[--m_count] = nullptr;
            return;
        }
    }

    // -----------------------------------------------------------------------
    /*!
    \brief  Compute aggregate statistics across all registered pools.
    \return Summed stats snapshot (valid until next mutation of the registry).
    */
    // -----------------------------------------------------------------------
    AggregateStats aggregate() const {
        AggregateStats agg;
        for (size_t i = 0; i < m_count; ++i) {
            const IPoolStats* p = m_pools[i];
            agg.usedBytes  += p->usedBlocks()     * p->blockSize();
            agg.totalBytes += p->totalBlocks()    * p->blockSize();
            agg.peakBytes  += p->peakUsedBlocks() * p->blockSize();
            agg.usedBlocks += p->usedBlocks();
            agg.freeBlocks += p->freeBlocks();
        }
        return agg;
    }

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------

    /// Number of pools currently registered.
    size_t poolCount() const { return m_count; }

    /// Get pool at index i (bounds-checked; returns nullptr if out of range).
    const IPoolStats* getPool(size_t i) const {
        return (i < m_count) ? m_pools[i] : nullptr;
    }

    /*!
    \brief  Returns true if any pool has triggered the leak warning.
            Set permanently once a leak is detected — does not reset.
    */
    bool leakDetected() const { return m_leakDetected; }

private:
    MemoryDebugRegistry()  = default;
    ~MemoryDebugRegistry() = default;
    MemoryDebugRegistry(const MemoryDebugRegistry&)            = delete;
    MemoryDebugRegistry& operator=(const MemoryDebugRegistry&) = delete;

    const IPoolStats* m_pools[MAX_POOLS]{};  ///< Non-owning pointers.
    size_t            m_count{0};
    bool              m_leakDetected{false};
};

} // namespace Memory
