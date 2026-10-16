/**
 * @file nimcp_mirror_tom_bridge.h
 * @brief Mirror Neurons - Theory of Mind Bidirectional Bridge
 * @version 1.0.0
 * @date 2026-01-05
 *
 * WHAT: Bidirectional integration between mirror neurons and Theory of Mind
 * WHY:  Mirror neurons provide embodied simulation that grounds mental state inference;
 *       ToM predictions refine mirror neuron observation and imitation behavior
 * HOW:  Action simulation → intention inference → belief tracking with SIMD-optimized
 *       similarity computations for efficient multi-agent mental state comparison
 *
 * BIOLOGICAL BASIS:
 * ==============================================================================
 *
 * MIRROR NEURON FOUNDATION FOR THEORY OF MIND:
 * --------------------------------------------
 * Mirror neurons in premotor (F5) and parietal (IPL) cortex provide the
 * "embodied simulation" that grounds abstract mentalizing:
 *
 * 1. Action Understanding → Intention Inference (Gallese & Goldman, 1998)
 *    - Observing action → automatic motor simulation
 *    - Motor goals extracted from simulation → intention inference
 *    - Same neurons fire for self/other actions → self-other mapping
 *
 * 2. Emotional Resonance → Empathy (Carr et al., 2003)
 *    - Facial expression observation → emotional mirror neurons (insula)
 *    - Embodied emotional state → empathic understanding
 *    - Automatic unless inhibited by prefrontal cortex
 *
 * 3. Hierarchical Goal System → Belief-Desire Inference (Fogassi et al., 2005)
 *    - IPL mirror neurons encode action goals, not just movements
 *    - Goal-level representation enables desire inference
 *    - Context modulates goal interpretation (same action, different goals)
 *
 * 4. Predictive Coding → False Belief Detection (Kilner et al., 2007)
 *    - Mirror system generates action predictions
 *    - Prediction errors signal unexpected behavior
 *    - Large prediction error → possible false belief in observed agent
 *
 * NEURAL PATHWAYS:
 * ----------------
 *   Visual Input → STS → Mirror Neurons (F5/IPL) → mPFC/TPJ → ToM System
 *                                                      ↑
 *                                                 Prediction Error
 *
 * KEY REFERENCES:
 * - Gallese V & Goldman A (1998). Mirror neurons and the simulation theory.
 * - Rizzolatti G & Sinigaglia C (2010). The mirror mechanism and social cognition.
 * - Keysers C & Gazzola V (2007). Social neuroscience: Mirror neurons in the brain.
 *
 * ARCHITECTURE:
 * ==============================================================================
 * ```
 * ┌═══════════════════════════════════════════════════════════════════════════┐
 * │                    MIRROR-TOM BIDIRECTIONAL BRIDGE                         │
 * ├═══════════════════════════════════════════════════════════════════════════┤
 * │                                                                            │
 * │   ┌────────────────────────────────────────────────────────────────────┐  │
 * │   │              MIRROR NEURONS → THEORY OF MIND                        │  │
 * │   │                                                                     │  │
 * │   │  ┌─────────────┐    ┌─────────────┐    ┌──────────────────────┐   │  │
 * │   │  │   Action    │───>│    Goal     │───>│    Intention         │   │  │
 * │   │  │ Observation │    │  Inference  │    │    Attribution       │   │  │
 * │   │  └─────────────┘    └─────────────┘    └──────────────────────┘   │  │
 * │   │                                                                     │  │
 * │   │  ┌─────────────┐    ┌─────────────┐    ┌──────────────────────┐   │  │
 * │   │  │  Emotional  │───>│   Empathic  │───>│    Emotion           │   │  │
 * │   │  │  Resonance  │    │  Simulation │    │    Inference         │   │  │
 * │   │  └─────────────┘    └─────────────┘    └──────────────────────┘   │  │
 * │   │                                                                     │  │
 * │   │  ┌─────────────┐    ┌─────────────┐    ┌──────────────────────┐   │  │
 * │   │  │ Prediction  │───>│   Surprise  │───>│   False Belief       │   │  │
 * │   │  │   Error     │    │   Signal    │    │   Detection          │   │  │
 * │   │  └─────────────┘    └─────────────┘    └──────────────────────┘   │  │
 * │   └────────────────────────────────────────────────────────────────────┘  │
 * │                                                                            │
 * │   ┌────────────────────────────────────────────────────────────────────┐  │
 * │   │              THEORY OF MIND → MIRROR NEURONS                        │  │
 * │   │                                                                     │  │
 * │   │  ┌─────────────┐    ┌─────────────┐    ┌──────────────────────┐   │  │
 * │   │  │  Intention  │───>│ Observation │───>│   Expectation        │   │  │
 * │   │  │ Prediction  │    │    Bias     │    │   Priming            │   │  │
 * │   │  └─────────────┘    └─────────────┘    └──────────────────────┘   │  │
 * │   │                                                                     │  │
 * │   │  ┌─────────────┐    ┌─────────────┐    ┌──────────────────────┐   │  │
 * │   │  │   Social    │───>│  Resonance  │───>│   Imitation          │   │  │
 * │   │  │ Evaluation  │    │    Gain     │    │   Gating             │   │  │
 * │   │  └─────────────┘    └─────────────┘    └──────────────────────┘   │  │
 * │   │                                                                     │  │
 * │   │  ┌─────────────┐    ┌─────────────┐    ┌──────────────────────┐   │  │
 * │   │  │   Belief    │───>│  Deception  │───>│   Selective          │   │  │
 * │   │  │  Tracking   │    │  Detection  │    │   Suppression        │   │  │
 * │   │  └─────────────┘    └─────────────┘    └──────────────────────┘   │  │
 * │   └────────────────────────────────────────────────────────────────────┘  │
 * │                                                                            │
 * │   ┌────────────────────────────────────────────────────────────────────┐  │
 * │   │                    SIMD-OPTIMIZED COMPUTATIONS                      │  │
 * │   │                                                                     │  │
 * │   │  • tensor_simd_dot_f32: Action-intention similarity                │  │
 * │   │  • tensor_simd_sum_sq_f32: Belief state norms                      │  │
 * │   │  • Batch processing: Multi-agent mental state comparison           │  │
 * │   └────────────────────────────────────────────────────────────────────┘  │
 * │                                                                            │
 * └═══════════════════════════════════════════════════════════════════════════┘
 * ```
 *
 * BIO-ASYNC MODULE ID: 0x0280 (MIRROR_TOM_BRIDGE)
 *
 * NIMCP CODING STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT/WHY/HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free
 * - SIMD-optimized similarity computations
 *
 * @see nimcp_mirror_neurons.h
 * @see nimcp_theory_of_mind.h
 * @see nimcp_tensor_simd.h
 */

#ifndef NIMCP_MIRROR_TOM_BRIDGE_H
#define NIMCP_MIRROR_TOM_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Forward declarations to avoid circular dependencies */
typedef struct mirror_neurons_system* mirror_neurons_t;
typedef struct theory_of_mind_impl* theory_of_mind_t;

/* Thread synchronization */
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Bio-Async Module ID
 * ============================================================================ */

/* Note: BIO_MODULE_MIRROR_TOM_BRIDGE (0x0280) is defined in nimcp_bio_messages.h */

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum agents to track mental states for */
#define MIRROR_TOM_MAX_AGENTS                   32

/** @brief Maximum belief state vector dimension */
#define MIRROR_TOM_BELIEF_STATE_DIM             64

/** @brief Maximum intention vector dimension */
#define MIRROR_TOM_INTENTION_DIM                32

/** @brief Action similarity threshold for intention inference */
#define MIRROR_TOM_ACTION_INTENT_THRESHOLD      0.6f

/** @brief Prediction error threshold for false belief detection */
#define MIRROR_TOM_FALSE_BELIEF_PE_THRESHOLD    0.5f

/** @brief Emotional resonance threshold for empathy */
#define MIRROR_TOM_EMPATHY_RESONANCE_THRESHOLD  0.4f

/** @brief Deception confidence threshold for imitation suppression */
#define MIRROR_TOM_DECEPTION_SUPPRESS_THRESHOLD 0.7f

/** @brief Trust threshold for full resonance gain */
#define MIRROR_TOM_TRUST_RESONANCE_THRESHOLD    0.5f

/** @brief Mirror-to-ToM update coupling rate */
#define MIRROR_TOM_COUPLING_RATE                0.3f

/** @brief ToM-to-Mirror modulation gain range */
#define MIRROR_TOM_MODULATION_MIN               0.2f
#define MIRROR_TOM_MODULATION_MAX               1.5f

/** @brief SIMD batch processing threshold (elements) */
#define MIRROR_TOM_SIMD_BATCH_THRESHOLD         16

/* ============================================================================
 * Bio-Async Message Types
 * ============================================================================ */

/** @brief Mirror observation triggers intention inference */
#define BIO_MSG_MIRROR_TOM_INTENTION_INFER      0x2801

/** @brief Mirror resonance triggers empathy */
#define BIO_MSG_MIRROR_TOM_EMPATHY_TRIGGER      0x2802

/** @brief Prediction error signals false belief */
#define BIO_MSG_MIRROR_TOM_FALSE_BELIEF         0x2803

/** @brief ToM prediction biases observation */
#define BIO_MSG_TOM_MIRROR_OBSERVATION_BIAS     0x2804

/** @brief ToM evaluation modulates resonance */
#define BIO_MSG_TOM_MIRROR_RESONANCE_GAIN       0x2805

/** @brief Deception detected, suppress imitation */
#define BIO_MSG_TOM_MIRROR_DECEPTION_SUPPRESS   0x2806

/* ============================================================================
 * Emotion Types (aligned with ToM module)
 * ============================================================================ */

/**
 * @brief Emotion types for mirror-ToM empathy pathway
 */
typedef enum {
    MIRROR_TOM_EMOTION_UNKNOWN = 0,
    MIRROR_TOM_EMOTION_NEUTRAL,
    MIRROR_TOM_EMOTION_JOY,
    MIRROR_TOM_EMOTION_SADNESS,
    MIRROR_TOM_EMOTION_ANGER,
    MIRROR_TOM_EMOTION_FEAR,
    MIRROR_TOM_EMOTION_SURPRISE,
    MIRROR_TOM_EMOTION_DISGUST,
    MIRROR_TOM_EMOTION_COUNT
} mirror_tom_emotion_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Mirror neuron observation data for ToM inference
 *
 * WHAT: Observation data extracted from mirror neuron activation
 * WHY:  Provides embodied grounding for mental state inference
 */
typedef struct {
    float action_features[MIRROR_TOM_INTENTION_DIM];  /**< Action feature vector */
    uint32_t action_dim;                              /**< Actual dimension used */
    float resonance_strength;                         /**< Motor resonance level (0-1) */
    float goal_confidence;                            /**< Goal inference confidence (0-1) */
    uint32_t inferred_goal_id;                        /**< Hierarchical goal ID */
    mirror_tom_emotion_t emotional_resonance;         /**< Emotional mirror state */
    float emotional_intensity;                        /**< Emotion intensity (0-1) */
    float prediction_error;                           /**< Action prediction error (0-1) */
    uint64_t timestamp_us;                            /**< Observation timestamp */
} mirror_tom_observation_t;

/**
 * @brief ToM mental state for mirror neuron modulation
 *
 * WHAT: Mental state inference from ToM to modulate mirroring
 * WHY:  Social evaluation affects resonance and imitation tendency
 */
typedef struct {
    uint32_t agent_id;                                /**< Agent being evaluated */
    float trust_level;                                /**< Trust in agent (0-1) */
    float deception_likelihood;                       /**< Probability of deception (0-1) */
    float intention_alignment;                        /**< Alignment with own goals (-1 to 1) */
    float social_distance;                            /**< Social distance (0=close, 1=distant) */
    float competence_estimate;                        /**< Perceived competence (0-1) */
    bool is_false_belief_holder;                      /**< Agent holds false belief */
    char predicted_intention[128];                    /**< Predicted next action description */
    float intention_confidence;                       /**< Confidence in prediction (0-1) */
} mirror_tom_mental_state_t;

/**
 * @brief Per-agent tracking state
 *
 * WHAT: Accumulated state for each tracked agent
 * WHY:  Enable persistent mental state modeling across observations
 */
typedef struct {
    uint32_t agent_id;                                /**< Agent identifier */
    bool is_active;                                   /**< Slot is in use */

    /* Belief state vector (SIMD-aligned) */
    float belief_state[MIRROR_TOM_BELIEF_STATE_DIM];  /**< Belief representation */
    uint32_t belief_dim;                              /**< Actual dimension */

    /* Intention tracking */
    float intention_history[8][MIRROR_TOM_INTENTION_DIM]; /**< Recent intentions */
    uint32_t intention_count;                         /**< History count */
    float intention_consistency;                      /**< How consistent are intentions */

    /* Mental state */
    mirror_tom_mental_state_t mental_state;           /**< Current mental state model */

    /* Statistics */
    uint32_t observation_count;                       /**< Total observations */
    uint32_t false_belief_count;                      /**< False beliefs detected */
    float avg_prediction_error;                       /**< Average prediction error */
    uint64_t first_seen_us;                           /**< First observation */
    uint64_t last_seen_us;                            /**< Last observation */
} mirror_tom_agent_state_t;

/**
 * @brief Bridge configuration
 *
 * WHAT: Parameters controlling mirror-ToM integration
 * WHY:  Allow tuning of coupling strength and thresholds
 */
typedef struct {
    /* Mirror → ToM thresholds */
    float action_intent_threshold;        /**< Action similarity for intent inference */
    float false_belief_pe_threshold;      /**< Prediction error for false belief */
    float empathy_resonance_threshold;    /**< Resonance for empathy activation */

    /* ToM → Mirror modulation */
    float trust_resonance_threshold;      /**< Trust level for full resonance */
    float deception_suppress_threshold;   /**< Deception confidence for suppression */
    float modulation_min;                 /**< Minimum resonance modulation */
    float modulation_max;                 /**< Maximum resonance modulation */

    /* Coupling parameters */
    float coupling_rate;                  /**< Update coupling strength */
    float belief_decay_rate;              /**< Decay for unused beliefs */

    /* Feature flags */
    bool enable_intention_inference;      /**< Enable mirror → ToM intent */
    bool enable_empathy_pathway;          /**< Enable emotional resonance */
    bool enable_false_belief_detection;   /**< Enable prediction error → false belief */
    bool enable_deception_suppression;    /**< Enable ToM → mirror suppression */
    bool enable_simd_optimization;        /**< Use SIMD for similarity computations */
    bool bio_async_enabled;              /**< Enable bio-async messaging (default: true) */
} mirror_tom_config_t;

/**
 * @brief Mirror → ToM effects
 *
 * WHAT: Effects of mirror neuron activity on ToM processing
 * WHY:  Track embodied simulation contributions to mentalizing
 */
typedef struct {
    float intention_inference_strength;   /**< How much mirrors inform intent (0-1) */
    float empathy_contribution;           /**< Emotional resonance to ToM (0-1) */
    float false_belief_signal;            /**< Prediction error signal (0-1) */
    float action_understanding_depth;     /**< How deeply action is understood (0-1) */
    uint32_t goals_inferred;              /**< Number of goals extracted */
} mirror_to_tom_effects_t;

/**
 * @brief ToM → Mirror effects
 *
 * WHAT: Effects of ToM processing on mirror neuron activity
 * WHY:  Track top-down social modulation of mirroring
 */
typedef struct {
    float observation_bias;               /**< Bias toward expected actions (-1 to 1) */
    float resonance_gain;                 /**< Modulation of resonance strength */
    float imitation_gate;                 /**< Imitation likelihood (0-1) */
    bool deception_suppression_active;    /**< Currently suppressing imitation */
    uint32_t suppressed_agent_id;         /**< Agent being suppressed */
} tom_to_mirror_effects_t;

/**
 * @brief Bridge statistics
 *
 * WHAT: Runtime statistics for monitoring
 * WHY:  Track bridge health and performance
 */
typedef struct {
    /* Observation stats */
    uint32_t total_observations;          /**< Total mirror observations processed */
    uint32_t intentions_inferred;         /**< Successful intention inferences */
    uint32_t false_beliefs_detected;      /**< False beliefs detected */
    uint32_t empathy_activations;         /**< Empathy pathway activations */

    /* Modulation stats */
    uint32_t resonance_modulations;       /**< Times resonance was modulated */
    uint32_t deception_suppressions;      /**< Deception-based suppressions */
    float avg_resonance_gain;             /**< Average resonance modulation */

    /* Performance stats */
    uint32_t simd_computations;           /**< SIMD-accelerated operations */
    uint32_t scalar_computations;         /**< Scalar fallback operations */
    uint64_t total_compute_time_us;       /**< Total computation time */

    /* Agent tracking */
    uint32_t agents_tracked;              /**< Currently tracked agents */
    uint32_t peak_agents;                 /**< Maximum agents tracked */

    uint64_t last_update_us;              /**< Last statistics update */
} mirror_tom_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct mirror_tom_bridge* mirror_tom_bridge_t;

/* ============================================================================
 * Core API - Lifecycle Management
 * ============================================================================ */

/**
 * @brief Create mirror-ToM bridge
 *
 * WHAT: Initialize bidirectional integration between mirror neurons and ToM
 * WHY:  Enable embodied simulation to ground mentalizing
 * HOW:  Allocate agent tracking, initialize SIMD context, configure thresholds
 *
 * COMPLEXITY: O(MAX_AGENTS)
 * THREAD-SAFE: Yes (creates new instance)
 *
 * @param config Configuration (NULL = use defaults)
 * @return Bridge handle or NULL on error
 */
mirror_tom_bridge_t mirror_tom_create(const mirror_tom_config_t* config);

/**
 * @brief Destroy mirror-ToM bridge
 *
 * WHAT: Release all bridge resources
 * WHY:  Prevent memory leaks
 * HOW:  Free agent states, disconnect systems
 *
 * COMPLEXITY: O(agents)
 * THREAD-SAFE: No (caller must ensure exclusive access)
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void mirror_tom_destroy(mirror_tom_bridge_t bridge);

/**
 * @brief Get default configuration
 *
 * WHAT: Return sensible default configuration
 * WHY:  Provide good starting point
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (no shared state)
 *
 * @return Default configuration
 */
mirror_tom_config_t mirror_tom_get_default_config(void);

/**
 * @brief Connect to mirror neuron system
 *
 * WHAT: Establish connection to mirror neuron module
 * WHY:  Receive observation events, send modulation signals
 * HOW:  Register callbacks for observation/resonance events
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must synchronize)
 *
 * @param bridge Bridge instance
 * @param mirror Mirror neuron system
 * @return 0 on success, -1 on error
 */
int mirror_tom_connect_mirror(mirror_tom_bridge_t bridge, mirror_neurons_t mirror);

/**
 * @brief Connect to Theory of Mind system
 *
 * WHAT: Establish connection to ToM module
 * WHY:  Send inference requests, receive mental state updates
 * HOW:  Register with ToM observation and query interfaces
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must synchronize)
 *
 * @param bridge Bridge instance
 * @param tom Theory of Mind system
 * @return 0 on success, -1 on error
 */
int mirror_tom_connect_tom(mirror_tom_bridge_t bridge, theory_of_mind_t tom);

/* ============================================================================
 * Core API - Mirror → ToM Pathway
 * ============================================================================ */

/**
 * @brief Process mirror neuron observation for ToM
 *
 * WHAT: Extract mental state information from mirror neuron activation
 * WHY:  Embodied simulation grounds intention/emotion inference
 * HOW:  Analyze action features, resonance, prediction error
 *
 * This function performs SIMD-optimized similarity computations when
 * comparing observed actions against known intention patterns.
 *
 * COMPLEXITY: O(intention_dim) with SIMD acceleration
 * THREAD-SAFE: No (caller must synchronize)
 *
 * @param bridge Bridge instance
 * @param agent_id Agent being observed
 * @param observation Mirror neuron observation data
 * @return 0 on success, -1 on error
 */
int mirror_tom_process_observation(mirror_tom_bridge_t bridge,
                                    uint32_t agent_id,
                                    const mirror_tom_observation_t* observation);

/**
 * @brief Trigger intention inference from action observation
 *
 * WHAT: Infer agent's intention from observed action features
 * WHY:  Map motor simulation to goal/intention understanding
 * HOW:  SIMD cosine similarity of action vs. known intention patterns
 *
 * COMPLEXITY: O(num_patterns * intention_dim) with SIMD
 * THREAD-SAFE: No (caller must synchronize)
 *
 * @param bridge Bridge instance
 * @param agent_id Agent to infer intention for
 * @param action_features Action feature vector
 * @param action_dim Feature dimension
 * @param out_intention Output intention description (can be NULL)
 * @param out_confidence Output confidence (can be NULL)
 * @return 0 on success, -1 on error
 */
int mirror_tom_infer_intention(mirror_tom_bridge_t bridge,
                                uint32_t agent_id,
                                const float* action_features,
                                uint32_t action_dim,
                                char* out_intention,
                                float* out_confidence);

/**
 * @brief Trigger empathy from emotional resonance
 *
 * WHAT: Activate empathy pathway from mirror neuron emotional resonance
 * WHY:  Embodied emotion simulation enables empathic understanding
 * HOW:  Map resonance intensity/type to ToM emotion inference
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must synchronize)
 *
 * @param bridge Bridge instance
 * @param agent_id Agent resonating with
 * @param emotion Observed/resonated emotion
 * @param intensity Resonance intensity (0-1)
 * @return 0 on success, -1 on error
 */
int mirror_tom_trigger_empathy(mirror_tom_bridge_t bridge,
                                uint32_t agent_id,
                                mirror_tom_emotion_t emotion,
                                float intensity);

/**
 * @brief Signal false belief detection from prediction error
 *
 * WHAT: Detect when agent may hold false belief based on prediction error
 * WHY:  Unexpected behavior suggests incorrect beliefs about world
 * HOW:  High prediction error + context → false belief hypothesis
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must synchronize)
 *
 * @param bridge Bridge instance
 * @param agent_id Agent with unexpected behavior
 * @param prediction_error Magnitude of prediction error (0-1)
 * @param context_features Context that may explain error (can be NULL)
 * @param context_dim Context feature dimension
 * @return 0 on success, -1 on error
 */
int mirror_tom_signal_false_belief(mirror_tom_bridge_t bridge,
                                    uint32_t agent_id,
                                    float prediction_error,
                                    const float* context_features,
                                    uint32_t context_dim);

/* ============================================================================
 * Core API - ToM → Mirror Pathway
 * ============================================================================ */

/**
 * @brief Update mental state model for agent
 *
 * WHAT: Receive ToM mental state update to modulate mirroring
 * WHY:  Social evaluation affects resonance and imitation
 * HOW:  Store mental state, compute modulation factors
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must synchronize)
 *
 * @param bridge Bridge instance
 * @param mental_state Updated mental state from ToM
 * @return 0 on success, -1 on error
 */
int mirror_tom_update_mental_state(mirror_tom_bridge_t bridge,
                                    const mirror_tom_mental_state_t* mental_state);

/**
 * @brief Compute resonance modulation based on ToM evaluation
 *
 * WHAT: Calculate how much to modulate mirror neuron resonance
 * WHY:  Trust, alignment, social distance affect mirroring strength
 * HOW:  Weighted combination of social factors
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 *
 * @param bridge Bridge instance
 * @param agent_id Agent to compute modulation for
 * @return Resonance gain (modulation_min to modulation_max), or 1.0 on error
 */
float mirror_tom_compute_resonance_gain(mirror_tom_bridge_t bridge,
                                         uint32_t agent_id);

/**
 * @brief Check if imitation should be suppressed due to deception
 *
 * WHAT: Query whether ToM has flagged agent as potentially deceptive
 * WHY:  Prevent imitation of deceptive or harmful actions
 * HOW:  Check deception likelihood against threshold
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 *
 * @param bridge Bridge instance
 * @param agent_id Agent to check
 * @return true if imitation should be suppressed
 */
bool mirror_tom_should_suppress_imitation(mirror_tom_bridge_t bridge,
                                           uint32_t agent_id);

/**
 * @brief Get observation bias based on ToM prediction
 *
 * WHAT: Query expected action to bias observation processing
 * WHY:  Top-down prediction from ToM primes mirror observation
 * HOW:  Retrieve predicted intention, convert to observation bias
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 *
 * @param bridge Bridge instance
 * @param agent_id Agent to get bias for
 * @param out_bias Output bias vector (can be NULL)
 * @param out_confidence Output confidence (can be NULL)
 * @return 0 on success, -1 if no prediction available
 */
int mirror_tom_get_observation_bias(mirror_tom_bridge_t bridge,
                                     uint32_t agent_id,
                                     float* out_bias,
                                     float* out_confidence);

/* ============================================================================
 * Core API - SIMD-Optimized Computations
 * ============================================================================ */

/**
 * @brief Compute action-intention similarity using SIMD
 *
 * WHAT: Fast cosine similarity between action and intention vectors
 * WHY:  Core computation for intention inference
 * HOW:  SIMD dot product and magnitude computation
 *
 * Uses tensor_simd_dot_f32 and tensor_simd_sum_sq_f32 for acceleration.
 *
 * COMPLEXITY: O(dim) with SIMD parallelism
 * THREAD-SAFE: Yes (no shared state)
 *
 * @param action Action feature vector
 * @param intention Intention template vector
 * @param dim Vector dimension
 * @return Cosine similarity (-1 to 1), or 0 on error
 */
float mirror_tom_simd_similarity(const float* action,
                                  const float* intention,
                                  uint32_t dim);

/**
 * @brief Batch compare belief states using SIMD
 *
 * WHAT: Efficiently compare multiple agents' belief states
 * WHY:  Multi-agent mental state comparison for social reasoning
 * HOW:  Vectorized pairwise similarity computation
 *
 * @param belief_states Array of belief state vectors
 * @param num_agents Number of agents
 * @param belief_dim Dimension of belief vectors
 * @param out_similarity Output similarity matrix (num_agents x num_agents)
 * @return 0 on success, -1 on error
 */
int mirror_tom_batch_belief_similarity(const float** belief_states,
                                        uint32_t num_agents,
                                        uint32_t belief_dim,
                                        float* out_similarity);

/* ============================================================================
 * Core API - Query Functions
 * ============================================================================ */

/**
 * @brief Get current effects from mirror → ToM
 *
 * @param bridge Bridge instance
 * @param out_effects Output effects structure
 * @return 0 on success, -1 on error
 */
int mirror_tom_get_mirror_effects(mirror_tom_bridge_t bridge,
                                   mirror_to_tom_effects_t* out_effects);

/**
 * @brief Get current effects from ToM → mirror
 *
 * @param bridge Bridge instance
 * @param out_effects Output effects structure
 * @return 0 on success, -1 on error
 */
int mirror_tom_get_tom_effects(mirror_tom_bridge_t bridge,
                                tom_to_mirror_effects_t* out_effects);

/**
 * @brief Get agent tracking state
 *
 * @param bridge Bridge instance
 * @param agent_id Agent to query
 * @param out_state Output agent state
 * @return 0 on success, -1 if agent not tracked
 */
int mirror_tom_get_agent_state(mirror_tom_bridge_t bridge,
                                uint32_t agent_id,
                                mirror_tom_agent_state_t* out_state);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param out_stats Output statistics
 * @return 0 on success, -1 on error
 */
int mirror_tom_get_stats(mirror_tom_bridge_t bridge,
                          mirror_tom_stats_t* out_stats);

/* ============================================================================
 * Core API - Update and Maintenance
 * ============================================================================ */

/**
 * @brief Periodic update tick
 *
 * WHAT: Perform periodic maintenance and decay
 * WHY:  Age out unused beliefs, update running statistics
 * HOW:  Decay belief confidence, prune inactive agents
 *
 * COMPLEXITY: O(agents)
 * THREAD-SAFE: No (caller must synchronize)
 *
 * @param bridge Bridge instance
 * @param delta_time_us Microseconds since last update
 * @return 0 on success, -1 on error
 */
int mirror_tom_update(mirror_tom_bridge_t bridge, uint64_t delta_time_us);

/**
 * @brief Reset agent tracking state
 *
 * WHAT: Clear tracked state for specific agent
 * WHY:  Handle agent departure or identity confusion
 *
 * @param bridge Bridge instance
 * @param agent_id Agent to reset
 * @return 0 on success, -1 on error
 */
int mirror_tom_reset_agent(mirror_tom_bridge_t bridge, uint32_t agent_id);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get emotion name string
 */
const char* mirror_tom_emotion_name(mirror_tom_emotion_t emotion);

/**
 * @brief Print observation for debugging
 */
void mirror_tom_print_observation(const mirror_tom_observation_t* obs,
                                   const char* prefix);

/**
 * @brief Print mental state for debugging
 */
void mirror_tom_print_mental_state(const mirror_tom_mental_state_t* state,
                                    const char* prefix);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MIRROR_TOM_BRIDGE_H */
