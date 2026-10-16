/**
 * @file nimcp_speech_cortex_fep_bridge.c
 * @brief Free Energy Principle - Speech Cortex Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 */

#include "perception/nimcp_speech_cortex_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>

#define LOG_MODULE_SPEECH_FEP "[SPEECH_FEP]"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(speech_cortex_fep_bridge)

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

/**
 * WHAT: Provide default configuration for speech-FEP bridge
 * WHY:  Simplify initialization with reasonable defaults
 * HOW:  Set biologically-plausible thresholds and enable all features
 */
int speech_cortex_fep_bridge_default_config(speech_cortex_fep_config_t* config) {
    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "NULL config in speech_cortex_fep_bridge_default_config");

    config->prediction_error_threshold = SPEECH_FEP_PE_THRESHOLD_MEDIUM;
    config->precision_category_factor = SPEECH_FEP_PRECISION_CATEGORY_DEFAULT;
    config->phoneme_prediction_weight = 0.6f;

    config->enable_phoneme_predictions = true;
    config->enable_precision_categories = true;
    config->enable_motor_theory = true;
    config->enable_phoneme_pe_updates = true;

    config->phoneme_precision_sensitivity = 1.0f;
    config->lexical_prediction_sensitivity = 0.7f;
    config->pe_propagation_rate = 0.2f;

    return 0;
}

/**
 * WHAT: Create speech cortex FEP bridge
 * WHY:  Initialize bidirectional integration between speech cortex and FEP
 * HOW:  Allocate memory, initialize state, create mutex
 */
speech_cortex_fep_bridge_t* speech_cortex_fep_bridge_create(
    const speech_cortex_fep_config_t* config
) {
    speech_cortex_fep_bridge_t* bridge = (speech_cortex_fep_bridge_t*)
        nimcp_malloc(sizeof(speech_cortex_fep_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate speech_cortex_fep_bridge_t");
        return NULL;
    }

    memset(bridge, 0, sizeof(speech_cortex_fep_bridge_t));

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        speech_cortex_fep_bridge_default_config(&bridge->config);
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "speech_cortex_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to create mutex for speech_cortex_fep_bridge");
        nimcp_free(bridge);
        return NULL;
    }

    /* Initialize state */
    bridge->state.phoneme_precision = 1.0f;
    bridge->state.lexical_precision = 1.0f;
    bridge->state.prediction_horizon = SPEECH_FEP_PREDICTION_HORIZON;
    bridge->effects.phoneme_category_sharpness = 1.0f;
    bridge->effects.phoneme_detector_gain = 1.0f;
    bridge->effects.formant_discrimination = 1.0f;
    bridge->effects.precision_category_modifier = 1.0f;

    /* Initialize predicted phonemes to silence */
    for (uint32_t i = 0; i < SPEECH_FEP_PREDICTION_HORIZON; i++) {
        bridge->state.predicted_phonemes[i] = PHONEME_SILENCE;
        bridge->state.prediction_confidence[i] = 0.0f;
    }

    NIMCP_LOGGING_INFO(LOG_MODULE_SPEECH_FEP " Bridge created successfully");
    return bridge;
}

/**
 * WHAT: Destroy speech cortex FEP bridge
 * WHY:  Free all allocated resources
 * HOW:  Disconnect bio-async, destroy mutex, free memory
 */
void speech_cortex_fep_bridge_destroy(speech_cortex_fep_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        speech_cortex_fep_bridge_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO(LOG_MODULE_SPEECH_FEP " Bridge destroyed");
}

/* ============================================================================
 * Connection Implementation
 * ============================================================================ */

/**
 * WHAT: Connect FEP system to bridge
 * WHY:  Enable FEP state queries and updates
 * HOW:  Store FEP pointer with mutex protection
 */
int speech_cortex_fep_bridge_connect_fep(
    speech_cortex_fep_bridge_t* bridge,
    fep_system_t* fep
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "NULL bridge in speech_cortex_fep_bridge_connect_fep");
    NIMCP_FEP_CHECK_THROW(fep, NIMCP_ERROR_NULL_POINTER, "NULL fep in speech_cortex_fep_bridge_connect_fep");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO(LOG_MODULE_SPEECH_FEP " FEP system connected");
    return 0;
}

/**
 * WHAT: Connect speech cortex to bridge
 * WHY:  Enable speech processing monitoring and modulation
 * HOW:  Store speech cortex pointer with mutex protection
 */
int speech_cortex_fep_bridge_connect_speech_cortex(
    speech_cortex_fep_bridge_t* bridge,
    speech_cortex_t* speech
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "NULL bridge in speech_cortex_fep_bridge_connect_speech_cortex");
    NIMCP_FEP_CHECK_THROW(speech, NIMCP_ERROR_NULL_POINTER, "NULL speech in speech_cortex_fep_bridge_connect_speech_cortex");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->speech_cortex = speech;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO(LOG_MODULE_SPEECH_FEP " Speech cortex connected");
    return 0;
}

/* ============================================================================
 * FEP → Speech Implementation
 * ============================================================================ */

/**
 * WHAT: Apply FEP phoneme predictions to speech processing
 * WHY:  Lexical context primes expected phonemes
 * HOW:  Query FEP lexical model, modulate detector gain
 */
int speech_cortex_fep_apply_phoneme_predictions(
    speech_cortex_fep_bridge_t* bridge
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "NULL bridge in speech_cortex_fep_apply_phoneme_predictions");
    NIMCP_FEP_CHECK_THROW(bridge->fep_system, NIMCP_ERROR_INVALID_STATE, "FEP system not connected in speech_cortex_fep_apply_phoneme_predictions");
    if (!bridge->config.enable_phoneme_predictions) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute prediction priming (simplified) */
    float avg_confidence = 0.0f;
    for (uint32_t i = 0; i < bridge->state.prediction_horizon; i++) {
        avg_confidence += bridge->state.prediction_confidence[i];
    }
    avg_confidence /= (float)bridge->state.prediction_horizon;

    /* Modulate detector gain for predicted phonemes */
    bridge->effects.phoneme_detector_gain = 1.0f +
        (avg_confidence * bridge->config.phoneme_prediction_weight);

    /* Prediction priming strength */
    bridge->effects.prediction_priming = avg_confidence *
        bridge->config.lexical_prediction_sensitivity;

    /* Novelty sensitivity (inverse of prediction) */
    bridge->effects.novelty_sensitivity = (1.0f - avg_confidence) * 0.5f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Apply FEP precision to phoneme categories
 * WHY:  Precision sharpens/broadens category boundaries
 * HOW:  Modulate formant distance thresholds by precision
 */
int speech_cortex_fep_apply_precision_categories(
    speech_cortex_fep_bridge_t* bridge
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "NULL bridge in speech_cortex_fep_apply_precision_categories");
    NIMCP_FEP_CHECK_THROW(bridge->fep_system, NIMCP_ERROR_INVALID_STATE, "FEP system not connected in speech_cortex_fep_apply_precision_categories");
    if (!bridge->config.enable_precision_categories) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get precision from FEP (placeholder) */
    float fep_precision = bridge->state.phoneme_precision;

    /* Convert precision to category sharpness */
    bridge->effects.phoneme_category_sharpness =
        SPEECH_FEP_PRECISION_CATEGORY_DEFAULT +
        (fep_precision * bridge->config.precision_category_factor);

    /* Clamp to valid range */
    if (bridge->effects.phoneme_category_sharpness > SPEECH_FEP_PRECISION_CATEGORY_MAX) {
        bridge->effects.phoneme_category_sharpness = SPEECH_FEP_PRECISION_CATEGORY_MAX;
    }
    if (bridge->effects.phoneme_category_sharpness < SPEECH_FEP_PRECISION_CATEGORY_MIN) {
        bridge->effects.phoneme_category_sharpness = SPEECH_FEP_PRECISION_CATEGORY_MIN;
    }

    /* Formant discrimination follows category sharpness */
    bridge->effects.formant_discrimination = bridge->effects.phoneme_category_sharpness;

    bridge->effects.precision_category_modifier = fep_precision *
        bridge->config.phoneme_precision_sensitivity;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Apply motor predictions to articulation (motor theory)
 * WHY:  Speech perception uses motor predictions
 * HOW:  Predict formants from articulatory gestures
 */
int speech_cortex_fep_apply_motor_predictions(
    speech_cortex_fep_bridge_t* bridge
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "NULL bridge in speech_cortex_fep_apply_motor_predictions");
    NIMCP_FEP_CHECK_THROW(bridge->fep_system, NIMCP_ERROR_INVALID_STATE, "FEP system not connected in speech_cortex_fep_apply_motor_predictions");
    if (!bridge->config.enable_motor_theory) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Motor theory: predict acoustic consequences (simplified) */
    /* Would query FEP motor model for articulatory predictions */

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Speech → FEP Implementation
 * ============================================================================ */

/**
 * WHAT: Compute phoneme prediction error
 * WHY:  PE drives lexical belief updates
 * HOW:  Compare detected phoneme to FEP prediction
 */
int speech_cortex_fep_compute_phoneme_prediction_error(
    speech_cortex_fep_bridge_t* bridge,
    phoneme_t detected_phoneme,
    float confidence,
    float* prediction_error
) {
    NIMCP_FEP_CHECK_THROW(bridge && prediction_error, NIMCP_ERROR_NULL_POINTER, "NULL parameter in speech_cortex_fep_compute_phoneme_prediction_error");
    NIMCP_FEP_CHECK_THROW(bridge->fep_system, NIMCP_ERROR_INVALID_STATE, "FEP system not connected in speech_cortex_fep_compute_phoneme_prediction_error");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if phoneme was predicted */
    bool was_predicted = false;
    float max_prediction_conf = 0.0f;

    for (uint32_t i = 0; i < bridge->state.prediction_horizon; i++) {
        if (bridge->state.predicted_phonemes[i] == detected_phoneme) {
            was_predicted = true;
            if (bridge->state.prediction_confidence[i] > max_prediction_conf) {
                max_prediction_conf = bridge->state.prediction_confidence[i];
            }
        }
    }

    /* PE is high if phoneme was unexpected */
    if (was_predicted) {
        *prediction_error = (1.0f - max_prediction_conf) * confidence;
    } else {
        *prediction_error = confidence * 2.0f; /* Double PE for unexpected */
    }

    /* Update state */
    bridge->state.current_phoneme_pe = *prediction_error;
    float new_avg_ph_pe = 0.9f * bridge->state.avg_phoneme_pe + 0.1f * (*prediction_error);
    if (isfinite(new_avg_ph_pe)) bridge->state.avg_phoneme_pe = new_avg_ph_pe;

    if (*prediction_error > bridge->state.max_phoneme_pe) {
        bridge->state.max_phoneme_pe = *prediction_error;
    }

    /* Check for high PE events (word boundaries) */
    if (*prediction_error > bridge->config.prediction_error_threshold) {
        bridge->state.word_boundary_events++;
        bridge->stats.high_pe_events++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Report phoneme observation to FEP
 * WHY:  Phoneme sequence drives word recognition
 * HOW:  Convert phoneme to FEP observation format
 */
int speech_cortex_fep_report_phoneme_observation(
    speech_cortex_fep_bridge_t* bridge,
    phoneme_t phoneme,
    float confidence
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "NULL bridge in speech_cortex_fep_report_phoneme_observation");
    NIMCP_FEP_CHECK_THROW(bridge->fep_system, NIMCP_ERROR_INVALID_STATE, "FEP system not connected in speech_cortex_fep_report_phoneme_observation");
    if (!bridge->config.enable_phoneme_pe_updates) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Report to FEP system (would call FEP observation API) */
    bridge->state.phonemes_processed++;
    bridge->stats.total_phonemes_processed++;

    /* Shift prediction window (would be updated by FEP) */
    for (uint32_t i = 0; i < SPEECH_FEP_PREDICTION_HORIZON - 1; i++) {
        bridge->state.predicted_phonemes[i] = bridge->state.predicted_phonemes[i + 1];
        bridge->state.prediction_confidence[i] = bridge->state.prediction_confidence[i + 1];
    }
    bridge->state.predicted_phonemes[SPEECH_FEP_PREDICTION_HORIZON - 1] = PHONEME_SILENCE;
    bridge->state.prediction_confidence[SPEECH_FEP_PREDICTION_HORIZON - 1] = 0.0f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Report word boundary to FEP
 * WHY:  High PE indicates word segmentation
 * HOW:  Detect PE spike, signal to FEP
 */
int speech_cortex_fep_report_word_boundary(
    speech_cortex_fep_bridge_t* bridge
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "NULL bridge in speech_cortex_fep_report_word_boundary");
    NIMCP_FEP_CHECK_THROW(bridge->fep_system, NIMCP_ERROR_INVALID_STATE, "FEP system not connected in speech_cortex_fep_report_word_boundary");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Report word boundary to FEP (placeholder) */
    bridge->stats.word_recognition_events++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

/**
 * WHAT: Update speech-FEP bridge state
 * WHY:  Maintain bidirectional coupling
 * HOW:  Apply predictions, compute PE, update statistics
 */
int speech_cortex_fep_bridge_update(
    speech_cortex_fep_bridge_t* bridge,
    uint64_t delta_ms
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "NULL bridge in speech_cortex_fep_bridge_update");

    /* Apply FEP effects to speech processing */
    speech_cortex_fep_apply_phoneme_predictions(bridge);
    speech_cortex_fep_apply_precision_categories(bridge);
    speech_cortex_fep_apply_motor_predictions(bridge);

    /* Update statistics */
    nimcp_mutex_lock(bridge->base.mutex);

    bridge->stats.avg_prediction_error = bridge->state.avg_phoneme_pe;

    /* Compute phoneme prediction accuracy */
    if (bridge->state.phonemes_processed > 0) {
        float pe_rate = (float)bridge->stats.high_pe_events /
                       (float)bridge->state.phonemes_processed;
        bridge->stats.phoneme_prediction_accuracy = 1.0f - pe_rate;
    }

    /* Compute average word boundary PE */
    if (bridge->stats.word_recognition_events > 0) {
        bridge->stats.avg_word_boundary_pe =
            bridge->state.max_phoneme_pe / (float)bridge->stats.word_recognition_events;
    }

    bridge->stats.avg_category_sharpness = bridge->effects.phoneme_category_sharpness;
    bridge->stats.avg_prediction_priming = bridge->effects.prediction_priming;
    bridge->stats.lexical_facilitation_rate = bridge->effects.prediction_priming;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * State/Stats Implementation
 * ============================================================================ */

/**
 * WHAT: Get current bridge state
 * WHY:  Monitor speech-FEP interaction
 * HOW:  Copy state with mutex protection
 */
int speech_cortex_fep_bridge_get_state(
    const speech_cortex_fep_bridge_t* bridge,
    speech_cortex_fep_state_t* state
) {
    NIMCP_FEP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "NULL parameter in speech_cortex_fep_bridge_get_state");

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
int speech_cortex_fep_bridge_get_stats(
    const speech_cortex_fep_bridge_t* bridge,
    speech_cortex_fep_stats_t* stats
) {
    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "NULL parameter in speech_cortex_fep_bridge_get_stats");

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
 * WHY:  Enable distributed speech-FEP messaging
 * HOW:  Register module with BIO_MODULE_FEP_SPEECH_CORTEX_BRIDGE ID
 */
int speech_cortex_fep_bridge_connect_bio_async(
    speech_cortex_fep_bridge_t* bridge
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "NULL bridge in speech_cortex_fep_bridge_connect_bio_async");
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_SPEECH_CORTEX_BRIDGE,
        .module_name = "speech_cortex_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO(LOG_MODULE_SPEECH_FEP " Connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available for speech_cortex_fep_bridge");
    return -1;
}

/**
 * WHAT: Disconnect from bio-async router
 * WHY:  Clean shutdown
 * HOW:  Unregister module
 */
int speech_cortex_fep_bridge_disconnect_bio_async(
    speech_cortex_fep_bridge_t* bridge
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "NULL bridge in speech_cortex_fep_bridge_disconnect_bio_async");
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO(LOG_MODULE_SPEECH_FEP " Disconnected from bio-async");
    return 0;
}

/**
 * WHAT: Check bio-async connection status
 * WHY:  Query if messaging is available
 * HOW:  Return bio_async_enabled flag
 */
bool speech_cortex_fep_bridge_is_bio_async_connected(
    const speech_cortex_fep_bridge_t* bridge
) {
    return bridge ? bridge->base.bio_async_enabled : false;
}
