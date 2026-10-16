//=============================================================================
// nimcp_stdp_omni_bridge.h - STDP ↔ Omnidirectional Inference Bridge
//=============================================================================
/**
 * @file nimcp_stdp_omni_bridge.h
 * @brief Bidirectional integration between STDP plasticity and Omnidirectional inference
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Bridge connecting STDP (spike-timing dependent plasticity) with
 *       Omnidirectional inference (forward/backward/lateral prediction)
 * WHY:  STDP weight changes should update world model parameters, and
 *       omnidirectional prediction errors should drive STDP learning
 * HOW:  Bidirectional communication where:
 *       - STDP weight updates → refine world model weights
 *       - Forward prediction errors → trigger LTP (correct predictions)
 *       - Backward prediction errors → trigger LTD (incorrect predictions)
 *       - Lateral prediction errors → cross-modal STDP coordination
 *
 * NEUROSCIENCE FOUNDATION:
 * =============================================================================
 *
 *   STDP → Omnidirectional Inference:
 *   +-----------------------------------------------------------------------+
 *   |  Synaptic plasticity refines predictive models:                       |
 *   |                                                                        |
 *   |  1. Weight Changes Update World Model:                                |
 *   |     - STDP LTP → strengthen prediction pathway                        |
 *   |     - STDP LTD → weaken prediction pathway                            |
 *   |     - Accumulated changes refine generative model                     |
 *   |                                                                        |
 *   |  2. Temporal Patterns Inform Predictions:                             |
 *   |     - Pre-before-post patterns → forward prediction learned           |
 *   |     - Post-before-pre patterns → backward inference refined           |
 *   |     - Lateral STDP → cross-modal prediction binding                   |
 *   +-----------------------------------------------------------------------+
 *
 *   Omnidirectional → STDP:
 *   +-----------------------------------------------------------------------+
 *   |  Prediction errors drive synaptic learning:                           |
 *   |                                                                        |
 *   |  1. Forward Prediction Error → STDP:                                  |
 *   |     - High forward PE → enhance STDP (model needs updating)           |
 *   |     - Low forward PE → reduce STDP (model is accurate)                |
 *   |     - PE magnitude scales learning rate                               |
 *   |                                                                        |
 *   |  2. Backward Inference Error → STDP:                                  |
 *   |     - Backward PE → adjust temporal STDP windows                      |
 *   |     - Informs credit assignment for past actions                      |
 *   |                                                                        |
 *   |  3. Lateral Prediction Error → STDP:                                  |
 *   |     - Cross-modal PE → coordinate multi-region STDP                   |
 *   |     - Enables multimodal binding through plasticity                   |
 *   +-----------------------------------------------------------------------+
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_STDP_OMNI_BRIDGE_H
#define NIMCP_STDP_OMNI_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Prediction error scaling */
#define STDP_OMNI_PE_MIN_THRESHOLD        0.1f    /**< Min PE for learning */
#define STDP_OMNI_PE_MAX_THRESHOLD        5.0f    /**< Max PE (saturation) */
#define STDP_OMNI_PE_LR_SCALING           1.5f    /**< PE → LR scaling factor */

/** Direction-specific modulation */
#define STDP_OMNI_FORWARD_WEIGHT          1.0f    /**< Forward PE weight */
#define STDP_OMNI_BACKWARD_WEIGHT         0.8f    /**< Backward PE weight */
#define STDP_OMNI_LATERAL_WEIGHT          0.6f    /**< Lateral PE weight */

/** World model update factors */
#define STDP_OMNI_WM_UPDATE_RATE          0.1f    /**< STDP → world model rate */
#define STDP_OMNI_WM_MIN_DELTA            0.01f   /**< Min weight change to update */

/** Precision modulation */
#define STDP_OMNI_PRECISION_MIN           0.1f    /**< Min precision factor */
#define STDP_OMNI_PRECISION_MAX           2.0f    /**< Max precision factor */

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Omnidirectional inference direction
 */
typedef enum {
    STDP_OMNI_DIR_FORWARD = 0,    /**< Forward prediction (t → t+1) */
    STDP_OMNI_DIR_BACKWARD,       /**< Backward inference (t ← t+1) */
    STDP_OMNI_DIR_LATERAL,        /**< Cross-modal lateral */
    STDP_OMNI_DIR_COUNT
} stdp_omni_direction_t;

/**
 * @brief Configuration for STDP-Omni bridge
 */
typedef struct {
    /* Prediction error thresholds */
    float pe_min_threshold;           /**< Min PE for learning */
    float pe_max_threshold;           /**< Max PE (saturation) */
    float pe_lr_scaling;              /**< PE → LR scaling */

    /* Direction weights */
    float forward_weight;             /**< Forward PE weight */
    float backward_weight;            /**< Backward PE weight */
    float lateral_weight;             /**< Lateral PE weight */

    /* World model updates */
    float wm_update_rate;             /**< STDP → world model rate */
    bool enable_wm_updates;           /**< Enable world model updates */

    /* Precision modulation */
    float precision_min;              /**< Min precision factor */
    float precision_max;              /**< Max precision factor */
    bool enable_precision_modulation; /**< Enable precision gating */

    /* Feature enables */
    bool enable_forward_pe;           /**< Enable forward PE → STDP */
    bool enable_backward_pe;          /**< Enable backward PE → STDP */
    bool enable_lateral_pe;           /**< Enable lateral PE → STDP */

    /* Bio-async */
    bool enable_bio_async;            /**< Enable async messaging */
} stdp_omni_bridge_config_t;

/**
 * @brief STDP effects on Omni (forward direction)
 */
typedef struct {
    float weight_change;              /**< Synaptic weight change */
    float wm_weight_delta;            /**< World model weight update */
    stdp_omni_direction_t affected_dir; /**< Direction affected */
    uint64_t timestamp_ms;            /**< Event timestamp */
} stdp_omni_forward_effect_t;

/**
 * @brief Omni effects on STDP (backward direction)
 */
typedef struct {
    float forward_pe;                 /**< Forward prediction error */
    float backward_pe;                /**< Backward prediction error */
    float lateral_pe;                 /**< Lateral prediction error */
    float combined_pe;                /**< Combined weighted PE */
    float precision;                  /**< Current precision */
    float lr_modulation;              /**< Computed LR modulation */
    float effective_a_plus;           /**< Modulated A+ */
    float effective_a_minus;          /**< Modulated A- */
} stdp_omni_backward_effect_t;

/**
 * @brief Bridge state
 */
typedef struct {
    float current_forward_pe;         /**< Latest forward PE */
    float current_backward_pe;        /**< Latest backward PE */
    float current_lateral_pe;         /**< Latest lateral PE */
    float current_precision;          /**< Current precision level */
    float cumulative_wm_delta;        /**< Accumulated WM changes */
    float bridge_coherence;           /**< STDP-Omni alignment [0,1] */
} stdp_omni_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t forward_pe_events;       /**< Forward PE events */
    uint64_t backward_pe_events;      /**< Backward PE events */
    uint64_t lateral_pe_events;       /**< Lateral PE events */
    uint64_t wm_updates;              /**< World model updates */
    float total_wm_delta;             /**< Total WM change */
    float avg_pe_magnitude;           /**< Average PE magnitude */
    float avg_lr_modulation;          /**< Average LR modulation */
    uint64_t forward_calls;           /**< STDP → Omni calls */
    uint64_t backward_calls;          /**< Omni → STDP calls */
} stdp_omni_bridge_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct stdp_omni_bridge_struct* stdp_omni_bridge_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default bridge configuration
 */
stdp_omni_bridge_config_t stdp_omni_bridge_default_config(void);

/**
 * @brief Validate bridge configuration
 */
bool stdp_omni_bridge_validate_config(const stdp_omni_bridge_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create STDP-Omni bridge
 */
stdp_omni_bridge_t stdp_omni_bridge_create(const stdp_omni_bridge_config_t* config);

/**
 * @brief Destroy STDP-Omni bridge
 */
void stdp_omni_bridge_destroy(stdp_omni_bridge_t bridge);

/**
 * @brief Check if bridge is connected
 */
bool stdp_omni_bridge_is_connected(stdp_omni_bridge_t bridge);

//=============================================================================
// Forward Direction: STDP → Omnidirectional
//=============================================================================

/**
 * @brief Notify Omni of STDP weight change
 *
 * WHAT: Report STDP plasticity event to update world model
 * WHY:  Synaptic changes should refine predictive model
 */
int stdp_omni_notify_weight_change(stdp_omni_bridge_t bridge,
                                   float weight_change,
                                   stdp_omni_direction_t direction,
                                   stdp_omni_forward_effect_t* effect);

/**
 * @brief Notify Omni of LTP event for forward prediction
 */
int stdp_omni_notify_ltp_forward(stdp_omni_bridge_t bridge,
                                 float weight_change,
                                 stdp_omni_forward_effect_t* effect);

/**
 * @brief Notify Omni of LTD event for backward adjustment
 */
int stdp_omni_notify_ltd_backward(stdp_omni_bridge_t bridge,
                                  float weight_change,
                                  stdp_omni_forward_effect_t* effect);

/**
 * @brief Notify Omni of lateral STDP for cross-modal binding
 */
int stdp_omni_notify_lateral(stdp_omni_bridge_t bridge,
                             float weight_change,
                             stdp_omni_forward_effect_t* effect);

//=============================================================================
// Backward Direction: Omnidirectional → STDP
//=============================================================================

/**
 * @brief Apply forward prediction error to STDP
 *
 * WHAT: Use forward PE to modulate STDP learning
 * WHY:  High PE indicates model needs updating via plasticity
 */
int stdp_omni_apply_forward_pe(stdp_omni_bridge_t bridge,
                               float prediction_error,
                               float base_lr,
                               float* modulated_lr);

/**
 * @brief Apply backward prediction error to STDP
 */
int stdp_omni_apply_backward_pe(stdp_omni_bridge_t bridge,
                                float prediction_error,
                                float base_lr,
                                float* modulated_lr);

/**
 * @brief Apply lateral prediction error to STDP
 */
int stdp_omni_apply_lateral_pe(stdp_omni_bridge_t bridge,
                               float prediction_error,
                               float base_lr,
                               float* modulated_lr);

/**
 * @brief Apply precision-weighted modulation
 */
int stdp_omni_apply_precision(stdp_omni_bridge_t bridge,
                              float precision,
                              float base_lr,
                              float* modulated_lr);

/**
 * @brief Compute combined STDP modulation from all PE sources
 */
int stdp_omni_compute_modulation(stdp_omni_bridge_t bridge,
                                 float forward_pe, float backward_pe,
                                 float lateral_pe, float precision,
                                 float base_a_plus, float base_a_minus,
                                 stdp_omni_backward_effect_t* effect);

//=============================================================================
// State and Statistics
//=============================================================================

/**
 * @brief Get current bridge state
 */
int stdp_omni_bridge_get_state(stdp_omni_bridge_t bridge,
                               stdp_omni_bridge_state_t* state);

/**
 * @brief Get bridge statistics
 */
int stdp_omni_bridge_get_stats(stdp_omni_bridge_t bridge,
                               stdp_omni_bridge_stats_t* stats);

/**
 * @brief Reset bridge statistics
 */
int stdp_omni_bridge_reset_stats(stdp_omni_bridge_t bridge);

/**
 * @brief Update bridge state
 */
int stdp_omni_bridge_update(stdp_omni_bridge_t bridge, float dt_ms);

/**
 * @brief Get bridge coherence
 */
float stdp_omni_bridge_get_coherence(stdp_omni_bridge_t bridge);

/**
 * @brief Print bridge summary
 */
void stdp_omni_bridge_print_summary(stdp_omni_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_STDP_OMNI_BRIDGE_H */
