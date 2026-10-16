/**
 * @file nimcp_adaptive_fep_bridge.h
 * @brief Free Energy Principle - Adaptive Plasticity Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between Free Energy Principle and adaptive threshold plasticity
 * WHY:  FEP prediction errors drive adaptive threshold learning; threshold adaptation minimizes free energy.
 *       Essential for efficient prediction-driven sparse coding under active inference.
 * HOW:  FEP precision modulates sparsity targets; prediction error magnitude scales threshold adaptation;
 *       adaptive neuron activations inform FEP belief updates about learned representations.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * FEP → ADAPTIVE PATHWAYS:
 * ------------------------
 * 1. Precision Controls Sparsity Target:
 *    - High precision → higher sparsity (confident predictions need fewer active neurons)
 *    - Low precision → lower sparsity (uncertain predictions need broader sampling)
 *    - Sparsity as precision-dependent resource allocation
 *    - Reference: Friston (2010) "Precision and attention in active inference"
 *
 * 2. Prediction Error Scales Threshold Adaptation:
 *    - High PE → rapid threshold adaptation (unexpected input requires recalibration)
 *    - Low PE → slow threshold adaptation (stable predictions maintain thresholds)
 *    - PE magnitude controls adaptation learning rate
 *    - Reference: Gershman (2019) "Uncertainty and exploration"
 *
 * 3. Complexity Cost Regulates Thresholds:
 *    - FEP complexity term penalizes excessive activation
 *    - Adaptive thresholds implement complexity minimization
 *    - Higher complexity cost → higher thresholds → more sparsity
 *
 * ADAPTIVE → FEP PATHWAYS:
 * ------------------------
 * 1. Activation Sparsity Informs Precision Estimates:
 *    - Low sparsity (many active) → low precision inference (uncertain state)
 *    - High sparsity (few active) → high precision inference (confident state)
 *    - Sparsity statistics update FEP precision beliefs
 *
 * 2. Threshold Adaptation Updates Generative Model:
 *    - Threshold changes = model parameter updates
 *    - Adaptive thresholds learn optimal activation statistics
 *    - Long-term threshold learning shapes FEP generative model structure
 *
 * 3. Integer Spike Counts as Discrete Beliefs:
 *    - Spike counts implement discrete probability representations
 *    - Efficient sampling from posterior distributions
 *    - Supports variational message passing
 *
 * INTEGRATION MECHANISMS:
 * -----------------------
 * - Precision-scaled sparsity: target_sparsity = base_sparsity × f(precision)
 * - PE-scaled adaptation: η_threshold = η_base × |PE|
 * - Sparsity-to-precision mapping: precision_estimate = g(measured_sparsity)
 * - Homeostatic balance: FEP complexity cost + adaptive sparsity target
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_ADAPTIVE_FEP_BRIDGE_H
#define NIMCP_ADAPTIVE_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Prediction error scaling factors */
#define ADAPTIVE_FEP_PE_MIN_THRESHOLD        0.5f    /**< Min PE for adaptation */
#define ADAPTIVE_FEP_PE_MAX_THRESHOLD       10.0f    /**< Max PE (saturation) */
#define ADAPTIVE_FEP_PE_SCALING_FACTOR       1.0f    /**< PE → adaptation rate */

/* Precision modulation factors */
#define ADAPTIVE_FEP_PRECISION_MIN           0.1f    /**< Min precision scaling */
#define ADAPTIVE_FEP_PRECISION_MAX           2.0f    /**< Max precision boost */
#define ADAPTIVE_FEP_PRECISION_SENSITIVITY   1.5f    /**< Precision → sparsity scaling */

/* Sparsity control */
#define ADAPTIVE_FEP_SPARSITY_MIN            0.1f    /**< Min sparsity target */
#define ADAPTIVE_FEP_SPARSITY_MAX            0.95f   /**< Max sparsity target */
#define ADAPTIVE_FEP_SPARSITY_BASELINE       0.7f    /**< Baseline sparsity */

/* Threshold adaptation */
#define ADAPTIVE_FEP_THRESHOLD_MIN_FACTOR    0.5f    /**< Min threshold scaling */
#define ADAPTIVE_FEP_THRESHOLD_MAX_FACTOR    2.0f    /**< Max threshold scaling */
#define ADAPTIVE_FEP_ADAPTATION_RATE_MIN     0.001f  /**< Min adaptation rate */
#define ADAPTIVE_FEP_ADAPTATION_RATE_MAX     0.1f    /**< Max adaptation rate */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct adaptive_fep_bridge adaptive_fep_bridge_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for adaptive-FEP bridge
 */
typedef struct {
    /* Thresholds */
    float pe_min_threshold;              /**< Min PE for adaptation */
    float pe_max_threshold;              /**< Max PE (saturation) */
    float precision_sensitivity;         /**< Precision → sparsity scaling */
    float sparsity_min;                  /**< Min sparsity target */
    float sparsity_max;                  /**< Max sparsity target */

    /* Feature enables */
    bool enable_pe_scaling;              /**< PE scales adaptation rate */
    bool enable_precision_sparsity;      /**< Precision modulates sparsity target */
    bool enable_complexity_regularization; /**< FEP complexity adjusts thresholds */
    bool enable_sparsity_feedback;       /**< Sparsity updates FEP precision */

    /* Sensitivity factors */
    float pe_sensitivity;                /**< PE effect scaling */
    float precision_gain;                /**< Precision effect scaling */
    float complexity_gain;               /**< Complexity cost scaling */
    float sparsity_feedback_gain;        /**< Sparsity → precision scaling */
} adaptive_fep_config_t;

/**
 * @brief FEP effects on adaptive plasticity
 */
typedef struct {
    /* Prediction error effects */
    float pe_magnitude;                  /**< Current PE magnitude */
    float pe_adaptation_scaling;         /**< PE → adaptation rate scaling */

    /* Precision effects */
    float precision_value;               /**< Current precision */
    float precision_sparsity_scaling;    /**< Precision → sparsity target scaling */

    /* Complexity effects */
    float complexity_value;              /**< Current complexity cost */
    float complexity_threshold_scaling;  /**< Complexity → threshold scaling */

    /* Total effects */
    float effective_sparsity_target;     /**< Modulated sparsity target */
    float effective_adaptation_rate;     /**< Modulated adaptation rate */
    float effective_threshold_scaling;   /**< Combined threshold scaling */
} adaptive_fep_effects_t;

/**
 * @brief Adaptive plasticity effects on FEP
 */
typedef struct {
    /* Sparsity feedback */
    float measured_sparsity;             /**< Current network sparsity */
    float sparsity_precision_estimate;   /**< Sparsity-derived precision */

    /* Activation statistics */
    uint32_t num_active_neurons;         /**< Active neuron count */
    uint32_t total_neurons;              /**< Total neuron count */
    float activation_entropy;            /**< Activation distribution entropy */

    /* Threshold state */
    float mean_threshold;                /**< Mean adaptive threshold */
    float threshold_variance;            /**< Threshold variance */
} adaptive_fep_feedback_t;

/**
 * @brief Current state of adaptive-FEP interaction
 */
typedef struct {
    /* Current FEP state */
    float current_pe;                    /**< Current prediction error */
    float current_precision;             /**< Current precision estimate */
    float current_complexity;            /**< Current complexity cost */

    /* Current adaptive state */
    float current_sparsity;              /**< Current network sparsity */
    float current_mean_threshold;        /**< Current mean threshold */

    /* Applied modifiers */
    float sparsity_modulation;           /**< Current sparsity modifier */
    float threshold_modulation;          /**< Current threshold modifier */
    float adaptation_rate_modulation;    /**< Current adaptation rate modifier */

    /* Statistics */
    uint32_t adaptation_events;          /**< Threshold adaptation events */
    uint64_t last_update_time;           /**< Last update timestamp */
} adaptive_fep_state_t;

/**
 * @brief Statistics for adaptive-FEP bridge
 */
typedef struct {
    /* Learning events */
    uint64_t total_updates;              /**< Total bridge updates */
    uint64_t pe_scaled_events;           /**< PE-scaled adaptation events */
    uint64_t precision_modulated_events; /**< Precision-modulated events */

    /* Effect magnitudes */
    float avg_pe_scaling;                /**< Average PE scaling factor */
    float avg_precision_scaling;         /**< Average precision scaling */
    float avg_sparsity;                  /**< Average network sparsity */

    /* Model updates */
    uint64_t threshold_updates;          /**< Threshold adaptation events */
    float total_threshold_delta;         /**< Cumulative threshold changes */

    /* Performance */
    float avg_free_energy;               /**< Average free energy */
    float avg_prediction_error;          /**< Average PE */
    float avg_complexity;                /**< Average complexity cost */
} adaptive_fep_stats_t;

/**
 * @brief Adaptive-FEP bridge state
 */
struct adaptive_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    adaptive_fep_config_t config;

    /* Connected systems */
    fep_system_t* fep_system;            /**< FEP system */
    adaptive_network_t adaptive_network; /**< Adaptive network */

    /* Current effects */
    adaptive_fep_effects_t effects;
    adaptive_fep_feedback_t feedback;
    adaptive_fep_state_t state;

    /* Statistics */
    adaptive_fep_stats_t stats;

};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default adaptive-FEP configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible defaults
 * HOW:  Set standard thresholds and enable all features
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int adaptive_fep_bridge_default_config(adaptive_fep_config_t* config);

/**
 * @brief Create adaptive-FEP bridge
 *
 * WHAT: Initialize adaptive-FEP integration bridge
 * WHY:  Enable bidirectional adaptive-FEP interaction
 * HOW:  Allocate bridge, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
adaptive_fep_bridge_t* adaptive_fep_bridge_create(const adaptive_fep_config_t* config);

/**
 * @brief Destroy adaptive-FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect systems, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void adaptive_fep_bridge_destroy(adaptive_fep_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect FEP system
 *
 * WHAT: Link bridge to FEP system
 * WHY:  Enable FEP state monitoring
 * HOW:  Store FEP system pointer
 *
 * @param bridge Adaptive-FEP bridge
 * @param fep FEP system
 * @return 0 on success
 */
int adaptive_fep_bridge_connect_fep(
    adaptive_fep_bridge_t* bridge,
    fep_system_t* fep
);

/**
 * @brief Connect adaptive network
 *
 * WHAT: Link bridge to adaptive network
 * WHY:  Enable adaptive parameter modulation
 * HOW:  Store adaptive network handle
 *
 * @param bridge Adaptive-FEP bridge
 * @param network Adaptive network
 * @return 0 on success
 */
int adaptive_fep_bridge_connect_adaptive(
    adaptive_fep_bridge_t* bridge,
    adaptive_network_t network
);

/**
 * @brief Disconnect all systems
 *
 * WHAT: Unlink FEP and adaptive systems
 * WHY:  Safe shutdown
 * HOW:  Clear system pointers
 *
 * @param bridge Adaptive-FEP bridge
 * @return 0 on success
 */
int adaptive_fep_bridge_disconnect(adaptive_fep_bridge_t* bridge);

/* ============================================================================
 * FEP → Adaptive Direction
 * ============================================================================ */

/**
 * @brief Apply prediction error scaling to adaptation rate
 *
 * WHAT: Modulate threshold adaptation rate by PE magnitude
 * WHY:  Unexpected events drive faster threshold adaptation
 * HOW:  η_adapt = η_base × f(|PE|)
 *
 * @param bridge Adaptive-FEP bridge
 * @param pe Prediction error magnitude
 * @return Adaptation rate scaling factor
 */
float adaptive_fep_apply_pe_scaling(
    adaptive_fep_bridge_t* bridge,
    float pe
);

/**
 * @brief Apply precision modulation to sparsity target
 *
 * WHAT: Modulate sparsity target by precision (confidence)
 * WHY:  High precision predictions benefit from higher sparsity
 * HOW:  sparsity_target = base_sparsity × f(precision)
 *
 * @param bridge Adaptive-FEP bridge
 * @param precision Current precision estimate
 * @return Sparsity target scaling factor
 */
float adaptive_fep_apply_precision_sparsity(
    adaptive_fep_bridge_t* bridge,
    float precision
);

/**
 * @brief Apply complexity regularization to thresholds
 *
 * WHAT: Adjust thresholds based on FEP complexity cost
 * WHY:  Complexity minimization encourages sparse representations
 * HOW:  threshold_scale = 1.0 + complexity × gain
 *
 * @param bridge Adaptive-FEP bridge
 * @param complexity Current complexity cost
 * @return Threshold scaling factor
 */
float adaptive_fep_apply_complexity_regularization(
    adaptive_fep_bridge_t* bridge,
    float complexity
);

/**
 * @brief Get effective sparsity target
 *
 * WHAT: Compute FEP-modulated sparsity target
 * WHY:  Combine all FEP effects on sparsity
 * HOW:  Multiply base sparsity by precision and complexity factors
 *
 * @param bridge Adaptive-FEP bridge
 * @param base_sparsity Base sparsity target
 * @return Effective sparsity target
 */
float adaptive_fep_get_effective_sparsity(
    const adaptive_fep_bridge_t* bridge,
    float base_sparsity
);

/**
 * @brief Get effective adaptation rate
 *
 * WHAT: Compute FEP-modulated adaptation rate
 * WHY:  Combine all FEP effects on threshold adaptation
 * HOW:  Multiply base rate by PE scaling factor
 *
 * @param bridge Adaptive-FEP bridge
 * @param base_rate Base adaptation rate
 * @return Effective adaptation rate
 */
float adaptive_fep_get_effective_adaptation_rate(
    const adaptive_fep_bridge_t* bridge,
    float base_rate
);

/* ============================================================================
 * Adaptive → FEP Direction
 * ============================================================================ */

/**
 * @brief Report sparsity to FEP system
 *
 * WHAT: Update FEP precision estimates based on network sparsity
 * WHY:  Sparsity indicates confidence in representations
 * HOW:  Convert sparsity statistics to precision beliefs
 *
 * @param bridge Adaptive-FEP bridge
 * @param sparsity Current network sparsity
 * @return 0 on success
 */
int adaptive_fep_report_sparsity(
    adaptive_fep_bridge_t* bridge,
    float sparsity
);

/**
 * @brief Report threshold changes to FEP system
 *
 * WHAT: Update FEP generative model based on threshold adaptation
 * WHY:  Threshold changes refine generative model parameters
 * HOW:  Convert threshold deltas to model parameter updates
 *
 * @param bridge Adaptive-FEP bridge
 * @param threshold_delta Total threshold change
 * @return 0 on success
 */
int adaptive_fep_report_threshold_changes(
    adaptive_fep_bridge_t* bridge,
    float threshold_delta
);

/**
 * @brief Compute precision estimate from sparsity
 *
 * WHAT: Derive precision from activation sparsity
 * WHY:  Sparsity reflects confidence in current beliefs
 * HOW:  precision ∝ sparsity (high sparsity = high confidence)
 *
 * @param bridge Adaptive-FEP bridge
 * @return Precision estimate
 */
float adaptive_fep_compute_sparsity_precision(
    const adaptive_fep_bridge_t* bridge
);

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

/**
 * @brief Update adaptive-FEP bridge state
 *
 * WHAT: Main update loop for bidirectional integration
 * WHY:  Keep adaptive and FEP systems synchronized
 * HOW:  Update effects, apply modulation, track statistics
 *
 * @param bridge Adaptive-FEP bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int adaptive_fep_bridge_update(
    adaptive_fep_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 *
 * @param bridge Adaptive-FEP bridge
 * @param state Output state
 * @return 0 on success
 */
int adaptive_fep_bridge_get_state(
    const adaptive_fep_bridge_t* bridge,
    adaptive_fep_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Adaptive-FEP bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int adaptive_fep_bridge_get_stats(
    const adaptive_fep_bridge_t* bridge,
    adaptive_fep_stats_t* stats
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable bio-async messaging for adaptive-FEP coordination
 * WHY:  Distributed learning signaling
 * HOW:  Register module, set up handlers
 *
 * @param bridge Adaptive-FEP bridge
 * @return 0 on success
 */
int adaptive_fep_bridge_connect_bio_async(adaptive_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Adaptive-FEP bridge
 * @return 0 on success
 */
int adaptive_fep_bridge_disconnect_bio_async(adaptive_fep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Adaptive-FEP bridge
 * @return true if bio-async enabled
 */
bool adaptive_fep_bridge_is_bio_async_connected(
    const adaptive_fep_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ADAPTIVE_FEP_BRIDGE_H */
