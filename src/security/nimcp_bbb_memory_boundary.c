/**
 * @file nimcp_bbb_memory_boundary.c
 * @brief Blood-Brain Barrier - Memory Boundary Monitor Implementation
 *
 * WHAT: Implements memory region protection and stack canary verification
 *       for the NIMCP Blood-Brain Barrier security layer.
 *
 * WHY:  The memory boundary monitor acts as "astrocyte end-feet" in the BBB,
 *       protecting the neural substrate from memory corruption attacks:
 *       - Buffer overflows (heap/stack)
 *       - Use-after-free vulnerabilities
 *       - Out-of-bounds memory access
 *       - Stack smashing attacks
 *
 * HOW:  Uses a combination of:
 *       1. Memory region registry with bounds tracking
 *       2. mprotect() for OS-level page protection
 *       3. Stack canaries for function return protection
 *       4. Runtime bounds checking on memory accesses
 *
 * BIOLOGICAL MODEL:
 * ```
 * Astrocyte End-Feet         ->  Memory Boundary Monitor
 * - Form physical barrier    ->  mprotect() page protection
 * - Regulate ion flow        ->  Read/write permission control
 * - Detect damage signals    ->  Stack canary verification
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe considerations
 *
 * @author NIMCP Team
 * @date 2025-11-24
 */

#include "security/nimcp_blood_brain_barrier.h"
#include "async/nimcp_bio_async.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/platform/nimcp_platform_once.h"
#include "utils/thread/nimcp_atomic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stddef.h>  /* for NULL */
#include <stdatomic.h>

#ifndef _WIN32
#include <sys/mman.h>
#endif

#define LOG_MODULE "security_bbb_memory_boundary"

//=============================================================================
// Health Agent Integration (Phase 8: System-Wide Health Integration)
//=============================================================================
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;
extern void nimcp_health_agent_heartbeat_ex(nimcp_health_agent_t* agent,
                                             const char* operation,
                                             float progress);

/** Global health agent for bbb_memory_boundary module - P2 fix: Use atomic for thread safety */
static _Atomic(nimcp_health_agent_t*) g_bbb_memory_boundary_health_agent = NULL;

/**
 * @brief Set health agent for bbb_memory_boundary heartbeats (thread-safe)
 * @param agent Health agent (can be NULL to disable)
 */
void bbb_memory_boundary_set_health_agent(nimcp_health_agent_t* agent) {
    atomic_store(&g_bbb_memory_boundary_health_agent, agent);
}

/** @brief Send heartbeat from bbb_memory_boundary module (thread-safe) */
static inline void bbb_memory_boundary_heartbeat(const char* operation, float progress) {
    /* P2 fix: Atomic load to prevent data race */
    nimcp_health_agent_t* agent = atomic_load(&g_bbb_memory_boundary_health_agent);
    if (agent) {
        nimcp_health_agent_heartbeat_ex(agent, operation, progress);
    }
}

//=============================================================================
// Constants
//=============================================================================

/**
 * WHAT: Maximum number of protected memory regions
 * WHY:  Bounded array prevents unbounded memory growth and enables O(1) lookup
 */
#define BBB_MAX_MEMORY_REGIONS 256

/**
 * WHAT: Invalid region ID sentinel value
 * WHY:  Distinguishes unregistered regions from valid ID 1+
 */
#define BBB_INVALID_REGION_ID 0

/**
 * WHAT: Stack canary magic pattern for quick validation
 * WHY:  High-entropy pattern that's unlikely to appear naturally in stack data
 */
#define BBB_CANARY_MAGIC 0xDEADBEEFCAFEBABEULL

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Protected memory region descriptor
 *
 * WHAT: Tracks a single memory region registered for protection
 * WHY:  Enables bounds checking and permission enforcement
 */
typedef struct {
    uint32_t id;              /**< Unique region identifier (1-based) */
    void* address;            /**< Start address of protected region */
    size_t size;              /**< Size in bytes of protected region */
    bool read_only;           /**< Whether region is read-only */
    bool active;              /**< Whether region is currently registered */
    uint64_t registration_ts; /**< Timestamp when registered */
} bbb_memory_region_t;

/**
 * @brief Memory boundary system state
 *
 * WHAT: Internal state for the memory boundary subsystem
 * WHY:  Encapsulates all protected regions and configuration
 */
typedef struct {
    bbb_memory_region_t regions[BBB_MAX_MEMORY_REGIONS];
    uint32_t region_count;          /**< Number of active regions */
    uint32_t next_region_id;        /**< Next ID to assign */
    uint64_t total_protected_bytes; /**< Sum of all protected region sizes */
    bool initialized;               /**< Whether subsystem is initialized */
} bbb_memory_state_t;

//=============================================================================
// Global State (per-system, accessed via bbb_system_t)
//=============================================================================

/**
 * WHAT: Global memory boundary state
 * WHY:  Singleton pattern - one instance per process
 *
 * NOTE: In production, this would be stored in bbb_system_t structure.
 *       For now, using static global for simplicity.
 *
 * THREAD-SAFETY: Protected by g_memory_state_lock mutex
 */
static bbb_memory_state_t g_memory_state = {0};

/**
 * WHAT: Mutex protecting g_memory_state
 * WHY:  Thread-safe access to global state
 * HOW:  All read-modify-write operations must hold this lock
 */
static nimcp_platform_mutex_t g_memory_state_lock;

/**
 * WHAT: Module initialization state using platform_once
 * WHY:  Thread-safe lazy initialization
 */
static nimcp_platform_once_t g_memory_boundary_init_once = NIMCP_PLATFORM_ONCE_INIT;
static bool g_memory_boundary_module_initialized = false;

/* Note: g_bbb_memory_boundary_health_agent is declared near top of file with _Atomic type */

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Find region index by ID
 *
 * WHAT: Locate a registered region by its unique ID
 * WHY:  Required for unregistration and access checking
 * HOW:  Linear scan through active regions
 *
 * COMPLEXITY: O(n) where n = number of registered regions
 *
 * @param id Region ID to find
 * @return Index in regions array, or -1 if not found
 */
static int find_region_by_id(uint32_t id)
{
    if (id == BBB_INVALID_REGION_ID) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_region_by_id: validation failed");
        return -1;
    }

    for (uint32_t i = 0; i < BBB_MAX_MEMORY_REGIONS; i++) {
        if (g_memory_state.regions[i].active &&
            g_memory_state.regions[i].id == id) {
            return (int)i;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_region_by_id: operation failed");
    return -1;
}

/**
 * @brief Find first available slot in regions array
 *
 * WHAT: Locate an unused slot for new region registration
 * WHY:  Enables O(1) registration when slots are available
 * HOW:  Linear scan for inactive slot
 *
 * COMPLEXITY: O(n) where n = max regions
 *
 * @return Index of available slot, or -1 if full
 */
static int find_available_slot(void)
{
    for (uint32_t i = 0; i < BBB_MAX_MEMORY_REGIONS; i++) {
        if (!g_memory_state.regions[i].active) {
            return (int)i;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "find_available_slot: g_memory_state is NULL");
    return -1;
}

/**
 * @brief Generate random canary value using secure random source
 *
 * WHAT: Create unpredictable 64-bit canary value
 * WHY:  Attackers cannot predict canary, so overflows are detected
 * HOW:  Read from /dev/urandom, fallback to time-based if unavailable
 *
 * SECURITY: Canary predictability compromises stack protection
 *
 * @return Random 64-bit canary value
 */
static uint64_t generate_random_canary(void)
{
    uint64_t canary = 0;

#ifndef _WIN32
    /* WHAT: Try cryptographically secure source first
     * WHY:  /dev/urandom provides high-quality randomness
     */
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ssize_t n = read(fd, &canary, sizeof(canary));
        close(fd);
        if (n == sizeof(canary)) {
            return canary ^ BBB_CANARY_MAGIC;
        }
    }
#endif

    /* WHAT: Fallback to time-based randomness
     * WHY:  Still better than constant; provides some entropy
     */
    canary = (uint64_t)time(NULL);
    canary ^= (uint64_t)(uintptr_t)&canary;  /* Stack address adds entropy */
    canary ^= BBB_CANARY_MAGIC;
    canary = (canary << 13) ^ (canary >> 7);  /* Simple mixing */

    return canary;
}

/**
 * @brief Internal initialization routine called by nimcp_platform_once
 *
 * WHAT: Thread-safe one-time initialization of module state
 * WHY:  Ensures mutex and state are initialized exactly once
 */
static void memory_boundary_module_init_internal(void)
{
    /* Initialize the global state mutex */
    nimcp_platform_mutex_init(&g_memory_state_lock, false);

    /* Health agent uses _Atomic type, no init needed (statically initialized to NULL) */

    /* Initialize the state itself */
    memset(&g_memory_state, 0, sizeof(g_memory_state));
    g_memory_state.next_region_id = 1;  /* Start IDs at 1, 0 is invalid */
    g_memory_state.initialized = true;

    g_memory_boundary_module_initialized = true;

    LOG_MODULE_INFO("bbb_memory_boundary", "Memory boundary module initialized");
}

/**
 * @brief Initialize memory boundary subsystem (thread-safe)
 *
 * WHAT: Initialize global memory state on first use
 * WHY:  Lazy initialization avoids startup overhead
 * HOW:  Uses nimcp_platform_once for thread-safe init
 *
 * THREAD-SAFETY: nimcp_platform_once guarantees exactly-once execution
 */
static void ensure_initialized(void)
{
    /* Thread-safe one-time initialization */
    nimcp_platform_once(&g_memory_boundary_init_once, memory_boundary_module_init_internal);
}

/**
 * @brief Reset memory boundary state (internal)
 *
 * WHAT: Clear all registered memory regions
 * WHY:  Enable test isolation by resetting between test cases
 * HOW:  Zero out global state and re-initialize under lock
 *
 * THREAD-SAFETY: Acquires g_memory_state_lock before modifying state
 */
void bbb_memory_boundary_reset_internal(void)
{
    /* Ensure module is initialized first */
    ensure_initialized();

    /* Lock before modifying state */
    nimcp_platform_mutex_lock(&g_memory_state_lock);

    memset(&g_memory_state, 0, sizeof(g_memory_state));
    g_memory_state.next_region_id = 1;
    g_memory_state.initialized = true;

    nimcp_platform_mutex_unlock(&g_memory_state_lock);
}

//=============================================================================
// Public API Implementation
//=============================================================================

/**
 * @brief Register memory region for protection
 *
 * WHAT: Add a memory region to the protected list
 * WHY:  Tracked regions can be monitored for unauthorized access
 * HOW:  Store region metadata in internal registry
 *
 * DESIGN PATTERN: Registry Pattern - centralized region tracking
 *
 * COMPLEXITY: O(n) for slot search, O(1) for registration
 *
 * @param system BBB system handle (unused in current impl)
 * @param address Start address of region
 * @param size Size in bytes
 * @param read_only Whether region should be read-only
 * @return Region ID (1+), or 0 on failure
 */
NIMCP_EXPORT uint32_t bbb_register_memory_region(bbb_system_t system,
                                                  void* address,
                                                  size_t size,
                                                  bool read_only)
{
    (void)system;  /* Will be used when integrated with full BBB system */

    /* Guard: Null address */
    if (!address)
        return BBB_INVALID_REGION_ID;

    /* Guard: Zero size */
    if (size == 0)
        return BBB_INVALID_REGION_ID;

    ensure_initialized();

    /* Lock for read-modify-write operations on global state */
    nimcp_platform_mutex_lock(&g_memory_state_lock);

    /* Guard: No available slots */
    int slot = find_available_slot();
    if (slot < 0) {
        nimcp_platform_mutex_unlock(&g_memory_state_lock);
        fprintf(stderr, "[BBB-MEM] Region registry full (max %d)\n",
                BBB_MAX_MEMORY_REGIONS);
        return BBB_INVALID_REGION_ID;
    }

    /* WHAT: Assign unique ID and store region metadata
     * WHY:  ID enables future unregistration and lookup
     */
    uint32_t region_id = g_memory_state.next_region_id++;
    bbb_memory_region_t* region = &g_memory_state.regions[slot];

    region->id = region_id;
    region->address = address;
    region->size = size;
    region->read_only = read_only;
    region->active = true;
    region->registration_ts = (uint64_t)time(NULL);

    g_memory_state.region_count++;
    g_memory_state.total_protected_bytes += size;

    nimcp_platform_mutex_unlock(&g_memory_state_lock);

    return region_id;
}

/**
 * @brief Unregister memory region
 *
 * WHAT: Remove a memory region from protection
 * WHY:  Allows dynamic region management (e.g., freed memory)
 * HOW:  Mark region slot as inactive
 *
 * COMPLEXITY: O(n) for ID lookup
 *
 * @param system BBB system handle
 * @param region_id Region ID returned from registration
 * @return true on success, false if region not found
 */
NIMCP_EXPORT bool bbb_unregister_memory_region(bbb_system_t system,
                                                uint32_t region_id)
{
    (void)system;

    /* Guard: Invalid region ID */
    if (region_id == BBB_INVALID_REGION_ID) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_unregister_memory_region: validation failed");
        return false;
    }

    ensure_initialized();

    /* Lock for read-modify-write operations on global state */
    nimcp_platform_mutex_lock(&g_memory_state_lock);

    /* Find region by ID */
    int slot = find_region_by_id(region_id);
    if (slot < 0) {
        nimcp_platform_mutex_unlock(&g_memory_state_lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_unregister_memory_region: validation failed");
        return false;
    }

    /* WHAT: Mark region as inactive
     * WHY:  Slot can be reused for new registrations
     */
    bbb_memory_region_t* region = &g_memory_state.regions[slot];
    g_memory_state.total_protected_bytes -= region->size;
    g_memory_state.region_count--;

    memset(region, 0, sizeof(bbb_memory_region_t));

    nimcp_platform_mutex_unlock(&g_memory_state_lock);

    return true;
}

/**
 * @brief Check if memory access is within registered regions
 *
 * WHAT: Verify that an access falls within a protected region
 * WHY:  Detects out-of-bounds access before it causes corruption
 * HOW:  Check if [address, address+size) falls within any region
 *
 * COMPLEXITY: O(n) where n = number of registered regions
 *
 * @param system BBB system handle
 * @param address Address being accessed
 * @param size Size of access
 * @param write Whether this is a write access
 * @return true if access is valid, false if violation
 */
NIMCP_EXPORT bool bbb_check_memory_access(bbb_system_t system,
                                           const void* address,
                                           size_t size,
                                           bool write)
{
    /* Guard: Null address */
    if (!address)
        {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "bbb_check_memory_access: address is NULL");

            return false;

        }

    /* Guard: Zero size is technically valid (no access) */
    if (size == 0)
        return true;

    /* Check if memory is quarantined (access denied if quarantined) */
    if (bbb_is_quarantined(system, address, size)) {
        return false;
    }

    ensure_initialized();

    uintptr_t access_start = (uintptr_t)address;
    uintptr_t access_end = access_start + size;

    /* WHAT: Search for a region containing this access
     * WHY:  Access is valid if fully contained within registered region
     */
    for (uint32_t i = 0; i < BBB_MAX_MEMORY_REGIONS; i++) {
        bbb_memory_region_t* region = &g_memory_state.regions[i];

        if (!region->active)
            continue;

        uintptr_t region_start = (uintptr_t)region->address;
        uintptr_t region_end = region_start + region->size;

        /* WHAT: Check if access is fully contained within this region
         * WHY:  Partial overlaps are violations (accessing outside bounds)
         */
        if (access_start >= region_start && access_end <= region_end) {
            /* WHAT: Check write permission
             * WHY:  Read-only regions must reject write attempts
             */
            if (write && region->read_only) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_check_memory_access: validation failed");
                return false;  /* Write to read-only region */
            }
            return true;  /* Valid access within region */
        }
    }

    /* Access not within any registered region */
    return false;
}

/**
 * @brief Protect memory region using mprotect()
 *
 * WHAT: Apply OS-level memory protection to a region
 * WHY:  Hardware-enforced protection catches violations immediately
 * HOW:  Call mprotect() with appropriate permission flags
 *
 * SECURITY: mprotect() provides kernel-level enforcement
 *
 * NOTE: Address must be page-aligned for mprotect() to work
 *
 * @param system BBB system handle
 * @param address Start address (should be page-aligned)
 * @param size Size of region
 * @param read Allow read access
 * @param write Allow write access
 * @param execute Allow execute access
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool bbb_protect_memory(bbb_system_t system,
                                      void* address,
                                      size_t size,
                                      bool read,
                                      bool write,
                                      bool execute)
{
    (void)system;

    /* Guard: Null address */
    if (!address)
        {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "bbb_protect_memory: address is NULL");

            return false;

        }

    /* Guard: Zero size */
    if (size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_protect_memory: size is zero");
        return false;
    }

#ifndef _WIN32
    /* WHAT: Build mprotect permission flags
     * WHY:  Convert boolean flags to PROT_* bitmask
     */
    int prot = PROT_NONE;

    if (read)
        prot |= PROT_READ;
    if (write)
        prot |= PROT_WRITE;
    if (execute)
        prot |= PROT_EXEC;

    /* WHAT: Enforce W^X (Write XOR Execute) policy
     * WHY:  Writable+executable memory is a security risk (code injection)
     */
    if (write && execute) {
        fprintf(stderr, "[BBB-MEM] W^X violation: cannot set both write and execute\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_protect_memory: validation failed");
        return false;
    }

    /* WHAT: Apply OS-level protection
     * WHY:  Kernel enforces protection, faults on violation
     */
    if (mprotect(address, size, prot) != 0) {
        perror("[BBB-MEM] mprotect failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_protect_memory: validation failed");
        return false;
    }

    return true;
#else
    /* Windows would use VirtualProtect() */
    (void)read;
    (void)write;
    (void)execute;
    fprintf(stderr, "[BBB-MEM] mprotect not implemented for Windows\n");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_protect_memory: operation failed");
    return false;
#endif
}

/**
 * @brief Install stack canary at specified stack location
 *
 * WHAT: Write a random canary value to detect stack smashing
 * WHY:  Buffer overflows overwrite canary, detected on function return
 * HOW:  Generate random value, write to stack pointer location
 *
 * DESIGN PATTERN: Defense-in-depth - software canary complements compiler
 *
 * USAGE: Call at function entry, verify at function exit
 *
 * @param system BBB system handle
 * @param stack_ptr Pointer to stack location for canary
 * @return Canary value that was installed
 */
NIMCP_EXPORT uint64_t bbb_install_stack_canary(bbb_system_t system,
                                                void* stack_ptr)
{
    (void)system;

    /* Guard: Null stack pointer */
    if (!stack_ptr)
        return 0;

    ensure_initialized();

    /* WHAT: Generate and install random canary
     * WHY:  Random value is unpredictable to attackers
     */
    uint64_t canary = generate_random_canary();
    *(uint64_t*)stack_ptr = canary;

    return canary;
}

/**
 * @brief Verify stack canary is intact
 *
 * WHAT: Check if stack canary has been overwritten
 * WHY:  Detects stack buffer overflows that corrupt return address
 * HOW:  Compare current value at stack_ptr with expected canary
 *
 * SECURITY: Stack smashing attacks overwrite canary before return address
 *
 * @param system BBB system handle
 * @param stack_ptr Pointer to stack location of canary
 * @param expected_canary Original canary value from installation
 * @return true if canary is intact, false if corrupted
 */
NIMCP_EXPORT bool bbb_verify_stack_canary(bbb_system_t system,
                                           void* stack_ptr,
                                           uint64_t expected_canary)
{
    (void)system;

    /* Guard: Null stack pointer */
    if (!stack_ptr)
        {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "bbb_verify_stack_canary: stack_ptr is NULL");

            return false;

        }

    /* Guard: Zero canary is likely uninitialized */
    if (expected_canary == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_verify_stack_canary: expected_canary is zero");
        return false;
    }

    /* WHAT: Read current value and compare
     * WHY:  If different, stack was corrupted (overflow/smashing)
     */
    uint64_t current_canary = *(uint64_t*)stack_ptr;

    if (current_canary != expected_canary) {
        /* WHAT: Stack smashing detected - log critical event
         * WHY:  This is a serious security incident
         */
        fprintf(stderr,
                "[BBB-MEM] CRITICAL: Stack canary corrupted! "
                "Expected: 0x%016llx, Found: 0x%016llx\n",
                (unsigned long long)expected_canary,
                (unsigned long long)current_canary);
        return false;
    }

    return true;
}
