/**
 * @file nimcp_mirror_neurons_fep_bridge.h
 * @brief Free Energy Principle - Mirror Neurons Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between Free Energy Principle and mirror neuron system
 * WHY:  Mirror neurons perform action understanding as FEP inference over others' generative models;
 *       FEP prediction errors drive mirror neuron activation and learning
 * HOW:  Mirror neuron goal/motor predictions update FEP beliefs; FEP errors modulate
 *       mirror neuron precision and goal inference
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * MIRROR NEURONS AS PREDICTIVE MODELS:
 * ------------------------------------
 * 1. Action Understanding as Inference:
 *    - Mirror neurons implement predictive coding for observed actions
 *    - Observing action → predict goal → match motor representation
 *    - Prediction errors → update action understanding
 *    - Reference: Kilner et al. (2007) "Predictive coding: An account of the
 *      mirror neuron system"
 *
 * 2. Goal Inference Through FEP:
 *    - Observed motor patterns → infer latent goals (hidden states)
 *    - Minimize free energy over goal hypotheses
 *    - Precision-weighted motor evidence
 *    - Reference: Friston et al. (2011) "Action understanding and active inference"
 *
 * 3. Motor Simulation and Prediction:
 *    - Top-down goal → predict motor trajectory
 *    - Compare prediction to observation
 *    - Motor prediction errors drive goal updates
 *    - Reference: Csibra (2007) "Action mirroring and action interpretation"
 *
 * FEP → MIRROR NEURONS PATHWAYS:
 * ------------------------------
 * 1. Precision Modulation:
 *    - FEP precision → mirror neuron gain
 *    - High precision → stronger motor resonance
 *    - Low precision → reduced mirroring
 *
 * 2. Goal Belief Updates:
 *    - FEP posterior beliefs → goal representations
 *    - Hierarchical belief updating
 *    - Goal-motor binding strengthening
 *
 * 3. Motor Prediction Errors:
 *    - FEP sensory PEs → motor prediction errors
 *    - Drive mirror neuron adaptation
 *    - Update goal-motor mappings
 *
 * MIRROR NEURONS → FEP PATHWAYS:
 * -------------------------------
 * 1. Goal as Hidden States:
 *    - Mirror neuron goal inferences → FEP hidden states
 *    - Goal probabilities → belief distributions
 *    - Feed into hierarchical FEP levels
 *
 * 2. Motor Evidence:
 *    - Observed motor patterns → sensory observations in FEP
 *    - Motor activations → likelihood p(o|s)
 *    - Goal-motor bindings → generative model structure
 *
 * 3. Action Understanding Predictions:
 *    - Mirror system predictions → top-down FEP predictions
 *    - Goal inferences → prior beliefs
 *    - Resonance strength → confidence (precision)
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                  MIRROR NEURONS - FEP BRIDGE                               ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  FEP → MIRROR NEURONS                               │  ║
 * ║   │                                                                     │  ║
 * ║   │   FEP Precision → Mirror Neuron Gain                              │  ║
 * ║   │   FEP Beliefs → Goal Representations                              │  ║
 * ║   │   FEP Errors → Motor Prediction Errors                            │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                 MIRROR NEURONS → FEP                                │  ║
 * ║   │                                                                     │  ║
 * ║   │   Goal Inferences → Hidden State Beliefs                          │  ║
 * ║   │   Motor Patterns → Sensory Observations                           │  ║
 * ║   │   Resonance Strength → Precision Estimates                        │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_MIRROR_NEURONS_FEP_BRIDGE_H
#define NIMCP_MIRROR_NEURONS_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/mirror_neurons/nimcp_mirror_hierarchy.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* FEP precision impact on mirror neurons */
#define FEP_PRECISION_MIRROR_GAIN_MIN     0.5f   /**< Minimum mirror gain */
#define FEP_PRECISION_MIRROR_GAIN_MAX     1.5f   /**< Maximum mirror gain */
#define FEP_PRECISION_DEFAULT             1.0f   /**< Baseline precision */

/* Goal-belief coupling thresholds */
#define GOAL_BELIEF_CONFIDENCE_THRESHOLD  0.6f   /**< Min confidence for goal update */
#define MOTOR_EVIDENCE_THRESHOLD          0.5f   /**< Min motor activation */

/* Prediction error thresholds */
#define MOTOR_PE_THRESHOLD_LOW            0.1f   /**< Minor mismatch */
#define MOTOR_PE_THRESHOLD_HIGH           0.5f   /**< Major mismatch */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct mirror_neurons_fep_bridge mirror_neurons_fep_bridge_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for mirror neurons-FEP bridge
 */
typedef struct {
    /* Precision modulation */
    float precision_gain_factor;           /**< FEP precision → mirror gain scaling */
    float min_mirror_gain;                 /**< Minimum mirror neuron gain */
    float max_mirror_gain;                 /**< Maximum mirror neuron gain */

    /* Goal-belief coupling */
    float goal_belief_coupling_rate;       /**< Goal → belief update rate */
    float belief_goal_coupling_rate;       /**< Belief → goal update rate */
    float confidence_threshold;            /**< Min confidence for updates */

    /* Motor evidence */
    float motor_evidence_weight;           /**< Motor → sensory evidence weight */
    float motor_evidence_threshold;        /**< Min motor activation */

    /* Prediction errors */
    float motor_pe_sensitivity;            /**< Sensitivity to motor PEs */
    float goal_pe_sensitivity;             /**< Sensitivity to goal PEs */

    /* Feature enables */
    bool enable_precision_modulation;      /**< FEP precision → mirror gain */
    bool enable_goal_belief_coupling;      /**< Goal ↔ belief updates */
    bool enable_motor_evidence;            /**< Motor → sensory evidence */
    bool enable_pe_propagation;            /**< Propagate prediction errors */
} mirror_neurons_fep_config_t;

/**
 * @brief Effects of FEP on mirror neurons
 */
typedef struct {
    float mirror_gain_modulation;          /**< Current mirror gain factor */
    float goal_activation_boost;           /**< Goal activation enhancement */
    float motor_prediction_error;          /**< Motor PE from FEP */
    float goal_prediction_error;           /**< Goal PE from FEP */
} mirror_neurons_fep_effects_t;

/**
 * @brief Current state of mirror neurons-FEP interaction
 */
typedef struct {
    /* FEP state */
    float current_precision;               /**< Current FEP precision */
    float current_free_energy;             /**< Current free energy */

    /* Mirror neuron state */
    uint32_t active_goals;                 /**< Number of active goals */
    uint32_t active_motors;                /**< Number of active motors */
    float max_goal_activation;             /**< Highest goal activation */
    float max_motor_activation;            /**< Highest motor activation */

    /* Coupling state */
    uint32_t goal_belief_updates;          /**< Goal → belief updates count */
    uint32_t belief_goal_updates;          /**< Belief → goal updates count */
    uint32_t motor_evidence_samples;       /**< Motor evidence provided */

    /* Prediction errors */
    float avg_motor_pe;                    /**< Average motor PE */
    float avg_goal_pe;                     /**< Average goal PE */
} mirror_neurons_fep_state_t;

/**
 * @brief Statistics for mirror neurons-FEP bridge
 */
typedef struct {
    /* Coupling events */
    uint64_t total_goal_updates;           /**< Total goal → belief updates */
    uint64_t total_belief_updates;         /**< Total belief → goal updates */
    uint64_t total_motor_evidence;         /**< Total motor evidence samples */

    /* Precision modulation */
    float avg_mirror_gain;                 /**< Average mirror gain */
    float min_mirror_gain_applied;         /**< Minimum gain applied */
    float max_mirror_gain_applied;         /**< Maximum gain applied */

    /* Prediction errors */
    float avg_motor_pe;                    /**< Average motor prediction error */
    float avg_goal_pe;                     /**< Average goal prediction error */
    uint64_t high_motor_pe_count;          /**< High motor PE events */
    uint64_t high_goal_pe_count;           /**< High goal PE events */

    /* Performance */
    float avg_goal_inference_accuracy;     /**< Goal inference accuracy */
    float avg_motor_prediction_accuracy;   /**< Motor prediction accuracy */
} mirror_neurons_fep_stats_t;

/**
 * @brief Mirror neurons-FEP bridge state
 */
struct mirror_neurons_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */
    struct nimcp_health_agent* health_agent; /**< Instance-level health agent */

    /* Configuration */
    mirror_neurons_fep_config_t config;

    /* Connected systems */
    fep_system_t* fep_system;              /**< FEP system */
    mirror_hierarchy_t mirror_system;      /**< Mirror neuron hierarchy */

    /* Current effects */
    mirror_neurons_fep_effects_t effects;
    mirror_neurons_fep_state_t state;

    /* Statistics */
    mirror_neurons_fep_stats_t stats;

};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default mirror neurons-FEP configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible defaults
 * HOW:  Set standard thresholds and enable all features
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int mirror_neurons_fep_bridge_default_config(mirror_neurons_fep_config_t* config);

/**
 * @brief Create mirror neurons-FEP bridge
 *
 * WHAT: Initialize mirror neurons-FEP integration bridge
 * WHY:  Enable bidirectional mirror neurons-FEP interaction
 * HOW:  Allocate bridge, link systems, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
mirror_neurons_fep_bridge_t* mirror_neurons_fep_bridge_create(
    const mirror_neurons_fep_config_t* config
);

/**
 * @brief Destroy mirror neurons-FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect systems, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void mirror_neurons_fep_bridge_destroy(mirror_neurons_fep_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect FEP system
 *
 * WHAT: Link bridge to FEP system
 * WHY:  Enable FEP state monitoring and modulation
 * HOW:  Store FEP system pointer
 *
 * @param bridge Mirror neurons-FEP bridge
 * @param fep FEP system
 * @return 0 on success
 */
int mirror_neurons_fep_bridge_connect_fep(
    mirror_neurons_fep_bridge_t* bridge,
    fep_system_t* fep
);

/**
 * @brief Connect mirror neuron system
 *
 * WHAT: Link bridge to mirror neuron hierarchy
 * WHY:  Enable mirror neuron state monitoring and modulation
 * HOW:  Store mirror system handle
 *
 * @param bridge Mirror neurons-FEP bridge
 * @param mirror Mirror neuron hierarchy
 * @return 0 on success
 */
int mirror_neurons_fep_bridge_connect_mirror_neurons(
    mirror_neurons_fep_bridge_t* bridge,
    mirror_hierarchy_t mirror
);

/* ============================================================================
 * FEP → Mirror Neurons Direction
 * ============================================================================ */

/**
 * @brief Apply FEP precision to mirror neuron gain
 *
 * WHAT: Modulate mirror neuron responsiveness based on FEP precision
 * WHY:  High precision → stronger mirroring; low precision → reduced
 * HOW:  Scale mirror neuron activations by precision factor
 *
 * @param bridge Mirror neurons-FEP bridge
 * @return 0 on success
 */
int mirror_neurons_fep_apply_precision_modulation(
    mirror_neurons_fep_bridge_t* bridge
);

/**
 * @brief Update mirror neuron goals from FEP beliefs
 *
 * WHAT: Transfer FEP posterior beliefs to goal representations
 * WHY:  FEP inference updates goal hypotheses
 * HOW:  Convert belief distributions to goal activations
 *
 * @param bridge Mirror neurons-FEP bridge
 * @return 0 on success
 */
int mirror_neurons_fep_update_goals_from_beliefs(
    mirror_neurons_fep_bridge_t* bridge
);

/**
 * @brief Propagate FEP prediction errors to mirror neurons
 *
 * WHAT: Use FEP errors to drive mirror neuron adaptation
 * WHY:  Prediction errors indicate model mismatches
 * HOW:  Convert sensory PEs to motor prediction errors
 *
 * @param bridge Mirror neurons-FEP bridge
 * @return 0 on success
 */
int mirror_neurons_fep_propagate_prediction_errors(
    mirror_neurons_fep_bridge_t* bridge
);

/* ============================================================================
 * Mirror Neurons → FEP Direction
 * ============================================================================ */

/**
 * @brief Transfer mirror neuron goals to FEP hidden states
 *
 * WHAT: Use goal inferences as FEP hidden state beliefs
 * WHY:  Goals represent latent causes of observed actions
 * HOW:  Convert goal probabilities to belief distributions
 *
 * @param bridge Mirror neurons-FEP bridge
 * @return 0 on success
 */
int mirror_neurons_fep_transfer_goals_to_beliefs(
    mirror_neurons_fep_bridge_t* bridge
);

/**
 * @brief Provide motor patterns as sensory evidence to FEP
 *
 * WHAT: Feed motor activations to FEP as observations
 * WHY:  Motor patterns are sensory evidence for goal inference
 * HOW:  Convert motor activations to sensory observations
 *
 * @param bridge Mirror neurons-FEP bridge
 * @return 0 on success
 */
int mirror_neurons_fep_provide_motor_evidence(
    mirror_neurons_fep_bridge_t* bridge
);

/**
 * @brief Set FEP precision from mirror neuron resonance
 *
 * WHAT: Use resonance strength as precision estimate
 * WHY:  Strong resonance → high confidence → high precision
 * HOW:  Map resonance to precision weights
 *
 * @param bridge Mirror neurons-FEP bridge
 * @return 0 on success
 */
int mirror_neurons_fep_set_precision_from_resonance(
    mirror_neurons_fep_bridge_t* bridge
);

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

/**
 * @brief Update mirror neurons-FEP bridge state
 *
 * WHAT: Main update loop for bidirectional integration
 * WHY:  Keep mirror neurons and FEP systems synchronized
 * HOW:  Update precision, goals, motor evidence, errors
 *
 * @param bridge Mirror neurons-FEP bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int mirror_neurons_fep_bridge_update(
    mirror_neurons_fep_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 *
 * @param bridge Mirror neurons-FEP bridge
 * @param state Output state
 * @return 0 on success
 */
int mirror_neurons_fep_bridge_get_state(
    const mirror_neurons_fep_bridge_t* bridge,
    mirror_neurons_fep_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Mirror neurons-FEP bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int mirror_neurons_fep_bridge_get_stats(
    const mirror_neurons_fep_bridge_t* bridge,
    mirror_neurons_fep_stats_t* stats
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable bio-async messaging for mirror neurons-FEP coordination
 * WHY:  Distributed action understanding signaling
 * HOW:  Register module, set up handlers
 *
 * @param bridge Mirror neurons-FEP bridge
 * @return 0 on success
 */
int mirror_neurons_fep_bridge_connect_bio_async(
    mirror_neurons_fep_bridge_t* bridge
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Mirror neurons-FEP bridge
 * @return 0 on success
 */
int mirror_neurons_fep_bridge_disconnect_bio_async(
    mirror_neurons_fep_bridge_t* bridge
);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Mirror neurons-FEP bridge
 * @return true if bio-async enabled
 */
bool mirror_neurons_fep_bridge_is_bio_async_connected(
    const mirror_neurons_fep_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MIRROR_NEURONS_FEP_BRIDGE_H */
