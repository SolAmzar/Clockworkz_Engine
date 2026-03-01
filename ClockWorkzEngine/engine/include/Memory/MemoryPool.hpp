/******************************************************************************/
/*!
\file           MemoryPool.hpp
\project        ClockWorkz Engine
\author(s)      [Syafiq]
\brief          Custom fixed-block memory pool allocator.

                Implements a page-based, free-list memory pool that satisfies
                the following requirements:
                  - Original (large) memory allocations happen at load time
                    via preallocate(). Runtime allocations draw from the
                    pre-allocated pool with NO OS calls.
                  - Allocation: O(1) via intrusive free linked list.
                  - Deallocation: O(1) push back onto free list. Bytes are
                    never returned to the OS until the pool is destroyed.
                  - Dynamic extension: if the pre-allocated pool is exhausted
                    at runtime, growPage() adds a new page automatically.
                  - Uses placement new for object construction in pool memory.
                  - No STL containers used for allocation/deallocation.

                Key structures:
                  - Page:      raw uint8_t block linked by raw pointer.
                  - FreeBlock: intrusive node overlaid on unused block bytes.

                IPoolStats integration:
                  - Implements Memory::IPoolStats for the Memory Debug Panel.
                  - Auto-registers with MemoryDebugRegistry on construction.
                  - Auto-unregisters on destruction (with leak warning if
                    usedBlocks > 0).

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "Memory/IPoolStats.hpp"
#include "Memory/MemoryDebugRegistry.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <new>

namespace Memory {

/******************************************************************************/
/*!
\class  MemoryPool
\brief  Fixed-block memory pool with page-based growth and free-list recycling.
        Template parameters:
          BLOCK_SIZE      - byte size of each managed block (should be sizeof(T) for the type you intend to store).
          ALIGNMENT       - byte alignment requirement (should be alignof(T)).
          BLOCKS_PER_PAGE - how many blocks each page holds (default 64).
*/
/******************************************************************************/
template <size_t BLOCK_SIZE,
          size_t ALIGNMENT       = alignof(std::max_align_t),
          size_t BLOCKS_PER_PAGE = 64>

class MemoryPool : public IPoolStats {
public:
    // -----------------------------------------------------------------------
    // A block must be large enough to hold either a live object OR a
    // FreeBlock* pointer (for the intrusive free list).
    // -----------------------------------------------------------------------
    static constexpr size_t EFFECTIVE_BLOCK_SIZE =
        (BLOCK_SIZE >= sizeof(void*)) ? BLOCK_SIZE : sizeof(void*);

    // -----------------------------------------------------------------------
    /*!
    \struct FreeBlock
    \brief  Intrusive linked-list node overlaid on unused block memory.

            When a block is free its first sizeof(FreeBlock*) bytes store a
            pointer to the next free block. This means no separate
            bookkeeping array is needed — the free list lives inside the
            free blocks themselves.
    */
    // -----------------------------------------------------------------------
    struct FreeBlock {
        FreeBlock* next;
    };

    // -----------------------------------------------------------------------
    /*!
    \struct Page
    \brief  A contiguous raw byte array that holds BLOCKS_PER_PAGE blocks.

            Pages are linked by a raw pointer (no STL). The pool allocates
            pages from the OS via ::operator new and links them into
            m_pageHead. Pages are never freed until the pool is destroyed.
    */
    // -----------------------------------------------------------------------
    struct alignas(ALIGNMENT) Page {

        // Raw storage for BLOCKS_PER_PAGE blocks.
        uint8_t data[EFFECTIVE_BLOCK_SIZE * BLOCKS_PER_PAGE];

        // Intrusive linked-list pointer to the next page.
        Page*   next;
    };

    // -----------------------------------------------------------------------
    // Constructor / Destructor
    // -----------------------------------------------------------------------

    /*!
    \brief  Construct an empty pool and register it with MemoryDebugRegistry.

    \param  name  Human-readable label shown in the Memory Debug Panel.
                  Must be a string literal or a pointer that outlives the pool.
    */
    explicit MemoryPool(const char* name = "MemoryPool")
        : m_freeList(nullptr)
        , m_pageHead(nullptr)
        , m_totalBlocks(0)
        , m_usedBlocks(0)
        , m_peakUsedBlocks(0)
        , m_name(name)
    {
        MemoryDebugRegistry::Get().registerPool(this);
    }

    /*!
    \brief  Destroy all pages, return raw memory to OS, and unregister from
            the debug registry.
    */
    ~MemoryPool() {
        // Unregister first so the registry can check for leaks before pages are freed
        MemoryDebugRegistry::Get().unregisterPool(this);

        Page* page = m_pageHead;
        while (page) {
            Page* next = page->next;
            page->~Page();
            ::operator delete(page);
            page = next;
        }
        m_freeList    = nullptr;
        m_pageHead    = nullptr;
        m_totalBlocks = 0;
        m_usedBlocks  = 0;
    }

    // Non-copyable, non-movable — pool owns its pages.
    MemoryPool(const MemoryPool&)            = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    MemoryPool(MemoryPool&&)                 = delete;
    MemoryPool& operator=(MemoryPool&&)      = delete;

    // -----------------------------------------------------------------------
    /*!
    \brief  Pre-allocates memory so that at least 'count' blocks are
            available without any further OS calls at runtime.

    \param  count  Number of blocks to ensure are available.
    */
    // -----------------------------------------------------------------------
    void preallocate(size_t count) {
        while (freeBlocks() < count) {
            growPage();
        }
    }

    // -----------------------------------------------------------------------
    /*!
    \brief  Allocates (books) one block from the pool.
    \return Pointer to raw memory. Caller must use placement new to
            construct an object in this memory.
    */
    // -----------------------------------------------------------------------
    void* allocate() {
        if (!m_freeList) {
            // Dynamic extension when pool is full.
            growPage(); 
        }

        FreeBlock* block = m_freeList;
        m_freeList = block->next;
        ++m_usedBlocks;

        // Update peak usage.
        if (m_usedBlocks > m_peakUsedBlocks) {
            m_peakUsedBlocks = m_usedBlocks;
        }

        return static_cast<void*>(block);
    }

    // -----------------------------------------------------------------------
    /*!
    \brief  Frees (returns) one block to the pool.

            O(1) operation: pushes the block onto the head of the free list.
            The bytes are NOT returned to the OS — they remain in the pool's
            page memory for future reuse.

    \param  ptr  Pointer previously returned by allocate(). Must not be null.

    \note   Caller is responsible for explicitly calling the object's
            destructor BEFORE calling deallocate().
    */
    // -----------------------------------------------------------------------
    void deallocate(void* ptr) {
        assert(ptr != nullptr && "MemoryPool::deallocate() called with nullptr");

        FreeBlock* block = static_cast<FreeBlock*>(ptr);
        block->next      = m_freeList;
        m_freeList       = block;
        --m_usedBlocks;
    }

    // -----------------------------------------------------------------------
    // IPoolStats interface
    // -----------------------------------------------------------------------

    const char* poolName()       const override { return m_name; }
    size_t      blockSize()      const override { return EFFECTIVE_BLOCK_SIZE; }
    size_t      usedBlocks()     const override { return m_usedBlocks; }
    size_t      freeBlocks()     const override { return m_totalBlocks - m_usedBlocks; }
    size_t      totalBlocks()    const override { return m_totalBlocks; }
    size_t      peakUsedBlocks() const override { return m_peakUsedBlocks; }

    // -----------------------------------------------------------------------
    // Additional diagnostics
    // -----------------------------------------------------------------------

    /// Number of pages currently allocated from the OS.
    size_t pageCount() const {
        size_t count = 0;
        for (Page* p = m_pageHead; p; p = p->next) { ++count; }
        return count;
    }

private:
    // -----------------------------------------------------------------------
    /*!
    \brief  Allocates a new Page from the OS and threads all its blocks
            onto the free linked list.
    */
    // -----------------------------------------------------------------------
    void growPage() {
        void* raw  = ::operator new(sizeof(Page));
        Page* page = new (raw) Page();   // placement new to construct Page

        page->next = m_pageHead;
        m_pageHead = page;

        for (size_t i = 0; i < BLOCKS_PER_PAGE; ++i) {
            uint8_t*   blockPtr = page->data + i * EFFECTIVE_BLOCK_SIZE;
            FreeBlock* fb       = reinterpret_cast<FreeBlock*>(blockPtr);
            fb->next   = m_freeList;
            m_freeList = fb;
        }

        m_totalBlocks += BLOCKS_PER_PAGE;
    }

    // -----------------------------------------------------------------------
    // Member data
    // -----------------------------------------------------------------------
    FreeBlock*  m_freeList;       // Head of the intrusive free linked list.
    Page*       m_pageHead;       // Head of the page linked list.
    size_t      m_totalBlocks;    // Total blocks across all pages.
    size_t      m_usedBlocks;     // Blocks currently in use.
    size_t      m_peakUsedBlocks; // Highest value m_usedBlocks ever reached.
    const char* m_name;           // Human-readable pool name for debug panel.
};

} // namespace Memory
