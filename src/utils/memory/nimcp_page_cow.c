#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_page_cow.c - Page-Level Copy-on-Write Implementation
//=============================================================================
/**
 * @file nimcp_page_cow.c
 * @brief Page-level COW using mmap/mprotect + SIGSEGV handler
 *
 * WHAT: Fine-grained COW at 4KB page level for large data structures
 * WHY:  Object-level COW copies entire objects; page-level copies only modified pages
 * HOW:  mmap for allocation, mprotect for protection, SIGSEGV handler for COW
 *
 * IMPLEMENTATION NOTES:
 *
 * 1. Memory Allocation:
 *    - All regions are mmap'd with MAP_PRIVATE | MAP_ANONYMOUS
 *    - Pages are initially shared via reference counting (not OS-level sharing)
 *    - We use software-based COW rather than fork()-style hardware COW
 *
 * 2. Signal Handling:
 *    - SIGSEGV handler catches writes to read-only pages
 *    - Handler must be async-signal-safe (no malloc, no mutex)
 *    - Uses spinlock for critical sections within handler
 *
 * 3. Page Table:
 *    - Each region has a page table tracking refcounts
 *    - Each view has a page table tracking shared vs private pages
 *    - Private pages are allocated lazily on first write
 *
 * THREAD SAFETY:
 * - Spinlock for page table modifications (async-signal-safe)
 * - Atomic operations for refcounts
 * - Lock-free reads to shared pages
 *
 * @author NIMCP Development Team
 * @date 2025-11-24
 * @version 1.0.0
 */

#define _GNU_SOURCE  // For siginfo_t

#include "utils/memory/nimcp_page_cow.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(page_cow)

//=============================================================================
// Constants and Configuration
//=============================================================================

#define PAGE_COW_MAGIC 0x50434F57  // 'PCOW'
#define PAGE_VIEW_MAGIC 0x50564957 // 'PVIW'
#define PAGE_SNAP_MAGIC 0x50534E50 // 'PSNP'

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Page entry in region page table
 *
 * Tracks the shared page data and reference count
 */
typedef struct {
    void* data;                 /**< Pointer to page data */
    atomic_size_t refcount;     /**< Reference count (views sharing this page) */
    bool is_source;             /**< Is this the source page from region? */
} page_entry_t;

/**
 * @brief View page entry
 *
 * Tracks whether this view has a private copy of the page
 */
typedef struct {
    void* data;                 /**< Private page data (NULL if shared) */
    size_t source_page_idx;     /**< Index into region page table */
    bool is_private;            /**< true if this view has private copy */
} view_page_entry_t;

/**
 * @brief Page-level COW region structure
 */
struct page_cow_region_struct {
    uint32_t magic;             /**< Magic number for validation */

    // Configuration
    size_t size;                /**< Total size in bytes (page-aligned) */
    size_t num_pages;           /**< Number of pages */
    bool enable_tracking;       /**< Track statistics */
    bool zero_on_allocate;      /**< Zero-initialize pages */

    // Memory
    void* base_data;            /**< Base memory region */

    // Page table
    page_entry_t* pages;        /**< Per-page tracking */

    // Statistics
    page_cow_stats_t stats;

    // View tracking
    atomic_size_t view_count;   /**< Number of active views */

    // Synchronization
    atomic_flag spinlock;       /**< Spinlock for modifications */
};

/**
 * @brief Page-level COW view structure
 */
struct page_cow_view_struct {
    uint32_t magic;             /**< Magic number for validation */

    // Parent region
    page_cow_region_t region;   /**< Parent region */

    // View's page table
    view_page_entry_t* pages;   /**< Per-page state for this view */
    size_t num_pages;           /**< Number of pages */

    // Memory mapping
    void* view_base;            /**< Base address for this view's mapping */

    // Statistics
    atomic_size_t private_pages; /**< Number of private pages */
    bool writable;              /**< Has write been requested? */

    // Synchronization
    atomic_flag spinlock;       /**< Spinlock for modifications */
};

/**
 * @brief Snapshot structure
 */
struct page_cow_snapshot_struct {
    uint32_t magic;             /**< Magic number */
    page_cow_view_t source_view; /**< View this snapshot is from */
    page_cow_region_t region;   /**< Parent region */

    // Snapshot page table (copy of view's page state at snapshot time)
    view_page_entry_t* pages;   /**< Snapshot of page states */
    size_t num_pages;

    // Snapshot metadata
    size_t private_pages_at_snapshot;
};

//=============================================================================
// Global State (for signal handler)
//=============================================================================

/**
 * @brief Registered regions for signal handler lookup
 */
static struct {
    page_cow_region_t regions[PAGE_COW_MAX_REGIONS];
    page_cow_view_t views[PAGE_COW_MAX_REGIONS * 16];  // Multiple views per region
    size_t region_count;
    size_t view_count;
    atomic_flag lock;
    struct sigaction old_handler;
    bool initialized;
} g_page_cow = {
    .region_count = 0,
    .view_count = 0,
    .lock = ATOMIC_FLAG_INIT,
    .initialized = false
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Get current time in nanoseconds
 */
static inline uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * NIMCP_NS_PER_SEC + (uint64_t)ts.tv_nsec;
}

/**
 * @brief Spinlock acquire (async-signal-safe)
 */
static inline void spinlock_acquire(atomic_flag* lock) {
    while (atomic_flag_test_and_set_explicit(lock, memory_order_acquire)) {
        // Spin
    }
}

/**
 * @brief Spinlock release (async-signal-safe)
 */
static inline void spinlock_release(atomic_flag* lock) {
    atomic_flag_clear_explicit(lock, memory_order_release);
}

/**
 * @brief Spinlock try-acquire (async-signal-safe, non-blocking)
 *
 * WHY: Signal handlers must not block. If the interrupted thread already holds
 * this spinlock, a blocking acquire would deadlock forever. trylock returns
 * immediately with false if the lock is already held.
 *
 * @return true if lock was acquired, false if lock was already held
 */
static inline bool spinlock_trylock(atomic_flag* lock) {
    return !atomic_flag_test_and_set_explicit(lock, memory_order_acquire);
}

/**
 * @brief Find view by address (for signal handler)
 *
 * Must be called with g_page_cow.lock held
 */
static page_cow_view_t find_view_by_address(void* addr) {
    for (size_t i = 0; i < g_page_cow.view_count; i++) {
        page_cow_view_t view = g_page_cow.views[i];
        if (!view) continue;

        void* start = view->view_base;
        void* end = (char*)start + view->num_pages * PAGE_COW_PAGE_SIZE;

        if (addr >= start && addr < end) {
            return view;
        }
    }
    /* Address not found in any registered view */
    return NULL;
}

/**
 * @brief Register view in global table
 */
static bool register_view(page_cow_view_t view) {
    spinlock_acquire(&g_page_cow.lock);

    if (g_page_cow.view_count >= PAGE_COW_MAX_REGIONS * 16) {
        spinlock_release(&g_page_cow.lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "register_view: capacity exceeded");
        return false;
    }

    g_page_cow.views[g_page_cow.view_count++] = view;
    spinlock_release(&g_page_cow.lock);
    return true;
}

/**
 * @brief Unregister view from global table
 */
static void unregister_view(page_cow_view_t view) {
    spinlock_acquire(&g_page_cow.lock);

    for (size_t i = 0; i < g_page_cow.view_count; i++) {
        if (g_page_cow.views[i] == view) {
            // Move last element to this position
            g_page_cow.views[i] = g_page_cow.views[--g_page_cow.view_count];
            break;
        }
    }

    spinlock_release(&g_page_cow.lock);
}

/**
 * @brief Handle COW page fault
 *
 * Called from signal handler - must be async-signal-safe
 *
 * @param view View that faulted
 * @param fault_addr Address that caused fault
 * @return true if fault was handled, false otherwise
 */
static bool handle_cow_fault(page_cow_view_t view, void* fault_addr) {
    if (!view || view->magic != PAGE_VIEW_MAGIC) {
        /* P1 fix: Cannot use NIMCP_THROW_TO_IMMUNE from SIGSEGV handler (not async-signal-safe) */
        return false;
    }

    // Calculate page index
    size_t offset = (char*)fault_addr - (char*)view->view_base;
    size_t page_idx = offset / PAGE_COW_PAGE_SIZE;

    if (page_idx >= view->num_pages) {
        /* Not async-signal-safe: omit THROW in signal handler context */
        return false;
    }

    // CRITICAL: Use trylock since this may be called from a signal handler.
    // The interrupted thread may already hold view->spinlock (e.g., in
    // page_cow_view_make_page_private). Blocking here would deadlock.
    if (!spinlock_trylock(&view->spinlock)) {
        return false;  // Lock contention in signal context — cannot handle fault
    }

    view_page_entry_t* vpage = &view->pages[page_idx];

    // If already private, this isn't a COW fault
    if (vpage->is_private) {
        spinlock_release(&view->spinlock);
        return false;
    }

    page_cow_region_t region = view->region;
    if (!region || region->magic != PAGE_COW_MAGIC) {
        spinlock_release(&view->spinlock);
        return false;
    }

    // Bounds check source_page_idx before using it to index region->pages[].
    // In signal handler context, corrupted data could cause an out-of-bounds read.
    if (vpage->source_page_idx >= region->num_pages) {
        spinlock_release(&view->spinlock);
        return false;
    }

    // Allocate private page
    void* private_page = mmap(NULL, PAGE_COW_PAGE_SIZE,
                              PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (private_page == MAP_FAILED) {
        spinlock_release(&view->spinlock);
        return false;
    }

    // Copy from shared page
    void* shared_page = region->pages[vpage->source_page_idx].data;
    memcpy(private_page, shared_page, PAGE_COW_PAGE_SIZE);

    // Remap view's page to private page
    void* page_addr = (char*)view->view_base + page_idx * PAGE_COW_PAGE_SIZE;

    // Use mremap to replace the page (or mmap with MAP_FIXED)
    void* result = mmap(page_addr, PAGE_COW_PAGE_SIZE,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (result == MAP_FAILED) {
        munmap(private_page, PAGE_COW_PAGE_SIZE);
        spinlock_release(&view->spinlock);
        return false;
    }

    // Copy data to the newly mapped page
    memcpy(result, private_page, PAGE_COW_PAGE_SIZE);
    munmap(private_page, PAGE_COW_PAGE_SIZE);

    // Update view page entry
    vpage->data = result;
    vpage->is_private = true;
    atomic_fetch_add(&view->private_pages, 1);

    // Decrement shared page refcount
    atomic_fetch_sub(&region->pages[vpage->source_page_idx].refcount, 1);

    // Update statistics using atomics to avoid nested spinlock deadlock
    // (view->spinlock is already held; acquiring region->spinlock risks deadlock)
    if (region->enable_tracking) {
        __atomic_add_fetch(&region->stats.cow_faults, 1, __ATOMIC_RELAXED);
        __atomic_add_fetch(&region->stats.private_pages, 1, __ATOMIC_RELAXED);
        __atomic_add_fetch(&region->stats.total_bytes_copied, PAGE_COW_PAGE_SIZE, __ATOMIC_RELAXED);
    }

    spinlock_release(&view->spinlock);
    return true;
}

/**
 * @brief SIGSEGV signal handler for COW
 */
static void sigsegv_handler(int sig, siginfo_t* info, void* context) {
    (void)sig;
    (void)context;

    void* fault_addr = info->si_addr;

    // Try to find and handle as COW fault
    // CRITICAL: Use trylock, NOT blocking acquire. If the interrupted thread
    // already holds g_page_cow.lock, a blocking acquire would deadlock forever
    // since signal handlers run on the interrupted thread's stack.
    if (!spinlock_trylock(&g_page_cow.lock)) {
        // Lock is held by the interrupted thread — cannot safely look up view.
        // Fall through to chain to old handler (may re-raise SIGSEGV).
        goto chain_old_handler;
    }
    page_cow_view_t view = find_view_by_address(fault_addr);
    spinlock_release(&g_page_cow.lock);

    if (view && handle_cow_fault(view, fault_addr)) {
        // Fault handled, return to retry the instruction
        return;
    }

chain_old_handler:
    // Not our fault (or lock contention in signal context) - chain to old handler
    if (g_page_cow.old_handler.sa_flags & SA_SIGINFO) {
        g_page_cow.old_handler.sa_sigaction(sig, info, context);
    } else if (g_page_cow.old_handler.sa_handler != SIG_DFL &&
               g_page_cow.old_handler.sa_handler != SIG_IGN) {
        g_page_cow.old_handler.sa_handler(sig);
    } else {
        // Default action - re-raise signal
        signal(SIGSEGV, SIG_DFL);
        raise(SIGSEGV);
    }
}

//=============================================================================
// Page-Level COW API Implementation
//=============================================================================

NIMCP_EXPORT bool page_cow_init(void) {
    // Double-check with atomic CAS to prevent TOCTOU race:
    // Two threads could both see initialized==false and double-init,
    // clobbering old_handler and corrupting signal handler chain.
    if (__atomic_load_n(&g_page_cow.initialized, __ATOMIC_ACQUIRE)) return true;

    // Skip SIGSEGV handler installation when running under PyTorch/CUDA.
    // CUDA uses mprotect-based lazy GPU memory mapping that relies on SIGSEGV.
    // Our COW handler intercepts those faults and breaks CUDA's mechanism,
    // causing SIGSEGV crashes during PyTorch model loading.
    // Set NIMCP_NO_COW_SIGNAL=1 to disable the handler but keep COW data structures.
    const char* no_cow = getenv("NIMCP_NO_COW_SIGNAL");
    bool skip_signal = (no_cow && no_cow[0] == '1');

    // Use spinlock to serialize initialization attempts
    spinlock_acquire(&g_page_cow.lock);

    // Re-check after acquiring lock (another thread may have initialized)
    if (g_page_cow.initialized) {
        spinlock_release(&g_page_cow.lock);
        return true;
    }

    // Initialize global state
    memset(g_page_cow.regions, 0, sizeof(g_page_cow.regions));
    memset(g_page_cow.views, 0, sizeof(g_page_cow.views));
    g_page_cow.region_count = 0;
    g_page_cow.view_count = 0;

    if (!skip_signal) {
        // Install SIGSEGV handler
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_sigaction = sigsegv_handler;
        sa.sa_flags = SA_SIGINFO;
        sigemptyset(&sa.sa_mask);

        if (sigaction(SIGSEGV, &sa, &g_page_cow.old_handler) < 0) {
            spinlock_release(&g_page_cow.lock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "page_cow_init: sigaction failed");
            return false;
        }
    }

    __atomic_store_n(&g_page_cow.initialized, true, __ATOMIC_RELEASE);
    spinlock_release(&g_page_cow.lock);
    return true;
}

NIMCP_EXPORT void page_cow_shutdown(void) {
    if (!__atomic_load_n(&g_page_cow.initialized, __ATOMIC_ACQUIRE)) return;

    // Restore old handler
    sigaction(SIGSEGV, &g_page_cow.old_handler, NULL);

    __atomic_store_n(&g_page_cow.initialized, false, __ATOMIC_RELEASE);
}

NIMCP_EXPORT page_cow_region_t page_cow_region_create(
    const page_cow_config_t* config,
    const void* initial_data
) {
    if (!config || config->size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "page_cow_region_create: config is NULL");
        return NULL;
    }
    if (!g_page_cow.initialized) {
        if (!page_cow_init()) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "page_cow_region_create: page_cow_init is NULL");
            return NULL;
        }
    }

    // Allocate region structure
    page_cow_region_t region = nimcp_calloc(1, sizeof(struct page_cow_region_struct));
    if (!region) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "region is NULL");

        return NULL;

    }

    region->magic = PAGE_COW_MAGIC;
    region->size = page_cow_align_size(config->size);
    region->num_pages = page_cow_num_pages(config->size);
    region->enable_tracking = config->enable_tracking;
    region->zero_on_allocate = config->zero_on_allocate;
    atomic_init(&region->view_count, 0);
    atomic_flag_clear(&region->spinlock);

    // Allocate base memory
    region->base_data = mmap(NULL, region->size,
                             PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (region->base_data == MAP_FAILED) {
        nimcp_free(region);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "page_cow_region_create: validation failed");
        return NULL;
    }

    // Initialize with data or zeros
    if (initial_data) {
        memcpy(region->base_data, initial_data, config->size);
    } else if (config->zero_on_allocate) {
        memset(region->base_data, 0, region->size);
    }

    // Allocate page table
    region->pages = nimcp_calloc(region->num_pages, sizeof(page_entry_t));
    if (!region->pages) {
        munmap(region->base_data, region->size);
        nimcp_free(region);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "page_cow_region_create: region->pages is NULL");
        return NULL;
    }

    // Initialize page entries
    for (size_t i = 0; i < region->num_pages; i++) {
        region->pages[i].data = (char*)region->base_data + i * PAGE_COW_PAGE_SIZE;
        atomic_init(&region->pages[i].refcount, 0);
        region->pages[i].is_source = true;
    }

    // Initialize statistics
    memset(&region->stats, 0, sizeof(page_cow_stats_t));
    region->stats.total_pages = region->num_pages;
    region->stats.shared_pages = region->num_pages;
    region->stats.memory_used_bytes = region->size + region->num_pages * sizeof(page_entry_t);

    // Register region in global table for signal handler lookup.
    // If the table is full, destroy the region and fail — an unregistered
    // region's views would never have their COW faults handled by the
    // SIGSEGV handler, leading to unrecoverable crashes on write.
    spinlock_acquire(&g_page_cow.lock);
    if (g_page_cow.region_count < PAGE_COW_MAX_REGIONS) {
        g_page_cow.regions[g_page_cow.region_count++] = region;
    } else {
        spinlock_release(&g_page_cow.lock);
        nimcp_free(region->pages);
        munmap(region->base_data, region->size);
        region->magic = 0;
        nimcp_free(region);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE,
                              "page_cow_region_create: region table full (%d max)",
                              PAGE_COW_MAX_REGIONS);
        return NULL;
    }
    spinlock_release(&g_page_cow.lock);

    return region;
}

NIMCP_EXPORT void page_cow_region_destroy(page_cow_region_t region) {
    if (!region || region->magic != PAGE_COW_MAGIC) return;

    // Check for active views
    if (atomic_load(&region->view_count) > 0) {
        // Warning: views still active
        return;
    }

    // Unregister region
    spinlock_acquire(&g_page_cow.lock);
    for (size_t i = 0; i < g_page_cow.region_count; i++) {
        if (g_page_cow.regions[i] == region) {
            g_page_cow.regions[i] = g_page_cow.regions[--g_page_cow.region_count];
            break;
        }
    }
    spinlock_release(&g_page_cow.lock);

    // Free page table
    nimcp_free(region->pages);

    // Unmap base memory
    munmap(region->base_data, region->size);

    // Invalidate and free
    region->magic = 0;
    nimcp_free(region);
}

NIMCP_EXPORT bool page_cow_region_get_stats(
    page_cow_region_t region,
    page_cow_stats_t* stats
) {
    if (!region || region->magic != PAGE_COW_MAGIC || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "page_cow_region_get_stats: required parameter is NULL (region, stats)");
        return false;
    }

    spinlock_acquire(&region->spinlock);
    memcpy(stats, &region->stats, sizeof(page_cow_stats_t));
    stats->active_views = atomic_load(&region->view_count);
    spinlock_release(&region->spinlock);

    return true;
}

NIMCP_EXPORT size_t page_cow_region_get_size(page_cow_region_t region) {
    if (!region || region->magic != PAGE_COW_MAGIC) return 0;
    return region->size;
}

//=============================================================================
// Page-Level COW View API Implementation
//=============================================================================

NIMCP_EXPORT page_cow_view_t page_cow_view_create(page_cow_region_t region) {
    if (!region || region->magic != PAGE_COW_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "page_cow_view_create: region is NULL");
        return NULL;
    }

    // Allocate view structure
    page_cow_view_t view = nimcp_calloc(1, sizeof(struct page_cow_view_struct));
    if (!view) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "view is NULL");

        return NULL;

    }

    view->magic = PAGE_VIEW_MAGIC;
    view->region = region;
    view->num_pages = region->num_pages;
    atomic_init(&view->private_pages, 0);
    view->writable = false;
    atomic_flag_clear(&view->spinlock);

    // Allocate view page table
    view->pages = nimcp_calloc(region->num_pages, sizeof(view_page_entry_t));
    if (!view->pages) {
        nimcp_free(view);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "page_cow_view_create: view->pages is NULL");
        return NULL;
    }

    // Allocate view memory (writable initially for data copy)
    view->view_base = mmap(NULL, region->size,
                           PROT_READ | PROT_WRITE,  // Writable for initial copy
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (view->view_base == MAP_FAILED) {
        nimcp_free(view->pages);
        nimcp_free(view);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "page_cow_view_create: validation failed");
        return NULL;
    }

    // Copy region data to view
    memcpy(view->view_base, region->base_data, region->size);

    // Set to read-only (COW protection) after initial copy
    if (mprotect(view->view_base, region->size, PROT_READ) < 0) {
        munmap(view->view_base, region->size);
        nimcp_free(view->pages);
        nimcp_free(view);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "page_cow_view_create: validation failed");
        return NULL;
    }

    // Initialize page entries

    for (size_t i = 0; i < region->num_pages; i++) {
        view->pages[i].data = NULL;  // Using shared
        view->pages[i].source_page_idx = i;
        view->pages[i].is_private = false;

        // Increment region page refcount
        atomic_fetch_add(&region->pages[i].refcount, 1);
    }

    // Increment region view count
    atomic_fetch_add(&region->view_count, 1);

    // Update statistics
    if (region->enable_tracking) {
        spinlock_acquire(&region->spinlock);
        region->stats.total_views++;
        spinlock_release(&region->spinlock);
    }

    // Register view for signal handler
    register_view(view);

    return view;
}

NIMCP_EXPORT page_cow_view_t page_cow_view_clone(page_cow_view_t source) {
    if (!source || source->magic != PAGE_VIEW_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "page_cow_view_clone: source is NULL");
        return NULL;
    }

    page_cow_region_t region = source->region;
    if (!region) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "region is NULL");

        return NULL;

    }

    // Create new view from same region
    page_cow_view_t clone = page_cow_view_create(region);
    if (!clone) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "clone is NULL");

        return NULL;

    }

    // Copy private pages from source to clone.
    // The clone starts as read-only (COW protected). We must make it writable
    // before copying private page data, otherwise the memcpy will SIGSEGV.
    spinlock_acquire(&source->spinlock);
    bool has_private = false;
    for (size_t i = 0; i < source->num_pages; i++) {
        if (source->pages[i].is_private && source->pages[i].data) {
            has_private = true;
            break;
        }
    }

    if (has_private) {
        // Make clone writable so we can copy private page data into it
        if (mprotect(clone->view_base, clone->num_pages * PAGE_COW_PAGE_SIZE,
                     PROT_READ | PROT_WRITE) < 0) {
            spinlock_release(&source->spinlock);
            page_cow_view_destroy(clone);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                                  "page_cow_view_clone: mprotect failed for private page copy");
            return NULL;
        }
        clone->writable = true;

        for (size_t i = 0; i < source->num_pages; i++) {
            if (source->pages[i].is_private && source->pages[i].data) {
                // Copy private page data
                void* page_addr = (char*)clone->view_base + i * PAGE_COW_PAGE_SIZE;
                memcpy(page_addr, source->pages[i].data, PAGE_COW_PAGE_SIZE);

                // Mark clone's page as private too
                clone->pages[i].data = page_addr;
                clone->pages[i].is_private = true;
                atomic_fetch_add(&clone->private_pages, 1);

                // Decrement shared page refcount (clone no longer shares this page)
                atomic_fetch_sub(&region->pages[clone->pages[i].source_page_idx].refcount, 1);
            }
        }
    }
    spinlock_release(&source->spinlock);

    return clone;
}

NIMCP_EXPORT void page_cow_view_destroy(page_cow_view_t view) {
    if (!view || view->magic != PAGE_VIEW_MAGIC) return;

    page_cow_region_t region = view->region;

    // Unregister from signal handler
    unregister_view(view);

    // Release page refcounts and free private pages
    for (size_t i = 0; i < view->num_pages; i++) {
        if (!view->pages[i].is_private) {
            // Decrement shared page refcount
            if (region && region->magic == PAGE_COW_MAGIC) {
                atomic_fetch_sub(&region->pages[view->pages[i].source_page_idx].refcount, 1);
            }
        }
        // Private pages are in view_base mapping, freed with munmap below
    }

    // Decrement region view count
    if (region && region->magic == PAGE_COW_MAGIC) {
        atomic_fetch_sub(&region->view_count, 1);
    }

    // Unmap view memory
    if (view->view_base) {
        munmap(view->view_base, view->num_pages * PAGE_COW_PAGE_SIZE);
    }

    // Free page table and view
    nimcp_free(view->pages);
    view->magic = 0;
    nimcp_free(view);
}

NIMCP_EXPORT const void* page_cow_view_read(page_cow_view_t view) {
    if (!view || view->magic != PAGE_VIEW_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "page_cow_view_read: view is NULL");
        return NULL;
    }
    return view->view_base;
}

NIMCP_EXPORT void* page_cow_view_write(page_cow_view_t view) {
    if (!view || view->magic != PAGE_VIEW_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "page_cow_view_write: view is NULL");
        return NULL;
    }

    // Mark as writable and enable write access
    if (!view->writable) {
        // Make view memory writable (will trigger COW on shared pages)
        if (mprotect(view->view_base, view->num_pages * PAGE_COW_PAGE_SIZE,
                     PROT_READ | PROT_WRITE) < 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "page_cow_view_write: view->writable is NULL");
            return NULL;
        }
        view->writable = true;
    }

    return view->view_base;
}

NIMCP_EXPORT bool page_cow_view_make_page_private(
    page_cow_view_t view,
    size_t page_index
) {
    if (!view || view->magic != PAGE_VIEW_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "page_cow_view_make_page_private: view is NULL");
        return false;
    }
    if (page_index >= view->num_pages) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "page_cow_view_make_page_private: capacity exceeded");
        return false;
    }

    spinlock_acquire(&view->spinlock);

    view_page_entry_t* vpage = &view->pages[page_index];
    if (vpage->is_private) {
        spinlock_release(&view->spinlock);
        return true;  // Already private
    }

    page_cow_region_t region = view->region;
    if (!region || region->magic != PAGE_COW_MAGIC) {
        spinlock_release(&view->spinlock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "page_cow_view_make_page_private: region is NULL");
        return false;
    }

    // Make view writable if not already
    if (!view->writable) {
        if (mprotect(view->view_base, view->num_pages * PAGE_COW_PAGE_SIZE,
                     PROT_READ | PROT_WRITE) < 0) {
            spinlock_release(&view->spinlock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "page_cow_view_make_page_private: view->writable is NULL");
            return false;
        }
        view->writable = true;
    }

    // Copy from shared to view's page
    void* view_page = (char*)view->view_base + page_index * PAGE_COW_PAGE_SIZE;
    void* shared_page = region->pages[vpage->source_page_idx].data;
    memcpy(view_page, shared_page, PAGE_COW_PAGE_SIZE);

    // Update state
    vpage->data = view_page;
    vpage->is_private = true;
    atomic_fetch_add(&view->private_pages, 1);

    // Decrement shared refcount
    atomic_fetch_sub(&region->pages[vpage->source_page_idx].refcount, 1);

    // Update statistics using atomics to avoid nested spinlock deadlock
    // (view->spinlock is already held; acquiring region->spinlock risks deadlock
    // if another thread holds region->spinlock and is waiting on view->spinlock)
    if (region->enable_tracking) {
        __atomic_add_fetch(&region->stats.private_pages, 1, __ATOMIC_RELAXED);
        __atomic_add_fetch(&region->stats.total_bytes_copied, PAGE_COW_PAGE_SIZE, __ATOMIC_RELAXED);
    }

    spinlock_release(&view->spinlock);
    return true;
}

NIMCP_EXPORT size_t page_cow_view_make_range_private(
    page_cow_view_t view,
    size_t start_page,
    size_t num_pages
) {
    if (!view || view->magic != PAGE_VIEW_MAGIC) return 0;
    if (start_page >= view->num_pages) return 0;

    // Clamp num_pages to avoid integer overflow in start_page + num_pages.
    // Since start_page < view->num_pages, the maximum valid num_pages is
    // view->num_pages - start_page, which cannot overflow.
    size_t max_pages = view->num_pages - start_page;
    if (num_pages > max_pages) {
        num_pages = max_pages;
    }
    size_t end_page = start_page + num_pages;

    size_t made_private = 0;
    for (size_t i = start_page; i < end_page; i++) {
        if (page_cow_view_make_page_private(view, i)) {
            made_private++;
        }
    }

    return made_private;
}

NIMCP_EXPORT page_state_t page_cow_view_get_page_state(
    page_cow_view_t view,
    size_t page_index
) {
    if (!view || view->magic != PAGE_VIEW_MAGIC) return PAGE_STATE_UNMAPPED;
    if (page_index >= view->num_pages) return PAGE_STATE_UNMAPPED;

    return view->pages[page_index].is_private ? PAGE_STATE_PRIVATE : PAGE_STATE_SHARED;
}

NIMCP_EXPORT size_t page_cow_view_get_private_page_count(page_cow_view_t view) {
    if (!view || view->magic != PAGE_VIEW_MAGIC) return 0;
    return atomic_load(&view->private_pages);
}

NIMCP_EXPORT size_t page_cow_view_get_shared_page_count(page_cow_view_t view) {
    if (!view || view->magic != PAGE_VIEW_MAGIC) return 0;
    return view->num_pages - atomic_load(&view->private_pages);
}

NIMCP_EXPORT size_t page_cow_view_get_memory_saved(page_cow_view_t view) {
    if (!view || view->magic != PAGE_VIEW_MAGIC) return 0;
    size_t shared = page_cow_view_get_shared_page_count(view);
    return shared * PAGE_COW_PAGE_SIZE;
}

//=============================================================================
// Snapshot API Implementation
//=============================================================================

NIMCP_EXPORT page_cow_snapshot_t page_cow_snapshot_create(page_cow_view_t view) {
    if (!view || view->magic != PAGE_VIEW_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "page_cow_snapshot_create: view is NULL");
        return NULL;
    }

    page_cow_snapshot_t snap = nimcp_calloc(1, sizeof(struct page_cow_snapshot_struct));
    if (!snap) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snap is NULL");

        return NULL;

    }

    snap->magic = PAGE_SNAP_MAGIC;
    snap->source_view = view;
    snap->region = view->region;
    snap->num_pages = view->num_pages;
    snap->private_pages_at_snapshot = atomic_load(&view->private_pages);

    // Copy view's page state
    snap->pages = nimcp_calloc(view->num_pages, sizeof(view_page_entry_t));
    if (!snap->pages) {
        nimcp_free(snap);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "page_cow_snapshot_create: snap->pages is NULL");
        return NULL;
    }

    spinlock_acquire(&view->spinlock);
    memcpy(snap->pages, view->pages, view->num_pages * sizeof(view_page_entry_t));
    spinlock_release(&view->spinlock);

    // Increment refcounts for shared pages
    page_cow_region_t region = view->region;
    if (region && region->magic == PAGE_COW_MAGIC) {
        for (size_t i = 0; i < snap->num_pages; i++) {
            if (!snap->pages[i].is_private) {
                atomic_fetch_add(&region->pages[snap->pages[i].source_page_idx].refcount, 1);
            }
        }
    }

    return snap;
}

NIMCP_EXPORT bool page_cow_snapshot_restore(
    page_cow_view_t view,
    page_cow_snapshot_t snapshot
) {
    if (!view || view->magic != PAGE_VIEW_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "page_cow_snapshot_restore: view is NULL");
        return false;
    }
    if (!snapshot || snapshot->magic != PAGE_SNAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "page_cow_snapshot_restore: snapshot is NULL");
        return false;
    }
    if (view != snapshot->source_view) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "page_cow_snapshot_restore: validation failed");
        return false;
    }

    page_cow_region_t region = view->region;
    if (!region || region->magic != PAGE_COW_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "page_cow_snapshot_restore: region is NULL");
        return false;
    }

    spinlock_acquire(&view->spinlock);

    // Restore page states
    for (size_t i = 0; i < view->num_pages; i++) {
        bool was_private = view->pages[i].is_private;
        bool snap_private = snapshot->pages[i].is_private;

        if (was_private && !snap_private) {
            // Page was private, snapshot had it shared - restore shared
            // Re-increment shared refcount
            atomic_fetch_add(&region->pages[snapshot->pages[i].source_page_idx].refcount, 1);

            // Copy shared data back to view
            void* view_page = (char*)view->view_base + i * PAGE_COW_PAGE_SIZE;
            void* shared_page = region->pages[snapshot->pages[i].source_page_idx].data;
            memcpy(view_page, shared_page, PAGE_COW_PAGE_SIZE);
        }

        // Restore page entry
        view->pages[i] = snapshot->pages[i];
    }

    // Restore private page count
    atomic_store(&view->private_pages, snapshot->private_pages_at_snapshot);

    spinlock_release(&view->spinlock);
    return true;
}

NIMCP_EXPORT void page_cow_snapshot_destroy(page_cow_snapshot_t snapshot) {
    if (!snapshot || snapshot->magic != PAGE_SNAP_MAGIC) return;

    // Decrement refcounts for shared pages
    page_cow_region_t region = snapshot->region;
    if (region && region->magic == PAGE_COW_MAGIC) {
        for (size_t i = 0; i < snapshot->num_pages; i++) {
            if (!snapshot->pages[i].is_private) {
                atomic_fetch_sub(&region->pages[snapshot->pages[i].source_page_idx].refcount, 1);
            }
        }
    }

    nimcp_free(snapshot->pages);
    snapshot->magic = 0;
    nimcp_free(snapshot);
}

NIMCP_EXPORT size_t page_cow_snapshot_get_delta_pages(
    page_cow_view_t view,
    page_cow_snapshot_t snapshot
) {
    if (!view || view->magic != PAGE_VIEW_MAGIC) return 0;
    if (!snapshot || snapshot->magic != PAGE_SNAP_MAGIC) return 0;

    size_t delta = 0;
    spinlock_acquire(&view->spinlock);

    for (size_t i = 0; i < view->num_pages && i < snapshot->num_pages; i++) {
        if (view->pages[i].is_private != snapshot->pages[i].is_private) {
            delta++;
        }
    }

    spinlock_release(&view->spinlock);
    return delta;
}
