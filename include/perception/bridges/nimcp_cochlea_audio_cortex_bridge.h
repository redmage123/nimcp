/**
 * @file nimcp_cochlea_audio_cortex_bridge.h
 * @brief Cochlea-Audio Cortex (A1) integration bridge
 *
 * WHAT: Connect cochlear processing to primary auditory cortex
 * WHY:  Enable cortical auditory perception and feature extraction
 * HOW:  Route MGN output to tonotopically-organized A1 columns
 *
 * BIOLOGICAL BASIS:
 * - A1 receives input from MGN (thalamus) via Layer 4
 * - Tonotopic organization: frequency maps across cortical surface
 * - Isofrequency columns: neurons tuned to same frequency
 * - Forward/feedback streams: A1 → belt → parabelt (ventral/dorsal)
 *
 * PROCESSING HIERARCHY:
 * - Layer 4: Thalamic input, frequency-specific
 * - Layer 2/3: Local integration, lateral connections
 * - Layer 5: Cortical output (feedback to thalamus)
 * - Layer 6: Modulatory feedback to MGN
 *
 * BIDIRECTIONAL DATA FLOWS:
 * - OUTBOUND: Cochlea/MGN → A1: Frequency activations, temporal patterns
 * - INBOUND:  A1 → Cochlea: Predictive coding, attention modulation
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#ifndef NIMCP_COCHLEA_AUDIO_CORTEX_BRIDGE_H
#define NIMCP_COCHLEA_AUDIO_CORTEX_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/error/nimcp_error_codes.h"
#include "perception/nimcp_cochlea.h"
#include "perception/nimcp_audio_cortex.h"
#include "perception/bridges/nimcp_cochlea_thalamic_bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** A1 cortical parameters */
#define COCHLEA_CORTEX_DEFAULT_COLUMNS      64      /**< Number of isofrequency columns */
#define COCHLEA_CORTEX_LAYERS               6       /**< Cortical layers */
#define COCHLEA_CORTEX_FEEDBACK_DELAY_MS    10.0f   /**< Feedback loop delay */

/** Predictive coding parameters */
#define COCHLEA_CORTEX_PREDICTION_WEIGHT    0.3f    /**< Weight of prediction in output */
#define COCHLEA_CORTEX_ERROR_GAIN           2.0f    /**< Gain for prediction errors */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Cortical processing stream
 *
 * BIOLOGICAL: "What" and "Where" pathways
 */
typedef enum {
    CORTEX_STREAM_VENTRAL,          /**< "What" pathway - sound identity */
    CORTEX_STREAM_DORSAL,           /**< "Where" pathway - sound location */
    CORTEX_STREAM_BOTH              /**< Both streams active */
} cortex_stream_t;

/**
 * @brief Cortical layer for input/output
 */
typedef enum {
    CORTEX_LAYER_1 = 0,             /**< Layer 1 - Molecular (feedback) */
    CORTEX_LAYER_2_3,               /**< Layer 2/3 - Supragranular */
    CORTEX_LAYER_4,                 /**< Layer 4 - Granular (thalamic input) */
    CORTEX_LAYER_5,                 /**< Layer 5 - Deep (output) */
    CORTEX_LAYER_6                  /**< Layer 6 - Deep (thalamic feedback) */
} cortex_layer_t;

/**
 * @brief Prediction mode
 */
typedef enum {
    PRED_MODE_NONE,                 /**< No prediction (feedforward only) */
    PRED_MODE_SIMPLE,               /**< Simple prediction (exponential decay) */
    PRED_MODE_HIERARCHICAL          /**< Full hierarchical predictive coding */
} prediction_mode_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Cortical column output
 */
typedef struct {
    uint32_t column_id;             /**< Column identifier */
    float center_freq_hz;           /**< Best frequency of column */

    /* Layer activations */
    float layer_activation[COCHLEA_CORTEX_LAYERS];

    /* Derived features */
    float onset_response;           /**< Onset detection */
    float offset_response;          /**< Offset detection */
    float rate_response;            /**< Rate coding response */
    float temporal_response;        /**< Temporal fine structure */
} cortical_column_output_t;

/**
 * @brief A1 cortical output
 */
typedef struct {
    /* Per-column outputs */
    cortical_column_output_t* columns;
    uint32_t num_columns;

    /* Aggregate responses */
    float total_activation;         /**< Total cortical activation */
    float* tonotopic_map;           /**< Frequency activation map */
    uint32_t map_size;

    /* Feature representations */
    float* spectral_features;       /**< Spectral feature vector */
    float* temporal_features;       /**< Temporal feature vector */
    uint32_t feature_dim;

    /* Prediction outputs */
    float* prediction;              /**< Predicted next input */
    float* prediction_error;        /**< Prediction error signal */
    float prediction_confidence;    /**< Confidence in prediction */

    /* Stream activations */
    float ventral_activation;       /**< "What" stream */
    float dorsal_activation;        /**< "Where" stream */
} a1_output_t;

/**
 * @brief Feedback signal to cochlea/thalamus
 */
typedef struct {
    float* frequency_attention;     /**< Frequency-specific attention */
    float* gain_modulation;         /**< Gain modulation per channel */
    uint32_t num_channels;

    float global_gain;              /**< Global gain adjustment */
    float prediction_signal;        /**< Prediction back to periphery */
} cortical_feedback_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Cortical parameters */
    uint32_t num_columns;           /**< Number of isofrequency columns */
    cortex_stream_t active_stream;  /**< Which streams to process */

    /* Predictive coding */
    prediction_mode_t prediction_mode;
    float prediction_weight;        /**< Weight of prediction */
    float error_gain;               /**< Prediction error gain */

    /* Feedback parameters */
    bool enable_feedback;           /**< Enable cortical feedback */
    float feedback_delay_ms;        /**< Feedback loop delay */
    float feedback_strength;        /**< Feedback modulation strength */

    /* Feature extraction */
    uint32_t spectral_feature_dim;  /**< Spectral feature dimension */
    uint32_t temporal_feature_dim;  /**< Temporal feature dimension */

    /* Learning */
    bool enable_plasticity;         /**< Enable cortical plasticity */
    float learning_rate;            /**< Plasticity learning rate */
} cochlea_audio_cortex_config_t;

/**
 * @brief Bridge instance (opaque)
 */
typedef struct cochlea_audio_cortex_bridge cochlea_audio_cortex_bridge_t;

//=============================================================================
// Configuration Helpers
//=============================================================================

/**
 * @brief Get default bridge configuration
 */
cochlea_audio_cortex_config_t cochlea_audio_cortex_config_default(void);

//=============================================================================
// Core API
//=============================================================================

/**
 * @brief Create cochlea-audio cortex bridge
 *
 * @param cochlea Cochlea instance (optional if using thalamic bridge)
 * @param thalamic_bridge Thalamic bridge instance (optional)
 * @param audio_cortex Audio cortex instance (optional)
 * @param config Bridge configuration
 * @return Bridge instance or NULL
 */
cochlea_audio_cortex_bridge_t* cochlea_audio_cortex_bridge_create(
    cochlea_t* cochlea,
    cochlea_thalamic_bridge_t* thalamic_bridge,
    audio_cortex_t* audio_cortex,
    const cochlea_audio_cortex_config_t* config
);

/**
 * @brief Destroy bridge
 */
void cochlea_audio_cortex_bridge_destroy(cochlea_audio_cortex_bridge_t* bridge);

/**
 * @brief Update bridge with cochlear output
 *
 * @param bridge Bridge instance
 * @param cochlea_output Cochlear output (can be NULL if using MGN)
 * @param mgn_output MGN output from thalamic bridge (can be NULL)
 * @param dt_ms Time step
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_audio_cortex_bridge_update(
    cochlea_audio_cortex_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    const mgn_output_t* mgn_output,
    float dt_ms
);

/**
 * @brief Reset bridge state
 */
nimcp_error_t cochlea_audio_cortex_bridge_reset(cochlea_audio_cortex_bridge_t* bridge);

//=============================================================================
// Cortical Output Access
//=============================================================================

/**
 * @brief Get A1 cortical output
 */
nimcp_error_t cochlea_audio_cortex_get_output(
    const cochlea_audio_cortex_bridge_t* bridge,
    a1_output_t* output
);

/**
 * @brief Get single column output
 */
nimcp_error_t cochlea_audio_cortex_get_column(
    const cochlea_audio_cortex_bridge_t* bridge,
    uint32_t column_id,
    cortical_column_output_t* output
);

/**
 * @brief Get layer-specific activation
 *
 * @param bridge Bridge instance
 * @param layer Cortical layer
 * @param activation Output: activation array [num_columns]
 * @param num_columns Output: number of columns
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_audio_cortex_get_layer(
    const cochlea_audio_cortex_bridge_t* bridge,
    cortex_layer_t layer,
    float** activation,
    uint32_t* num_columns
);

//=============================================================================
// Predictive Coding
//=============================================================================

/**
 * @brief Get prediction for next input
 */
nimcp_error_t cochlea_audio_cortex_get_prediction(
    const cochlea_audio_cortex_bridge_t* bridge,
    float** prediction,
    uint32_t* num_channels
);

/**
 * @brief Get prediction error signal
 */
nimcp_error_t cochlea_audio_cortex_get_prediction_error(
    const cochlea_audio_cortex_bridge_t* bridge,
    float** error,
    uint32_t* num_channels
);

/**
 * @brief Set external prediction (for hierarchical integration)
 */
nimcp_error_t cochlea_audio_cortex_set_top_down_prediction(
    cochlea_audio_cortex_bridge_t* bridge,
    const float* prediction,
    uint32_t num_channels
);

//=============================================================================
// Feedback Control
//=============================================================================

/**
 * @brief Get cortical feedback signal
 */
nimcp_error_t cochlea_audio_cortex_get_feedback(
    const cochlea_audio_cortex_bridge_t* bridge,
    cortical_feedback_t* feedback
);

/**
 * @brief Apply cortical feedback to thalamic bridge
 */
nimcp_error_t cochlea_audio_cortex_apply_feedback(
    cochlea_audio_cortex_bridge_t* bridge,
    cochlea_thalamic_bridge_t* thalamic_bridge
);

/**
 * @brief Set feedback strength
 */
nimcp_error_t cochlea_audio_cortex_set_feedback_strength(
    cochlea_audio_cortex_bridge_t* bridge,
    float strength
);

//=============================================================================
// Stream Control
//=============================================================================

/**
 * @brief Set active processing stream
 */
nimcp_error_t cochlea_audio_cortex_set_stream(
    cochlea_audio_cortex_bridge_t* bridge,
    cortex_stream_t stream
);

/**
 * @brief Get stream activation levels
 */
nimcp_error_t cochlea_audio_cortex_get_stream_activation(
    const cochlea_audio_cortex_bridge_t* bridge,
    float* ventral,
    float* dorsal
);

//=============================================================================
// Feature Extraction
//=============================================================================

/**
 * @brief Get spectral features
 */
nimcp_error_t cochlea_audio_cortex_get_spectral_features(
    const cochlea_audio_cortex_bridge_t* bridge,
    float** features,
    uint32_t* dim
);

/**
 * @brief Get temporal features
 */
nimcp_error_t cochlea_audio_cortex_get_temporal_features(
    const cochlea_audio_cortex_bridge_t* bridge,
    float** features,
    uint32_t* dim
);

//=============================================================================
// Bidirectional Verification
//=============================================================================

/**
 * @brief Verify bidirectional data flow
 */
bool cochlea_audio_cortex_verify_bidirectional(
    const cochlea_audio_cortex_bridge_t* bridge
);

uint64_t cochlea_audio_cortex_get_last_outbound(
    const cochlea_audio_cortex_bridge_t* bridge
);

uint64_t cochlea_audio_cortex_get_last_inbound(
    const cochlea_audio_cortex_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COCHLEA_AUDIO_CORTEX_BRIDGE_H */
