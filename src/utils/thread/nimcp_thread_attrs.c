#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_thread_attrs.c - Thread Attributes (Naming and Affinity)
//=============================================================================
// Enable GNU extensions for CPU affinity APIs (pthread_setaffinity_np, cpu_set_t)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "utils/thread/nimcp_thread.h"
#include "nimcp_thread_internal.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <errno.h>
#include <string.h>
#include <pthread.h>

#define LOG_MODULE "thread_attrs"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(thread_attrs)

//=============================================================================
// Thread Naming
//=============================================================================

/**
 * @brief Set name of calling thread (Adapter for pthread_setname_np)
 *
 * WHY THREAD NAMING:
 * - Debugger visibility: gdb shows thread names in 'info threads'
 * - Profiler clarity: perf, vtune show named threads
 * - Log correlation: Associate log messages with specific threads
 * - Development aid: Easier to identify threads in dumps/traces
 *
 * ALGORITHM:
 * 1. Validate name pointer
 * 2. Check name length (pthread limit is 16 chars including null)
 * 3. Call pthread_setname_np with current thread handle
 * 4. Handle errors (name too long, permission denied)
 *
 * PLATFORM NOTES:
 * - Linux: pthread_setname_np available (glibc 2.12+)
 * - macOS: Different signature (takes pthread_t explicitly)
 * - Windows: No pthread equivalent (use SetThreadDescription on Win10+)
 * - Other POSIX: May not be available (compile-time check)
 *
 * WHY 16 CHARACTER LIMIT:
 * - Linux kernel limit (TASK_COMM_LEN = 16 in kernel)
 * - 15 characters + null terminator
 * - Historical limit from early Unix
 *
 * TYPICAL USAGE (worker threads):
 *   void* worker_thread(void* arg) {
 *       nimcp_thread_set_name("bcm_worker");
 *       // ... thread work ...
 *   }
 *
 * DEBUGGER EXAMPLE:
 *   (gdb) info threads
 *     Id   Target Id         Frame     Name
 *   * 1    Thread 0x7f... main          main() at main.c:42
 *     2    Thread 0x7f... bcm_worker    bcm_update() at bcm.c:156
 *     3    Thread 0x7f... neuro_updater neuromod_process() at neuro.c:89
 *
 * COMPLEXITY: O(1) (kernel syscall)
 * THREAD SAFETY: Fully safe (only affects calling thread)
 *
 * @param name Thread name (max 15 chars + null, will be truncated if longer)
 * @return NIMCP_SUCCESS or NIMCP_ERROR_INVALID_PARAM or NIMCP_ERROR_SYSTEM
 */
nimcp_result_t nimcp_thread_set_name(const char* name)
{
    if (!name) {
        set_thread_error(NIMCP_ERROR_INVALID_PARAM, "Invalid thread name pointer");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM,
                          "nimcp_thread_set_name: name is NULL");
    }

#if defined(__linux__) || defined(__APPLE__)
    // WHAT: Truncate name to platform limits
    // WHY: Linux limit is 16 chars (15 + null), macOS is similar
    // HOW: Copy to local buffer with size limit
    char truncated_name[NIMCP_THREAD_NAME_MAX];
    size_t len = strlen(name);
    if (len >= NIMCP_THREAD_NAME_MAX) {
        // Truncate to 15 chars + null terminator
        strncpy(truncated_name, name, NIMCP_THREAD_NAME_MAX - 1);
        truncated_name[NIMCP_THREAD_NAME_MAX - 1] = '\0';
    } else {
        // Name fits, copy as-is (use strncpy for consistency/safety)
        strncpy(truncated_name, name, NIMCP_THREAD_NAME_MAX - 1);
        truncated_name[NIMCP_THREAD_NAME_MAX - 1] = '\0';
    }

    // Linux and macOS support pthread_setname_np
    // Note: macOS has different signature but we use Linux version
    #ifdef __linux__
        // Linux: pthread_setname_np(pthread_self(), name)
        int result = pthread_setname_np(pthread_self(), truncated_name);
        if (result != 0) {
            set_thread_error(NIMCP_ERROR_SYSTEM, "pthread_setname_np failed: %s", strerror(result));
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_SYSTEM,
                              "nimcp_thread_set_name: pthread_setname_np failed");
        }
    #elif __APPLE__
        // macOS: pthread_setname_np(name) - operates on current thread
        int result = pthread_setname_np(truncated_name);
        if (result != 0) {
            set_thread_error(NIMCP_ERROR_SYSTEM, "pthread_setname_np failed: %s", strerror(result));
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_SYSTEM,
                              "nimcp_thread_set_name: pthread_setname_np failed");
        }
    #endif
#else
    // Platform doesn't support thread naming (not an error, just no-op)
    // WHY: Graceful degradation on platforms without this feature
    (void)name;  // Suppress unused parameter warning
#endif

    return NIMCP_SUCCESS;
}

/**
 * @brief Get name of calling thread (Adapter for pthread_getname_np)
 *
 * WHY GET THREAD NAME:
 * - Verify thread name was set correctly
 * - Log current thread name in error messages
 * - Testing and debugging
 *
 * ALGORITHM:
 * 1. Validate output buffer and length
 * 2. Check buffer is large enough (NIMCP_THREAD_NAME_MAX)
 * 3. Call pthread_getname_np with current thread handle
 * 4. Copy name to output buffer
 *
 * PLATFORM NOTES:
 * - Linux: pthread_getname_np available (glibc 2.12+)
 * - macOS: pthread_getname_np available (macOS 10.6+)
 * - Windows: No pthread equivalent
 * - Other POSIX: May not be available (compile-time check)
 *
 * TYPICAL USAGE (logging):
 *   char thread_name[NIMCP_THREAD_NAME_MAX];
 *   if (nimcp_thread_get_name(thread_name, sizeof(thread_name)) == NIMCP_SUCCESS) {
 *       fprintf(stderr, "[%s] Error occurred\n", thread_name);
 *   }
 *
 * COMPLEXITY: O(1) (kernel syscall)
 * THREAD SAFETY: Fully safe (only affects calling thread)
 *
 * @param name Buffer to receive thread name
 * @param len Buffer length (must be at least NIMCP_THREAD_NAME_MAX)
 * @return NIMCP_SUCCESS or NIMCP_ERROR_INVALID_PARAM or NIMCP_ERROR_SYSTEM
 */
nimcp_result_t nimcp_thread_get_name(char* name, size_t len)
{
    if (!name || len < NIMCP_THREAD_NAME_MAX) {
        set_thread_error(NIMCP_ERROR_INVALID_PARAM, "Invalid name buffer or length");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM,
                          "nimcp_thread_get_name: invalid buffer or length");
    }

#if defined(__linux__) || defined(__APPLE__)
    // Linux and macOS support pthread_getname_np
    int result = pthread_getname_np(pthread_self(), name, len);
    if (result != 0) {
        set_thread_error(NIMCP_ERROR_SYSTEM, "pthread_getname_np failed: %s", strerror(result));
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_SYSTEM,
                          "nimcp_thread_get_name: pthread_getname_np failed");
    }
#else
    // Platform doesn't support thread naming
    // WHY: Return empty string on platforms without this feature
    name[0] = '\0';
#endif

    return NIMCP_SUCCESS;
}

//=============================================================================
// Thread Affinity
//=============================================================================

/**
 * @brief Set CPU affinity for thread (Adapter for pthread_setaffinity_np)
 *
 * WHY CPU AFFINITY:
 * - Performance: Pin thread to specific core (cache locality)
 * - Real-time: Isolate critical threads from general workload
 * - NUMA optimization: Bind thread to core with local memory
 * - Benchmarking: Eliminate scheduling variability
 *
 * ALGORITHM:
 * 1. Validate thread handle and cpu_id
 * 2. Create CPU set with single CPU
 * 3. Call pthread_setaffinity_np
 * 4. Handle errors (invalid CPU, permission denied)
 *
 * PLATFORM NOTES:
 * - Linux: pthread_setaffinity_np available (glibc 2.3.4+)
 * - macOS: No pthread_setaffinity_np (use thread_policy_set)
 * - Windows: Use SetThreadAffinityMask
 * - Other POSIX: May not be available (returns success, no-op)
 *
 * WHY LINUX-SPECIFIC:
 * - Not in POSIX standard (Linux extension)
 * - Platform-specific CPU scheduling policy
 * - Optional optimization (not required for correctness)
 *
 * TYPICAL USAGE (real-time thread):
 *   nimcp_thread_t rt_thread;
 *   nimcp_thread_create(&rt_thread, rt_worker, NULL, NULL);
 *   nimcp_thread_set_affinity(rt_thread, 3);  // Pin to CPU 3
 *
 * PERFORMANCE BENEFITS:
 * - Cache affinity: Data stays in L1/L2 cache
 * - No migration overhead: Thread stays on same core
 * - Predictable timing: No variability from core migration
 *
 * COMPLEXITY: O(1) (kernel syscall)
 * THREAD SAFETY: Fully safe
 *
 * @param thread Thread handle
 * @param cpu_id CPU core ID to bind to (0-based)
 * @return NIMCP_SUCCESS or NIMCP_ERROR_SYSTEM
 */
nimcp_result_t nimcp_thread_set_affinity(nimcp_thread_t thread, uint32_t cpu_id)
{
#ifdef __linux__
    // Linux: Use pthread_setaffinity_np with cpu_set_t
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);

    int result = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (result != 0) {
        set_thread_error(NIMCP_ERROR_SYSTEM, "pthread_setaffinity_np failed: %s", strerror(result));
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_SYSTEM,
                          "nimcp_thread_set_affinity: pthread_setaffinity_np failed");
    }
#else
    // Platform doesn't support CPU affinity (not an error, just no-op)
    // WHY: Graceful degradation - affinity is optimization, not requirement
    (void)thread;
    (void)cpu_id;
#endif

    return NIMCP_SUCCESS;
}

/**
 * @brief Get CPU affinity for thread (Adapter for pthread_getaffinity_np)
 *
 * WHY GET AFFINITY:
 * - Verify affinity was set correctly
 * - Query current CPU assignment
 * - Testing and debugging
 *
 * ALGORITHM:
 * 1. Validate thread handle and cpu_id pointer
 * 2. Call pthread_getaffinity_np
 * 3. Find first set CPU in cpu_set
 * 4. Return CPU ID to caller
 *
 * PLATFORM NOTES:
 * - Linux: pthread_getaffinity_np available (glibc 2.3.4+)
 * - macOS: No pthread_getaffinity_np
 * - Windows: Use GetThreadAffinityMask
 * - Other POSIX: Returns 0 (no affinity set)
 *
 * TYPICAL USAGE (verification):
 *   uint32_t cpu;
 *   nimcp_thread_get_affinity(thread, &cpu);
 *   printf("Thread bound to CPU %u\n", cpu);
 *
 * COMPLEXITY: O(1) (kernel syscall)
 * THREAD SAFETY: Fully safe
 *
 * @param thread Thread handle
 * @param cpu_id Output parameter for CPU core ID
 * @return NIMCP_SUCCESS or NIMCP_ERROR_INVALID_PARAM or NIMCP_ERROR_SYSTEM
 */
nimcp_result_t nimcp_thread_get_affinity(nimcp_thread_t thread, uint32_t* cpu_id)
{
    if (!cpu_id) {
        set_thread_error(NIMCP_ERROR_INVALID_PARAM, "Invalid cpu_id pointer");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM,
                          "nimcp_thread_get_affinity: cpu_id is NULL");
    }

#ifdef __linux__
    // Linux: Use pthread_getaffinity_np with cpu_set_t
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);

    int result = pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (result != 0) {
        set_thread_error(NIMCP_ERROR_SYSTEM, "pthread_getaffinity_np failed: %s", strerror(result));
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_SYSTEM,
                          "nimcp_thread_get_affinity: pthread_getaffinity_np failed");
    }

    // Find first set CPU in the set
    // WHY: Thread may be bound to multiple CPUs (return first one)
    for (int i = 0; i < CPU_SETSIZE; i++) {
        if (CPU_ISSET(i, &cpuset)) {
            *cpu_id = (uint32_t)i;
            return NIMCP_SUCCESS;
        }
    }

    // No CPU set (should not happen if setaffinity was called)
    *cpu_id = 0;
#else
    // Platform doesn't support CPU affinity
    // WHY: Return 0 as default (no specific CPU)
    *cpu_id = 0;
#endif

    return NIMCP_SUCCESS;
}
