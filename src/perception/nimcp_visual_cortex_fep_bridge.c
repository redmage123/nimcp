/**
 * @file nimcp_visual_cortex_fep_bridge.c
 * @brief Free Energy Principle - Visual Cortex Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 */

#include "perception/nimcp_visual_cortex_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>

#define LOG_MODULE_VISUAL_FEP "[VISUAL_FEP]"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(visual_cortex_fep_bridge)

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

/**
 * WHAT: Provide default configuration for visual-FEP bridge
 * WHY:  Simplify initialization with reasonable defaults
 * HOW:  Set biologically-plausible thresholds and enable all features
 */
int visual_cortex_fep_bridge_default_config(visual_cortex_fep_config_t* config) {
    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    config->prediction_error_threshold = VISUAL_FEP_PE_THRESHOLD_MEDIUM;
    config->precision_gain_factor = VISUAL_FEP_PRECISION_GAIN_DEFAULT;
    config->attention_boost_factor = VISUAL_FEP_ATTENTION_BOOST_MEDIUM;

    config->enable_top_down_predictions = true;
    config->enable_precision_attention = true;
    config->enable_active_vision = true;
    config->enable_visual_pe_updates = true;

    config->visual_precision_sensitivity = 1.0f;
    config->prediction_gain_sensitivity = 0.5f;
    config->pe_propagation_rate = 0.1f;

    return 0;
}

/**
 * WHAT: Create visual cortex FEP bridge
 * WHY:  Initialize bidirectional integration between visual cortex and FEP
 * HOW:  Allocate memory, initialize state, create mutex
 */
visual_cortex_fep_bridge_t* visual_cortex_fep_bridge_create(
    const visual_cortex_fep_config_t* config
) {
    visual_cortex_fep_bridge_t* bridge = (visual_cortex_fep_bridge_t*)
        nimcp_malloc(sizeof(visual_cortex_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR(LOG_MODULE_VISUAL_FEP " Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    memset(bridge, 0, sizeof(visual_cortex_fep_bridge_t));

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        visual_cortex_fep_bridge_default_config(&bridge->config);
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "visual_cortex_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR(LOG_MODULE_VISUAL_FEP " Failed to create mutex");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_cortex_fep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize state */
    bridge->state.visual_precision = 1.0f;
    bridge->effects.gabor_gain = 1.0f;
    bridge->effects.attention_boost = 1.0f;
    bridge->effects.feature_gain = 1.0f;
    bridge->effects.precision_gain_modifier = 1.0f;

    NIMCP_LOGGING_INFO(LOG_MODULE_VISUAL_FEP " Bridge created successfully");
    return bridge;
}

/**
 * WHAT: Destroy visual cortex FEP bridge
 * WHY:  Free all allocated resources
 * HOW:  Disconnect bio-async, destroy mutex, free memory
 */
void visual_cortex_fep_bridge_destroy(visual_cortex_fep_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        visual_cortex_fep_bridge_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO(LOG_MODULE_VISUAL_FEP " Bridge destroyed");
}

/* ============================================================================
 * Connection Implementation
 * ============================================================================ */

/**
 * WHAT: Connect FEP system to bridge
 * WHY:  Enable FEP state queries and updates
 * HOW:  Store FEP pointer with mutex protection
 */
int visual_cortex_fep_bridge_connect_fep(
    visual_cortex_fep_bridge_t* bridge,
    fep_system_t* fep
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(fep, NIMCP_ERROR_NULL_POINTER, "fep system is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO(LOG_MODULE_VISUAL_FEP " FEP system connected");
    return 0;
}

/**
 * WHAT: Connect visual cortex to bridge
 * WHY:  Enable visual processing monitoring and modulation
 * HOW:  Store visual cortex pointer with mutex protection
 */
int visual_cortex_fep_bridge_connect_visual_cortex(
    visual_cortex_fep_bridge_t* bridge,
    visual_cortex_t* visual
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(visual, NIMCP_ERROR_NULL_POINTER, "visual cortex is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->visual_cortex = visual;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO(LOG_MODULE_VISUAL_FEP " Visual cortex connected");
    return 0;
}

/* ============================================================================
 * FEP → Visual Implementation
 * ============================================================================ */

/**
 * WHAT: Apply FEP predictions to visual gain
 * WHY:  Top-down predictions modulate sensory processing
 * HOW:  Query FEP beliefs, convert to Gabor gain modulation
 */
int visual_cortex_fep_apply_predictions(visual_cortex_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(bridge->fep_system, NIMCP_ERROR_INVALID_STATE, "fep_system not connected");
    if (!bridge->config.enable_top_down_predictions) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute prediction gain (simplified - would query FEP beliefs) */
    float prediction_confidence = 0.8f; /* Placeholder */
    bridge->effects.gabor_gain = 1.0f +
        (prediction_confidence * bridge->config.prediction_gain_sensitivity);

    /* Predictive suppression of expected features */
    bridge->effects.prediction_suppression = prediction_confidence * 0.3f;

    /* Enhancement of novel features */
    bridge->effects.novelty_enhancement = (1.0f - prediction_confidence) * 0.5f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Apply FEP precision to visual attention
 * WHY:  Precision weights prediction errors
 * HOW:  Scale attention map by precision
 */
int visual_cortex_fep_apply_precision(visual_cortex_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(bridge->fep_system, NIMCP_ERROR_INVALID_STATE, "fep_system not connected");
    if (!bridge->config.enable_precision_attention) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get precision from FEP (placeholder) */
    float fep_precision = 1.0f;

    /* Convert precision to attention boost */
    bridge->effects.attention_boost = VISUAL_FEP_ATTENTION_BOOST_LOW +
        (fep_precision * bridge->config.attention_boost_factor);

    /* Clamp to valid range */
    if (bridge->effects.attention_boost > VISUAL_FEP_ATTENTION_BOOST_HIGH) {
        bridge->effects.attention_boost = VISUAL_FEP_ATTENTION_BOOST_HIGH;
    }

    bridge->state.visual_precision = fep_precision;
    bridge->state.attention_precision = fep_precision *
        bridge->config.visual_precision_sensitivity;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Generate saccade target via active inference
 * WHY:  Eye movements minimize expected free energy
 * HOW:  Find high-uncertainty visual regions
 */
int visual_cortex_fep_generate_saccade(
    visual_cortex_fep_bridge_t* bridge,
    float* target_x,
    float* target_y
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(target_x, NIMCP_ERROR_NULL_POINTER, "target_x is NULL");
    NIMCP_FEP_CHECK_THROW(target_y, NIMCP_ERROR_NULL_POINTER, "target_y is NULL");
    NIMCP_FEP_CHECK_THROW(bridge->fep_system, NIMCP_ERROR_INVALID_STATE, "fep_system not connected");
    NIMCP_FEP_CHECK_THROW(bridge->config.enable_active_vision, NIMCP_ERROR_INVALID_STATE, "active_vision not enabled");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Simplified: move toward high-PE region (would use attention map) */
    *target_x = bridge->state.saccade_target_x;
    *target_y = bridge->state.saccade_target_y;

    bridge->stats.saccades_generated++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Visual → FEP Implementation
 * ============================================================================ */

/**
 * WHAT: Compute visual prediction error
 * WHY:  PE drives belief updates in FEP
 * HOW:  Compare visual features to FEP predictions
 */
int visual_cortex_fep_compute_prediction_error(
    visual_cortex_fep_bridge_t* bridge,
    const float* visual_features,
    uint32_t num_features,
    float* prediction_error
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(visual_features, NIMCP_ERROR_NULL_POINTER, "visual_features is NULL");
    NIMCP_FEP_CHECK_THROW(prediction_error, NIMCP_ERROR_NULL_POINTER, "prediction_error is NULL");
    NIMCP_FEP_CHECK_THROW(bridge->fep_system, NIMCP_ERROR_INVALID_STATE, "fep_system not connected");
    if (num_features == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_fep_compute_prediction_error: num_features is 0");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Simplified PE computation (would compare to FEP predictions) */
    float pe_sum = 0.0f;
    for (uint32_t i = 0; i < num_features; i++) {
        float expected = 0.0f; /* Placeholder: get from FEP */
        float error = visual_features[i] - expected;
        pe_sum += error * error;
    }

    *prediction_error = sqrtf(pe_sum / (float)num_features);

    /* Update state */
    bridge->state.current_visual_pe = *prediction_error;
    float new_avg_vis_pe = 0.9f * bridge->state.avg_visual_pe + 0.1f * (*prediction_error);
    if (isfinite(new_avg_vis_pe)) bridge->state.avg_visual_pe = new_avg_vis_pe;

    if (*prediction_error > bridge->state.max_visual_pe) {
        bridge->state.max_visual_pe = *prediction_error;
    }

    /* Check for high PE events */
    if (*prediction_error > bridge->config.prediction_error_threshold) {
        bridge->state.pe_events++;
        bridge->stats.high_pe_events++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Report visual observations to FEP
 * WHY:  Visual features drive inference
 * HOW:  Convert V1 features to FEP observation format
 */
int visual_cortex_fep_report_observations(
    visual_cortex_fep_bridge_t* bridge,
    const float* visual_features,
    uint32_t num_features
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(visual_features, NIMCP_ERROR_NULL_POINTER, "visual_features is NULL");
    NIMCP_FEP_CHECK_THROW(bridge->fep_system, NIMCP_ERROR_INVALID_STATE, "fep_system not connected");
    if (!bridge->config.enable_visual_pe_updates) return 0;
    if (num_features == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_fep_report_observations: num_features is 0");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Report to FEP system (would call FEP observation API) */
    bridge->state.frames_processed++;
    bridge->stats.total_frames_processed++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Report visual novelty to FEP
 * WHY:  Novel patterns trigger learning
 * HOW:  Detect high PE and signal FEP
 */
int visual_cortex_fep_report_novelty(
    visual_cortex_fep_bridge_t* bridge,
    float novelty_score
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(bridge->fep_system, NIMCP_ERROR_INVALID_STATE, "fep_system not connected");
    if (novelty_score < 0.0f || novelty_score > 1.0f) {
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (novelty_score > 0.7f) {
        bridge->state.novelty_detected = true;
        bridge->stats.novelty_events++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

/**
 * WHAT: Update visual-FEP bridge state
 * WHY:  Maintain bidirectional coupling
 * HOW:  Apply predictions, compute PE, update statistics
 */
int visual_cortex_fep_bridge_update(
    visual_cortex_fep_bridge_t* bridge,
    uint64_t delta_ms
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Apply FEP effects to visual processing */
    visual_cortex_fep_apply_predictions(bridge);
    visual_cortex_fep_apply_precision(bridge);

    /* Update statistics */
    nimcp_mutex_lock(bridge->base.mutex);

    bridge->stats.avg_prediction_error = bridge->state.avg_visual_pe;
    bridge->stats.prediction_accuracy =
        1.0f / (1.0f + bridge->state.avg_visual_pe);
    bridge->stats.avg_precision_gain = bridge->effects.precision_gain_modifier;
    bridge->stats.avg_attention_boost = bridge->effects.attention_boost;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * State/Stats Implementation
 * ============================================================================ */

/**
 * WHAT: Get current bridge state
 * WHY:  Monitor visual-FEP interaction
 * HOW:  Copy state with mutex protection
 */
int visual_cortex_fep_bridge_get_state(
    const visual_cortex_fep_bridge_t* bridge,
    visual_cortex_fep_state_t* state
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(state, NIMCP_ERROR_NULL_POINTER, "state is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/**
 * WHAT: Get bridge statistics
 * WHY:  Monitor performance and effects
 * HOW:  Copy stats with mutex protection
 */
int visual_cortex_fep_bridge_get_stats(
    const visual_cortex_fep_bridge_t* bridge,
    visual_cortex_fep_stats_t* stats
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

/**
 * WHAT: Connect to bio-async router
 * WHY:  Enable distributed visual-FEP messaging
 * HOW:  Register module with BIO_MODULE_FEP_VISUAL_CORTEX_BRIDGE ID
 */
int visual_cortex_fep_bridge_connect_bio_async(
    visual_cortex_fep_bridge_t* bridge
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_VISUAL_CORTEX_BRIDGE,
        .module_name = "visual_cortex_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO(LOG_MODULE_VISUAL_FEP " Connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN(LOG_MODULE_VISUAL_FEP " Bio-async router not available");
    return -1;
}

/**
 * WHAT: Disconnect from bio-async router
 * WHY:  Clean shutdown
 * HOW:  Unregister module
 */
int visual_cortex_fep_bridge_disconnect_bio_async(
    visual_cortex_fep_bridge_t* bridge
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO(LOG_MODULE_VISUAL_FEP " Disconnected from bio-async");
    return 0;
}

/**
 * WHAT: Check bio-async connection status
 * WHY:  Query if messaging is available
 * HOW:  Return bio_async_enabled flag
 */
bool visual_cortex_fep_bridge_is_bio_async_connected(
    const visual_cortex_fep_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}
