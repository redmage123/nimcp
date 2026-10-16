//=============================================================================
// nimcp_language_temporal_bridge.h - Language-Temporal Lobe Bridge
//=============================================================================
/**
 * @file nimcp_language_temporal_bridge.h
 * @brief Bidirectional bridge integrating Language Layer with Temporal Lobe
 *
 * WHAT: Bridge connecting language processing with temporal cortex
 * WHY:  Enable semantic memory, auditory processing, and concept retrieval
 *       to participate in language comprehension and production
 * HOW:  Temporal provides semantic concepts and auditory features;
 *       Language sends lexical queries and activated word forms
 *
 * BIOLOGICAL BASIS:
 * - Superior Temporal Gyrus (STG): Speech perception, routes to Wernicke
 * - Anterior Temporal Lobe (ATL): Semantic hub, concept binding
 * - Middle Temporal Gyrus (MTG): Lexical-semantic interface
 * - Inferotemporal Cortex (IT): Visual word forms, reading
 *
 * KEY CONNECTIONS:
 * - Auditory → Wernicke: Speech features, phoneme candidates
 * - Semantic → Wernicke: Concept activation, word meanings
 * - Wernicke → Semantic: Lexical queries, priming requests
 * - Broca → Semantic: Word selection queries, production planning
 *
 * DATA FLOW:
 * - Temporal → Language: Semantic concepts, auditory features, priming
 * - Language → Temporal: Word activations, semantic queries, speech tokens
 *
 * @version 1.0.0 - Phase LT1: Language-Temporal Integration
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#ifndef NIMCP_LANGUAGE_TEMPORAL_BRIDGE_H
#define NIMCP_LANGUAGE_TEMPORAL_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

/* Include temporal adapter first to avoid macro/enum conflict with bio_messages */
#include "core/brain/regions/temporal/nimcp_temporal_adapter.h"

/* Language types must come after bio_messages is included */
#include "language/nimcp_language_types.h"
#include "language/nimcp_language_config.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct language_temporal_bridge language_temporal_bridge_t;
typedef struct language_orchestrator language_orchestrator_t;
typedef struct wernicke_adapter wernicke_adapter_t;
typedef struct broca_adapter broca_adapter_t;

/* bio_router_t is a pointer type defined in bio_router.h */
#ifndef NIMCP_BIO_ROUTER_H
typedef void* bio_router_t;
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module identification */
#define LANGUAGE_TEMPORAL_MODULE_NAME      "language_temporal_bridge"
#define LANGUAGE_TEMPORAL_MODULE_VERSION   "1.0.0"

/** Bio-async module ID for this bridge (uses range 0x0820-0x082F) */
#define LANGUAGE_TEMPORAL_BIO_MODULE_ID    0x0820

/** Default configuration values */
#define LANGUAGE_TEMPORAL_DEFAULT_UPDATE_INTERVAL_MS    20
#define LANGUAGE_TEMPORAL_DEFAULT_SEMANTIC_CACHE_SIZE   128
#define LANGUAGE_TEMPORAL_DEFAULT_PRIMING_STRENGTH      0.5f
#define LANGUAGE_TEMPORAL_DEFAULT_PRIMING_DECAY         0.9f
#define LANGUAGE_TEMPORAL_DEFAULT_SPREADING_DEPTH       2
#define LANGUAGE_TEMPORAL_DEFAULT_SPEECH_THRESHOLD      0.6f

/** Semantic activation */
#define LANGUAGE_TEMPORAL_MAX_ACTIVE_CONCEPTS           64
#define LANGUAGE_TEMPORAL_ACTIVATION_THRESHOLD          0.15f
#define LANGUAGE_TEMPORAL_MIN_CONFIDENCE                0.3f

/** Auditory speech routing */
#define LANGUAGE_TEMPORAL_SPEECH_BUFFER_SIZE            32
#define LANGUAGE_TEMPORAL_PHONEME_BUFFER_SIZE           64

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Bridge operating modes
 */
typedef enum {
    LT_MODE_BIDIRECTIONAL = 0,    /**< Full bidirectional integration */
    LT_MODE_COMPREHENSION_ONLY,   /**< Only Temporal → Language flow */
    LT_MODE_PRODUCTION_ONLY,      /**< Only Language → Temporal flow */
    LT_MODE_PASSIVE,              /**< Monitoring only, no active transfer */
    LT_MODE_COUNT
} lt_bridge_mode_t;

/**
 * @brief Semantic query types
 */
typedef enum {
    LT_QUERY_WORD_MEANING = 0,    /**< Get meaning for word form */
    LT_QUERY_CONCEPT_WORD,        /**< Get word form for concept */
    LT_QUERY_RELATED_CONCEPTS,    /**< Get semantically related concepts */
    LT_QUERY_PRIMING,             /**< Apply semantic priming */
    LT_QUERY_DISAMBIGUATION,      /**< Help disambiguate word sense */
    LT_QUERY_COUNT
} lt_query_type_t;

/**
 * @brief Speech event types from auditory processing
 */
typedef enum {
    LT_SPEECH_ONSET = 0,          /**< Speech onset detected */
    LT_SPEECH_OFFSET,             /**< Speech offset detected */
    LT_SPEECH_PHONEME,            /**< Phoneme candidate identified */
    LT_SPEECH_WORD_BOUNDARY,      /**< Word boundary detected */
    LT_SPEECH_PROSODY,            /**< Prosodic feature detected */
    LT_SPEECH_COUNT
} lt_speech_event_t;

/**
 * @brief Bridge state
 */
typedef enum {
    LT_STATE_IDLE = 0,            /**< No active processing */
    LT_STATE_LISTENING,           /**< Monitoring auditory stream */
    LT_STATE_SPEECH_ACTIVE,       /**< Speech detected, routing to Wernicke */
    LT_STATE_SEMANTIC_QUERY,      /**< Processing semantic query */
    LT_STATE_CONCEPT_TRANSFER,    /**< Transferring concept activations */
    LT_STATE_ERROR,               /**< Error state */
    LT_STATE_COUNT
} lt_bridge_state_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Configuration for Language-Temporal bridge
 */
typedef struct {
    /* Operating mode */
    lt_bridge_mode_t mode;                    /**< Bridge operating mode */
    bool enable_speech_routing;               /**< Route detected speech to Wernicke */
    bool enable_semantic_queries;             /**< Allow semantic memory queries */
    bool enable_priming;                      /**< Enable semantic priming effects */
    bool enable_spreading_activation;         /**< Enable spreading activation */

    /* Timing */
    uint32_t update_interval_ms;              /**< Update cycle interval */

    /* Semantic memory */
    uint32_t semantic_cache_size;             /**< Size of concept cache */
    float priming_strength;                   /**< Base priming strength [0-1] */
    float priming_decay;                      /**< Priming decay rate per cycle */
    uint32_t spreading_depth;                 /**< Max spreading activation depth */
    float activation_threshold;               /**< Min activation to transfer */

    /* Speech routing */
    float speech_detection_threshold;         /**< Threshold for speech detection */
    uint32_t speech_buffer_size;              /**< Speech event buffer size */
    uint32_t phoneme_buffer_size;             /**< Phoneme candidate buffer */

    /* Bio-async */
    bool enable_bio_async;                    /**< Enable bio-async messaging */
} language_temporal_config_t;

/**
 * @brief Semantic concept activation from temporal lobe
 *
 * Represents an activated concept to be transferred to language processing
 */
typedef struct {
    uint32_t concept_id;                      /**< Temporal lobe concept ID */
    char name[64];                            /**< Concept name */
    float activation;                         /**< Current activation [0-1] */
    float* embedding;                         /**< Semantic embedding vector */
    uint32_t embedding_dim;                   /**< Embedding dimension */
    uint8_t modality;                         /**< Primary modality */
    bool is_primed;                           /**< Was this primed? */
    uint64_t activation_time_ms;              /**< When activated */
} lt_concept_activation_t;

/**
 * @brief Speech event from auditory processing
 *
 * Represents speech-related events to route to Wernicke
 */
typedef struct {
    lt_speech_event_t type;                   /**< Event type */
    float confidence;                         /**< Detection confidence [0-1] */
    double timestamp_ms;                      /**< Event timestamp */

    /* Phoneme data (for LT_SPEECH_PHONEME) */
    uint8_t phoneme_id;                       /**< Candidate phoneme ID */
    float phoneme_confidence;                 /**< Phoneme confidence */
    float duration_ms;                        /**< Phoneme duration */

    /* Prosodic data (for LT_SPEECH_PROSODY) */
    float pitch_hz;                           /**< Fundamental frequency */
    float intensity;                          /**< Intensity/loudness */
    float pitch_contour;                      /**< Pitch movement direction */

    /* Spectral features */
    float* spectral_features;                 /**< Optional spectral data */
    uint32_t num_spectral;                    /**< Number of spectral features */
} lt_speech_event_data_t;

/**
 * @brief Semantic query from language to temporal
 *
 * Request for semantic information from language processing
 */
typedef struct {
    lt_query_type_t type;                     /**< Query type */

    /* Word-based query */
    char word[64];                            /**< Word form to query */
    uint32_t word_id;                         /**< Word ID if known */

    /* Concept-based query */
    uint32_t concept_id;                      /**< Concept ID for lookup */
    float* query_embedding;                   /**< Query embedding vector */
    uint32_t embedding_dim;                   /**< Embedding dimension */

    /* Query parameters */
    uint32_t max_results;                     /**< Maximum results to return */
    float min_similarity;                     /**< Minimum similarity threshold */
    uint32_t context_concept_ids[8];          /**< Context concepts for disambiguation */
    uint32_t num_context;                     /**< Number of context concepts */

    /* Response callback */
    void (*callback)(const lt_concept_activation_t* results,
                     uint32_t count, void* user_data);
    void* user_data;                          /**< User data for callback */
} lt_semantic_query_t;

/**
 * @brief Word activation from language to temporal
 *
 * Notification of word recognition/production for semantic updating
 */
typedef struct {
    uint32_t word_id;                         /**< Word ID in lexicon */
    char word[64];                            /**< Word form */
    float activation;                         /**< Activation strength [0-1] */
    bool is_comprehension;                    /**< true=comprehended, false=produced */
    uint32_t sense_id;                        /**< Disambiguated sense ID */
    uint64_t timestamp_ms;                    /**< Activation timestamp */
} lt_word_activation_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Counts */
    uint64_t speech_events_routed;            /**< Speech events sent to Wernicke */
    uint64_t semantic_queries_processed;      /**< Semantic queries handled */
    uint64_t concepts_transferred;            /**< Concepts transferred to language */
    uint64_t word_activations_received;       /**< Word activations from language */
    uint64_t priming_requests;                /**< Priming requests processed */

    /* Timing */
    float avg_query_latency_ms;               /**< Average query response time */
    float avg_speech_routing_latency_ms;      /**< Average speech routing time */
    float max_latency_ms;                     /**< Maximum observed latency */

    /* Quality */
    float avg_concept_activation;             /**< Average concept activation */
    float avg_speech_confidence;              /**< Average speech detection confidence */
    uint64_t failed_queries;                  /**< Failed semantic queries */
    uint64_t cache_hits;                      /**< Concept cache hits */
    uint64_t cache_misses;                    /**< Concept cache misses */

    /* Current state */
    uint32_t active_concepts;                 /**< Currently active concepts */
    uint32_t pending_queries;                 /**< Queries awaiting response */
    lt_bridge_state_t current_state;          /**< Current bridge state */
} language_temporal_stats_t;

//=============================================================================
// Callback Types
//=============================================================================

/**
 * @brief Callback for speech events routed to Wernicke
 */
typedef void (*lt_speech_callback_t)(
    const lt_speech_event_data_t* event,
    void* user_data
);

/**
 * @brief Callback for concept activations from temporal
 */
typedef void (*lt_concept_callback_t)(
    const lt_concept_activation_t* concepts,
    uint32_t count,
    void* user_data
);

/**
 * @brief Callback for semantic query responses
 */
typedef void (*lt_query_response_callback_t)(
    const lt_semantic_query_t* query,
    const lt_concept_activation_t* results,
    uint32_t count,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default configuration
 *
 * @return Default configuration with sensible values
 */
language_temporal_config_t language_temporal_default_config(void);

/**
 * @brief Create language-temporal bridge
 *
 * @param language Language orchestrator (required)
 * @param temporal Temporal lobe adapter (required)
 * @param config Configuration (NULL for defaults)
 * @return New bridge instance, or NULL on failure
 */
language_temporal_bridge_t* language_temporal_bridge_create(
    language_orchestrator_t* language,
    temporal_adapter_t* temporal,
    const language_temporal_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
void language_temporal_bridge_destroy(language_temporal_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_temporal_bridge_reset(language_temporal_bridge_t* bridge);

//=============================================================================
// Connection Functions
//=============================================================================

/**
 * @brief Connect Wernicke adapter for speech routing
 *
 * @param bridge Bridge instance
 * @param wernicke Wernicke adapter
 * @return 0 on success, -1 on error
 */
int language_temporal_connect_wernicke(
    language_temporal_bridge_t* bridge,
    wernicke_adapter_t* wernicke
);

/**
 * @brief Connect Broca adapter for production queries
 *
 * @param bridge Bridge instance
 * @param broca Broca adapter
 * @return 0 on success, -1 on error
 */
int language_temporal_connect_broca(
    language_temporal_bridge_t* bridge,
    broca_adapter_t* broca
);

/**
 * @brief Connect bio-async router
 *
 * @param bridge Bridge instance
 * @param router Bio-async router
 * @return 0 on success, -1 on error
 */
int language_temporal_connect_bio_async(
    language_temporal_bridge_t* bridge,
    bio_router_t router
);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Main update cycle
 *
 * WHAT: Process pending events, transfer activations, handle queries
 * WHY:  Maintain synchronized state between language and temporal systems
 * HOW:  Check auditory for speech, transfer concept activations, process queries
 *
 * @param bridge Bridge instance
 * @param timestamp_ms Current timestamp
 * @return Number of events processed, or -1 on error
 */
int language_temporal_bridge_update(
    language_temporal_bridge_t* bridge,
    uint64_t timestamp_ms
);

/**
 * @brief Process auditory input for speech detection
 *
 * WHAT: Analyze auditory stream for speech content
 * WHY:  Route speech to Wernicke for comprehension
 * HOW:  Use temporal's speech detection, create speech events
 *
 * @param bridge Bridge instance
 * @param auditory_result Latest auditory processing result
 * @return Number of speech events generated, or -1 on error
 */
int language_temporal_process_auditory(
    language_temporal_bridge_t* bridge,
    const temporal_auditory_result_t* auditory_result
);

//=============================================================================
// Semantic Query Functions (Language → Temporal)
//=============================================================================

/**
 * @brief Query semantic memory for word meaning
 *
 * WHAT: Look up concept(s) associated with a word form
 * WHY:  Support lexical-semantic access during comprehension
 * HOW:  Query temporal's semantic memory, return activated concepts
 *
 * @param bridge Bridge instance
 * @param word Word form to look up
 * @param results Output buffer for concept activations
 * @param max_results Maximum results to return
 * @return Number of concepts found, or -1 on error
 */
int language_temporal_query_word_meaning(
    language_temporal_bridge_t* bridge,
    const char* word,
    lt_concept_activation_t* results,
    uint32_t max_results
);

/**
 * @brief Query semantic memory for word form
 *
 * WHAT: Look up word form(s) for a concept
 * WHY:  Support lexical access during production
 * HOW:  Reverse lookup from concept to word forms
 *
 * @param bridge Bridge instance
 * @param concept_id Concept to look up
 * @param words Output buffer for word forms
 * @param max_words Maximum words to return
 * @return Number of words found, or -1 on error
 */
int language_temporal_query_concept_word(
    language_temporal_bridge_t* bridge,
    uint32_t concept_id,
    char words[][64],
    uint32_t max_words
);

/**
 * @brief Get semantically related concepts
 *
 * WHAT: Retrieve concepts related to a given concept
 * WHY:  Support semantic spreading during comprehension/production
 * HOW:  Spreading activation in temporal semantic memory
 *
 * @param bridge Bridge instance
 * @param concept_id Source concept
 * @param results Output buffer for related concepts
 * @param max_results Maximum results
 * @param depth Spreading depth (0 = immediate neighbors only)
 * @return Number of related concepts, or -1 on error
 */
int language_temporal_get_related_concepts(
    language_temporal_bridge_t* bridge,
    uint32_t concept_id,
    lt_concept_activation_t* results,
    uint32_t max_results,
    uint32_t depth
);

/**
 * @brief Request semantic priming
 *
 * WHAT: Pre-activate concepts related to a word/concept
 * WHY:  Speed up subsequent semantic access
 * HOW:  Apply priming through temporal's priming mechanism
 *
 * @param bridge Bridge instance
 * @param concept_id Concept to prime from
 * @param strength Priming strength [0-1]
 * @return 0 on success, -1 on error
 */
int language_temporal_apply_priming(
    language_temporal_bridge_t* bridge,
    uint32_t concept_id,
    float strength
);

/**
 * @brief Submit async semantic query
 *
 * WHAT: Asynchronous semantic query with callback
 * WHY:  Non-blocking semantic access
 * HOW:  Queue query, invoke callback when ready
 *
 * @param bridge Bridge instance
 * @param query Query specification
 * @return Query ID for tracking, or 0 on error
 */
uint32_t language_temporal_query_async(
    language_temporal_bridge_t* bridge,
    const lt_semantic_query_t* query
);

//=============================================================================
// Word Activation Functions (Language → Temporal)
//=============================================================================

/**
 * @brief Notify temporal of word activation
 *
 * WHAT: Inform temporal that a word was recognized/produced
 * WHY:  Update semantic activations, enable priming
 * HOW:  Activate corresponding concept in temporal memory
 *
 * @param bridge Bridge instance
 * @param activation Word activation data
 * @return 0 on success, -1 on error
 */
int language_temporal_word_activated(
    language_temporal_bridge_t* bridge,
    const lt_word_activation_t* activation
);

/**
 * @brief Batch word activation notification
 *
 * WHAT: Notify temporal of multiple word activations
 * WHY:  Efficient batch update for phrase/sentence processing
 * HOW:  Batch activate concepts in temporal memory
 *
 * @param bridge Bridge instance
 * @param activations Array of word activations
 * @param count Number of activations
 * @return Number processed, or -1 on error
 */
int language_temporal_words_activated_batch(
    language_temporal_bridge_t* bridge,
    const lt_word_activation_t* activations,
    uint32_t count
);

//=============================================================================
// Concept Transfer Functions (Temporal → Language)
//=============================================================================

/**
 * @brief Get currently active concepts for language
 *
 * WHAT: Retrieve concepts currently activated in temporal
 * WHY:  Provide semantic context for language processing
 * HOW:  Copy active concepts above threshold
 *
 * @param bridge Bridge instance
 * @param concepts Output buffer for active concepts
 * @param max_concepts Maximum concepts to return
 * @return Number of active concepts, or -1 on error
 */
int language_temporal_get_active_concepts(
    language_temporal_bridge_t* bridge,
    lt_concept_activation_t* concepts,
    uint32_t max_concepts
);

/**
 * @brief Transfer concept embedding to Wernicke
 *
 * WHAT: Send concept embedding for semantic integration
 * WHY:  Provide semantic vector for comprehension
 * HOW:  Copy embedding, notify Wernicke
 *
 * @param bridge Bridge instance
 * @param concept_id Concept to transfer
 * @return 0 on success, -1 on error
 */
int language_temporal_transfer_concept_embedding(
    language_temporal_bridge_t* bridge,
    uint32_t concept_id
);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Set speech event callback
 *
 * @param bridge Bridge instance
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, -1 on error
 */
int language_temporal_set_speech_callback(
    language_temporal_bridge_t* bridge,
    lt_speech_callback_t callback,
    void* user_data
);

/**
 * @brief Set concept activation callback
 *
 * @param bridge Bridge instance
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, -1 on error
 */
int language_temporal_set_concept_callback(
    language_temporal_bridge_t* bridge,
    lt_concept_callback_t callback,
    void* user_data
);

//=============================================================================
// Status and Statistics
//=============================================================================

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge instance
 * @return Current state
 */
lt_bridge_state_t language_temporal_get_state(
    const language_temporal_bridge_t* bridge
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int language_temporal_get_stats(
    const language_temporal_bridge_t* bridge,
    language_temporal_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge instance
 */
void language_temporal_reset_stats(language_temporal_bridge_t* bridge);

/**
 * @brief Get configuration
 *
 * @param bridge Bridge instance
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int language_temporal_get_config(
    const language_temporal_bridge_t* bridge,
    language_temporal_config_t* config
);

/**
 * @brief Update configuration
 *
 * @param bridge Bridge instance
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int language_temporal_set_config(
    language_temporal_bridge_t* bridge,
    const language_temporal_config_t* config
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Process pending bio-async messages
 *
 * @param bridge Bridge instance
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number processed, or -1 on error
 */
int language_temporal_process_bio_messages(
    language_temporal_bridge_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Get bio-async module context
 *
 * @param bridge Bridge instance
 * @return Module context, or NULL if not enabled
 */
void* language_temporal_get_bio_context(
    language_temporal_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LANGUAGE_TEMPORAL_BRIDGE_H */
