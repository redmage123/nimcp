/**
 * @file nimcp_predictive_coding_sleep_bridge.h
 * @brief Sleep-Predictive Coding Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and predictive coding
 * WHY:  Sleep states fundamentally alter prediction error processing and precision
 * HOW:  Sleep state modulates prediction strength, error weighting, and precision learning
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP → PREDICTIVE CODING PATHWAYS:
 * ------------------------------------
 * 1. Top-Down Prediction Strength:
 *    - AWAKE: Balanced prediction/error (bidirectional processing)
 *    - DROWSY: Increased prediction weight (preparation for offline processing)
 *    - LIGHT_NREM: Strong predictions (replay based on internal model)
 *    - DEEP_NREM: Maximum prediction weight (consolidate internal model)
 *    - REM: Reduced prediction weight (creative exploration)
 *    - Reference: Friston (2010) "Free-energy principle"
 *
 * 2. Precision-Weighted Error:
 *    - AWAKE: High precision on sensory errors (learning from environment)
 *    - DROWSY: Reduced sensory precision (transition)
 *    - LIGHT_NREM: Low sensory precision, high internal precision
 *    - DEEP_NREM: Minimal sensory precision (offline consolidation)
 *    - REM: Variable precision (exploration of state space)
 *    - Reference: Hobson & Friston (2012) "Waking and dreaming consciousness"
 *
 * 3. Prediction Error Learning Rate:
 *    - AWAKE: Full learning rate (online adaptation)
 *    - LIGHT_NREM: Moderate rate (consolidate predictions)
 *    - DEEP_NREM: Enhanced rate for internal model refinement
 *    - REM: High rate for creative associations
 *
 * 4. Hierarchical Dynamics:
 *    - AWAKE: Bottom-up and top-down flow
 *    - NREM: Primarily top-down (generative replay)
 *    - Deep NREM: Top-down consolidation of hierarchical structure
 *    - REM: Balanced but with increased noise (creativity)
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║            SLEEP-PREDICTIVE CODING INTEGRATION BRIDGE                      ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   SLEEP STATE      Pred    Precision  Error LR   Effect                  ║
 * ║   ────────────────────────────────────────────────────────────────────    ║
 * ║   AWAKE            1.0     1.0        1.0         Balanced processing     ║
 * ║   DROWSY           1.2     0.7        0.7         Shift to internal       ║
 * ║   LIGHT_NREM       1.4     0.4        0.8         Replay from model       ║
 * ║   DEEP_NREM        1.6     0.2        1.2         Consolidate model       ║
 * ║   REM              0.8     0.5        1.3         Creative exploration    ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PREDICTIVE_CODING_SLEEP_BRIDGE_H
#define NIMCP_PREDICTIVE_CODING_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "plasticity/predictive/nimcp_predictive_coding.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Sleep State Predictive Coding Modulation
 * ============================================================================ */

/* Prediction strength modulation by sleep state */
#define PREDICTIVE_SLEEP_PRED_AWAKE         1.0f   /**< Balanced */
#define PREDICTIVE_SLEEP_PRED_DROWSY        1.2f   /**< Increased internal */
#define PREDICTIVE_SLEEP_PRED_LIGHT_NREM    1.4f   /**< Strong internal model */
#define PREDICTIVE_SLEEP_PRED_DEEP_NREM     1.6f   /**< Maximum internal */
#define PREDICTIVE_SLEEP_PRED_REM           0.8f   /**< Reduced (exploration) */

/* Precision (inverse variance) modulation by sleep state */
#define PREDICTIVE_SLEEP_PRECISION_AWAKE      1.0f   /**< Full sensory precision */
#define PREDICTIVE_SLEEP_PRECISION_DROWSY     0.7f   /**< Reduced sensory */
#define PREDICTIVE_SLEEP_PRECISION_LIGHT_NREM 0.4f   /**< Low sensory */
#define PREDICTIVE_SLEEP_PRECISION_DEEP_NREM  0.2f   /**< Minimal sensory */
#define PREDICTIVE_SLEEP_PRECISION_REM        0.5f   /**< Variable */

/* Error learning rate modulation by sleep state */
#define PREDICTIVE_SLEEP_ERROR_LR_AWAKE       1.0f   /**< Online learning */
#define PREDICTIVE_SLEEP_ERROR_LR_DROWSY      0.7f   /**< Reduced */
#define PREDICTIVE_SLEEP_ERROR_LR_LIGHT_NREM  0.8f   /**< Moderate consolidation */
#define PREDICTIVE_SLEEP_ERROR_LR_DEEP_NREM   1.2f   /**< Enhanced consolidation */
#define PREDICTIVE_SLEEP_ERROR_LR_REM         1.3f   /**< Enhanced exploration */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Sleep-predictive coding bridge configuration
 */
typedef struct {
    bool enable_prediction_modulation;  /**< Enable prediction strength changes */
    bool enable_precision_modulation;   /**< Enable precision weighting changes */
    bool enable_error_lr_modulation;    /**< Enable error learning rate changes */
    float modulation_strength;          /**< Overall modulation strength (0-1) */
} predictive_sleep_config_t;

/**
 * @brief Computed sleep effects on predictive coding
 */
typedef struct {
    float prediction_strength_factor;   /**< Multiply prediction weight by this */
    float precision_factor;             /**< Multiply precision by this */
    float error_learning_rate_factor;   /**< Multiply error LR by this */
    sleep_state_t current_state;        /**< Current sleep state */
    float sleep_pressure;               /**< Current sleep pressure */
    bool offline_consolidation;         /**< True during deep NREM */
} predictive_sleep_effects_t;

/**
 * @brief Sleep-predictive coding integration bridge
 */
typedef struct predictive_sleep_bridge_struct* predictive_sleep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Get default sleep-predictive coding bridge configuration
 * WHY:  Provide sensible defaults based on free energy principle
 */
int predictive_sleep_default_config(predictive_sleep_config_t* config);

/**
 * WHAT: Create sleep-predictive coding bridge
 * WHY:  Initialize integration between sleep and predictive coding systems
 */
predictive_sleep_bridge_t predictive_sleep_bridge_create(
    const predictive_sleep_config_t* config,
    sleep_system_t sleep_system
);

/**
 * WHAT: Destroy sleep-predictive coding bridge
 * WHY:  Clean up resources and unregister callbacks
 */
void predictive_sleep_bridge_destroy(predictive_sleep_bridge_t bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * WHAT: Update predictive coding effects from sleep system state
 * WHY:  Compute how current sleep state affects prediction/error processing
 */
int predictive_sleep_update(predictive_sleep_bridge_t bridge);

/**
 * WHAT: Get modulated predictive coding parameters for current sleep state
 * WHY:  Apply sleep modulation to prediction error minimization
 */
int predictive_sleep_get_effects(const predictive_sleep_bridge_t bridge,
                                  predictive_sleep_effects_t* effects);

/**
 * WHAT: Get sleep-modulated prediction strength
 * WHY:  Apply prediction modulation to top-down processing
 */
float predictive_sleep_get_prediction_strength(const predictive_sleep_bridge_t bridge,
                                                float base_strength);

/**
 * WHAT: Get sleep-modulated precision
 * WHY:  Apply precision modulation to error weighting
 */
float predictive_sleep_get_precision(const predictive_sleep_bridge_t bridge,
                                      float base_precision);

/**
 * WHAT: Get sleep-modulated error learning rate
 * WHY:  Apply learning rate modulation to prediction updates
 */
float predictive_sleep_get_error_learning_rate(const predictive_sleep_bridge_t bridge,
                                                float base_lr);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

float predictive_sleep_get_pred_factor(sleep_state_t state);
float predictive_sleep_get_precision_factor(sleep_state_t state);
float predictive_sleep_get_error_lr_factor(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PREDICTIVE_CODING_SLEEP_BRIDGE_H */
