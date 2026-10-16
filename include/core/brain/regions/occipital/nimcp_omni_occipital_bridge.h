/**
 * @file nimcp_omni_occipital_bridge.h
 * @brief Omnidirectional Inference to Occipital Lobe Bridge
 * @version 1.0.0
 * @date 2025-01-04
 *
 * WHAT: Bridge integrating omnidirectional inference with occipital visual hierarchy
 * WHY:  Enable predictive visual processing through V1-V5 hierarchy
 * HOW:  Top-down predictions flow backward, prediction errors flow forward
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * PREDICTIVE VISUAL HIERARCHY:
 * ----------------------------
 * The occipital lobe implements predictive coding across visual areas:
 *
 *   1. V1 (PRIMARY VISUAL CORTEX):
 *      - Receives retinal input via LGN
 *      - Computes edge, orientation, spatial frequency PEs
 *      - Top-down predictions from V2 bias edge detection
 *
 *   2. V2 (SECONDARY VISUAL CORTEX):
 *      - Contour integration and texture processing
 *      - Receives V1 PEs, sends to V4/V5
 *      - Top-down predictions specify expected contours
 *
 *   3. V4 (VENTRAL "WHAT" STREAM):
 *      - Color constancy and complex form processing
 *      - Object identity predictions from IT cortex
 *      - Color and shape PE computation
 *
 *   4. V5/MT (DORSAL "WHERE" STREAM):
 *      - Motion detection and optic flow
 *      - Spatial position predictions from parietal
 *      - Motion PE for action guidance
 *
 * BIDIRECTIONAL INFERENCE:
 * ------------------------
 *   Direction        Visual Operation
 *   ─────────────────────────────────────────────
 *   Forward (↑)      PE propagation: V1→V2→V4/V5
 *   Backward (↓)     Prediction: IT/Parietal→V4/V5→V2→V1
 *   Lateral          Cross-stream: V4↔V5 (what+where binding)
 *   Hierarchical     Multi-level predictive coding
 *
 * ATTENTION AND PRECISION:
 * ------------------------
 * - Spatial attention modulates V1-V4 precision
 * - Feature attention modulates V4 precision
 * - Motion attention modulates V5 precision
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_OMNI_OCCIPITAL_BRIDGE_H
#define NIMCP_OMNI_OCCIPITAL_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct omni_occipital_bridge omni_occipital_bridge_t;
typedef struct jepa_bidirectional jepa_bidirectional_t;
typedef struct predictive_hierarchy predictive_hierarchy_t;
typedef struct hopfield_memory hopfield_memory_t;
typedef struct occipital_adapter occipital_adapter_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Bio-async module ID for omni-occipital bridge */
#define BIO_MODULE_OMNI_OCCIPITAL_BRIDGE           0x0E57

/** @brief Number of visual areas */
#define OMNI_OCCIPITAL_NUM_AREAS                   4

/** @brief Default prediction error threshold */
#define OMNI_OCCIPITAL_PE_THRESHOLD                1.0f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Visual area identifier
 */
typedef enum {
    OMNI_VISUAL_V1 = 0,          /**< Primary visual cortex */
    OMNI_VISUAL_V2,              /**< Secondary visual cortex */
    OMNI_VISUAL_V4,              /**< Ventral stream (what) */
    OMNI_VISUAL_V5               /**< Dorsal stream (where/motion) */
} omni_visual_area_t;

/**
 * @brief Visual stream type
 */
typedef enum {
    OMNI_STREAM_DORSAL = 0,      /**< Where/how stream (V1→V2→V5→Parietal) */
    OMNI_STREAM_VENTRAL,         /**< What stream (V1→V2→V4→IT) */
    OMNI_STREAM_BOTH             /**< Both streams */
} omni_visual_stream_t;

/**
 * @brief Visual feature type for predictions
 */
typedef enum {
    OMNI_VIS_EDGE = 0,           /**< Edge/orientation (V1) */
    OMNI_VIS_CONTOUR,            /**< Contour/texture (V2) */
    OMNI_VIS_COLOR,              /**< Color constancy (V4) */
    OMNI_VIS_FORM,               /**< Complex form (V4) */
    OMNI_VIS_MOTION,             /**< Motion/flow (V5) */
    OMNI_VIS_DEPTH               /**< Depth/stereo (V2/V5) */
} omni_visual_feature_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Per-area state
 */
typedef struct {
    float* prediction;           /**< Top-down prediction */
    float* prediction_error;     /**< Bottom-up PE */
    float* features;             /**< Current features */
    uint32_t dim;                /**< Feature dimension */
    float precision;             /**< Current precision */
    float pe_magnitude;          /**< PE magnitude */
    bool active;                 /**< Area active */
} omni_visual_area_state_t;

/**
 * @brief Omni effects on occipital lobe
 */
typedef struct {
    float* v1_prediction;        /**< V1 edge prediction */
    float* v2_prediction;        /**< V2 contour prediction */
    float* v4_prediction;        /**< V4 object prediction */
    float* v5_prediction;        /**< V5 motion prediction */
    uint32_t v1_dim;
    uint32_t v2_dim;
    uint32_t v4_dim;
    uint32_t v5_dim;
    float precision_v1;          /**< V1 precision weight */
    float precision_v2;          /**< V2 precision weight */
    float precision_v4;          /**< V4 precision weight */
    float precision_v5;          /**< V5 precision weight */
    omni_visual_stream_t active_stream; /**< Currently active stream */
} omni_to_occipital_effects_t;

/**
 * @brief Occipital lobe effects on omni
 */
typedef struct {
    float* v1_pe;                /**< V1 prediction errors */
    float* v2_pe;                /**< V2 prediction errors */
    float* v4_pe;                /**< V4 prediction errors */
    float* v5_pe;                /**< V5 prediction errors */
    uint32_t v1_dim;
    uint32_t v2_dim;
    uint32_t v4_dim;
    uint32_t v5_dim;
    float combined_pe;           /**< Combined visual PE */
    float dorsal_pe;             /**< Dorsal stream PE */
    float ventral_pe;            /**< Ventral stream PE */
    float free_energy;           /**< Visual free energy */
    bool motion_detected;        /**< Motion detected in V5 */
    bool object_recognized;      /**< Object recognized in V4 */
} occipital_to_omni_effects_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Per-area precision */
    float default_v1_precision;  /**< Default V1 precision */
    float default_v2_precision;  /**< Default V2 precision */
    float default_v4_precision;  /**< Default V4 precision */
    float default_v5_precision;  /**< Default V5 precision */

    /* PE thresholds */
    float pe_threshold;          /**< PE threshold for update */
    float motion_threshold;      /**< Motion detection threshold */
    float recognition_threshold; /**< Object recognition threshold */

    /* Stream control */
    omni_visual_stream_t default_stream; /**< Default active stream */
    bool enable_cross_stream;    /**< Enable dorsal-ventral binding */

    /* Attention */
    bool enable_spatial_attention;   /**< Spatial attention modulation */
    bool enable_feature_attention;   /**< Feature attention modulation */
    float attention_gain;            /**< Attention gain factor */

    /* Integration */
    bool enable_bio_async;       /**< Enable bio-async messaging */
    bool enable_logging;         /**< Enable logging */
} omni_occipital_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;          /**< Total bridge updates */
    uint64_t v1_predictions;         /**< V1 predictions count */
    uint64_t v2_predictions;         /**< V2 predictions count */
    uint64_t v4_predictions;         /**< V4 predictions count */
    uint64_t v5_predictions;         /**< V5 predictions count */
    uint64_t motion_events;          /**< Motion detection events */
    uint64_t recognition_events;     /**< Object recognition events */
    float avg_v1_pe;                 /**< Average V1 PE */
    float avg_v2_pe;                 /**< Average V2 PE */
    float avg_v4_pe;                 /**< Average V4 PE */
    float avg_v5_pe;                 /**< Average V5 PE */
    float avg_free_energy;           /**< Average visual free energy */
} omni_occipital_stats_t;

/**
 * @brief Omni-occipital bridge structure
 */
struct omni_occipital_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge */

    omni_occipital_config_t config;  /**< Configuration */

    /* Connected systems */
    jepa_bidirectional_t* jepa;      /**< Bidirectional JEPA */
    predictive_hierarchy_t* pred_hier; /**< Predictive hierarchy */
    hopfield_memory_t* hopfield;     /**< Associative memory */
    occipital_adapter_t* occipital;  /**< Occipital adapter */

    /* Per-area state */
    omni_visual_area_state_t area_states[OMNI_OCCIPITAL_NUM_AREAS];

    /* Computed effects */
    omni_to_occipital_effects_t omni_effects;    /**< Omni → occipital */
    occipital_to_omni_effects_t occipital_effects; /**< Occipital → omni */

    /* Statistics */
    omni_occipital_stats_t stats;

    /* Bio-async integration */
    void* bio_context;               /**< Bio-async module context */
    bool bio_async_connected;        /**< Bio-async connection state */

    /* Thread safety */
    void* mutex;
};

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int omni_occipital_default_config(omni_occipital_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

omni_occipital_bridge_t* omni_occipital_bridge_create(
    const omni_occipital_config_t* config);

void omni_occipital_bridge_destroy(omni_occipital_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

int omni_occipital_connect_jepa(omni_occipital_bridge_t* bridge,
                                 jepa_bidirectional_t* jepa);

int omni_occipital_connect_pred_hier(omni_occipital_bridge_t* bridge,
                                      predictive_hierarchy_t* pred_hier);

int omni_occipital_connect_hopfield(omni_occipital_bridge_t* bridge,
                                     hopfield_memory_t* hopfield);

int omni_occipital_connect_occipital(omni_occipital_bridge_t* bridge,
                                      occipital_adapter_t* occipital);

/* ============================================================================
 * Update API
 * ============================================================================ */

int omni_occipital_update(omni_occipital_bridge_t* bridge);

int omni_occipital_apply_to_occipital(omni_occipital_bridge_t* bridge);

int omni_occipital_apply_to_omni(omni_occipital_bridge_t* bridge);

/* ============================================================================
 * Prediction API
 * ============================================================================ */

/**
 * @brief Generate top-down prediction for visual area
 */
int omni_occipital_predict_area(omni_occipital_bridge_t* bridge,
                                 omni_visual_area_t area,
                                 float* prediction,
                                 uint32_t dim);

/**
 * @brief Compute prediction error for visual area
 */
int omni_occipital_compute_pe(omni_occipital_bridge_t* bridge,
                               omni_visual_area_t area,
                               const float* observation,
                               uint32_t dim,
                               float* pe);

/**
 * @brief Propagate prediction backward through hierarchy
 */
int omni_occipital_propagate_backward(omni_occipital_bridge_t* bridge);

/**
 * @brief Propagate prediction error forward through hierarchy
 */
int omni_occipital_propagate_forward(omni_occipital_bridge_t* bridge);

/* ============================================================================
 * Stream API
 * ============================================================================ */

/**
 * @brief Set active visual stream
 */
int omni_occipital_set_stream(omni_occipital_bridge_t* bridge,
                               omni_visual_stream_t stream);

/**
 * @brief Get dorsal stream prediction error
 */
float omni_occipital_get_dorsal_pe(const omni_occipital_bridge_t* bridge);

/**
 * @brief Get ventral stream prediction error
 */
float omni_occipital_get_ventral_pe(const omni_occipital_bridge_t* bridge);

/**
 * @brief Bind dorsal and ventral streams
 */
int omni_occipital_bind_streams(omni_occipital_bridge_t* bridge);

/* ============================================================================
 * Precision API
 * ============================================================================ */

int omni_occipital_set_precision(omni_occipital_bridge_t* bridge,
                                  omni_visual_area_t area,
                                  float precision);

float omni_occipital_get_precision(const omni_occipital_bridge_t* bridge,
                                    omni_visual_area_t area);

int omni_occipital_update_precision(omni_occipital_bridge_t* bridge);

/* ============================================================================
 * Attention API
 * ============================================================================ */

/**
 * @brief Apply spatial attention to V1-V2
 */
int omni_occipital_apply_spatial_attention(omni_occipital_bridge_t* bridge,
                                            const float* attention_map,
                                            uint32_t width,
                                            uint32_t height);

/**
 * @brief Apply feature attention to V4
 */
int omni_occipital_apply_feature_attention(omni_occipital_bridge_t* bridge,
                                            omni_visual_feature_t feature,
                                            float attention_weight);

/* ============================================================================
 * Query API
 * ============================================================================ */

int omni_occipital_get_omni_effects(const omni_occipital_bridge_t* bridge,
                                     omni_to_occipital_effects_t* effects);

int omni_occipital_get_occipital_effects(const omni_occipital_bridge_t* bridge,
                                          occipital_to_omni_effects_t* effects);

int omni_occipital_get_area_state(const omni_occipital_bridge_t* bridge,
                                   omni_visual_area_t area,
                                   omni_visual_area_state_t* state);

int omni_occipital_get_stats(const omni_occipital_bridge_t* bridge,
                              omni_occipital_stats_t* stats);

int omni_occipital_reset_stats(omni_occipital_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

int omni_occipital_connect_bio_async(omni_occipital_bridge_t* bridge);
int omni_occipital_disconnect_bio_async(omni_occipital_bridge_t* bridge);
bool omni_occipital_is_bio_async_connected(const omni_occipital_bridge_t* bridge);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* omni_occipital_area_to_string(omni_visual_area_t area);
const char* omni_occipital_stream_to_string(omni_visual_stream_t stream);
const char* omni_occipital_feature_to_string(omni_visual_feature_t feature);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_OCCIPITAL_BRIDGE_H */
