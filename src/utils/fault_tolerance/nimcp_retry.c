/**
 * @file nimcp_retry.c
 * @brief Retry framework with exponential backoff implementation
 *
 * WHAT: Generic retry mechanism with configurable backoff, jitter, and circuit breaker
 * WHY:  Transient failures often succeed on retry; exponential backoff prevents
 *       resource exhaustion; jitter prevents thundering herd synchronization
 * HOW:  Execute -> fail -> sleep(delay * backoff^attempt * jitter) -> retry
 *
 * BACKOFF SEQUENCE (defaults):
 *   Attempt 0: ~10ms (initial_delay)
 *   Attempt 1: ~20ms (10 * 2^1)
 *   Attempt 2: ~40ms (10 * 2^2)
 *   Attempt 3: ~80ms (10 * 2^3)
 *   Attempt 4: ~160ms (10 * 2^4)
 *   (Each with +/-25% jitter)
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include "utils/fault_tolerance/nimcp_retry.h"
#include "utils/fault_tolerance/nimcp_recovery_cache.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "utils_retry"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(retry)

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

#include "utils/memory/nimcp_unified_memory.h"

/* Fallback logging macros */
#ifndef LOG_INFO
#define LOG_INFO(fmt, ...) fprintf(stderr, "[INFO] " fmt "\n", ##__VA_ARGS__)
#endif
#ifndef LOG_WARN
#define LOG_WARN(fmt, ...) fprintf(stderr, "[WARN] " fmt "\n", ##__VA_ARGS__)
#endif
#ifndef LOG_ERROR
#define LOG_ERROR(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)
#endif
#ifndef LOG_DEBUG
#define LOG_DEBUG(fmt, ...) fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#endif

/* ============================================================================
 * RETRY STATISTICS (module-level, thread-safe)
 * ============================================================================ */

typedef struct {
    uint64_t total_retries;        /**< Total retry loops initiated */
    uint64_t total_attempts;       /**< Total individual attempts across all retries */
    uint64_t successes;            /**< Operations that eventually succeeded */
    uint64_t exhausted;            /**< Operations that exhausted all retries */
    uint64_t circuit_blocked;      /**< Operations blocked by circuit breaker */
    uint64_t total_delay_ms;       /**< Cumulative backoff delay across all retries */
} retry_stats_t;

static retry_stats_t g_retry_stats = {0};
static nimcp_mutex_t g_retry_stats_mutex = NIMCP_MUTEX_INITIALIZER;

/* ============================================================================
 * INTERNAL HELPERS
 * ============================================================================ */

/**
 * WHAT: Get current time in microseconds
 * WHY:  Measure retry loop duration
 * HOW:  gettimeofday() for portable wall-clock time
 */
static uint64_t retry_get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

/**
 * WHAT: Sleep for a given number of milliseconds
 * WHY:  Backoff delay between retry attempts
 * HOW:  Convert to microseconds and call usleep()
 */
static void retry_sleep_ms(uint32_t ms) {
    if (ms > 0) {
        usleep((useconds_t)ms * 1000U);
    }
}

/**
 * WHAT: Generate a pseudo-random float in [-1.0, 1.0]
 * WHY:  Jitter factor to prevent thundering herd synchronization
 * HOW:  Use thread-local seed with rand_r() for thread safety
 *
 * NOTE: This is not cryptographically secure, but sufficient for jitter.
 *       The goal is decorrelation, not unpredictability.
 */
static float retry_random_jitter(void) {
    static __thread unsigned int seed = 0;
    if (seed == 0) {
        /* Seed with time + thread-unique data */
        struct timeval tv;
        gettimeofday(&tv, NULL);
        seed = (unsigned int)(tv.tv_usec ^ (unsigned long)(uintptr_t)&seed);
    }
    /* rand_r returns [0, RAND_MAX], scale to [-1.0, 1.0] */
    int r = rand_r(&seed);
    return ((float)r / (float)RAND_MAX) * 2.0f - 1.0f;
}

/**
 * WHAT: Compute backoff delay for a given attempt
 * WHY:  Exponential backoff with jitter and capping
 * HOW:  delay = initial * factor^attempt * (1 + random * jitter), clamped to max
 *
 * @param config  Retry configuration
 * @param attempt Current attempt number (0-based)
 * @return Delay in milliseconds to sleep before next attempt
 */
static uint32_t compute_delay_ms(const nimcp_retry_config_t* config, uint32_t attempt) {
    /* Base exponential: initial_delay * backoff_factor^attempt */
    float delay = (float)config->initial_delay_ms * powf(config->backoff_factor, (float)attempt);

    /* Apply jitter: delay * (1.0 + random * jitter_factor) */
    if (config->jitter_factor > 0.0f) {
        float jitter = retry_random_jitter() * config->jitter_factor;
        delay *= (1.0f + jitter);
    }

    /* Clamp to [0, max_delay_ms] */
    if (delay < 0.0f) {
        delay = 0.0f;
    }
    if (delay > (float)config->max_delay_ms) {
        delay = (float)config->max_delay_ms;
    }

    return (uint32_t)delay;
}

/**
 * WHAT: Update global retry statistics (thread-safe)
 * WHY:  Track retry behavior for monitoring and tuning
 * HOW:  Mutex-protected increment of counters
 */
static void update_stats(bool success, bool circuit_blocked,
                         uint32_t attempts, uint32_t delay_ms) {
    nimcp_mutex_lock(&g_retry_stats_mutex);
    g_retry_stats.total_retries++;
    g_retry_stats.total_attempts += attempts;
    g_retry_stats.total_delay_ms += delay_ms;
    if (success) {
        g_retry_stats.successes++;
    } else if (circuit_blocked) {
        g_retry_stats.circuit_blocked++;
    } else {
        g_retry_stats.exhausted++;
    }
    nimcp_mutex_unlock(&g_retry_stats_mutex);
}

/* ============================================================================
 * PUBLIC API
 * ============================================================================ */

nimcp_retry_config_t nimcp_retry_default_config(void) {
    nimcp_retry_config_t config = {
        .max_retries      = 5,
        .initial_delay_ms = 10,
        .max_delay_ms     = 5000,
        .backoff_factor   = 2.0f,
        .jitter_factor    = 0.25f,
        .on_retry         = NULL
    };
    return config;
}

nimcp_error_t nimcp_retry_with_backoff(
    operation_t* op,
    const nimcp_retry_config_t* config,
    circuit_breaker_t* cb,
    nimcp_retry_result_t* result)
{
    /* ------------------------------------------------------------------
     * Input validation
     * ------------------------------------------------------------------ */
    if (!op || !op->execute || !config || !result) {
        LOG_ERROR("Retry: Invalid arguments (op=%p, config=%p, result=%p)",
                  (void*)op, (const void*)config, (void*)result);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Initialize result */
    memset(result, 0, sizeof(*result));
    result->last_error = NIMCP_ERROR_OPERATION_FAILED;

    uint64_t start_us = retry_get_time_us();
    uint32_t total_delay_ms = 0;
    const char* op_name = op->name ? op->name : "unnamed";

    LOG_INFO("Retry: Starting operation '%s' (max_retries=%u, initial_delay=%ums, "
             "backoff=%.1f, jitter=%.2f)",
             op_name, config->max_retries, config->initial_delay_ms,
             (double)config->backoff_factor, (double)config->jitter_factor);

    retry_heartbeat("retry_start", 0.0f);

    /* ------------------------------------------------------------------
     * Retry loop
     * ------------------------------------------------------------------ */
    for (uint32_t attempt = 0; attempt <= config->max_retries; attempt++) {

        /* Check circuit breaker before each attempt */
        if (cb) {
            if (!circuit_breaker_allow_operation(cb)) {
                circuit_state_t state = circuit_breaker_get_state(cb);
                LOG_WARN("Retry: Circuit breaker is %s for operation '%s', aborting",
                         state == CIRCUIT_OPEN ? "OPEN" : "HALF_OPEN", op_name);

                result->success = false;
                result->attempts = attempt;
                result->total_delay_ms = total_delay_ms;
                result->last_error = NIMCP_ERROR_INVALID_STATE;

                update_stats(false, true, attempt, total_delay_ms);
                return NIMCP_ERROR_INVALID_STATE;
            }
        }

        /* Report progress via health agent */
        float progress = (float)attempt / (float)(config->max_retries + 1);
        retry_heartbeat("retry_attempt", progress);

        /* Execute the operation */
        op->execution_count++;
        LOG_DEBUG("Retry: Attempt %u/%u for '%s'",
                  attempt + 1, config->max_retries + 1, op_name);

        bool success = op->execute(op->context);

        if (success) {
            /* Operation succeeded */
            result->success = true;
            result->attempts = attempt + 1;
            result->total_delay_ms = total_delay_ms;
            result->last_error = NIMCP_OK;

            /* Record success with circuit breaker */
            if (cb) {
                circuit_breaker_record_success(cb);
            }

            uint64_t elapsed_us = retry_get_time_us() - start_us;
            LOG_INFO("Retry: Operation '%s' succeeded on attempt %u/%u "
                     "(total_delay=%ums, elapsed=%luus)",
                     op_name, attempt + 1, config->max_retries + 1,
                     total_delay_ms, (unsigned long)elapsed_us);

            retry_heartbeat("retry_success", 1.0f);
            update_stats(true, false, attempt + 1, total_delay_ms);
            return NIMCP_OK;
        }

        /* Record failure with circuit breaker */
        if (cb) {
            circuit_breaker_record_failure(cb);
        }

        /* If not the last attempt, apply backoff delay */
        if (attempt < config->max_retries) {
            uint32_t delay_ms = compute_delay_ms(config, attempt);
            total_delay_ms += delay_ms;

            LOG_INFO("Retry: Attempt %u failed for '%s', backing off %ums",
                     attempt + 1, op_name, delay_ms);

            /* Invoke on_retry callback if set */
            if (config->on_retry) {
                config->on_retry(attempt, delay_ms, op->context);
            }

            /* Sleep with backoff */
            retry_sleep_ms(delay_ms);

            /* Heartbeat during delay so health monitor knows we're alive */
            retry_heartbeat("retry_backoff", progress);
        }
    }

    /* ------------------------------------------------------------------
     * All retries exhausted
     * ------------------------------------------------------------------ */
    result->success = false;
    result->attempts = config->max_retries + 1;
    result->total_delay_ms = total_delay_ms;
    result->last_error = NIMCP_ERROR_OPERATION_FAILED;

    uint64_t elapsed_us = retry_get_time_us() - start_us;
    LOG_ERROR("Retry: Operation '%s' failed after %u attempts "
              "(total_delay=%ums, elapsed=%luus)",
              op_name, config->max_retries + 1,
              total_delay_ms, (unsigned long)elapsed_us);

    /* Execute rollback if provided */
    if (op->rollback) {
        LOG_INFO("Retry: Executing rollback for '%s'", op_name);
        op->rollback(op->context);
    }

    retry_heartbeat("retry_exhausted", 1.0f);
    update_stats(false, false, config->max_retries + 1, total_delay_ms);
    return NIMCP_ERROR_OPERATION_FAILED;
}
