/******************************************************************************/
/*!
\file           IPoolStats.hpp
\project        ClockWorkz Engine
\author(s)      [Syafiq]
\brief          Abstract interface for memory pool statistics.

                Both MemoryPool<> and ComponentArray<T> implement this
                interface so they can be registered with MemoryDebugRegistry
                and displayed in the Memory Debug Panel without either class
                needing to know about the other.

                Keep this file dependency-free: only standard library headers
                so it can be included anywhere without pulling in heavy engine
                headers.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include <cstddef> // size_t

namespace Memory {
class IPoolStats {
  public:
      virtual ~IPoolStats() = default;

      // Human-readable name for the pool
      virtual const char* poolName()       const = 0;

      /// Byte size of one block / slot in this pool.
      virtual size_t      blockSize()      const = 0;

      // Number of blocks currently booked
      virtual size_t      usedBlocks()     const = 0;

      // Number of blocks currently free
      virtual size_t      freeBlocks()     const = 0;

      // Total blocks across all pages
      virtual size_t      totalBlocks()    const = 0;

      // Highest value usedBlocks() has ever reached.
      virtual size_t      peakUsedBlocks() const = 0;
};

} // namespace Memory
