//=============================================================================
// nimcp_language_training_bridge.h - Language-Training Bridge Integration
//=============================================================================
/**
 * @file nimcp_language_training_bridge.h
 * @brief Bidirectional bridge integrating Language Layer with Training Layer
 *
 * WHAT: Bridge connecting language processing with training/learning systems
 * WHY:  Enable language-specific learning (vocabulary, grammar, phonemes)
 *       modulated by cognitive state and training parameters
 * HOW:  Vocabulary expansion, grammar induction, STDP for phoneme learning
 *
 * BIOLOGICAL BASIS:
 * - Vocabulary Learning: New word-concept associations (hippocampus → cortex)
 * - Grammar Learning: Statistical learning of grammatical patterns
 * - Phoneme Learning: Perceptual learning via STDP in auditory cortex
 * - Semantic Binding: Strengthening word-concept associations
 * - Error-Driven Learning: N400-like prediction errors drive learning
 *
 * KEY CONNECTIONS:
 * - Training Context: Global learning parameters
 * - Cognitive Training Bridge: Cognitive modulation of language learning
 * - Perception Training Bridge: Perceptual learning for phonemes
 * - Plasticity Bridge: STDP, BCM for synaptic updates
 *
 * DATA FLOW:
 * - Training → Language: Learning rates, plasticity parameters
 * - Language → Training: Error signals, vocabulary updates
 *
 * @version 1.0.0 - Phase L2: Language Layer Bridges
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#ifndef NIMCP_LANGUAGE_TRAINING_BRIDGE_H
#define NIMCP_LANGUAGE_TRAINING_BRIDGE_H

#include "language/nimcp_language_types.h"
#include "language/nimcp_language_config.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct language_training_bridge language_training_bridge_t;
typedef struct language_orchestrator language_orchestrator_t;
typedef struct nimcp_brain_training_ctx nimcp_brain_training_ctx_t;
typedef struct cognitive_training_bridge cognitive_training_bridge_t;
typedef struct perception_training_bridge perception_training_bridge_t;
typedef struct training_plasticity_bridge training_plasticity_bridge_t;

/* bio_router_t is a pointer type defined in bio_router.h */
#ifndef NIMCP_BIO_ROUTER_H
typedef void* bio_router_t;
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module identification */
#define LANGUAGE_TRAINING_MODULE_NAME      "language_training_bridge"
#define LANGUAGE_TRAINING_MODULE_VERSION   "1.0.0"

/** Default configuration values */
#define LANGUAGE_TRAINING_DEFAULT_UPDATE_INTERVAL_MS    50
#define LANGUAGE_TRAINING_DEFAULT_VOCAB_LR              0.01f
#define LANGUAGE_TRAINING_DEFAULT_GRAMMAR_LR            0.005f
#define LANGUAGE_TRAINING_DEFAULT_PHONEME_LR            0.001f
#define LANGUAGE_TRAINING_DEFAULT_SEMANTIC_LR           0.01f

/** STDP parameters */
#define LANGUAGE_TRAINING_STDP_TAU_PLUS                 20.0f   /* ms */
#define LANGUAGE_TRAINING_STDP_TAU_MINUS                20.0f   /* ms */
#define LANGUAGE_TRAINING_STDP_A_PLUS                   0.01f
#define LANGUAGE_TRAINING_STDP_A_MINUS                  0.012f

/** Error signal parameters */
#define LANGUAGE_TRAINING_N400_THRESHOLD                0.3f
#define LANGUAGE_TRAINING_P600_THRESHOLD                0.4f
#define LANGUAGE_TRAINING_ERROR_SCALE_DEFAULT           1.0f

/** Vocabulary limits */
#define LANGUAGE_TRAINING_MAX_NEW_WORDS_PER_BATCH       16
#define LANGUAGE_TRAINING_MAX_GRAMMAR_RULES             256

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Learning type categories
 */
typedef enum {
    LEARNING_TYPE_VOCABULARY = 0,     /**< New word learning */
    LEARNING_TYPE_GRAMMAR,            /**< Grammar pattern learning */
    LEARNING_TYPE_PHONEME,            /**< Phoneme perception learning */
    LEARNING_TYPE_SEMANTIC,           /**< Semantic association learning */
    LEARNING_TYPE_PROSODY,            /**< Prosodic pattern learning */
    LEARNING_TYPE_COUNT
} language_learning_type_t;

/**
 * @brief Error signal types (ERP-like)
 */
typedef enum {
    ERROR_SIGNAL_N400 = 0,            /**< Semantic anomaly (N400-like) */
    ERROR_SIGNAL_P600,                /**< Syntactic anomaly (P600-like) */
    ERROR_SIGNAL_ELAN,                /**< Early syntactic (ELAN-like) */
    ERROR_SIGNAL_LAN,                 /**< Left-anterior negativity */
    ERROR_SIGNAL_COUNT
} language_error_signal_t;

/**
 * @brief Learning event types
 */
typedef enum {
    LEARNING_EVENT_WORD_ACQUIRED = 0, /**< New word learned */
    LEARNING_EVENT_WORD_REINFORCED,   /**< Existing word strengthened */
    LEARNING_EVENT_WORD_DECAYED,      /**< Word weakened (disuse) */
    LEARNING_EVENT_GRAMMAR_INDUCED,   /**< Grammar rule induced */
    LEARNING_EVENT_PHONEME_TUNED,     /**< Phoneme representation tuned */
    LEARNING_EVENT_SEMANTIC_BOUND,    /**< Word-concept binding strengthened */
    LEARNING_EVENT_COUNT
} language_learning_event_t;

/**
 * @brief Plasticity mechanism types
 */
typedef enum {
    PLASTICITY_STDP = 0,              /**< Spike-timing dependent */
    PLASTICITY_BCM,                   /**< Bienenstock-Cooper-Munro */
    PLASTICITY_HEBBIAN,               /**< Simple Hebbian */
    PLASTICITY_ANTI_HEBBIAN,          /**< Anti-Hebbian */
    PLASTICITY_COUNT
} language_plasticity_type_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Vocabulary learning state
 */
typedef struct {
    /* Learning rates */
    float vocabulary_lr;              /**< Word acquisition LR */
    float vocabulary_decay_rate;      /**< Forgetting rate */

    /* New word queue */
    uint32_t* new_word_ids;           /**< Pending new words */
    float* new_word_novelty;          /**< Novelty scores */
    uint32_t new_word_count;          /**< Number of new words */
    uint32_t new_word_capacity;       /**< Queue capacity */

    /* Statistics */
    uint64_t words_acquired;          /**< Total words learned */
    uint64_t words_reinforced;        /**< Words strengthened */
    uint64_t words_forgotten;         /**< Words lost */
    float avg_word_strength;          /**< Average word strength */

    /* Control */
    bool expansion_enabled;           /**< Allow new word learning */
    uint32_t vocab_size;              /**< Current vocabulary size */
    uint32_t max_vocab_size;          /**< Maximum vocabulary */
} vocabulary_learning_state_t;

/**
 * @brief Grammar learning state
 */
typedef struct {
    /* Learning rates */
    float grammar_lr;                 /**< Grammar pattern LR */

    /* Pattern tracking */
    uint32_t num_rules;               /**< Number of induced rules */
    float* rule_strengths;            /**< Rule confidence levels */
    uint32_t rule_capacity;           /**< Maximum rules */

    /* Statistical learning */
    float* transition_probabilities;  /**< Bigram/trigram stats */
    uint32_t transition_dim;          /**< Probability matrix dim */
    uint64_t exposure_count;          /**< Training examples seen */

    /* Control */
    bool induction_enabled;           /**< Allow grammar induction */
} grammar_learning_state_t;

/**
 * @brief Phoneme learning state (STDP-based)
 */
typedef struct {
    /* STDP parameters */
    float tau_plus;                   /**< LTP time constant */
    float tau_minus;                  /**< LTD time constant */
    float a_plus;                     /**< LTP amplitude */
    float a_minus;                    /**< LTD amplitude */
    float time_window_ms;             /**< STDP time window */

    /* Learning rates */
    float phoneme_lr;                 /**< Base learning rate */
    float plasticity_scale;           /**< Current plasticity level */

    /* Trace */
    float* eligibility_trace;         /**< Eligibility traces */
    uint32_t trace_dim;               /**< Trace dimension */

    /* Statistics */
    uint64_t potentiation_events;     /**< LTP events */
    uint64_t depression_events;       /**< LTD events */

    /* Control */
    bool stdp_enabled;                /**< STDP active */
    language_plasticity_type_t type;  /**< Plasticity mechanism */
} phoneme_learning_state_t;

/**
 * @brief Semantic binding state
 */
typedef struct {
    /* Learning rates */
    float binding_lr;                 /**< Binding strength LR */

    /* Binding updates */
    uint32_t* word_ids;               /**< Words with updated bindings */
    uint32_t* concept_ids;            /**< Associated concepts */
    float* binding_deltas;            /**< Binding strength changes */
    uint32_t num_updates;             /**< Number of updates */
    uint32_t update_capacity;         /**< Maximum updates */

    /* Statistics */
    uint64_t bindings_strengthened;   /**< Bindings reinforced */
    uint64_t bindings_weakened;       /**< Bindings reduced */

    /* Control */
    bool semantic_learning_enabled;   /**< Learning active */
} semantic_learning_state_t;

/**
 * @brief Error signal for learning
 */
typedef struct {
    language_error_signal_t type;     /**< Error type */
    float magnitude;                  /**< Error magnitude [0-1] */
    uint32_t word_position;           /**< Position in utterance */
    uint32_t word_id;                 /**< Word causing error */
    uint32_t expected_id;             /**< Expected word/concept */
    uint64_t timestamp_ms;            /**< Error timestamp */
} language_error_t;

/**
 * @brief Error feedback state
 */
typedef struct {
    /* Error queue */
    language_error_t* error_queue;    /**< Pending errors */
    uint32_t error_count;             /**< Number of errors */
    uint32_t error_capacity;          /**< Queue capacity */

    /* Thresholds */
    float n400_threshold;             /**< N400 detection threshold */
    float p600_threshold;             /**< P600 detection threshold */

    /* Scaling */
    float error_scale;                /**< Error signal scaling */

    /* Statistics */
    uint64_t n400_events;             /**< Total N400 errors */
    uint64_t p600_events;             /**< Total P600 errors */
    float avg_error_magnitude;        /**< Average error */

    /* Control */
    bool comprehension_feedback_enabled;  /**< Report comprehension errors */
    bool production_feedback_enabled;     /**< Report production errors */
} error_feedback_state_t;

/**
 * @brief Learning event record
 */
typedef struct {
    language_learning_event_t event;  /**< Event type */
    language_learning_type_t learning_type;  /**< Learning category */
    uint32_t target_id;               /**< Word/rule/phoneme ID */
    float delta;                      /**< Change magnitude */
    uint64_t timestamp_ms;            /**< Event timestamp */
} learning_event_record_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Learning counts */
    uint64_t vocabulary_updates;      /**< Vocabulary changes */
    uint64_t grammar_updates;         /**< Grammar changes */
    uint64_t phoneme_updates;         /**< Phoneme tuning events */
    uint64_t semantic_updates;        /**< Semantic binding updates */

    /* Error statistics */
    uint64_t total_errors;            /**< Total error signals */
    float error_rate;                 /**< Errors per utterance */

    /* Learning rates (effective) */
    float effective_vocab_lr;         /**< Modulated vocabulary LR */
    float effective_grammar_lr;       /**< Modulated grammar LR */
    float effective_phoneme_lr;       /**< Modulated phoneme LR */

    /* Performance */
    float avg_processing_time_ms;     /**< Average processing time */
    uint64_t last_update_time_ms;     /**< Last update timestamp */
} language_training_stats_t;

//=============================================================================
// Bridge State Structure
//=============================================================================

/**
 * @brief Language-training bridge state
 */
struct language_training_bridge {
    /* Configuration */
    language_training_config_t config;        /**< Bridge configuration */
    bool initialized;                         /**< Initialization state */
    bool active;                              /**< Active processing */

    /* Connected components */
    language_orchestrator_t* orchestrator;    /**< Parent orchestrator */
    nimcp_brain_training_ctx_t* training_ctx; /**< Training context */
    cognitive_training_bridge_t* cognitive_bridge;    /**< Cognitive training */
    perception_training_bridge_t* perception_bridge;  /**< Perception training */
    training_plasticity_bridge_t* plasticity_bridge;  /**< Plasticity bridge */

    /* Learning states */
    vocabulary_learning_state_t vocab_state;  /**< Vocabulary learning */
    grammar_learning_state_t grammar_state;   /**< Grammar learning */
    phoneme_learning_state_t phoneme_state;   /**< Phoneme learning */
    semantic_learning_state_t semantic_state; /**< Semantic learning */

    /* Error feedback */
    error_feedback_state_t error_state;       /**< Error feedback */

    /* Event log */
    learning_event_record_t* event_log;       /**< Recent learning events */
    uint32_t event_log_size;                  /**< Log size */
    uint32_t event_log_idx;                   /**< Current log position */

    /* Statistics */
    language_training_stats_t stats;          /**< Bridge statistics */

    /* Bio-async */
    bio_router_t* bio_router;                 /**< Bio-async router */
    bool bio_async_registered;                /**< Registration status */
};

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create language-training bridge
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return New bridge instance or NULL on error
 */
language_training_bridge_t* language_training_bridge_create(
    const language_training_config_t* config
);

/**
 * @brief Destroy language-training bridge
 *
 * @param bridge Bridge instance
 */
void language_training_bridge_destroy(language_training_bridge_t* bridge);

/**
 * @brief Initialize bridge with default configuration
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_training_bridge_init(language_training_bridge_t* bridge);

/**
 * @brief Start bridge processing
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_training_bridge_start(language_training_bridge_t* bridge);

/**
 * @brief Stop bridge processing
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_training_bridge_stop(language_training_bridge_t* bridge);

//=============================================================================
// Connection API
//=============================================================================

/**
 * @brief Connect to language orchestrator
 *
 * @param bridge Bridge instance
 * @param orchestrator Language orchestrator
 * @return 0 on success, -1 on error
 */
int language_training_bridge_connect_orchestrator(
    language_training_bridge_t* bridge,
    language_orchestrator_t* orchestrator
);

/**
 * @brief Connect to training context
 *
 * @param bridge Bridge instance
 * @param training_ctx Training context
 * @return 0 on success, -1 on error
 */
int language_training_bridge_connect_training_context(
    language_training_bridge_t* bridge,
    nimcp_brain_training_ctx_t* training_ctx
);

/**
 * @brief Connect to cognitive training bridge
 *
 * @param bridge Bridge instance
 * @param cognitive_bridge Cognitive training bridge
 * @return 0 on success, -1 on error
 */
int language_training_bridge_connect_cognitive_training(
    language_training_bridge_t* bridge,
    cognitive_training_bridge_t* cognitive_bridge
);

/**
 * @brief Connect to perception training bridge
 *
 * @param bridge Bridge instance
 * @param perception_bridge Perception training bridge
 * @return 0 on success, -1 on error
 */
int language_training_bridge_connect_perception_training(
    language_training_bridge_t* bridge,
    perception_training_bridge_t* perception_bridge
);

/**
 * @brief Connect to plasticity bridge
 *
 * @param bridge Bridge instance
 * @param plasticity_bridge Plasticity bridge
 * @return 0 on success, -1 on error
 */
int language_training_bridge_connect_plasticity(
    language_training_bridge_t* bridge,
    training_plasticity_bridge_t* plasticity_bridge
);

//=============================================================================
// Vocabulary Learning API
//=============================================================================

/**
 * @brief Learn new word
 *
 * @param bridge Bridge instance
 * @param word Word to learn
 * @param novelty Novelty score [0-1]
 * @return 0 on success, -1 on error
 */
int language_training_bridge_learn_word(
    language_training_bridge_t* bridge,
    const language_word_t* word,
    float novelty
);

/**
 * @brief Reinforce existing word
 *
 * @param bridge Bridge instance
 * @param word_id Word identifier
 * @param reinforcement Reinforcement strength [0-1]
 * @return 0 on success, -1 on error
 */
int language_training_bridge_reinforce_word(
    language_training_bridge_t* bridge,
    uint32_t word_id,
    float reinforcement
);

/**
 * @brief Set vocabulary learning rate
 *
 * @param bridge Bridge instance
 * @param lr Learning rate
 * @return 0 on success, -1 on error
 */
int language_training_bridge_set_vocab_lr(
    language_training_bridge_t* bridge,
    float lr
);

//=============================================================================
// Grammar Learning API
//=============================================================================

/**
 * @brief Submit utterance for grammar learning
 *
 * @param bridge Bridge instance
 * @param words Word sequence
 * @param count Number of words
 * @return 0 on success, -1 on error
 */
int language_training_bridge_learn_grammar(
    language_training_bridge_t* bridge,
    const language_word_t* words,
    uint32_t count
);

/**
 * @brief Get induced grammar rules
 *
 * @param bridge Bridge instance
 * @param rule_strengths Output rule strengths
 * @param max_rules Maximum rules to retrieve
 * @return Number of rules, or -1 on error
 */
int language_training_bridge_get_grammar_rules(
    const language_training_bridge_t* bridge,
    float* rule_strengths,
    uint32_t max_rules
);

//=============================================================================
// Phoneme Learning API (STDP)
//=============================================================================

/**
 * @brief Process phoneme pair for STDP
 *
 * @param bridge Bridge instance
 * @param pre_phoneme Pre-synaptic phoneme
 * @param post_phoneme Post-synaptic phoneme
 * @param dt Time difference (post - pre) in ms
 * @return Weight change, or 0 on error
 */
float language_training_bridge_stdp_update(
    language_training_bridge_t* bridge,
    uint32_t pre_phoneme,
    uint32_t post_phoneme,
    float dt
);

/**
 * @brief Set STDP parameters
 *
 * @param bridge Bridge instance
 * @param tau_plus LTP time constant
 * @param tau_minus LTD time constant
 * @param a_plus LTP amplitude
 * @param a_minus LTD amplitude
 * @return 0 on success, -1 on error
 */
int language_training_bridge_set_stdp_params(
    language_training_bridge_t* bridge,
    float tau_plus,
    float tau_minus,
    float a_plus,
    float a_minus
);

//=============================================================================
// Semantic Binding API
//=============================================================================

/**
 * @brief Strengthen word-concept binding
 *
 * @param bridge Bridge instance
 * @param word_id Word identifier
 * @param concept_id Concept identifier
 * @param strength Binding strength update
 * @return 0 on success, -1 on error
 */
int language_training_bridge_bind_word_concept(
    language_training_bridge_t* bridge,
    uint32_t word_id,
    uint32_t concept_id,
    float strength
);

//=============================================================================
// Error Feedback API
//=============================================================================

/**
 * @brief Report comprehension error
 *
 * @param bridge Bridge instance
 * @param error Error information
 * @return 0 on success, -1 on error
 */
int language_training_bridge_report_error(
    language_training_bridge_t* bridge,
    const language_error_t* error
);

/**
 * @brief Get pending error signals
 *
 * @param bridge Bridge instance
 * @param errors Output error array
 * @param max_errors Maximum errors to retrieve
 * @return Number of errors, or -1 on error
 */
int language_training_bridge_get_errors(
    language_training_bridge_t* bridge,
    language_error_t* errors,
    uint32_t max_errors
);

/**
 * @brief Set error signal scaling
 *
 * @param bridge Bridge instance
 * @param scale Error scale factor
 * @return 0 on success, -1 on error
 */
int language_training_bridge_set_error_scale(
    language_training_bridge_t* bridge,
    float scale
);

//=============================================================================
// Training Parameter API
//=============================================================================

/**
 * @brief Get current effective learning rates
 *
 * @param bridge Bridge instance
 * @param vocab_lr Output vocabulary LR
 * @param grammar_lr Output grammar LR
 * @param phoneme_lr Output phoneme LR
 * @param semantic_lr Output semantic LR
 * @return 0 on success, -1 on error
 */
int language_training_bridge_get_learning_rates(
    const language_training_bridge_t* bridge,
    float* vocab_lr,
    float* grammar_lr,
    float* phoneme_lr,
    float* semantic_lr
);

/**
 * @brief Apply training update from context
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_training_bridge_apply_training_update(
    language_training_bridge_t* bridge
);

//=============================================================================
// Update and Query API
//=============================================================================

/**
 * @brief Update bridge (call each frame/cycle)
 *
 * @param bridge Bridge instance
 * @param current_time_ms Current time in milliseconds
 * @return 0 on success, -1 on error
 */
int language_training_bridge_update(
    language_training_bridge_t* bridge,
    uint64_t current_time_ms
);

/**
 * @brief Get recent learning events
 *
 * @param bridge Bridge instance
 * @param events Output event array
 * @param max_events Maximum events to retrieve
 * @return Number of events, or -1 on error
 */
int language_training_bridge_get_events(
    const language_training_bridge_t* bridge,
    learning_event_record_t* events,
    uint32_t max_events
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int language_training_bridge_get_stats(
    const language_training_bridge_t* bridge,
    language_training_stats_t* stats
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Register with bio-async router
 *
 * @param bridge Bridge instance
 * @param router Bio-async router
 * @return 0 on success, -1 on error
 */
int language_training_bridge_bio_async_register(
    language_training_bridge_t* bridge,
    bio_router_t* router
);

/**
 * @brief Unregister from bio-async router
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_training_bridge_bio_async_unregister(
    language_training_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LANGUAGE_TRAINING_BRIDGE_H */
