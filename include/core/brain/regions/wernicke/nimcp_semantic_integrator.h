/**
 * @file nimcp_semantic_integrator.h
 * @brief Semantic integration layer for Wernicke's area - Meaning extraction and context integration
 *
 * WHAT: Word-to-concept mapping, disambiguation, context accumulation, and semantic priming
 * WHY:  Transform word forms into conceptual representations for language comprehension
 * HOW:  Spreading activation + constraint satisfaction + contextual weighting
 *
 * BIOLOGICAL BASIS:
 * - Posterior middle temporal gyrus (pMTG) for concept retrieval
 * - Angular gyrus for semantic integration across modalities
 * - Anterior temporal lobe (ATL) for conceptual combination
 * - Semantic control via left prefrontal cortex (Jefferies & Lambon Ralph, 2006)
 *
 * PROCESSING MODEL:
 * 1. Lexical-Semantic Interface: Word form → Concept activation
 * 2. Sense Selection: Disambiguation via context (Swinney 1979 - initial access all senses)
 * 3. Context Integration: Accumulate meaning across words
 * 4. Thematic Binding: Assign semantic roles (agent, patient, etc.)
 * 5. Inference: Generate implied meanings from combinations
 *
 * KEY PHENOMENA MODELED:
 * - Lexical ambiguity resolution (Seidenberg et al. 1982)
 * - Semantic priming (Meyer & Schvaneveldt 1971)
 * - N400 semantic anomaly detection (Kutas & Hillyard 1980)
 * - Semantic similarity effects (Plaut & Shallice 1993)
 * - Context-dependent meaning construction (Barsalou 1999)
 *
 * @version Phase W2: Wernicke's Area Semantic Integration
 * @date 2026-01-04
 */

#ifndef NIMCP_SEMANTIC_INTEGRATOR_H
#define NIMCP_SEMANTIC_INTEGRATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

/* External types - use void* to avoid header dependencies */
/* semantic_memory_system_t from cognitive/memory/nimcp_semantic_memory.h */
/* lexical_entry_t from core/brain/regions/wernicke/nimcp_lexical_access.h */

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Default configuration values
 */
#define SEM_DEFAULT_MAX_SENSES           8
#define SEM_DEFAULT_MAX_CONTEXT_WORDS    32
#define SEM_DEFAULT_MAX_ACTIVE_CONCEPTS  128
#define SEM_DEFAULT_CONTEXT_WINDOW       7
#define SEM_DEFAULT_DISAMBIGUATION_THRESHOLD 0.6f
#define SEM_DEFAULT_CONCEPT_FEATURE_DIM  32
#define SEM_DEFAULT_MAX_THEMATIC_ROLES   6

/**
 * @brief Thematic role types (case grammar)
 */
typedef enum {
    ROLE_NONE = 0,
    ROLE_AGENT,          /**< Doer of action (The DOG bit the man) */
    ROLE_PATIENT,        /**< Affected by action (The dog bit the MAN) */
    ROLE_THEME,          /**< Thing moved/affected (He gave her a BOOK) */
    ROLE_EXPERIENCER,    /**< Perceiver/feeler (JOHN likes pizza) */
    ROLE_BENEFICIARY,    /**< Benefiting entity (He baked HER a cake) */
    ROLE_INSTRUMENT,     /**< Tool/means (Cut with a KNIFE) */
    ROLE_LOCATION,       /**< Place (She lives in PARIS) */
    ROLE_SOURCE,         /**< Origin (Came from LONDON) */
    ROLE_GOAL,           /**< Destination (Went to PARIS) */
    ROLE_TIME,           /**< Temporal reference (On TUESDAY) */
    ROLE_COUNT
} thematic_role_t;

/**
 * @brief Disambiguation strategy
 */
typedef enum {
    DISAMBIG_FREQUENCY,      /**< Most frequent sense wins */
    DISAMBIG_CONTEXT,        /**< Best contextual fit wins */
    DISAMBIG_COMBINED,       /**< Weighted frequency + context */
    DISAMBIG_ALL_ACTIVE      /**< Keep all senses active (Swinney model) */
} disambiguation_strategy_t;

/**
 * @brief Semantic integrator configuration
 */
typedef struct {
    /* Capacity */
    uint32_t max_senses;               /**< Max senses per word */
    uint32_t max_context_words;        /**< Max words in context buffer */
    uint32_t max_active_concepts;      /**< Max simultaneously active concepts */
    uint32_t concept_feature_dim;      /**< Concept feature vector dimension */

    /* Context window */
    uint32_t context_window;           /**< Words to consider for disambiguation */
    float context_decay;               /**< Older context weight decay (0-1) */

    /* Disambiguation */
    disambiguation_strategy_t strategy; /**< Sense selection strategy */
    float disambiguation_threshold;    /**< Confidence for sense selection */
    float frequency_weight;            /**< Weight for sense frequency */
    float context_weight;              /**< Weight for contextual fit */

    /* Activation dynamics */
    float activation_threshold;        /**< Minimum for concept to stay active */
    float activation_decay;            /**< Decay rate per timestep */
    float spreading_rate;              /**< Activation spread factor */

    /* Integration */
    bool enable_spreading_activation;  /**< Enable spreading in semantic network */
    bool enable_thematic_roles;        /**< Enable role assignment */
    bool enable_inference;             /**< Enable implicit meaning inference */
    bool enable_anomaly_detection;     /**< Enable N400-like anomaly detection */
} semantic_config_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Word sense (one meaning of a word)
 */
typedef struct {
    uint32_t sense_id;                 /**< Unique sense ID */
    uint32_t concept_id;               /**< Associated concept in semantic memory */
    char gloss[64];                    /**< Brief definition */

    float frequency;                   /**< Relative sense frequency (0-1) */
    float activation;                  /**< Current activation level (0-1) */
    float context_fit;                 /**< Fit with current context (0-1) */

    uint32_t* related_concepts;        /**< Related concept IDs */
    uint32_t num_related;              /**< Number of related concepts */
} word_sense_t;

/**
 * @brief Activated concept with context
 */
typedef struct {
    uint32_t concept_id;               /**< Concept ID */
    float activation;                  /**< Current activation (0-1) */
    uint32_t source_word_id;           /**< Word that activated this concept */
    uint32_t sense_id;                 /**< Sense that activated this concept */
    uint64_t activation_time_ms;       /**< When concept was activated */
    thematic_role_t role;              /**< Assigned thematic role */
} active_concept_t;

/**
 * @brief Context word entry
 */
typedef struct {
    uint32_t word_id;                  /**< Word ID from lexicon */
    uint32_t sense_id;                 /**< Selected sense ID */
    uint32_t concept_id;               /**< Primary concept ID */
    float* concept_features;           /**< Concept feature vector */
    uint32_t position;                 /**< Position in utterance */
    thematic_role_t role;              /**< Thematic role */
} context_word_t;

/**
 * @brief Sentence context state
 */
typedef struct {
    context_word_t* words;             /**< Context word buffer */
    uint32_t num_words;                /**< Number of words in context */
    uint32_t max_words;                /**< Maximum buffer size */

    float* context_vector;             /**< Aggregated context representation */
    uint32_t context_dim;              /**< Context vector dimension */

    /* Thematic frame */
    uint32_t agent_concept;            /**< Current agent concept */
    uint32_t patient_concept;          /**< Current patient concept */
    uint32_t action_concept;           /**< Current action/verb concept */

    /* Anomaly state */
    float coherence_score;             /**< Overall semantic coherence (0-1) */
    float anomaly_level;               /**< N400-like anomaly detection (0-1) */
} sentence_context_t;

/**
 * @brief Semantic integration result
 */
typedef struct {
    /* Recognition status */
    bool word_recognized;              /**< Word found in sense table */

    /* Primary interpretation */
    uint32_t concept_id;               /**< Selected concept ID */
    uint32_t sense_id;                 /**< Selected sense ID */
    float confidence;                  /**< Selection confidence (0-1) */

    /* Disambiguation */
    bool was_ambiguous;                /**< Word had multiple senses */
    uint32_t num_senses_active;        /**< Senses still active */
    word_sense_t* active_senses;       /**< Active sense details */

    /* Context integration */
    float context_fit;                 /**< Fit with context (0-1) */
    thematic_role_t assigned_role;     /**< Thematic role assigned */

    /* Anomaly detection */
    float anomaly_score;               /**< Semantic anomaly (N400) (0-1) */
    bool is_anomalous;                 /**< Above anomaly threshold */

    /* Timing */
    uint64_t processing_time_us;       /**< Processing time */
} semantic_result_t;

/**
 * @brief Semantic priming state
 */
typedef struct {
    uint32_t* primed_concepts;         /**< Pre-activated concept IDs */
    float* priming_levels;             /**< Priming activation levels */
    uint32_t num_primed;               /**< Number of primed concepts */
    uint32_t max_primed;               /**< Maximum primed concepts */
} semantic_priming_t;

/**
 * @brief Semantic integration statistics
 */
typedef struct {
    uint64_t words_processed;          /**< Total words processed */
    uint64_t disambiguations;          /**< Disambiguation attempts */
    uint64_t successful_disambig;      /**< Successful disambiguations */
    uint64_t anomalies_detected;       /**< Semantic anomalies detected */

    float avg_sense_count;             /**< Average senses per word */
    float avg_disambiguation_time_us;  /**< Average disambiguation time */
    float avg_context_fit;             /**< Average context fit score */
    float avg_anomaly_score;           /**< Average anomaly score */

    uint64_t spreading_activations;    /**< Spreading activation events */
    uint64_t thematic_assignments;     /**< Thematic role assignments */
} semantic_stats_t;

/*=============================================================================
 * OPAQUE TYPE
 *===========================================================================*/

typedef struct semantic_integrator semantic_integrator_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * @return Default semantic integrator configuration
 */
semantic_config_t semantic_default_config(void);

/**
 * @brief Create semantic integrator
 *
 * @param config Configuration (NULL for defaults)
 * @return New integrator or NULL on failure
 */
semantic_integrator_t* semantic_create(const semantic_config_t* config);

/**
 * @brief Destroy semantic integrator
 *
 * @param sem Integrator to destroy
 */
void semantic_destroy(semantic_integrator_t* sem);

/**
 * @brief Reset integrator state (clear context)
 *
 * @param sem Integrator instance
 * @return true on success
 */
bool semantic_reset(semantic_integrator_t* sem);

/**
 * @brief Connect to semantic memory system
 *
 * WHAT: Link to semantic memory for concept retrieval
 * WHY:  Access concept network for meaning integration
 * HOW:  Store pointer to semantic memory system
 *
 * @param sem Integrator instance
 * @param memory Semantic memory system (not owned)
 * @return true on success
 */
bool semantic_connect_memory(
    semantic_integrator_t* sem,
    void* memory  /* semantic_memory_system_t* */
);

/*=============================================================================
 * WORD SENSE MANAGEMENT
 *===========================================================================*/

/**
 * @brief Register word senses
 *
 * WHAT: Associate senses with a word
 * WHY:  Enable disambiguation of polysemous words
 * HOW:  Store sense-concept mappings for word
 *
 * @param sem Integrator instance
 * @param word_id Word ID from lexicon
 * @param senses Array of word senses
 * @param num_senses Number of senses
 * @return true on success
 */
bool semantic_register_senses(
    semantic_integrator_t* sem,
    uint32_t word_id,
    const word_sense_t* senses,
    uint32_t num_senses
);

/**
 * @brief Get senses for word
 *
 * @param sem Integrator instance
 * @param word_id Word ID
 * @param senses Output sense array
 * @param max_senses Maximum senses to return
 * @param num_senses Output: actual count
 * @return true on success
 */
bool semantic_get_senses(
    const semantic_integrator_t* sem,
    uint32_t word_id,
    word_sense_t* senses,
    uint32_t max_senses,
    uint32_t* num_senses
);

/**
 * @brief Add single sense to word
 *
 * @param sem Integrator instance
 * @param word_id Word ID
 * @param concept_id Associated concept
 * @param gloss Brief definition (can be NULL)
 * @param frequency Relative frequency (0-1)
 * @return Sense ID or 0 on failure
 */
uint32_t semantic_add_sense(
    semantic_integrator_t* sem,
    uint32_t word_id,
    uint32_t concept_id,
    const char* gloss,
    float frequency
);

/*=============================================================================
 * SEMANTIC INTEGRATION (Core Processing)
 *===========================================================================*/

/**
 * @brief Integrate word into context
 *
 * WHAT: Full semantic processing of word in context
 * WHY:  Extract meaning considering prior context
 * HOW:  Activate senses → Disambiguate → Update context → Assign role
 *
 * @param sem Integrator instance
 * @param word_id Word ID from lexical access
 * @param word_features Word features from lexical entry
 * @param feature_dim Feature dimension
 * @param result Output semantic result
 * @return true on success
 */
bool semantic_integrate_word(
    semantic_integrator_t* sem,
    uint32_t word_id,
    const float* word_features,
    uint32_t feature_dim,
    semantic_result_t* result
);

/**
 * @brief Integrate lexical entry directly
 *
 * WHAT: Convenience wrapper for lexical entry integration
 * WHY:  Direct interface with lexical access output
 * HOW:  Extract word ID and features, call semantic_integrate_word
 *
 * @param sem Integrator instance
 * @param entry Lexical entry from word recognition
 * @param result Output semantic result
 * @return true on success
 */
bool semantic_integrate_entry(
    semantic_integrator_t* sem,
    const void* entry,  /* lexical_entry_t* */
    semantic_result_t* result
);

/**
 * @brief Begin new utterance
 *
 * WHAT: Clear context for new sentence/utterance
 * WHY:  Fresh context window for each utterance
 * HOW:  Clear context buffer, reset thematic frame
 *
 * @param sem Integrator instance
 * @return true on success
 */
bool semantic_begin_utterance(semantic_integrator_t* sem);

/**
 * @brief End current utterance
 *
 * WHAT: Finalize utterance processing
 * WHY:  Compute final coherence, trigger consolidation
 * HOW:  Compute aggregate scores, update statistics
 *
 * @param sem Integrator instance
 * @param coherence Output: overall coherence score (0-1)
 * @return true on success
 */
bool semantic_end_utterance(
    semantic_integrator_t* sem,
    float* coherence
);

/*=============================================================================
 * DISAMBIGUATION
 *===========================================================================*/

/**
 * @brief Disambiguate word sense
 *
 * WHAT: Select most appropriate sense in context
 * WHY:  Resolve lexical ambiguity
 * HOW:  Weight senses by frequency and context fit
 *
 * @param sem Integrator instance
 * @param word_id Word ID
 * @param context_features Current context vector
 * @param context_dim Context dimension
 * @param selected_sense Output: selected sense ID
 * @param confidence Output: selection confidence
 * @return true on success
 */
bool semantic_disambiguate(
    semantic_integrator_t* sem,
    uint32_t word_id,
    const float* context_features,
    uint32_t context_dim,
    uint32_t* selected_sense,
    float* confidence
);

/**
 * @brief Force sense selection
 *
 * WHAT: Manually select specific sense
 * WHY:  Override automatic disambiguation
 * HOW:  Suppress alternatives, boost selected
 *
 * @param sem Integrator instance
 * @param word_id Word ID
 * @param sense_id Sense to select
 * @return true on success
 */
bool semantic_select_sense(
    semantic_integrator_t* sem,
    uint32_t word_id,
    uint32_t sense_id
);

/**
 * @brief Get disambiguation confidence
 *
 * @param sem Integrator instance
 * @param word_id Word ID
 * @return Confidence in selected sense (0-1)
 */
float semantic_get_disambiguation_confidence(
    const semantic_integrator_t* sem,
    uint32_t word_id
);

/*=============================================================================
 * CONTEXT MANAGEMENT
 *===========================================================================*/

/**
 * @brief Get current context
 *
 * @param sem Integrator instance
 * @param context Output context (do not free)
 * @return true on success
 */
bool semantic_get_context(
    const semantic_integrator_t* sem,
    const sentence_context_t** context
);

/**
 * @brief Get context vector
 *
 * WHAT: Get aggregated context representation
 * WHY:  Use for downstream processing
 * HOW:  Return accumulated context features
 *
 * @param sem Integrator instance
 * @param vector Output vector (copied)
 * @param dim Expected dimension
 * @return true on success
 */
bool semantic_get_context_vector(
    const semantic_integrator_t* sem,
    float* vector,
    uint32_t dim
);

/**
 * @brief Update context with external information
 *
 * WHAT: Inject external context (e.g., from vision)
 * WHY:  Multi-modal semantic integration
 * HOW:  Blend external features into context
 *
 * @param sem Integrator instance
 * @param features External feature vector
 * @param dim Feature dimension
 * @param weight Blend weight (0-1)
 * @return true on success
 */
bool semantic_update_context(
    semantic_integrator_t* sem,
    const float* features,
    uint32_t dim,
    float weight
);

/**
 * @brief Decay context over time
 *
 * WHAT: Reduce activation of older context words
 * WHY:  Model recency effects in comprehension
 * HOW:  Apply decay factor to context activations
 *
 * @param sem Integrator instance
 * @return true on success
 */
bool semantic_decay_context(semantic_integrator_t* sem);

/*=============================================================================
 * THEMATIC ROLE ASSIGNMENT
 *===========================================================================*/

/**
 * @brief Assign thematic role to concept
 *
 * WHAT: Label concept with semantic role
 * WHY:  Build argument structure of sentence
 * HOW:  Apply role assignment rules based on syntax/semantics
 *
 * @param sem Integrator instance
 * @param concept_id Concept ID
 * @param role Thematic role to assign
 * @return true on success
 */
bool semantic_assign_role(
    semantic_integrator_t* sem,
    uint32_t concept_id,
    thematic_role_t role
);

/**
 * @brief Get role for concept
 *
 * @param sem Integrator instance
 * @param concept_id Concept ID
 * @return Assigned role or ROLE_NONE
 */
thematic_role_t semantic_get_role(
    const semantic_integrator_t* sem,
    uint32_t concept_id
);

/**
 * @brief Get concept for role
 *
 * @param sem Integrator instance
 * @param role Thematic role
 * @return Concept ID or 0 if not assigned
 */
uint32_t semantic_get_role_filler(
    const semantic_integrator_t* sem,
    thematic_role_t role
);

/**
 * @brief Get thematic role name
 *
 * @param role Thematic role
 * @return String name (static, do not free)
 */
const char* semantic_role_name(thematic_role_t role);

/*=============================================================================
 * SPREADING ACTIVATION
 *===========================================================================*/

/**
 * @brief Activate concept with spreading
 *
 * WHAT: Activate concept and spread to related concepts
 * WHY:  Model semantic priming in conceptual network
 * HOW:  BFS with decay through semantic memory relations
 *
 * @param sem Integrator instance
 * @param concept_id Concept to activate
 * @param activation Initial activation (0-1)
 * @return true on success
 */
bool semantic_activate_concept(
    semantic_integrator_t* sem,
    uint32_t concept_id,
    float activation
);

/**
 * @brief Get active concepts
 *
 * @param sem Integrator instance
 * @param concepts Output concept array
 * @param max_concepts Maximum concepts to return
 * @param num_concepts Output: actual count
 * @return true on success
 */
bool semantic_get_active_concepts(
    const semantic_integrator_t* sem,
    active_concept_t* concepts,
    uint32_t max_concepts,
    uint32_t* num_concepts
);

/**
 * @brief Get concept activation level
 *
 * @param sem Integrator instance
 * @param concept_id Concept ID
 * @return Activation level (0-1) or 0 if not active
 */
float semantic_get_activation(
    const semantic_integrator_t* sem,
    uint32_t concept_id
);

/**
 * @brief Decay all activations
 *
 * WHAT: Reduce activation levels over time
 * WHY:  Model activation decay without input
 * HOW:  Multiply all activations by decay factor
 *
 * @param sem Integrator instance
 * @return true on success
 */
bool semantic_decay_activations(semantic_integrator_t* sem);

/*=============================================================================
 * SEMANTIC PRIMING
 *===========================================================================*/

/**
 * @brief Prime concept for faster access
 *
 * WHAT: Pre-activate concept and related concepts
 * WHY:  Model semantic priming effects
 * HOW:  Boost activation of concept and neighbors
 *
 * @param sem Integrator instance
 * @param concept_id Concept to prime
 * @param strength Priming strength (0-1)
 * @return true on success
 */
bool semantic_prime_concept(
    semantic_integrator_t* sem,
    uint32_t concept_id,
    float strength
);

/**
 * @brief Check if concept is primed
 *
 * @param sem Integrator instance
 * @param concept_id Concept ID
 * @return Priming level (0-1) or 0 if not primed
 */
float semantic_is_primed(
    const semantic_integrator_t* sem,
    uint32_t concept_id
);

/**
 * @brief Decay priming levels
 *
 * @param sem Integrator instance
 * @return true on success
 */
bool semantic_decay_priming(semantic_integrator_t* sem);

/**
 * @brief Clear all priming
 *
 * @param sem Integrator instance
 */
void semantic_clear_priming(semantic_integrator_t* sem);

/*=============================================================================
 * ANOMALY DETECTION (N400-like)
 *===========================================================================*/

/**
 * @brief Compute semantic anomaly score
 *
 * WHAT: Detect semantic violations/surprisal
 * WHY:  Model N400 ERP component
 * HOW:  Measure word-context semantic distance
 *
 * @param sem Integrator instance
 * @param word_id Word ID
 * @param concept_id Selected concept
 * @param anomaly_score Output: anomaly score (0-1)
 * @return true on success
 */
bool semantic_compute_anomaly(
    semantic_integrator_t* sem,
    uint32_t word_id,
    uint32_t concept_id,
    float* anomaly_score
);

/**
 * @brief Get current coherence score
 *
 * WHAT: Overall semantic coherence of utterance
 * WHY:  Monitor comprehension quality
 * HOW:  Average semantic relatedness across context
 *
 * @param sem Integrator instance
 * @return Coherence score (0-1)
 */
float semantic_get_coherence(const semantic_integrator_t* sem);

/**
 * @brief Set anomaly threshold
 *
 * @param sem Integrator instance
 * @param threshold Anomaly detection threshold (0-1)
 */
void semantic_set_anomaly_threshold(
    semantic_integrator_t* sem,
    float threshold
);

/*=============================================================================
 * INFERENCE
 *===========================================================================*/

/**
 * @brief Generate inferences from context
 *
 * WHAT: Derive implied meanings
 * WHY:  Go beyond literal word meanings
 * HOW:  Apply inference rules to concept combinations
 *
 * @param sem Integrator instance
 * @param inferred_concepts Output concept IDs
 * @param max_inferences Maximum inferences
 * @param num_inferred Output: actual count
 * @return true on success
 */
bool semantic_generate_inferences(
    semantic_integrator_t* sem,
    uint32_t* inferred_concepts,
    uint32_t max_inferences,
    uint32_t* num_inferred
);

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Get integration statistics
 *
 * @param sem Integrator instance
 * @param stats Output statistics
 * @return true on success
 */
bool semantic_get_stats(
    const semantic_integrator_t* sem,
    semantic_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param sem Integrator instance
 */
void semantic_reset_stats(semantic_integrator_t* sem);

/*=============================================================================
 * UTILITY
 *===========================================================================*/

/**
 * @brief Free semantic result resources
 *
 * @param result Result to free
 */
void semantic_free_result(semantic_result_t* result);

/**
 * @brief Get configuration
 *
 * @param sem Integrator instance
 * @param config Output configuration
 * @return true on success
 */
bool semantic_get_config(
    const semantic_integrator_t* sem,
    semantic_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SEMANTIC_INTEGRATOR_H */
