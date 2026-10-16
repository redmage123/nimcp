/**
 * @file nimcp_cingulate_adapter.c
 * @brief Implementation of Cingulate Cortex brain adapter
 *
 * WHAT: Unified adapter connecting ACC and PCC modules to the brain system
 * WHY:  Enable conflict monitoring, error detection, and self-referential processing
 * HOW:  Implements Botvinick's conflict model and ERN generation
 *
 * @version Phase B4: Cingulate Cortex Integration
 * @date 2025-12-30
 */

#include "core/brain/regions/cingulate/nimcp_cingulate_adapter.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(cingulate_adapter, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define CINGULATE_LOG_MODULE "CINGULATE"

/*=============================================================================
 * INTERNAL CONSTANTS
 *===========================================================================*/

#define MAX_RESPONSE_OPTIONS 16
#define ERN_PEAK_LATENCY_MS  80.0f
#define PE_PEAK_LATENCY_MS   300.0f

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Conflict history entry
 */
typedef struct conflict_entry {
    cingulate_conflict_t conflict;
    struct conflict_entry* next;
} conflict_entry_t;

/**
 * @brief Error history entry
 */
typedef struct error_entry {
    cingulate_error_event_t error;
    struct error_entry* next;
} error_entry_t;

/**
 * @brief Internal adapter structure
 */
struct cingulate_adapter {
    /* Configuration */
    cingulate_config_t config;

    /* Response monitoring state (ACC) */
    cingulate_response_option_t* response_options;
    uint32_t num_response_options;
    uint32_t active_response_count;
    bool monitoring_active;

    /* Conflict tracking */
    conflict_entry_t* conflict_history;
    uint32_t conflict_count;
    cingulate_conflict_t last_conflict;
    bool has_pending_conflict;

    /* Error tracking */
    error_entry_t* error_history;
    uint32_t error_count;
    cingulate_error_event_t last_error;
    bool has_pending_error;
    uint32_t next_error_id;

    /* Cognitive control state */
    float current_control_level;
    float conflict_accumulator;
    float error_accumulator;
    cingulate_control_signal_t last_control_signal;

    /* Self-referential processing (PCC) */
    float self_relevance_baseline;
    float autobio_activation;
    float internal_focus_level;
    bool dmn_active;
    cingulate_self_reference_t last_self_reference;

    /* Emotion integration */
    float emotional_valence;
    float emotional_arousal;
    float pain_level;

    /* Callbacks */
    cingulate_conflict_callback_t conflict_callback;
    void* conflict_user_data;
    cingulate_error_callback_t error_callback;
    void* error_user_data;
    cingulate_control_callback_t control_callback;
    void* control_user_data;
    cingulate_event_callback_t event_callback;
    void* event_user_data;

    /* State */
    cingulate_status_t status;
    cingulate_error_t last_error_code;
    double current_time_ms;

    /* Bio-async communication */
    bio_module_context_t bio_ctx;
    nimcp_bio_channel_type_t default_channel;

    /* Statistics */
    cingulate_stats_t stats;
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Set error state
 */
static void set_error(cingulate_adapter_t* adapter, cingulate_error_t error) {
    if (!adapter) return;
    adapter->last_error_code = error;
    if (error != CINGULATE_ERROR_NONE) {
        adapter->status = CINGULATE_STATUS_ERROR;
        LOG_ERROR("[%s] Error set: %d", CINGULATE_LOG_MODULE, error);
    }
}

/**
 * @brief Emit event to callback
 */
static void emit_event(cingulate_adapter_t* adapter, uint32_t event_type, const void* data) {
    if (adapter->config.enable_events && adapter->event_callback) {
        adapter->event_callback(event_type, data, adapter->event_user_data);
    }
}

/**
 * @brief Compute conflict using Botvinick's model
 *
 * WHAT: Compute response conflict as product of activations
 * WHY:  Conflicts arise when multiple responses are simultaneously active
 * HOW:  Sum of activation products for all response pairs
 *
 * BIOLOGICAL: Based on Botvinick et al. (2001) conflict monitoring model
 */
static float compute_conflict(const cingulate_response_option_t* options, uint32_t count) {
    float conflict = 0.0f;

    for (uint32_t i = 0; i < count; i++) {
        for (uint32_t j = i + 1; j < count; j++) {
            conflict += options[i].activation * options[j].activation;
        }
    }

    return conflict;
}

/**
 * @brief Compute ERN amplitude based on error magnitude
 *
 * WHAT: Model error-related negativity amplitude
 * WHY:  ERN reflects prediction error magnitude
 * HOW:  Scale by error size with biologically plausible range
 */
static float compute_ern_amplitude(float error_magnitude) {
    /* ERN typically ranges -5 to -15 microvolts, map to normalized scale */
    const float ERN_BASE = -0.3f;
    const float ERN_SCALE = -0.7f;
    return ERN_BASE + ERN_SCALE * error_magnitude;
}

/**
 * @brief Compute Pe amplitude (conscious error awareness)
 *
 * WHAT: Model positivity error amplitude
 * WHY:  Pe indicates conscious error awareness
 * HOW:  Scale by error magnitude and awareness threshold
 */
static float compute_pe_amplitude(float error_magnitude, float threshold) {
    if (error_magnitude < threshold) {
        return 0.0f;  /* Error not consciously perceived */
    }
    /* Pe typically 2-10 microvolts, map to normalized scale */
    const float PE_SCALE = 0.5f;
    return PE_SCALE * (error_magnitude - threshold);
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

cingulate_config_t cingulate_default_config(void) {
    cingulate_config_t config;
    memset(&config, 0, sizeof(config));

    config.max_conflicts = CINGULATE_DEFAULT_MAX_CONFLICTS;
    config.max_errors = CINGULATE_DEFAULT_MAX_ERRORS;
    config.response_options = MAX_RESPONSE_OPTIONS;

    config.conflict_threshold = CINGULATE_DEFAULT_CONFLICT_THRESHOLD;
    config.error_threshold = CINGULATE_DEFAULT_ERROR_THRESHOLD;
    config.adjustment_threshold = 0.4f;

    config.ern_window_ms = CINGULATE_DEFAULT_ERN_WINDOW_MS;
    config.adaptation_decay_ms = 500.0f;

    config.enable_conflict_monitoring = true;
    config.enable_error_detection = true;
    config.enable_cognitive_control = true;

    config.enable_self_referential = true;
    config.enable_dmn_integration = true;
    config.enable_autobio_access = true;

    config.enable_learning = true;
    config.adaptation_rate = CINGULATE_DEFAULT_ADAPTATION_RATE;

    config.enable_events = true;

    config.enable_bio_async = true;
    config.default_channel = BIO_CHANNEL_NOREPINEPHRINE;  /* NE for alerting/error */

    return config;
}

cingulate_adapter_t* cingulate_create(const cingulate_config_t* config) {
    LOG_INFO("[%s] Creating cingulate cortex adapter", CINGULATE_LOG_MODULE);

    cingulate_adapter_t* adapter = (cingulate_adapter_t*)nimcp_calloc(1, sizeof(cingulate_adapter_t));
    if (!adapter) {
        LOG_ERROR("[%s] Failed to allocate adapter memory", CINGULATE_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cingulate_create: adapter is NULL");
        return NULL;
    }

    /* Set configuration */
    if (config) {
        adapter->config = *config;
        LOG_DEBUG("[%s] Using provided configuration", CINGULATE_LOG_MODULE);
    } else {
        adapter->config = cingulate_default_config();
        LOG_DEBUG("[%s] Using default configuration", CINGULATE_LOG_MODULE);
    }

    /* Allocate response options buffer */
    adapter->response_options = (cingulate_response_option_t*)nimcp_calloc(
        adapter->config.response_options, sizeof(cingulate_response_option_t));
    if (!adapter->response_options) {
        LOG_ERROR("[%s] Failed to allocate response options", CINGULATE_LOG_MODULE);
        cingulate_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cingulate_create: adapter->response_options is NULL");
        return NULL;
    }

    /* Initialize state */
    adapter->status = CINGULATE_STATUS_IDLE;
    adapter->last_error_code = CINGULATE_ERROR_NONE;
    adapter->current_time_ms = 0.0;

    adapter->current_control_level = 0.5f;  /* Baseline control */
    adapter->self_relevance_baseline = 0.3f;
    adapter->internal_focus_level = 0.5f;
    adapter->dmn_active = false;

    adapter->next_error_id = 1;

    /* Initialize bio-async communication */
    adapter->bio_ctx = NULL;
    adapter->default_channel = adapter->config.default_channel;

    if (adapter->config.enable_bio_async && bio_router_is_initialized()) {
        LOG_DEBUG("[%s] Registering with bio-async router", CINGULATE_LOG_MODULE);

        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_CINGULATE,
            .module_name = "cingulate_cortex",
            .inbox_capacity = 64,
            .user_data = adapter
        };

        adapter->bio_ctx = bio_router_register_module(&bio_info);
        if (adapter->bio_ctx) {
            LOG_INFO("[%s] Bio-async handlers registered successfully", CINGULATE_LOG_MODULE);
        } else {
            LOG_WARNING("[%s] Failed to register with bio-async router", CINGULATE_LOG_MODULE);
        }
    }

    LOG_INFO("[%s] Cingulate cortex adapter created successfully", CINGULATE_LOG_MODULE);
    return adapter;
}

void cingulate_destroy(cingulate_adapter_t* adapter) {
    if (!adapter) return;

    LOG_INFO("[%s] Destroying cingulate cortex adapter", CINGULATE_LOG_MODULE);

    /* Unregister from bio-async */
    if (adapter->bio_ctx) {
        bio_router_unregister_module(adapter->bio_ctx);
        adapter->bio_ctx = NULL;
    }

    /* Free response options */
    if (adapter->response_options) {
        nimcp_free(adapter->response_options);
    }

    /* Free conflict history */
    conflict_entry_t* conflict = adapter->conflict_history;
    while (conflict) {
        conflict_entry_t* next = conflict->next;
        nimcp_free(conflict);
        conflict = next;
    }

    /* Free error history */
    error_entry_t* error = adapter->error_history;
    while (error) {
        error_entry_t* next = error->next;
        nimcp_free(error);
        error = next;
    }

    nimcp_free(adapter);
    LOG_DEBUG("[%s] Cingulate cortex adapter destroyed", CINGULATE_LOG_MODULE);
}

bool cingulate_reset(cingulate_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_reset: adapter is NULL");
        return false;
    }

    LOG_DEBUG("[%s] Resetting adapter state", CINGULATE_LOG_MODULE);

    /* Clear response monitoring */
    memset(adapter->response_options, 0,
           adapter->config.response_options * sizeof(cingulate_response_option_t));
    adapter->num_response_options = 0;
    adapter->active_response_count = 0;
    adapter->monitoring_active = false;

    /* Clear conflict state */
    adapter->has_pending_conflict = false;
    adapter->conflict_accumulator = 0.0f;

    /* Clear error state */
    adapter->has_pending_error = false;
    adapter->error_accumulator = 0.0f;

    /* Reset control level */
    adapter->current_control_level = 0.5f;

    /* Reset PCC state */
    adapter->internal_focus_level = 0.5f;
    adapter->dmn_active = false;
    adapter->autobio_activation = 0.0f;

    /* Reset emotion state */
    adapter->emotional_valence = 0.0f;
    adapter->emotional_arousal = 0.5f;
    adapter->pain_level = 0.0f;

    /* Reset status */
    adapter->status = CINGULATE_STATUS_IDLE;
    adapter->last_error_code = CINGULATE_ERROR_NONE;

    return true;
}

/*=============================================================================
 * CONFLICT MONITORING (ACC - Dorsal)
 *===========================================================================*/

bool cingulate_begin_monitoring(cingulate_adapter_t* adapter, uint32_t num_options) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_begin_monitoring: adapter is NULL");
        return false;
    }

    if (num_options == 0) {
        set_error(adapter, CINGULATE_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cingulate_begin_monitoring: num_options is 0");
        return false;
    }

    /* Clamp to maximum allowed options */
    if (num_options > adapter->config.response_options) {
        num_options = adapter->config.response_options;
    }

    /* Reset response options */
    memset(adapter->response_options, 0,
           adapter->config.response_options * sizeof(cingulate_response_option_t));

    adapter->num_response_options = num_options;
    adapter->active_response_count = 0;
    adapter->monitoring_active = true;
    adapter->has_pending_conflict = false;

    /* Initialize options with IDs */
    for (uint32_t i = 0; i < num_options; i++) {
        adapter->response_options[i].option_id = i;
        adapter->response_options[i].prior_probability = 1.0f / (float)num_options;
    }

    adapter->status = CINGULATE_STATUS_MONITORING;

    LOG_DEBUG("[%s] Begin monitoring %u response options", CINGULATE_LOG_MODULE, num_options);
    return true;
}

bool cingulate_update_response(cingulate_adapter_t* adapter,
                                const cingulate_response_option_t* option) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_update_response: adapter is NULL");
        return false;
    }
    if (!option) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_update_response: option is NULL");
        set_error(adapter, CINGULATE_ERROR_INVALID_INPUT);
        return false;
    }

    if (!adapter->monitoring_active) {
        set_error(adapter, CINGULATE_ERROR_NOT_INITIALIZED);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_begin_monitoring: adapter->monitoring_active is NULL");
        return false;
    }

    if (option->option_id >= adapter->num_response_options) {
        set_error(adapter, CINGULATE_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cingulate_begin_monitoring: capacity exceeded");
        return false;
    }

    /* Update the response option */
    adapter->response_options[option->option_id] = *option;

    /* Track active responses */
    uint32_t active = 0;
    for (uint32_t i = 0; i < adapter->num_response_options; i++) {
        if (adapter->response_options[i].activation > 0.1f) {
            active++;
        }
    }
    adapter->active_response_count = active;

    return true;
}

bool cingulate_evaluate_conflict(cingulate_adapter_t* adapter,
                                  cingulate_conflict_t* conflict) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_evaluate_conflict: adapter is NULL");
        return false;
    }

    if (!adapter->monitoring_active || adapter->num_response_options < 2) {
        return false;
    }

    /* Compute conflict level using Botvinick model */
    float conflict_level = compute_conflict(adapter->response_options,
                                             adapter->num_response_options);

    /* Modulate by emotional state (anxiety increases conflict sensitivity) */
    conflict_level *= (1.0f + 0.5f * adapter->emotional_arousal);

    /* Check if above threshold */
    if (conflict_level < adapter->config.conflict_threshold) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cingulate_begin_monitoring: validation failed");
        return false;  /* No significant conflict */
    }

    /* Find the two most activated competing responses */
    uint32_t best_idx = 0;
    uint32_t second_idx = 1;
    float best_activation = adapter->response_options[0].activation;
    float second_activation = adapter->response_options[1].activation;

    if (second_activation > best_activation) {
        uint32_t tmp_idx = best_idx;
        best_idx = second_idx;
        second_idx = tmp_idx;
        float tmp_act = best_activation;
        best_activation = second_activation;
        second_activation = tmp_act;
    }

    for (uint32_t i = 2; i < adapter->num_response_options; i++) {
        float act = adapter->response_options[i].activation;
        if (act > best_activation) {
            second_idx = best_idx;
            second_activation = best_activation;
            best_idx = i;
            best_activation = act;
        } else if (act > second_activation) {
            second_idx = i;
            second_activation = act;
        }
    }

    /* Create conflict event */
    cingulate_conflict_t new_conflict;
    memset(&new_conflict, 0, sizeof(new_conflict));

    new_conflict.conflict_id = adapter->conflict_count + 1;
    new_conflict.option_a_id = best_idx;
    new_conflict.option_b_id = second_idx;
    new_conflict.conflict_level = conflict_level;
    new_conflict.energy = sqrtf(conflict_level);  /* N2 amplitude analog */
    new_conflict.timestamp_ms = adapter->current_time_ms;
    new_conflict.requires_control = (conflict_level > adapter->config.adjustment_threshold);

    /* Store conflict */
    adapter->last_conflict = new_conflict;
    adapter->has_pending_conflict = true;
    adapter->conflict_count++;
    adapter->conflict_accumulator += conflict_level;

    /* Update statistics */
    adapter->stats.conflicts_detected++;
    adapter->stats.avg_conflict_level =
        (adapter->stats.avg_conflict_level * (adapter->stats.conflicts_detected - 1) +
         conflict_level) / adapter->stats.conflicts_detected;
    if (conflict_level > adapter->stats.max_conflict_level) {
        adapter->stats.max_conflict_level = conflict_level;
    }

    adapter->status = CINGULATE_STATUS_CONFLICT_DETECTED;

    /* Invoke callback */
    if (adapter->conflict_callback) {
        adapter->conflict_callback(&new_conflict, adapter->conflict_user_data);
    }

    if (conflict) {
        *conflict = new_conflict;
    }

    LOG_DEBUG("[%s] Conflict detected: level=%.3f, options=%u vs %u",
              CINGULATE_LOG_MODULE, conflict_level, best_idx, second_idx);

    return true;
}

bool cingulate_requires_control(const cingulate_adapter_t* adapter,
                                 const cingulate_conflict_t* conflict) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_requires_control: adapter is NULL");
        return false;
    }
    if (!conflict) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_requires_control: conflict is NULL");
        return false;
    }
    return conflict->requires_control;
}

/*=============================================================================
 * ERROR DETECTION (ACC - Rostral)
 *===========================================================================*/

bool cingulate_report_response(cingulate_adapter_t* adapter,
                                uint32_t executed_option,
                                uint32_t intended_option) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_report_response: adapter is NULL");
        return false;
    }

    /* Check for error (mismatch between executed and intended) */
    if (intended_option != 0 && executed_option != intended_option) {
        /* Create error event */
        cingulate_error_event_t error;
        memset(&error, 0, sizeof(error));

        error.error_id = adapter->next_error_id++;
        error.executed_option = executed_option;
        error.intended_option = intended_option;
        error.error_magnitude = 1.0f;  /* Binary error */
        error.ern_amplitude = compute_ern_amplitude(error.error_magnitude);
        error.pe_amplitude = compute_pe_amplitude(error.error_magnitude,
                                                   adapter->config.error_threshold);
        error.timestamp_ms = adapter->current_time_ms;
        error.is_conscious = (error.pe_amplitude > 0.0f);

        /* Store error */
        adapter->last_error = error;
        adapter->has_pending_error = true;
        adapter->error_count++;
        adapter->error_accumulator += error.error_magnitude;

        /* Update statistics */
        adapter->stats.errors_detected++;
        adapter->stats.avg_error_magnitude =
            (adapter->stats.avg_error_magnitude * (adapter->stats.errors_detected - 1) +
             error.error_magnitude) / adapter->stats.errors_detected;
        adapter->stats.avg_ern_amplitude =
            (adapter->stats.avg_ern_amplitude * (adapter->stats.errors_detected - 1) +
             fabsf(error.ern_amplitude)) / adapter->stats.errors_detected;
        if (error.is_conscious) {
            adapter->stats.error_awareness_rate =
                (adapter->stats.error_awareness_rate * (adapter->stats.errors_detected - 1) + 1.0f) /
                adapter->stats.errors_detected;
        } else {
            adapter->stats.error_awareness_rate =
                (adapter->stats.error_awareness_rate * (adapter->stats.errors_detected - 1)) /
                adapter->stats.errors_detected;
        }

        adapter->status = CINGULATE_STATUS_ERROR_DETECTED;

        /* Invoke callback */
        if (adapter->error_callback) {
            adapter->error_callback(&error, adapter->error_user_data);
        }

        LOG_DEBUG("[%s] Error detected: executed=%u, intended=%u, ERN=%.3f",
                  CINGULATE_LOG_MODULE, executed_option, intended_option, error.ern_amplitude);
    }

    /* Update stats */
    adapter->stats.responses_monitored++;
    adapter->monitoring_active = false;

    return true;
}

bool cingulate_report_outcome(cingulate_adapter_t* adapter,
                               float outcome,
                               float expected) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: adapter is NULL");
        return false;
    }

    float error_magnitude = fabsf(outcome - expected);

    /* Check if error significant */
    if (error_magnitude > adapter->config.error_threshold) {
        /* Create error event */
        cingulate_error_event_t error;
        memset(&error, 0, sizeof(error));

        error.error_id = adapter->next_error_id++;
        error.executed_option = 0;  /* Not a response error */
        error.intended_option = 0;
        error.error_magnitude = error_magnitude;
        error.ern_amplitude = compute_ern_amplitude(error_magnitude);
        error.pe_amplitude = compute_pe_amplitude(error_magnitude, adapter->config.error_threshold);
        error.timestamp_ms = adapter->current_time_ms;
        error.is_conscious = (error.pe_amplitude > 0.0f);

        /* Store error */
        adapter->last_error = error;
        adapter->has_pending_error = true;
        adapter->error_count++;
        adapter->error_accumulator += error_magnitude;

        /* Update statistics */
        adapter->stats.errors_detected++;

        adapter->status = CINGULATE_STATUS_ERROR_DETECTED;

        /* Invoke callback */
        if (adapter->error_callback) {
            adapter->error_callback(&error, adapter->error_user_data);
        }

        LOG_DEBUG("[%s] Outcome error: magnitude=%.3f, ERN=%.3f",
                  CINGULATE_LOG_MODULE, error_magnitude, error.ern_amplitude);

        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: operation failed");
    return false;  /* No significant error */
}

bool cingulate_get_last_error(const cingulate_adapter_t* adapter,
                               cingulate_error_event_t* error) {
    if (!adapter || !error) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (adapter, error)");
        return false;
    }
    if (!adapter->has_pending_error) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: adapter->has_pending_error is NULL");
        return false;
    }

    *error = adapter->last_error;
    return true;
}

bool cingulate_error_is_conscious(const cingulate_adapter_t* adapter,
                                   uint32_t error_id) {
    if (!adapter) {
        return false;
    }

    /* Check last error first */
    if (adapter->last_error.error_id == error_id) {
        return adapter->last_error.is_conscious;
    }

    /* Search history */
    error_entry_t* entry = adapter->error_history;
    while (entry) {
        if (entry->error.error_id == error_id) {
            return entry->error.is_conscious;
        }
        entry = entry->next;
    }

    return false;
}

/*=============================================================================
 * COGNITIVE CONTROL (ACC Output)
 *===========================================================================*/

bool cingulate_generate_control_signal(cingulate_adapter_t* adapter,
                                        cingulate_control_signal_t* signal) {
    if (!adapter || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (adapter, signal)");
        return false;
    }

    memset(signal, 0, sizeof(cingulate_control_signal_t));

    /* Compute control level based on recent conflicts and errors */
    float conflict_contribution = adapter->conflict_accumulator * 0.5f;
    float error_contribution = adapter->error_accumulator * 0.7f;

    /* Modulate by emotional arousal (high arousal increases control) */
    float arousal_factor = 1.0f + 0.3f * adapter->emotional_arousal;

    float new_control = adapter->current_control_level +
                        (conflict_contribution + error_contribution) * arousal_factor;

    /* Clamp to [0, 1] */
    if (new_control > 1.0f) new_control = 1.0f;
    if (new_control < 0.0f) new_control = 0.0f;

    /* Apply adaptation decay */
    float decay_factor = expf(-1.0f / adapter->config.adaptation_decay_ms);
    adapter->conflict_accumulator *= decay_factor;
    adapter->error_accumulator *= decay_factor;

    /* Fill signal */
    signal->control_level = new_control;
    signal->adjustment_magnitude = fabsf(new_control - adapter->current_control_level);
    signal->threshold_shift = (new_control - 0.5f) * 0.2f;  /* Shift response threshold */
    signal->attention_boost = fmaxf(0.0f, new_control - 0.5f);
    signal->target_module = 0;  /* Broadcast */
    signal->apply_inhibition = (adapter->has_pending_conflict &&
                                adapter->last_conflict.requires_control);
    signal->apply_enhancement = (new_control > 0.7f);

    /* Update state */
    adapter->current_control_level = new_control;
    adapter->last_control_signal = *signal;
    adapter->has_pending_conflict = false;
    adapter->has_pending_error = false;

    adapter->stats.adjustments_made++;
    adapter->stats.current_control_level = new_control;

    adapter->status = CINGULATE_STATUS_ADJUSTING;

    /* Invoke callback */
    if (adapter->control_callback) {
        adapter->control_callback(signal, adapter->control_user_data);
    }

    LOG_DEBUG("[%s] Control signal generated: level=%.3f, adjustment=%.3f",
              CINGULATE_LOG_MODULE, new_control, signal->adjustment_magnitude);

    return true;
}

bool cingulate_set_control_callback(cingulate_adapter_t* adapter,
                                     cingulate_control_callback_t callback,
                                     void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: adapter is NULL");
        return false;
    }
    adapter->control_callback = callback;
    adapter->control_user_data = user_data;
    return true;
}

float cingulate_get_control_level(const cingulate_adapter_t* adapter) {
    if (!adapter) return 0.0f;
    return adapter->current_control_level;
}

/*=============================================================================
 * SELF-REFERENTIAL PROCESSING (PCC)
 *===========================================================================*/

bool cingulate_evaluate_self_relevance(cingulate_adapter_t* adapter,
                                        const float* features,
                                        uint32_t num_features,
                                        cingulate_self_reference_t* result) {
    if (!adapter || !features || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_get_control_level: required parameter is NULL (adapter, features, result)");
        return false;
    }

    memset(result, 0, sizeof(cingulate_self_reference_t));

    /* Compute self-relevance (simplified model) */
    float feature_sum = 0.0f;
    for (uint32_t i = 0; i < num_features; i++) {
        feature_sum += fabsf(features[i]);
    }
    float feature_mean = (num_features > 0) ? feature_sum / (float)num_features : 0.0f;

    /* Self-relevance is higher for internally-focused, emotional content */
    result->self_relevance = adapter->self_relevance_baseline + feature_mean * 0.5f;
    if (result->self_relevance > 1.0f) result->self_relevance = 1.0f;

    /* Modulate by emotional state */
    result->self_relevance *= (1.0f + 0.3f * fabsf(adapter->emotional_valence));

    /* Estimate autobiographical match */
    result->autobio_match = result->self_relevance * 0.8f;

    /* Internal focus level */
    result->internal_focus = adapter->internal_focus_level;

    /* Mentalizing engagement */
    result->mentalizing_level = fmaxf(0.0f, result->self_relevance - 0.3f);

    /* Check DMN state */
    result->is_default_mode = adapter->dmn_active;

    /* Update PCC state */
    adapter->autobio_activation = result->autobio_match;
    adapter->last_self_reference = *result;

    adapter->stats.self_references++;
    adapter->status = CINGULATE_STATUS_SELF_REFERENTIAL;

    return true;
}

bool cingulate_is_default_mode(const cingulate_adapter_t* adapter) {
    if (!adapter) {
        return false;
    }
    return adapter->dmn_active;
}

bool cingulate_request_autobio_memory(cingulate_adapter_t* adapter,
                                       const float* query_features,
                                       uint32_t num_features,
                                       uint32_t* memory_id) {
    if (!adapter || !memory_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cingulate_is_default_mode: required parameter is NULL (adapter, memory_id)");
        return false;
    }

    /* Self-referential memory retrieval: use cingulate activation pattern
     * to generate a pseudo-memory ID based on feature similarity.
     * In a full implementation, this would query autobiographical_memory_t. */
    (void)query_features;
    (void)num_features;

    /* Use the adapter's current conflict level as a retrieval cue.
     * Higher conflict = more likely to retrieve a relevant memory. */
    float avg_conflict = (adapter->conflict_count > 0) ?
        adapter->conflict_accumulator / (float)adapter->conflict_count : 0.0f;
    if (avg_conflict > 0.3f && adapter->status == CINGULATE_STATUS_SELF_REFERENTIAL) {
        /* Simulated memory retrieval based on conflict state */
        *memory_id = (uint32_t)(avg_conflict * 1000.0f);
        LOG_DEBUG("[%s] Autobiographical memory retrieved (conflict=%.2f, id=%u)",
                  CINGULATE_LOG_MODULE, avg_conflict, *memory_id);
        return true;
    }

    *memory_id = 0;
    return false;  /* No relevant memory found for current state */
}

/*=============================================================================
 * EMOTION-COGNITION INTEGRATION
 *===========================================================================*/

bool cingulate_integrate_emotion(cingulate_adapter_t* adapter,
                                  float valence,
                                  float arousal) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_is_default_mode: adapter is NULL");
        return false;
    }

    /* Clamp values */
    if (valence < -1.0f) valence = -1.0f;
    if (valence > 1.0f) valence = 1.0f;
    if (arousal < 0.0f) arousal = 0.0f;
    if (arousal > 1.0f) arousal = 1.0f;

    /* Exponential moving average for smooth integration */
    const float alpha = 0.3f;
    adapter->emotional_valence = alpha * valence + (1.0f - alpha) * adapter->emotional_valence;
    adapter->emotional_arousal = alpha * arousal + (1.0f - alpha) * adapter->emotional_arousal;

    /* High arousal shifts to external focus, low arousal allows DMN */
    if (arousal > 0.7f) {
        adapter->internal_focus_level = fmaxf(0.0f, adapter->internal_focus_level - 0.1f);
        adapter->dmn_active = false;
    } else if (arousal < 0.3f) {
        adapter->internal_focus_level = fminf(1.0f, adapter->internal_focus_level + 0.1f);
        if (adapter->internal_focus_level > 0.7f) {
            adapter->dmn_active = true;
        }
    }

    return true;
}

bool cingulate_report_pain(cingulate_adapter_t* adapter,
                            float pain_level,
                            bool is_physical) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_is_default_mode: adapter is NULL");
        return false;
    }

    /* Clamp pain level */
    if (pain_level < 0.0f) pain_level = 0.0f;
    if (pain_level > 1.0f) pain_level = 1.0f;

    adapter->pain_level = pain_level;

    /* Pain increases arousal and shifts valence negative */
    adapter->emotional_arousal = fminf(1.0f, adapter->emotional_arousal + pain_level * 0.5f);
    adapter->emotional_valence = fmaxf(-1.0f, adapter->emotional_valence - pain_level * 0.3f);

    /* High pain may trigger control signal */
    if (pain_level > 0.5f) {
        adapter->error_accumulator += pain_level * 0.5f;
    }

    LOG_DEBUG("[%s] Pain reported: level=%.3f, physical=%s",
              CINGULATE_LOG_MODULE, pain_level, is_physical ? "true" : "false");

    return true;
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

cingulate_status_t cingulate_get_status(const cingulate_adapter_t* adapter) {
    if (!adapter) return CINGULATE_STATUS_ERROR;
    return adapter->status;
}

cingulate_error_t cingulate_get_last_error_code(const cingulate_adapter_t* adapter) {
    if (!adapter) return CINGULATE_ERROR_INTERNAL;
    return adapter->last_error_code;
}

const char* cingulate_error_string(cingulate_error_t error) {
    switch (error) {
        case CINGULATE_ERROR_NONE: return "No error";
        case CINGULATE_ERROR_INVALID_INPUT: return "Invalid input";
        case CINGULATE_ERROR_CONFLICT_OVERFLOW: return "Conflict buffer overflow";
        case CINGULATE_ERROR_ERROR_OVERFLOW: return "Error buffer overflow";
        case CINGULATE_ERROR_THRESHOLD_INVALID: return "Invalid threshold";
        case CINGULATE_ERROR_BUFFER_OVERFLOW: return "Buffer overflow";
        case CINGULATE_ERROR_NOT_INITIALIZED: return "Not initialized";
        case CINGULATE_ERROR_INTERNAL: return "Internal error";
        default: return "Unknown error";
    }
}

const char* cingulate_status_string(cingulate_status_t status) {
    switch (status) {
        case CINGULATE_STATUS_IDLE: return "Idle";
        case CINGULATE_STATUS_MONITORING: return "Monitoring";
        case CINGULATE_STATUS_CONFLICT_DETECTED: return "Conflict detected";
        case CINGULATE_STATUS_ERROR_DETECTED: return "Error detected";
        case CINGULATE_STATUS_ADJUSTING: return "Adjusting";
        case CINGULATE_STATUS_SELF_REFERENTIAL: return "Self-referential";
        case CINGULATE_STATUS_READY: return "Ready";
        case CINGULATE_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}

bool cingulate_get_stats(const cingulate_adapter_t* adapter, cingulate_stats_t* stats) {
    if (!adapter || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_get_stats: required parameter is NULL (adapter, stats)");
        return false;
    }
    *stats = adapter->stats;
    return true;
}

bool cingulate_get_config(const cingulate_adapter_t* adapter, cingulate_config_t* config) {
    if (!adapter || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_get_config: required parameter is NULL (adapter, config)");
        return false;
    }
    *config = adapter->config;
    return true;
}

/*=============================================================================
 * CALLBACK REGISTRATION
 *===========================================================================*/

bool cingulate_set_conflict_callback(cingulate_adapter_t* adapter,
                                      cingulate_conflict_callback_t callback,
                                      void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_get_config: adapter is NULL");
        return false;
    }
    adapter->conflict_callback = callback;
    adapter->conflict_user_data = user_data;
    return true;
}

bool cingulate_set_error_callback(cingulate_adapter_t* adapter,
                                   cingulate_error_callback_t callback,
                                   void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_get_config: adapter is NULL");
        return false;
    }
    adapter->error_callback = callback;
    adapter->error_user_data = user_data;
    return true;
}

bool cingulate_set_event_callback(cingulate_adapter_t* adapter,
                                   cingulate_event_callback_t callback,
                                   void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_get_config: adapter is NULL");
        return false;
    }
    adapter->event_callback = callback;
    adapter->event_user_data = user_data;
    return true;
}

/*=============================================================================
 * BIO-ASYNC COMMUNICATION
 *===========================================================================*/

bio_module_context_t cingulate_get_bio_context(cingulate_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_get_bio_context: adapter is NULL");
        return NULL;
    }
    return adapter->bio_ctx;
}

uint32_t cingulate_process_bio_messages(cingulate_adapter_t* adapter, uint32_t max_messages) {
    if (!adapter || !adapter->bio_ctx) return 0;

    uint32_t processed = bio_router_process_inbox(adapter->bio_ctx, max_messages);
    if (processed > 0) {
        LOG_DEBUG("[%s] Processed %u bio-async messages", CINGULATE_LOG_MODULE, processed);
    }
    return processed;
}

nimcp_error_t cingulate_broadcast_conflict(cingulate_adapter_t* adapter,
                                            const cingulate_conflict_t* conflict) {
    if (!adapter || !conflict) {
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    if (!adapter->bio_ctx) {
        return NIMCP_SUCCESS;  /* Not an error if bio-async disabled */
    }

    /* Create conflict broadcast message */
    bio_message_header_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = BIO_MSG_CONFLICT_DETECTED;
    msg.source_module = BIO_MODULE_CINGULATE;
    msg.target_module = 0;  /* Broadcast */
    msg.payload_size = sizeof(*conflict);
    msg.channel = adapter->default_channel;
    msg.flags = BIO_MSG_FLAG_BROADCAST;

    LOG_INFO("[%s] Broadcasting conflict: level=%.3f", CINGULATE_LOG_MODULE, conflict->conflict_level);

    return bio_router_broadcast(adapter->bio_ctx, &msg, sizeof(msg));
}

nimcp_error_t cingulate_broadcast_error(cingulate_adapter_t* adapter,
                                         const cingulate_error_event_t* error) {
    if (!adapter || !error) {
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    if (!adapter->bio_ctx) {
        return NIMCP_SUCCESS;  /* Not an error if bio-async disabled */
    }

    /* Create error broadcast message */
    bio_message_header_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = BIO_MSG_ERROR_DETECTED;
    msg.source_module = BIO_MODULE_CINGULATE;
    msg.target_module = 0;  /* Broadcast */
    msg.payload_size = sizeof(*error);
    msg.channel = adapter->default_channel;
    msg.flags = BIO_MSG_FLAG_BROADCAST;

    LOG_INFO("[%s] Broadcasting error: magnitude=%.3f, ERN=%.3f",
             CINGULATE_LOG_MODULE, error->error_magnitude, error->ern_amplitude);

    return bio_router_broadcast(adapter->bio_ctx, &msg, sizeof(msg));
}
