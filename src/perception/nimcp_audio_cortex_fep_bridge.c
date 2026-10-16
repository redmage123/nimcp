/**
 * @file nimcp_audio_cortex_fep_bridge.c
 * @brief Free Energy Principle - Audio Cortex Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 */

#include "perception/nimcp_audio_cortex_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>

#define LOG_MODULE_AUDIO_FEP "[AUDIO_FEP]"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(audio_cortex_fep_bridge)

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

/**
 * WHAT: Provide default configuration for audio-FEP bridge
 * WHY:  Simplify initialization with reasonable defaults
 * HOW:  Set biologically-plausible thresholds and enable all features
 */
int audio_cortex_fep_bridge_default_config(audio_cortex_fep_config_t* config) {
    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "NULL config pointer in audio_cortex_fep_bridge_default_config");

    config->prediction_error_threshold = AUDIO_FEP_PE_THRESHOLD_MEDIUM;
    config->precision_tuning_factor = AUDIO_FEP_PRECISION_TUNING_DEFAULT;
    config->temporal_prediction_weight = 0.5f;

    config->enable_temporal_predictions = true;
    config->enable_precision_tuning = true;
    config->enable_cocktail_party = true;
    config->enable_auditory_pe_updates = true;

    config->frequency_precision_sensitivity = 1.0f;
    config->temporal_prediction_sensitivity = 0.8f;
    config->pe_propagation_rate = 0.15f;

    return 0;
}

/**
 * WHAT: Create audio cortex FEP bridge
 * WHY:  Initialize bidirectional integration between audio cortex and FEP
 * HOW:  Allocate memory, initialize state, create mutex
 */
audio_cortex_fep_bridge_t* audio_cortex_fep_bridge_create(
    const audio_cortex_fep_config_t* config
) {
    audio_cortex_fep_bridge_t* bridge = (audio_cortex_fep_bridge_t*)
        nimcp_malloc(sizeof(audio_cortex_fep_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate audio_cortex_fep_bridge_t");
        return NULL;
    }

    memset(bridge, 0, sizeof(audio_cortex_fep_bridge_t));

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        audio_cortex_fep_bridge_default_config(&bridge->config);
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "audio_cortex_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to create mutex for audio_cortex_fep_bridge");
        nimcp_free(bridge);
        return NULL;
    }

    /* Initialize state */
    bridge->state.auditory_precision = 1.0f;
    bridge->effects.frequency_tuning_sharpness = 1.0f;
    bridge->effects.mel_filter_gain = 1.0f;
    bridge->effects.temporal_resolution_boost = 1.0f;
    bridge->effects.precision_tuning_modifier = 1.0f;

    /* Initialize per-frequency precision */
    for (int i = 0; i < 32; i++) {
        bridge->state.frequency_precision[i] = 1.0f;
    }

    NIMCP_LOGGING_INFO(LOG_MODULE_AUDIO_FEP " Bridge created successfully");
    return bridge;
}

/**
 * WHAT: Destroy audio cortex FEP bridge
 * WHY:  Free all allocated resources
 * HOW:  Disconnect bio-async, destroy mutex, free memory
 */
void audio_cortex_fep_bridge_destroy(audio_cortex_fep_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        audio_cortex_fep_bridge_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO(LOG_MODULE_AUDIO_FEP " Bridge destroyed");
}

/* ============================================================================
 * Connection Implementation
 * ============================================================================ */

/**
 * WHAT: Connect FEP system to bridge
 * WHY:  Enable FEP state queries and updates
 * HOW:  Store FEP pointer with mutex protection
 */
int audio_cortex_fep_bridge_connect_fep(
    audio_cortex_fep_bridge_t* bridge,
    fep_system_t* fep
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "NULL bridge in audio_cortex_fep_bridge_connect_fep");
    NIMCP_FEP_CHECK_THROW(fep, NIMCP_ERROR_NULL_POINTER, "NULL fep in audio_cortex_fep_bridge_connect_fep");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO(LOG_MODULE_AUDIO_FEP " FEP system connected");
    return 0;
}

/**
 * WHAT: Connect audio cortex to bridge
 * WHY:  Enable auditory processing monitoring and modulation
 * HOW:  Store audio cortex pointer with mutex protection
 */
int audio_cortex_fep_bridge_connect_audio_cortex(
    audio_cortex_fep_bridge_t* bridge,
    audio_cortex_t* audio
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "NULL bridge in audio_cortex_fep_bridge_connect_audio_cortex");
    NIMCP_FEP_CHECK_THROW(audio, NIMCP_ERROR_NULL_POINTER, "NULL audio in audio_cortex_fep_bridge_connect_audio_cortex");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->audio_cortex = audio;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO(LOG_MODULE_AUDIO_FEP " Audio cortex connected");
    return 0;
}

/* ============================================================================
 * FEP → Audio Implementation
 * ============================================================================ */

/**
 * WHAT: Apply FEP temporal predictions to auditory processing
 * WHY:  Top-down predictions modulate frequency tuning
 * HOW:  Query FEP temporal model, convert to mel filter gain
 */
int audio_cortex_fep_apply_temporal_predictions(
    audio_cortex_fep_bridge_t* bridge
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "NULL bridge in audio_cortex_fep_apply_temporal_predictions");
    NIMCP_FEP_CHECK_THROW(bridge->fep_system, NIMCP_ERROR_INVALID_STATE, "FEP system not connected in audio_cortex_fep_apply_temporal_predictions");
    if (!bridge->config.enable_temporal_predictions) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute temporal prediction confidence (simplified) */
    float prediction_confidence = bridge->state.temporal_prediction_accuracy;

    /* Modulate mel filter gain */
    bridge->effects.mel_filter_gain = 1.0f +
        (prediction_confidence * bridge->config.temporal_prediction_weight);

    /* Predictive suppression of expected sounds */
    bridge->effects.prediction_suppression = prediction_confidence * 0.2f;

    /* Enhancement of novel sounds */
    bridge->effects.novelty_enhancement = (1.0f - prediction_confidence) * 0.4f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Apply FEP precision to frequency tuning (cocktail party effect)
 * WHY:  Precision sharpens tuning for attended frequencies
 * HOW:  Modulate mel filterbank sharpness by precision
 */
int audio_cortex_fep_apply_precision_tuning(
    audio_cortex_fep_bridge_t* bridge
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "NULL bridge in audio_cortex_fep_apply_precision_tuning");
    NIMCP_FEP_CHECK_THROW(bridge->fep_system, NIMCP_ERROR_INVALID_STATE, "FEP system not connected in audio_cortex_fep_apply_precision_tuning");
    if (!bridge->config.enable_precision_tuning) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get precision from FEP (placeholder) */
    float fep_precision = 1.0f;

    /* Convert precision to tuning sharpness */
    bridge->effects.frequency_tuning_sharpness =
        AUDIO_FEP_PRECISION_TUNING_DEFAULT +
        (fep_precision * bridge->config.precision_tuning_factor);

    /* Clamp to valid range */
    if (bridge->effects.frequency_tuning_sharpness > AUDIO_FEP_PRECISION_TUNING_MAX) {
        bridge->effects.frequency_tuning_sharpness = AUDIO_FEP_PRECISION_TUNING_MAX;
    }
    if (bridge->effects.frequency_tuning_sharpness < AUDIO_FEP_PRECISION_TUNING_MIN) {
        bridge->effects.frequency_tuning_sharpness = AUDIO_FEP_PRECISION_TUNING_MIN;
    }

    bridge->state.auditory_precision = fep_precision;
    bridge->effects.precision_tuning_modifier = fep_precision *
        bridge->config.frequency_precision_sensitivity;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Audio → FEP Implementation
 * ============================================================================ */

/**
 * WHAT: Compute auditory prediction error (MMN response)
 * WHY:  PE drives belief updates in FEP
 * HOW:  Compare audio features to FEP temporal predictions
 */
int audio_cortex_fep_compute_prediction_error(
    audio_cortex_fep_bridge_t* bridge,
    const float* audio_features,
    uint32_t num_features,
    float* prediction_error
) {
    NIMCP_FEP_CHECK_THROW(bridge && audio_features && prediction_error, NIMCP_ERROR_NULL_POINTER, "NULL parameter in audio_cortex_fep_compute_prediction_error");
    NIMCP_FEP_CHECK_THROW(bridge->fep_system, NIMCP_ERROR_INVALID_STATE, "FEP system not connected in audio_cortex_fep_compute_prediction_error");
    NIMCP_FEP_CHECK_THROW(num_features > 0, NIMCP_ERROR_INVALID_PARAM, "num_features is 0 in audio_cortex_fep_compute_prediction_error");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Simplified PE computation (would compare to FEP temporal predictions) */
    float pe_sum = 0.0f;
    for (uint32_t i = 0; i < num_features; i++) {
        float expected = 0.0f; /* Placeholder: get from FEP temporal model */
        float error = audio_features[i] - expected;
        pe_sum += error * error;
    }

    *prediction_error = sqrtf(pe_sum / (float)num_features);

    /* Update state */
    bridge->state.current_auditory_pe = *prediction_error;
    float new_avg_aud_pe = 0.9f * bridge->state.avg_auditory_pe + 0.1f * (*prediction_error);
    if (isfinite(new_avg_aud_pe)) bridge->state.avg_auditory_pe = new_avg_aud_pe;

    if (*prediction_error > bridge->state.max_auditory_pe) {
        bridge->state.max_auditory_pe = *prediction_error;
    }

    /* Check for MMN events (high PE) */
    if (*prediction_error > bridge->config.prediction_error_threshold) {
        bridge->state.mmn_events++;
        bridge->stats.high_pe_events++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Report auditory observations to FEP
 * WHY:  Audio features drive temporal inference
 * HOW:  Convert mel features to FEP observation format
 */
int audio_cortex_fep_report_observations(
    audio_cortex_fep_bridge_t* bridge,
    const float* audio_features,
    uint32_t num_features
) {
    NIMCP_FEP_CHECK_THROW(bridge && audio_features, NIMCP_ERROR_NULL_POINTER, "NULL parameter in audio_cortex_fep_report_observations");
    NIMCP_FEP_CHECK_THROW(bridge->fep_system, NIMCP_ERROR_INVALID_STATE, "FEP system not connected in audio_cortex_fep_report_observations");
    if (!bridge->config.enable_auditory_pe_updates) return 0;
    NIMCP_FEP_CHECK_THROW(num_features > 0, NIMCP_ERROR_INVALID_PARAM, "num_features is 0 in audio_cortex_fep_report_observations");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Report to FEP system (would call FEP observation API) */
    bridge->state.frames_processed++;
    bridge->stats.total_frames_processed++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Report temporal events to FEP
 * WHY:  Onsets/offsets structure temporal predictions
 * HOW:  Signal event boundaries to FEP temporal model
 */
int audio_cortex_fep_report_temporal_events(
    audio_cortex_fep_bridge_t* bridge,
    bool onset_detected,
    bool offset_detected
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "NULL bridge in audio_cortex_fep_report_temporal_events");
    NIMCP_FEP_CHECK_THROW(bridge->fep_system, NIMCP_ERROR_INVALID_STATE, "FEP system not connected in audio_cortex_fep_report_temporal_events");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Report temporal events to FEP (placeholder) */
    if (onset_detected || offset_detected) {
        /* Update temporal prediction model */
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

/**
 * WHAT: Update audio-FEP bridge state
 * WHY:  Maintain bidirectional coupling
 * HOW:  Apply predictions, compute PE, update statistics
 */
int audio_cortex_fep_bridge_update(
    audio_cortex_fep_bridge_t* bridge,
    uint64_t delta_ms
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "NULL bridge in audio_cortex_fep_bridge_update");

    /* Apply FEP effects to auditory processing */
    audio_cortex_fep_apply_temporal_predictions(bridge);
    audio_cortex_fep_apply_precision_tuning(bridge);

    /* Update statistics */
    nimcp_mutex_lock(bridge->base.mutex);

    bridge->stats.avg_prediction_error = bridge->state.avg_auditory_pe;
    bridge->stats.temporal_prediction_accuracy =
        bridge->state.temporal_prediction_accuracy;

    /* Compute average frequency precision */
    float total_precision = 0.0f;
    for (int i = 0; i < 32; i++) {
        total_precision += bridge->state.frequency_precision[i];
    }
    bridge->stats.avg_frequency_precision = total_precision / 32.0f;

    bridge->stats.avg_tuning_sharpness = bridge->effects.frequency_tuning_sharpness;
    bridge->stats.avg_prediction_suppression = bridge->effects.prediction_suppression;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * State/Stats Implementation
 * ============================================================================ */

/**
 * WHAT: Get current bridge state
 * WHY:  Monitor audio-FEP interaction
 * HOW:  Copy state with mutex protection
 */
int audio_cortex_fep_bridge_get_state(
    const audio_cortex_fep_bridge_t* bridge,
    audio_cortex_fep_state_t* state
) {
    NIMCP_FEP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "NULL parameter in audio_cortex_fep_bridge_get_state");

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
int audio_cortex_fep_bridge_get_stats(
    const audio_cortex_fep_bridge_t* bridge,
    audio_cortex_fep_stats_t* stats
) {
    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "NULL parameter in audio_cortex_fep_bridge_get_stats");

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
 * WHY:  Enable distributed audio-FEP messaging
 * HOW:  Register module with BIO_MODULE_FEP_AUDIO_CORTEX_BRIDGE ID
 */
int audio_cortex_fep_bridge_connect_bio_async(
    audio_cortex_fep_bridge_t* bridge
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "NULL bridge in audio_cortex_fep_bridge_connect_bio_async");
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_AUDIO_CORTEX_BRIDGE,
        .module_name = "audio_cortex_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO(LOG_MODULE_AUDIO_FEP " Connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available for audio_cortex_fep_bridge");
    return -1;
}

/**
 * WHAT: Disconnect from bio-async router
 * WHY:  Clean shutdown
 * HOW:  Unregister module
 */
int audio_cortex_fep_bridge_disconnect_bio_async(
    audio_cortex_fep_bridge_t* bridge
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "NULL bridge in audio_cortex_fep_bridge_disconnect_bio_async");
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO(LOG_MODULE_AUDIO_FEP " Disconnected from bio-async");
    return 0;
}

/**
 * WHAT: Check bio-async connection status
 * WHY:  Query if messaging is available
 * HOW:  Return bio_async_enabled flag
 */
bool audio_cortex_fep_bridge_is_bio_async_connected(
    const audio_cortex_fep_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}
