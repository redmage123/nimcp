//=============================================================================
// nimcp_language_hippocampus_bridge.h - Language-Hippocampus Memory Bridge
//=============================================================================
/**
 * @file nimcp_language_hippocampus_bridge.h
 * @brief Bidirectional bridge integrating Language Layer with Hippocampus
 *
 * WHAT: Bridge connecting Language Layer with Hippocampus for lexical memory,
 *       vocabulary learning, and language-context episodic encoding
 * WHY:  Enable word learning, semantic memory consolidation, and contextual
 *       language memory retrieval through hippocampal-cortical interactions
 * HOW:  Language encodes new vocabulary and contexts; Hippocampus provides
 *       memory consolidation and retrieval support
 *
 * BIOLOGICAL BASIS:
 * - Hippocampus: Episodic memory encoding and retrieval
 * - Parahippocampal regions: Semantic memory consolidation
 * - Dentate Gyrus: Pattern separation for similar words
 * - CA3: Pattern completion for partial word recall
 * - CA1-Cortical connections: Memory consolidation to language areas
 *
 * KEY CONNECTIONS:
 * - Language → Hippocampus: New word encoding, semantic associations
 * - Hippocampus → Language: Memory retrieval, vocabulary recall
 * - Wernicke → Hippocampus: Comprehension context encoding
 * - Broca → Hippocampus: Production context encoding
 *
 * MEMORY FUNCTIONS:
 * - Vocabulary learning: Encode new words with meanings
 * - Semantic memory: Store word-concept associations
 * - Episodic language: Remember language events in context
 * - Memory consolidation: Transfer to long-term cortical storage
 * - Pattern completion: Retrieve words from partial cues
 *
 * @version 1.0.0 - Phase LH1: Language-Hippocampus Integration
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#ifndef NIMCP_LANGUAGE_HIPPOCAMPUS_BRIDGE_H
#define NIMCP_LANGUAGE_HIPPOCAMPUS_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

/* Include hippocampus adapter first to avoid include order issues */
#include "core/brain/regions/hippocampus/nimcp_hippocampus_adapter.h"

/* Language types after bio_messages */
#include "language/nimcp_language_types.h"
#include "language/nimcp_language_config.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct language_hippocampus_bridge language_hippocampus_bridge_t;
typedef struct language_orchestrator language_orchestrator_t;
typedef struct broca_adapter broca_adapter_t;
typedef struct wernicke_adapter wernicke_adapter_t;

#ifndef NIMCP_BIO_ROUTER_H
typedef void* bio_router_t;
#endif

//=============================================================================
// Constants
//=============================================================================

#define LANGUAGE_HIPPOCAMPUS_MODULE_NAME      "language_hippocampus_bridge"
#define LANGUAGE_HIPPOCAMPUS_MODULE_VERSION   "1.0.0"
#define LANGUAGE_HIPPOCAMPUS_BIO_MODULE_ID    0x0824

/* Default configuration values */
#define LH_DEFAULT_UPDATE_INTERVAL_MS        50
#define LH_DEFAULT_MAX_WORD_MEMORIES         1024
#define LH_DEFAULT_FEATURE_DIM               64
#define LH_DEFAULT_CONSOLIDATION_THRESHOLD   0.7f
#define LH_DEFAULT_RETRIEVAL_THRESHOLD       0.5f
#define LH_DEFAULT_DECAY_RATE                0.001f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Language memory type
 */
typedef enum {
    LANG_MEM_LEXICAL = 0,                 /**< Word form memory */
    LANG_MEM_SEMANTIC,                    /**< Word meaning memory */
    LANG_MEM_PHONOLOGICAL,                /**< Sound pattern memory */
    LANG_MEM_SYNTACTIC,                   /**< Grammar pattern memory */
    LANG_MEM_EPISODIC,                    /**< Context/event memory */
    LANG_MEM_PROCEDURAL,                  /**< Language skill memory */
    LANG_MEM_COUNT
} language_memory_type_t;

/**
 * @brief Memory encoding strength
 */
typedef enum {
    ENCODING_WEAK = 0,                    /**< Weak/incidental encoding */
    ENCODING_MODERATE,                    /**< Normal encoding */
    ENCODING_STRONG,                      /**< Intentional/repeated encoding */
    ENCODING_CONSOLIDATED,                /**< Transferred to cortex */
    ENCODING_COUNT
} encoding_strength_t;

/**
 * @brief Retrieval cue type
 */
typedef enum {
    CUE_ORTHOGRAPHIC = 0,                 /**< Written form cue */
    CUE_PHONOLOGICAL,                     /**< Sound form cue */
    CUE_SEMANTIC,                         /**< Meaning cue */
    CUE_CONTEXTUAL,                       /**< Context/situation cue */
    CUE_ASSOCIATION,                      /**< Associated word cue */
    CUE_COUNT
} retrieval_cue_type_t;

/**
 * @brief Memory operation result
 */
typedef enum {
    MEM_OP_SUCCESS = 0,                   /**< Operation succeeded */
    MEM_OP_NOT_FOUND,                     /**< Memory not found */
    MEM_OP_PARTIAL,                       /**< Partial retrieval */
    MEM_OP_INTERFERENCE,                  /**< Interference from similar */
    MEM_OP_CAPACITY_ERROR,                /**< Memory capacity exceeded */
    MEM_OP_ENCODING_ERROR,                /**< Encoding failed */
    MEM_OP_COUNT
} memory_operation_result_t;

/**
 * @brief Bridge operating state
 */
typedef enum {
    LH_STATE_IDLE = 0,                    /**< No active processing */
    LH_STATE_ENCODING,                    /**< Encoding new memory */
    LH_STATE_RETRIEVING,                  /**< Retrieving memory */
    LH_STATE_CONSOLIDATING,               /**< Consolidation in progress */
    LH_STATE_ERROR,                       /**< Error state */
    LH_STATE_COUNT
} lh_bridge_state_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Configuration for Language-Hippocampus bridge
 */
typedef struct {
    /* Operating parameters */
    uint32_t update_interval_ms;          /**< Update cycle interval */
    uint32_t max_word_memories;           /**< Maximum word memories */
    uint32_t feature_dim;                 /**< Feature vector dimension */

    /* Memory parameters */
    float consolidation_threshold;        /**< Threshold for consolidation */
    float retrieval_threshold;            /**< Minimum similarity for retrieval */
    float decay_rate;                     /**< Memory decay rate per update */

    /* Features */
    bool enable_pattern_separation;       /**< Use DG for similar words */
    bool enable_pattern_completion;       /**< Use CA3 for partial recall */
    bool enable_consolidation;            /**< Enable cortical transfer */
    bool enable_replay;                   /**< Enable memory replay */
    bool enable_semantic_priming;         /**< Enable semantic priming */

    /* Bio-async */
    bool enable_bio_async;                /**< Enable bio-async messaging */
} language_hippocampus_config_t;

/**
 * @brief Word memory entry
 */
typedef struct {
    uint32_t memory_id;                   /**< Memory identifier */
    char word[64];                        /**< Word string */
    uint32_t word_id;                     /**< Internal word ID */
    language_memory_type_t type;          /**< Memory type */
    float* semantic_features;             /**< Semantic feature vector */
    uint32_t feature_count;               /**< Feature vector size */
    float strength;                       /**< Memory strength [0-1] */
    encoding_strength_t encoding;         /**< Encoding strength level */
    uint64_t encoding_time_ms;            /**< When encoded */
    uint64_t last_access_ms;              /**< Last retrieval time */
    uint32_t access_count;                /**< Times retrieved */
    bool is_consolidated;                 /**< Transferred to cortex */
} word_memory_t;

/**
 * @brief Semantic association
 */
typedef struct {
    uint32_t word_a_id;                   /**< First word memory ID */
    uint32_t word_b_id;                   /**< Second word memory ID */
    float strength;                       /**< Association strength [0-1] */
    char relation_type[32];               /**< Relation type (synonym, antonym, etc.) */
} semantic_association_t;

/**
 * @brief Encoding request
 */
typedef struct {
    char word[64];                        /**< Word to encode */
    language_memory_type_t type;          /**< Memory type */
    float* semantic_features;             /**< Semantic features (optional) */
    uint32_t feature_count;               /**< Feature count */
    float emotional_valence;              /**< Emotional tag [-1, 1] */
    encoding_strength_t encoding;         /**< Encoding strength */
    uint32_t* associations;               /**< Associated word IDs */
    uint32_t association_count;           /**< Number of associations */
} encoding_request_t;

/**
 * @brief Retrieval request
 */
typedef struct {
    retrieval_cue_type_t cue_type;        /**< Type of retrieval cue */
    char cue_string[64];                  /**< String cue (if applicable) */
    float* cue_features;                  /**< Feature cue (if applicable) */
    uint32_t feature_count;               /**< Feature count */
    uint32_t max_results;                 /**< Maximum results to return */
    float similarity_threshold;           /**< Minimum similarity */
} retrieval_request_t;

/**
 * @brief Retrieval result
 */
typedef struct {
    word_memory_t* memories;              /**< Retrieved memories */
    uint32_t count;                       /**< Number of memories */
    float* similarities;                  /**< Similarity scores */
    memory_operation_result_t status;     /**< Operation status */
} lh_retrieval_result_t;

/**
 * @brief Consolidation event
 */
typedef struct {
    uint32_t memory_id;                   /**< Memory being consolidated */
    char word[64];                        /**< Word being consolidated */
    language_memory_type_t type;          /**< Memory type */
    float final_strength;                 /**< Strength at consolidation */
    uint64_t consolidation_time_ms;       /**< When consolidated */
} consolidation_event_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Counts */
    uint64_t words_encoded;               /**< Words encoded */
    uint64_t retrieval_attempts;          /**< Retrieval attempts */
    uint64_t successful_retrievals;       /**< Successful retrievals */
    uint64_t consolidations;              /**< Memories consolidated */
    uint64_t replay_events;               /**< Replay events triggered */

    /* Memory breakdown */
    uint64_t memories_by_type[LANG_MEM_COUNT];  /**< Memories by type */

    /* Quality */
    float avg_retrieval_similarity;       /**< Average retrieval similarity */
    float avg_memory_strength;            /**< Average memory strength */
    float retrieval_success_rate;         /**< Retrieval success rate */

    /* Current state */
    uint32_t current_memory_count;        /**< Current memory count */
    uint32_t consolidated_count;          /**< Consolidated count */
    lh_bridge_state_t bridge_state;       /**< Current bridge state */
} language_hippocampus_stats_t;

//=============================================================================
// Callback Types
//=============================================================================

typedef void (*lh_encoding_callback_t)(const word_memory_t* memory, void* user_data);
typedef void (*lh_retrieval_callback_t)(const lh_retrieval_result_t* result, void* user_data);
typedef void (*lh_consolidation_callback_t)(const consolidation_event_t* event, void* user_data);

//=============================================================================
// Lifecycle Functions
//=============================================================================

language_hippocampus_config_t language_hippocampus_default_config(void);

language_hippocampus_bridge_t* language_hippocampus_bridge_create(
    language_orchestrator_t* language,
    hippocampus_adapter_t* hippocampus,
    const language_hippocampus_config_t* config
);

void language_hippocampus_bridge_destroy(language_hippocampus_bridge_t* bridge);

int language_hippocampus_bridge_reset(language_hippocampus_bridge_t* bridge);

//=============================================================================
// Connection Functions
//=============================================================================

int language_hippocampus_connect_broca(
    language_hippocampus_bridge_t* bridge,
    broca_adapter_t* broca
);

int language_hippocampus_connect_wernicke(
    language_hippocampus_bridge_t* bridge,
    wernicke_adapter_t* wernicke
);

int language_hippocampus_connect_bio_async(
    language_hippocampus_bridge_t* bridge,
    bio_router_t router
);

//=============================================================================
// Update Functions
//=============================================================================

int language_hippocampus_bridge_update(
    language_hippocampus_bridge_t* bridge,
    uint64_t timestamp_ms
);

//=============================================================================
// Memory Encoding (Language -> Hippocampus)
//=============================================================================

uint32_t language_hippocampus_encode_word(
    language_hippocampus_bridge_t* bridge,
    const encoding_request_t* request
);

int language_hippocampus_encode_association(
    language_hippocampus_bridge_t* bridge,
    uint32_t word_a_id,
    uint32_t word_b_id,
    float strength,
    const char* relation_type
);

int language_hippocampus_strengthen_memory(
    language_hippocampus_bridge_t* bridge,
    uint32_t memory_id,
    float strength_boost
);

//=============================================================================
// Memory Retrieval (Hippocampus -> Language)
//=============================================================================

int language_hippocampus_retrieve(
    language_hippocampus_bridge_t* bridge,
    const retrieval_request_t* request,
    lh_retrieval_result_t* result
);

int language_hippocampus_retrieve_by_word(
    language_hippocampus_bridge_t* bridge,
    const char* word,
    word_memory_t* memory
);

int language_hippocampus_retrieve_associations(
    language_hippocampus_bridge_t* bridge,
    uint32_t word_id,
    semantic_association_t* associations,
    uint32_t max_associations
);

int language_hippocampus_pattern_complete(
    language_hippocampus_bridge_t* bridge,
    const char* partial_word,
    char* completed_words,
    uint32_t max_completions
);

//=============================================================================
// Memory Consolidation
//=============================================================================

int language_hippocampus_trigger_consolidation(
    language_hippocampus_bridge_t* bridge,
    float strength_threshold
);

int language_hippocampus_trigger_replay(
    language_hippocampus_bridge_t* bridge,
    uint32_t num_memories,
    bool reverse_order
);

bool language_hippocampus_is_consolidated(
    const language_hippocampus_bridge_t* bridge,
    uint32_t memory_id
);

//=============================================================================
// Memory Management
//=============================================================================

int language_hippocampus_get_memory(
    const language_hippocampus_bridge_t* bridge,
    uint32_t memory_id,
    word_memory_t* memory
);

int language_hippocampus_delete_memory(
    language_hippocampus_bridge_t* bridge,
    uint32_t memory_id
);

uint32_t language_hippocampus_get_memory_count(
    const language_hippocampus_bridge_t* bridge
);

//=============================================================================
// Callback Registration
//=============================================================================

int language_hippocampus_set_encoding_callback(
    language_hippocampus_bridge_t* bridge,
    lh_encoding_callback_t callback,
    void* user_data
);

int language_hippocampus_set_retrieval_callback(
    language_hippocampus_bridge_t* bridge,
    lh_retrieval_callback_t callback,
    void* user_data
);

int language_hippocampus_set_consolidation_callback(
    language_hippocampus_bridge_t* bridge,
    lh_consolidation_callback_t callback,
    void* user_data
);

//=============================================================================
// Status and Statistics
//=============================================================================

lh_bridge_state_t language_hippocampus_get_state(
    const language_hippocampus_bridge_t* bridge
);

int language_hippocampus_get_stats(
    const language_hippocampus_bridge_t* bridge,
    language_hippocampus_stats_t* stats
);

void language_hippocampus_reset_stats(language_hippocampus_bridge_t* bridge);

int language_hippocampus_get_config(
    const language_hippocampus_bridge_t* bridge,
    language_hippocampus_config_t* config
);

int language_hippocampus_set_config(
    language_hippocampus_bridge_t* bridge,
    const language_hippocampus_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LANGUAGE_HIPPOCAMPUS_BRIDGE_H */
