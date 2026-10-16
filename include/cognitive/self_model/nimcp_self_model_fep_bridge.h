/**
 * @file nimcp_self_model_fep_bridge.h
 * @brief Free Energy Principle - Self-Model Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between Free Energy Principle and Self-Model system
 * WHY:  Self-awareness requires a meta-generative model of self. FEP minimizes prediction
 *       errors about self-states, enabling metacognition and self-model refinement.
 * HOW:  FEP operates over self-representation, using prediction errors to update
 *       self-beliefs; self-model provides priors for FEP inference about self.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SELF-MODEL AS META-GENERATIVE MODEL:
 * ------------------------------------
 * 1. Medial Prefrontal Cortex (mPFC) as Self-Prior:
 *    - mPFC maintains generative model of self
 *    - Self-referential processing minimizes self-prediction errors
 *    - Reference: Qin & Northoff (2011) "How is our self related to midline regions
 *      and the default-mode network?"
 *
 * 2. Default Mode Network (DMN) and FEP:
 *    - DMN active during self-reflection (FEP inference about self)
 *    - Minimizes surprise about internal states
 *    - Reference: Carhart-Harris & Friston (2010) "The default-mode, ego-functions
 *      and free-energy: a neurobiological account of Freudian ideas"
 *
 * 3. Interoceptive Inference:
 *    - Bodily self-model via predictive processing
 *    - Insula computes interoceptive prediction errors
 *    - Reference: Seth (2013) "Interoceptive inference, emotion, and the embodied self"
 *
 * 4. Self-Belief Updating:
 *    - Self-beliefs as Bayesian priors
 *    - Prediction errors update self-model (identity revision)
 *    - Reference: Apps & Tsakiris (2014) "The free-energy self: A predictive coding
 *      account of self-recognition"
 *
 * FEP → SELF-MODEL PATHWAYS:
 * --------------------------
 * 1. Prediction Errors Refine Self-Beliefs:
 *    - High PE about self-capabilities → belief update
 *    - Capability assessment error → proficiency revision
 *    - Self-state prediction errors → mental state update
 *
 * 2. Free Energy Drives Self-Knowledge:
 *    - Minimizing self-related F → self-understanding
 *    - Epistemic foraging for self-knowledge
 *    - Exploration of self-capabilities
 *
 * 3. Meta-Uncertainty about Self:
 *    - FEP uncertainty quantifies self-knowledge gaps
 *    - High uncertainty → "I don't know myself well"
 *    - Low uncertainty → "I am certain about who I am"
 *
 * SELF-MODEL → FEP PATHWAYS:
 * --------------------------
 * 1. Self-Beliefs as Priors:
 *    - Core beliefs shape FEP inference
 *    - "I am good at X" → higher prior for success
 *    - "I cannot do Y" → constrained action space
 *
 * 2. Self-Boundaries Constrain Inference:
 *    - Self vs non-self boundary → sensory attenuation
 *    - Agency detection via prediction error
 *    - Reference: Friston (2011) "What is optimal about motor control?"
 *
 * 3. Capability Assessments Guide Active Inference:
 *    - Known capabilities → policy selection
 *    - Limitations → action pruning
 *    - Proficiency → precision weighting
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    FEP-SELF-MODEL BRIDGE                                   ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  FEP → SELF-MODEL PATHWAYS                          │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ Self-Prediction  │ ──→ Self-Belief Update                      │  ║
 * ║   │   │ Errors           │ ──→ Capability Revision                     │  ║
 * ║   │   │ (capability PE)  │ ──→ Mental State Correction                 │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ Epistemic Value  │ ──→ Self-Knowledge Foraging                 │  ║
 * ║   │   │ (self-related)   │ ──→ Capability Exploration                  │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ Meta-Uncertainty │ ──→ "How well do I know myself?"            │  ║
 * ║   │   │ (self FE)        │ ──→ Self-Confidence Estimation              │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  SELF-MODEL → FEP PATHWAYS                          │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ Core Beliefs     │ ──→ FEP Priors                              │  ║
 * ║   │   │ "I am X"         │ ──→ Inference Constraints                   │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ Capability Map   │ ──→ Policy Selection                        │  ║
 * ║   │   │ (proficiencies)  │ ──→ Action Pruning                          │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ Self-Boundaries  │ ──→ Sensory Attenuation                     │  ║
 * ║   │   │ (agency)         │ ──→ Self vs World Distinction               │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
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

#ifndef NIMCP_SELF_MODEL_FEP_BRIDGE_H
#define NIMCP_SELF_MODEL_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/nimcp_self_model.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Self-prediction error thresholds */
#define SELF_FEP_PE_CAPABILITY_THRESHOLD     3.0f   /**< Capability mismatch */
#define SELF_FEP_PE_BELIEF_THRESHOLD         5.0f   /**< Belief violation */
#define SELF_FEP_PE_IDENTITY_THRESHOLD       10.0f  /**< Identity crisis */

/* Self-knowledge uncertainty thresholds */
#define SELF_FEP_UNCERTAINTY_LOW             0.2f   /**< Know myself well */
#define SELF_FEP_UNCERTAINTY_MEDIUM          0.5f   /**< Moderate self-knowledge */
#define SELF_FEP_UNCERTAINTY_HIGH            0.8f   /**< Don't know myself */

/* Belief update rates */
#define SELF_FEP_BELIEF_UPDATE_RATE          0.1f   /**< Belief revision speed */
#define SELF_FEP_CAPABILITY_UPDATE_RATE      0.2f   /**< Capability learning speed */
#define SELF_FEP_CORE_BELIEF_RESISTANCE      0.95f  /**< Core beliefs resist change */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct self_model_fep_bridge self_model_fep_bridge_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for Self-Model FEP bridge
 */
typedef struct {
    /* Thresholds */
    float capability_pe_threshold;        /**< PE → capability update */
    float belief_pe_threshold;            /**< PE → belief update */
    float self_uncertainty_threshold;     /**< Uncertainty → exploration */

    /* Update rates */
    float belief_update_rate;             /**< Belief revision speed */
    float capability_update_rate;         /**< Capability learning speed */
    float core_belief_resistance;         /**< Core belief inertia */

    /* Feature enables */
    bool enable_belief_updates;           /**< PE → belief revision */
    bool enable_capability_learning;      /**< PE → proficiency update */
    bool enable_self_exploration;         /**< Uncertainty → foraging */
    bool enable_identity_protection;      /**< Resist identity-threatening PEs */

    /* Sensitivity factors */
    float pe_sensitivity;                 /**< PE effect scaling */
    float uncertainty_sensitivity;        /**< Uncertainty effect scaling */
} self_model_fep_config_t;

/**
 * @brief FEP effects on self-model
 */
typedef struct {
    /* Prediction errors */
    float capability_pe;                  /**< Capability mismatch PE */
    float belief_pe;                      /**< Belief violation PE */
    float identity_pe;                    /**< Identity-level PE */

    /* Uncertainty */
    float self_knowledge_uncertainty;     /**< Epistemic self-uncertainty */
    float capability_uncertainty;         /**< Capability confidence */

    /* Active inference */
    float exploration_drive;              /**< Self-exploration motivation */
    bool identity_crisis;                 /**< Major self-model violation */
} self_model_fep_effects_t;

/**
 * @brief Current state of Self-Model FEP interaction
 */
typedef struct {
    /* FEP-derived self-knowledge */
    float self_knowledge_certainty;       /**< How well do I know myself? */
    float capability_confidence;          /**< Confidence in abilities */

    /* Belief revision state */
    uint32_t beliefs_updated;             /**< Beliefs revised by FEP */
    uint32_t capabilities_updated;        /**< Capabilities revised by FEP */

    /* Exploration state */
    bool exploring_self;                  /**< Active self-exploration */
    float exploration_progress;           /**< Self-knowledge acquisition */

    /* Identity stability */
    bool identity_stable;                 /**< Stable self-concept */
    uint64_t last_identity_crisis_ms;     /**< Last major revision */
} self_model_fep_state_t;

/**
 * @brief Statistics for Self-Model FEP bridge
 */
typedef struct {
    /* Belief updates */
    uint64_t belief_updates_total;        /**< Total belief revisions */
    uint64_t capability_updates_total;    /**< Total capability updates */
    uint64_t identity_crises;             /**< Major self-model violations */

    /* Prediction errors */
    float avg_capability_pe;              /**< Average capability PE */
    float avg_belief_pe;                  /**< Average belief PE */
    float max_identity_pe;                /**< Worst identity violation */

    /* Self-knowledge */
    float avg_self_certainty;             /**< Average self-knowledge */
    uint64_t exploration_episodes;        /**< Self-exploration events */

    /* Performance */
    float avg_free_energy_self;           /**< Avg F about self */
} self_model_fep_stats_t;

/**
 * @brief Self-Model FEP bridge state
 */
struct self_model_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    self_model_fep_config_t config;

    /* Connected systems */
    fep_system_t* fep_system;             /**< FEP system */
    self_model_t* self_model_system;      /**< Self-model system */

    /* Current effects */
    self_model_fep_effects_t effects;
    self_model_fep_state_t state;

    /* Statistics */
    self_model_fep_stats_t stats;

};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default Self-Model FEP configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible defaults
 * HOW:  Set standard thresholds and enable all features
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int self_model_fep_bridge_default_config(self_model_fep_config_t* config);

/**
 * @brief Create Self-Model FEP bridge
 *
 * WHAT: Initialize Self-Model FEP integration bridge
 * WHY:  Enable bidirectional FEP-self-model interaction
 * HOW:  Allocate bridge, link systems, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
self_model_fep_bridge_t* self_model_fep_bridge_create(
    const self_model_fep_config_t* config
);

/**
 * @brief Destroy Self-Model FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect systems, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void self_model_fep_bridge_destroy(self_model_fep_bridge_t* bridge);

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
 * @param bridge Self-Model FEP bridge
 * @param fep FEP system
 * @return 0 on success
 */
int self_model_fep_bridge_connect_fep(
    self_model_fep_bridge_t* bridge,
    fep_system_t* fep
);

/**
 * @brief Connect self-model system
 *
 * WHAT: Link bridge to self-model system
 * WHY:  Enable self-model state monitoring and updates
 * HOW:  Store self-model system pointer
 *
 * @param bridge Self-Model FEP bridge
 * @param self_model Self-model system
 * @return 0 on success
 */
int self_model_fep_bridge_connect_self_model(
    self_model_fep_bridge_t* bridge,
    self_model_t* self_model
);

/* ============================================================================
 * FEP → Self-Model Direction
 * ============================================================================ */

/**
 * @brief Update self-belief based on prediction error
 *
 * WHAT: Revise self-belief when PE indicates belief inaccuracy
 * WHY:  Self-model refinement via Bayesian updating
 * HOW:  Adjust belief confidence/content based on PE magnitude
 *
 * @param bridge Self-Model FEP bridge
 * @param belief_index Index of belief to update
 * @param prediction_error PE magnitude
 * @return 0 on success
 */
int self_model_fep_update_belief(
    self_model_fep_bridge_t* bridge,
    uint32_t belief_index,
    float prediction_error
);

/**
 * @brief Update capability assessment based on performance PE
 *
 * WHAT: Revise capability proficiency based on success/failure PE
 * WHY:  Learn true capabilities via experience
 * HOW:  Adjust proficiency based on PE (high PE = overestimated)
 *
 * @param bridge Self-Model FEP bridge
 * @param capability_index Capability to update
 * @param prediction_error Performance PE
 * @return 0 on success
 */
int self_model_fep_update_capability(
    self_model_fep_bridge_t* bridge,
    uint32_t capability_index,
    float prediction_error
);

/**
 * @brief Trigger self-exploration based on uncertainty
 *
 * WHAT: Initiate epistemic foraging for self-knowledge
 * WHY:  High uncertainty about self → explore capabilities
 * HOW:  Drive exploration actions to reduce self-uncertainty
 *
 * @param bridge Self-Model FEP bridge
 * @return 0 on success
 */
int self_model_fep_explore_self(self_model_fep_bridge_t* bridge);

/* ============================================================================
 * Self-Model → FEP Direction
 * ============================================================================ */

/**
 * @brief Apply self-beliefs as FEP priors
 *
 * WHAT: Configure FEP priors based on self-beliefs
 * WHY:  Self-beliefs constrain inference ("I am good at X")
 * HOW:  Set FEP prior distributions from belief structure
 *
 * @param bridge Self-Model FEP bridge
 * @return 0 on success
 */
int self_model_fep_apply_belief_priors(self_model_fep_bridge_t* bridge);

/**
 * @brief Constrain FEP policy space by capabilities
 *
 * WHAT: Prune FEP action space based on known limitations
 * WHY:  Don't plan actions beyond capabilities
 * HOW:  Filter policies by capability assessments
 *
 * @param bridge Self-Model FEP bridge
 * @return 0 on success
 */
int self_model_fep_constrain_policies(self_model_fep_bridge_t* bridge);

/**
 * @brief Apply self-boundary sensory attenuation
 *
 * WHAT: Reduce precision of self-generated sensory predictions
 * WHY:  Self-generated actions are predictable (motor control)
 * HOW:  Attenuate precision for self-originated signals
 *
 * @param bridge Self-Model FEP bridge
 * @param is_self_generated True if action is self-originated
 * @return 0 on success
 */
int self_model_fep_apply_sensory_attenuation(
    self_model_fep_bridge_t* bridge,
    bool is_self_generated
);

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

/**
 * @brief Update Self-Model FEP bridge state
 *
 * WHAT: Main update loop for bidirectional integration
 * WHY:  Keep FEP and self-model systems synchronized
 * HOW:  Update effects, check PE thresholds, apply modifiers
 *
 * @param bridge Self-Model FEP bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int self_model_fep_bridge_update(
    self_model_fep_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 *
 * @param bridge Self-Model FEP bridge
 * @param state Output state
 * @return 0 on success
 */
int self_model_fep_bridge_get_state(
    const self_model_fep_bridge_t* bridge,
    self_model_fep_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Self-Model FEP bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int self_model_fep_bridge_get_stats(
    const self_model_fep_bridge_t* bridge,
    self_model_fep_stats_t* stats
);

/**
 * @brief Check if self-exploration is active
 *
 * @param bridge Self-Model FEP bridge
 * @return true if exploring self
 */
bool self_model_fep_is_exploring(const self_model_fep_bridge_t* bridge);

/**
 * @brief Get current self-knowledge certainty
 *
 * @param bridge Self-Model FEP bridge
 * @return Self-knowledge certainty [0-1]
 */
float self_model_fep_get_self_certainty(const self_model_fep_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable bio-async messaging for self-model FEP coordination
 * WHY:  Distributed self-awareness signaling
 * HOW:  Register module, set up handlers
 *
 * @param bridge Self-Model FEP bridge
 * @return 0 on success
 */
int self_model_fep_bridge_connect_bio_async(self_model_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Self-Model FEP bridge
 * @return 0 on success
 */
int self_model_fep_bridge_disconnect_bio_async(self_model_fep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Self-Model FEP bridge
 * @return true if bio-async enabled
 */
bool self_model_fep_bridge_is_bio_async_connected(
    const self_model_fep_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SELF_MODEL_FEP_BRIDGE_H */
