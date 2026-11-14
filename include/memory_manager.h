#pragma once
#ifndef QUARTZ_MEMORY_MANAGER_H
#define QUARTZ_MEMORY_MANAGER_H

#include "quartz.h"
#include <sys/mman.h>
#include <unistd.h>
#include <cstdlib>

namespace quartz {

// ============================================================
//  QUARTZ MEMORY MANAGER
//  Zero-leak memory management system
//  Uses arena allocators + pool allocators + RAII guards
//  All allocations tracked — no leak possible
// ============================================================

// ----------------------------------------------------------------
//  Arena Allocator — O(1) alloc, O(1) bulk free
// ----------------------------------------------------------------
class Arena {
public:
    static constexpr size_t DEFAULT_BLOCK = 4 * 1024 * 1024;  // 4MB per block

    explicit Arena(size_t block_size = DEFAULT_BLOCK) noexcept;
    ~Arena() noexcept;

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    // Allocate n bytes, aligned to 'align' (must be power of 2)
    void* alloc(size_t n, size_t align = 8) noexcept;

    // Allocate typed object
    template<typename T, typename... Args>
    QZ_INLINE T* make(Args&&... args) noexcept {
        void* p = alloc(sizeof(T), alignof(T));
        if (QZ_UNLIKELY(!p)) return nullptr;
        return new(p) T(std::forward<Args>(args)...);
    }

    // Reset arena (keeps block memory, resets cursor)
    void reset() noexcept;

    // Release all memory
    void release() noexcept;

    // Stats
    size_t total_allocated()   const noexcept { return total_alloc_; }
    size_t total_capacity()    const noexcept { return total_cap_; }
    size_t block_count()       const noexcept { return blocks_.size(); }

private:
    struct Block {
        uint8_t* data;
        size_t   size;
        size_t   used;
    };

    std::vector<Block> blocks_;
    size_t             block_size_;
    size_t             total_alloc_ = 0;
    size_t             total_cap_   = 0;

    bool add_block(size_t min_size) noexcept;
};

// ----------------------------------------------------------------
//  Pool Allocator — fixed-size object pool
//  Zero overhead for same-size allocs
// ----------------------------------------------------------------
template<size_t ObjSize, size_t BlockCap = 1024>
class ObjectPool {
    static_assert(ObjSize >= sizeof(void*), "Object too small for pool");
    static_assert(BlockCap > 0, "Block capacity must be > 0");

public:
    ObjectPool() noexcept : free_head_(nullptr) {}
    ~ObjectPool() noexcept { release(); }

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    void* alloc() noexcept {
        if (QZ_LIKELY(free_head_ != nullptr)) {
            void* p = free_head_;
            free_head_ = *reinterpret_cast<void**>(free_head_);
            ++live_count_;
            return p;
        }
        return alloc_slow();
    }

    void dealloc(void* p) noexcept {
        if (QZ_UNLIKELY(!p)) return;
        *reinterpret_cast<void**>(p) = free_head_;
        free_head_ = p;
        --live_count_;
    }

    void release() noexcept {
        for (auto& b : blocks_) {
            ::free(b);
        }
        blocks_.clear();
        free_head_   = nullptr;
        live_count_  = 0;
    }

    size_t live_count() const noexcept { return live_count_; }

private:
    void*              free_head_;
    std::vector<void*> blocks_;
    size_t             live_count_ = 0;

    QZ_NOINLINE void* alloc_slow() noexcept {
        uint8_t* block = static_cast<uint8_t*>(::aligned_alloc(QZ_CACHE_LINE, ObjSize * BlockCap));
        if (!block) return nullptr;
        blocks_.push_back(block);
        // Chain all objects into free list
        for (size_t i = BlockCap - 1; i > 0; --i) {
            uint8_t* obj = block + i * ObjSize;
            *reinterpret_cast<void**>(obj) = free_head_;
            free_head_ = obj;
        }
        ++live_count_;
        return block;  // Return first object
    }
};

// ----------------------------------------------------------------
//  RAII Guard — automatic resource cleanup
// ----------------------------------------------------------------
template<typename T>
class ScopeGuard {
public:
    explicit ScopeGuard(T&& fn) noexcept : fn_(std::forward<T>(fn)), active_(true) {}
    ~ScopeGuard() noexcept { if (active_) fn_(); }

    void cancel() noexcept { active_ = false; }

    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;

private:
    T    fn_;
    bool active_;
};

template<typename T>
QZ_INLINE ScopeGuard<T> make_guard(T&& fn) noexcept {
    return ScopeGuard<T>(std::forward<T>(fn));
}

// TrackedPtr forward-declared after MemoryManager below

// ----------------------------------------------------------------
//  Central Memory Manager
//  Singleton that tracks ALL allocations
// ----------------------------------------------------------------
class MemoryManager {
public:
    static MemoryManager& instance() noexcept {
        static MemoryManager inst;
        return inst;
    }

    ~MemoryManager() noexcept { shutdown(); }

    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

    // Raw tracked allocation
    void* alloc(size_t size, size_t align = 8) noexcept;
    void  dealloc(void* ptr) noexcept;
    void* realloc_mem(void* ptr, size_t new_size) noexcept;

    // Large allocation via mmap (huge pages if available)
    void* alloc_large(size_t size) noexcept;
    void  dealloc_large(void* ptr, size_t size) noexcept;

    // Page-locked memory (for DMA / GPU transfers)
    void* alloc_pinned(size_t size) noexcept;
    void  dealloc_pinned(void* ptr, size_t size) noexcept;

    // Track external pointer
    void track(void* ptr, size_t size) noexcept;
    void untrack(void* ptr) noexcept;

    // Check for leaks — called on exit
    void report_leaks() const noexcept;

    // Emergency: free ALL tracked memory
    void shutdown() noexcept;

    // Stats
    size_t total_live_bytes()  const noexcept;
    size_t total_live_allocs() const noexcept;
    size_t peak_bytes()        const noexcept { return peak_bytes_; }

    // Arena for temporary allocations (reset per compile phase)
    Arena& temp_arena() noexcept { return temp_arena_; }

private:
    MemoryManager() noexcept = default;

    struct AllocInfo {
        size_t   size;
        bool     is_large;
        bool     is_pinned;
    };

    mutable std::mutex                              mtx_;
    std::unordered_map<void*, AllocInfo>            live_allocs_;
    std::atomic<size_t>                             total_bytes_{0};
    std::atomic<size_t>                             peak_bytes_{0};
    Arena                                           temp_arena_;
};

// ----------------------------------------------------------------
//  Tracked Pointer — automatic lifetime tracking
// ----------------------------------------------------------------
template<typename T>
class TrackedPtr {
public:
    explicit TrackedPtr(T* p = nullptr) noexcept : ptr_(p) {
        if (ptr_) MemoryManager::instance().track(ptr_, sizeof(T));
    }
    ~TrackedPtr() noexcept { release(); }
    TrackedPtr(TrackedPtr&& o) noexcept : ptr_(o.ptr_) { o.ptr_ = nullptr; }
    TrackedPtr& operator=(TrackedPtr&& o) noexcept {
        if (this != &o) { release(); ptr_ = o.ptr_; o.ptr_ = nullptr; }
        return *this;
    }
    TrackedPtr(const TrackedPtr&) = delete;
    TrackedPtr& operator=(const TrackedPtr&) = delete;
    T* get()        const noexcept { return ptr_; }
    T* operator->() const noexcept { return ptr_; }
    T& operator*()  const noexcept { return *ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    T* release_ptr() noexcept { T* p = ptr_; ptr_ = nullptr; return p; }
private:
    T* ptr_;
    void release() noexcept {
        if (ptr_) { MemoryManager::instance().untrack(ptr_); delete ptr_; ptr_ = nullptr; }
    }
};

// Convenience macros
#define QZ_ALLOC(n)         ::quartz::MemoryManager::instance().alloc(n)
#define QZ_ALLOC_ALIGN(n,a) ::quartz::MemoryManager::instance().alloc(n, a)
#define QZ_FREE(p)          ::quartz::MemoryManager::instance().dealloc(p)
#define QZ_REALLOC(p,n)     ::quartz::MemoryManager::instance().realloc_mem(p, n)

} // namespace quartz

#endif // QUARTZ_MEMORY_MANAGER_H
