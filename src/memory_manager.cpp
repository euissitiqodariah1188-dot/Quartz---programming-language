#include "../include/memory_manager.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <sys/mman.h>
#include <unistd.h>

namespace quartz {

// ============================================================
//  ARENA IMPLEMENTATION
// ============================================================
Arena::Arena(size_t block_size) noexcept : block_size_(block_size) {}

Arena::~Arena() noexcept { release(); }

void* Arena::alloc(size_t n, size_t align) noexcept {
    if (n == 0) return nullptr;
    QZ_ASSERT((align & (align - 1)) == 0, "Alignment must be power of 2");

    for (auto& b : blocks_) {
        // Align cursor
        size_t aligned = (b.used + align - 1) & ~(align - 1);
        if (aligned + n <= b.size) {
            void* p = b.data + aligned;
            b.used = aligned + n;
            total_alloc_ += n;
            return p;
        }
    }
    // Need new block
    if (!add_block(n + align)) return nullptr;
    auto& b = blocks_.back();
    size_t aligned = (b.used + align - 1) & ~(align - 1);
    void* p = b.data + aligned;
    b.used = aligned + n;
    total_alloc_ += n;
    return p;
}

bool Arena::add_block(size_t min_size) noexcept {
    size_t sz = block_size_ > min_size ? block_size_ : min_size;
    // Round up to page size
    long page = ::sysconf(_SC_PAGESIZE);
    if (page > 0) sz = (sz + static_cast<size_t>(page) - 1) & ~(static_cast<size_t>(page) - 1);

    uint8_t* data = static_cast<uint8_t*>(
        ::mmap(nullptr, sz, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (data == MAP_FAILED) return false;

    blocks_.push_back({data, sz, 0});
    total_cap_ += sz;
    return true;
}

void Arena::reset() noexcept {
    for (auto& b : blocks_) {
        b.used = 0;
        ::memset(b.data, 0, b.size);
    }
    total_alloc_ = 0;
}

void Arena::release() noexcept {
    for (auto& b : blocks_) {
        ::munmap(b.data, b.size);
    }
    blocks_.clear();
    total_alloc_ = 0;
    total_cap_   = 0;
}

// ============================================================
//  MEMORY MANAGER IMPLEMENTATION
// ============================================================
void* MemoryManager::alloc(size_t size, size_t align) noexcept {
    if (size == 0) return nullptr;
    void* p = ::aligned_alloc(align, (size + align - 1) & ~(align - 1));
    if (!p) return nullptr;
    ::memset(p, 0, size);

    std::lock_guard<std::mutex> lk(mtx_);
    live_allocs_[p] = {size, false, false};
    size_t total = total_bytes_.fetch_add(size) + size;
    if (total > peak_bytes_) peak_bytes_ = total;
    return p;
}

void MemoryManager::dealloc(void* ptr) noexcept {
    if (!ptr) return;
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = live_allocs_.find(ptr);
    if (it != live_allocs_.end()) {
        total_bytes_.fetch_sub(it->second.size);
        live_allocs_.erase(it);
    }
    ::free(ptr);
}

void* MemoryManager::realloc_mem(void* ptr, size_t new_size) noexcept {
    if (!ptr) return alloc(new_size);
    if (new_size == 0) { dealloc(ptr); return nullptr; }

    std::lock_guard<std::mutex> lk(mtx_);
    auto it = live_allocs_.find(ptr);
    size_t old_size = it != live_allocs_.end() ? it->second.size : 0;

    void* np = ::realloc(ptr, new_size);
    if (!np) return nullptr;

    if (it != live_allocs_.end()) live_allocs_.erase(it);
    live_allocs_[np] = {new_size, false, false};

    if (new_size > old_size)
        total_bytes_.fetch_add(new_size - old_size);
    else
        total_bytes_.fetch_sub(old_size - new_size);

    return np;
}

void* MemoryManager::alloc_large(size_t size) noexcept {
    if (size == 0) return nullptr;
    // Round up to huge page size (2MB) if possible
    static const size_t HUGE = 2 * 1024 * 1024;
    size_t aligned = (size + HUGE - 1) & ~(HUGE - 1);

    void* p = ::mmap(nullptr, aligned,
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) return nullptr;

    // Try huge pages
    ::madvise(p, aligned, MADV_HUGEPAGE);

    std::lock_guard<std::mutex> lk(mtx_);
    live_allocs_[p] = {aligned, true, false};
    size_t total = total_bytes_.fetch_add(aligned) + aligned;
    if (total > peak_bytes_) peak_bytes_ = total;
    return p;
}

void MemoryManager::dealloc_large(void* ptr, size_t size) noexcept {
    if (!ptr) return;
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = live_allocs_.find(ptr);
    if (it != live_allocs_.end()) {
        total_bytes_.fetch_sub(it->second.size);
        live_allocs_.erase(it);
    }
    ::munmap(ptr, size);
}

void* MemoryManager::alloc_pinned(size_t size) noexcept {
    void* p = alloc_large(size);
    if (!p) return nullptr;
    // Lock pages in RAM (prevent swapping)
    ::mlock(p, size);
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = live_allocs_.find(p);
    if (it != live_allocs_.end()) it->second.is_pinned = true;
    return p;
}

void MemoryManager::dealloc_pinned(void* ptr, size_t size) noexcept {
    if (!ptr) return;
    ::munlock(ptr, size);
    dealloc_large(ptr, size);
}

void MemoryManager::track(void* ptr, size_t size) noexcept {
    if (!ptr) return;
    std::lock_guard<std::mutex> lk(mtx_);
    live_allocs_[ptr] = {size, false, false};
    total_bytes_.fetch_add(size);
}

void MemoryManager::untrack(void* ptr) noexcept {
    if (!ptr) return;
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = live_allocs_.find(ptr);
    if (it != live_allocs_.end()) {
        total_bytes_.fetch_sub(it->second.size);
        live_allocs_.erase(it);
    }
}

void MemoryManager::report_leaks() const noexcept {
    std::lock_guard<std::mutex> lk(mtx_);
    if (live_allocs_.empty()) {
        std::fprintf(stderr, "[QUARTZ MEMORY] Clean exit — no leaks.\n");
        return;
    }
    std::fprintf(stderr,
        "[QUARTZ MEMORY] WARNING: %zu allocation(s) still live (%zu bytes)!\n",
        live_allocs_.size(),
        static_cast<size_t>(total_bytes_.load()));
    size_t shown = 0;
    for (const auto& kv : live_allocs_) {
        if (shown++ > 20) { std::fprintf(stderr, "  ... (more)\n"); break; }
        std::fprintf(stderr, "  ptr=0x%016lx size=%zu %s%s\n",
            reinterpret_cast<unsigned long>(kv.first),
            kv.second.size,
            kv.second.is_large  ? "[large]"  : "",
            kv.second.is_pinned ? "[pinned]" : "");
    }
}

void MemoryManager::shutdown() noexcept {
    // Free all remaining allocations
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& kv : live_allocs_) {
        if (kv.second.is_large || kv.second.is_pinned) {
            if (kv.second.is_pinned) ::munlock(kv.first, kv.second.size);
            ::munmap(kv.first, kv.second.size);
        } else {
            ::free(kv.first);
        }
    }
    live_allocs_.clear();
    total_bytes_ = 0;
    temp_arena_.release();
}

size_t MemoryManager::total_live_bytes() const noexcept {
    return total_bytes_.load();
}

size_t MemoryManager::total_live_allocs() const noexcept {
    std::lock_guard<std::mutex> lk(mtx_);
    return live_allocs_.size();
}

} // namespace quartz
