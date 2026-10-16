/**
 * @file nimcp_deadlock_detector.c
 * @brief Deadlock detection implementation
 *
 * @author NIMCP Team
 * @date 2025-11-09
 */

#include "utils/thread/nimcp_deadlock_detector.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/platform/nimcp_platform_once.h"
#include "utils/thread/nimcp_atomic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(deadlock_detector)

//=============================================================================
// Internal Data Structures
//=============================================================================

// Global state
static deadlock_detector_config_t g_config;
static deadlock_detector_stats_t g_stats = {0};
static tracked_mutex_t* g_mutex_table[MAX_TRACKED_MUTEXES];
static lock_dependency_t g_thread_deps[MAX_THREADS];
static nimcp_mutex_t g_detector_mutex = NIMCP_MUTEX_INITIALIZER;
static volatile bool g_initialized = false;  // volatile + mutex for thread safety
static uint32_t g_next_order = 0;

//=============================================================================
// Helper Functions
//=============================================================================

static void lock_detector(void) {
    nimcp_mutex_lock(&g_detector_mutex);
}

static void unlock_detector(void) {
    nimcp_mutex_unlock(&g_detector_mutex);
}

static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static lock_dependency_t* find_thread_deps(pthread_t thread_id) {
    int first_empty = -1;
    for (uint32_t i = 0; i < MAX_THREADS; i++) {
        /* Use in_use flag for portable slot detection (don't compare pthread_t to 0) */
        if (g_thread_deps[i].in_use) {
            /* Slot is in use, check if it's our thread using pthread_equal */
            if (pthread_equal(g_thread_deps[i].thread_id, thread_id)) {
                return &g_thread_deps[i];
            }
        } else if (first_empty < 0) {
            /* Track first empty slot for new thread */
            first_empty = (int)i;
        }
    }
    /* New thread, initialize in first empty slot */
    if (first_empty >= 0) {
        g_thread_deps[first_empty].thread_id = thread_id;
        g_thread_deps[first_empty].waiting_on = NULL;
        g_thread_deps[first_empty].num_holding = 0;
        g_thread_deps[first_empty].in_use = true;  /* Mark slot as in use */
        return &g_thread_deps[first_empty];
    }
    /* P2: Thread table full is a capacity condition, not an error */
    return NULL; // Table full
}

static uint32_t get_thread_max_order(lock_dependency_t* deps) {
    uint32_t max_order = 0;
    for (uint32_t i = 0; i < deps->num_holding; i++) {
        if (deps->holding[i] && deps->holding[i]->order > max_order) {
            max_order = deps->holding[i]->order;
        }
    }
    return max_order;
}

static bool check_lock_ordering(lock_dependency_t* deps, tracked_mutex_t* mutex) {
    if (!g_config.enable_lock_ordering || !deps) return true;

    uint32_t max_held_order = get_thread_max_order(deps);

    if (mutex->order <= max_held_order) {
        /* P3: Using raw fprintf intentionally - LOG macros could recurse into lock tracking */
        fprintf(stderr, "\n*** LOCK ORDER VIOLATION DETECTED ***\n");
        fprintf(stderr, "Thread %lu attempting to acquire '%s' (order %u)\n",
                (unsigned long)pthread_self(), mutex->name, mutex->order);
        fprintf(stderr, "But already holds locks with max order %u\n", max_held_order);
        fprintf(stderr, "This may cause deadlock!\n");

        g_stats.order_violations++;

        if (g_config.abort_on_order_violation) {
            abort();
        }
        return false;
    }

    return true;
}

static bool detect_cycle_recursive(pthread_t start_thread, pthread_t current_thread,
                                     bool visited[], int depth) {
    if (depth > MAX_THREADS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "detect_cycle_recursive: max depth exceeded");
        return false;
    }

    // Check if we've cycled back to start (use pthread_equal for portability)
    if (depth > 0 && pthread_equal(current_thread, start_thread)) {
        fprintf(stderr, "\n*** DEADLOCK CYCLE DETECTED (depth %d) ***\n", depth);
        g_stats.cycles_detected++;
        return true;
    }

    // Find what this thread is waiting on
    lock_dependency_t* deps = find_thread_deps(current_thread);
    if (!deps || !deps->waiting_on) {
        /* P2: Thread not waiting on anything is a normal state, not an error */
        return false;
    }

    tracked_mutex_t* waiting_on = deps->waiting_on;

    // Mark as visited (use in_use flag and pthread_equal for portability)
    for (uint32_t i = 0; i < MAX_THREADS; i++) {
        if (g_thread_deps[i].in_use &&
            pthread_equal(g_thread_deps[i].thread_id, current_thread)) {
            visited[i] = true;
            break;
        }
    }

    // Check if mutex is held by another thread - use owner_valid flag (P2-U20)
    if (waiting_on->is_locked && waiting_on->owner_valid) {
        pthread_t owner = waiting_on->owner;

        // Check if already visited (cycle) - use in_use flag and pthread_equal for portability
        for (uint32_t i = 0; i < MAX_THREADS; i++) {
            if (g_thread_deps[i].in_use &&
                pthread_equal(g_thread_deps[i].thread_id, owner) && visited[i]) {
                return true; // Cycle found
            }
        }

        // Recurse to owner thread
        return detect_cycle_recursive(start_thread, owner, visited, depth + 1);
    }

    /* P2: No cycle found from this thread is a normal result */
    return false;
}

//=============================================================================
// Public API Implementation
//=============================================================================

deadlock_detector_config_t deadlock_detector_default_config(void) {
    deadlock_detector_config_t config = {
        .enable_detector = true,
        .enable_lock_ordering = true,
        .enable_timeout = true,
        .abort_on_deadlock = false,  // Log only by default
        .abort_on_order_violation = false,
        .default_timeout_ms = DEFAULT_TIMEOUT_MS,
        .check_interval_ms = 1000  // Check every 1 second
    };
    return config;
}

bool deadlock_detector_init(const deadlock_detector_config_t* config) {
    lock_detector();

    if (g_initialized) {
        unlock_detector();
        return true; // Already initialized
    }

    if (config) {
        g_config = *config;
    } else {
        g_config = deadlock_detector_default_config();
    }

    memset(&g_stats, 0, sizeof(g_stats));
    memset(g_mutex_table, 0, sizeof(g_mutex_table));
    memset(g_thread_deps, 0, sizeof(g_thread_deps));
    g_next_order = 0;
    g_initialized = true;

    unlock_detector();

    if (g_config.enable_detector) {
        /* P3: Using raw printf intentionally here. LOG macros could trigger mutex
         * lock tracking which recurses back into the deadlock detector. */
        printf("Deadlock detector initialized (ordering=%s, timeout=%s, check_interval=%ums)\n",
               g_config.enable_lock_ordering ? "ON" : "OFF",
               g_config.enable_timeout ? "ON" : "OFF",
               g_config.check_interval_ms);
    }

    return true;
}

void deadlock_detector_shutdown(void) {
    // Acquire lock BEFORE checking g_initialized to prevent race condition
    // where another thread could be initializing while we check
    lock_detector();

    if (!g_initialized) {
        unlock_detector();
        return;
    }

    // Capture stats while holding lock
    uint64_t deadlocks = g_stats.deadlocks_detected;
    uint64_t cycles = g_stats.cycles_detected;

    /* P2: Print stats BEFORE releasing lock and setting initialized=false
     * to avoid TOCTOU where print_stats reads stale/zeroed data after
     * another thread re-initializes the detector. */
    printf("\n=== Deadlock Detector Shutdown ===\n");
    /* NOTE: Using raw printf here (not LOG macros) because the deadlock detector
     * uses its own internal locking. Using LOG macros could trigger lock tracking
     * which would recurse into the detector while it's shutting down. */
    printf("Total lock attempts:  %lu\n", (unsigned long)g_stats.total_locks);
    printf("Total unlocks:        %lu\n", (unsigned long)g_stats.total_unlocks);
    printf("Active threads:       %u\n", g_stats.active_threads);
    printf("Active mutexes:       %u\n", g_stats.active_mutexes);
    printf("  Lock timeouts:      %lu\n", (unsigned long)g_stats.lock_timeouts);
    printf("  Order violations:   %lu\n", (unsigned long)g_stats.order_violations);
    printf("  Deadlocks:          %lu\n", (unsigned long)g_stats.deadlocks_detected);
    printf("  Cycles:             %lu\n", (unsigned long)g_stats.cycles_detected);
    printf("====================================\n");

    // Mark as uninitialized while still holding lock
    g_initialized = false;

    unlock_detector();

    if (deadlocks > 0 || cycles > 0) {
        fprintf(stderr, "\n*** WARNING: %lu deadlocks and %lu cycles detected during execution ***\n",
                (unsigned long)deadlocks,
                (unsigned long)cycles);
    }
}

bool tracked_mutex_init(tracked_mutex_t* mutex, const char* name, uint32_t timeout_ms) {
    if (!mutex) {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "tracked_mutex_init: mutex is NULL");

            return false;

        }

    // Initialize underlying mutex
    if (nimcp_mutex_init(&mutex->mutex, NULL) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "tracked_mutex_init: validation failed");
        return false;
    }

    // Setup tracking
    lock_detector();

    mutex->name = name ? name : "unnamed";
    mutex->order = g_next_order++;
    mutex->timeout_ms = (timeout_ms > 0) ? timeout_ms : g_config.default_timeout_ms;
    mutex->owner_valid = false;  /* P2-U20: Use bool flag instead of comparing pthread_t to 0 */
    mutex->is_locked = false;
    mutex->lock_count = 0;
    mutex->timeout_count = 0;
    mutex->contention_count = 0;
    mutex->total_wait_time_us = 0;

    // Add to tracking table
    for (uint32_t i = 0; i < MAX_TRACKED_MUTEXES; i++) {
        if (g_mutex_table[i] == NULL) {
            g_mutex_table[i] = mutex;
            g_stats.active_mutexes++;
            break;
        }
    }

    unlock_detector();

    return true;
}

void tracked_mutex_destroy(tracked_mutex_t* mutex) {
    if (!mutex) return;

    lock_detector();

    // Remove from tracking table
    for (uint32_t i = 0; i < MAX_TRACKED_MUTEXES; i++) {
        if (g_mutex_table[i] == mutex) {
            g_mutex_table[i] = NULL;
            g_stats.active_mutexes--;
            break;
        }
    }

    unlock_detector();

    nimcp_mutex_destroy(&mutex->mutex);
}

bool tracked_mutex_lock(tracked_mutex_t* mutex) {
    if (!mutex) {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "tracked_mutex_lock: mutex is NULL");

            return false;

        }

    /* Use atomic load to safely check g_initialized without holding the detector lock.
     * This avoids the race condition where g_initialized could change between check and use. */
    bool is_initialized = __atomic_load_n(&g_initialized, __ATOMIC_ACQUIRE);
    if (!is_initialized) {
        // Fallback to standard lock when detector not initialized
        nimcp_mutex_lock(&mutex->mutex);
        return true;
    }

    /* Now check enable_detector under the detector lock for full consistency */
    lock_detector();
    bool detector_enabled = g_config.enable_detector;
    unlock_detector();

    if (!detector_enabled) {
        // Fallback to standard lock when detector disabled
        nimcp_mutex_lock(&mutex->mutex);
        return true;
    }

    pthread_t self = pthread_self();
    uint64_t start_time = get_time_us();

    lock_detector();

    // Find/create thread dependency record
    lock_dependency_t* deps = find_thread_deps(self);

    // Check lock ordering
    if (!check_lock_ordering(deps, mutex)) {
        unlock_detector();
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "tracked_mutex_lock: check_lock_ordering is NULL");
        return false; // Order violation
    }

    // Update waiting state
    if (deps) {
        deps->waiting_on = mutex;
    }

    g_stats.total_locks++;

    unlock_detector();

    // Try to acquire lock with timeout
    bool acquired = false;

    if (g_config.enable_timeout && mutex->timeout_ms > 0) {
        // Timeout-based lock
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += mutex->timeout_ms / 1000;
        timeout.tv_nsec += (mutex->timeout_ms % 1000) * 1000000;
        if (timeout.tv_nsec >= 1000000000) {
            timeout.tv_sec++;
            timeout.tv_nsec -= 1000000000;
        }

        int result = pthread_mutex_timedlock(&mutex->mutex, &timeout);
        if (result == 0) {
            acquired = true;
        } else if (result == ETIMEDOUT) {
            fprintf(stderr, "\n*** LOCK TIMEOUT: '%s' (waited %ums) ***\n",
                    mutex->name, mutex->timeout_ms);
            g_stats.lock_timeouts++;
            mutex->timeout_count++;
        }
    } else {
        // Blocking lock (no timeout)
        nimcp_mutex_lock(&mutex->mutex);
        acquired = true;
    }

    uint64_t wait_time = get_time_us() - start_time;

    // Update state if acquired
    if (acquired) {
        lock_detector();

        mutex->is_locked = true;
        mutex->owner = self;
        mutex->owner_valid = true;  /* P2-U20: Mark owner as valid */
        mutex->lock_count++;
        mutex->total_wait_time_us += wait_time;

        if (wait_time > 1000) { // More than 1ms = contention
            mutex->contention_count++;
        }

        // Update thread dependencies
        if (deps) {
            deps->waiting_on = NULL; // No longer waiting
            if (deps->num_holding < MAX_LOCKS_PER_THREAD) {
                deps->holding[deps->num_holding++] = mutex;
            }
        }

        unlock_detector();
    } else {
        // Failed to acquire
        lock_detector();
        if (deps) {
            deps->waiting_on = NULL; // No longer waiting
        }
        unlock_detector();
    }

    return acquired;
}

bool tracked_mutex_trylock(tracked_mutex_t* mutex) {
    if (!mutex) {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "tracked_mutex_trylock: mutex is NULL");

            return false;

        }

    /* P2: Use NIMCP_SUCCESS instead of raw 0 for consistency */
    int result = nimcp_mutex_trylock(&mutex->mutex);
    if (result == NIMCP_SUCCESS) {
        // Acquired
        lock_detector();
        mutex->is_locked = true;
        mutex->owner = pthread_self();
        mutex->owner_valid = true;  /* P2-U20: Mark owner as valid */
        mutex->lock_count++;
        unlock_detector();
        return true;
    }

    /* P2: trylock returning EBUSY is normal contention, not an error */
    return false;
}

void tracked_mutex_unlock(tracked_mutex_t* mutex) {
    if (!mutex) return;

    lock_detector();

    pthread_t self = pthread_self();

    // Update tracking - use pthread_equal for portability (P2-U20)
    if (mutex->owner_valid && pthread_equal(mutex->owner, self)) {
        mutex->is_locked = false;
        mutex->owner_valid = false;  /* P2-U20: Clear flag instead of setting owner=0 */
        g_stats.total_unlocks++;

        // Remove from thread's holding list
        lock_dependency_t* deps = find_thread_deps(self);
        if (deps) {
            for (uint32_t i = 0; i < deps->num_holding; i++) {
                if (deps->holding[i] == mutex) {
                    // Shift remaining locks
                    for (uint32_t j = i; j < deps->num_holding - 1; j++) {
                        deps->holding[j] = deps->holding[j + 1];
                    }
                    deps->num_holding--;
                    break;
                }
            }
        }
    }

    unlock_detector();

    nimcp_mutex_unlock(&mutex->mutex);
}

uint32_t deadlock_detector_check(void) {
    if (!g_initialized || !g_config.enable_detector) {
        /* P2: Not initialized or detector disabled is a normal state */
        return 0;
    }

    lock_detector();

    uint32_t deadlocks = 0;

    // Check each thread for cycles
    for (uint32_t i = 0; i < MAX_THREADS; i++) {
        if (!g_thread_deps[i].in_use) continue;  /* Use in_use flag for portability */
        if (g_thread_deps[i].waiting_on == NULL) continue;

        // Check for cycle starting from this thread
        bool visited[MAX_THREADS] = {false};
        if (detect_cycle_recursive(g_thread_deps[i].thread_id,
                                    g_thread_deps[i].thread_id,
                                    visited, 0)) {
            deadlocks++;
            g_stats.deadlocks_detected++;

            if (g_config.abort_on_deadlock) {
                fprintf(stderr, "Aborting due to deadlock detection\n");
                abort();
            }
        }
    }

    unlock_detector();

    return deadlocks;
}

deadlock_detector_stats_t deadlock_detector_get_stats(void) {
    /* P2-U21: Copy stats under lock to get a consistent snapshot */
    lock_detector();
    deadlock_detector_stats_t snapshot = g_stats;
    unlock_detector();
    return snapshot;
}

void deadlock_detector_print_stats(void) {
    lock_detector();
    deadlock_detector_stats_t snapshot = g_stats;
    unlock_detector();

    printf("\n=== Deadlock Detector Statistics ===\n");
    printf("Total lock attempts:  %lu\n", (unsigned long)snapshot.total_locks);
    printf("Total unlocks:        %lu\n", (unsigned long)snapshot.total_unlocks);
    printf("Active threads:       %u\n", snapshot.active_threads);
    printf("Active mutexes:       %u\n", snapshot.active_mutexes);
    printf("\nErrors Detected:\n");
    printf("  Lock timeouts:      %lu\n", (unsigned long)snapshot.lock_timeouts);
    printf("  Order violations:   %lu\n", (unsigned long)snapshot.order_violations);
    printf("  Deadlocks:          %lu\n", (unsigned long)snapshot.deadlocks_detected);
    printf("  Cycles:             %lu\n", (unsigned long)snapshot.cycles_detected);
    printf("====================================\n\n");
}

void deadlock_detector_report(void) {
    lock_detector();

    printf("\n=== Lock State Report ===\n");

    for (uint32_t i = 0; i < MAX_TRACKED_MUTEXES; i++) {
        tracked_mutex_t* m = g_mutex_table[i];
        if (!m) continue;

        printf("Mutex '%s' (order %u):\n", m->name, m->order);
        printf("  Locked: %s\n", m->is_locked ? "YES" : "NO");
        if (m->is_locked) {
            printf("  Owner: %lu\n", (unsigned long)m->owner);
        }
        printf("  Lock count: %lu\n", (unsigned long)m->lock_count);
        printf("  Timeouts: %lu\n", (unsigned long)m->timeout_count);
        printf("  Contention: %lu\n", (unsigned long)m->contention_count);
        printf("  Avg wait: %lu us\n",
               m->lock_count > 0 ? (unsigned long)(m->total_wait_time_us / m->lock_count) : 0);
        printf("\n");
    }

    printf("=========================\n\n");

    unlock_detector();
}

void deadlock_detector_print_dependencies(void) {
    lock_detector();

    printf("\n=== Thread Dependencies ===\n");

    for (uint32_t i = 0; i < MAX_THREADS; i++) {
        if (!g_thread_deps[i].in_use) continue;  /* Use in_use flag for portability */

        printf("Thread %lu:\n", (unsigned long)g_thread_deps[i].thread_id);

        if (g_thread_deps[i].waiting_on) {
            printf("  Waiting on: '%s'\n", g_thread_deps[i].waiting_on->name);
        }

        if (g_thread_deps[i].num_holding > 0) {
            printf("  Holding: ");
            for (uint32_t j = 0; j < g_thread_deps[i].num_holding; j++) {
                printf("'%s'", g_thread_deps[i].holding[j]->name);
                if (j < g_thread_deps[i].num_holding - 1) printf(", ");
            }
            printf("\n");
        }

        printf("\n");
    }

    printf("===========================\n\n");

    unlock_detector();
}

void deadlock_detector_set_enabled(bool enable) {
    lock_detector();
    g_config.enable_detector = enable;
    unlock_detector();
}

bool deadlock_detector_is_enabled(void) {
    /* Use atomic load to avoid race condition on g_initialized */
    bool initialized = __atomic_load_n(&g_initialized, __ATOMIC_ACQUIRE);
    if (!initialized) {
        /* P2: Not initialized is a normal state - just report disabled */
        return false;
    }
    lock_detector();
    bool enabled = g_config.enable_detector;
    unlock_detector();
    return enabled;
}

void deadlock_detector_set_default_timeout(uint32_t timeout_ms) {
    lock_detector();
    g_config.default_timeout_ms = timeout_ms;
    unlock_detector();
}
