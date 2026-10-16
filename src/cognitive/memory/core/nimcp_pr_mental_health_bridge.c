//=============================================================================
// nimcp_pr_mental_health_bridge.c - Prime Resonant Mental Health Bridge Implementation
//=============================================================================
/**
 * @file nimcp_pr_mental_health_bridge.c
 * @brief Implementation of the PR memory-mental health integration bridge
 */

#include "cognitive/memory/core/nimcp_pr_mental_health_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include "glial/myelin_sheath/nimcp_myelin_math.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_timing_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pr_mental_health_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_pr_mental_health_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_pr_mental_health_bridge_mesh_registry = NULL;

nimcp_error_t pr_mental_health_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_pr_mental_health_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "pr_mental_health_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_MEMORY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "pr_mental_health_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_pr_mental_health_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_pr_mental_health_bridge_mesh_registry = registry;
    return err;
}

void pr_mental_health_bridge_mesh_unregister(void) {
    if (g_pr_mental_health_bridge_mesh_registry && g_pr_mental_health_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_pr_mental_health_bridge_mesh_registry, g_pr_mental_health_bridge_mesh_id);
        g_pr_mental_health_bridge_mesh_id = 0;
        g_pr_mental_health_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from pr_mental_health_bridge module (instance-level) */
static inline void pr_mental_health_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_pr_mental_health_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_pr_mental_health_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_pr_mental_health_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "PR_MENTAL_HEALTH_BRIDGE"

/* Security subsystem setters (Phase 1: Audit Gap Remediation) */
//=============================================================================
// Internal Structures
//=============================================================================

/** Intervention callback entry */
typedef struct {
    pr_mh_intervention_callback_t callback;
    void* user_data;
    bool active;
} intervention_callback_entry_t;

/**
 * @brief Internal bridge structure
 */
struct pr_mental_health_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */

    // Configuration
    pr_mh_config_t config;

    // Retrieval history (circular buffer)
    pr_mh_retrieval_event_t* history;
    size_t history_capacity;
    size_t history_count;
    size_t history_head;  // Next write position

    // Rumination tracking
    pr_mh_rumination_pattern_t* rumination_patterns;
    size_t rumination_count;
    size_t rumination_capacity;

    // Intrusion tracking
    pr_mh_intrusion_record_t* intrusion_records;
    size_t intrusion_count;
    size_t intrusion_capacity;

    // Cached analysis results
    pr_mh_valence_bias_t cached_valence_bias;
    bool valence_bias_valid;
    uint64_t valence_bias_compute_time_ms;

    pr_mh_trauma_assessment_t cached_trauma_assessment;
    bool trauma_assessment_valid;
    uint64_t trauma_assessment_compute_time_ms;

    pr_mh_mood_inference_t cached_mood;
    bool mood_valid;
    uint64_t mood_compute_time_ms;

    // Intervention management
    intervention_callback_entry_t intervention_callbacks[PR_MH_MAX_INTERVENTION_CALLBACKS];
    size_t intervention_callback_count;
    uint64_t last_intervention_time_ms;

    // Flashbulb integration
    flashbulb_system_t* flashbulb_system;
    bool flashbulb_connected;

    // Statistics
    pr_mh_stats_t stats;

    // State
    bool initialized;
    uint64_t creation_time_ms;

    /* Health agent (instance-level) - Phase 8 */
    nimcp_health_agent_t* health_agent;
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(pr_mental_health_bridge, struct pr_mental_health_bridge_struct)

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief Add retrieval event to circular buffer
 */
static void add_to_history(
    pr_mental_health_bridge_t bridge,
    const pr_mh_retrieval_event_t* event
) {
    bridge->history[bridge->history_head] = *event;
    bridge->history_head = (bridge->history_head + 1) % bridge->history_capacity;
    if (bridge->history_count < bridge->history_capacity) {
        bridge->history_count++;
    }
}

/**
 * @brief Get history entry by index (0 = oldest, count-1 = newest)
 */
static pr_mh_retrieval_event_t* get_history_entry(
    pr_mental_health_bridge_t bridge,
    size_t index
) {
    if (index >= bridge->history_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "get_history_entry: capacity exceeded");
        return NULL;
    }

    // Calculate actual position in circular buffer
    size_t start = 0;
    if (bridge->history_count == bridge->history_capacity) {
        start = bridge->history_head;  // Buffer is full, head is oldest
    } else {
        start = 0;  // Buffer not full, start from 0
    }
    size_t pos = (start + index) % bridge->history_capacity;
    return &bridge->history[pos];
}

/**
 * @brief Find or create intrusion record for node
 */
static pr_mh_intrusion_record_t* find_or_create_intrusion_record(
    pr_mental_health_bridge_t bridge,
    uint64_t node_id
) {
    // Search existing
    for (size_t i = 0; i < bridge->intrusion_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->intrusion_count > 256) {
            pr_mental_health_bridge_heartbeat("pr_mental_he_loop",
                             (float)(i + 1) / (float)bridge->intrusion_count);
        }

        if (bridge->intrusion_records[i].node_id == node_id) {
            return &bridge->intrusion_records[i];
        }
    }

    // Create new if capacity allows
    if (bridge->intrusion_count >= bridge->intrusion_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "find_or_create_intrusion_record: capacity exceeded");
        return NULL;  // At capacity
    }

    pr_mh_intrusion_record_t* record = &bridge->intrusion_records[bridge->intrusion_count];
    memset(record, 0, sizeof(*record));
    record->node_id = node_id;
    bridge->intrusion_count++;

    return record;
}

/**
 * @brief Find rumination pattern for node
 */
static pr_mh_rumination_pattern_t* find_rumination_pattern(
    pr_mental_health_bridge_t bridge,
    uint64_t node_id
) {
    for (size_t i = 0; i < bridge->rumination_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->rumination_count > 256) {
            pr_mental_health_bridge_heartbeat("pr_mental_he_loop",
                             (float)(i + 1) / (float)bridge->rumination_count);
        }

        if (bridge->rumination_patterns[i].target_node_id == node_id) {
            return &bridge->rumination_patterns[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_rumination_pattern: validation failed");
    return NULL;
}

/**
 * @brief Create or find rumination pattern
 */
static pr_mh_rumination_pattern_t* find_or_create_rumination_pattern(
    pr_mental_health_bridge_t bridge,
    uint64_t node_id
) {
    pr_mh_rumination_pattern_t* pattern = find_rumination_pattern(bridge, node_id);
    if (pattern) return pattern;

    if (bridge->rumination_count >= bridge->rumination_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "find_or_create_rumination_pattern: capacity exceeded");
        return NULL;
    }

    pattern = &bridge->rumination_patterns[bridge->rumination_count];
    memset(pattern, 0, sizeof(*pattern));
    pattern->target_node_id = node_id;
    bridge->rumination_count++;

    return pattern;
}

/**
 * @brief Count retrievals of specific node in time window
 */
static uint32_t count_retrievals_in_window(
    pr_mental_health_bridge_t bridge,
    uint64_t node_id,
    uint64_t window_start_ms,
    uint64_t window_end_ms
) {
    uint32_t count = 0;
    for (size_t i = 0; i < bridge->history_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->history_count > 256) {
            pr_mental_health_bridge_heartbeat("pr_mental_he_loop",
                             (float)(i + 1) / (float)bridge->history_count);
        }

        pr_mh_retrieval_event_t* event = get_history_entry(bridge, i);
        if (event && event->node_id == node_id &&
            event->timestamp_ms >= window_start_ms &&
            event->timestamp_ms <= window_end_ms) {
            count++;
        }
    }
    return count;
}

/**
 * @brief Compute valence statistics from history
 */
static void compute_valence_stats(
    pr_mental_health_bridge_t bridge,
    uint64_t window_start_ms,
    float* positive_count,
    float* negative_count,
    float* neutral_count,
    float* total
) {
    *positive_count = 0;
    *negative_count = 0;
    *neutral_count = 0;
    *total = 0;

    for (size_t i = 0; i < bridge->history_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->history_count > 256) {
            pr_mental_health_bridge_heartbeat("pr_mental_he_loop",
                             (float)(i + 1) / (float)bridge->history_count);
        }

        pr_mh_retrieval_event_t* event = get_history_entry(bridge, i);
        if (!event || event->timestamp_ms < window_start_ms) continue;

        (*total)++;
        if (event->valence > 0.1f) {
            (*positive_count)++;
        } else if (event->valence < -0.1f) {
            (*negative_count)++;
        } else {
            (*neutral_count)++;
        }
    }
}

/**
 * @brief Invalidate cached analysis
 */
static void invalidate_caches(pr_mental_health_bridge_t bridge) {
    bridge->valence_bias_valid = false;
    bridge->trauma_assessment_valid = false;
    bridge->mood_valid = false;
}

/**
 * @brief Invoke intervention callbacks
 */
static int invoke_intervention_callbacks(
    pr_mental_health_bridge_t bridge,
    const pr_mh_intervention_request_t* request
) {
    int result = 0;

    // Release mutex during callbacks to prevent deadlock
    nimcp_mutex_unlock(bridge->base.mutex);

    for (size_t i = 0; i < bridge->intervention_callback_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->intervention_callback_count > 256) {
            pr_mental_health_bridge_heartbeat("pr_mental_he_loop",
                             (float)(i + 1) / (float)bridge->intervention_callback_count);
        }

        if (bridge->intervention_callbacks[i].active &&
            bridge->intervention_callbacks[i].callback) {
            int cb_result = bridge->intervention_callbacks[i].callback(
                request,
                bridge->intervention_callbacks[i].user_data
            );
            if (cb_result < 0) result = cb_result;
        }
    }

    nimcp_mutex_lock(bridge->base.mutex);
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

//=============================================================================
// Configuration Functions
//=============================================================================

pr_mh_config_t pr_mental_health_bridge_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_default_config", 0.0f);


    pr_mh_config_t config = {
        // History
        .retrieval_history_size = PR_MH_DEFAULT_HISTORY_SIZE,
        .analysis_window_ms = PR_MH_DEFAULT_RUMINATION_WINDOW_MS,

        // Rumination
        .enable_rumination_detection = true,
        .rumination_threshold = PR_MH_DEFAULT_RUMINATION_THRESHOLD,
        .rumination_window_ms = PR_MH_DEFAULT_RUMINATION_WINDOW_MS,
        .rumination_valence_threshold = -0.2f,

        // Intrusion
        .enable_intrusion_tracking = true,
        .intrusion_intensity_threshold = PR_MH_DEFAULT_INTRUSION_THRESHOLD,
        .intrusion_distress_threshold = 0.5f,

        // Valence bias
        .enable_valence_analysis = true,
        .valence_bias_healthy_min = PR_MH_VALENCE_BIAS_HEALTHY_MIN,
        .valence_bias_healthy_max = PR_MH_VALENCE_BIAS_HEALTHY_MAX,
        .valence_bias_depressive = PR_MH_VALENCE_BIAS_DEPRESSIVE,
        .valence_bias_manic = 0.4f,

        // Trauma
        .enable_trauma_assessment = true,
        .trauma_load_threshold = PR_MH_TRAUMA_LOAD_THRESHOLD,

        // Intervention
        .enable_auto_intervention = true,
        .min_intervention_interval_ms = PR_MH_MIN_INTERVENTION_INTERVAL_MS,

        // Flashbulb
        .flashbulb_system = NULL
    };
    return config;
}

bool pr_mental_health_bridge_validate_config(const pr_mh_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_mental_health_bridge_validate_config: config is NULL");
        return false;
    }

    if (config->retrieval_history_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_mental_health_bridge_validate_config: config->retrieval_history_size is zero");
        return false;
    }
    if (config->rumination_threshold == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_mental_health_bridge_validate_config: config->rumination_threshold is zero");
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_validate_config", 0.0f);


    if (config->intrusion_intensity_threshold < 0.0f ||
        config->intrusion_intensity_threshold > 1.0f) return false;
    if (config->valence_bias_healthy_min >= config->valence_bias_healthy_max) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "pr_mental_health_bridge_validate_config: capacity exceeded");
        return false;
    }
    if (config->trauma_load_threshold < 0.0f ||
        config->trauma_load_threshold > 1.0f) return false;

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

pr_mental_health_bridge_t pr_mental_health_bridge_create(
    const pr_mh_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_create", 0.0f);


    pr_mh_config_t cfg;
    if (config) {
        if (!pr_mental_health_bridge_validate_config(config)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_mental_health_bridge_create: pr_mental_health_bridge_validate_config is NULL");
            return NULL;
        }
        cfg = *config;
    } else {
        cfg = pr_mental_health_bridge_default_config();
    }

    // Allocate bridge
    pr_mental_health_bridge_t bridge = nimcp_calloc(1, sizeof(struct pr_mental_health_bridge_struct));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    // Initialize bridge base infrastructure (includes mutex)
    if (bridge_base_init(&bridge->base, 0, "pr_mental_health") != 0) { nimcp_free(bridge); return NULL; }

    // Store configuration
    bridge->config = cfg;

    // Allocate history buffer
    bridge->history_capacity = cfg.retrieval_history_size;
    bridge->history = nimcp_calloc(bridge->history_capacity, sizeof(pr_mh_retrieval_event_t));
    if (!bridge->history) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_mental_health_bridge_create: bridge->history is NULL");
        return NULL;
    }

    // Allocate rumination patterns
    bridge->rumination_capacity = 64;  // Reasonable default
    bridge->rumination_patterns = nimcp_calloc(bridge->rumination_capacity,
                                         sizeof(pr_mh_rumination_pattern_t));
    if (!bridge->rumination_patterns) {
        nimcp_free(bridge->history);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_mental_health_bridge_create: bridge->rumination_patterns is NULL");
        return NULL;
    }

    // Allocate intrusion records
    bridge->intrusion_capacity = PR_MH_MAX_INTRUSIVE_MEMORIES;
    bridge->intrusion_records = nimcp_calloc(bridge->intrusion_capacity,
                                       sizeof(pr_mh_intrusion_record_t));
    if (!bridge->intrusion_records) {
        nimcp_free(bridge->rumination_patterns);
        nimcp_free(bridge->history);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_mental_health_bridge_create: bridge->intrusion_records is NULL");
        return NULL;
    }

    // Initialize state
    bridge->history_count = 0;
    bridge->history_head = 0;
    bridge->rumination_count = 0;
    bridge->intrusion_count = 0;
    bridge->intervention_callback_count = 0;
    bridge->last_intervention_time_ms = 0;

    // Initialize cached results as invalid
    bridge->valence_bias_valid = false;
    bridge->trauma_assessment_valid = false;
    bridge->mood_valid = false;

    // Initialize flashbulb connection
    bridge->flashbulb_system = cfg.flashbulb_system;
    bridge->flashbulb_connected = (cfg.flashbulb_system != NULL);

    // Initialize statistics
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.current_mood = PR_MH_MOOD_UNKNOWN;

    // Set creation time
    bridge->creation_time_ms = get_current_time_ms();
    bridge->initialized = true;

    return bridge;
}

void pr_mental_health_bridge_destroy(pr_mental_health_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "pr_mental_health");

    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_destroy", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    // Free allocated arrays
    if (bridge->history) {
        nimcp_free(bridge->history);
        bridge->history = NULL;
    }

    if (bridge->rumination_patterns) {
        nimcp_free(bridge->rumination_patterns);
        bridge->rumination_patterns = NULL;
    }

    if (bridge->intrusion_records) {
        nimcp_free(bridge->intrusion_records);
        bridge->intrusion_records = NULL;
    }

    bridge->initialized = false;

    nimcp_mutex_unlock(bridge->base.mutex);
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

pr_mh_error_t pr_mental_health_bridge_reset(pr_mental_health_bridge_t bridge) {
    if (!bridge) return PR_MH_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_MH_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    // Reset history
    bridge->history_count = 0;
    bridge->history_head = 0;
    memset(bridge->history, 0, bridge->history_capacity * sizeof(pr_mh_retrieval_event_t));

    // Reset rumination tracking
    bridge->rumination_count = 0;
    memset(bridge->rumination_patterns, 0,
           bridge->rumination_capacity * sizeof(pr_mh_rumination_pattern_t));

    // Reset intrusion tracking
    bridge->intrusion_count = 0;
    memset(bridge->intrusion_records, 0,
           bridge->intrusion_capacity * sizeof(pr_mh_intrusion_record_t));

    // Invalidate caches
    invalidate_caches(bridge);

    // Reset statistics
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.current_mood = PR_MH_MOOD_UNKNOWN;

    // Reset intervention timing
    bridge->last_intervention_time_ms = 0;

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_MH_SUCCESS;
}

//=============================================================================
// Retrieval Tracking Functions
//=============================================================================

pr_mh_error_t pr_mental_health_bridge_track_retrieval(
    pr_mental_health_bridge_t bridge,
    const pr_memory_node_t* node,
    bool was_voluntary,
    float intensity
) {
    if (!bridge || !node) return PR_MH_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_MH_ERROR_NOT_INITIALIZED;

    // Build event from node
    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_track_retrieval", 0.0f);


    pr_mh_retrieval_event_t event;
    memset(&event, 0, sizeof(event));

    event.node_id = pr_memory_node_get_id(node);
    event.timestamp_ms = get_current_time_ms();
    event.valence = node->state.x;  // Quaternion x = emotional valence
    event.arousal = node->state.y;  // Quaternion y = salience/arousal
    event.intensity = nimcp_myelin_clamp(intensity, 0.0f, 1.0f);
    event.was_voluntary = was_voluntary;
    event.is_trauma_related = false;  // Would need flashbulb integration to determine
    event.retrieval_count = (uint32_t)atomic_load(&node->access_count);

    return pr_mental_health_bridge_track_retrieval_event(bridge, &event);
}

pr_mh_error_t pr_mental_health_bridge_track_retrieval_event(
    pr_mental_health_bridge_t bridge,
    const pr_mh_retrieval_event_t* event
) {
    if (!bridge || !event) return PR_MH_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_MH_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_track_retrieval_even", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    // Add to history
    add_to_history(bridge, event);

    // Update statistics
    bridge->stats.total_retrievals_tracked++;
    if (event->was_voluntary) {
        bridge->stats.voluntary_retrievals++;
    } else {
        bridge->stats.involuntary_retrievals++;
    }
    if (event->is_trauma_related) {
        bridge->stats.trauma_related_retrievals++;
    }

    // Invalidate cached analysis
    invalidate_caches(bridge);

    // Check for intrusion (involuntary, high intensity)
    if (!event->was_voluntary &&
        event->intensity >= bridge->config.intrusion_intensity_threshold) {

        // Track as potential intrusion
        pr_mh_intrusion_record_t* record = find_or_create_intrusion_record(
            bridge, event->node_id);
        if (record) {
            record->intrusion_count++;
            if (record->first_intrusion_ms == 0) {
                record->first_intrusion_ms = event->timestamp_ms;
            }
            record->last_intrusion_ms = event->timestamp_ms;

            // Update running average intensity
            float n = (float)record->intrusion_count;
            record->average_intensity = ((n - 1) * record->average_intensity +
                                         event->intensity) / (fabsf(n) > 1e-7f ? n : 1e-7f);

            bridge->stats.intrusion_events++;
        }
    }

    // Update rumination patterns for negative valence memories
    if (event->valence < bridge->config.rumination_valence_threshold) {
        uint64_t window_start = event->timestamp_ms - bridge->config.rumination_window_ms;

        pr_mh_rumination_pattern_t* pattern = find_or_create_rumination_pattern(
            bridge, event->node_id);
        if (pattern) {
            uint32_t count = count_retrievals_in_window(
                bridge, event->node_id, window_start, event->timestamp_ms);

            if (count >= bridge->config.rumination_threshold) {
                if (!pattern->is_active) {
                    pattern->is_active = true;
                    bridge->stats.rumination_episodes++;
                    bridge->stats.active_ruminations++;
                }
            }

            pattern->repetition_count = count;
            if (pattern->first_retrieval_ms == 0) {
                pattern->first_retrieval_ms = event->timestamp_ms;
            }
            pattern->last_retrieval_ms = event->timestamp_ms;

            // Compute pattern strength based on count and interval
            if (count > 1) {
                float interval = (float)(pattern->last_retrieval_ms -
                                         pattern->first_retrieval_ms) / (count - 1);
                pattern->mean_interval_ms = interval;

                // Shorter intervals = stronger pattern
                float max_interval = (float)bridge->config.rumination_window_ms /
                                    bridge->config.rumination_threshold;
                pattern->pattern_strength = nimcp_myelin_clamp(
                    1.0f - (interval / max_interval), 0.0f, 1.0f);
            }
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_MH_SUCCESS;
}

//=============================================================================
// Rumination Detection Functions
//=============================================================================

pr_mh_error_t pr_mental_health_bridge_detect_rumination(
    pr_mental_health_bridge_t bridge,
    pr_mh_rumination_pattern_t* patterns,
    size_t max_patterns,
    size_t* count
) {
    if (!bridge || !patterns || !count) return PR_MH_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_MH_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_detect_rumination", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    size_t out_count = 0;
    for (size_t i = 0; i < bridge->rumination_count && out_count < max_patterns; i++) {
        if (bridge->rumination_patterns[i].is_active) {
            patterns[out_count++] = bridge->rumination_patterns[i];
        }
    }

    *count = out_count;

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_MH_SUCCESS;
}

bool pr_mental_health_bridge_is_ruminating_on(
    pr_mental_health_bridge_t bridge,
    uint64_t node_id,
    pr_mh_rumination_pattern_t* pattern
) {
    if (!bridge || !bridge->initialized) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_is_ruminating_on", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    pr_mh_rumination_pattern_t* found = find_rumination_pattern(bridge, node_id);
    bool is_ruminating = found && found->is_active;

    if (is_ruminating && pattern) {
        *pattern = *found;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return is_ruminating;
}

float pr_mental_health_bridge_get_rumination_score(pr_mental_health_bridge_t bridge) {
    if (!bridge || !bridge->initialized) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_get_rumination_score", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float total_strength = 0.0f;
    uint32_t active_count = 0;

    for (size_t i = 0; i < bridge->rumination_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->rumination_count > 256) {
            pr_mental_health_bridge_heartbeat("pr_mental_he_loop",
                             (float)(i + 1) / (float)bridge->rumination_count);
        }

        if (bridge->rumination_patterns[i].is_active) {
            total_strength += bridge->rumination_patterns[i].pattern_strength;
            active_count++;
        }
    }

    float score = (active_count > 0) ? (total_strength / active_count) : 0.0f;

    nimcp_mutex_unlock(bridge->base.mutex);

    return score;
}

//=============================================================================
// Intrusion Tracking Functions
//=============================================================================

pr_mh_error_t pr_mental_health_bridge_track_intrusion(
    pr_mental_health_bridge_t bridge,
    uint64_t node_id,
    float intensity,
    float distress
) {
    if (!bridge) return PR_MH_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_MH_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_track_intrusion", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    pr_mh_intrusion_record_t* record = find_or_create_intrusion_record(bridge, node_id);
    if (!record) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return PR_MH_ERROR_CAPACITY;
    }

    uint64_t now = get_current_time_ms();

    record->intrusion_count++;
    if (record->first_intrusion_ms == 0) {
        record->first_intrusion_ms = now;
    }
    record->last_intrusion_ms = now;

    // Update running averages
    float n = (float)record->intrusion_count;
    record->average_intensity = ((n - 1) * record->average_intensity +
                                 nimcp_myelin_clamp(intensity, 0.0f, 1.0f)) / (fabsf(n) > 1e-7f ? n : 1e-7f);
    record->distress_level = ((n - 1) * record->distress_level +
                              nimcp_myelin_clamp(distress, 0.0f, 1.0f)) / (fabsf(n) > 1e-7f ? n : 1e-7f);

    bridge->stats.intrusion_events++;
    bridge->stats.tracked_intrusions = bridge->intrusion_count;

    // Check if intervention needed
    if (bridge->config.enable_auto_intervention &&
        distress >= bridge->config.intrusion_distress_threshold) {

        uint64_t time_since_last = now - bridge->last_intervention_time_ms;
        if (time_since_last >= bridge->config.min_intervention_interval_ms) {

            pr_mh_intervention_request_t request = {
                .type = PR_MH_INTERVENTION_DISTRACTION,
                .trigger = PR_MH_INDICATOR_INTRUSION,
                .target_node_id = node_id,
                .severity = distress,
                .urgency = intensity,
                .timestamp_ms = now,
                .context_data = NULL
            };

            invoke_intervention_callbacks(bridge, &request);
            bridge->last_intervention_time_ms = now;
            bridge->stats.interventions_triggered++;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_MH_SUCCESS;
}

pr_mh_error_t pr_mental_health_bridge_get_intrusion_records(
    pr_mental_health_bridge_t bridge,
    pr_mh_intrusion_record_t* records,
    size_t max_records,
    size_t* count
) {
    if (!bridge || !records || !count) return PR_MH_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_MH_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_get_intrusion_record", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    size_t out_count = (bridge->intrusion_count < max_records) ?
                       bridge->intrusion_count : max_records;

    memcpy(records, bridge->intrusion_records,
           out_count * sizeof(pr_mh_intrusion_record_t));
    *count = out_count;

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_MH_SUCCESS;
}

float pr_mental_health_bridge_get_intrusion_frequency(
    pr_mental_health_bridge_t bridge,
    uint64_t node_id
) {
    if (!bridge || !bridge->initialized) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_get_intrusion_freque", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float frequency = -1.0f;

    for (size_t i = 0; i < bridge->intrusion_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->intrusion_count > 256) {
            pr_mental_health_bridge_heartbeat("pr_mental_he_loop",
                             (float)(i + 1) / (float)bridge->intrusion_count);
        }

        if (bridge->intrusion_records[i].node_id == node_id) {
            pr_mh_intrusion_record_t* record = &bridge->intrusion_records[i];
            if (record->intrusion_count > 1 && record->last_intrusion_ms > record->first_intrusion_ms) {
                float hours = (float)(record->last_intrusion_ms - record->first_intrusion_ms) /
                              (1000.0f * 60.0f * 60.0f);
                if (hours > 0) {
                    frequency = (float)record->intrusion_count / hours;
                }
            }
            break;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return frequency;
}

//=============================================================================
// Valence Bias Analysis Functions
//=============================================================================

pr_mh_error_t pr_mental_health_bridge_analyze_valence_bias(
    pr_mental_health_bridge_t bridge,
    pr_mh_valence_bias_t* bias
) {
    if (!bridge || !bias) return PR_MH_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_MH_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_analyze_valence_bias", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t now = get_current_time_ms();
    uint64_t window_start = now - bridge->config.analysis_window_ms;

    // Compute statistics
    float positive = 0, negative = 0, neutral = 0, total = 0;
    compute_valence_stats(bridge, window_start, &positive, &negative, &neutral, &total);

    if (total < 10) {  // Minimum sample size
        nimcp_mutex_unlock(bridge->base.mutex);
        return PR_MH_ERROR_INSUFFICIENT_DATA;
    }

    // Fill bias structure
    memset(bias, 0, sizeof(*bias));
    bias->positive_ratio = positive / (total > 0 ? total : 1);
    bias->negative_ratio = negative / (total > 0 ? total : 1);
    bias->neutral_ratio = neutral / (total > 0 ? total : 1);
    bias->total_retrievals = (uint32_t)total;
    bias->analysis_window_ms = bridge->config.analysis_window_ms;

    // Compute bias score: +1 = all positive, -1 = all negative
    bias->bias_score = (positive - negative) / (total > 0 ? total : 1);

    // Interpret bias
    bias->indicates_depression = (bias->bias_score < bridge->config.valence_bias_depressive);
    bias->indicates_mania = (bias->bias_score > bridge->config.valence_bias_manic);

    // Compute confidence based on sample size
    bias->confidence = fminf(1.0f, total / 100.0f);

    // Cache result
    bridge->cached_valence_bias = *bias;
    bridge->valence_bias_valid = true;
    bridge->valence_bias_compute_time_ms = now;

    // Update stats
    bridge->stats.current_valence_bias = bias->bias_score;
    if (bias->indicates_depression || bias->indicates_mania) {
        bridge->stats.bias_alerts++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_MH_SUCCESS;
}

float pr_mental_health_bridge_get_valence_bias(pr_mental_health_bridge_t bridge) {
    if (!bridge || !bridge->initialized) return NAN;

    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_get_valence_bias", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float bias = 0.0f;
    uint64_t now = get_current_time_ms();

    // Use cached value if recent (within 10 seconds)
    if (bridge->valence_bias_valid &&
        (now - bridge->valence_bias_compute_time_ms) < 10000) {
        bias = bridge->cached_valence_bias.bias_score;
    } else {
        // Recompute
        pr_mh_valence_bias_t result;
        nimcp_mutex_unlock(bridge->base.mutex);

        if (pr_mental_health_bridge_analyze_valence_bias(bridge, &result) != PR_MH_SUCCESS) {
            return NAN;
        }
        return result.bias_score;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return bias;
}

bool pr_mental_health_bridge_valence_bias_concerning(
    pr_mental_health_bridge_t bridge,
    pr_mh_indicator_t* indicator
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_mental_health_bridge_valence_bias_concerning: required parameter is NULL (bridge, bridge->initialized)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_valence_bias_concern", 0.0f);


    pr_mh_valence_bias_t bias;
    if (pr_mental_health_bridge_analyze_valence_bias(bridge, &bias) != PR_MH_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_mental_health_bridge_valence_bias_concerning: validation failed");
        return false;
    }

    if (bias.indicates_depression) {
        if (indicator) *indicator = PR_MH_INDICATOR_NEGATIVE_BIAS;
        return true;
    }

    if (bias.indicates_mania) {
        if (indicator) *indicator = PR_MH_INDICATOR_POSITIVE_BIAS;
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_mental_health_bridge_valence_bias_concerning: validation failed");
    return false;
}

//=============================================================================
// Trauma Assessment Functions
//=============================================================================

pr_mh_error_t pr_mental_health_bridge_assess_trauma_load(
    pr_mental_health_bridge_t bridge,
    pr_mh_trauma_assessment_t* assessment
) {
    if (!bridge || !assessment) return PR_MH_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_MH_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_assess_trauma_load", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    memset(assessment, 0, sizeof(*assessment));

    // Count trauma-related intrusions
    uint32_t trauma_count = 0;
    uint32_t active_intrusions = 0;
    float total_intrusion_freq = 0;
    float total_distress = 0;
    float total_avoidance = 0;

    for (size_t i = 0; i < bridge->intrusion_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->intrusion_count > 256) {
            pr_mental_health_bridge_heartbeat("pr_mental_he_loop",
                             (float)(i + 1) / (float)bridge->intrusion_count);
        }

        pr_mh_intrusion_record_t* record = &bridge->intrusion_records[i];
        if (record->is_trauma || record->is_flashbulb ||
            record->distress_level > 0.5f) {
            trauma_count++;
            active_intrusions++;
            total_distress += record->distress_level;

            // Compute frequency for this record
            if (record->intrusion_count > 1) {
                float hours = (float)(record->last_intrusion_ms - record->first_intrusion_ms) /
                              (1000.0f * 60.0f * 60.0f);
                if (hours > 0) {
                    total_intrusion_freq += (float)record->intrusion_count / hours;
                }
            }
        }
    }

    assessment->trauma_memory_count = trauma_count;
    assessment->active_intrusions = active_intrusions;

    if (trauma_count > 0) {
        assessment->mean_intrusion_frequency = total_intrusion_freq / trauma_count;
        assessment->mean_distress = total_distress / trauma_count;
        assessment->mean_avoidance_level = total_avoidance / trauma_count;
    }

    // Compute overall trauma load as weighted combination
    float intrusion_component = fminf(1.0f, assessment->mean_intrusion_frequency / 10.0f);
    float distress_component = assessment->mean_distress;
    float count_component = fminf(1.0f, trauma_count / 10.0f);

    assessment->trauma_load = (0.4f * intrusion_component +
                               0.4f * distress_component +
                               0.2f * count_component);

    // Determine if intervention needed
    assessment->needs_intervention = (assessment->trauma_load >= bridge->config.trauma_load_threshold);

    if (assessment->needs_intervention) {
        // Suggest intervention type based on dominant factor
        if (assessment->mean_intrusion_frequency > 5.0f) {
            assessment->suggested_intervention = PR_MH_INTERVENTION_DISTRACTION;
        } else if (assessment->mean_distress > 0.7f) {
            assessment->suggested_intervention = PR_MH_INTERVENTION_DAMPENING;
        } else {
            assessment->suggested_intervention = PR_MH_INTERVENTION_EXPOSURE;
        }
    }

    // Cache result
    bridge->cached_trauma_assessment = *assessment;
    bridge->trauma_assessment_valid = true;
    bridge->trauma_assessment_compute_time_ms = get_current_time_ms();

    // Update stats
    bridge->stats.current_trauma_load = assessment->trauma_load;
    if (assessment->needs_intervention) {
        bridge->stats.trauma_alerts++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_MH_SUCCESS;
}

float pr_mental_health_bridge_get_trauma_load(pr_mental_health_bridge_t bridge) {
    if (!bridge || !bridge->initialized) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_get_trauma_load", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float load = 0.0f;
    uint64_t now = get_current_time_ms();

    if (bridge->trauma_assessment_valid &&
        (now - bridge->trauma_assessment_compute_time_ms) < 10000) {
        load = bridge->cached_trauma_assessment.trauma_load;
        nimcp_mutex_unlock(bridge->base.mutex);
    } else {
        nimcp_mutex_unlock(bridge->base.mutex);
        pr_mh_trauma_assessment_t assessment;
        if (pr_mental_health_bridge_assess_trauma_load(bridge, &assessment) != PR_MH_SUCCESS) {
            return -1.0f;
        }
        load = assessment.trauma_load;
    }

    return load;
}

pr_mh_error_t pr_mental_health_bridge_mark_trauma_memory(
    pr_mental_health_bridge_t bridge,
    uint64_t node_id,
    float distress_level
) {
    if (!bridge) return PR_MH_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_MH_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_mark_trauma_memory", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    pr_mh_intrusion_record_t* record = find_or_create_intrusion_record(bridge, node_id);
    if (!record) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return PR_MH_ERROR_CAPACITY;
    }

    record->is_trauma = true;
    record->distress_level = nimcp_myelin_clamp(distress_level, 0.0f, 1.0f);

    invalidate_caches(bridge);

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_MH_SUCCESS;
}

//=============================================================================
// Mood Inference Functions
//=============================================================================

pr_mh_error_t pr_mental_health_bridge_get_mood_from_memories(
    pr_mental_health_bridge_t bridge,
    pr_mh_mood_inference_t* inference
) {
    if (!bridge || !inference) return PR_MH_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_MH_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_get_mood_from_memori", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    memset(inference, 0, sizeof(*inference));
    inference->inference_time_ms = get_current_time_ms();

    if (bridge->history_count < 10) {
        inference->primary_mood = PR_MH_MOOD_UNKNOWN;
        inference->confidence = 0.0f;
        nimcp_mutex_unlock(bridge->base.mutex);
        return PR_MH_ERROR_INSUFFICIENT_DATA;
    }

    // Analyze recent history
    float valence_sum = 0, arousal_sum = 0;
    float valence_sq_sum = 0;
    uint32_t count = 0;

    uint64_t now = get_current_time_ms();
    uint64_t window_start = now - bridge->config.analysis_window_ms;

    for (size_t i = 0; i < bridge->history_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->history_count > 256) {
            pr_mental_health_bridge_heartbeat("pr_mental_he_loop",
                             (float)(i + 1) / (float)bridge->history_count);
        }

        pr_mh_retrieval_event_t* event = get_history_entry(bridge, i);
        if (!event || event->timestamp_ms < window_start) continue;

        valence_sum += event->valence;
        arousal_sum += event->arousal;
        valence_sq_sum += event->valence * event->valence;
        count++;
    }

    if (count < 10) {
        inference->primary_mood = PR_MH_MOOD_UNKNOWN;
        inference->confidence = 0.0f;
        nimcp_mutex_unlock(bridge->base.mutex);
        return PR_MH_ERROR_INSUFFICIENT_DATA;
    }

    float mean_valence = valence_sum / (count > 0 ? count : 1);
    float mean_arousal = arousal_sum / (count > 0 ? count : 1);
    float variance = (valence_sq_sum / (count > 0 ? count : 1)) - (mean_valence * mean_valence);
    float variability = sqrtf(fmaxf(0.0f, variance));

    inference->valence_trend = mean_valence;
    inference->arousal_mean = mean_arousal;
    inference->variability = variability;
    inference->samples_used = count;

    // Determine mood based on valence/arousal combination
    // Using Russell's circumplex model of affect
    if (mean_valence < -0.3f && mean_arousal < 0.4f) {
        inference->primary_mood = PR_MH_MOOD_DEPRESSED;
    } else if (mean_valence < -0.2f && mean_arousal > 0.5f) {
        inference->primary_mood = PR_MH_MOOD_ANXIOUS;
    } else if (mean_valence > 0.3f && mean_arousal > 0.6f) {
        inference->primary_mood = PR_MH_MOOD_ELEVATED;
    } else if (mean_valence > 0.2f) {
        inference->primary_mood = PR_MH_MOOD_POSITIVE;
    } else {
        inference->primary_mood = PR_MH_MOOD_NEUTRAL;
    }

    // Check for mixed indicators
    if (variability > 0.4f) {
        inference->secondary_mood = inference->primary_mood;
        inference->primary_mood = PR_MH_MOOD_MIXED;
    }

    // Compute confidence
    inference->confidence = fminf(1.0f, count / 50.0f) *
                           (1.0f - variability);

    // Cache result
    bridge->cached_mood = *inference;
    bridge->mood_valid = true;
    bridge->mood_compute_time_ms = now;

    // Update stats
    bridge->stats.current_mood = inference->primary_mood;

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_MH_SUCCESS;
}

const char* pr_mental_health_bridge_mood_name(pr_mh_mood_state_t mood) {
    switch (mood) {
        case PR_MH_MOOD_UNKNOWN:   return "UNKNOWN";
        case PR_MH_MOOD_DEPRESSED: return "DEPRESSED";
        case PR_MH_MOOD_ANXIOUS:   return "ANXIOUS";
        case PR_MH_MOOD_NEUTRAL:   return "NEUTRAL";
        case PR_MH_MOOD_POSITIVE:  return "POSITIVE";
        case PR_MH_MOOD_ELEVATED:  return "ELEVATED";
        case PR_MH_MOOD_MIXED:     return "MIXED";
        default:                   return "INVALID";
    }
}

//=============================================================================
// Intervention Functions
//=============================================================================

pr_mh_error_t pr_mental_health_bridge_trigger_intervention(
    pr_mental_health_bridge_t bridge,
    pr_mh_intervention_type_t type,
    pr_mh_indicator_t indicator,
    uint64_t target_node_id,
    float severity
) {
    if (!bridge) return PR_MH_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_MH_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_trigger_intervention", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t now = get_current_time_ms();
    uint64_t time_since_last = now - bridge->last_intervention_time_ms;

    // Rate limiting check
    if (time_since_last < bridge->config.min_intervention_interval_ms) {
        bridge->stats.interventions_blocked++;
        nimcp_mutex_unlock(bridge->base.mutex);
        return PR_MH_ERROR_INTERVENTION_BLOCKED;
    }

    pr_mh_intervention_request_t request = {
        .type = type,
        .trigger = indicator,
        .target_node_id = target_node_id,
        .severity = nimcp_myelin_clamp(severity, 0.0f, 1.0f),
        .urgency = severity,  // Use severity as urgency by default
        .timestamp_ms = now,
        .context_data = NULL
    };

    invoke_intervention_callbacks(bridge, &request);

    bridge->last_intervention_time_ms = now;
    bridge->stats.interventions_triggered++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_MH_SUCCESS;
}

pr_mh_error_t pr_mental_health_bridge_register_intervention_callback(
    pr_mental_health_bridge_t bridge,
    pr_mh_intervention_callback_t callback,
    void* user_data
) {
    if (!bridge || !callback) return PR_MH_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_MH_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_register_interventio", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->intervention_callback_count >= PR_MH_MAX_INTERVENTION_CALLBACKS) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return PR_MH_ERROR_CAPACITY;
    }

    intervention_callback_entry_t* entry =
        &bridge->intervention_callbacks[bridge->intervention_callback_count];
    entry->callback = callback;
    entry->user_data = user_data;
    entry->active = true;
    bridge->intervention_callback_count++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_MH_SUCCESS;
}

pr_mh_error_t pr_mental_health_bridge_suggest_intervention(
    pr_mental_health_bridge_t bridge,
    pr_mh_intervention_type_t* intervention,
    pr_mh_indicator_t* indicator
) {
    if (!bridge || !intervention || !indicator) return PR_MH_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_MH_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_suggest_intervention", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    *intervention = PR_MH_INTERVENTION_NONE;
    *indicator = PR_MH_INDICATOR_RUMINATION;  // Default

    // Check rumination score
    float rumination_score = 0;
    for (size_t i = 0; i < bridge->rumination_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->rumination_count > 256) {
            pr_mental_health_bridge_heartbeat("pr_mental_he_loop",
                             (float)(i + 1) / (float)bridge->rumination_count);
        }

        if (bridge->rumination_patterns[i].is_active) {
            rumination_score = fmaxf(rumination_score,
                                     bridge->rumination_patterns[i].pattern_strength);
        }
    }

    if (rumination_score > 0.7f) {
        *intervention = PR_MH_INTERVENTION_DISTRACTION;
        *indicator = PR_MH_INDICATOR_RUMINATION;
        nimcp_mutex_unlock(bridge->base.mutex);
        return PR_MH_SUCCESS;
    }

    // Check trauma load
    if (bridge->trauma_assessment_valid &&
        bridge->cached_trauma_assessment.needs_intervention) {
        *intervention = bridge->cached_trauma_assessment.suggested_intervention;
        *indicator = PR_MH_INDICATOR_TRAUMA_LOAD;
        nimcp_mutex_unlock(bridge->base.mutex);
        return PR_MH_SUCCESS;
    }

    // Check valence bias
    if (bridge->valence_bias_valid) {
        if (bridge->cached_valence_bias.indicates_depression) {
            *intervention = PR_MH_INTERVENTION_COGNITIVE;
            *indicator = PR_MH_INDICATOR_NEGATIVE_BIAS;
            nimcp_mutex_unlock(bridge->base.mutex);
            return PR_MH_SUCCESS;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_MH_ERROR_INSUFFICIENT_DATA;
}

const char* pr_mental_health_bridge_intervention_name(pr_mh_intervention_type_t type) {
    switch (type) {
        case PR_MH_INTERVENTION_NONE:           return "NONE";
        case PR_MH_INTERVENTION_DISTRACTION:    return "DISTRACTION";
        case PR_MH_INTERVENTION_RECONSOLIDATION:return "RECONSOLIDATION";
        case PR_MH_INTERVENTION_DAMPENING:      return "DAMPENING";
        case PR_MH_INTERVENTION_EXPOSURE:       return "EXPOSURE";
        case PR_MH_INTERVENTION_COGNITIVE:      return "COGNITIVE";
        case PR_MH_INTERVENTION_MINDFULNESS:    return "MINDFULNESS";
        default:                                return "INVALID";
    }
}

//=============================================================================
// Update and Analysis Functions
//=============================================================================

pr_mh_error_t pr_mental_health_bridge_update(
    pr_mental_health_bridge_t bridge,
    uint64_t current_time_ms
) {
    if (!bridge) return PR_MH_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_MH_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_update", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    // Update uptime
    bridge->stats.uptime_ms = current_time_ms - bridge->creation_time_ms;
    bridge->stats.last_analysis_ms = current_time_ms;

    // Expire old rumination patterns
    uint64_t window_cutoff = current_time_ms - bridge->config.rumination_window_ms;
    for (size_t i = 0; i < bridge->rumination_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->rumination_count > 256) {
            pr_mental_health_bridge_heartbeat("pr_mental_he_loop",
                             (float)(i + 1) / (float)bridge->rumination_count);
        }

        pr_mh_rumination_pattern_t* pattern = &bridge->rumination_patterns[i];
        if (pattern->is_active && pattern->last_retrieval_ms < window_cutoff) {
            pattern->is_active = false;
            if (bridge->stats.active_ruminations > 0) {
                bridge->stats.active_ruminations--;
            }
        }
    }

    // Invalidate old caches
    uint64_t cache_timeout = NIMCP_LONG_TIMEOUT_MS;  // 30 seconds
    if (bridge->valence_bias_valid &&
        (current_time_ms - bridge->valence_bias_compute_time_ms) > cache_timeout) {
        bridge->valence_bias_valid = false;
    }
    if (bridge->trauma_assessment_valid &&
        (current_time_ms - bridge->trauma_assessment_compute_time_ms) > cache_timeout) {
        bridge->trauma_assessment_valid = false;
    }
    if (bridge->mood_valid &&
        (current_time_ms - bridge->mood_compute_time_ms) > cache_timeout) {
        bridge->mood_valid = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_MH_SUCCESS;
}

size_t pr_mental_health_bridge_prune_history(
    pr_mental_health_bridge_t bridge,
    uint64_t max_age_ms
) {
    if (!bridge || !bridge->initialized) return 0;

    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_prune_history", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t now = get_current_time_ms();
    uint64_t cutoff = now - max_age_ms;
    size_t pruned = 0;

    // Since it's a circular buffer, we can't easily remove old entries
    // Instead, we mark them as invalid by zeroing node_id and count them
    for (size_t i = 0; i < bridge->history_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->history_count > 256) {
            pr_mental_health_bridge_heartbeat("pr_mental_he_loop",
                             (float)(i + 1) / (float)bridge->history_count);
        }

        pr_mh_retrieval_event_t* event = get_history_entry(bridge, i);
        if (event && event->timestamp_ms < cutoff && event->node_id != 0) {
            event->node_id = 0;  // Mark as pruned
            pruned++;
        }
    }

    invalidate_caches(bridge);

    nimcp_mutex_unlock(bridge->base.mutex);

    return pruned;
}

//=============================================================================
// Statistics and Queries
//=============================================================================

pr_mh_error_t pr_mental_health_bridge_get_stats(
    pr_mental_health_bridge_t bridge,
    pr_mh_stats_t* stats
) {
    if (!bridge || !stats) return PR_MH_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_MH_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_get_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_MH_SUCCESS;
}

pr_mh_error_t pr_mental_health_bridge_reset_stats(pr_mental_health_bridge_t bridge) {
    if (!bridge) return PR_MH_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_MH_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_reset_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    // Preserve current state indicators
    pr_mh_mood_state_t current_mood = bridge->stats.current_mood;
    float current_bias = bridge->stats.current_valence_bias;
    float current_trauma = bridge->stats.current_trauma_load;
    uint32_t active_rum = bridge->stats.active_ruminations;
    uint32_t tracked_int = bridge->stats.tracked_intrusions;

    memset(&bridge->stats, 0, sizeof(bridge->stats));

    // Restore current state
    bridge->stats.current_mood = current_mood;
    bridge->stats.current_valence_bias = current_bias;
    bridge->stats.current_trauma_load = current_trauma;
    bridge->stats.active_ruminations = active_rum;
    bridge->stats.tracked_intrusions = tracked_int;

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_MH_SUCCESS;
}

size_t pr_mental_health_bridge_get_history_count(pr_mental_health_bridge_t bridge) {
    if (!bridge || !bridge->initialized) return 0;

    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_get_history_count", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    size_t count = bridge->history_count;
    nimcp_mutex_unlock(bridge->base.mutex);

    return count;
}

pr_mh_error_t pr_mental_health_bridge_export_history(
    pr_mental_health_bridge_t bridge,
    pr_mh_retrieval_event_t* events,
    size_t max_events,
    size_t* count
) {
    if (!bridge || !events || !count) return PR_MH_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_MH_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_export_history", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    size_t out_count = (bridge->history_count < max_events) ?
                       bridge->history_count : max_events;

    for (size_t i = 0; i < out_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && out_count > 256) {
            pr_mental_health_bridge_heartbeat("pr_mental_he_loop",
                             (float)(i + 1) / (float)out_count);
        }

        pr_mh_retrieval_event_t* event = get_history_entry(bridge, i);
        if (event) {
            events[i] = *event;
        }
    }

    *count = out_count;

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_MH_SUCCESS;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* pr_mental_health_bridge_error_string(pr_mh_error_t error) {
    switch (error) {
        case PR_MH_SUCCESS:                     return "Success";
        case PR_MH_ERROR_NULL_POINTER:          return "Null pointer argument";
        case PR_MH_ERROR_NOT_INITIALIZED:       return "Bridge not initialized";
        case PR_MH_ERROR_INVALID_CONFIG:        return "Invalid configuration";
        case PR_MH_ERROR_NO_MEMORY:             return "Memory allocation failed";
        case PR_MH_ERROR_CAPACITY:              return "Capacity limit reached";
        case PR_MH_ERROR_NOT_FOUND:             return "Memory/entry not found";
        case PR_MH_ERROR_INSUFFICIENT_DATA:     return "Insufficient data for analysis";
        case PR_MH_ERROR_INTERVENTION_BLOCKED:  return "Intervention rate limited";
        case PR_MH_ERROR_INVALID_STATE:         return "Invalid state for operation";
        default:                                return "Unknown error";
    }
}

const char* pr_mental_health_bridge_indicator_name(pr_mh_indicator_t indicator) {
    switch (indicator) {
        case PR_MH_INDICATOR_RUMINATION:        return "RUMINATION";
        case PR_MH_INDICATOR_INTRUSION:         return "INTRUSION";
        case PR_MH_INDICATOR_NEGATIVE_BIAS:     return "NEGATIVE_BIAS";
        case PR_MH_INDICATOR_POSITIVE_BIAS:     return "POSITIVE_BIAS";
        case PR_MH_INDICATOR_TRAUMA_LOAD:       return "TRAUMA_LOAD";
        case PR_MH_INDICATOR_AVOIDANCE:         return "AVOIDANCE";
        case PR_MH_INDICATOR_FRAGMENTATION:     return "FRAGMENTATION";
        default:                                return "INVALID";
    }
}

uint64_t pr_mental_health_bridge_current_time_ms(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_current_time_ms", 0.0f);


    return get_current_time_ms();
}

bool pr_mental_health_bridge_validate(pr_mental_health_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_mental_health_bridge_validate: bridge is NULL");
        return false;
    }
    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_mental_health_bridge_validate: bridge->initialized is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_validate", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bool valid = true;

    // Check array allocations
    if (!bridge->history) valid = false;
    if (!bridge->rumination_patterns) valid = false;
    if (!bridge->intrusion_records) valid = false;

    // Check counts vs capacities
    if (bridge->history_count > bridge->history_capacity) valid = false;
    if (bridge->rumination_count > bridge->rumination_capacity) valid = false;
    if (bridge->intrusion_count > bridge->intrusion_capacity) valid = false;

    // Check circular buffer invariant
    if (bridge->history_head >= bridge->history_capacity) valid = false;

    nimcp_mutex_unlock(bridge->base.mutex);

    return valid;
}

//=============================================================================
// Flashbulb Integration Functions
//=============================================================================

pr_mh_error_t pr_mental_health_bridge_connect_flashbulb(
    pr_mental_health_bridge_t bridge,
    flashbulb_system_t* flashbulb_system
) {
    if (!bridge) return PR_MH_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_MH_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_connect_flashbulb", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->flashbulb_system = flashbulb_system;
    bridge->flashbulb_connected = (flashbulb_system != NULL);

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_MH_SUCCESS;
}

pr_mh_error_t pr_mental_health_bridge_sync_flashbulb(pr_mental_health_bridge_t bridge) {
    if (!bridge) return PR_MH_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_MH_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_mental_health_bridge_heartbeat("pr_mental_he_sync_flashbulb", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->flashbulb_connected || !bridge->flashbulb_system) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return PR_MH_ERROR_NOT_INITIALIZED;
    }

    // Sync trauma memories from flashbulb system
    flashbulb_system_t* fb = bridge->flashbulb_system;

    // Get trauma memories from flashbulb system
    flashbulb_memory_t* trauma_memories[64];
    size_t trauma_count = 0;

    pr_mh_error_t result = PR_MH_SUCCESS;

    // Note: This would need to use flashbulb_retrieve_by_type but we need
    // to release our mutex first to avoid deadlock
    nimcp_mutex_unlock(bridge->base.mutex);

    flashbulb_error_t fb_err = flashbulb_retrieve_by_type(
        fb,
        FLASHBULB_TRAUMATIC,
        trauma_memories,
        64,
        &trauma_count
    );

    if (fb_err != FLASHBULB_SUCCESS) {
        return PR_MH_ERROR_NOT_FOUND;
    }

    // Update intrusion records from flashbulb trauma memories
    for (size_t i = 0; i < trauma_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && trauma_count > 256) {
            pr_mental_health_bridge_heartbeat("pr_mental_he_loop",
                             (float)(i + 1) / (float)trauma_count);
        }

        flashbulb_memory_t* fb_mem = trauma_memories[i];
        if (fb_mem && fb_mem->requires_trauma_handling) {
            nimcp_mutex_lock(bridge->base.mutex);

            pr_mh_intrusion_record_t* record = find_or_create_intrusion_record(
                bridge, fb_mem->flashbulb_id);

            if (record) {
                record->is_trauma = true;
                record->is_flashbulb = true;
                record->distress_level = fb_mem->hyperarousal;
            }

            nimcp_mutex_unlock(bridge->base.mutex);
        }
    }

    return result;
}

//=============================================================================
// Instance Health Agent Setter (B25 Upgrade)
//=============================================================================

void pr_mental_health_bridge_set_instance_health_agent(
    pr_mental_health_bridge_t bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B25 Upgrade)
//=============================================================================

int pr_mental_health_bridge_training_begin(pr_mental_health_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_mental_health_bridge_training_begin: NULL argument");
        return -1;
    }
    pr_mental_health_bridge_heartbeat_instance(bridge->health_agent, "pr_mental_health_bridge_training_begin", 0.0f);
    return 0;
}

int pr_mental_health_bridge_training_end(pr_mental_health_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_mental_health_bridge_training_end: NULL argument");
        return -1;
    }
    pr_mental_health_bridge_heartbeat_instance(bridge->health_agent, "pr_mental_health_bridge_training_end", 1.0f);
    return 0;
}

int pr_mental_health_bridge_training_step(pr_mental_health_bridge_t bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_mental_health_bridge_training_step: NULL argument");
        return -1;
    }
    pr_mental_health_bridge_heartbeat_instance(bridge->health_agent, "pr_mental_health_bridge_training_step", progress);
    return 0;
}
