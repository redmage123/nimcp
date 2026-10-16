/**
 * @file nimcp_stdp_fep_bridge.h
 * @brief Free Energy Principle - STDP Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between Free Energy Principle and STDP plasticity
 * WHY:  FEP prediction errors drive STDP learning; STDP weight changes minimize free energy.
 *       Essential for biologically realistic learning under predictive coding.
 * HOW:  FEP belief updates modulate STDP learning rates; prediction error magnitude
 *       scales synaptic plasticity; precision controls weight update sensitivity.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * FEP → STDP PATHWAYS:
 * --------------------
 * 1. Prediction Error Drives Synaptic Learning:
 *    - High PE → enhanced STDP (unexpected events drive plasticity)
 *    - Low PE → reduced STDP (predicted events require less learning)
 *    - PE magnitude scales learning rate
 *    - Reference: Friston (2010) "The free-energy principle: a unified brain theory"
 *
 * 2. Precision Weighting of Plasticity:
 *    - High precision → strong STDP (confident predictions)
 *    - Low precision → weak STDP (uncertain predictions)
 *    - Implements attention-gated learning
 *    - Reference: Feldman & Friston (2010) "Attention, uncertainty, and free-energy"
 *
 * 3. Belief Updates as Learning Signals:
 *    - FEP belief convergence → stable STDP thresholds
 *    - Rapid belief changes → dynamic STDP parameters
 *    - Hierarchical prediction errors at different timescales
 *
 * STDP → FEP PATHWAYS:
 * --------------------
 * 1. Weight Changes Update Generative Model:
 *    - STDP weight updates = model parameter updates
 *    - Synaptic potentiation/depression refines predictions
 *    - Long-term learning shapes generative model structure
 *
 * 2. Spike Timing as Temporal Prediction:
 *    - Pre-before-post (LTP) = correct temporal prediction
 *    - Post-before-pre (LTD) = incorrect temporal prediction
 *    - STDP implements temporal credit assignment for FEP
 *
 * INTEGRATION MECHANISMS:
 * -----------------------
 * - PE-scaled learning: η_effective = η_base × |PE| × precision
 * - Dopamine-PE interaction: Both modulate STDP (multiplicative)
 * - Burst amplification: Phasic DA × high PE → maximal learning
 * - Homeostatic balance: FEP complexity cost regulates STDP bounds
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

#ifndef NIMCP_STDP_FEP_BRIDGE_H
#define NIMCP_STDP_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "plasticity/stdp/nimcp_stdp.h"
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
#define STDP_FEP_PE_MIN_THRESHOLD        0.5f    /**< Min PE for learning */
#define STDP_FEP_PE_MAX_THRESHOLD       10.0f    /**< Max PE (saturation) */
#define STDP_FEP_PE_SCALING_FACTOR       1.0f    /**< PE → LR scaling */

/* Precision modulation factors */
#define STDP_FEP_PRECISION_MIN           0.1f    /**< Min precision scaling */
#define STDP_FEP_PRECISION_MAX           2.0f    /**< Max precision boost */
#define STDP_FEP_PRECISION_SENSITIVITY   1.5f    /**< Precision → LR scaling */

/* Learning rate modulation */
#define STDP_FEP_LR_MIN_FACTOR           0.1f    /**< Min LR scaling (10%) */
#define STDP_FEP_LR_MAX_FACTOR           5.0f    /**< Max LR scaling (500%) */
#define STDP_FEP_LR_BASELINE             1.0f    /**< Baseline LR factor */

/* Belief update thresholds */
#define STDP_FEP_BELIEF_CONVERGENCE_THRESHOLD 0.01f  /**< Converged if delta < threshold */
#define STDP_FEP_BELIEF_STABLE_DURATION       100    /**< Stability duration (ms) */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct stdp_fep_bridge stdp_fep_bridge_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for STDP-FEP bridge
 */
typedef struct {
    /* Thresholds */
    float pe_min_threshold;              /**< Min PE for learning */
    float pe_max_threshold;              /**< Max PE (saturation) */
    float precision_sensitivity;         /**< Precision → LR scaling */
    float lr_min_factor;                 /**< Min LR scaling */
    float lr_max_factor;                 /**< Max LR scaling */

    /* Feature enables */
    bool enable_pe_scaling;              /**< PE scales learning rate */
    bool enable_precision_weighting;     /**< Precision gates plasticity */
    bool enable_belief_modulation;       /**< Belief updates modulate STDP */
    bool enable_complexity_regularization; /**< FEP complexity limits weights */

    /* Sensitivity factors */
    float pe_sensitivity;                /**< PE effect scaling */
    float precision_gain;                /**< Precision effect scaling */
    float belief_sensitivity;            /**< Belief change → STDP effect */

    /* Quantum acceleration */
    bool enable_quantum_optimization;    /**< Use quantum-accelerated LR optimization */
} stdp_fep_config_t;

/**
 * @brief FEP effects on STDP
 */
typedef struct {
    /* Prediction error effects */
    float pe_magnitude;                  /**< Current PE magnitude */
    float pe_lr_scaling;                 /**< PE → learning rate scaling */

    /* Precision effects */
    float precision_value;               /**< Current precision */
    float precision_lr_scaling;          /**< Precision → LR scaling */

    /* Belief update effects */
    float belief_delta;                  /**< Recent belief change */
    float belief_lr_scaling;             /**< Belief change → LR scaling */

    /* Total effects */
    float total_lr_scaling;              /**< Combined LR scaling factor */
    float effective_learning_rate;       /**< Modulated learning rate */
} stdp_fep_effects_t;

/**
 * @brief Current state of STDP-FEP interaction
 */
typedef struct {
    /* Current FEP state */
    float current_pe;                    /**< Current prediction error */
    float current_precision;             /**< Current precision estimate */
    float current_free_energy;           /**< Current free energy */

    /* Applied modifiers */
    float lr_modulation;                 /**< Current LR modifier */
    float weight_regularization;         /**< Complexity-based regularization */

    /* Belief tracking */
    bool beliefs_converged;              /**< Beliefs stable */
    uint64_t convergence_duration;       /**< Time stable (ms) */
    float last_belief_delta;             /**< Recent belief change */

    /* Statistics */
    uint32_t pe_updates;                 /**< PE-driven updates */
    uint64_t last_update_time;           /**< Last update timestamp */
} stdp_fep_state_t;

/**
 * @brief Statistics for STDP-FEP bridge
 */
typedef struct {
    /* Learning events */
    uint64_t total_updates;              /**< Total bridge updates */
    uint64_t pe_scaled_events;           /**< PE-scaled learning events */
    uint64_t precision_gated_events;     /**< Precision-gated events */

    /* Effect magnitudes */
    float avg_pe_scaling;                /**< Average PE scaling factor */
    float avg_precision_scaling;         /**< Average precision scaling */
    float avg_lr_modulation;             /**< Average LR modulation */

    /* Model updates */
    uint64_t weight_updates;             /**< STDP weight changes */
    float total_weight_delta;            /**< Cumulative weight changes */

    /* Performance */
    float avg_free_energy;               /**< Average free energy */
    float avg_prediction_error;          /**< Average PE */
} stdp_fep_stats_t;

/**
 * @brief STDP-FEP bridge state
 */
struct stdp_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    stdp_fep_config_t config;

    /* Connected systems */
    fep_system_t* fep_system;            /**< FEP system */
    stdp_synapse_t* stdp_system;         /**< STDP synapse array */
    uint32_t num_synapses;               /**< Number of synapses */

    /* Current effects */
    stdp_fep_effects_t effects;
    stdp_fep_state_t state;

    /* Statistics */
    stdp_fep_stats_t stats;

    /* Quantum optimization */
    void* quantum_bridge;                /**< Quantum STDP optimizer bridge */
    uint64_t quantum_optimizations;      /**< Number of quantum-optimized updates */

};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default STDP-FEP configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible defaults
 * HOW:  Set standard thresholds and enable all features
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int stdp_fep_bridge_default_config(stdp_fep_config_t* config);

/**
 * @brief Create STDP-FEP bridge
 *
 * WHAT: Initialize STDP-FEP integration bridge
 * WHY:  Enable bidirectional STDP-FEP interaction
 * HOW:  Allocate bridge, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
stdp_fep_bridge_t* stdp_fep_bridge_create(const stdp_fep_config_t* config);

/**
 * @brief Destroy STDP-FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect systems, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void stdp_fep_bridge_destroy(stdp_fep_bridge_t* bridge);

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
 * @param bridge STDP-FEP bridge
 * @param fep FEP system
 * @return 0 on success
 */
int stdp_fep_bridge_connect_fep(
    stdp_fep_bridge_t* bridge,
    fep_system_t* fep
);

/**
 * @brief Connect STDP system
 *
 * WHAT: Link bridge to STDP synapses
 * WHY:  Enable STDP parameter modulation
 * HOW:  Store STDP synapse array pointer
 *
 * @param bridge STDP-FEP bridge
 * @param stdp_synapses STDP synapse array
 * @param num_synapses Number of synapses
 * @return 0 on success
 */
int stdp_fep_bridge_connect_stdp(
    stdp_fep_bridge_t* bridge,
    stdp_synapse_t* stdp_synapses,
    uint32_t num_synapses
);

/**
 * @brief Disconnect all systems
 *
 * WHAT: Unlink FEP and STDP systems
 * WHY:  Safe shutdown
 * HOW:  Clear system pointers
 *
 * @param bridge STDP-FEP bridge
 * @return 0 on success
 */
int stdp_fep_bridge_disconnect(stdp_fep_bridge_t* bridge);

/* ============================================================================
 * FEP → STDP Direction
 * ============================================================================ */

/**
 * @brief Apply prediction error scaling to learning rate
 *
 * WHAT: Modulate STDP learning rate by PE magnitude
 * WHY:  Unexpected events drive stronger plasticity
 * HOW:  η_effective = η_base × f(|PE|)
 *
 * @param bridge STDP-FEP bridge
 * @param pe Prediction error magnitude
 * @return LR scaling factor
 */
float stdp_fep_apply_pe_scaling(
    stdp_fep_bridge_t* bridge,
    float pe
);

/**
 * @brief Apply precision weighting to plasticity
 *
 * WHAT: Gate STDP by precision (confidence)
 * WHY:  High precision predictions drive stronger learning
 * HOW:  η_effective = η_base × precision^sensitivity
 *
 * @param bridge STDP-FEP bridge
 * @param precision Current precision estimate
 * @return LR scaling factor
 */
float stdp_fep_apply_precision_weighting(
    stdp_fep_bridge_t* bridge,
    float precision
);

/**
 * @brief Apply belief update modulation
 *
 * WHAT: Modulate STDP based on belief changes
 * WHY:  Rapid belief updates indicate active learning
 * HOW:  η_effective = η_base × (1 + belief_delta)
 *
 * @param bridge STDP-FEP bridge
 * @param belief_delta Recent belief change
 * @return LR scaling factor
 */
float stdp_fep_apply_belief_modulation(
    stdp_fep_bridge_t* bridge,
    float belief_delta
);

/**
 * @brief Get effective learning rate
 *
 * WHAT: Compute FEP-modulated learning rate for STDP
 * WHY:  Combine all FEP effects on plasticity
 * HOW:  Multiply base LR by PE, precision, and belief factors
 *
 * @param bridge STDP-FEP bridge
 * @param base_lr Base STDP learning rate
 * @return Effective learning rate
 */
float stdp_fep_get_effective_lr(
    const stdp_fep_bridge_t* bridge,
    float base_lr
);

/* ============================================================================
 * STDP → FEP Direction
 * ============================================================================ */

/**
 * @brief Report weight changes to FEP system
 *
 * WHAT: Update FEP generative model based on STDP weight changes
 * WHY:  Synaptic plasticity refines generative model parameters
 * HOW:  Convert weight deltas to model parameter updates
 *
 * @param bridge STDP-FEP bridge
 * @param weight_delta Total weight change
 * @return 0 on success
 */
int stdp_fep_report_weight_changes(
    stdp_fep_bridge_t* bridge,
    float weight_delta
);

/**
 * @brief Apply complexity regularization to STDP
 *
 * WHAT: Limit weight growth based on FEP complexity cost
 * WHY:  Model complexity should minimize free energy
 * HOW:  Penalize large weights based on complexity term
 *
 * @param bridge STDP-FEP bridge
 * @return Regularization factor [0-1]
 */
float stdp_fep_compute_complexity_regularization(
    const stdp_fep_bridge_t* bridge
);

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

/**
 * @brief Update STDP-FEP bridge state
 *
 * WHAT: Main update loop for bidirectional integration
 * WHY:  Keep STDP and FEP systems synchronized
 * HOW:  Update effects, apply modulation, track statistics
 *
 * @param bridge STDP-FEP bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int stdp_fep_bridge_update(
    stdp_fep_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 *
 * @param bridge STDP-FEP bridge
 * @param state Output state
 * @return 0 on success
 */
int stdp_fep_bridge_get_state(
    const stdp_fep_bridge_t* bridge,
    stdp_fep_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge STDP-FEP bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int stdp_fep_bridge_get_stats(
    const stdp_fep_bridge_t* bridge,
    stdp_fep_stats_t* stats
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable bio-async messaging for STDP-FEP coordination
 * WHY:  Distributed learning signaling
 * HOW:  Register module, set up handlers
 *
 * @param bridge STDP-FEP bridge
 * @return 0 on success
 */
int stdp_fep_bridge_connect_bio_async(stdp_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge STDP-FEP bridge
 * @return 0 on success
 */
int stdp_fep_bridge_disconnect_bio_async(stdp_fep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge STDP-FEP bridge
 * @return true if bio-async enabled
 */
bool stdp_fep_bridge_is_bio_async_connected(
    const stdp_fep_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_STDP_FEP_BRIDGE_H */
