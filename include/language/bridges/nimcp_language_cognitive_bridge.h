//=============================================================================
// nimcp_language_cognitive_bridge.h - Language-Cognitive Bridge Integration
//=============================================================================
/**
 * @file nimcp_language_cognitive_bridge.h
 * @brief Bidirectional bridge integrating Language Layer with Cognitive Layer
 *
 * WHAT: Bridge connecting language processing with cognitive systems
 * WHY:  Enable working memory, attention, semantic memory, and reasoning
 *       to participate in language comprehension and production
 * HOW:  WM holds phonological loop, attention guides processing,
 *       semantic memory provides concepts, reasoning aids interpretation
 *
 * BIOLOGICAL BASIS:
 * - Working Memory: Phonological loop (Baddeley model) - 7±2 slots
 * - Attention: Selective attention to linguistic features
 * - Semantic Memory: Concept activation, spreading activation
 * - Reasoning: Inference for pragmatic interpretation
 * - Executive: Conflict resolution, ambiguity handling
 *
 * KEY CONNECTIONS:
 * - Working Memory: Phonological buffer, subvocal rehearsal
 * - Attention: Feature attention, word/phoneme attention
 * - Semantic Memory: Concept lookup, spreading activation
 * - Reasoning: Pragmatic inference, implicature
 * - Executive: Goal management, conflict resolution
 *
 * DATA FLOW:
 * - Cognitive → Language: Concepts, attention, inferences
 * - Language → Cognitive: Words to rehearse, concepts to activate
 *
 * @version 1.0.0 - Phase L2: Language Layer Bridges
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#ifndef NIMCP_LANGUAGE_COGNITIVE_BRIDGE_H
#define NIMCP_LANGUAGE_COGNITIVE_BRIDGE_H

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

typedef struct language_cognitive_bridge language_cognitive_bridge_t;
typedef struct language_orchestrator language_orchestrator_t;
typedef struct working_memory_system working_memory_system_t;
typedef struct attention_system attention_system_t;
typedef struct semantic_memory_system semantic_memory_system_t;
typedef struct reasoning_engine reasoning_engine_t;
typedef struct executive_controller executive_controller_t;

/* bio_router_t is a pointer type defined in bio_router.h */
#ifndef NIMCP_BIO_ROUTER_H
typedef void* bio_router_t;
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module identification */
#define LANGUAGE_COGNITIVE_MODULE_NAME      "language_cognitive_bridge"
#define LANGUAGE_COGNITIVE_MODULE_VERSION   "1.0.0"

/** Default configuration values */
#define LANGUAGE_COGNITIVE_DEFAULT_UPDATE_INTERVAL_MS    20
#define LANGUAGE_COGNITIVE_DEFAULT_WM_SLOTS              7
#define LANGUAGE_COGNITIVE_DEFAULT_REHEARSAL_DECAY       0.95f
#define LANGUAGE_COGNITIVE_DEFAULT_SPREADING_DECAY       0.8f
#define LANGUAGE_COGNITIVE_DEFAULT_SPREADING_DEPTH       3

/** Semantic memory */
#define LANGUAGE_COGNITIVE_MAX_ACTIVE_CONCEPTS           64
#define LANGUAGE_COGNITIVE_ACTIVATION_THRESHOLD          0.1f

/** Attention */
#define LANGUAGE_COGNITIVE_ATTENTION_BASE                0.5f
#define LANGUAGE_COGNITIVE_ATTENTION_BOOST               0.3f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Working memory buffer types
 */
typedef enum {
    WM_BUFFER_PHONOLOGICAL = 0,       /**< Phonological loop */
    WM_BUFFER_VISUOSPATIAL,           /**< Visuospatial sketchpad */
    WM_BUFFER_EPISODIC,               /**< Episodic buffer */
    WM_BUFFER_COUNT
} wm_buffer_type_t;

/**
 * @brief Semantic spreading mode
 */
typedef enum {
    SPREADING_MODE_BREADTH_FIRST = 0, /**< BFS spreading */
    SPREADING_MODE_DEPTH_FIRST,       /**< DFS spreading */
    SPREADING_MODE_WEIGHTED,          /**< Weight-based spreading */
    SPREADING_MODE_COUNT
} spreading_mode_t;

/**
 * @brief Attention target types
 */
typedef enum {
    ATTENTION_TARGET_PHONEME = 0,     /**< Phoneme-level attention */
    ATTENTION_TARGET_WORD,            /**< Word-level attention */
    ATTENTION_TARGET_PHRASE,          /**< Phrase-level attention */
    ATTENTION_TARGET_DISCOURSE,       /**< Discourse-level attention */
    ATTENTION_TARGET_COUNT
} attention_target_t;

/**
 * @brief Inference types for reasoning
 */
typedef enum {
    INFERENCE_LITERAL = 0,            /**< Literal meaning */
    INFERENCE_IMPLICATURE,            /**< Pragmatic implicature */
    INFERENCE_PRESUPPOSITION,         /**< Presupposition */
    INFERENCE_REFERENCE,              /**< Reference resolution */
    INFERENCE_COUNT
} inference_type_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Phonological loop item
 *
 * BIOLOGICAL BASIS:
 * - Represents item in Baddeley's phonological loop
 * - Decays without rehearsal (~2 seconds)
 * - Capacity limited (7±2 items)
 */
typedef struct {
    uint32_t word_id;                 /**< Word identifier */
    char phonological_form[64];       /**< Phonological representation */
    float activation;                 /**< Current activation [0-1] */
    float decay_rate;                 /**< Decay rate */
    uint64_t entry_time_ms;           /**< When entered buffer */
    uint64_t last_rehearsal_ms;       /**< Last rehearsal time */
    bool needs_rehearsal;             /**< Rehearsal needed */
} phonological_item_t;

/**
 * @brief Working memory connection data
 */
typedef struct {
    /* Phonological loop */
    phonological_item_t* phonological_buffer;  /**< Phonological items */
    uint32_t phonological_capacity;            /**< Buffer capacity */
    uint32_t phonological_count;               /**< Current item count */

    /* Rehearsal */
    bool articulatory_rehearsal_active;        /**< Subvocal rehearsal on */
    float rehearsal_rate;                      /**< Rehearsal rate (items/sec) */
    uint32_t current_rehearsal_idx;            /**< Current rehearsal position */

    /* Binding */
    float* episodic_binding;                   /**< Episodic buffer binding */
    uint32_t binding_dim;                      /**< Binding dimension */
} language_working_memory_data_t;

/**
 * @brief Attention data
 */
typedef struct {
    /* Feature attention */
    float* feature_weights;                    /**< Feature attention weights */
    uint32_t num_features;                     /**< Number of features */

    /* Word attention */
    float* word_attention;                     /**< Per-word attention */
    uint32_t num_words;                        /**< Number of words tracked */
    uint32_t focus_word_idx;                   /**< Currently focused word */

    /* Linguistic attention level */
    float linguistic_attention;                /**< Overall language attention */
    attention_target_t current_target;         /**< Current attention target */

    /* Modulation */
    float attention_gain;                      /**< Attention gain factor */
    bool suppression_active;                   /**< Distractor suppression on */
} language_attention_data_t;

/**
 * @brief Active concept in semantic memory
 */
typedef struct {
    uint32_t concept_id;                       /**< Concept identifier */
    char name[64];                             /**< Concept name */
    float activation;                          /**< Activation level [0-1] */
    float* semantic_vector;                    /**< Semantic embedding */
    uint32_t semantic_dim;                     /**< Embedding dimension */
    uint32_t source_word_id;                   /**< Activating word */
    uint32_t spreading_depth;                  /**< Depth from source */
    uint64_t activation_time_ms;               /**< Activation timestamp */
} active_concept_t;

/**
 * @brief Semantic memory connection data
 */
typedef struct {
    /* Active concepts */
    active_concept_t* active_concepts;         /**< Currently active concepts */
    uint32_t max_concepts;                     /**< Maximum capacity */
    uint32_t num_active;                       /**< Currently active count */

    /* Spreading activation */
    spreading_mode_t spreading_mode;           /**< Spreading strategy */
    float spreading_decay;                     /**< Activation decay per hop */
    uint32_t max_spreading_depth;              /**< Maximum hops */
    bool spreading_in_progress;                /**< Spreading active */

    /* Context */
    float* context_vector;                     /**< Current context embedding */
    uint32_t context_dim;                      /**< Context dimension */
    float context_coherence;                   /**< Context coherence [0-1] */
} language_semantic_memory_data_t;

/**
 * @brief Inference result
 */
typedef struct {
    inference_type_t type;                     /**< Inference type */
    uint32_t source_word_idx;                  /**< Source word position */
    float confidence;                          /**< Inference confidence */
    char inference_text[256];                  /**< Inferred meaning */
    uint32_t resolved_reference_id;            /**< For reference resolution */
    bool valid;                                /**< Inference valid */
} language_inference_t;

/**
 * @brief Reasoning engine connection data
 */
typedef struct {
    /* Inferences */
    language_inference_t* inferences;          /**< Current inferences */
    uint32_t max_inferences;                   /**< Maximum capacity */
    uint32_t num_inferences;                   /**< Current count */

    /* Reference resolution */
    uint32_t* reference_candidates;            /**< Candidate referents */
    float* reference_scores;                   /**< Candidate scores */
    uint32_t num_candidates;                   /**< Number of candidates */

    /* Pragmatic state */
    float implicature_strength;                /**< Implicature detection strength */
    bool inference_enabled;                    /**< Inference processing on */
} language_reasoning_data_t;

/**
 * @brief Executive control data
 */
typedef struct {
    /* Conflict detection */
    float conflict_level;                      /**< Current conflict [0-1] */
    bool ambiguity_detected;                   /**< Ambiguous input */
    uint32_t ambiguity_word_idx;               /**< Ambiguous word position */

    /* Goal state */
    bool comprehension_goal_active;            /**< Comprehension goal */
    bool production_goal_active;               /**< Production goal */
    float goal_priority;                       /**< Current goal priority */

    /* Control signals */
    float inhibition_strength;                 /**< Inhibitory control */
    float flexibility;                         /**< Cognitive flexibility */
} language_executive_data_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Working memory */
    uint64_t items_rehearsed;                  /**< Total rehearsals */
    uint64_t items_forgotten;                  /**< Items lost to decay */
    float avg_buffer_utilization;              /**< Average WM utilization */

    /* Semantic memory */
    uint64_t concepts_activated;               /**< Total activations */
    uint64_t spreading_events;                 /**< Spreading events */
    float avg_concept_activation;              /**< Average activation */

    /* Reasoning */
    uint64_t inferences_made;                  /**< Total inferences */
    float avg_inference_confidence;            /**< Average confidence */

    /* Performance */
    float avg_processing_time_ms;              /**< Average processing time */
    uint64_t last_update_time_ms;              /**< Last update timestamp */
} language_cognitive_stats_t;

//=============================================================================
// Bridge State Structure
//=============================================================================

/**
 * @brief Language-cognitive bridge state
 */
struct language_cognitive_bridge {
    /* Configuration */
    language_cognitive_config_t config;        /**< Bridge configuration */
    bool initialized;                          /**< Initialization state */
    bool active;                               /**< Active processing */

    /* Connected components */
    language_orchestrator_t* orchestrator;     /**< Parent orchestrator */
    working_memory_system_t* working_memory;   /**< Working memory system */
    attention_system_t* attention;             /**< Attention system */
    semantic_memory_system_t* semantic_memory; /**< Semantic memory */
    reasoning_engine_t* reasoning;             /**< Reasoning engine */
    executive_controller_t* executive;         /**< Executive controller */

    /* Connection data */
    language_working_memory_data_t wm_data;    /**< Working memory data */
    language_attention_data_t attention_data;  /**< Attention data */
    language_semantic_memory_data_t semantic_data; /**< Semantic memory data */
    language_reasoning_data_t reasoning_data;  /**< Reasoning data */
    language_executive_data_t executive_data;  /**< Executive data */

    /* Statistics */
    language_cognitive_stats_t stats;          /**< Bridge statistics */

    /* Bio-async */
    bio_router_t* bio_router;                  /**< Bio-async router */
    bool bio_async_registered;                 /**< Registration status */
};

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create language-cognitive bridge
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return New bridge instance or NULL on error
 */
language_cognitive_bridge_t* language_cognitive_bridge_create(
    const language_cognitive_config_t* config
);

/**
 * @brief Destroy language-cognitive bridge
 *
 * @param bridge Bridge instance
 */
void language_cognitive_bridge_destroy(language_cognitive_bridge_t* bridge);

/**
 * @brief Initialize bridge with default configuration
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_cognitive_bridge_init(language_cognitive_bridge_t* bridge);

/**
 * @brief Start bridge processing
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_cognitive_bridge_start(language_cognitive_bridge_t* bridge);

/**
 * @brief Stop bridge processing
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_cognitive_bridge_stop(language_cognitive_bridge_t* bridge);

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
int language_cognitive_bridge_connect_orchestrator(
    language_cognitive_bridge_t* bridge,
    language_orchestrator_t* orchestrator
);

/**
 * @brief Connect to working memory system
 *
 * @param bridge Bridge instance
 * @param working_memory Working memory system
 * @return 0 on success, -1 on error
 */
int language_cognitive_bridge_connect_working_memory(
    language_cognitive_bridge_t* bridge,
    working_memory_system_t* working_memory
);

/**
 * @brief Connect to attention system
 *
 * @param bridge Bridge instance
 * @param attention Attention system
 * @return 0 on success, -1 on error
 */
int language_cognitive_bridge_connect_attention(
    language_cognitive_bridge_t* bridge,
    attention_system_t* attention
);

/**
 * @brief Connect to semantic memory
 *
 * @param bridge Bridge instance
 * @param semantic_memory Semantic memory system
 * @return 0 on success, -1 on error
 */
int language_cognitive_bridge_connect_semantic_memory(
    language_cognitive_bridge_t* bridge,
    semantic_memory_system_t* semantic_memory
);

/**
 * @brief Connect to reasoning engine
 *
 * @param bridge Bridge instance
 * @param reasoning Reasoning engine
 * @return 0 on success, -1 on error
 */
int language_cognitive_bridge_connect_reasoning(
    language_cognitive_bridge_t* bridge,
    reasoning_engine_t* reasoning
);

/**
 * @brief Connect to executive controller
 *
 * @param bridge Bridge instance
 * @param executive Executive controller
 * @return 0 on success, -1 on error
 */
int language_cognitive_bridge_connect_executive(
    language_cognitive_bridge_t* bridge,
    executive_controller_t* executive
);

//=============================================================================
// Working Memory API
//=============================================================================

/**
 * @brief Add word to phonological buffer
 *
 * @param bridge Bridge instance
 * @param word Word to add
 * @return 0 on success, -1 on error (buffer full)
 */
int language_cognitive_bridge_wm_add_word(
    language_cognitive_bridge_t* bridge,
    const language_word_t* word
);

/**
 * @brief Trigger subvocal rehearsal
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_cognitive_bridge_wm_rehearse(
    language_cognitive_bridge_t* bridge
);

/**
 * @brief Get phonological buffer contents
 *
 * @param bridge Bridge instance
 * @param items Output array
 * @param max_items Maximum items to retrieve
 * @return Number of items retrieved, or -1 on error
 */
int language_cognitive_bridge_wm_get_items(
    const language_cognitive_bridge_t* bridge,
    phonological_item_t* items,
    uint32_t max_items
);

/**
 * @brief Get current phonological loop load
 *
 * @param bridge Bridge instance
 * @return Load as fraction of capacity [0-1]
 */
float language_cognitive_bridge_wm_get_load(
    const language_cognitive_bridge_t* bridge
);

//=============================================================================
// Semantic Memory API
//=============================================================================

/**
 * @brief Activate concept from word
 *
 * @param bridge Bridge instance
 * @param word_id Word identifier
 * @param activation Initial activation level
 * @return Concept ID, or 0 on error
 */
uint32_t language_cognitive_bridge_activate_concept(
    language_cognitive_bridge_t* bridge,
    uint32_t word_id,
    float activation
);

/**
 * @brief Trigger spreading activation
 *
 * @param bridge Bridge instance
 * @param source_concept_id Source concept for spreading
 * @return 0 on success, -1 on error
 */
int language_cognitive_bridge_spread_activation(
    language_cognitive_bridge_t* bridge,
    uint32_t source_concept_id
);

/**
 * @brief Get active concepts
 *
 * @param bridge Bridge instance
 * @param concepts Output array
 * @param max_concepts Maximum concepts to retrieve
 * @return Number of concepts retrieved, or -1 on error
 */
int language_cognitive_bridge_get_active_concepts(
    const language_cognitive_bridge_t* bridge,
    active_concept_t* concepts,
    uint32_t max_concepts
);

/**
 * @brief Get concept by word
 *
 * @param bridge Bridge instance
 * @param word_id Word identifier
 * @param concept Output concept
 * @return 0 on success, -1 if not found
 */
int language_cognitive_bridge_get_concept_for_word(
    const language_cognitive_bridge_t* bridge,
    uint32_t word_id,
    active_concept_t* concept_out
);

//=============================================================================
// Attention API
//=============================================================================

/**
 * @brief Set attention to word
 *
 * @param bridge Bridge instance
 * @param word_idx Word index in utterance
 * @param attention Attention level [0-1]
 * @return 0 on success, -1 on error
 */
int language_cognitive_bridge_set_word_attention(
    language_cognitive_bridge_t* bridge,
    uint32_t word_idx,
    float attention
);

/**
 * @brief Get current attention distribution
 *
 * @param bridge Bridge instance
 * @param attention Output attention array
 * @param max_words Maximum words
 * @return Number of words, or -1 on error
 */
int language_cognitive_bridge_get_attention(
    const language_cognitive_bridge_t* bridge,
    float* attention,
    uint32_t max_words
);

/**
 * @brief Set linguistic attention target
 *
 * @param bridge Bridge instance
 * @param target Attention target level
 * @return 0 on success, -1 on error
 */
int language_cognitive_bridge_set_attention_target(
    language_cognitive_bridge_t* bridge,
    attention_target_t target
);

//=============================================================================
// Reasoning API
//=============================================================================

/**
 * @brief Request inference for utterance
 *
 * @param bridge Bridge instance
 * @param type Inference type requested
 * @return 0 on success, -1 on error
 */
int language_cognitive_bridge_request_inference(
    language_cognitive_bridge_t* bridge,
    inference_type_t type
);

/**
 * @brief Get current inferences
 *
 * @param bridge Bridge instance
 * @param inferences Output array
 * @param max_inferences Maximum to retrieve
 * @return Number retrieved, or -1 on error
 */
int language_cognitive_bridge_get_inferences(
    const language_cognitive_bridge_t* bridge,
    language_inference_t* inferences,
    uint32_t max_inferences
);

/**
 * @brief Resolve reference (anaphora)
 *
 * @param bridge Bridge instance
 * @param referring_word_idx Word index of referring expression
 * @param referent_id Output referent ID
 * @return 0 on success, -1 if unresolved
 */
int language_cognitive_bridge_resolve_reference(
    language_cognitive_bridge_t* bridge,
    uint32_t referring_word_idx,
    uint32_t* referent_id
);

//=============================================================================
// Executive API
//=============================================================================

/**
 * @brief Report conflict/ambiguity
 *
 * @param bridge Bridge instance
 * @param word_idx Ambiguous word index
 * @param conflict_level Conflict magnitude [0-1]
 * @return 0 on success, -1 on error
 */
int language_cognitive_bridge_report_conflict(
    language_cognitive_bridge_t* bridge,
    uint32_t word_idx,
    float conflict_level
);

/**
 * @brief Get current conflict level
 *
 * @param bridge Bridge instance
 * @return Conflict level [0-1]
 */
float language_cognitive_bridge_get_conflict(
    const language_cognitive_bridge_t* bridge
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
int language_cognitive_bridge_update(
    language_cognitive_bridge_t* bridge,
    uint64_t current_time_ms
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int language_cognitive_bridge_get_stats(
    const language_cognitive_bridge_t* bridge,
    language_cognitive_stats_t* stats
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
int language_cognitive_bridge_bio_async_register(
    language_cognitive_bridge_t* bridge,
    bio_router_t* router
);

/**
 * @brief Unregister from bio-async router
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_cognitive_bridge_bio_async_unregister(
    language_cognitive_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LANGUAGE_COGNITIVE_BRIDGE_H */
