/**
 * @file nimcp_mesh_pattern_routing.h
 * @brief Brain-Inspired Pattern-Based Transaction Routing
 *
 * WHAT: Routes transactions based on pattern similarity, not discrete types
 * WHY:  The brain doesn't have predefined "transaction types" - it routes
 *       based on pattern recognition, learned associations, and self-selection
 * HOW:  Each module has a "receptive field" (pattern it responds to).
 *       Transactions carry a pattern vector. Modules with high similarity
 *       to the pattern self-select as endorsers.
 *
 * BRAIN ANALOGY:
 * ```
 *   Visual Input                    Auditory Input
 *        │                               │
 *        ▼                               ▼
 *   ┌─────────┐                     ┌─────────┐
 *   │ V1, V2  │ ◄── High match      │   A1    │ ◄── High match
 *   │ V4, IT  │     to visual       │  Wernicke│     to auditory
 *   └─────────┘     patterns        └─────────┘     patterns
 *        │                               │
 *        └───────────┬───────────────────┘
 *                    ▼
 *              ┌───────────┐
 *              │  PFC      │ ◄── Responds to many patterns
 *              │           │     (high-level integration)
 *              └───────────┘
 *
 *   No central "type checker" - modules activate based on pattern match
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_PATTERN_ROUTING_H
#define NIMCP_MESH_PATTERN_ROUTING_H

#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_participant.h"
#include "utils/error/nimcp_error_codes.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Dimension of pattern vectors (like neural population codes) */
#define MESH_PATTERN_DIM            64

/** @brief Maximum receptive field patterns per module */
#define MESH_MAX_RECEPTIVE_PATTERNS 8

/** @brief Default activation threshold for self-selection */
#define MESH_DEFAULT_ACTIVATION_THRESHOLD 0.5f

/* ============================================================================
 * Pattern Types
 * ============================================================================ */

/**
 * @brief Pattern vector - distributed representation of transaction content
 *
 * BRAIN ANALOGY: Population code across neurons
 * Instead of "type = MOTOR_COMMAND", the pattern encodes:
 *   - What kind of action (movement, memory, perception)
 *   - Urgency/salience
 *   - Context features
 *   - Semantic content
 */
typedef struct mesh_pattern {
    float vector[MESH_PATTERN_DIM];     /**< Pattern vector */
    float magnitude;                     /**< Pattern strength/salience */
    uint32_t active_dims;               /**< Number of active dimensions */
} mesh_pattern_t;

/**
 * @brief Receptive field - what patterns a module responds to
 *
 * BRAIN ANALOGY: Tuning curves of neurons
 * Visual cortex responds to edges, motion, color
 * Auditory cortex responds to frequencies, temporal patterns
 * PFC responds to abstract goals, rules
 */
typedef struct mesh_receptive_field {
    mesh_pattern_t preferred[MESH_MAX_RECEPTIVE_PATTERNS]; /**< Preferred patterns */
    size_t pattern_count;                                   /**< Number of patterns */
    float threshold;                                        /**< Activation threshold */
    float sharpness;                                        /**< Tuning sharpness (selectivity) */

    /* Learned modulation */
    float learned_bias;                 /**< Bias from experience */
    float neuromod_gain;                /**< Current neuromodulator effect */
} mesh_receptive_field_t;

/**
 * @brief Module activation state
 *
 * BRAIN ANALOGY: Firing rate / activation level
 */
typedef struct mesh_activation {
    mesh_participant_id_t module_id;
    float activation_level;             /**< How strongly activated [0, 1] */
    float confidence;                   /**< Confidence in activation */
    float pattern_similarity;           /**< Similarity to input pattern */
    bool should_endorse;                /**< Self-selected as endorser */
} mesh_activation_t;

/* ============================================================================
 * Pattern-Based Transaction
 * ============================================================================ */

/**
 * @brief Transaction with pattern-based routing (replaces enum type)
 */
typedef struct mesh_pattern_transaction {
    mesh_tx_id_t id;                    /**< Transaction ID */
    mesh_participant_id_t proposer;     /**< Who proposed */
    mesh_channel_id_t channel;          /**< Target channel */

    /* Pattern-based type (replaces discrete enum) */
    mesh_pattern_t content_pattern;     /**< What this transaction IS */
    mesh_pattern_t context_pattern;     /**< Current context */
    mesh_pattern_t goal_pattern;        /**< Intended outcome */

    /* Urgency/salience (like norepinephrine) */
    float urgency;                      /**< How urgent [0, 1] */
    float novelty;                      /**< How novel [0, 1] */

    /* Payload */
    void* payload;
    size_t payload_size;

    uint64_t timestamp_ns;
} mesh_pattern_transaction_t;

/* ============================================================================
 * Pattern Router
 * ============================================================================ */

/**
 * @brief Pattern-based router (opaque)
 */
typedef struct mesh_pattern_router mesh_pattern_router_t;

/**
 * @brief Router configuration
 */
typedef struct mesh_pattern_router_config {
    float default_threshold;            /**< Default activation threshold */
    float competition_strength;         /**< Lateral inhibition strength */
    bool enable_learning;               /**< Learn from endorsement outcomes */
    float learning_rate;                /**< How fast to learn associations */
    size_t max_endorsers;               /**< Max modules that can self-select */
} mesh_pattern_router_config_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create pattern router
 */
mesh_pattern_router_t* mesh_pattern_router_create(
    const mesh_pattern_router_config_t* config
);

/**
 * @brief Destroy pattern router
 */
void mesh_pattern_router_destroy(mesh_pattern_router_t* router);

/* ============================================================================
 * Module Registration API
 * ============================================================================ */

/**
 * @brief Register module's receptive field
 *
 * WHAT: Tell the router what patterns this module responds to
 * WHY:  Modules self-select based on pattern similarity
 *
 * Example: Motor cortex registers patterns for movement intentions
 *          Hippocampus registers patterns for memory-related content
 *          Amygdala registers patterns for threat/salience
 */
nimcp_error_t mesh_pattern_router_register_receptive_field(
    mesh_pattern_router_t* router,
    mesh_participant_id_t module_id,
    const mesh_receptive_field_t* field
);

/**
 * @brief Update module's receptive field (learning)
 */
nimcp_error_t mesh_pattern_router_update_receptive_field(
    mesh_pattern_router_t* router,
    mesh_participant_id_t module_id,
    const mesh_pattern_t* new_preferred,
    float learning_rate
);

/* ============================================================================
 * Routing API
 * ============================================================================ */

/**
 * @brief Compute which modules should participate
 *
 * WHAT: Pattern matching + self-selection
 * WHY:  No predefined types - modules activate based on similarity
 *
 * Process:
 * 1. Compute similarity between transaction pattern and each module's field
 * 2. Apply activation threshold
 * 3. Apply lateral inhibition (competition)
 * 4. Return activated modules as potential endorsers
 *
 * @param router Pattern router
 * @param tx Pattern-based transaction
 * @param activations Output: activated modules
 * @param max_activations Max output size
 * @param count_out Output: number of activated modules
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_pattern_router_compute_activations(
    mesh_pattern_router_t* router,
    const mesh_pattern_transaction_t* tx,
    mesh_activation_t* activations,
    size_t max_activations,
    size_t* count_out
);

/**
 * @brief Get endorsers for transaction (self-selection)
 *
 * Returns modules that should endorse based on pattern match
 */
nimcp_error_t mesh_pattern_router_get_endorsers(
    mesh_pattern_router_t* router,
    const mesh_pattern_transaction_t* tx,
    mesh_participant_id_t* endorsers,
    size_t max_endorsers,
    size_t* count_out
);

/* ============================================================================
 * Neuromodulation API
 * ============================================================================ */

/**
 * @brief Apply neuromodulator effect
 *
 * BRAIN ANALOGY:
 *   Dopamine  → Increases salience of reward-related patterns
 *   Norepinephrine → Increases urgency, broadens receptive fields
 *   Acetylcholine → Sharpens attention, narrows focus
 *   Serotonin → Modulates emotional/social patterns
 */
typedef enum mesh_neuromodulator {
    MESH_NEUROMOD_DOPAMINE,             /**< Reward/motivation */
    MESH_NEUROMOD_NOREPINEPHRINE,       /**< Arousal/urgency */
    MESH_NEUROMOD_ACETYLCHOLINE,        /**< Attention/focus */
    MESH_NEUROMOD_SEROTONIN             /**< Mood/social */
} mesh_neuromodulator_t;

nimcp_error_t mesh_pattern_router_apply_neuromodulation(
    mesh_pattern_router_t* router,
    mesh_neuromodulator_t neuromod,
    float level
);

/* ============================================================================
 * Learning API
 * ============================================================================ */

/**
 * @brief Learn from endorsement outcome
 *
 * WHAT: Strengthen associations that led to good outcomes
 * WHY:  The brain learns which modules should participate together
 *
 * If transaction succeeded:
 *   - Strengthen pattern→endorser associations
 *   - Modules that endorsed become more likely to endorse similar patterns
 *
 * If transaction failed:
 *   - Weaken associations
 *   - System learns different modules should handle this pattern
 */
nimcp_error_t mesh_pattern_router_learn_outcome(
    mesh_pattern_router_t* router,
    const mesh_pattern_transaction_t* tx,
    const mesh_participant_id_t* endorsers,
    size_t endorser_count,
    bool success,
    float reward_signal
);

/* ============================================================================
 * Pattern Utilities
 * ============================================================================ */

/**
 * @brief Compute cosine similarity between patterns
 */
float mesh_pattern_similarity(
    const mesh_pattern_t* a,
    const mesh_pattern_t* b
);

/**
 * @brief Create pattern from semantic description
 *
 * Example: "motor intention arm reach" → pattern vector
 * Uses learned embeddings to convert concepts to patterns
 */
nimcp_error_t mesh_pattern_from_semantics(
    const char* description,
    mesh_pattern_t* pattern_out
);

/**
 * @brief Blend patterns (like attention-weighted combination)
 */
nimcp_error_t mesh_pattern_blend(
    const mesh_pattern_t* patterns,
    const float* weights,
    size_t count,
    mesh_pattern_t* result
);

/**
 * @brief Initialize pattern with default values
 */
void mesh_pattern_init(mesh_pattern_t* pattern);

/**
 * @brief Initialize receptive field
 */
void mesh_receptive_field_init(mesh_receptive_field_t* field);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_PATTERN_ROUTING_H */
