/**
 * @file nimcp_hypothalamus_logging_bridge.c
 * @brief Implementation of Unified Hypothalamus-Logging Integration Bridge
 *
 * WHAT: Comprehensive logging and monitoring of all hypothalamus activity
 * WHY:  Provides audit trail for safety-critical operations, enables real-time
 *       monitoring, and supports structured logging for analysis
 * HOW:  Ring buffer for entries, mutex for thread safety, orchestrator subscription
 *
 * IMPLEMENTATION NOTES:
 * =====================
 * - Thread-safe via mutex protection
 * - Ring buffer with configurable overwrite behavior
 * - Supports both console and file output
 * - JSON export for structured analysis
 * - Integrates with nimcp_logging for unified logging
 *
 * @version Phase 21: Logging Integration Bridge
 * @date 2026-01-10
 */

#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_logging_bridge.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_logging_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "HYPOTHALAMUS_LOGGING_BRIDGE"


/*=============================================================================
 * FORWARD DECLARATIONS FROM ORCHESTRATOR
 * (Included here to avoid type conflicts with hypo_drive_state_t)
 *===========================================================================*/

/** Bridge type enum - must match nimcp_hypothalamus_orchestrator.h */
typedef enum {
    HYPO_BRIDGE_UNKNOWN_ORCH = 0,
    HYPO_BRIDGE_EMOTION_ORCH,
    HYPO_BRIDGE_EXECUTIVE_ORCH,
    HYPO_BRIDGE_ATTENTION_ORCH,
    HYPO_BRIDGE_SLEEP_ORCH,
    HYPO_BRIDGE_IMMUNE_ORCH,
    HYPO_BRIDGE_WELLBEING_ORCH,
    HYPO_BRIDGE_MEMORY_ORCH,
    HYPO_BRIDGE_PERCEPTION_ORCH,
    HYPO_BRIDGE_SALIENCE_ORCH,
    HYPO_BRIDGE_REASONING_ORCH,
    HYPO_BRIDGE_GLOBAL_WORKSPACE_ORCH,
    HYPO_BRIDGE_INTROSPECTION_ORCH,
    HYPO_BRIDGE_CURIOSITY_ORCH,
    HYPO_BRIDGE_GAME_THEORY_ORCH,
    HYPO_BRIDGE_IMAGINATION_ORCH,
    HYPO_BRIDGE_EPISTEMIC_ORCH,
    HYPO_BRIDGE_COLLECTIVE_ORCH,
    HYPO_BRIDGE_BIAS_ORCH,
    HYPO_BRIDGE_THEORY_OF_MIND_ORCH,
    HYPO_BRIDGE_PREDICTIVE_ORCH,
    HYPO_BRIDGE_LOGGING_ORCH,
    HYPO_BRIDGE_BIO_ASYNC_ORCH,
    HYPO_BRIDGE_COUNT_ORCH
} hypo_bridge_type_orch_t;

/** Event type enum - must match nimcp_hypothalamus_orchestrator.h */
typedef enum {
    HYPO_EVENT_DRIVE_ACTIVATED_ORCH = 0,
    HYPO_EVENT_DRIVE_SATISFIED_ORCH,
    HYPO_EVENT_DRIVE_CONFLICT_ORCH,
    HYPO_EVENT_HOMEOSTATIC_ALERT_ORCH,
    HYPO_EVENT_CIRCADIAN_PHASE_ORCH,
    HYPO_EVENT_STRESS_RESPONSE_ORCH,
    HYPO_EVENT_AUTONOMIC_SHIFT_ORCH,
    HYPO_EVENT_ALIGNMENT_CHECK_ORCH,
    HYPO_EVENT_REWARD_SIGNAL_ORCH,
    HYPO_EVENT_SETPOINT_CHANGE_ORCH,
    HYPO_EVENT_COUNT_ORCH
} hypo_event_type_orch_t;

/** Event data structure - simplified version for logging */
typedef struct {
    hypo_event_type_orch_t event_type;
    hypo_bridge_type_orch_t source;
    uint32_t urgency;
    uint64_t timestamp;
    uint32_t event_id;

    union {
        struct {
            uint32_t drive_type;
            float drive_level;
            float deviation;
            float urgency_weight;
            char description[NIMCP_ERROR_BUFFER_MEDIUM];
        } drive;

        struct {
            uint32_t variable_id;
            float current_value;
            float setpoint;
            float deviation;
            bool is_critical;
        } homeostatic;

        struct {
            uint32_t phase;
            float phase_progress;
            float alertness;
            uint64_t next_transition;
        } circadian;

        struct {
            float stress_level;
            uint32_t stressor_type;
            float cortisol_level;
            bool is_acute;
        } stress;

        struct {
            float reward_magnitude;
            uint32_t source_drive;
            float prediction_error;
            bool is_intrinsic;
        } reward;

        struct {
            float alignment_score;
            uint32_t checked_drives;
            uint32_t violations;
            bool locked;
        } alignment;
    };
} hypo_event_data_orch_t;

/** Event callback type */
typedef int (*hypo_event_callback_orch_t)(
    const hypo_event_data_orch_t* event,
    void* user_data
);

/** Orchestrator API function declarations */
extern int hypo_orch_register_bridge(
    hypo_orchestrator_t orch,
    hypo_bridge_type_orch_t bridge_type,
    const char* name,
    void* bridge_handle,
    void* context,
    uint32_t* bridge_id_out
);

extern int hypo_orch_unregister_bridge(
    hypo_orchestrator_t orch,
    uint32_t bridge_id
);

extern int hypo_orch_subscribe(
    hypo_orchestrator_t orch,
    uint32_t subscriber_id,
    hypo_event_type_orch_t event_type,
    hypo_event_callback_orch_t callback,
    void* user_data
);

extern int hypo_orch_unsubscribe(
    hypo_orchestrator_t orch,
    uint32_t subscriber_id,
    hypo_event_type_orch_t event_type
);

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Internal logging bridge structure
 */
struct hypo_logging_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    hypo_logging_config_t config;

    /* Ring buffer for log entries */
    hypo_log_entry_t* entries;
    uint32_t buffer_size;
    uint32_t head;              /* Next write position */
    uint32_t count;             /* Current entry count */
    uint32_t sequence;          /* Monotonic sequence number */

    /* Connection state */
    hypo_orchestrator_t orchestrator;
    uint32_t orch_bridge_id;
    hypo_drive_system_handle_t* drives;
    hypo_homeostasis_handle_t* homeostasis;
    bool connected;

    /* Statistics */
    hypo_logging_stats_t stats;
    uint64_t start_time_us;

    /* File output */
    FILE* log_file;
    char* file_path;
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_current_time_us(void) {
    return nimcp_platform_time_monotonic_us();
}

/**
 * @brief Lock the bridge mutex
 */
static int bridge_lock(hypo_logging_bridge_t* bridge) {
    if (!bridge || !bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge_lock: required parameter is NULL (bridge, bridge->base)");
        return -1;
    }
    return nimcp_mutex_lock(bridge->base.mutex);
}

/**
 * @brief Unlock the bridge mutex
 */
static int bridge_unlock(hypo_logging_bridge_t* bridge) {
    if (!bridge || !bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge_unlock: required parameter is NULL (bridge, bridge->base)");
        return -1;
    }
    return nimcp_mutex_unlock(bridge->base.mutex);
}

/**
 * @brief Map log type to nimcp log level
 */
static log_level_t type_to_log_level(hypo_log_type_t type) {
    switch (type) {
        case HYPO_LOG_ERROR:
            return LOG_LEVEL_ERROR;
        case HYPO_LOG_ALIGNMENT_VIOLATION:
            return LOG_LEVEL_ERROR;
        case HYPO_LOG_STRESS_RESPONSE:
            return LOG_LEVEL_WARN;
        case HYPO_LOG_HOMEOSTATIC_ALERT:
            return LOG_LEVEL_WARN;
        default:
            return LOG_LEVEL_INFO;
    }
}

/**
 * @brief Check if type is enabled in configuration
 */
static bool type_enabled(const hypo_logging_bridge_t* bridge, hypo_log_type_t type) {
    const hypo_logging_config_t* cfg = &bridge->config;

    switch (type) {
        case HYPO_LOG_DRIVE_CHANGE:
        case HYPO_LOG_DRIVE_SATISFIED:
        case HYPO_LOG_DRIVE_CONFLICT:
            return cfg->enable_drive_logging;

        case HYPO_LOG_HOMEOSTATIC_ALERT:
            return cfg->enable_homeostatic_logging;

        case HYPO_LOG_STRESS_RESPONSE:
        case HYPO_LOG_AUTONOMIC_SHIFT:
            return cfg->enable_stress_logging;

        case HYPO_LOG_CIRCADIAN_PHASE:
            return cfg->enable_circadian_logging;

        case HYPO_LOG_ALIGNMENT_CHECK:
        case HYPO_LOG_ALIGNMENT_VIOLATION:
        case HYPO_LOG_SETPOINT_CHANGE:
            return cfg->enable_alignment_logging;

        case HYPO_LOG_REWARD_SIGNAL:
            return cfg->enable_reward_logging;

        case HYPO_LOG_BRIDGE_EVENT:
            return cfg->enable_bridge_logging;

        case HYPO_LOG_ERROR:
            return true;  /* Always log errors */

        default:
            return true;
    }
}

/**
 * @brief Add entry to ring buffer (unlocked version)
 */
static int add_entry_unlocked(hypo_logging_bridge_t* bridge,
                               const hypo_log_entry_t* entry) {
    if (!bridge || !entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "type_enabled: required parameter is NULL (bridge, entry)");
        return -1;
    }

    /* Check type filter */
    if (!type_enabled(bridge, entry->type)) {
        bridge->stats.entries_filtered++;
        return 0;
    }

    /* Check severity filter */
    if (entry->severity < (uint32_t)bridge->config.min_level) {
        bridge->stats.entries_filtered++;
        return 0;
    }

    /* Check if buffer is full */
    if (bridge->count >= bridge->buffer_size) {
        if (!bridge->config.overwrite_when_full) {
            bridge->stats.entries_dropped++;
            return -1;  /* Buffer full, drop silently — not an error condition */
        }
        /* Overwrite oldest entry */
    }

    /* Copy entry to buffer */
    hypo_log_entry_t* dest = &bridge->entries[bridge->head];
    memcpy(dest, entry, sizeof(hypo_log_entry_t));

    /* Set sequence number */
    dest->sequence_number = bridge->sequence++;

    /* Advance head */
    bridge->head = (bridge->head + 1) % bridge->buffer_size;
    if (bridge->count < bridge->buffer_size) {
        bridge->count++;
    }

    /* Update statistics */
    bridge->stats.total_entries++;
    bridge->stats.entries_by_type[entry->type]++;
    bridge->stats.last_entry_time_us = entry->timestamp_us;

    if (bridge->stats.first_entry_time_us == 0) {
        bridge->stats.first_entry_time_us = entry->timestamp_us;
    }

    /* Update severity counts */
    switch (entry->severity) {
        case HYPO_LOG_SEVERITY_TRACE:
            bridge->stats.trace_count++;
            break;
        case HYPO_LOG_SEVERITY_DEBUG:
            bridge->stats.debug_count++;
            break;
        case HYPO_LOG_SEVERITY_INFO:
            bridge->stats.info_count++;
            break;
        case HYPO_LOG_SEVERITY_WARNING:
            bridge->stats.warning_count++;
            break;
        case HYPO_LOG_SEVERITY_ERROR:
            bridge->stats.error_count++;
            break;
        case HYPO_LOG_SEVERITY_CRITICAL:
            bridge->stats.critical_count++;
            break;
    }

    /* Update safety counts */
    if (entry->type == HYPO_LOG_ALIGNMENT_CHECK) {
        bridge->stats.alignment_checks++;
    } else if (entry->type == HYPO_LOG_ALIGNMENT_VIOLATION) {
        bridge->stats.alignment_violations++;
    }

    if (entry->flags & HYPO_LOG_FLAG_SAFETY_EVENT) {
        bridge->stats.safety_events++;
    }

    if (entry->flags & HYPO_LOG_FLAG_AUDIT_REQUIRED) {
        bridge->stats.audit_entries++;
    }

    return 0;
}

/**
 * @brief Output entry to console if enabled
 */
static void output_to_console(const hypo_logging_bridge_t* bridge,
                               const hypo_log_entry_t* entry) {
    if (!bridge->config.enable_console_output) return;

    char buffer[NIMCP_ERROR_BUFFER_LARGE];
    hypo_log_entry_format(entry, buffer, sizeof(buffer));

    log_level_t level = type_to_log_level(entry->type);
    nimcp_log_write(NULL, level, HYPO_LOG_MODULE_NAME, __FILE__, __LINE__,
                    "%s", buffer);
}

/**
 * @brief Output entry to file if enabled
 */
static void output_to_file(hypo_logging_bridge_t* bridge,
                            const hypo_log_entry_t* entry) {
    if (!bridge->config.enable_file_output || !bridge->log_file) return;

    char buffer[NIMCP_LOG_BUFFER_SIZE];
    if (bridge->config.enable_structured_output) {
        hypo_log_entry_format_json(entry, buffer, sizeof(buffer));
    } else {
        hypo_log_entry_format(entry, buffer, sizeof(buffer));
    }

    fprintf(bridge->log_file, "%s\n", buffer);
    fflush(bridge->log_file);
}

/**
 * @brief Create log entry with common fields filled
 */
static hypo_log_entry_t create_entry(hypo_log_type_t type,
                                      uint32_t severity,
                                      float value,
                                      const char* message) {
    hypo_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));

    entry.timestamp_us = get_current_time_us();
    entry.type = type;
    entry.severity = severity;
    entry.value = value;

    if (message) {
        strncpy(entry.message, message, HYPO_LOG_MAX_MESSAGE_LEN - 1);
        entry.message[HYPO_LOG_MAX_MESSAGE_LEN - 1] = '\0';
    }

    return entry;
}

/**
 * @brief Orchestrator event callback
 */
static int orch_event_callback(const hypo_event_data_orch_t* event, void* user_data) {
    hypo_logging_bridge_t* bridge = (hypo_logging_bridge_t*)user_data;
    if (!bridge || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "orch_event_callback: required parameter is NULL (bridge, event)");
        return -1;
    }

    hypo_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.timestamp_us = event->timestamp;
    entry.source_bridge = (uint32_t)event->source;
    entry.severity = HYPO_LOG_SEVERITY_INFO;

    switch (event->event_type) {
        case HYPO_EVENT_DRIVE_ACTIVATED_ORCH:
            entry.type = HYPO_LOG_DRIVE_CHANGE;
            entry.value = event->drive.drive_level;
            entry.secondary_value = event->drive.deviation;
            snprintf(entry.message, HYPO_LOG_MAX_MESSAGE_LEN,
                     "Drive activated: type=%u level=%.3f",
                     event->drive.drive_type, event->drive.drive_level);
            break;

        case HYPO_EVENT_DRIVE_SATISFIED_ORCH:
            entry.type = HYPO_LOG_DRIVE_SATISFIED;
            entry.value = event->drive.drive_level;
            snprintf(entry.message, HYPO_LOG_MAX_MESSAGE_LEN,
                     "Drive satisfied: type=%u", event->drive.drive_type);
            break;

        case HYPO_EVENT_DRIVE_CONFLICT_ORCH:
            entry.type = HYPO_LOG_DRIVE_CONFLICT;
            entry.severity = HYPO_LOG_SEVERITY_WARNING;
            snprintf(entry.message, HYPO_LOG_MAX_MESSAGE_LEN,
                     "Drive conflict detected");
            break;

        case HYPO_EVENT_HOMEOSTATIC_ALERT_ORCH:
            entry.type = HYPO_LOG_HOMEOSTATIC_ALERT;
            entry.severity = event->homeostatic.is_critical ?
                             HYPO_LOG_SEVERITY_ERROR : HYPO_LOG_SEVERITY_WARNING;
            entry.value = event->homeostatic.deviation;
            entry.secondary_value = event->homeostatic.setpoint;
            snprintf(entry.message, HYPO_LOG_MAX_MESSAGE_LEN,
                     "Homeostatic alert: var=%u deviation=%.3f",
                     event->homeostatic.variable_id, event->homeostatic.deviation);
            break;

        case HYPO_EVENT_CIRCADIAN_PHASE_ORCH:
            entry.type = HYPO_LOG_CIRCADIAN_PHASE;
            entry.value = (float)event->circadian.phase;
            entry.secondary_value = event->circadian.alertness;
            snprintf(entry.message, HYPO_LOG_MAX_MESSAGE_LEN,
                     "Circadian phase: %u alertness=%.2f",
                     event->circadian.phase, event->circadian.alertness);
            break;

        case HYPO_EVENT_STRESS_RESPONSE_ORCH:
            entry.type = HYPO_LOG_STRESS_RESPONSE;
            entry.severity = HYPO_LOG_SEVERITY_WARNING;
            entry.value = event->stress.cortisol_level;
            entry.secondary_value = event->stress.stress_level;
            entry.flags |= HYPO_LOG_FLAG_SAFETY_EVENT;
            snprintf(entry.message, HYPO_LOG_MAX_MESSAGE_LEN,
                     "Stress response: level=%.2f cortisol=%.2f acute=%s",
                     event->stress.stress_level, event->stress.cortisol_level,
                     event->stress.is_acute ? "yes" : "no");
            break;

        case HYPO_EVENT_AUTONOMIC_SHIFT_ORCH:
            entry.type = HYPO_LOG_AUTONOMIC_SHIFT;
            snprintf(entry.message, HYPO_LOG_MAX_MESSAGE_LEN,
                     "Autonomic state shift");
            break;

        case HYPO_EVENT_ALIGNMENT_CHECK_ORCH:
            entry.type = HYPO_LOG_ALIGNMENT_CHECK;
            entry.value = event->alignment.alignment_score;
            entry.flags |= HYPO_LOG_FLAG_AUDIT_REQUIRED;
            snprintf(entry.message, HYPO_LOG_MAX_MESSAGE_LEN,
                     "Alignment check: score=%.3f violations=%u",
                     event->alignment.alignment_score, event->alignment.violations);

            if (event->alignment.violations > 0) {
                entry.type = HYPO_LOG_ALIGNMENT_VIOLATION;
                entry.severity = HYPO_LOG_SEVERITY_ERROR;
                entry.flags |= HYPO_LOG_FLAG_SAFETY_EVENT;
            }
            break;

        case HYPO_EVENT_REWARD_SIGNAL_ORCH:
            entry.type = HYPO_LOG_REWARD_SIGNAL;
            entry.value = event->reward.reward_magnitude;
            entry.secondary_value = event->reward.prediction_error;
            snprintf(entry.message, HYPO_LOG_MAX_MESSAGE_LEN,
                     "Reward: mag=%.3f PE=%.3f drive=%u",
                     event->reward.reward_magnitude,
                     event->reward.prediction_error,
                     event->reward.source_drive);
            break;

        case HYPO_EVENT_SETPOINT_CHANGE_ORCH:
            entry.type = HYPO_LOG_SETPOINT_CHANGE;
            entry.severity = HYPO_LOG_SEVERITY_WARNING;
            entry.flags |= HYPO_LOG_FLAG_ALIGNMENT_CRITICAL |
                          HYPO_LOG_FLAG_AUDIT_REQUIRED;
            snprintf(entry.message, HYPO_LOG_MAX_MESSAGE_LEN,
                     "Setpoint changed");
            break;

        default:
            entry.type = HYPO_LOG_BRIDGE_EVENT;
            snprintf(entry.message, HYPO_LOG_MAX_MESSAGE_LEN,
                     "Unknown event type: %d", event->event_type);
            break;
    }

    /* Log the entry */
    bridge_lock(bridge);
    add_entry_unlocked(bridge, &entry);
    bridge_unlock(bridge);

    /* Output to console/file */
    output_to_console(bridge, &entry);
    output_to_file(bridge, &entry);

    return 0;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

int hypo_logging_bridge_default_config(hypo_logging_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    memset(config, 0, sizeof(hypo_logging_config_t));

    /* Enable all logging by default */
    config->enable_drive_logging = true;
    config->enable_homeostatic_logging = true;
    config->enable_stress_logging = true;
    config->enable_circadian_logging = true;
    config->enable_alignment_logging = true;
    config->enable_reward_logging = true;
    config->enable_bridge_logging = true;

    /* Output options */
    config->enable_structured_output = false;
    config->enable_console_output = true;
    config->enable_file_output = false;

    /* Ring buffer */
    config->ring_buffer_size = HYPO_LOG_DEFAULT_BUFFER_SIZE;
    config->overwrite_when_full = true;

    /* Filtering */
    config->min_level = LOG_LEVEL_INFO;
    config->type_filter_mask = 0xFFFFFFFF;  /* All types */

    /* Format */
    config->log_prefix = HYPO_LOG_DEFAULT_PREFIX;
    config->include_timestamp = true;
    config->include_sequence = true;

    /* File output */
    config->log_file_path = NULL;
    config->append_to_file = true;
    config->max_file_size = 10 * 1024 * 1024;  /* 10 MB */

    /* Performance */
    config->async_logging = false;
    config->batch_size = 32;

    return 0;
}

hypo_logging_bridge_t* hypo_logging_bridge_create(
    const hypo_logging_config_t* config)
{
    hypo_logging_bridge_t* bridge = nimcp_calloc(1, sizeof(hypo_logging_bridge_t));
    if (!bridge) {
        LOG_ERROR("hypo_logging_bridge_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        hypo_logging_bridge_default_config(&bridge->config);
    }

    /* Allocate ring buffer */
    bridge->buffer_size = bridge->config.ring_buffer_size;
    if (bridge->buffer_size == 0) {
        bridge->buffer_size = HYPO_LOG_DEFAULT_BUFFER_SIZE;
    }

    bridge->entries = nimcp_calloc(bridge->buffer_size, sizeof(hypo_log_entry_t));
    if (!bridge->entries) {
        LOG_ERROR("hypo_logging_bridge_create: buffer allocation failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_logging_bridge_create: bridge->entries is NULL");
        return NULL;
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "hypothalamus_logging") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        LOG_ERROR("hypo_logging_bridge_create: mutex creation failed");
        nimcp_free(bridge->entries);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_logging_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize state */
    bridge->head = 0;
    bridge->count = 0;
    bridge->sequence = 1;
    bridge->connected = false;
    bridge->start_time_us = get_current_time_us();

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(hypo_logging_stats_t));
    bridge->stats.buffer_size = bridge->buffer_size;

    /* Open log file if configured */
    if (bridge->config.enable_file_output && bridge->config.log_file_path) {
        const char* mode = bridge->config.append_to_file ? "a" : "w";
        bridge->log_file = fopen(bridge->config.log_file_path, mode);
        if (!bridge->log_file) {
            LOG_WARN("hypo_logging_bridge_create: could not open log file: %s",
                     bridge->config.log_file_path);
        } else {
            bridge->file_path = nimcp_strdup(bridge->config.log_file_path);
        }
    }

    LOG_INFO("hypo_logging_bridge: created with buffer_size=%u",
             bridge->buffer_size);

    return bridge;
}

void hypo_logging_bridge_destroy(hypo_logging_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "hypothalamus_logging");

    /* Disconnect from orchestrator */
    if (bridge->connected) {
        hypo_logging_disconnect(bridge);
    }

    /* Close log file */
    if (bridge->log_file) {
        fclose(bridge->log_file);
        bridge->log_file = NULL;
    }

    /* Free file path */
    if (bridge->file_path) {
        nimcp_free(bridge->file_path);
        bridge->file_path = NULL;
    }

    /* Free mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
        bridge->base.mutex = NULL;
    }

    /* Free buffer */
    if (bridge->entries) {
        nimcp_free(bridge->entries);
        bridge->entries = NULL;
    }

    nimcp_free(bridge);
    LOG_INFO("hypo_logging_bridge: destroyed");
}

int hypo_logging_bridge_reset(hypo_logging_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge_lock(bridge);

    /* Clear buffer */
    memset(bridge->entries, 0, bridge->buffer_size * sizeof(hypo_log_entry_t));
    bridge->head = 0;
    bridge->count = 0;
    bridge->sequence = 1;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(hypo_logging_stats_t));
    bridge->stats.buffer_size = bridge->buffer_size;
    bridge->start_time_us = get_current_time_us();

    bridge_unlock(bridge);

    LOG_INFO("hypo_logging_bridge: reset");
    return 0;
}

/*=============================================================================
 * CONNECTION FUNCTIONS
 *===========================================================================*/

int hypo_logging_connect(
    hypo_logging_bridge_t* bridge,
    hypo_orchestrator_t orch)
{
    if (!bridge || !orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logging_connect: bridge or orch is NULL");
        return -1;
    }

    if (bridge->connected) {
        LOG_WARN("hypo_logging_connect: already connected");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_logging_connect: validation failed");
        return -1;
    }

    /* Register with orchestrator */
    int ret = hypo_orch_register_bridge(
        orch,
        HYPO_BRIDGE_LOGGING_ORCH,
        "logging_bridge",
        bridge,
        bridge,
        &bridge->orch_bridge_id
    );

    if (ret != 0) {
        LOG_ERROR("hypo_logging_connect: failed to register with orchestrator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_logging_connect: validation failed");
        return -1;
    }

    /* Subscribe to all event types */
    for (int i = 0; i < HYPO_EVENT_COUNT_ORCH; i++) {
        hypo_orch_subscribe(orch, bridge->orch_bridge_id,
                            (hypo_event_type_orch_t)i,
                            orch_event_callback, bridge);
    }

    bridge->orchestrator = orch;
    bridge->connected = true;

    /* Log the connection */
    hypo_log_entry_t entry = create_entry(
        HYPO_LOG_BRIDGE_EVENT,
        HYPO_LOG_SEVERITY_INFO,
        0.0f,
        "Connected to orchestrator"
    );

    bridge_lock(bridge);
    add_entry_unlocked(bridge, &entry);
    bridge_unlock(bridge);

    LOG_INFO("hypo_logging_bridge: connected to orchestrator");
    return 0;
}

int hypo_logging_disconnect(hypo_logging_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    if (!bridge->connected) {
        return 0;  /* Already disconnected */
    }

    /* Unsubscribe from events */
    for (int i = 0; i < HYPO_EVENT_COUNT_ORCH; i++) {
        hypo_orch_unsubscribe(bridge->orchestrator, bridge->orch_bridge_id,
                              (hypo_event_type_orch_t)i);
    }

    /* Unregister from orchestrator */
    hypo_orch_unregister_bridge(bridge->orchestrator, bridge->orch_bridge_id);

    bridge->orchestrator = NULL;
    bridge->connected = false;

    LOG_INFO("hypo_logging_bridge: disconnected from orchestrator");
    return 0;
}

int hypo_logging_connect_drives(
    hypo_logging_bridge_t* bridge,
    hypo_drive_system_handle_t* drives)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->drives = drives;
    return 0;
}

int hypo_logging_connect_homeostasis(
    hypo_logging_bridge_t* bridge,
    hypo_homeostasis_handle_t* homeostasis)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->homeostasis = homeostasis;
    return 0;
}

/*=============================================================================
 * LOGGING FUNCTIONS
 *===========================================================================*/

int hypo_logging_log_drive(
    hypo_logging_bridge_t* bridge,
    hypo_drive_type_t drive,
    float old_val,
    float new_val)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    hypo_log_entry_t entry = create_entry(
        HYPO_LOG_DRIVE_CHANGE,
        HYPO_LOG_SEVERITY_DEBUG,
        new_val,
        NULL
    );

    entry.secondary_value = old_val;
    snprintf(entry.message, HYPO_LOG_MAX_MESSAGE_LEN,
             "Drive %s: %.3f -> %.3f",
             hypo_drive_type_string(drive), old_val, new_val);

    bridge_lock(bridge);
    int ret = add_entry_unlocked(bridge, &entry);
    bridge_unlock(bridge);

    output_to_console(bridge, &entry);
    output_to_file(bridge, &entry);

    return ret;
}

int hypo_logging_log_drive_satisfied(
    hypo_logging_bridge_t* bridge,
    hypo_drive_type_t drive,
    float satisfaction_level,
    float reward)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    hypo_log_entry_t entry = create_entry(
        HYPO_LOG_DRIVE_SATISFIED,
        HYPO_LOG_SEVERITY_INFO,
        satisfaction_level,
        NULL
    );

    entry.secondary_value = reward;
    snprintf(entry.message, HYPO_LOG_MAX_MESSAGE_LEN,
             "Drive %s satisfied: level=%.3f reward=%.3f",
             hypo_drive_type_string(drive), satisfaction_level, reward);

    bridge_lock(bridge);
    int ret = add_entry_unlocked(bridge, &entry);
    bridge_unlock(bridge);

    output_to_console(bridge, &entry);
    output_to_file(bridge, &entry);

    return ret;
}

int hypo_logging_log_drive_conflict(
    hypo_logging_bridge_t* bridge,
    hypo_drive_type_t drive1,
    hypo_drive_type_t drive2,
    hypo_drive_type_t winner)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    hypo_log_entry_t entry = create_entry(
        HYPO_LOG_DRIVE_CONFLICT,
        HYPO_LOG_SEVERITY_WARNING,
        (float)winner,
        NULL
    );

    snprintf(entry.message, HYPO_LOG_MAX_MESSAGE_LEN,
             "Drive conflict: %s vs %s, winner=%s",
             hypo_drive_type_string(drive1),
             hypo_drive_type_string(drive2),
             hypo_drive_type_string(winner));

    bridge_lock(bridge);
    int ret = add_entry_unlocked(bridge, &entry);
    bridge_unlock(bridge);

    output_to_console(bridge, &entry);
    output_to_file(bridge, &entry);

    return ret;
}

int hypo_logging_log_homeostatic(
    hypo_logging_bridge_t* bridge,
    const char* variable,
    float deviation)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    uint32_t severity = HYPO_LOG_SEVERITY_WARNING;
    if (deviation > 0.5f) {
        severity = HYPO_LOG_SEVERITY_ERROR;
    }

    hypo_log_entry_t entry = create_entry(
        HYPO_LOG_HOMEOSTATIC_ALERT,
        severity,
        deviation,
        NULL
    );

    snprintf(entry.message, HYPO_LOG_MAX_MESSAGE_LEN,
             "Homeostatic alert: %s deviation=%.3f",
             variable ? variable : "unknown", deviation);

    bridge_lock(bridge);
    int ret = add_entry_unlocked(bridge, &entry);
    bridge_unlock(bridge);

    output_to_console(bridge, &entry);
    output_to_file(bridge, &entry);

    return ret;
}

int hypo_logging_log_stress(
    hypo_logging_bridge_t* bridge,
    float cortisol,
    const char* trigger)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    hypo_log_entry_t entry = create_entry(
        HYPO_LOG_STRESS_RESPONSE,
        HYPO_LOG_SEVERITY_WARNING,
        cortisol,
        NULL
    );

    entry.flags |= HYPO_LOG_FLAG_SAFETY_EVENT;
    snprintf(entry.message, HYPO_LOG_MAX_MESSAGE_LEN,
             "Stress response: cortisol=%.3f trigger=%s",
             cortisol, trigger ? trigger : "unknown");

    bridge_lock(bridge);
    int ret = add_entry_unlocked(bridge, &entry);
    bridge_unlock(bridge);

    output_to_console(bridge, &entry);
    output_to_file(bridge, &entry);

    return ret;
}

int hypo_logging_log_circadian(
    hypo_logging_bridge_t* bridge,
    uint32_t old_phase,
    uint32_t new_phase,
    float alertness)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    hypo_log_entry_t entry = create_entry(
        HYPO_LOG_CIRCADIAN_PHASE,
        HYPO_LOG_SEVERITY_INFO,
        (float)new_phase,
        NULL
    );

    entry.secondary_value = alertness;
    snprintf(entry.message, HYPO_LOG_MAX_MESSAGE_LEN,
             "Circadian phase: %u -> %u alertness=%.2f",
             old_phase, new_phase, alertness);

    bridge_lock(bridge);
    int ret = add_entry_unlocked(bridge, &entry);
    bridge_unlock(bridge);

    output_to_console(bridge, &entry);
    output_to_file(bridge, &entry);

    return ret;
}

int hypo_logging_log_alignment(
    hypo_logging_bridge_t* bridge,
    bool passed,
    const char* check_name)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    hypo_log_entry_t entry = create_entry(
        passed ? HYPO_LOG_ALIGNMENT_CHECK : HYPO_LOG_ALIGNMENT_VIOLATION,
        passed ? HYPO_LOG_SEVERITY_INFO : HYPO_LOG_SEVERITY_ERROR,
        passed ? 1.0f : 0.0f,
        NULL
    );

    entry.flags |= HYPO_LOG_FLAG_AUDIT_REQUIRED;
    if (!passed) {
        entry.flags |= HYPO_LOG_FLAG_SAFETY_EVENT;
    }

    snprintf(entry.message, HYPO_LOG_MAX_MESSAGE_LEN,
             "Alignment check '%s': %s",
             check_name ? check_name : "unnamed",
             passed ? "PASSED" : "FAILED");

    bridge_lock(bridge);
    int ret = add_entry_unlocked(bridge, &entry);
    bridge_unlock(bridge);

    output_to_console(bridge, &entry);
    output_to_file(bridge, &entry);

    return ret;
}

int hypo_logging_log_alignment_violation(
    hypo_logging_bridge_t* bridge,
    const char* violation_type,
    const char* details,
    uint32_t severity)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    hypo_log_entry_t entry = create_entry(
        HYPO_LOG_ALIGNMENT_VIOLATION,
        severity,
        0.0f,
        NULL
    );

    entry.flags |= HYPO_LOG_FLAG_AUDIT_REQUIRED | HYPO_LOG_FLAG_SAFETY_EVENT |
                   HYPO_LOG_FLAG_ALIGNMENT_CRITICAL;

    snprintf(entry.message, HYPO_LOG_MAX_MESSAGE_LEN,
             "ALIGNMENT VIOLATION [%s]: %s",
             violation_type ? violation_type : "unknown",
             details ? details : "no details");

    bridge_lock(bridge);
    int ret = add_entry_unlocked(bridge, &entry);
    bridge_unlock(bridge);

    output_to_console(bridge, &entry);
    output_to_file(bridge, &entry);

    return ret;
}

int hypo_logging_log_setpoint_change(
    hypo_logging_bridge_t* bridge,
    hypo_param_type_log_t param_type,
    float old_value,
    float new_value,
    uint32_t modifier_id)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    hypo_log_entry_t entry = create_entry(
        HYPO_LOG_SETPOINT_CHANGE,
        HYPO_LOG_SEVERITY_WARNING,
        new_value,
        NULL
    );

    entry.secondary_value = old_value;
    entry.flags |= HYPO_LOG_FLAG_AUDIT_REQUIRED | HYPO_LOG_FLAG_ALIGNMENT_CRITICAL;

    snprintf(entry.message, HYPO_LOG_MAX_MESSAGE_LEN,
             "Setpoint change: type=%u %.3f -> %.3f (modifier=%u)",
             param_type, old_value, new_value, modifier_id);

    bridge_lock(bridge);
    int ret = add_entry_unlocked(bridge, &entry);
    bridge_unlock(bridge);

    output_to_console(bridge, &entry);
    output_to_file(bridge, &entry);

    return ret;
}

int hypo_logging_log_reward(
    hypo_logging_bridge_t* bridge,
    float reward,
    hypo_drive_type_t source_drive,
    float prediction_error)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    hypo_log_entry_t entry = create_entry(
        HYPO_LOG_REWARD_SIGNAL,
        HYPO_LOG_SEVERITY_DEBUG,
        reward,
        NULL
    );

    entry.secondary_value = prediction_error;
    snprintf(entry.message, HYPO_LOG_MAX_MESSAGE_LEN,
             "Reward: %.3f from %s PE=%.3f",
             reward, hypo_drive_type_string(source_drive), prediction_error);

    bridge_lock(bridge);
    int ret = add_entry_unlocked(bridge, &entry);
    bridge_unlock(bridge);

    output_to_console(bridge, &entry);
    output_to_file(bridge, &entry);

    return ret;
}

int hypo_logging_log_event(
    hypo_logging_bridge_t* bridge,
    hypo_log_type_t type,
    const char* msg)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    hypo_log_entry_t entry = create_entry(
        type,
        type_to_log_level(type),
        0.0f,
        msg
    );

    bridge_lock(bridge);
    int ret = add_entry_unlocked(bridge, &entry);
    bridge_unlock(bridge);

    output_to_console(bridge, &entry);
    output_to_file(bridge, &entry);

    return ret;
}

int hypo_logging_log_entry(
    hypo_logging_bridge_t* bridge,
    const hypo_log_entry_t* entry)
{
    if (!bridge || !entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logging_log_entry: required parameter is NULL (bridge, entry)");
        return -1;
    }

    bridge_lock(bridge);
    int ret = add_entry_unlocked(bridge, entry);
    bridge_unlock(bridge);

    output_to_console(bridge, entry);
    output_to_file(bridge, entry);

    return ret;
}

/*=============================================================================
 * QUERY FUNCTIONS
 *===========================================================================*/

int hypo_logging_get_recent(
    hypo_logging_bridge_t* bridge,
    hypo_log_entry_t* entries,
    uint32_t max,
    uint32_t* count)
{
    if (!bridge || !entries || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logging_get_recent: required parameter is NULL (bridge, entries, count)");
        return -1;
    }

    bridge_lock(bridge);

    uint32_t to_copy = bridge->count;
    if (to_copy > max) {
        to_copy = max;
    }

    /* Copy entries from oldest to newest */
    uint32_t start_idx;
    if (bridge->count < bridge->buffer_size) {
        start_idx = 0;
    } else {
        start_idx = bridge->head;  /* Oldest entry */
    }

    /* Start from most recent */
    for (uint32_t i = 0; i < to_copy; i++) {
        uint32_t idx = (bridge->head - 1 - i + bridge->buffer_size) % bridge->buffer_size;
        memcpy(&entries[i], &bridge->entries[idx], sizeof(hypo_log_entry_t));
    }

    *count = to_copy;

    bridge_unlock(bridge);
    return 0;
}

int hypo_logging_query(
    hypo_logging_bridge_t* bridge,
    const hypo_log_query_t* query,
    hypo_log_entry_t* entries,
    uint32_t max,
    uint32_t* count)
{
    if (!bridge || !query || !entries || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logging_query: required parameter is NULL (bridge, query, entries, count)");
        return -1;
    }

    bridge_lock(bridge);

    uint32_t result_count = 0;
    uint32_t skipped = 0;
    uint32_t max_results = query->max_results > 0 ? query->max_results : max;
    if (max_results > max) max_results = max;

    /* Iterate through buffer */
    for (uint32_t i = 0; i < bridge->count && result_count < max_results; i++) {
        uint32_t idx;
        if (query->reverse_order) {
            /* Newest first */
            idx = (bridge->head - 1 - i + bridge->buffer_size) % bridge->buffer_size;
        } else {
            /* Oldest first */
            uint32_t start = (bridge->head - bridge->count + bridge->buffer_size) % bridge->buffer_size;
            idx = (start + i) % bridge->buffer_size;
        }

        const hypo_log_entry_t* entry = &bridge->entries[idx];

        /* Apply filters */
        if (query->start_time_us > 0 && entry->timestamp_us < query->start_time_us) {
            continue;
        }
        if (query->end_time_us > 0 && entry->timestamp_us > query->end_time_us) {
            continue;
        }
        if (query->filter_by_type && !(query->type_mask & (1u << entry->type))) {
            continue;
        }
        if (query->filter_by_severity) {
            if (entry->severity < query->min_severity ||
                entry->severity > query->max_severity) {
                continue;
            }
        }
        if (query->filter_by_source && entry->source_bridge != query->source_bridge) {
            continue;
        }
        if (query->filter_by_flags) {
            if ((entry->flags & query->required_flags) != query->required_flags) {
                continue;
            }
            if (entry->flags & query->excluded_flags) {
                continue;
            }
        }

        /* Apply offset */
        if (skipped < query->offset) {
            skipped++;
            continue;
        }

        /* Copy entry */
        memcpy(&entries[result_count], entry, sizeof(hypo_log_entry_t));
        result_count++;
    }

    *count = result_count;

    bridge_unlock(bridge);
    return 0;
}

uint64_t hypo_logging_get_count_by_type(
    const hypo_logging_bridge_t* bridge,
    hypo_log_type_t type)
{
    if (!bridge || type >= HYPO_LOG_COUNT) return 0;
    return bridge->stats.entries_by_type[type];
}

uint64_t hypo_logging_get_total_count(const hypo_logging_bridge_t* bridge) {
    if (!bridge) return 0;
    return bridge->stats.total_entries;
}

/*=============================================================================
 * EXPORT FUNCTIONS
 *===========================================================================*/

int hypo_logging_export(
    hypo_logging_bridge_t* bridge,
    const char* filepath)
{
    if (!bridge || !filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logging_export: required parameter is NULL (bridge, filepath)");
        return -1;
    }

    FILE* f = fopen(filepath, "w");
    if (!f) {
        LOG_ERROR("hypo_logging_export: failed to open file: %s", filepath);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logging_export: f is NULL");
        return -1;
    }

    bridge_lock(bridge);

    /* Write header */
    fprintf(f, "# Hypothalamus Log Export\n");
    fprintf(f, "# Entries: %u\n", bridge->count);
    fprintf(f, "# Format: timestamp_us|type|severity|value|message\n");
    fprintf(f, "\n");

    /* Write entries */
    for (uint32_t i = 0; i < bridge->count; i++) {
        uint32_t start = (bridge->head - bridge->count + bridge->buffer_size) % bridge->buffer_size;
        uint32_t idx = (start + i) % bridge->buffer_size;
        const hypo_log_entry_t* entry = &bridge->entries[idx];

        char buffer[NIMCP_ERROR_BUFFER_LARGE];
        hypo_log_entry_format(entry, buffer, sizeof(buffer));
        fprintf(f, "%s\n", buffer);
    }

    bridge->stats.entries_exported += bridge->count;

    bridge_unlock(bridge);

    fclose(f);
    LOG_INFO("hypo_logging_export: exported %u entries to %s",
             bridge->count, filepath);

    return 0;
}

int hypo_logging_export_json(
    hypo_logging_bridge_t* bridge,
    const char* filepath)
{
    if (!bridge || !filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logging_export_json: required parameter is NULL (bridge, filepath)");
        return -1;
    }

    FILE* f = fopen(filepath, "w");
    if (!f) {
        LOG_ERROR("hypo_logging_export_json: failed to open file: %s", filepath);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logging_export_json: f is NULL");
        return -1;
    }

    bridge_lock(bridge);

    fprintf(f, "{\n");
    fprintf(f, "  \"export_time_us\": %lu,\n", get_current_time_us());
    fprintf(f, "  \"total_entries\": %u,\n", bridge->count);
    fprintf(f, "  \"entries\": [\n");

    for (uint32_t i = 0; i < bridge->count; i++) {
        uint32_t start = (bridge->head - bridge->count + bridge->buffer_size) % bridge->buffer_size;
        uint32_t idx = (start + i) % bridge->buffer_size;
        const hypo_log_entry_t* entry = &bridge->entries[idx];

        char buffer[NIMCP_LOG_BUFFER_SIZE];
        hypo_log_entry_format_json(entry, buffer, sizeof(buffer));

        fprintf(f, "    %s%s\n", buffer, (i < bridge->count - 1) ? "," : "");
    }

    fprintf(f, "  ]\n");
    fprintf(f, "}\n");

    bridge->stats.entries_exported += bridge->count;

    bridge_unlock(bridge);

    fclose(f);
    LOG_INFO("hypo_logging_export_json: exported %u entries to %s",
             bridge->count, filepath);

    return 0;
}

int hypo_logging_export_query(
    hypo_logging_bridge_t* bridge,
    const hypo_log_query_t* query,
    const char* filepath)
{
    if (!bridge || !query || !filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logging_export_query: required parameter is NULL (bridge, query, filepath)");
        return -1;
    }

    /* Query entries */
    hypo_log_entry_t* entries = nimcp_calloc(HYPO_LOG_MAX_QUERY_ENTRIES,
                                              sizeof(hypo_log_entry_t));
    if (!entries) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "entries is NULL");

        return -1;

    }

    uint32_t count = 0;
    int ret = hypo_logging_query(bridge, query, entries,
                                  HYPO_LOG_MAX_QUERY_ENTRIES, &count);
    if (ret != 0) {
        nimcp_free(entries);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_logging_export_query: validation failed");
        return -1;
    }

    /* Write to file */
    FILE* f = fopen(filepath, "w");
    if (!f) {
        nimcp_free(entries);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logging_export_query: f is NULL");
        return -1;
    }

    fprintf(f, "# Filtered export: %u entries\n\n", count);

    for (uint32_t i = 0; i < count; i++) {
        char buffer[NIMCP_ERROR_BUFFER_LARGE];
        hypo_log_entry_format(&entries[i], buffer, sizeof(buffer));
        fprintf(f, "%s\n", buffer);
    }

    fclose(f);
    nimcp_free(entries);

    return 0;
}

/*=============================================================================
 * STATISTICS FUNCTIONS
 *===========================================================================*/

int hypo_logging_get_stats(
    const hypo_logging_bridge_t* bridge,
    hypo_logging_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logging_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    memcpy(stats, &bridge->stats, sizeof(hypo_logging_stats_t));

    /* Update dynamic fields */
    stats->buffer_used = bridge->count;
    if (bridge->buffer_size > 0) {
        stats->buffer_utilization = (float)bridge->count / (float)bridge->buffer_size;
    }
    stats->uptime_us = get_current_time_us() - bridge->start_time_us;

    return 0;
}

int hypo_logging_reset_stats(hypo_logging_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge_lock(bridge);

    /* Reset counters but keep buffer size */
    uint32_t buffer_size = bridge->stats.buffer_size;
    memset(&bridge->stats, 0, sizeof(hypo_logging_stats_t));
    bridge->stats.buffer_size = buffer_size;

    bridge_unlock(bridge);
    return 0;
}

/*=============================================================================
 * CONFIGURATION FUNCTIONS
 *===========================================================================*/

int hypo_logging_set_config(
    hypo_logging_bridge_t* bridge,
    const hypo_logging_config_t* config)
{
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logging_set_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    bridge_lock(bridge);
    bridge->config = *config;
    bridge_unlock(bridge);

    return 0;
}

int hypo_logging_get_config(
    const hypo_logging_bridge_t* bridge,
    hypo_logging_config_t* config)
{
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logging_get_config: required parameter is NULL (bridge, config)");
        return -1;
    }
    *config = bridge->config;
    return 0;
}

int hypo_logging_set_level(
    hypo_logging_bridge_t* bridge,
    log_level_t level)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->config.min_level = level;
    return 0;
}

int hypo_logging_set_type_enabled(
    hypo_logging_bridge_t* bridge,
    hypo_log_type_t type,
    bool enable)
{
    if (!bridge || type >= HYPO_LOG_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "hypo_logging_set_type_enabled: bridge is NULL");
        return -1;
    }

    if (enable) {
        bridge->config.type_filter_mask |= (1u << type);
    } else {
        bridge->config.type_filter_mask &= ~(1u << type);
    }

    return 0;
}

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

static const char* log_type_names[HYPO_LOG_COUNT] = {
    "DRIVE_CHANGE",
    "DRIVE_SATISFIED",
    "DRIVE_CONFLICT",
    "HOMEOSTATIC_ALERT",
    "CIRCADIAN_PHASE",
    "STRESS_RESPONSE",
    "AUTONOMIC_SHIFT",
    "ALIGNMENT_CHECK",
    "ALIGNMENT_VIOLATION",
    "SETPOINT_CHANGE",
    "REWARD_SIGNAL",
    "BRIDGE_EVENT",
    "ERROR"
};

const char* hypo_log_type_name(hypo_log_type_t type) {
    if (type >= HYPO_LOG_COUNT) return "UNKNOWN";
    return log_type_names[type];
}

static const char* severity_names[] = {
    "TRACE",
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR",
    "CRITICAL"
};

const char* hypo_log_severity_name(hypo_log_severity_t severity) {
    if (severity > HYPO_LOG_SEVERITY_CRITICAL) return "UNKNOWN";
    return severity_names[severity];
}

int hypo_log_entry_format(
    const hypo_log_entry_t* entry,
    char* buffer,
    size_t buffer_size)
{
    if (!entry || !buffer || buffer_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_log_entry_format: required parameter is NULL (entry, buffer)");
        return -1;
    }

    return snprintf(buffer, buffer_size,
                    "[%lu] [%s] [%s] val=%.3f sec=%.3f: %s",
                    entry->timestamp_us,
                    hypo_log_type_name(entry->type),
                    hypo_log_severity_name((hypo_log_severity_t)entry->severity),
                    entry->value,
                    entry->secondary_value,
                    entry->message);
}

int hypo_log_entry_format_json(
    const hypo_log_entry_t* entry,
    char* buffer,
    size_t buffer_size)
{
    if (!entry || !buffer || buffer_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_log_entry_format_json: required parameter is NULL (entry, buffer)");
        return -1;
    }

    return snprintf(buffer, buffer_size,
                    "{\"timestamp_us\": %lu, \"type\": \"%s\", "
                    "\"severity\": \"%s\", \"value\": %.4f, "
                    "\"secondary_value\": %.4f, \"sequence\": %u, "
                    "\"flags\": %u, \"message\": \"%s\"}",
                    entry->timestamp_us,
                    hypo_log_type_name(entry->type),
                    hypo_log_severity_name((hypo_log_severity_t)entry->severity),
                    entry->value,
                    entry->secondary_value,
                    entry->sequence_number,
                    entry->flags,
                    entry->message);
}

void hypo_logging_print_summary(const hypo_logging_bridge_t* bridge) {
    if (!bridge) {
        printf("hypo_logging_bridge: NULL\n");
        return;
    }

    printf("\n=== Hypothalamus Logging Bridge Summary ===\n");
    printf("Connected: %s\n", bridge->connected ? "yes" : "no");
    printf("Buffer: %u / %u entries (%.1f%% full)\n",
           bridge->count, bridge->buffer_size,
           bridge->buffer_size > 0 ?
           (100.0f * bridge->count / bridge->buffer_size) : 0.0f);
    printf("Total logged: %lu\n", bridge->stats.total_entries);
    printf("Dropped: %lu\n", bridge->stats.entries_dropped);
    printf("Filtered: %lu\n", bridge->stats.entries_filtered);
    printf("\nBy Type:\n");
    for (int i = 0; i < HYPO_LOG_COUNT; i++) {
        if (bridge->stats.entries_by_type[i] > 0) {
            printf("  %s: %lu\n", hypo_log_type_name((hypo_log_type_t)i),
                   bridge->stats.entries_by_type[i]);
        }
    }
    printf("\nBy Severity:\n");
    printf("  TRACE: %lu\n", bridge->stats.trace_count);
    printf("  DEBUG: %lu\n", bridge->stats.debug_count);
    printf("  INFO: %lu\n", bridge->stats.info_count);
    printf("  WARNING: %lu\n", bridge->stats.warning_count);
    printf("  ERROR: %lu\n", bridge->stats.error_count);
    printf("  CRITICAL: %lu\n", bridge->stats.critical_count);
    printf("\nSafety Metrics:\n");
    printf("  Alignment checks: %lu\n", bridge->stats.alignment_checks);
    printf("  Alignment violations: %lu\n", bridge->stats.alignment_violations);
    printf("  Safety events: %lu\n", bridge->stats.safety_events);
    printf("  Audit entries: %lu\n", bridge->stats.audit_entries);
    printf("==========================================\n\n");
}

void hypo_logging_print_recent(
    const hypo_logging_bridge_t* bridge,
    uint32_t count)
{
    if (!bridge) return;

    hypo_log_entry_t* entries = nimcp_calloc(count, sizeof(hypo_log_entry_t));
    if (!entries) return;

    uint32_t actual_count = 0;
    hypo_logging_get_recent((hypo_logging_bridge_t*)bridge, entries, count, &actual_count);

    printf("\n=== Recent %u Log Entries ===\n", actual_count);
    for (uint32_t i = 0; i < actual_count; i++) {
        char buffer[NIMCP_ERROR_BUFFER_LARGE];
        hypo_log_entry_format(&entries[i], buffer, sizeof(buffer));
        printf("%s\n", buffer);
    }
    printf("==============================\n\n");

    nimcp_free(entries);
}
