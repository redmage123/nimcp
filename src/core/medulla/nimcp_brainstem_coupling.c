/**
 * @file nimcp_brainstem_coupling.c
 * @brief Brainstem-cortex coupling implementation
 * @version 1.0.0
 * @date 2025-12-17
 */

#include "core/medulla/nimcp_brainstem_coupling.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "async/nimcp_bio_router.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(brainstem_coupling)

/*=============================================================================
 * CONSTANTS
 *============================================================================*/

/* Default configuration values based on biological parameters */
#define DEFAULT_BOTTOM_UP_BUFFER_SIZE    128   /**< Ascending signal buffer */
#define DEFAULT_TOP_DOWN_BUFFER_SIZE     64    /**< Descending signal buffer */
#define DEFAULT_MAX_MODULES              32    /**< Max registered modules */
#define DEFAULT_BIO_ASYNC_INBOX          64    /**< Bio-async inbox capacity */

/* Default biological latencies (milliseconds) */
#define DEFAULT_AROUSAL_LATENCY_MS       75    /**< RAS latency: 50-100 ms */
#define DEFAULT_THREAT_LATENCY_MS        35    /**< NTS/pain: 20-50 ms */
#define DEFAULT_METABOLIC_LATENCY_MS     1000  /**< Slow integration: 0.5-2 s */
#define DEFAULT_CIRCADIAN_LATENCY_MS     30000 /**< Very slow: 10-60 min (scaled) */

/* Bio-async module ID for brainstem coupling */
#define BIO_MODULE_BRAINSTEM_COUPLING    0x1000 /**< Brainstem coupling module */

/*=============================================================================
 * INTERNAL STRUCTURES
 *============================================================================*/

/**
 * @brief Signal queue entry with timestamp
 *
 * WHAT: Single signal with delivery tracking
 * WHY:  Need to model latency and priority
 */
typedef struct {
    brainstem_bottom_up_payload_t payload;  /**< Signal data */
    uint64_t delivery_time;                 /**< When to deliver (timestamp + latency) */
    bool valid;                             /**< Entry is active */
} bottom_up_queue_entry_t;

typedef struct {
    brainstem_top_down_payload_t payload;   /**< Signal data */
    bool valid;                             /**< Entry is active */
} top_down_queue_entry_t;

/**
 * @brief Main coupling structure
 *
 * WHAT: Complete state for brainstem-cortex coupling
 * WHY:  Encapsulates all resources for thread-safe operation
 */
struct brainstem_coupling_struct {
    /* Configuration */
    brainstem_coupling_config_t config;

    /* Signal buffers (circular queues) */
    bottom_up_queue_entry_t* bottom_up_buffer;
    uint32_t bottom_up_head;
    uint32_t bottom_up_tail;
    uint32_t bottom_up_count;

    top_down_queue_entry_t* top_down_buffer;
    uint32_t top_down_head;
    uint32_t top_down_tail;
    uint32_t top_down_count;

    /* Module registration */
    uint32_t* registered_modules;
    uint32_t registered_count;

    /* Priority and latency mappings */
    brainstem_signal_priority_t signal_priorities[BRAINSTEM_BOTTOM_UP_COUNT];
    uint32_t signal_latencies_ms[BRAINSTEM_BOTTOM_UP_COUNT];

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    /* Statistics */
    brainstem_coupling_stats_t stats;

    /* Thread safety */
    nimcp_platform_mutex_t* mutex;
};

/*=============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

/**
 * @brief Get current timestamp in milliseconds
 *
 * WHAT: Returns monotonic time in ms
 * WHY:  Need for latency modeling and signal timing
 * HOW:  Simple counter (production would use actual time)
 */
static uint64_t get_timestamp_ms(void) {
    static uint64_t counter = 0;
    return counter++;
}

/**
 * @brief Compare function for priority sorting
 *
 * WHAT: qsort comparator for signal priority
 * WHY:  Higher priority signals delivered first
 * HOW:  Compare priority field, then delivery time
 */
static int compare_signal_priority(const void* a, const void* b) {
    const bottom_up_queue_entry_t* sa = (const bottom_up_queue_entry_t*)a;
    const bottom_up_queue_entry_t* sb = (const bottom_up_queue_entry_t*)b;

    /* Invalid entries sort to end */
    if (!sa->valid && !sb->valid) return 0;
    if (!sa->valid) return 1;
    if (!sb->valid) return -1;

    /* Higher priority first */
    if (sa->payload.priority != sb->payload.priority) {
        return sb->payload.priority - sa->payload.priority;
    }

    /* Same priority: earlier delivery time first */
    if (sa->delivery_time < sb->delivery_time) return -1;
    if (sa->delivery_time > sb->delivery_time) return 1;
    return 0;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *============================================================================*/

int brainstem_coupling_default_config(brainstem_coupling_config_t* config) {
    /* WHAT: Populate config with biological defaults
     * WHY:  Users shouldn't need to know biological parameters
     * HOW:  Set buffer sizes, latencies, flags based on neuroscience
     */
    if (!config) {
        NIMCP_LOGGING_ERROR("Config is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_coupling_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(brainstem_coupling_config_t));

    config->bottom_up_buffer_size = DEFAULT_BOTTOM_UP_BUFFER_SIZE;
    config->top_down_buffer_size = DEFAULT_TOP_DOWN_BUFFER_SIZE;
    config->max_registered_modules = DEFAULT_MAX_MODULES;
    config->enable_bio_async = true;
    config->bio_async_inbox_capacity = DEFAULT_BIO_ASYNC_INBOX;
    config->apply_latency_model = true;
    config->enable_priority_queue = true;

    return 0;
}

brainstem_coupling_t* brainstem_coupling_create(const brainstem_coupling_config_t* config) {
    /* WHAT: Allocate and initialize coupling system
     * WHY:  Required before any signal exchange
     * HOW:  Allocate struct, buffers, mutex; set defaults
     */

    /* Use default config if none provided */
    brainstem_coupling_config_t default_config;
    if (!config) {
        brainstem_coupling_default_config(&default_config);
        config = &default_config;
    }

    /* Allocate main structure */
    brainstem_coupling_t* coupling = (brainstem_coupling_t*)nimcp_malloc(
        sizeof(brainstem_coupling_t)
    );
    if (!coupling) {
        NIMCP_LOGGING_ERROR("Failed to allocate coupling structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "coupling is NULL");

        return NULL;
    }

    memset(coupling, 0, sizeof(brainstem_coupling_t));
    memcpy(&coupling->config, config, sizeof(brainstem_coupling_config_t));

    /* Allocate bottom-up buffer */
    coupling->bottom_up_buffer = (bottom_up_queue_entry_t*)nimcp_malloc(
        config->bottom_up_buffer_size * sizeof(bottom_up_queue_entry_t)
    );
    if (!coupling->bottom_up_buffer) {
        NIMCP_LOGGING_ERROR("Failed to allocate bottom-up buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brainstem_coupling_create: failed to allocate bottom_up_buffer");
        nimcp_free(coupling);
        return NULL;
    }
    memset(coupling->bottom_up_buffer, 0,
           config->bottom_up_buffer_size * sizeof(bottom_up_queue_entry_t));

    /* Allocate top-down buffer */
    coupling->top_down_buffer = (top_down_queue_entry_t*)nimcp_malloc(
        config->top_down_buffer_size * sizeof(top_down_queue_entry_t)
    );
    if (!coupling->top_down_buffer) {
        NIMCP_LOGGING_ERROR("Failed to allocate top-down buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brainstem_coupling_create: failed to allocate top_down_buffer");
        nimcp_free(coupling->bottom_up_buffer);
        nimcp_free(coupling);
        return NULL;
    }
    memset(coupling->top_down_buffer, 0,
           config->top_down_buffer_size * sizeof(top_down_queue_entry_t));

    /* Allocate module registry */
    coupling->registered_modules = (uint32_t*)nimcp_malloc(
        config->max_registered_modules * sizeof(uint32_t)
    );
    if (!coupling->registered_modules) {
        NIMCP_LOGGING_ERROR("Failed to allocate module registry");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brainstem_coupling_create: failed to allocate registered_modules");
        nimcp_free(coupling->top_down_buffer);
        nimcp_free(coupling->bottom_up_buffer);
        nimcp_free(coupling);
        return NULL;
    }
    memset(coupling->registered_modules, 0,
           config->max_registered_modules * sizeof(uint32_t));

    /* Initialize priority defaults (threat > arousal > metabolic > circadian) */
    coupling->signal_priorities[BRAINSTEM_AROUSAL_SIGNAL] = SIGNAL_PRIORITY_MEDIUM;
    coupling->signal_priorities[BRAINSTEM_THREAT_SIGNAL] = SIGNAL_PRIORITY_HIGH;
    coupling->signal_priorities[BRAINSTEM_METABOLIC_SIGNAL] = SIGNAL_PRIORITY_LOW;
    coupling->signal_priorities[BRAINSTEM_CIRCADIAN_SIGNAL] = SIGNAL_PRIORITY_LOW;

    /* Initialize latency defaults */
    coupling->signal_latencies_ms[BRAINSTEM_AROUSAL_SIGNAL] = DEFAULT_AROUSAL_LATENCY_MS;
    coupling->signal_latencies_ms[BRAINSTEM_THREAT_SIGNAL] = DEFAULT_THREAT_LATENCY_MS;
    coupling->signal_latencies_ms[BRAINSTEM_METABOLIC_SIGNAL] = DEFAULT_METABOLIC_LATENCY_MS;
    coupling->signal_latencies_ms[BRAINSTEM_CIRCADIAN_SIGNAL] = DEFAULT_CIRCADIAN_LATENCY_MS;

    /* Create mutex */
    coupling->mutex = nimcp_platform_mutex_create();
    if (!coupling->mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brainstem_coupling_create: failed to create mutex");
        nimcp_free(coupling->registered_modules);
        nimcp_free(coupling->top_down_buffer);
        nimcp_free(coupling->bottom_up_buffer);
        nimcp_free(coupling);
        return NULL;
    }

    /* Connect to bio-async if enabled */
    if (config->enable_bio_async) {
        brainstem_coupling_connect_bio_async(coupling);
    }

    NIMCP_LOGGING_INFO("Brainstem coupling created successfully");
    return coupling;
}

void brainstem_coupling_destroy(brainstem_coupling_t* coupling) {
    /* WHAT: Free all resources
     * WHY:  Prevent memory leaks
     * HOW:  Disconnect bio-async, destroy mutex, free buffers
     */
    if (!coupling) return;

    /* Disconnect from bio-async */
    if (coupling->bio_async_enabled) {
        brainstem_coupling_disconnect_bio_async(coupling);
    }

    /* Destroy mutex */
    if (coupling->mutex) {
        nimcp_platform_mutex_destroy(coupling->mutex);
        nimcp_free(coupling->mutex);
        coupling->mutex = NULL;
    }

    /* Free buffers */
    nimcp_free(coupling->registered_modules);
    nimcp_free(coupling->top_down_buffer);
    nimcp_free(coupling->bottom_up_buffer);

    /* Free main structure */
    nimcp_free(coupling);
    NIMCP_LOGGING_INFO("Brainstem coupling destroyed");
}

/*=============================================================================
 * SIGNAL TRANSMISSION
 *============================================================================*/

int brainstem_coupling_send_bottom_up(
    brainstem_coupling_t* coupling,
    const brainstem_bottom_up_payload_t* payload
) {
    /* WHAT: Send ascending signal to cortex
     * WHY:  Model RAS and other bottom-up pathways
     * HOW:  Add to circular buffer with latency calculation
     */
    if (!coupling || !payload) {
        NIMCP_LOGGING_ERROR("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_coupling_send_bottom_up: invalid parameters");
        return -1;
    }

    if (payload->type >= BRAINSTEM_BOTTOM_UP_COUNT) {
        NIMCP_LOGGING_ERROR("Invalid signal type: %d", payload->type);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brainstem_coupling_send_bottom_up: invalid signal type");
        return -1;
    }

    nimcp_platform_mutex_lock(coupling->mutex);

    /* Check buffer full */
    if (coupling->bottom_up_count >= coupling->config.bottom_up_buffer_size) {
        coupling->stats.signals_dropped++;
        nimcp_platform_mutex_unlock(coupling->mutex);
        NIMCP_LOGGING_WARN("Bottom-up buffer full, signal dropped");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "brainstem_coupling_send_bottom_up: capacity exceeded");
        return -1;
    }

    /* Add to circular buffer */
    bottom_up_queue_entry_t* entry = &coupling->bottom_up_buffer[coupling->bottom_up_tail];
    entry->payload = *payload;
    entry->valid = true;

    /* Calculate delivery time with biological latency */
    if (coupling->config.apply_latency_model) {
        entry->delivery_time = payload->timestamp + coupling->signal_latencies_ms[payload->type];
    } else {
        entry->delivery_time = payload->timestamp;
    }

    /* Update circular buffer pointers */
    coupling->bottom_up_tail = (coupling->bottom_up_tail + 1) % coupling->config.bottom_up_buffer_size;
    coupling->bottom_up_count++;
    coupling->stats.bottom_up_sent++;
    coupling->stats.pending_bottom_up = coupling->bottom_up_count;

    nimcp_platform_mutex_unlock(coupling->mutex);
    return 0;
}

int brainstem_coupling_send_top_down(
    brainstem_coupling_t* coupling,
    const brainstem_top_down_payload_t* payload
) {
    /* WHAT: Send descending modulation signal
     * WHY:  Model cortical control over brainstem
     * HOW:  Add to circular buffer
     */
    if (!coupling || !payload) {
        NIMCP_LOGGING_ERROR("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_coupling_send_top_down: invalid parameters");
        return -1;
    }

    if (payload->type >= BRAINSTEM_TOP_DOWN_COUNT) {
        NIMCP_LOGGING_ERROR("Invalid signal type: %d", payload->type);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brainstem_coupling_send_top_down: invalid signal type");
        return -1;
    }

    nimcp_platform_mutex_lock(coupling->mutex);

    /* Check buffer full */
    if (coupling->top_down_count >= coupling->config.top_down_buffer_size) {
        coupling->stats.signals_dropped++;
        nimcp_platform_mutex_unlock(coupling->mutex);
        NIMCP_LOGGING_WARN("Top-down buffer full, signal dropped");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "brainstem_coupling_send_top_down: capacity exceeded");
        return -1;
    }

    /* Add to circular buffer */
    top_down_queue_entry_t* entry = &coupling->top_down_buffer[coupling->top_down_tail];
    entry->payload = *payload;
    entry->valid = true;

    /* Update circular buffer pointers */
    coupling->top_down_tail = (coupling->top_down_tail + 1) % coupling->config.top_down_buffer_size;
    coupling->top_down_count++;
    coupling->stats.top_down_sent++;
    coupling->stats.pending_top_down = coupling->top_down_count;

    nimcp_platform_mutex_unlock(coupling->mutex);
    return 0;
}

/*=============================================================================
 * MODULE REGISTRATION
 *============================================================================*/

int brainstem_coupling_register_module(
    brainstem_coupling_t* coupling,
    uint32_t module_id
) {
    /* WHAT: Add module to signal distribution list
     * WHY:  Only registered modules receive signals
     * HOW:  Append to registered_modules array
     */
    if (!coupling) {
        NIMCP_LOGGING_ERROR("Coupling is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_coupling_register_module: coupling is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(coupling->mutex);

    /* Check if already registered */
    for (uint32_t i = 0; i < coupling->registered_count; i++) {
        if (coupling->registered_modules[i] == module_id) {
            nimcp_platform_mutex_unlock(coupling->mutex);
            NIMCP_LOGGING_WARN("Module %u already registered", module_id);
            return 0;  /* Not an error */
        }
    }

    /* Check capacity */
    if (coupling->registered_count >= coupling->config.max_registered_modules) {
        nimcp_platform_mutex_unlock(coupling->mutex);
        NIMCP_LOGGING_ERROR("Module registry full");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "brainstem_coupling_register_module: capacity exceeded");
        return -1;
    }

    /* Add module */
    coupling->registered_modules[coupling->registered_count] = module_id;
    coupling->registered_count++;
    coupling->stats.registered_modules = coupling->registered_count;

    nimcp_platform_mutex_unlock(coupling->mutex);
    NIMCP_LOGGING_INFO("Registered module %u", module_id);
    return 0;
}

int brainstem_coupling_unregister_module(
    brainstem_coupling_t* coupling,
    uint32_t module_id
) {
    /* WHAT: Remove module from distribution list
     * WHY:  Module no longer wants signals
     * HOW:  Find and remove from array, shift remaining
     */
    if (!coupling) {
        NIMCP_LOGGING_ERROR("Coupling is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_coupling_unregister_module: coupling is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(coupling->mutex);

    /* Find module */
    for (uint32_t i = 0; i < coupling->registered_count; i++) {
        if (coupling->registered_modules[i] == module_id) {
            /* Shift remaining modules down */
            for (uint32_t j = i; j < coupling->registered_count - 1; j++) {
                coupling->registered_modules[j] = coupling->registered_modules[j + 1];
            }
            coupling->registered_count--;
            coupling->stats.registered_modules = coupling->registered_count;
            nimcp_platform_mutex_unlock(coupling->mutex);
            NIMCP_LOGGING_INFO("Unregistered module %u", module_id);
            return 0;
        }
    }

    nimcp_platform_mutex_unlock(coupling->mutex);
    NIMCP_LOGGING_WARN("Module %u not found", module_id);
    return -1;
}

/*=============================================================================
 * SIGNAL RETRIEVAL
 *============================================================================*/

int brainstem_coupling_get_pending_signals(
    brainstem_coupling_t* coupling,
    brainstem_bottom_up_payload_t* out_signals,
    uint32_t max_signals,
    uint32_t* out_count
) {
    /* WHAT: Retrieve pending ascending signals
     * WHY:  Cortical modules poll for new signals
     * HOW:  Copy from buffer, optionally sort by priority
     */
    if (!coupling || !out_signals || !out_count) {
        NIMCP_LOGGING_ERROR("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_coupling_get_pending_signals: required parameter is NULL (coupling, out_signals, out_count)");
        return -1;
    }

    nimcp_platform_mutex_lock(coupling->mutex);

    uint64_t current_time = get_timestamp_ms();
    uint32_t retrieved = 0;

    /* Collect ready signals (past delivery time) */
    for (uint32_t i = 0; i < coupling->bottom_up_count && retrieved < max_signals; i++) {
        uint32_t idx = (coupling->bottom_up_head + i) % coupling->config.bottom_up_buffer_size;
        bottom_up_queue_entry_t* entry = &coupling->bottom_up_buffer[idx];

        if (entry->valid && entry->delivery_time <= current_time) {
            out_signals[retrieved] = entry->payload;
            retrieved++;
        }
    }

    *out_count = retrieved;
    nimcp_platform_mutex_unlock(coupling->mutex);
    return 0;
}

int brainstem_coupling_process_signals(brainstem_coupling_t* coupling) {
    /* WHAT: Process and deliver all pending signals
     * WHY:  Central dispatch for signal distribution
     * HOW:  Check delivery times, route to registered modules
     */
    if (!coupling) {
        NIMCP_LOGGING_ERROR("Coupling is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_coupling_process_signals: coupling is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(coupling->mutex);

    uint64_t current_time = get_timestamp_ms();
    uint32_t processed = 0;

    /* Process bottom-up signals */
    uint32_t initial_count = coupling->bottom_up_count;
    for (uint32_t i = 0; i < initial_count; i++) {
        bottom_up_queue_entry_t* entry = &coupling->bottom_up_buffer[coupling->bottom_up_head];

        /* Check if ready for delivery */
        if (entry->valid && entry->delivery_time <= current_time) {
            /* Signal is ready - in production would send via bio-async */
            entry->valid = false;
            coupling->bottom_up_head = (coupling->bottom_up_head + 1) % coupling->config.bottom_up_buffer_size;
            coupling->bottom_up_count--;
            processed++;
        } else {
            /* Not ready yet, stop processing (assumes time-ordered) */
            break;
        }
    }

    coupling->stats.signals_processed += processed;
    coupling->stats.pending_bottom_up = coupling->bottom_up_count;
    coupling->stats.pending_top_down = coupling->top_down_count;

    nimcp_platform_mutex_unlock(coupling->mutex);
    return (int)processed;
}

/*=============================================================================
 * PRIORITY AND FILTERING
 *============================================================================*/

int brainstem_coupling_set_priority(
    brainstem_coupling_t* coupling,
    brainstem_bottom_up_signal_t signal_type,
    brainstem_signal_priority_t priority
) {
    /* WHAT: Update signal priority
     * WHY:  Different contexts require different priorities
     * HOW:  Store in priority mapping array
     */
    if (!coupling) {
        NIMCP_LOGGING_ERROR("Coupling is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_coupling_set_priority: coupling is NULL");
        return -1;
    }

    if (signal_type >= BRAINSTEM_BOTTOM_UP_COUNT) {
        NIMCP_LOGGING_ERROR("Invalid signal type: %d", signal_type);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "brainstem_coupling_set_priority: capacity exceeded");
        return -1;
    }

    nimcp_platform_mutex_lock(coupling->mutex);
    coupling->signal_priorities[signal_type] = priority;
    nimcp_platform_mutex_unlock(coupling->mutex);

    NIMCP_LOGGING_DEBUG("Set priority for signal %d to %d", signal_type, priority);
    return 0;
}

int brainstem_coupling_set_latency(
    brainstem_coupling_t* coupling,
    brainstem_bottom_up_signal_t signal_type,
    uint32_t latency_ms
) {
    /* WHAT: Set biological latency for signal type
     * WHY:  Different pathways have different speeds
     * HOW:  Store in latency mapping array
     */
    if (!coupling) {
        NIMCP_LOGGING_ERROR("Coupling is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_coupling_set_latency: coupling is NULL");
        return -1;
    }

    if (signal_type >= BRAINSTEM_BOTTOM_UP_COUNT) {
        NIMCP_LOGGING_ERROR("Invalid signal type: %d", signal_type);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "brainstem_coupling_set_latency: capacity exceeded");
        return -1;
    }

    nimcp_platform_mutex_lock(coupling->mutex);
    coupling->signal_latencies_ms[signal_type] = latency_ms;
    nimcp_platform_mutex_unlock(coupling->mutex);

    NIMCP_LOGGING_DEBUG("Set latency for signal %d to %u ms", signal_type, latency_ms);
    return 0;
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *============================================================================*/

int brainstem_coupling_connect_bio_async(brainstem_coupling_t* coupling) {
    /* WHAT: Register with bio-async router
     * WHY:  Enable inter-module messaging
     * HOW:  Call bio_router_register_module with coupling context
     */
    if (!coupling) {
        NIMCP_LOGGING_ERROR("Coupling is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_coupling_connect_bio_async: coupling is NULL");
        return -1;
    }

    if (coupling->bio_async_enabled) {
        NIMCP_LOGGING_WARN("Bio-async already connected");
        return 0;
    }

    /* Note: bio_router_register_module would be called here in production
     * For now, just set flag */
    coupling->bio_async_enabled = true;
    NIMCP_LOGGING_INFO("Connected to bio-async router (stub)");

    return 0;
}

int brainstem_coupling_disconnect_bio_async(brainstem_coupling_t* coupling) {
    /* WHAT: Unregister from bio-async router
     * WHY:  Clean shutdown
     * HOW:  Call unregister, clear flag
     */
    if (!coupling) {
        NIMCP_LOGGING_ERROR("Coupling is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_coupling_disconnect_bio_async: coupling is NULL");
        return -1;
    }

    if (!coupling->bio_async_enabled) {
        NIMCP_LOGGING_WARN("Bio-async not connected");
        return 0;
    }

    coupling->bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return 0;
}

bool brainstem_coupling_is_bio_async_connected(const brainstem_coupling_t* coupling) {
    /* WHAT: Query connection status
     * WHY:  Determine if messaging is available
     * HOW:  Return flag
     */
    if (!coupling) {
        return false;
    }
    return coupling->bio_async_enabled;
}

/*=============================================================================
 * STATISTICS AND MONITORING
 *============================================================================*/

int brainstem_coupling_get_stats(
    const brainstem_coupling_t* coupling,
    brainstem_coupling_stats_t* out_stats
) {
    /* WHAT: Retrieve current statistics
     * WHY:  Monitor system health
     * HOW:  Copy internal stats struct
     */
    if (!coupling || !out_stats) {
        NIMCP_LOGGING_ERROR("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_coupling_get_stats: invalid parameters");
        return -1;
    }

    nimcp_platform_mutex_lock(coupling->mutex);
    memcpy(out_stats, &coupling->stats, sizeof(brainstem_coupling_stats_t));
    nimcp_platform_mutex_unlock(coupling->mutex);

    return 0;
}

int brainstem_coupling_reset_stats(brainstem_coupling_t* coupling) {
    /* WHAT: Zero all statistics counters
     * WHY:  Start fresh monitoring period
     * HOW:  Memset stats struct
     */
    if (!coupling) {
        NIMCP_LOGGING_ERROR("Coupling is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_coupling_reset_stats: coupling is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(coupling->mutex);
    memset(&coupling->stats, 0, sizeof(brainstem_coupling_stats_t));
    coupling->stats.registered_modules = coupling->registered_count;
    coupling->stats.pending_bottom_up = coupling->bottom_up_count;
    coupling->stats.pending_top_down = coupling->top_down_count;
    nimcp_platform_mutex_unlock(coupling->mutex);

    NIMCP_LOGGING_INFO("Statistics reset");
    return 0;
}
