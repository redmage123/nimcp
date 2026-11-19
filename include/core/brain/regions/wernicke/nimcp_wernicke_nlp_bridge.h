/**
 * @file nimcp_wernicke_nlp_bridge.h
 * @brief Wernicke's Area - Comprehensive NLP Integration Bridge
 * @version 1.0.0
 * @date 2026-01-05
 *
 * WHAT: Unified integration bridge connecting Wernicke's area to all NLP modules
 * WHY:  Language comprehension requires coordination with speech perception,
 *       semantic memory, NLP processing, and multimodal integration
 * HOW:  Bridge pattern connecting Wernicke adapter to speech cortex, NLP network,
 *       semantic memory, multimodal bridge, and knowledge graph
 *
 * INTEGRATION ARCHITECTURE:
 * ========================
 *
 *                    ┌─────────────────────────────────────────────────┐
 *                    │           WERNICKE NLP BRIDGE                    │
 *                    │      (Comprehensive NLP Integration)             │
 *                    └─────────────────────────────────────────────────┘
 *                                         │
 *         ┌───────────────┬───────────────┼───────────────┬───────────────┐
 *         ▼               ▼               ▼               ▼               ▼
 *    ┌─────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐
 *    │ Speech  │    │   NLP    │    │ Semantic │    │Multimodal│    │Knowledge │
 *    │ Cortex  │    │ Network  │    │  Memory  │    │NLP Bridge│    │  Graph   │
 *    └─────────┘    └──────────┘    └──────────┘    └──────────┘    └──────────┘
 *         │               │               │               │               │
 *    Phonemes      Token/Embed     Concepts      Audio+Visual     Self-Aware
 *    Prosody       Attention       Spreading     Fusion           KG Access
 *    Features      Neuromod        Activation    Cross-modal      Concept Link
 *
 * DATA FLOW:
 * ==========
 * 1. Speech Cortex → Wernicke: Phoneme sequences, prosodic features
 * 2. Wernicke → NLP Network: Tokens for attention-based processing
 * 3. Wernicke ↔ Semantic Memory: Concept activation, spreading, retrieval
 * 4. Multimodal → Wernicke: Cross-modal (audio+visual) features
 * 5. Wernicke → Knowledge Graph: Self-awareness, concept registration
 *
 * BIOLOGICAL BASIS:
 * =================
 * - Wernicke's area (BA22) is the hub for language comprehension
 * - Receives input from primary auditory cortex (A1) via speech cortex
 * - Bidirectional connections with semantic memory (anterior temporal lobe)
 * - Integration with visual language areas (angular gyrus) for reading
 * - Arcuate fasciculus connection to Broca's area for production
 *
 * @version Phase W7: Wernicke NLP Integration
 * @author NIMCP Development Team
 */

#ifndef NIMCP_WERNICKE_NLP_BRIDGE_H
#define NIMCP_WERNICKE_NLP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Include required type definitions */
#include "nlp/nimcp_nlp.h"                       /* nlp_network_t */
#include "cognitive/memory/nimcp_semantic_memory.h"  /* semantic_memory_system_t */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/* Wernicke's area */
struct wernicke_adapter;
typedef struct wernicke_adapter wernicke_adapter_t;

/* Speech perception */
struct speech_cortex;
typedef struct speech_cortex speech_cortex_t;

/* Multimodal NLP bridge */
struct multimodal_nlp_bridge;
typedef struct multimodal_nlp_bridge multimodal_nlp_bridge_t;

/* Knowledge graph */
struct brain_kg;
typedef struct brain_kg brain_kg_t;

/* Working memory */
struct working_memory;
typedef struct working_memory working_memory_t;

/* Wernicke NLP bridge */
struct wernicke_nlp_bridge;
typedef struct wernicke_nlp_bridge wernicke_nlp_bridge_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Bio-async module ID for Wernicke NLP bridge */
#define BIO_MODULE_WERNICKE_NLP_BRIDGE    0x0E5C

/** @brief Maximum phoneme sequence length */
#define WERNICKE_NLP_MAX_PHONEMES         512

/** @brief Maximum token sequence length */
#define WERNICKE_NLP_MAX_TOKENS           256

/** @brief Maximum active concepts */
#define WERNICKE_NLP_MAX_CONCEPTS         128

/** @brief Default embedding dimension */
#define WERNICKE_NLP_EMBEDDING_DIM        256

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief NLP processing mode
 */
typedef enum {
    WERNICKE_NLP_MODE_COMPREHENSION,    /**< Standard comprehension */
    WERNICKE_NLP_MODE_REPETITION,       /**< Direct repetition (bypass semantics) */
    WERNICKE_NLP_MODE_DICTATION,        /**< Speech-to-text mode */
    WERNICKE_NLP_MODE_TRANSLATION,      /**< Cross-language processing */
    WERNICKE_NLP_MODE_INFERENCE         /**< Deep inference mode */
} wernicke_nlp_mode_t;

/**
 * @brief Integration state
 */
typedef enum {
    WERNICKE_NLP_STATE_IDLE,            /**< No active processing */
    WERNICKE_NLP_STATE_RECEIVING,       /**< Receiving phonemes */
    WERNICKE_NLP_STATE_LEXICAL,         /**< Lexical access phase */
    WERNICKE_NLP_STATE_SEMANTIC,        /**< Semantic integration */
    WERNICKE_NLP_STATE_SYNTACTIC,       /**< Syntactic parsing */
    WERNICKE_NLP_STATE_COMPLETE,        /**< Processing complete */
    WERNICKE_NLP_STATE_ERROR            /**< Error state */
} wernicke_nlp_state_t;

/**
 * @brief Cross-modal integration mode
 */
typedef enum {
    WERNICKE_CROSSMODAL_AUDIO_ONLY,     /**< Audio-only processing */
    WERNICKE_CROSSMODAL_VISUAL_ONLY,    /**< Visual-only (reading) */
    WERNICKE_CROSSMODAL_AUDIOVISUAL,    /**< Combined audiovisual */
    WERNICKE_CROSSMODAL_TACTILE         /**< Tactile (Braille) */
} wernicke_crossmodal_mode_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Phoneme input from speech cortex
 */
typedef struct {
    uint32_t phoneme_id;                /**< Phoneme identifier */
    float confidence;                   /**< Recognition confidence [0-1] */
    float duration_ms;                  /**< Phoneme duration */
    float formants[4];                  /**< Formant frequencies (F1-F4) */
    float pitch;                        /**< Fundamental frequency (F0) */
    float intensity;                    /**< Amplitude/loudness */
    bool is_stressed;                   /**< Prosodic stress marker */
    bool is_boundary;                   /**< Word/phrase boundary */
} wernicke_phoneme_input_t;

/**
 * @brief Token output for NLP processing
 */
typedef struct {
    uint32_t token_id;                  /**< Token identifier */
    float* embedding;                   /**< Token embedding vector */
    uint32_t embedding_dim;             /**< Embedding dimension */
    float attention_weight;             /**< Attention weight */
    uint32_t position;                  /**< Position in sequence */
} wernicke_token_output_t;

/**
 * @brief Semantic concept activation
 */
typedef struct {
    uint32_t concept_id;                /**< Concept identifier */
    char concept_name[64];              /**< Human-readable name */
    float activation;                   /**< Activation level [0-1] */
    float relevance;                    /**< Context relevance [0-1] */
    uint32_t source;                    /**< Activation source */
    bool is_target;                     /**< Target of current utterance */
} wernicke_concept_activation_t;

/**
 * @brief Comprehension result
 */
typedef struct {
    /* Lexical results */
    uint32_t* word_ids;                 /**< Recognized word IDs */
    uint32_t word_count;                /**< Number of words */
    float lexical_confidence;           /**< Overall lexical confidence */

    /* Semantic results */
    wernicke_concept_activation_t* concepts;  /**< Activated concepts */
    uint32_t concept_count;             /**< Number of concepts */
    float semantic_coherence;           /**< Semantic coherence score */

    /* Syntactic results */
    uint32_t* constituent_ids;          /**< Syntactic constituents */
    uint32_t constituent_count;         /**< Number of constituents */
    float syntactic_wellformedness;     /**< Grammaticality score */

    /* Prosodic results */
    bool is_question;                   /**< Question intonation detected */
    bool is_command;                    /**< Command intonation detected */
    float emotional_valence;            /**< Emotional tone [-1, +1] */

    /* Performance */
    float processing_time_ms;           /**< Total processing time */
    float confidence;                   /**< Overall confidence */
} wernicke_comprehension_result_t;

/**
 * @brief Cross-modal input
 */
typedef struct {
    /* Audio features (from speech cortex) */
    float* audio_features;              /**< Audio feature vector */
    uint32_t audio_feature_dim;         /**< Audio feature dimension */

    /* Visual features (from visual cortex / reading) */
    float* visual_features;             /**< Visual feature vector */
    uint32_t visual_feature_dim;        /**< Visual feature dimension */

    /* Lip reading features */
    float* lip_features;                /**< Lip movement features */
    uint32_t lip_feature_dim;           /**< Lip feature dimension */

    /* Mode */
    wernicke_crossmodal_mode_t mode;    /**< Current modality mode */
    float audio_weight;                 /**< Audio contribution weight */
    float visual_weight;                /**< Visual contribution weight */
} wernicke_crossmodal_input_t;

/**
 * @brief Configuration
 */
typedef struct {
    /* Module enables */
    bool enable_speech_cortex;          /**< Enable speech cortex integration */
    bool enable_nlp_network;            /**< Enable NLP network integration */
    bool enable_semantic_memory;        /**< Enable semantic memory integration */
    bool enable_multimodal;             /**< Enable multimodal integration */
    bool enable_knowledge_graph;        /**< Enable KG integration */
    bool enable_working_memory;         /**< Enable WM integration */

    /* Processing parameters */
    wernicke_nlp_mode_t default_mode;   /**< Default processing mode */
    uint32_t max_sequence_length;       /**< Maximum sequence to process */
    uint32_t embedding_dim;             /**< Embedding dimension */
    float attention_dropout;            /**< Attention dropout rate */

    /* Semantic parameters */
    float spreading_activation_decay;   /**< Decay rate for spreading */
    uint32_t max_spreading_depth;       /**< Max depth for activation spread */
    float concept_threshold;            /**< Min activation for reporting */

    /* Cross-modal parameters */
    wernicke_crossmodal_mode_t crossmodal_mode;  /**< Cross-modal mode */
    float mcgurk_weight;                /**< McGurk effect strength */

    /* Bio-async */
    bool enable_bio_async;              /**< Enable bio-async messaging */
    uint32_t inbox_capacity;            /**< Message inbox capacity */

    /* Logging */
    bool enable_logging;                /**< Enable detailed logging */
} wernicke_nlp_config_t;

/**
 * @brief Statistics
 */
typedef struct {
    /* Processing counts */
    uint64_t phonemes_processed;        /**< Total phonemes processed */
    uint64_t words_recognized;          /**< Total words recognized */
    uint64_t sentences_parsed;          /**< Total sentences parsed */
    uint64_t concepts_activated;        /**< Total concepts activated */

    /* Integration stats */
    uint64_t speech_cortex_inputs;      /**< Inputs from speech cortex */
    uint64_t nlp_network_calls;         /**< Calls to NLP network */
    uint64_t semantic_queries;          /**< Semantic memory queries */
    uint64_t kg_registrations;          /**< KG concept registrations */
    uint64_t multimodal_fusions;        /**< Multimodal fusion events */

    /* Performance */
    float avg_lexical_time_ms;          /**< Average lexical access time */
    float avg_semantic_time_ms;         /**< Average semantic integration time */
    float avg_total_time_ms;            /**< Average total processing time */

    /* Accuracy */
    float avg_lexical_confidence;       /**< Average lexical confidence */
    float avg_semantic_coherence;       /**< Average semantic coherence */
    float avg_syntactic_score;          /**< Average syntactic score */
} wernicke_nlp_stats_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int wernicke_nlp_default_config(wernicke_nlp_config_t* config);

/**
 * @brief Create NLP bridge
 *
 * WHAT: Create comprehensive NLP integration bridge for Wernicke's area
 * WHY:  Coordinate all NLP-related processing for language comprehension
 * HOW:  Allocate bridge, connect to all NLP modules, register handlers
 *
 * @param wernicke Wernicke adapter (required)
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
wernicke_nlp_bridge_t* wernicke_nlp_bridge_create(
    wernicke_adapter_t* wernicke,
    const wernicke_nlp_config_t* config
);

/**
 * @brief Destroy NLP bridge
 *
 * @param bridge Bridge to destroy
 */
void wernicke_nlp_bridge_destroy(wernicke_nlp_bridge_t* bridge);

/* ============================================================================
 * Module Connection API
 * ============================================================================ */

/**
 * @brief Connect to speech cortex
 *
 * WHAT: Establish connection to speech cortex for phoneme input
 * WHY:  Wernicke needs phoneme sequences from speech perception
 * HOW:  Register as consumer of speech cortex output
 *
 * @param bridge Bridge handle
 * @param speech_cortex Speech cortex instance
 * @return 0 on success, -1 on error
 */
int wernicke_nlp_connect_speech_cortex(
    wernicke_nlp_bridge_t* bridge,
    speech_cortex_t* speech_cortex
);

/**
 * @brief Connect to NLP network
 *
 * WHAT: Establish connection to NLP network for token processing
 * WHY:  Enable attention-based language processing
 * HOW:  Register for NLP network embedding and attention
 *
 * @param bridge Bridge handle
 * @param nlp_network NLP network instance
 * @return 0 on success, -1 on error
 */
int wernicke_nlp_connect_nlp_network(
    wernicke_nlp_bridge_t* bridge,
    nlp_network_t nlp_network
);

/**
 * @brief Connect to semantic memory
 *
 * WHAT: Establish connection to semantic memory for concept access
 * WHY:  Language comprehension requires concept activation and retrieval
 * HOW:  Register for semantic memory queries and spreading activation
 *
 * @param bridge Bridge handle
 * @param semantic_memory Semantic memory instance
 * @return 0 on success, -1 on error
 */
int wernicke_nlp_connect_semantic_memory(
    wernicke_nlp_bridge_t* bridge,
    semantic_memory_system_t* semantic_memory
);

/**
 * @brief Connect to multimodal NLP bridge
 *
 * WHAT: Establish connection for cross-modal integration
 * WHY:  Language comprehension can involve audio, visual, or both
 * HOW:  Register for multimodal fusion events
 *
 * @param bridge Bridge handle
 * @param multimodal Multimodal NLP bridge instance
 * @return 0 on success, -1 on error
 */
int wernicke_nlp_connect_multimodal(
    wernicke_nlp_bridge_t* bridge,
    multimodal_nlp_bridge_t* multimodal
);

/**
 * @brief Connect to knowledge graph
 *
 * WHAT: Establish connection to brain knowledge graph
 * WHY:  Enable self-awareness and concept relationship access
 * HOW:  Register for KG queries and concept registration
 *
 * @param bridge Bridge handle
 * @param kg Knowledge graph instance
 * @return 0 on success, -1 on error
 */
int wernicke_nlp_connect_knowledge_graph(
    wernicke_nlp_bridge_t* bridge,
    brain_kg_t* kg
);

/**
 * @brief Connect to working memory
 *
 * WHAT: Establish connection to working memory for phonological loop
 * WHY:  Comprehension requires active maintenance of recent input
 * HOW:  Register for WM phonological buffer access
 *
 * @param bridge Bridge handle
 * @param wm Working memory instance
 * @return 0 on success, -1 on error
 */
int wernicke_nlp_connect_working_memory(
    wernicke_nlp_bridge_t* bridge,
    working_memory_t* wm
);

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable bio-async messaging for NLP bridge
 * WHY:  Asynchronous event-driven language processing
 * HOW:  Register module and message handlers
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int wernicke_nlp_connect_bio_async(wernicke_nlp_bridge_t* bridge);

/* ============================================================================
 * Processing API
 * ============================================================================ */

/**
 * @brief Process phoneme sequence from speech cortex
 *
 * WHAT: Process incoming phoneme sequence for comprehension
 * WHY:  Main entry point for spoken language comprehension
 * HOW:  Phonemes → Lexical → Semantic → Syntactic pipeline
 *
 * @param bridge Bridge handle
 * @param phonemes Phoneme input array
 * @param count Number of phonemes
 * @param result Output: comprehension result (caller allocates)
 * @return 0 on success, -1 on error
 */
int wernicke_nlp_process_phonemes(
    wernicke_nlp_bridge_t* bridge,
    const wernicke_phoneme_input_t* phonemes,
    uint32_t count,
    wernicke_comprehension_result_t* result
);

/**
 * @brief Process token sequence from NLP network
 *
 * WHAT: Process tokens that have been embedded by NLP network
 * WHY:  Enable attention-based deep language processing
 * HOW:  Apply semantic integration to token embeddings
 *
 * @param bridge Bridge handle
 * @param tokens Token array
 * @param count Number of tokens
 * @param result Output: comprehension result
 * @return 0 on success, -1 on error
 */
int wernicke_nlp_process_tokens(
    wernicke_nlp_bridge_t* bridge,
    const wernicke_token_output_t* tokens,
    uint32_t count,
    wernicke_comprehension_result_t* result
);

/**
 * @brief Process cross-modal input
 *
 * WHAT: Process audiovisual or visual-only input
 * WHY:  Support reading and audiovisual speech perception
 * HOW:  Fuse modalities according to McGurk effect weights
 *
 * @param bridge Bridge handle
 * @param input Cross-modal input
 * @param result Output: comprehension result
 * @return 0 on success, -1 on error
 */
int wernicke_nlp_process_crossmodal(
    wernicke_nlp_bridge_t* bridge,
    const wernicke_crossmodal_input_t* input,
    wernicke_comprehension_result_t* result
);

/**
 * @brief Activate semantic concept
 *
 * WHAT: Activate a concept in semantic memory and spread activation
 * WHY:  Prime related concepts for upcoming comprehension
 * HOW:  Activate concept, spread to neighbors with decay
 *
 * @param bridge Bridge handle
 * @param concept_id Concept to activate
 * @param activation Initial activation level
 * @return Number of concepts activated, or -1 on error
 */
int wernicke_nlp_activate_concept(
    wernicke_nlp_bridge_t* bridge,
    uint32_t concept_id,
    float activation
);

/**
 * @brief Query semantic memory
 *
 * WHAT: Query semantic memory for concept information
 * WHY:  Retrieve semantic features and relations
 * HOW:  Query semantic memory via bridge connection
 *
 * @param bridge Bridge handle
 * @param query Query string or pattern
 * @param results Output: matching concepts (caller allocates array)
 * @param max_results Maximum results to return
 * @return Number of results, or -1 on error
 */
int wernicke_nlp_query_semantic(
    wernicke_nlp_bridge_t* bridge,
    const char* query,
    wernicke_concept_activation_t* results,
    uint32_t max_results
);

/**
 * @brief Register concept in knowledge graph
 *
 * WHAT: Register a newly-learned concept in brain KG
 * WHY:  Enable self-awareness of language knowledge
 * HOW:  Create KG node with concept properties
 *
 * @param bridge Bridge handle
 * @param concept_name Concept name
 * @param properties Concept properties (JSON-like string)
 * @return Concept ID, or 0 on error
 */
uint32_t wernicke_nlp_register_concept(
    wernicke_nlp_bridge_t* bridge,
    const char* concept_name,
    const char* properties
);

/* ============================================================================
 * State and Statistics API
 * ============================================================================ */

/**
 * @brief Update bridge state
 *
 * @param bridge Bridge handle
 * @param current_time_ms Current timestamp
 * @return 0 on success, -1 on error
 */
int wernicke_nlp_bridge_update(
    wernicke_nlp_bridge_t* bridge,
    uint64_t current_time_ms
);

/**
 * @brief Get current state
 *
 * @param bridge Bridge handle
 * @return Current state
 */
wernicke_nlp_state_t wernicke_nlp_get_state(
    const wernicke_nlp_bridge_t* bridge
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge handle
 * @param stats Output: statistics
 * @return 0 on success, -1 on error
 */
int wernicke_nlp_get_stats(
    const wernicke_nlp_bridge_t* bridge,
    wernicke_nlp_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void wernicke_nlp_reset_stats(wernicke_nlp_bridge_t* bridge);

/**
 * @brief Free comprehension result resources
 *
 * @param result Result to free (does not free result struct itself)
 */
void wernicke_nlp_free_result(wernicke_comprehension_result_t* result);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* wernicke_nlp_mode_to_string(wernicke_nlp_mode_t mode);
const char* wernicke_nlp_state_to_string(wernicke_nlp_state_t state);
const char* wernicke_crossmodal_mode_to_string(wernicke_crossmodal_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WERNICKE_NLP_BRIDGE_H */
