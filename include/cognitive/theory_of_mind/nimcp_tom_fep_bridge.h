/**
 * @file nimcp_tom_fep_bridge.h
 * @brief Free Energy Principle - Theory of Mind Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between Free Energy Principle and Theory of Mind
 * WHY:  Theory of Mind is hierarchical inference of others' mental states. FEP provides
 *       the computational framework for inferring nested generative models (beliefs about
 *       beliefs). ToM is FEP applied to social cognition.
 * HOW:  FEP infers others' beliefs/desires/intentions via nested models; ToM provides
 *       social priors and empathic predictions for FEP.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * THEORY OF MIND AS HIERARCHICAL FEP:
 * -----------------------------------
 * 1. Nested Generative Models:
 *    - Level 1: My beliefs about the world
 *    - Level 2: My beliefs about their beliefs about the world
 *    - Level 3: My beliefs about their beliefs about my beliefs...
 *    - Reference: Frith & Frith (2006) "The neural basis of mentalizing"
 *
 * 2. Temporoparietal Junction (TPJ) as Social Inference:
 *    - TPJ computes social prediction errors
 *    - Processes belief-reality mismatches
 *    - Enables false belief understanding
 *    - Reference: Saxe & Kanwisher (2003) "People thinking about thinking people"
 *
 * 3. Medial Prefrontal Cortex (mPFC) as Social Prior:
 *    - mPFC maintains generative models of others
 *    - Stores trait beliefs ("Alice is honest")
 *    - Reference: Mitchell et al. (2006) "Thinking about others: The neural substrates
 *      of social cognition"
 *
 * 4. Mirror Neurons as Action Prediction:
 *    - Mirror system predicts others' actions via motor simulation
 *    - Prediction errors → intention inference
 *    - Reference: Kilner et al. (2007) "Predictive coding: an account of the mirror
 *      neuron system"
 *
 * FEP → ToM PATHWAYS:
 * ------------------
 * 1. Belief Inference via Prediction Errors:
 *    - Observe action → predict outcome
 *    - PE → infer hidden beliefs ("they believe X")
 *    - False belief = high PE between observed and expected
 *
 * 2. Intention Inference via Active Inference:
 *    - Actions minimize expected free energy
 *    - Observe action → infer goal (minimize which G?)
 *    - Reference: Baker et al. (2009) "Action understanding as inverse planning"
 *
 * 3. Empathy via Shared Generative Models:
 *    - Simulate others' emotional states
 *    - Shared precision → empathic resonance
 *    - Reference: Seth & Friston (2016) "Active interoceptive inference and the
 *      emotional brain"
 *
 * ToM → FEP PATHWAYS:
 * ------------------
 * 1. Social Priors Constrain Inference:
 *    - Known traits → belief priors ("Bob is honest" → trust predictions)
 *    - Relationship history → precision weighting
 *    - Social norms → policy constraints
 *
 * 2. Empathic Predictions:
 *    - Inferred emotions → precision modulation
 *    - Others' distress → heightened precision
 *    - Compassion → action selection
 *
 * 3. Mentalizing Overhead:
 *    - Nested models → increased free energy
 *    - Social complexity → cognitive load
 *    - Reference: Frith & Frith (2012) "Mechanisms of social cognition"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    FEP-THEORY OF MIND BRIDGE                               ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  FEP → ToM PATHWAYS                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ Action PE        │ ──→ Intention Inference                     │  ║
 * ║   │   │ (unexpected act) │ ──→ "Why did they do that?"                 │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ Outcome PE       │ ──→ Belief Inference                        │  ║
 * ║   │   │ (belief mismatch)│ ──→ False Belief Detection                  │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ Motor Prediction │ ──→ Empathy (mirror simulation)             │  ║
 * ║   │   │ (shared model)   │ ──→ Emotional Resonance                     │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  ToM → FEP PATHWAYS                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ Trait Beliefs    │ ──→ Social Priors                           │  ║
 * ║   │   │ ("Alice honest") │ ──→ Trust Precision                         │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ Inferred Emotion │ ──→ Empathic Action                         │  ║
 * ║   │   │ (distress)       │ ──→ Helping Behavior                        │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ Nested Models    │ ──→ Mentalizing Overhead                    │  ║
 * ║   │   │ (recursion depth)│ ──→ Cognitive Load                          │  ║
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

#ifndef NIMCP_TOM_FEP_BRIDGE_H
#define NIMCP_TOM_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/nimcp_theory_of_mind.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Social prediction error thresholds */
#define TOM_FEP_PE_ACTION_THRESHOLD          3.0f   /**< Unexpected action */
#define TOM_FEP_PE_BELIEF_THRESHOLD          5.0f   /**< False belief */
#define TOM_FEP_PE_INTENTION_THRESHOLD       7.0f   /**< Intention mismatch */

/* Mentalizing complexity */
#define TOM_FEP_MAX_RECURSION_DEPTH          3      /**< "I think you think I think..." */
#define TOM_FEP_COMPLEXITY_PENALTY           1.5f   /**< FE cost per recursion level */

/* Empathy thresholds */
#define TOM_FEP_EMPATHY_THRESHOLD            0.6f   /**< Empathic resonance */
#define TOM_FEP_DISTRESS_THRESHOLD           0.7f   /**< Trigger helping */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct tom_fep_bridge tom_fep_bridge_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for ToM FEP bridge
 */
typedef struct {
    /* Thresholds */
    float action_pe_threshold;            /**< PE → intention inference */
    float belief_pe_threshold;            /**< PE → belief inference */
    float empathy_threshold;              /**< Empathic resonance threshold */

    /* Mentalizing */
    uint32_t max_recursion_depth;         /**< Max nested belief depth */
    float complexity_penalty;             /**< FE cost per recursion */

    /* Feature enables */
    bool enable_belief_inference;         /**< PE → belief updating */
    bool enable_intention_inference;      /**< PE → goal inference */
    bool enable_empathy;                  /**< Mirror simulation */
    bool enable_false_belief_detection;   /**< Detect belief-reality mismatch */

    /* Sensitivity factors */
    float pe_sensitivity;                 /**< PE effect scaling */
    float empathy_sensitivity;            /**< Empathy strength */
} tom_fep_config_t;

/**
 * @brief FEP effects on ToM
 */
typedef struct {
    /* Prediction errors */
    float action_pe;                      /**< Unexpected action PE */
    float belief_pe;                      /**< False belief PE */
    float intention_pe;                   /**< Intention mismatch PE */

    /* Inferences */
    bool belief_updated;                  /**< Belief inferred from PE */
    bool intention_inferred;              /**< Goal inferred from action */
    bool false_belief_detected;           /**< Belief-reality mismatch */

    /* Empathy */
    float empathic_resonance;             /**< Empathy strength [0-1] */
    tom_emotion_t inferred_emotion;       /**< Inferred emotional state */

    /* Mentalizing cost */
    uint32_t current_recursion_depth;     /**< Nested model depth */
    float mentalizing_overhead;           /**< Cognitive load */
} tom_fep_effects_t;

/**
 * @brief Current state of ToM FEP interaction
 */
typedef struct {
    /* Social inference */
    uint32_t beliefs_inferred;            /**< Beliefs inferred from PE */
    uint32_t intentions_inferred;         /**< Goals inferred from actions */
    uint32_t false_beliefs_detected;      /**< False belief episodes */

    /* Empathy */
    bool empathy_active;                  /**< Empathic simulation active */
    float empathy_magnitude;              /**< Current empathy strength */

    /* Mentalizing */
    uint32_t mentalizing_depth;           /**< Current recursion depth */
    float social_cognitive_load;          /**< Mentalizing overhead */

    /* Performance */
    float prediction_accuracy;            /**< Action prediction accuracy */
    uint64_t last_social_pe_ms;           /**< Last social PE */
} tom_fep_state_t;

/**
 * @brief Statistics for ToM FEP bridge
 */
typedef struct {
    /* Inferences */
    uint64_t belief_inferences_total;     /**< Total belief inferences */
    uint64_t intention_inferences_total;  /**< Total intention inferences */
    uint64_t false_beliefs_total;         /**< Total false belief detections */

    /* Prediction errors */
    float avg_action_pe;                  /**< Average action PE */
    float avg_belief_pe;                  /**< Average belief PE */

    /* Empathy */
    uint64_t empathy_episodes;            /**< Empathic resonance events */
    float avg_empathy_magnitude;          /**< Average empathy strength */

    /* Mentalizing */
    float avg_recursion_depth;            /**< Average nested depth */
    float avg_mentalizing_overhead;       /**< Average cognitive load */

    /* Performance */
    float avg_prediction_accuracy;        /**< Average action prediction */
} tom_fep_stats_t;

/**
 * @brief ToM FEP bridge state
 */
struct tom_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    tom_fep_config_t config;

    /* Connected systems */
    fep_system_t* fep_system;             /**< FEP system */
    theory_of_mind_t tom_system;          /**< Theory of Mind system */

    /* Current effects */
    tom_fep_effects_t effects;
    tom_fep_state_t state;

    /* Statistics */
    tom_fep_stats_t stats;

};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default ToM FEP configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible defaults
 * HOW:  Set standard thresholds and enable all features
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int tom_fep_bridge_default_config(tom_fep_config_t* config);

/**
 * @brief Create ToM FEP bridge
 *
 * WHAT: Initialize ToM FEP integration bridge
 * WHY:  Enable bidirectional FEP-ToM interaction
 * HOW:  Allocate bridge, link systems, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
tom_fep_bridge_t* tom_fep_bridge_create(const tom_fep_config_t* config);

/**
 * @brief Destroy ToM FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect systems, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void tom_fep_bridge_destroy(tom_fep_bridge_t* bridge);

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
 * @param bridge ToM FEP bridge
 * @param fep FEP system
 * @return 0 on success
 */
int tom_fep_bridge_connect_fep(tom_fep_bridge_t* bridge, fep_system_t* fep);

/**
 * @brief Connect Theory of Mind system
 *
 * WHAT: Link bridge to ToM system
 * WHY:  Enable ToM state monitoring and updates
 * HOW:  Store ToM system pointer
 *
 * @param bridge ToM FEP bridge
 * @param tom Theory of Mind system
 * @return 0 on success
 */
int tom_fep_bridge_connect_tom(tom_fep_bridge_t* bridge, theory_of_mind_t tom);

/* ============================================================================
 * FEP → ToM Direction
 * ============================================================================ */

/**
 * @brief Infer belief from prediction error
 *
 * WHAT: Infer others' beliefs when actions/outcomes violate predictions
 * WHY:  False beliefs detected via outcome PE
 * HOW:  High PE → "they believe X, but reality is Y"
 *
 * @param bridge ToM FEP bridge
 * @param prediction_error Outcome PE
 * @return 0 on success
 */
int tom_fep_infer_belief(tom_fep_bridge_t* bridge, float prediction_error);

/**
 * @brief Infer intention from action PE
 *
 * WHAT: Infer goals from unexpected actions (inverse planning)
 * WHY:  Actions minimize expected free energy → infer which goal
 * HOW:  Action PE → "they must want X to do that"
 *
 * @param bridge ToM FEP bridge
 * @param action_pe Action prediction error
 * @return 0 on success
 */
int tom_fep_infer_intention(tom_fep_bridge_t* bridge, float action_pe);

/**
 * @brief Activate empathy via shared generative model
 *
 * WHAT: Simulate others' emotional states via mirroring
 * WHY:  Empathy = shared precision-weighted predictions
 * HOW:  Run FEP inference with others' priors
 *
 * @param bridge ToM FEP bridge
 * @param observed_emotion Observed emotional cues
 * @return 0 on success
 */
int tom_fep_activate_empathy(
    tom_fep_bridge_t* bridge,
    tom_emotion_t observed_emotion
);

/* ============================================================================
 * ToM → FEP Direction
 * ============================================================================ */

/**
 * @brief Apply social priors from trait beliefs
 *
 * WHAT: Configure FEP priors based on known traits
 * WHY:  "Alice is honest" → trust her predictions
 * HOW:  Set FEP prior distributions from trait beliefs
 *
 * @param bridge ToM FEP bridge
 * @return 0 on success
 */
int tom_fep_apply_social_priors(tom_fep_bridge_t* bridge);

/**
 * @brief Modulate precision based on inferred emotion
 *
 * WHAT: Adjust precision weighting from empathic state
 * WHY:  Others' distress → heightened attention
 * HOW:  Increase precision for emotional signals
 *
 * @param bridge ToM FEP bridge
 * @param emotion Inferred emotion
 * @return 0 on success
 */
int tom_fep_modulate_empathic_precision(
    tom_fep_bridge_t* bridge,
    tom_emotion_t emotion
);

/**
 * @brief Add mentalizing overhead to free energy
 *
 * WHAT: Increase free energy for nested model inference
 * WHY:  "I think you think I think..." is cognitively costly
 * HOW:  Add FE penalty proportional to recursion depth
 *
 * @param bridge ToM FEP bridge
 * @param recursion_depth Depth of nested beliefs
 * @return 0 on success
 */
int tom_fep_add_mentalizing_overhead(
    tom_fep_bridge_t* bridge,
    uint32_t recursion_depth
);

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

/**
 * @brief Update ToM FEP bridge state
 *
 * WHAT: Main update loop for bidirectional integration
 * WHY:  Keep FEP and ToM systems synchronized
 * HOW:  Update effects, check PE thresholds, apply modifiers
 *
 * @param bridge ToM FEP bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int tom_fep_bridge_update(tom_fep_bridge_t* bridge, uint64_t delta_ms);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 *
 * @param bridge ToM FEP bridge
 * @param state Output state
 * @return 0 on success
 */
int tom_fep_bridge_get_state(
    const tom_fep_bridge_t* bridge,
    tom_fep_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge ToM FEP bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int tom_fep_bridge_get_stats(
    const tom_fep_bridge_t* bridge,
    tom_fep_stats_t* stats
);

/**
 * @brief Check if empathy is active
 *
 * @param bridge ToM FEP bridge
 * @return true if empathy active
 */
bool tom_fep_is_empathy_active(const tom_fep_bridge_t* bridge);

/**
 * @brief Get current mentalizing depth
 *
 * @param bridge ToM FEP bridge
 * @return Current recursion depth
 */
uint32_t tom_fep_get_mentalizing_depth(const tom_fep_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable bio-async messaging for ToM FEP coordination
 * WHY:  Distributed social cognition signaling
 * HOW:  Register module, set up handlers
 *
 * @param bridge ToM FEP bridge
 * @return 0 on success
 */
int tom_fep_bridge_connect_bio_async(tom_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge ToM FEP bridge
 * @return 0 on success
 */
int tom_fep_bridge_disconnect_bio_async(tom_fep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge ToM FEP bridge
 * @return true if bio-async enabled
 */
bool tom_fep_bridge_is_bio_async_connected(const tom_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TOM_FEP_BRIDGE_H */
