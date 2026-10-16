/**
 * @file nimcp_exception_circuit.c
 * @brief Circuit breaker pattern and exception suppression implementation
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: Implementation of circuit breaker and suppression for exceptions
 * WHY:  Prevent cascading failures and allow maintenance windows
 * HOW:  Thread-safe tracking with mutex protection, time-windowed counters
 *
 * @author NIMCP Development Team
 */

#include "utils/exception/nimcp_exception_circuit.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(exception_circuit)

/* ============================================================================
 * Module State
 * ============================================================================ */

static bool g_circuit_initialized = false;
static nimcp_platform_mutex_t* g_circuit_mutex = NULL;

/* Circuit breaker tracking array */
static nimcp_exception_circuit_t g_circuits[NIMCP_CIRCUIT_MAX_TRACKED];
static size_t g_circuit_count = 0;

/* Suppression tracking array */
static nimcp_suppression_entry_t g_suppressions[NIMCP_SUPPRESSION_MAX_ENTRIES];
static size_t g_suppression_count = 0;

/* Global statistics */
static uint64_t g_total_exceptions = 0;
static uint64_t g_total_blocked = 0;
static uint64_t g_total_suppressed = 0;

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Get current timestamp in microseconds
 */
static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Find or create circuit entry for error code (unlocked)
 */
static nimcp_exception_circuit_t* find_or_create_circuit_unlocked(nimcp_error_t code) {
    /* First, find existing entry */
    for (size_t i = 0; i < g_circuit_count; i++) {
        if (g_circuits[i].active && g_circuits[i].code == code) {
            return &g_circuits[i];
        }
    }

    /* Look for inactive slot or create new */
    for (size_t i = 0; i < NIMCP_CIRCUIT_MAX_TRACKED; i++) {
        if (!g_circuits[i].active) {
            memset(&g_circuits[i], 0, sizeof(nimcp_exception_circuit_t));
            g_circuits[i].code = code;
            g_circuits[i].category = nimcp_exception_get_category_from_code(code);
            g_circuits[i].state = CIRCUIT_STATE_CLOSED;
            g_circuits[i].trip_threshold = NIMCP_CIRCUIT_DEFAULT_THRESHOLD;
            g_circuits[i].reset_timeout_ms = NIMCP_CIRCUIT_DEFAULT_RESET_MS;
            g_circuits[i].last_window_reset_us = get_timestamp_us();
            g_circuits[i].active = true;

            if (i >= g_circuit_count) {
                g_circuit_count = i + 1;
            }
            return &g_circuits[i];
        }
    }

    return NULL; /* No room */
}

/**
 * @brief Find circuit entry for error code (unlocked)
 */
static nimcp_exception_circuit_t* find_circuit_unlocked(nimcp_error_t code) {
    for (size_t i = 0; i < g_circuit_count; i++) {
        if (g_circuits[i].active && g_circuits[i].code == code) {
            return &g_circuits[i];
        }
    }
    return NULL;
}

/**
 * @brief Find suppression entry for error code (unlocked)
 */
static nimcp_suppression_entry_t* find_suppression_unlocked(nimcp_error_t code) {
    for (size_t i = 0; i < g_suppression_count; i++) {
        if (g_suppressions[i].active && g_suppressions[i].code == code) {
            return &g_suppressions[i];
        }
    }
    return NULL;
}

/**
 * @brief Update window counters based on time elapsed (unlocked)
 */
static void update_window_counters_unlocked(nimcp_exception_circuit_t* circuit) {
    if (!circuit) return;

    uint64_t now = get_timestamp_us();
    uint64_t elapsed_us = now - circuit->last_window_reset_us;

    /* Reset 1-minute counter if > 60 seconds */
    if (elapsed_us >= 60000000ULL) {
        circuit->count_1min = 0;
        circuit->last_window_reset_us = now;
    }

    /* Decay 5-minute counter (simple approach: halve if > 150s since last) */
    if (elapsed_us >= 150000000ULL) {
        circuit->count_5min = circuit->count_5min / 2;
    }
}

/**
 * @brief Check and update circuit state (unlocked)
 */
static void update_circuit_state_unlocked(nimcp_exception_circuit_t* circuit) {
    if (!circuit) return;

    uint64_t now = get_timestamp_us();

    switch (circuit->state) {
        case CIRCUIT_STATE_CLOSED:
            /* Check if threshold exceeded */
            if (circuit->count_1min >= circuit->trip_threshold) {
                circuit->state = CIRCUIT_STATE_OPEN;
                circuit->circuit_open_until_us = now +
                    (uint64_t)circuit->reset_timeout_ms * 1000ULL;
                circuit->half_open_attempts = 0;
                circuit->half_open_successes = 0;

                LOG_WARNING("[Circuit] Circuit OPENED for error %d: %u exceptions in 1min "
                           "(threshold: %u)",
                           circuit->code, circuit->count_1min, circuit->trip_threshold);
            }
            break;

        case CIRCUIT_STATE_OPEN:
            /* Check if reset timeout elapsed */
            if (now >= circuit->circuit_open_until_us) {
                circuit->state = CIRCUIT_STATE_HALF_OPEN;
                circuit->half_open_attempts = 0;
                circuit->half_open_successes = 0;

                LOG_INFO("[Circuit] Circuit moved to HALF_OPEN for error %d", circuit->code);
            }
            break;

        case CIRCUIT_STATE_HALF_OPEN:
            /* If too many failures in half-open, revert to open */
            if (circuit->half_open_attempts >= NIMCP_CIRCUIT_HALF_OPEN_ALLOW) {
                if (circuit->half_open_successes < circuit->half_open_attempts / 2) {
                    /* Not enough successes, reopen */
                    circuit->state = CIRCUIT_STATE_OPEN;
                    circuit->circuit_open_until_us = now +
                        (uint64_t)circuit->reset_timeout_ms * 1000ULL;

                    LOG_WARNING("[Circuit] Circuit RE-OPENED for error %d: "
                               "%u attempts, %u successes",
                               circuit->code, circuit->half_open_attempts,
                               circuit->half_open_successes);
                } else {
                    /* Enough successes, close circuit */
                    circuit->state = CIRCUIT_STATE_CLOSED;
                    circuit->count_1min = 0;
                    circuit->count_5min = 0;

                    LOG_INFO("[Circuit] Circuit CLOSED for error %d: recovered", circuit->code);
                }
            }
            break;
    }
}

/* ============================================================================
 * Circuit Breaker API Implementation
 * ============================================================================ */

int nimcp_circuit_init(void) {
    if (g_circuit_initialized) {
        return 0;
    }

    g_circuit_mutex = nimcp_platform_mutex_create();
    if (!g_circuit_mutex) {
        return -1;
    }

    memset(g_circuits, 0, sizeof(g_circuits));
    memset(g_suppressions, 0, sizeof(g_suppressions));
    g_circuit_count = 0;
    g_suppression_count = 0;
    g_total_exceptions = 0;
    g_total_blocked = 0;
    g_total_suppressed = 0;

    g_circuit_initialized = true;
    return 0;
}

void nimcp_circuit_shutdown(void) {
    if (!g_circuit_initialized) {
        return;
    }

    if (g_circuit_mutex) {
        nimcp_platform_mutex_destroy(g_circuit_mutex);
        nimcp_free(g_circuit_mutex);
        g_circuit_mutex = NULL;
    }

    memset(g_circuits, 0, sizeof(g_circuits));
    memset(g_suppressions, 0, sizeof(g_suppressions));
    g_circuit_count = 0;
    g_suppression_count = 0;

    g_circuit_initialized = false;
}

bool nimcp_circuit_is_initialized(void) {
    return g_circuit_initialized;
}

int nimcp_circuit_record(nimcp_exception_t* ex) {
    if (!ex) return -1;
    if (!g_circuit_initialized) return 0;

    nimcp_platform_mutex_lock(g_circuit_mutex);

    nimcp_exception_circuit_t* circuit = find_or_create_circuit_unlocked(ex->code);
    if (!circuit) {
        nimcp_platform_mutex_unlock(g_circuit_mutex);
        return 0; /* No room, but don't block */
    }

    g_total_exceptions++;

    /* Update window counters based on time */
    update_window_counters_unlocked(circuit);

    /* Record this occurrence */
    circuit->count_1min++;
    circuit->count_5min++;
    circuit->count_total++;
    circuit->last_occurrence_us = get_timestamp_us();

    /* Track half-open attempts */
    if (circuit->state == CIRCUIT_STATE_HALF_OPEN) {
        circuit->half_open_attempts++;
    }

    /* Update circuit state */
    update_circuit_state_unlocked(circuit);

    /* Check if we should block */
    int result = 0;
    if (circuit->state == CIRCUIT_STATE_OPEN) {
        g_total_blocked++;
        result = 1; /* Blocked */
    }

    nimcp_platform_mutex_unlock(g_circuit_mutex);
    return result;
}

nimcp_circuit_state_t nimcp_circuit_get_state(nimcp_error_t code) {
    if (!g_circuit_initialized) {
        return CIRCUIT_STATE_CLOSED;
    }

    nimcp_platform_mutex_lock(g_circuit_mutex);

    nimcp_exception_circuit_t* circuit = find_circuit_unlocked(code);
    nimcp_circuit_state_t state = circuit ? circuit->state : CIRCUIT_STATE_CLOSED;

    /* Also update state based on timeouts */
    if (circuit) {
        update_circuit_state_unlocked(circuit);
        state = circuit->state;
    }

    nimcp_platform_mutex_unlock(g_circuit_mutex);
    return state;
}

bool nimcp_circuit_is_open(nimcp_error_t code) {
    nimcp_circuit_state_t state = nimcp_circuit_get_state(code);
    return (state == CIRCUIT_STATE_OPEN);
}

int nimcp_circuit_set_threshold(nimcp_error_t code, uint32_t threshold, uint32_t reset_ms) {
    if (!g_circuit_initialized) {
        return -1;
    }

    nimcp_platform_mutex_lock(g_circuit_mutex);

    nimcp_exception_circuit_t* circuit = find_or_create_circuit_unlocked(code);
    if (!circuit) {
        nimcp_platform_mutex_unlock(g_circuit_mutex);
        return -1;
    }

    if (threshold > 0) {
        circuit->trip_threshold = threshold;
    }
    if (reset_ms > 0) {
        circuit->reset_timeout_ms = reset_ms;
    }

    nimcp_platform_mutex_unlock(g_circuit_mutex);
    return 0;
}

int nimcp_circuit_reset(nimcp_error_t code) {
    if (!g_circuit_initialized) {
        return -1;
    }

    nimcp_platform_mutex_lock(g_circuit_mutex);

    nimcp_exception_circuit_t* circuit = find_circuit_unlocked(code);
    if (!circuit) {
        nimcp_platform_mutex_unlock(g_circuit_mutex);
        return -1;
    }

    circuit->state = CIRCUIT_STATE_CLOSED;
    circuit->count_1min = 0;
    circuit->count_5min = 0;
    circuit->half_open_attempts = 0;
    circuit->half_open_successes = 0;

    LOG_INFO("[Circuit] Circuit manually reset for error %d", code);

    nimcp_platform_mutex_unlock(g_circuit_mutex);
    return 0;
}

void nimcp_circuit_reset_all(void) {
    if (!g_circuit_initialized) {
        return;
    }

    nimcp_platform_mutex_lock(g_circuit_mutex);

    for (size_t i = 0; i < g_circuit_count; i++) {
        if (g_circuits[i].active) {
            g_circuits[i].state = CIRCUIT_STATE_CLOSED;
            g_circuits[i].count_1min = 0;
            g_circuits[i].count_5min = 0;
            g_circuits[i].half_open_attempts = 0;
            g_circuits[i].half_open_successes = 0;
        }
    }

    LOG_INFO("[Circuit] All circuits reset");

    nimcp_platform_mutex_unlock(g_circuit_mutex);
}

size_t nimcp_circuit_get_count(nimcp_error_t code, uint32_t window_seconds) {
    if (!g_circuit_initialized) {
        return 0;
    }

    nimcp_platform_mutex_lock(g_circuit_mutex);

    nimcp_exception_circuit_t* circuit = find_circuit_unlocked(code);
    size_t count = 0;

    if (circuit) {
        update_window_counters_unlocked(circuit);

        if (window_seconds == 0) {
            count = circuit->count_total;
        } else if (window_seconds <= 60) {
            count = circuit->count_1min;
        } else {
            count = circuit->count_5min;
        }
    }

    nimcp_platform_mutex_unlock(g_circuit_mutex);
    return count;
}

/**
 * P2-U32 WARNING: This returns a pointer into the static g_circuits[] array.
 * The pointer is stable (static lifetime), but the data may be modified
 * concurrently by other threads calling nimcp_circuit_record() or
 * nimcp_circuit_reset(). Callers should treat the returned data as a
 * best-effort snapshot. For a thread-safe snapshot, use nimcp_circuit_get_stats()
 * or nimcp_circuit_get_state() which return values, not pointers.
 */
const nimcp_exception_circuit_t* nimcp_circuit_get_entry(nimcp_error_t code) {
    if (!g_circuit_initialized) {
        return NULL;
    }

    nimcp_platform_mutex_lock(g_circuit_mutex);
    const nimcp_exception_circuit_t* circuit = find_circuit_unlocked(code);
    nimcp_platform_mutex_unlock(g_circuit_mutex);

    return circuit;
}

int nimcp_circuit_get_stats(nimcp_circuit_stats_t* stats) {
    if (!stats || !g_circuit_initialized) {
        return -1;
    }

    nimcp_platform_mutex_lock(g_circuit_mutex);

    memset(stats, 0, sizeof(nimcp_circuit_stats_t));
    stats->total_exceptions = g_total_exceptions;
    stats->total_blocked = g_total_blocked;
    stats->total_suppressed = g_total_suppressed;

    for (size_t i = 0; i < g_circuit_count; i++) {
        if (g_circuits[i].active) {
            stats->total_tracked++;
            if (g_circuits[i].state == CIRCUIT_STATE_OPEN) {
                stats->circuits_open++;
            } else if (g_circuits[i].state == CIRCUIT_STATE_HALF_OPEN) {
                stats->circuits_half_open++;
            }
        }
    }

    nimcp_platform_mutex_unlock(g_circuit_mutex);
    return 0;
}

int nimcp_circuit_report_success(nimcp_error_t code) {
    if (!g_circuit_initialized) {
        return -1;
    }

    nimcp_platform_mutex_lock(g_circuit_mutex);

    nimcp_exception_circuit_t* circuit = find_circuit_unlocked(code);
    if (!circuit) {
        nimcp_platform_mutex_unlock(g_circuit_mutex);
        return -1;
    }

    if (circuit->state == CIRCUIT_STATE_HALF_OPEN) {
        circuit->half_open_successes++;

        /* Check if enough successes to close circuit */
        if (circuit->half_open_successes >= NIMCP_CIRCUIT_HALF_OPEN_ALLOW) {
            circuit->state = CIRCUIT_STATE_CLOSED;
            circuit->count_1min = 0;
            circuit->count_5min = 0;

            LOG_INFO("[Circuit] Circuit CLOSED for error %d: enough successes (%u)",
                    code, circuit->half_open_successes);
        }
    }

    nimcp_platform_mutex_unlock(g_circuit_mutex);
    return 0;
}

/* ============================================================================
 * Suppression API Implementation
 * ============================================================================ */

int nimcp_exception_suppress(nimcp_error_t code, uint64_t duration_ms, const char* reason) {
    if (!g_circuit_initialized) {
        return -1;
    }

    nimcp_platform_mutex_lock(g_circuit_mutex);

    /* Check if already suppressed */
    nimcp_suppression_entry_t* entry = find_suppression_unlocked(code);
    if (entry) {
        /* Update existing entry */
        uint64_t now = get_timestamp_us();
        entry->suppress_until_us = (duration_ms > 0) ?
            now + duration_ms * 1000ULL : UINT64_MAX;
        entry->reason = reason;
        entry->created_us = now;

        nimcp_platform_mutex_unlock(g_circuit_mutex);
        return 0;
    }

    /* Find empty slot */
    for (size_t i = 0; i < NIMCP_SUPPRESSION_MAX_ENTRIES; i++) {
        if (!g_suppressions[i].active) {
            uint64_t now = get_timestamp_us();

            g_suppressions[i].code = code;
            g_suppressions[i].suppress_until_us = (duration_ms > 0) ?
                now + duration_ms * 1000ULL : UINT64_MAX;
            g_suppressions[i].reason = reason;
            g_suppressions[i].created_us = now;
            g_suppressions[i].suppressed_count = 0;
            g_suppressions[i].active = true;

            if (i >= g_suppression_count) {
                g_suppression_count = i + 1;
            }

            LOG_INFO("[Suppression] Exception %d suppressed: %s (duration: %lu ms)",
                    code, reason ? reason : "no reason", (unsigned long)duration_ms);

            nimcp_platform_mutex_unlock(g_circuit_mutex);
            return 0;
        }
    }

    nimcp_platform_mutex_unlock(g_circuit_mutex);
    return -1; /* No room */
}

int nimcp_exception_unsuppress(nimcp_error_t code) {
    if (!g_circuit_initialized) {
        return -1;
    }

    nimcp_platform_mutex_lock(g_circuit_mutex);

    nimcp_suppression_entry_t* entry = find_suppression_unlocked(code);
    if (!entry) {
        nimcp_platform_mutex_unlock(g_circuit_mutex);
        return -1;
    }

    LOG_INFO("[Suppression] Exception %d unsuppressed (suppressed %u occurrences)",
            code, entry->suppressed_count);

    entry->active = false;

    nimcp_platform_mutex_unlock(g_circuit_mutex);
    return 0;
}

bool nimcp_exception_is_suppressed(nimcp_error_t code) {
    if (!g_circuit_initialized) {
        return false;
    }

    nimcp_platform_mutex_lock(g_circuit_mutex);

    nimcp_suppression_entry_t* entry = find_suppression_unlocked(code);
    if (!entry) {
        nimcp_platform_mutex_unlock(g_circuit_mutex);
        return false;
    }

    uint64_t now = get_timestamp_us();

    /* Check expiration */
    if (now >= entry->suppress_until_us) {
        LOG_INFO("[Suppression] Exception %d suppression expired (suppressed %u occurrences)",
                code, entry->suppressed_count);
        entry->active = false;
        nimcp_platform_mutex_unlock(g_circuit_mutex);
        return false;
    }

    /* Count this suppression */
    entry->suppressed_count++;
    g_total_suppressed++;

    nimcp_platform_mutex_unlock(g_circuit_mutex);
    return true;
}

size_t nimcp_suppression_list_active(nimcp_error_t* codes, size_t max_codes) {
    if (!codes || max_codes == 0 || !g_circuit_initialized) {
        return 0;
    }

    nimcp_platform_mutex_lock(g_circuit_mutex);

    uint64_t now = get_timestamp_us();
    size_t count = 0;

    for (size_t i = 0; i < g_suppression_count && count < max_codes; i++) {
        if (g_suppressions[i].active) {
            /* Check expiration while listing */
            if (now >= g_suppressions[i].suppress_until_us) {
                g_suppressions[i].active = false;
                continue;
            }
            codes[count++] = g_suppressions[i].code;
        }
    }

    nimcp_platform_mutex_unlock(g_circuit_mutex);
    return count;
}

void nimcp_suppression_clear_all(void) {
    if (!g_circuit_initialized) {
        return;
    }

    nimcp_platform_mutex_lock(g_circuit_mutex);

    for (size_t i = 0; i < g_suppression_count; i++) {
        g_suppressions[i].active = false;
    }
    g_suppression_count = 0;

    LOG_INFO("[Suppression] All suppressions cleared");

    nimcp_platform_mutex_unlock(g_circuit_mutex);
}

void nimcp_suppression_clear_expired(void) {
    if (!g_circuit_initialized) {
        return;
    }

    nimcp_platform_mutex_lock(g_circuit_mutex);

    uint64_t now = get_timestamp_us();
    size_t cleared = 0;

    for (size_t i = 0; i < g_suppression_count; i++) {
        if (g_suppressions[i].active && now >= g_suppressions[i].suppress_until_us) {
            g_suppressions[i].active = false;
            cleared++;
        }
    }

    if (cleared > 0) {
        LOG_DEBUG("[Suppression] Cleared %zu expired suppressions", cleared);
    }

    nimcp_platform_mutex_unlock(g_circuit_mutex);
}

/**
 * P2-U33 WARNING: Same thread-safety caveat as nimcp_circuit_get_entry (P2-U32).
 * Returns a pointer into the static g_suppressions[] array. The pointer is stable
 * but data may change concurrently. For a thread-safe check, use
 * nimcp_exception_is_suppressed() which operates entirely under the mutex.
 */
const nimcp_suppression_entry_t* nimcp_suppression_get_entry(nimcp_error_t code) {
    if (!g_circuit_initialized) {
        return NULL;
    }

    nimcp_platform_mutex_lock(g_circuit_mutex);
    const nimcp_suppression_entry_t* entry = find_suppression_unlocked(code);
    nimcp_platform_mutex_unlock(g_circuit_mutex);

    return entry;
}

/* ============================================================================
 * String Conversion
 * ============================================================================ */

const char* nimcp_circuit_state_to_string(nimcp_circuit_state_t state) {
    switch (state) {
        case CIRCUIT_STATE_CLOSED:    return "CLOSED";
        case CIRCUIT_STATE_OPEN:      return "OPEN";
        case CIRCUIT_STATE_HALF_OPEN: return "HALF_OPEN";
        default:                      return "UNKNOWN";
    }
}

/* ============================================================================
 * Integration Functions
 * ============================================================================ */

bool nimcp_exception_should_process(nimcp_exception_t* ex) {
    if (!ex) return false;
    if (!g_circuit_initialized) return true;

    /* Check suppression first */
    if (nimcp_exception_is_suppressed(ex->code)) {
        return false;
    }

    /* Record and check circuit breaker */
    int result = nimcp_circuit_record(ex);
    if (result == 1) {
        /* Circuit is open, block exception processing */
        return false;
    }

    return true;
}

void nimcp_circuit_maintenance(void) {
    if (!g_circuit_initialized) {
        return;
    }

    nimcp_platform_mutex_lock(g_circuit_mutex);

    uint64_t now = get_timestamp_us();

    /* Update all circuit window counters */
    for (size_t i = 0; i < g_circuit_count; i++) {
        if (g_circuits[i].active) {
            update_window_counters_unlocked(&g_circuits[i]);
            update_circuit_state_unlocked(&g_circuits[i]);
        }
    }

    /* Clear expired suppressions */
    for (size_t i = 0; i < g_suppression_count; i++) {
        if (g_suppressions[i].active && now >= g_suppressions[i].suppress_until_us) {
            LOG_DEBUG("[Suppression] Auto-expired suppression for error %d",
                     g_suppressions[i].code);
            g_suppressions[i].active = false;
        }
    }

    nimcp_platform_mutex_unlock(g_circuit_mutex);
}
