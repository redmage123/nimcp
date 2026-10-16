/**
 * @file nimcp_memory_guards.c
 * @brief Memory corruption detection implementation
 *
 * @author NIMCP Team
 * @date 2025-11-09
 */

#include "utils/memory/nimcp_memory_guards.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/thread/nimcp_atomic.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "utils/logging/nimcp_logging.h"

/* Forward declaration - defined in nimcp_brain_strategy.c */
extern void set_error(const char* format, ...);

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(memory_guards)

//=============================================================================
// Recursion Guard for NIMCP_THROW_TO_IMMUNE
//=============================================================================
/**
 * WHY:  nimcp_malloc_guarded() and related functions use NIMCP_THROW_TO_IMMUNE.
 *       If guards are enabled and the exception system allocates through the
 *       guarded allocator, infinite recursion occurs.
 * HOW:  _Atomic prevents compiler reordering if longjmp occurs during throw.
 *       memory_order_relaxed is sufficient for single-thread access.
 */
static _Thread_local _Atomic bool g_guards_in_throw = false;

#define GUARDS_SAFE_THROW(code, msg) \
    do { \
        if (!atomic_load_explicit(&g_guards_in_throw, memory_order_relaxed)) { \
            atomic_store_explicit(&g_guards_in_throw, true, memory_order_relaxed); \
            NIMCP_THROW_TO_IMMUNE(code, msg); \
            atomic_store_explicit(&g_guards_in_throw, false, memory_order_relaxed); \
        } \
    } while(0)

//=============================================================================
// Internal Data Structures
//=============================================================================

/**
 * @brief Allocation header (placed before user data)
 */
typedef struct allocation_header {
    uint32_t canary_start;     /**< Start canary (0xDEADBEEF) */
    size_t size;               /**< User-requested size */
    const char* file;          /**< Source file */
    int line;                  /**< Source line */
    uint64_t alloc_id;         /**< Unique allocation ID */
    nimcp_atomic_bool_t is_freed; /**< Atomic freed flag for double-free detection */
    struct allocation_header* next; /**< Next in tracking list */
} allocation_header_t;

/**
 * @brief Allocation footer (placed after user data)
 */
typedef struct {
    uint32_t canary_end;       /**< End canary (0xCAFEBABE) */
} allocation_footer_t;

// Global state
static memory_guard_config_t g_config;
static memory_guard_stats_t g_stats = {0};
static allocation_header_t* g_allocation_list = NULL;
static nimcp_mutex_t g_guard_mutex = NIMCP_MUTEX_INITIALIZER;
static bool g_initialized = false;
static uint64_t g_next_alloc_id = 1;

//=============================================================================
// Helper Functions
//=============================================================================

static void lock_guards(void) {
    nimcp_mutex_lock(&g_guard_mutex);
}

static void unlock_guards(void) {
    nimcp_mutex_unlock(&g_guard_mutex);
}

static allocation_header_t* get_header(void* ptr) {
    if (!ptr) {

        GUARDS_SAFE_THROW(NIMCP_ERROR_NULL_POINTER, "ptr is NULL");

        return NULL;

    }
    return (allocation_header_t*)((char*)ptr - sizeof(allocation_header_t));
}

static allocation_footer_t* get_footer(allocation_header_t* header) {
    if (!header) {

        GUARDS_SAFE_THROW(NIMCP_ERROR_NULL_POINTER, "header is NULL");

        return NULL;

    }
    return (allocation_footer_t*)((char*)(header + 1) + header->size);
}

static void* get_user_ptr(allocation_header_t* header) {
    if (!header) {

        GUARDS_SAFE_THROW(NIMCP_ERROR_NULL_POINTER, "header is NULL");

        return NULL;

    }
    return (void*)(header + 1);
}

static bool check_canaries(allocation_header_t* header) {
    if (!header) {
        return false;
    }

    // Check start canary
    if (header->canary_start != CANARY_START) {
        fprintf(stderr, "\n*** MEMORY CORRUPTION DETECTED ***\n");
        fprintf(stderr, "Start canary corrupted at %p\n", (void*)header);
        fprintf(stderr, "Expected: 0x%08X, Got: 0x%08X\n", CANARY_START, header->canary_start);
        fprintf(stderr, "Allocation: %s:%d (%zu bytes)\n",
                header->file ? header->file : "unknown",
                header->line, header->size);
        g_stats.corruption_detected++;
        return false;
    }

    // Check end canary
    allocation_footer_t* footer = get_footer(header);
    if (footer->canary_end != CANARY_END) {
        fprintf(stderr, "\n*** BUFFER OVERFLOW DETECTED ***\n");
        fprintf(stderr, "End canary corrupted at %p\n", (void*)footer);
        fprintf(stderr, "Expected: 0x%08X, Got: 0x%08X\n", CANARY_END, footer->canary_end);
        fprintf(stderr, "Allocation: %s:%d (%zu bytes)\n",
                header->file ? header->file : "unknown",
                header->line, header->size);
        fprintf(stderr, "Buffer overflow of %zu bytes detected!\n", header->size);
        g_stats.buffer_overflows_detected++;
        return false;
    }

    return true;
}

static void poison_memory(void* ptr, size_t size) {
    if (!ptr || !size) return;
    memset(ptr, 0xDD, size); // 0xDD = "dead memory"
}

//=============================================================================
// Public API Implementation
//=============================================================================

memory_guard_config_t memory_guards_default_config(void) {
    memory_guard_config_t config = {
        .enable_guards = true,
        .enable_leak_detection = true,
        .enable_overflow_detection = true,
        .enable_double_free_detection = true,
        .enable_use_after_free_detection = true,
        .abort_on_error = false,  // Log only by default
        .check_frequency = 0      // Manual checking only
    };
    return config;
}

bool memory_guards_init(const memory_guard_config_t* config) {
    lock_guards();

    if (g_initialized) {
        unlock_guards();
        return true; // Already initialized
    }

    if (config) {
        g_config = *config;
    } else {
        g_config = memory_guards_default_config();
    }

    memset(&g_stats, 0, sizeof(g_stats));
    g_allocation_list = NULL;
    g_next_alloc_id = 1;
    g_initialized = true;

    unlock_guards();

    if (g_config.enable_guards) {
        printf("Memory guards initialized (guards=%s, leaks=%s, overflow=%s)\n",
               g_config.enable_guards ? "ON" : "OFF",
               g_config.enable_leak_detection ? "ON" : "OFF",
               g_config.enable_overflow_detection ? "ON" : "OFF");
    }

    return true;
}

void memory_guards_shutdown(void) {
    if (!g_initialized) return;

    printf("\n=== Memory Guard Shutdown ===\n");
    memory_guards_print_stats();

    if (g_config.enable_leak_detection) {
        uint32_t leaks = memory_guards_report_leaks();
        if (leaks > 0) {
            fprintf(stderr, "\n*** %u MEMORY LEAKS DETECTED ***\n", leaks);
        }
    }

    lock_guards();
    g_initialized = false;
    unlock_guards();
}

void* nimcp_malloc_guarded(size_t size, const char* file, int line) {
    if (!g_initialized || !g_config.enable_guards) {
        return nimcp_malloc(size);
    }

    if (size == 0) {
        GUARDS_SAFE_THROW(NIMCP_ERROR_INVALID_PARAM, "nimcp_malloc_guarded: size is zero");
        return NULL;
    }

    // Allocate: header + user_data + footer
    size_t total_size = sizeof(allocation_header_t) + size + sizeof(allocation_footer_t);
    allocation_header_t* header = (allocation_header_t*)nimcp_malloc(total_size);

    if (!header) {
        fprintf(stderr, "malloc_guarded: allocation failed (%zu bytes) at %s:%d\n",
                size, file ? file : "unknown", line);
        GUARDS_SAFE_THROW(NIMCP_ERROR_NO_MEMORY, "nimcp_malloc_guarded: header is NULL");
        return NULL;
    }

    // Setup header
    header->canary_start = CANARY_START;
    header->size = size;
    header->file = file;
    header->line = line;
    nimcp_atomic_init_bool(&header->is_freed, false);

    // Setup footer
    allocation_footer_t* footer = get_footer(header);
    footer->canary_end = CANARY_END;

    // Track allocation
    lock_guards();

    header->alloc_id = g_next_alloc_id++;
    g_stats.total_allocations++;
    g_stats.active_allocations++;
    g_stats.total_bytes_allocated += size;
    g_stats.active_bytes += size;

    if (g_stats.active_bytes > g_stats.peak_bytes) {
        g_stats.peak_bytes = g_stats.active_bytes;
    }
    if (g_stats.active_allocations > g_stats.peak_allocations) {
        g_stats.peak_allocations = g_stats.active_allocations;
    }

    // Add to tracking list
    if (g_config.enable_leak_detection) {
        header->next = g_allocation_list;
        g_allocation_list = header;
    }

    unlock_guards();

    return get_user_ptr(header);
}

void* nimcp_calloc_guarded(size_t nmemb, size_t size, const char* file, int line) {
    if (size != 0 && nmemb > SIZE_MAX / size) {
        set_error("nimcp_calloc_guarded: integer overflow %zu * %zu", nmemb, size);
        return NULL;
    }
    size_t total = nmemb * size;
    void* ptr = nimcp_malloc_guarded(total, file, line);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void* nimcp_realloc_guarded(void* ptr, size_t size, const char* file, int line) {
    if (!ptr) {
        return nimcp_malloc_guarded(size, file, line);
    }

    if (size == 0) {
        nimcp_free_guarded(ptr, file, line);
        GUARDS_SAFE_THROW(NIMCP_ERROR_INVALID_PARAM, "nimcp_realloc_guarded: size is zero");
        return NULL;
    }

    if (!g_initialized || !g_config.enable_guards) {
        return nimcp_realloc(ptr, size);
    }

    // Allocate new block
    void* new_ptr = nimcp_malloc_guarded(size, file, line);
    if (!new_ptr) {

        GUARDS_SAFE_THROW(NIMCP_ERROR_NULL_POINTER, "new_ptr is NULL");

        return NULL;

    }

    // Copy old data
    allocation_header_t* old_header = get_header(ptr);
    if (old_header && check_canaries(old_header)) {
        size_t copy_size = (old_header->size < size) ? old_header->size : size;
        memcpy(new_ptr, ptr, copy_size);
    }

    // Free old block
    nimcp_free_guarded(ptr, file, line);

    return new_ptr;
}

void nimcp_free_guarded(void* ptr, const char* file, int line) {
    if (!ptr) return;

    if (!g_initialized || !g_config.enable_guards) {
        nimcp_free(ptr);
        return;
    }

    allocation_header_t* header = get_header(ptr);

    /* Check if already freed (double-free) using atomic CAS
     * WHY: Prevent race conditions where two threads free same pointer
     * HOW: Atomic compare-and-swap ensures only one thread can transition false→true
     */
    if (g_config.enable_double_free_detection) {
        bool expected = false;
        bool was_freed = !nimcp_atomic_compare_exchange_bool(&header->is_freed, &expected, true,
                                                               NIMCP_MEMORY_ORDER_ACQ_REL);
        if (was_freed) {
            fprintf(stderr, "\n*** DOUBLE-FREE DETECTED ***\n");
            fprintf(stderr, "Attempted to free %p at %s:%d\n", ptr,
                    file ? file : "unknown", line);
            fprintf(stderr, "Originally allocated at %s:%d\n",
                    header->file ? header->file : "unknown", header->line);
            g_stats.double_frees_detected++;

            if (g_config.abort_on_error) {
                abort();
            }
            return; // Don't actually free again
        }
    }

    // Check canaries
    if (g_config.enable_overflow_detection) {
        bool valid = check_canaries(header);
        if (!valid && g_config.abort_on_error) {
            abort();
        }
    }

    // Update stats
    lock_guards();

    g_stats.total_frees++;
    g_stats.active_allocations--;
    g_stats.active_bytes -= header->size;

    // Remove from tracking list
    if (g_config.enable_leak_detection) {
        allocation_header_t** current = &g_allocation_list;
        while (*current) {
            if (*current == header) {
                *current = header->next;
                break;
            }
            current = &(*current)->next;
        }
    }

    unlock_guards();

    /* Mark as freed (if not already done by double-free check above)
     * NOTE: If double_free_detection is disabled, we still need to mark it
     */
    if (!g_config.enable_double_free_detection) {
        nimcp_atomic_store_bool(&header->is_freed, true, NIMCP_MEMORY_ORDER_RELEASE);
    }

    // Poison memory for use-after-free detection
    if (g_config.enable_use_after_free_detection) {
        poison_memory(ptr, header->size);
    }

    // Actually free
    nimcp_free(header);
}

bool memory_guards_check_ptr(void* ptr) {
    if (!ptr || !g_initialized || !g_config.enable_guards) {
        return true; // Can't check, assume OK
    }

    allocation_header_t* header = get_header(ptr);
    return check_canaries(header);
}

uint32_t memory_guards_check_all(void) {
    if (!g_initialized || !g_config.enable_guards || !g_config.enable_leak_detection) {
        return 0;
    }

    lock_guards();

    uint32_t corruptions = 0;
    allocation_header_t* current = g_allocation_list;

    while (current) {
        if (!check_canaries(current)) {
            corruptions++;
        }
        current = current->next;
    }

    unlock_guards();

    return corruptions;
}

memory_guard_stats_t memory_guards_get_stats(void) {
    return g_stats;
}

void memory_guards_print_stats(void) {
    printf("\n=== Memory Guard Statistics ===\n");
    printf("Total allocations:    %lu\n", (unsigned long)g_stats.total_allocations);
    printf("Total frees:          %lu\n", (unsigned long)g_stats.total_frees);
    printf("Active allocations:   %lu\n", (unsigned long)g_stats.active_allocations);
    printf("Active bytes:         %lu\n", (unsigned long)g_stats.active_bytes);
    printf("Peak bytes:           %lu\n", (unsigned long)g_stats.peak_bytes);
    printf("Peak allocations:     %lu\n", (unsigned long)g_stats.peak_allocations);
    printf("\nErrors Detected:\n");
    printf("  Buffer overflows:   %lu\n", (unsigned long)g_stats.buffer_overflows_detected);
    printf("  Double frees:       %lu\n", (unsigned long)g_stats.double_frees_detected);
    printf("  Use after frees:    %lu\n", (unsigned long)g_stats.use_after_frees_detected);
    printf("  Corruptions:        %lu\n", (unsigned long)g_stats.corruption_detected);
    printf("  Leaks:              %lu\n", (unsigned long)g_stats.leaks_detected);
    printf("================================\n\n");
}

uint32_t memory_guards_report_leaks(void) {
    if (!g_initialized || !g_config.enable_leak_detection) {
        return 0;
    }

    lock_guards();

    uint32_t leak_count = 0;
    allocation_header_t* current = g_allocation_list;

    if (current) {
        fprintf(stderr, "\n=== MEMORY LEAK REPORT ===\n");
    }

    while (current) {
        /* Use NIMCP atomic wrapper for consistency */
        bool is_freed = nimcp_atomic_load_bool(&current->is_freed, NIMCP_MEMORY_ORDER_ACQUIRE);
        if (!is_freed) {
            leak_count++;
            fprintf(stderr, "LEAK #%u: %zu bytes at %s:%d (alloc_id=%lu)\n",
                    leak_count, current->size,
                    current->file ? current->file : "unknown",
                    current->line,
                    (unsigned long)current->alloc_id);
        }
        current = current->next;
    }

    if (leak_count > 0) {
        fprintf(stderr, "=== TOTAL LEAKS: %u ===\n\n", leak_count);
    }

    g_stats.leaks_detected = leak_count;

    unlock_guards();

    return leak_count;
}

void memory_guards_set_enabled(bool enable) {
    lock_guards();
    g_config.enable_guards = enable;
    unlock_guards();
}

bool memory_guards_is_enabled(void) {
    return g_initialized && g_config.enable_guards;
}
