/**
 * @file nimcp_signal_exception_queue.c
 * @brief Signal exception queue implementation using NIMCP queue utilities
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: Implementation of signal-safe queue for crash context to exception bridging
 * WHY:  Enable deferred processing of signal crashes as exceptions
 * HOW:  Uses NIMCP SPSC queue for lock-free signal-safe operations
 *
 * @author NIMCP Development Team
 */

#include "utils/signal/nimcp_signal_exception_queue.h"
#include "utils/containers/nimcp_queue.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(signal_exception_queue)

/* ============================================================================
 * Module State
 * ============================================================================ */

/** The SPSC queue handle */
static nimcp_queue_handle_t g_signal_queue = NULL;

/** Initialization flag */
static bool g_initialized = false;

/** Statistics counters (updated atomically where needed) */
static volatile uint64_t g_enqueue_count = 0;
static volatile uint64_t g_dequeue_count = 0;
static volatile uint64_t g_overflow_count = 0;

/** Callback for custom exception processing */
static signal_exception_callback_t g_callback = NULL;
static void* g_callback_user_data = NULL;

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Get current timestamp in microseconds (signal-safe version)
 */
static uint64_t get_timestamp_us_signal_safe(void)
{
    struct timespec ts;
    /* clock_gettime is async-signal-safe per POSIX */
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/* ============================================================================
 * Queue API Implementation
 * ============================================================================ */

int signal_exception_queue_init(void)
{
    if (g_initialized) {
        return 0; /* Already initialized */
    }

    /* Configure SPSC queue for signal exception entries */
    nimcp_queue_config_t config = nimcp_queue_default_config(NIMCP_QUEUE_TYPE_SPSC);
    config.max_size = SIGNAL_EXCEPTION_QUEUE_SIZE;
    config.item_size = sizeof(signal_exception_entry_t);
    config.is_blocking = false;  /* Never block in signal handler */
    config.timeout_ms = 0;

    nimcp_result_t result = nimcp_queue_create(&config, &g_signal_queue);
    if (result != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to create signal exception queue: %d", result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "signal_exception_queue_init: validation failed");
        return -1;
    }

    /* Reset statistics */
    g_enqueue_count = 0;
    g_dequeue_count = 0;
    g_overflow_count = 0;
    g_callback = NULL;
    g_callback_user_data = NULL;

    g_initialized = true;
    LOG_INFO("Signal exception queue initialized (capacity=%d)", SIGNAL_EXCEPTION_QUEUE_SIZE);

    return 0;
}

void signal_exception_queue_shutdown(void)
{
    if (!g_initialized) {
        return;
    }

    /* Process any remaining entries before shutdown */
    signal_exception_queue_process(0);

    /* Destroy the queue */
    if (g_signal_queue) {
        nimcp_queue_destroy(g_signal_queue);
        g_signal_queue = NULL;
    }

    g_callback = NULL;
    g_callback_user_data = NULL;
    g_initialized = false;

    LOG_INFO("Signal exception queue shutdown");
}

bool signal_exception_queue_is_initialized(void)
{
    return g_initialized;
}

bool signal_exception_queue_enqueue(int sig, const signal_crash_context_t* ctx)
{
    if (!g_initialized || !g_signal_queue || !ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "signal_exception_queue_enqueue: invalid parameters");

            return false;
    }

    /* Prepare the entry */
    signal_exception_entry_t entry;
    memcpy(&entry.ctx, ctx, sizeof(signal_crash_context_t));
    entry.ctx.signal = sig;  /* Ensure signal number is set */
    entry.timestamp_us = get_timestamp_us_signal_safe();

    /* Use try_enqueue (non-blocking, lock-free for SPSC) */
    bool success = nimcp_queue_try_enqueue(g_signal_queue, &entry);

    if (success) {
        __atomic_add_fetch(&g_enqueue_count, 1, __ATOMIC_RELAXED);
    } else {
        __atomic_add_fetch(&g_overflow_count, 1, __ATOMIC_RELAXED);
    }

    return success;
}

bool signal_exception_queue_dequeue(signal_exception_entry_t* out)
{
    if (!g_initialized || !g_signal_queue || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "signal_exception_queue_dequeue: invalid parameters");

            return false;
    }

    /* Use try_dequeue (non-blocking, lock-free for SPSC) */
    bool success = nimcp_queue_try_dequeue(g_signal_queue, out);

    if (success) {
        __atomic_add_fetch(&g_dequeue_count, 1, __ATOMIC_RELAXED);
    }

    return success;
}

size_t signal_exception_queue_pending_count(void)
{
    if (!g_initialized || !g_signal_queue) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "signal_exception_queue_pending_count: invalid parameters");

            return 0;
    }
    return nimcp_queue_get_size(g_signal_queue);
}

bool signal_exception_queue_is_empty(void)
{
    if (!g_initialized || !g_signal_queue) {
        return true;
    }
    return nimcp_queue_is_empty(g_signal_queue);
}

bool signal_exception_queue_is_full(void)
{
    if (!g_initialized || !g_signal_queue) {
        return true;
    }
    return nimcp_queue_is_full(g_signal_queue);
}

void signal_exception_queue_get_stats(signal_exception_queue_stats_t* stats)
{
    if (!stats) return;

    stats->enqueue_count = g_enqueue_count;
    stats->dequeue_count = g_dequeue_count;
    stats->overflow_count = g_overflow_count;
    stats->pending_count = signal_exception_queue_pending_count();
    stats->queue_capacity = SIGNAL_EXCEPTION_QUEUE_SIZE;
}

void signal_exception_queue_reset_stats(void)
{
    g_enqueue_count = 0;
    g_dequeue_count = 0;
    g_overflow_count = 0;
}

/* ============================================================================
 * Processing API Implementation
 * ============================================================================ */

size_t signal_exception_queue_process(size_t max_count)
{
    if (!g_initialized || !g_signal_queue) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "signal_exception_queue_process: invalid parameters");

            return 0;
    }

    size_t processed = 0;
    signal_exception_entry_t entry;

    while ((max_count == 0 || processed < max_count) &&
           signal_exception_queue_dequeue(&entry)) {

        /* Call custom callback if registered */
        if (g_callback) {
            g_callback(&entry, g_callback_user_data);
        }

        /* Create signal exception from crash context */
        nimcp_signal_exception_t* ex = nimcp_signal_exception_create_from_context(&entry.ctx);
        if (ex) {
            /* Present to immune system */
            nimcp_exception_present_to_immune((nimcp_exception_t*)ex, NULL);

            /* Dispatch through handler chain */
            nimcp_exception_dispatch((nimcp_exception_t*)ex);

            /* Release our reference */
            nimcp_exception_unref((nimcp_exception_t*)ex);

            LOG_DEBUG("Processed signal exception: sig=%d, fault_addr=%p",
                      entry.ctx.signal, entry.ctx.fault_address);
        } else {
            LOG_WARNING("Failed to create signal exception from crash context");
        }

        processed++;
    }

    return processed;
}

void signal_exception_queue_set_callback(
    signal_exception_callback_t callback,
    void* user_data
)
{
    g_callback = callback;
    g_callback_user_data = user_data;
}
