/**
 * @file nimcp_protective_cutoff.c
 * @brief Multi-Tier Emergency Protective Shutdown System Implementation
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Implementation of progressive protective cutoff system
 * WHY:  Prevent catastrophic failure through graded protective responses
 * HOW:  Multi-dimensional threat assessment with hysteresis-based recovery
 */

#include <stddef.h>  /* for NULL */
//=============================================================================
// Required Headers
//=============================================================================

#include "core/medulla/nimcp_protective_cutoff.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "async/nimcp_bio_router.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(protective_cutoff)

//=============================================================================
// Constants
//=============================================================================

#define MAX_CALLBACKS 16  /**< Maximum number of level transition callbacks */

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * WHAT: Callback registration entry
 * WHY:  Track registered callbacks for level transitions
 */
typedef struct {
    protection_level_callback_t callback; /**< Callback function */
    void* user_data;                      /**< User-provided context */
} callback_entry_t;

/**
 * WHAT: Protective cutoff system context
 * WHY:  Encapsulate all state for multi-tier protection
 * HOW:  Tracks threats, thresholds, current level, callbacks
 */
struct protective_cutoff_s {
    // Configuration
    protective_cutoff_config_t config;    /**< Configuration parameters */

    // Current state
    protection_level_t current_level;     /**< Current protection level */
    float threat_severities[PROTECTIVE_THREAT_COUNT]; /**< Severity for each threat type */
    float combined_threat;                /**< Weighted combination of threats */
    uint64_t last_update_time;            /**< Last update timestamp (ms) */
    bool manual_override;                 /**< Manual level override active */

    // Capabilities matrix (rows=levels, cols=operations)
    bool capabilities[6][OP_COUNT];       /**< What ops are allowed at each level */

    // Callbacks
    callback_entry_t callbacks[MAX_CALLBACKS]; /**< Level transition callbacks */
    uint32_t callback_count;              /**< Number of registered callbacks */

    // Bio-async integration
    bio_module_context_t bio_ctx;         /**< Bio-async module context */
    bool bio_async_enabled;               /**< Bio-async connection active */

    // Thread safety
    nimcp_platform_mutex_t mutex;         /**< Protects all fields */
};

//=============================================================================
// Capability Matrix Initialization
//=============================================================================

/**
 * WHAT: Initialize capability matrix (which ops are allowed at each level)
 * WHY:  Define fine-grained operational constraints per protection level
 * HOW:  Sets boolean matrix based on biological protective reflex model
 *
 * BIOLOGICAL BASIS: Models progressive capability restriction in brainstem
 * reflexes (e.g., pain withdrawal progressively restricts motor control)
 *
 * @param cutoff Context to initialize (must not be NULL)
 */
static void init_capabilities(protective_cutoff_t* cutoff) {
    // Clear all capabilities
    memset(cutoff->capabilities, 0, sizeof(cutoff->capabilities));

    // PROTECTION_NORMAL (0): All operations allowed
    cutoff->capabilities[PROTECTION_NORMAL][OP_LEARNING] = true;
    cutoff->capabilities[PROTECTION_NORMAL][OP_INFERENCE] = true;
    cutoff->capabilities[PROTECTION_NORMAL][OP_ADAPTATION] = true;
    cutoff->capabilities[PROTECTION_NORMAL][OP_NETWORK_TX] = true;
    cutoff->capabilities[PROTECTION_NORMAL][OP_NETWORK_RX] = true;
    cutoff->capabilities[PROTECTION_NORMAL][OP_MEMORY_ALLOC] = true;
    cutoff->capabilities[PROTECTION_NORMAL][OP_FILE_IO] = true;
    cutoff->capabilities[PROTECTION_NORMAL][OP_CHECKPOINT] = true;

    // PROTECTION_WARN (1): All still allowed, just monitoring
    cutoff->capabilities[PROTECTION_WARN][OP_LEARNING] = true;
    cutoff->capabilities[PROTECTION_WARN][OP_INFERENCE] = true;
    cutoff->capabilities[PROTECTION_WARN][OP_ADAPTATION] = true;
    cutoff->capabilities[PROTECTION_WARN][OP_NETWORK_TX] = true;
    cutoff->capabilities[PROTECTION_WARN][OP_NETWORK_RX] = true;
    cutoff->capabilities[PROTECTION_WARN][OP_MEMORY_ALLOC] = true;
    cutoff->capabilities[PROTECTION_WARN][OP_FILE_IO] = true;
    cutoff->capabilities[PROTECTION_WARN][OP_CHECKPOINT] = true;

    // PROTECTION_THROTTLE (2): Non-essential ops throttled
    cutoff->capabilities[PROTECTION_THROTTLE][OP_LEARNING] = true;  // Throttled, not disabled
    cutoff->capabilities[PROTECTION_THROTTLE][OP_INFERENCE] = true;
    cutoff->capabilities[PROTECTION_THROTTLE][OP_ADAPTATION] = true;
    cutoff->capabilities[PROTECTION_THROTTLE][OP_NETWORK_TX] = true;
    cutoff->capabilities[PROTECTION_THROTTLE][OP_NETWORK_RX] = true;
    cutoff->capabilities[PROTECTION_THROTTLE][OP_MEMORY_ALLOC] = true;
    cutoff->capabilities[PROTECTION_THROTTLE][OP_FILE_IO] = true;
    cutoff->capabilities[PROTECTION_THROTTLE][OP_CHECKPOINT] = true;

    // PROTECTION_SHED_LOAD (3): Only critical ops
    cutoff->capabilities[PROTECTION_SHED_LOAD][OP_LEARNING] = false;  // Disabled
    cutoff->capabilities[PROTECTION_SHED_LOAD][OP_INFERENCE] = true;  // Critical only
    cutoff->capabilities[PROTECTION_SHED_LOAD][OP_ADAPTATION] = false;
    cutoff->capabilities[PROTECTION_SHED_LOAD][OP_NETWORK_TX] = true; // Critical only
    cutoff->capabilities[PROTECTION_SHED_LOAD][OP_NETWORK_RX] = true;
    cutoff->capabilities[PROTECTION_SHED_LOAD][OP_MEMORY_ALLOC] = true; // Critical only
    cutoff->capabilities[PROTECTION_SHED_LOAD][OP_FILE_IO] = true;
    cutoff->capabilities[PROTECTION_SHED_LOAD][OP_CHECKPOINT] = true;

    // PROTECTION_SAFE_MODE (4): Minimal vital ops
    cutoff->capabilities[PROTECTION_SAFE_MODE][OP_LEARNING] = false;
    cutoff->capabilities[PROTECTION_SAFE_MODE][OP_INFERENCE] = true;  // Emergency only
    cutoff->capabilities[PROTECTION_SAFE_MODE][OP_ADAPTATION] = false;
    cutoff->capabilities[PROTECTION_SAFE_MODE][OP_NETWORK_TX] = true; // Minimal
    cutoff->capabilities[PROTECTION_SAFE_MODE][OP_NETWORK_RX] = true;
    cutoff->capabilities[PROTECTION_SAFE_MODE][OP_MEMORY_ALLOC] = false; // Minimal
    cutoff->capabilities[PROTECTION_SAFE_MODE][OP_FILE_IO] = true;    // Checkpointing only
    cutoff->capabilities[PROTECTION_SAFE_MODE][OP_CHECKPOINT] = true;

    // PROTECTION_EMERGENCY_SHUTDOWN (5): All ops halted
    // All false (already zeroed)
}

//=============================================================================
// Configuration Functions
//=============================================================================

void protective_cutoff_default_config(protective_cutoff_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protective_cutoff_default_config: config is NULL");
        NIMCP_LOGGING_ERROR("Cannot initialize NULL config");
        return;
    }

    // Thresholds (based on brainstem reflex sensitivity)
    config->thresholds.warn_threshold = 0.30f;
    config->thresholds.throttle_threshold = 0.50f;
    config->thresholds.shed_load_threshold = 0.70f;
    config->thresholds.safe_mode_threshold = 0.85f;
    config->thresholds.emergency_threshold = 0.95f;
    config->thresholds.recovery_hysteresis = 0.15f;

    // Threat weights (some threats more critical than others)
    config->weights.temperature_weight = 1.5f;   // Thermal critical
    config->weights.memory_weight = 1.2f;        // Memory pressure important
    config->weights.cpu_weight = 1.0f;           // CPU baseline
    config->weights.error_rate_weight = 1.3f;    // Errors indicate instability
    config->weights.network_weight = 0.8f;       // Network less critical
    config->weights.immune_weight = 1.4f;        // Immune storm serious
    config->weights.leak_weight = 1.1f;          // Leaks accumulate
    config->weights.external_weight = 2.0f;      // External signals override

    // Other settings
    config->assessment_interval_ms = 1000;       // Assess every second
    config->enable_auto_recovery = true;         // Auto-recover when safe
    config->enable_bio_async = true;             // Enable messaging
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

protective_cutoff_t* protective_cutoff_create(const protective_cutoff_config_t* config) {
    // Allocate context
    protective_cutoff_t* cutoff = (protective_cutoff_t*)nimcp_calloc(1, sizeof(protective_cutoff_t));
    if (!cutoff) {
        NIMCP_LOGGING_ERROR("Failed to allocate protective cutoff context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "protective_cutoff_create: cutoff is NULL");
        return NULL;
    }

    // Initialize configuration
    if (config) {
        memcpy(&cutoff->config, config, sizeof(protective_cutoff_config_t));
    } else {
        protective_cutoff_default_config(&cutoff->config);
    }

    // Initialize state
    cutoff->current_level = PROTECTION_NORMAL;
    memset(cutoff->threat_severities, 0, sizeof(cutoff->threat_severities));
    cutoff->combined_threat = 0.0f;
    cutoff->last_update_time = 0;
    cutoff->manual_override = false;

    // Initialize capabilities matrix
    init_capabilities(cutoff);

    // Initialize callbacks
    cutoff->callback_count = 0;
    memset(cutoff->callbacks, 0, sizeof(cutoff->callbacks));

    // Initialize bio-async
    cutoff->bio_ctx = NULL;
    cutoff->bio_async_enabled = false;

    // Initialize mutex
    if (nimcp_platform_mutex_init(&cutoff->mutex, false) != 0) {
        NIMCP_LOGGING_ERROR("Failed to initialize mutex");
        nimcp_free(cutoff);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "protective_cutoff_create: validation failed");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Protective cutoff system created");
    return cutoff;
}

void protective_cutoff_destroy(protective_cutoff_t* cutoff) {
    if (!cutoff) {
        return;
    }

    // Disconnect bio-async
    if (cutoff->bio_async_enabled) {
        protective_cutoff_disconnect_bio_async(cutoff);
    }

    // Destroy mutex
    nimcp_platform_mutex_destroy(&cutoff->mutex);

    // Free context
    nimcp_free(cutoff);

    NIMCP_LOGGING_INFO("Protective cutoff system destroyed");
}

//=============================================================================
// Threat Assessment
//=============================================================================

/**
 * WHAT: Compute combined threat from all individual threats
 * WHY:  Unified threat metric drives protection level decisions
 * HOW:  Weighted sum of individual threat severities, normalized
 *
 * @param cutoff Context (must not be NULL, must be locked)
 * @return Combined threat severity 0.0-1.0
 */
static float compute_combined_threat(const protective_cutoff_t* cutoff) {
    const threat_weights_t* w = &cutoff->config.weights;
    const float* s = cutoff->threat_severities;

    float weighted_sum = 0.0f;
    float total_weight = 0.0f;

    weighted_sum += s[PROTECTIVE_THREAT_TEMPERATURE] * w->temperature_weight;
    total_weight += w->temperature_weight;

    weighted_sum += s[PROTECTIVE_THREAT_MEMORY_PRESSURE] * w->memory_weight;
    total_weight += w->memory_weight;

    weighted_sum += s[PROTECTIVE_THREAT_CPU_OVERLOAD] * w->cpu_weight;
    total_weight += w->cpu_weight;

    weighted_sum += s[PROTECTIVE_THREAT_ERROR_RATE] * w->error_rate_weight;
    total_weight += w->error_rate_weight;

    weighted_sum += s[PROTECTIVE_THREAT_NETWORK_LATENCY] * w->network_weight;
    total_weight += w->network_weight;

    weighted_sum += s[PROTECTIVE_THREAT_IMMUNE_STORM] * w->immune_weight;
    total_weight += w->immune_weight;

    weighted_sum += s[PROTECTIVE_THREAT_RESOURCE_LEAK] * w->leak_weight;
    total_weight += w->leak_weight;

    weighted_sum += s[PROTECTIVE_THREAT_EXTERNAL_SIGNAL] * w->external_weight;
    total_weight += w->external_weight;

    // Normalize
    if (total_weight > 0.0f) {
        return weighted_sum / total_weight;
    }

    return 0.0f;
}

/**
 * WHAT: Determine protection level from combined threat
 * WHY:  Map continuous threat severity to discrete protection level
 * HOW:  Compare against thresholds with hysteresis for recovery
 *
 * @param cutoff Context (must not be NULL, must be locked)
 * @param combined_threat Combined threat severity 0.0-1.0
 * @param is_recovery true if attempting recovery (apply hysteresis)
 * @return Appropriate protection level
 */
static protection_level_t determine_protection_level(
    const protective_cutoff_t* cutoff,
    float combined_threat,
    bool is_recovery)
{
    const protection_thresholds_t* t = &cutoff->config.thresholds;
    float hysteresis = is_recovery ? t->recovery_hysteresis : 0.0f;

    // Emergency shutdown
    if (combined_threat >= (t->emergency_threshold - hysteresis)) {
        return PROTECTION_EMERGENCY_SHUTDOWN;
    }

    // Safe mode
    if (combined_threat >= (t->safe_mode_threshold - hysteresis)) {
        return PROTECTION_SAFE_MODE;
    }

    // Load shedding
    if (combined_threat >= (t->shed_load_threshold - hysteresis)) {
        return PROTECTION_SHED_LOAD;
    }

    // Throttling
    if (combined_threat >= (t->throttle_threshold - hysteresis)) {
        return PROTECTION_THROTTLE;
    }

    // Warning
    if (combined_threat >= (t->warn_threshold - hysteresis)) {
        return PROTECTION_WARN;
    }

    // Normal
    return PROTECTION_NORMAL;
}

/**
 * WHAT: Invoke all registered callbacks for level transition
 * WHY:  Notify modules of protection state changes
 * HOW:  Iterates callback list, invokes each with old/new levels
 *
 * @param cutoff Context (must not be NULL, must be locked)
 * @param old_level Previous protection level
 * @param new_level New protection level
 */
static void invoke_callbacks(
    const protective_cutoff_t* cutoff,
    protection_level_t old_level,
    protection_level_t new_level)
{
    for (uint32_t i = 0; i < cutoff->callback_count; i++) {
        const callback_entry_t* entry = &cutoff->callbacks[i];
        if (entry->callback) {
            entry->callback(old_level, new_level, entry->user_data);
        }
    }
}

/**
 * WHAT: Publish protection level change via bio-async
 * WHY:  Broadcast state changes to other modules
 * HOW:  Sends bio-async message with old/new levels
 *
 * @param cutoff Context (must not be NULL, must be locked)
 * @param old_level Previous protection level
 * @param new_level New protection level
 */
static void publish_level_change(
    const protective_cutoff_t* cutoff,
    protection_level_t old_level,
    protection_level_t new_level)
{
    if (!cutoff->bio_async_enabled || !cutoff->bio_ctx) {
        return;
    }

    // Create message payload (old_level, new_level, combined_threat)
    struct {
        uint32_t old_level;
        uint32_t new_level;
        float combined_threat;
    } payload = {
        .old_level = (uint32_t)old_level,
        .new_level = (uint32_t)new_level,
        .combined_threat = cutoff->combined_threat
    };

    // Send message (ignore errors, best-effort)
    bio_router_send(cutoff->bio_ctx, &payload, sizeof(payload), 0);
}

int protective_cutoff_report_threat(
    protective_cutoff_t* cutoff,
    protective_threat_type_t type,
    float severity)
{
    if (!cutoff) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protective_cutoff_report_threat: cutoff is NULL");
        NIMCP_LOGGING_ERROR("NULL cutoff context");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (type >= PROTECTIVE_THREAT_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "protective_cutoff_report_threat: invalid threat type");
        NIMCP_LOGGING_ERROR("Invalid threat type: %d", type);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Clamp severity to [0.0, 1.0]
    if (severity < 0.0f) severity = 0.0f;
    if (severity > 1.0f) severity = 1.0f;

    nimcp_platform_mutex_lock(&cutoff->mutex);

    // Update threat severity
    cutoff->threat_severities[type] = severity;

    // Recompute combined threat
    cutoff->combined_threat = compute_combined_threat(cutoff);

    nimcp_platform_mutex_unlock(&cutoff->mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// State Queries
//=============================================================================

protection_level_t protective_cutoff_get_level(const protective_cutoff_t* cutoff) {
    if (!cutoff) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protective_cutoff_get_level: cutoff is NULL");
        NIMCP_LOGGING_ERROR("NULL cutoff context");
        return PROTECTION_EMERGENCY_SHUTDOWN;  // Fail-safe
    }

    // No lock needed, single enum read is atomic
    return cutoff->current_level;
}

bool protective_cutoff_can_execute(
    const protective_cutoff_t* cutoff,
    operation_type_t operation)
{
    if (!cutoff) {
        NIMCP_LOGGING_ERROR("NULL cutoff context");
        return false;
    }

    if (operation >= OP_COUNT) {
        NIMCP_LOGGING_ERROR("Invalid operation type: %d", operation);
        return false;
    }

    // No lock needed, reads are safe
    protection_level_t level = cutoff->current_level;
    return cutoff->capabilities[level][operation];
}

float protective_cutoff_get_threat(
    const protective_cutoff_t* cutoff,
    protective_threat_type_t type)
{
    if (!cutoff) {
        NIMCP_LOGGING_ERROR("NULL cutoff context");
        return -1.0f;
    }

    if (type >= PROTECTIVE_THREAT_COUNT) {
        NIMCP_LOGGING_ERROR("Invalid threat type: %d", type);
        return -1.0f;
    }

    // No lock needed, single float read is atomic
    return cutoff->threat_severities[type];
}

//=============================================================================
// Manual Control
//=============================================================================

int protective_cutoff_force_level(
    protective_cutoff_t* cutoff,
    protection_level_t level)
{
    if (!cutoff) {
        NIMCP_LOGGING_ERROR("NULL cutoff context");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (level > PROTECTION_EMERGENCY_SHUTDOWN) {
        NIMCP_LOGGING_ERROR("Invalid protection level: %d", level);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(&cutoff->mutex);

    protection_level_t old_level = cutoff->current_level;
    cutoff->current_level = level;
    cutoff->manual_override = true;

    NIMCP_LOGGING_WARN("Protection level manually forced: %s -> %s",
                       protective_cutoff_level_to_string(old_level),
                       protective_cutoff_level_to_string(level));

    // Invoke callbacks
    invoke_callbacks(cutoff, old_level, level);

    // Publish change
    publish_level_change(cutoff, old_level, level);

    nimcp_platform_mutex_unlock(&cutoff->mutex);

    return NIMCP_SUCCESS;
}

int protective_cutoff_attempt_recovery(protective_cutoff_t* cutoff) {
    if (!cutoff) {
        NIMCP_LOGGING_ERROR("NULL cutoff context");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(&cutoff->mutex);

    // Cannot recover if manually overridden
    if (cutoff->manual_override) {
        NIMCP_LOGGING_WARN("Cannot auto-recover: manual override active");
        nimcp_platform_mutex_unlock(&cutoff->mutex);
        return NIMCP_ERROR_INVALID_STATE;
    }

    // Recompute level with recovery hysteresis
    protection_level_t old_level = cutoff->current_level;
    protection_level_t new_level = determine_protection_level(
        cutoff, cutoff->combined_threat, true);

    // Only downgrade (recover), never escalate
    if (new_level < old_level) {
        cutoff->current_level = new_level;

        NIMCP_LOGGING_INFO("Protection level recovered: %s -> %s (threat: %.2f)",
                          protective_cutoff_level_to_string(old_level),
                          protective_cutoff_level_to_string(new_level),
                          cutoff->combined_threat);

        // Invoke callbacks
        invoke_callbacks(cutoff, old_level, new_level);

        // Publish change
        publish_level_change(cutoff, old_level, new_level);
    }

    nimcp_platform_mutex_unlock(&cutoff->mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Callback Registration
//=============================================================================

int protective_cutoff_register_callback(
    protective_cutoff_t* cutoff,
    protection_level_callback_t callback,
    void* user_data)
{
    if (!cutoff) {
        NIMCP_LOGGING_ERROR("NULL cutoff context");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!callback) {
        NIMCP_LOGGING_ERROR("NULL callback function");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(&cutoff->mutex);

    if (cutoff->callback_count >= MAX_CALLBACKS) {
        NIMCP_LOGGING_ERROR("Callback limit reached (%d)", MAX_CALLBACKS);
        nimcp_platform_mutex_unlock(&cutoff->mutex);
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    // Add callback
    callback_entry_t* entry = &cutoff->callbacks[cutoff->callback_count];
    entry->callback = callback;
    entry->user_data = user_data;
    cutoff->callback_count++;

    nimcp_platform_mutex_unlock(&cutoff->mutex);

    NIMCP_LOGGING_INFO("Registered protection level callback (%u total)",
                      cutoff->callback_count);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Update and Assessment
//=============================================================================

int protective_cutoff_update(protective_cutoff_t* cutoff) {
    if (!cutoff) {
        NIMCP_LOGGING_ERROR("NULL cutoff context");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(&cutoff->mutex);

    // Skip if manually overridden
    if (cutoff->manual_override) {
        nimcp_platform_mutex_unlock(&cutoff->mutex);
        return NIMCP_SUCCESS;
    }

    // Recompute combined threat
    cutoff->combined_threat = compute_combined_threat(cutoff);

    // Determine new protection level (no hysteresis for escalation)
    protection_level_t old_level = cutoff->current_level;
    protection_level_t new_level = determine_protection_level(
        cutoff, cutoff->combined_threat, false);

    // Transition if level changed
    if (new_level != old_level) {
        cutoff->current_level = new_level;

        NIMCP_LOGGING_WARN("Protection level changed: %s -> %s (threat: %.2f)",
                          protective_cutoff_level_to_string(old_level),
                          protective_cutoff_level_to_string(new_level),
                          cutoff->combined_threat);

        // Invoke callbacks
        invoke_callbacks(cutoff, old_level, new_level);

        // Publish change
        publish_level_change(cutoff, old_level, new_level);
    }

    // Attempt recovery if enabled
    if (cutoff->config.enable_auto_recovery && new_level < old_level) {
        // Already handled by level change above
    }

    nimcp_platform_mutex_unlock(&cutoff->mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int protective_cutoff_connect_bio_async(protective_cutoff_t* cutoff) {
    if (!cutoff) {
        NIMCP_LOGGING_ERROR("NULL cutoff context");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(&cutoff->mutex);

    if (cutoff->bio_async_enabled) {
        NIMCP_LOGGING_WARN("Bio-async already connected");
        nimcp_platform_mutex_unlock(&cutoff->mutex);
        return NIMCP_SUCCESS;
    }

    // Register with bio-async router
    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_BRAIN,  // Reuse immune module ID
        .module_name = "protective_cutoff",
        .inbox_capacity = 32,
        .user_data = cutoff
    };

    cutoff->bio_ctx = bio_router_register_module(&info);
    if (cutoff->bio_ctx) {
        cutoff->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }

    nimcp_platform_mutex_unlock(&cutoff->mutex);

    return NIMCP_SUCCESS;
}

int protective_cutoff_disconnect_bio_async(protective_cutoff_t* cutoff) {
    if (!cutoff) {
        NIMCP_LOGGING_ERROR("NULL cutoff context");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(&cutoff->mutex);

    if (!cutoff->bio_async_enabled) {
        nimcp_platform_mutex_unlock(&cutoff->mutex);
        return NIMCP_SUCCESS;
    }

    // Unregister from bio-async router
    if (cutoff->bio_ctx) {
        bio_router_unregister_module(cutoff->bio_ctx);
        cutoff->bio_ctx = NULL;
    }

    cutoff->bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");

    nimcp_platform_mutex_unlock(&cutoff->mutex);

    return NIMCP_SUCCESS;
}

bool protective_cutoff_is_bio_async_connected(const protective_cutoff_t* cutoff) {
    if (!cutoff) {
        NIMCP_LOGGING_ERROR("NULL cutoff context");
        return false;
    }

    return cutoff->bio_async_enabled;
}

//=============================================================================
// String Conversion Utilities
//=============================================================================

const char* protective_cutoff_level_to_string(protection_level_t level) {
    switch (level) {
        case PROTECTION_NORMAL:
            return "NORMAL";
        case PROTECTION_WARN:
            return "WARN";
        case PROTECTION_THROTTLE:
            return "THROTTLE";
        case PROTECTION_SHED_LOAD:
            return "SHED_LOAD";
        case PROTECTION_SAFE_MODE:
            return "SAFE_MODE";
        case PROTECTION_EMERGENCY_SHUTDOWN:
            return "EMERGENCY_SHUTDOWN";
        default:
            return "UNKNOWN";
    }
}

const char* protective_cutoff_threat_to_string(protective_threat_type_t type) {
    switch (type) {
        case PROTECTIVE_THREAT_TEMPERATURE:
            return "TEMPERATURE";
        case PROTECTIVE_THREAT_MEMORY_PRESSURE:
            return "MEMORY_PRESSURE";
        case PROTECTIVE_THREAT_CPU_OVERLOAD:
            return "CPU_OVERLOAD";
        case PROTECTIVE_THREAT_ERROR_RATE:
            return "ERROR_RATE";
        case PROTECTIVE_THREAT_NETWORK_LATENCY:
            return "NETWORK_LATENCY";
        case PROTECTIVE_THREAT_IMMUNE_STORM:
            return "IMMUNE_STORM";
        case PROTECTIVE_THREAT_RESOURCE_LEAK:
            return "RESOURCE_LEAK";
        case PROTECTIVE_THREAT_EXTERNAL_SIGNAL:
            return "EXTERNAL_SIGNAL";
        default:
            return "UNKNOWN";
    }
}

const char* protective_cutoff_operation_to_string(operation_type_t op) {
    switch (op) {
        case OP_LEARNING:
            return "LEARNING";
        case OP_INFERENCE:
            return "INFERENCE";
        case OP_ADAPTATION:
            return "ADAPTATION";
        case OP_NETWORK_TX:
            return "NETWORK_TX";
        case OP_NETWORK_RX:
            return "NETWORK_RX";
        case OP_MEMORY_ALLOC:
            return "MEMORY_ALLOC";
        case OP_FILE_IO:
            return "FILE_IO";
        case OP_CHECKPOINT:
            return "CHECKPOINT";
        default:
            return "UNKNOWN";
    }
}
