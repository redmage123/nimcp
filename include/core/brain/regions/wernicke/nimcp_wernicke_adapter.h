/**
 * @file nimcp_wernicke_adapter.h
 * @brief Brain adapter for Wernicke's area language comprehension
 *
 * WHAT: Unified adapter for language comprehension in Wernicke's area (posterior STG/BA 22)
 * WHY:  Enable speech comprehension, semantic integration, and lexical access in NIMCP
 * HOW:  Orchestrates phonological, lexical, semantic, and syntactic processing layers
 *
 * ARCHITECTURE:
 * - Four processing layers: Phonological → Lexical → Semantic → Syntactic
 * - Bidirectional connection to Broca's area (arcuate fasciculus)
 * - Integration with semantic memory for concept retrieval
 * - Cross-modal fusion for audiovisual speech perception
 * - Knowledge graph registration for self-awareness
 *
 * BIOLOGICAL BASIS:
 * - Models posterior Superior Temporal Gyrus (pSTG), Brodmann area 22
 * - Phonological analysis: Phoneme categorization from acoustic features
 * - Lexical access: Word recognition from phoneme sequences
 * - Semantic integration: Meaning retrieval and disambiguation
 * - Syntactic comprehension: Phrase structure and thematic role assignment
 *
 * WERNICKE'S APHASIA (Damage):
 * - Fluent but meaningless speech (word salad)
 * - Poor speech comprehension
 * - Neologisms and paraphasias
 * - Impaired repetition
 *
 * CONNECTIONS:
 * - Receives from: Audio cortex (A1/A2), Speech cortex, Audiovisual bridge
 * - Sends to: Broca's area (arcuate fasciculus), Semantic memory, Knowledge graph
 * - Bidirectional: Working memory (phonological loop)
 *
 * @version Phase W1: Wernicke's Area Core Integration
 * @date 2026-01-04
 */

#ifndef NIMCP_WERNICKE_ADAPTER_H
#define NIMCP_WERNICKE_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Bio-async communication system */
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

/* Logging system */
#include "utils/logging/nimcp_logging.h"

/* Unified memory system */
#include "utils/memory/nimcp_unified_memory.h"

/* Phoneme types from speech cortex */
#include "perception/nimcp_speech_cortex.h"

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

/* Sub-module forward declarations */
typedef struct phonological_analyzer phonological_analyzer_t;
typedef struct lexical_access lexical_access_t;
typedef struct semantic_integrator semantic_integrator_t;
typedef struct syntactic_comprehension syntactic_comprehension_t;

/* External module forward declarations.
 * Note on semantic_memory_t: the canonical typedef lives in
 * cognitive/parietal/nimcp_intuition_integrations.h as
 * `typedef struct semantic_memory_system semantic_memory_t;`. The local
 * forward decl MUST use the same struct tag or any TU that includes both
 * sees a conflicting-types error. */
typedef struct broca_adapter broca_adapter_t;
typedef struct speech_cortex speech_cortex_t;
typedef struct semantic_memory_system semantic_memory_t;
typedef struct brain_kg brain_kg_t;

/* Forward declaration for opaque adapter type */
typedef struct wernicke_adapter wernicke_adapter_t;

/* Forward decl for SNN substrate binding (kept opaque so adapter callers
 * don't need to pull in the full snn_network header). Struct tag matches
 * the canonical typedef in snn/nimcp_snn_types.h. */
#ifndef NIMCP_SNN_NETWORK_T_FWD
#define NIMCP_SNN_NETWORK_T_FWD
typedef struct snn_network_s snn_network_t;
#endif

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Default configuration values
 */
#define WERNICKE_DEFAULT_MAX_PHONEMES        256
#define WERNICKE_DEFAULT_MAX_WORDS           128
#define WERNICKE_DEFAULT_MAX_CONCEPTS        1024
#define WERNICKE_DEFAULT_LEXICON_SIZE        10000
#define WERNICKE_DEFAULT_WORKING_MEMORY_SLOTS 9
#define WERNICKE_DEFAULT_EMBEDDING_DIM       128
#define WERNICKE_DEFAULT_PROCESSING_WINDOW_MS 100.0f
#define WERNICKE_DEFAULT_FORMANT_COUNT       4

/**
 * @brief Wernicke's area adapter configuration
 */
typedef struct {
    /* Capacity limits */
    uint32_t max_phonemes;               /**< Maximum phonemes in buffer */
    uint32_t max_words;                  /**< Maximum words in utterance */
    uint32_t max_concepts;               /**< Maximum active concepts */

    /* Lexicon configuration */
    uint32_t lexicon_size;               /**< Vocabulary size */
    bool enable_lexicon;                 /**< Enable built-in lexicon */

    /* Working memory integration */
    uint32_t working_memory_slots;       /**< Phonological loop capacity (7+/-2) */
    bool enable_working_memory;          /**< Enable phonological working memory */

    /* Processing layers */
    bool enable_phonological;            /**< Enable phoneme analysis */
    bool enable_lexical;                 /**< Enable word recognition */
    bool enable_semantic;                /**< Enable meaning integration */
    bool enable_syntactic;               /**< Enable sentence parsing */

    /* Feature configuration */
    uint32_t embedding_dim;              /**< Embedding vector dimension */
    uint32_t formant_count;              /**< Number of formants to track (F1-F4) */

    /* Cross-modal integration */
    bool enable_audiovisual;             /**< Enable lip-reading fusion (McGurk) */
    bool enable_prosody;                 /**< Enable prosodic analysis */

    /* External connections */
    bool enable_broca_connection;        /**< Enable arcuate fasciculus to Broca */
    bool enable_semantic_memory;         /**< Enable semantic memory integration */
    bool enable_kg_registration;         /**< Enable knowledge graph registration */

    /* Event system */
    bool enable_events;                  /**< Enable event bus integration */

    /* Training */
    bool enable_training;                /**< Enable learning capabilities */
    float learning_rate;                 /**< Base learning rate */

    /* Timing */
    float processing_window_ms;          /**< Processing window duration */

    /* Bio-async communication */
    bool enable_bio_async;               /**< Enable bio-async messaging */
    nimcp_bio_channel_type_t default_channel; /**< Default neuromodulator channel */
} wernicke_config_t;

/*=============================================================================
 * STATUS AND STATE
 *===========================================================================*/

/**
 * @brief Processing status of the adapter
 */
typedef enum {
    WERNICKE_STATUS_IDLE = 0,            /**< Ready for input */
    WERNICKE_STATUS_PHONOLOGICAL,        /**< Analyzing phonemes */
    WERNICKE_STATUS_LEXICAL_ACCESS,      /**< Looking up words */
    WERNICKE_STATUS_SEMANTIC,            /**< Integrating meaning */
    WERNICKE_STATUS_SYNTACTIC,           /**< Parsing sentence structure */
    WERNICKE_STATUS_COMPREHENSION_READY, /**< Comprehension complete */
    WERNICKE_STATUS_ERROR                /**< Error state */
} wernicke_status_t;

/**
 * @brief Error codes for Wernicke's area operations
 */
typedef enum {
    WERNICKE_ERROR_NONE = 0,
    WERNICKE_ERROR_INVALID_INPUT,
    WERNICKE_ERROR_PHONOLOGICAL_FAILURE,
    WERNICKE_ERROR_LEXICAL_FAILURE,
    WERNICKE_ERROR_SEMANTIC_FAILURE,
    WERNICKE_ERROR_SYNTACTIC_FAILURE,
    WERNICKE_ERROR_WORKING_MEMORY_FULL,
    WERNICKE_ERROR_WORD_NOT_FOUND,
    WERNICKE_ERROR_CONCEPT_NOT_FOUND,
    WERNICKE_ERROR_BUFFER_OVERFLOW,
    WERNICKE_ERROR_BROCA_DISCONNECTED,
    WERNICKE_ERROR_INTERNAL
} wernicke_error_t;

/*=============================================================================
 * INPUT/OUTPUT STRUCTURES
 *===========================================================================*/

/**
 * @brief Word entry in Wernicke's lexicon
 */
typedef struct {
    uint32_t word_id;                    /**< Unique word identifier */
    char word[64];                       /**< Word string */
    uint8_t phonemes[32];                /**< Phoneme sequence */
    uint32_t phoneme_count;              /**< Number of phonemes */
    float frequency;                     /**< Word frequency (0-1) */
    uint32_t concept_id;                 /**< Associated concept ID */
    uint8_t pos;                         /**< Part of speech */
} wernicke_word_t;

/**
 * @brief Recognized word result
 */
typedef struct {
    wernicke_word_t word;                /**< Word entry */
    float confidence;                    /**< Recognition confidence [0,1] */
    uint64_t onset_time_ms;              /**< Word onset time */
    uint64_t offset_time_ms;             /**< Word offset time */
    uint32_t position_in_utterance;      /**< Position in sentence */
} wernicke_word_result_t;

/**
 * @brief Semantic concept activation
 */
typedef struct {
    uint32_t concept_id;                 /**< Concept identifier */
    char concept_name[64];               /**< Concept label */
    float activation;                    /**< Activation level [0,1] */
    float* embedding;                    /**< Concept embedding vector */
    uint32_t embedding_dim;              /**< Embedding dimension */
    uint32_t* related_concepts;          /**< Related concept IDs */
    uint32_t num_related;                /**< Number of related concepts */
} wernicke_concept_t;

/**
 * @brief Comprehension context for disambiguation
 */
typedef struct {
    wernicke_word_result_t* prior_words; /**< Previously recognized words */
    uint32_t num_prior_words;            /**< Number of prior words */
    wernicke_concept_t* active_concepts; /**< Currently active concepts */
    uint32_t num_active_concepts;        /**< Number of active concepts */
    float topic_embedding[128];          /**< Topic/context embedding */
    bool has_topic;                      /**< Whether topic is set */
} wernicke_context_t;

/**
 * @brief Parse tree node for syntactic comprehension
 */
typedef struct wernicke_parse_node {
    char label[32];                      /**< Syntactic category (NP, VP, S, etc.) */
    uint32_t start_word;                 /**< Start word index */
    uint32_t end_word;                   /**< End word index (exclusive) */
    uint8_t thematic_role;               /**< Thematic role (agent, patient, etc.) */
    struct wernicke_parse_node* children;/**< Child nodes */
    uint32_t num_children;               /**< Number of children */
} wernicke_parse_node_t;

/**
 * @brief Complete parse result
 */
typedef struct {
    wernicke_parse_node_t* root;         /**< Root of parse tree */
    bool is_valid;                       /**< Parse succeeded */
    float parse_confidence;              /**< Parse confidence [0,1] */
    char* semantic_representation;       /**< Logical form (optional) */
} wernicke_parse_t;

/**
 * @brief Complete comprehension result
 */
typedef struct {
    /* Phonological result */
    uint32_t phoneme_count;              /**< Phonemes processed */
    float avg_phoneme_confidence;        /**< Average phoneme confidence */

    /* Lexical result */
    wernicke_word_result_t* words;       /**< Recognized words */
    uint32_t word_count;                 /**< Number of words */

    /* Semantic result */
    wernicke_concept_t* concepts;        /**< Activated concepts */
    uint32_t concept_count;              /**< Number of concepts */
    float semantic_coherence;            /**< Semantic coherence score */

    /* Syntactic result */
    wernicke_parse_t* parse;             /**< Parse tree (if syntactic enabled) */

    /* Overall */
    float comprehension_score;           /**< Overall comprehension [0,1] */
    uint64_t processing_time_ms;         /**< Total processing time */
} wernicke_comprehension_t;

/**
 * @brief Word prediction result
 */
typedef struct {
    wernicke_word_t candidates[10];      /**< Top word candidates */
    float probabilities[10];             /**< Candidate probabilities */
    uint32_t num_candidates;             /**< Number of candidates */
} wernicke_word_pred_t;

/**
 * @brief Efference copy from Broca's area
 */
typedef struct {
    uint8_t* planned_phonemes;           /**< Planned phoneme sequence */
    uint32_t phoneme_count;              /**< Number of planned phonemes */
    uint64_t expected_onset_ms;          /**< Expected production onset */
    float confidence;                    /**< Planning confidence */
} broca_efference_copy_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Adapter statistics
 */
typedef struct {
    /* Processing counts */
    uint64_t phonemes_processed;         /**< Total phonemes processed */
    uint64_t words_recognized;           /**< Total words recognized */
    uint64_t concepts_activated;         /**< Total concepts activated */
    uint64_t utterances_comprehended;    /**< Total utterances */

    /* Success/failure */
    uint64_t successful_recognitions;    /**< Successful word recognitions */
    uint64_t phonological_errors;        /**< Phonological failures */
    uint64_t lexical_misses;             /**< Words not in lexicon */
    uint64_t semantic_errors;            /**< Semantic failures */
    uint64_t syntactic_errors;           /**< Parse failures */

    /* Timing */
    float avg_phoneme_latency_ms;        /**< Average phoneme processing time */
    float avg_word_latency_ms;           /**< Average word recognition time */
    float avg_comprehension_latency_ms;  /**< Average total comprehension time */
    float max_latency_ms;                /**< Maximum latency observed */

    /* Training */
    uint64_t training_iterations;        /**< Training updates */
    float training_loss;                 /**< Current training loss */

    /* Cross-modal */
    uint64_t audiovisual_fusions;        /**< Audiovisual fusion events */
    uint64_t mcgurk_effects;             /**< McGurk effect occurrences */
} wernicke_stats_t;

/*=============================================================================
 * CALLBACK TYPES
 *===========================================================================*/

/**
 * @brief Callback for word recognition events
 */
typedef void (*wernicke_word_callback_t)(
    const wernicke_word_result_t* word,
    void* user_data
);

/**
 * @brief Callback for concept activation events
 */
typedef void (*wernicke_concept_callback_t)(
    const wernicke_concept_t* concept_data,
    void* user_data
);

/**
 * @brief Callback for comprehension events
 */
typedef void (*wernicke_comprehension_callback_t)(
    const wernicke_comprehension_t* comprehension,
    void* user_data
);

/**
 * @brief Callback for general events
 */
typedef void (*wernicke_event_callback_t)(
    uint32_t event_type,
    const void* event_data,
    void* user_data
);

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * WHAT: Returns default configuration for Wernicke's area adapter
 * WHY:  Provide sensible defaults for common use cases
 * HOW:  Initialize all fields with biologically-motivated values
 *
 * @return Default configuration structure
 */
wernicke_config_t wernicke_default_config(void);

/**
 * @brief Create Wernicke's area adapter
 *
 * WHAT: Allocate and initialize the adapter with all sub-modules
 * WHY:  Central point for language comprehension initialization
 * HOW:  Create phonological, lexical, semantic, syntactic processors
 *
 * @param config Configuration (NULL for defaults)
 * @return New adapter instance, or NULL on failure
 */
wernicke_adapter_t* wernicke_create(const wernicke_config_t* config);

/**
 * @brief Destroy Wernicke's area adapter
 *
 * WHAT: Free all resources associated with the adapter
 * WHY:  Prevent memory leaks
 * HOW:  Destroy sub-modules, free buffers and lexicon
 *
 * @param adapter Adapter to destroy
 */
void wernicke_destroy(wernicke_adapter_t* adapter);

/**
 * @brief Reset adapter state
 *
 * WHAT: Clear buffers and reset to idle state
 * WHY:  Prepare for new utterance without full reinitialization
 * HOW:  Reset all sub-modules, clear working memory
 *
 * @param adapter Adapter instance
 * @return true on success, false on failure
 */
bool wernicke_reset(wernicke_adapter_t* adapter);

/*=============================================================================
 * PHONOLOGICAL PROCESSING
 *===========================================================================*/

/**
 * @brief Process raw audio for phoneme detection
 *
 * WHAT: Extract phonemes from audio stream
 * WHY:  First stage of language comprehension
 * HOW:  Formant analysis, phoneme classification
 *
 * @param adapter Adapter instance
 * @param audio Audio samples (float32, mono)
 * @param num_samples Number of samples
 * @param sample_rate Sample rate (Hz)
 * @param phonemes Output phoneme events
 * @param max_phonemes Maximum phonemes to detect
 * @param num_detected Output: number of phonemes detected
 * @return true on success
 */
bool wernicke_process_audio(
    wernicke_adapter_t* adapter,
    const float* audio,
    uint32_t num_samples,
    uint32_t sample_rate,
    phoneme_event_t* phonemes,
    uint32_t max_phonemes,
    uint32_t* num_detected
);

/**
 * @brief Process phoneme sequence (from speech cortex)
 *
 * WHAT: Process pre-detected phonemes
 * WHY:  Integration with existing speech cortex
 * HOW:  Store phonemes, trigger lexical processing
 *
 * @param adapter Adapter instance
 * @param phonemes Input phoneme events
 * @param count Number of phonemes
 * @return true on success
 */
bool wernicke_process_phonemes(
    wernicke_adapter_t* adapter,
    const phoneme_event_t* phonemes,
    uint32_t count
);

/*=============================================================================
 * LEXICAL ACCESS (Word Recognition)
 *===========================================================================*/

/**
 * @brief Recognize word from phoneme sequence
 *
 * WHAT: Map phoneme sequence to word
 * WHY:  Core lexical access function
 * HOW:  Search lexicon, frequency-weighted matching
 *
 * @param adapter Adapter instance
 * @param phonemes Input phoneme sequence
 * @param count Number of phonemes
 * @param result Output word result
 * @return true if word recognized
 */
bool wernicke_recognize_word(
    wernicke_adapter_t* adapter,
    const phoneme_t* phonemes,
    uint32_t count,
    wernicke_word_result_t* result
);

/**
 * @brief Add word to lexicon
 *
 * WHAT: Store word-phoneme mapping
 * WHY:  Build vocabulary for comprehension
 * HOW:  Hash storage with phoneme sequence
 *
 * @param adapter Adapter instance
 * @param word Word entry to add
 * @return true on success
 */
bool wernicke_add_word(
    wernicke_adapter_t* adapter,
    const wernicke_word_t* word
);

/**
 * @brief Look up word by string
 *
 * WHAT: Retrieve word entry from lexicon
 * WHY:  Direct lexical access
 * HOW:  Hash lookup by word string
 *
 * @param adapter Adapter instance
 * @param word_str Word string
 * @param entry Output word entry
 * @return true if found
 */
bool wernicke_lookup_word(
    const wernicke_adapter_t* adapter,
    const char* word_str,
    wernicke_word_t* entry
);

/**
 * @brief Predict next word
 *
 * WHAT: Generate word candidates based on context
 * WHY:  Predictive comprehension
 * HOW:  N-gram model, semantic priming
 *
 * @param adapter Adapter instance
 * @param context Current comprehension context
 * @param prediction Output word predictions
 * @return true on success
 */
bool wernicke_predict_next_word(
    wernicke_adapter_t* adapter,
    const wernicke_context_t* context,
    wernicke_word_pred_t* prediction
);

/*=============================================================================
 * SEMANTIC INTEGRATION
 *===========================================================================*/

/**
 * @brief Get meaning of word
 *
 * WHAT: Retrieve semantic concept for word
 * WHY:  Map words to meanings
 * HOW:  Look up concept via word's concept_id
 *
 * @param adapter Adapter instance
 * @param word Recognized word
 * @param concept Output concept
 * @return true if concept found
 */
bool wernicke_get_meaning(
    wernicke_adapter_t* adapter,
    const wernicke_word_result_t* word,
    wernicke_concept_t* concept_out
);

/**
 * @brief Disambiguate word meaning
 *
 * WHAT: Select correct meaning for ambiguous word
 * WHY:  Context-dependent interpretation (e.g., "bank")
 * HOW:  Semantic similarity with context concepts
 *
 * @param adapter Adapter instance
 * @param word Ambiguous word
 * @param context Current context
 * @param concept Output disambiguated concept
 * @return true on success
 */
bool wernicke_disambiguate(
    wernicke_adapter_t* adapter,
    const wernicke_word_result_t* word,
    const wernicke_context_t* context,
    wernicke_concept_t* concept_out
);

/**
 * @brief Activate related concepts (spreading activation)
 *
 * WHAT: Spread activation through semantic network
 * WHY:  Enable semantic priming and association
 * HOW:  BFS/DFS with decay over semantic graph
 *
 * @param adapter Adapter instance
 * @param concept_id Source concept
 * @param depth Spreading depth
 * @param activated Output activated concepts
 * @param max_concepts Maximum concepts to activate
 * @param num_activated Output: number activated
 * @return true on success
 */
bool wernicke_spread_activation(
    wernicke_adapter_t* adapter,
    uint32_t concept_id,
    uint32_t depth,
    wernicke_concept_t* activated,
    uint32_t max_concepts,
    uint32_t* num_activated
);

/*=============================================================================
 * SENTENCE COMPREHENSION
 *===========================================================================*/

/**
 * @brief Comprehend utterance
 *
 * WHAT: Full comprehension pipeline
 * WHY:  Process complete utterance
 * HOW:  Phonological → Lexical → Semantic → Syntactic
 *
 * @param adapter Adapter instance
 * @param audio Audio samples
 * @param num_samples Number of samples
 * @param sample_rate Sample rate (Hz)
 * @param result Output comprehension result
 * @return true on success
 */
bool wernicke_comprehend(
    wernicke_adapter_t* adapter,
    const float* audio,
    uint32_t num_samples,
    uint32_t sample_rate,
    wernicke_comprehension_t* result
);

/**
 * @brief Parse sentence structure
 *
 * WHAT: Build syntactic parse tree
 * WHY:  Enable sentence-level understanding
 * HOW:  CKY/Earley parsing with grammar
 *
 * @param adapter Adapter instance
 * @param words Recognized words
 * @param count Number of words
 * @param parse Output parse tree
 * @return true on successful parse
 */
bool wernicke_parse_sentence(
    wernicke_adapter_t* adapter,
    const wernicke_word_result_t* words,
    uint32_t count,
    wernicke_parse_t* parse
);

/**
 * @brief Free comprehension result
 *
 * @param result Comprehension result to free
 */
void wernicke_free_comprehension(wernicke_comprehension_t* result);

/**
 * @brief Free parse tree
 *
 * @param parse Parse tree to free
 */
void wernicke_free_parse(wernicke_parse_t* parse);

/*=============================================================================
 * CROSS-MODAL INTEGRATION
 *===========================================================================*/

/**
 * @brief Integrate audiovisual speech (McGurk effect)
 *
 * WHAT: Fuse audio and visual phoneme information
 * WHY:  Lip-reading enhances comprehension
 * HOW:  Bayesian fusion of audio/visual cues
 *
 * @param adapter Adapter instance
 * @param audio_phonemes Audio-derived phonemes
 * @param visual_lip_shapes Visual lip shape features
 * @param num_frames Number of frames
 * @param fused_phonemes Output fused phonemes
 * @param num_fused Output: number of fused phonemes
 * @return true on success
 */
bool wernicke_integrate_audiovisual(
    wernicke_adapter_t* adapter,
    const phoneme_event_t* audio_phonemes,
    const float* visual_lip_shapes,
    uint32_t num_frames,
    phoneme_event_t* fused_phonemes,
    uint32_t* num_fused
);

/*=============================================================================
 * BROCA CONNECTION (Arcuate Fasciculus)
 *===========================================================================*/

/**
 * @brief Connect to Broca's area
 *
 * WHAT: Establish arcuate fasciculus connection
 * WHY:  Enable bidirectional language loop
 * HOW:  Store Broca adapter reference
 *
 * @param adapter Wernicke adapter
 * @param broca Broca adapter to connect
 * @return true on success
 */
bool wernicke_connect_broca(
    wernicke_adapter_t* adapter,
    broca_adapter_t* broca
);

/**
 * @brief Send comprehension to Broca's area
 *
 * WHAT: Forward comprehension result for production
 * WHY:  Enable repetition, reformulation
 * HOW:  Send via arcuate fasciculus (bio-async)
 *
 * @param adapter Adapter instance
 * @param comprehension Comprehension to send
 * @return true on success
 */
bool wernicke_send_to_broca(
    wernicke_adapter_t* adapter,
    const wernicke_comprehension_t* comprehension
);

/**
 * @brief Receive efference copy from Broca
 *
 * WHAT: Process motor plan feedback
 * WHY:  Enable self-monitoring during speech
 * HOW:  Compare expected vs actual phonemes
 *
 * @param adapter Adapter instance
 * @param efference Efference copy from Broca
 * @return true on success
 */
bool wernicke_receive_efference_copy(
    wernicke_adapter_t* adapter,
    const broca_efference_copy_t* efference
);

/*=============================================================================
 * WORKING MEMORY (Phonological Loop)
 *===========================================================================*/

/**
 * @brief Store phonemes in working memory
 *
 * WHAT: Buffer phonemes in phonological store
 * WHY:  Enable rehearsal and sentence processing
 * HOW:  Circular buffer with decay
 *
 * @param adapter Adapter instance
 * @param phonemes Phonemes to store
 * @param count Number of phonemes
 * @return true on success, false if full
 */
bool wernicke_wm_store(
    wernicke_adapter_t* adapter,
    const phoneme_t* phonemes,
    uint32_t count
);

/**
 * @brief Rehearse working memory contents
 *
 * WHAT: Refresh phonological loop contents
 * WHY:  Prevent decay, maintain items
 * HOW:  Re-activate phoneme representations
 *
 * @param adapter Adapter instance
 * @return true on success
 */
bool wernicke_wm_rehearse(wernicke_adapter_t* adapter);

/**
 * @brief Get working memory contents
 *
 * WHAT: Retrieve current phonological buffer
 * WHY:  Inspection, serial recall
 * HOW:  Copy buffer to output
 *
 * @param adapter Adapter instance
 * @param phonemes Output buffer
 * @param max_count Buffer capacity
 * @param count Output: actual count
 * @return true on success
 */
bool wernicke_wm_get_contents(
    const wernicke_adapter_t* adapter,
    phoneme_t* phonemes,
    uint32_t max_count,
    uint32_t* count
);

/**
 * @brief Clear working memory
 *
 * @param adapter Adapter instance
 */
void wernicke_wm_clear(wernicke_adapter_t* adapter);

/*=============================================================================
 * EVENT INTEGRATION
 *===========================================================================*/

/**
 * @brief Set word recognition callback
 *
 * @param adapter Adapter instance
 * @param callback Word event handler
 * @param user_data User context
 * @return true on success
 */
bool wernicke_set_word_callback(
    wernicke_adapter_t* adapter,
    wernicke_word_callback_t callback,
    void* user_data
);

/**
 * @brief Set concept activation callback
 *
 * @param adapter Adapter instance
 * @param callback Concept event handler
 * @param user_data User context
 * @return true on success
 */
bool wernicke_set_concept_callback(
    wernicke_adapter_t* adapter,
    wernicke_concept_callback_t callback,
    void* user_data
);

/**
 * @brief Set comprehension callback
 *
 * @param adapter Adapter instance
 * @param callback Comprehension event handler
 * @param user_data User context
 * @return true on success
 */
bool wernicke_set_comprehension_callback(
    wernicke_adapter_t* adapter,
    wernicke_comprehension_callback_t callback,
    void* user_data
);

/**
 * @brief Set general event callback
 *
 * @param adapter Adapter instance
 * @param callback Event handler
 * @param user_data User context
 * @return true on success
 */
bool wernicke_set_event_callback(
    wernicke_adapter_t* adapter,
    wernicke_event_callback_t callback,
    void* user_data
);

/*=============================================================================
 * TRAINING INTERFACE
 *===========================================================================*/

/**
 * @brief Train word recognition
 *
 * WHAT: Update lexical access weights
 * WHY:  Learn word-phoneme mappings
 * HOW:  Contrastive learning on phoneme sequences
 *
 * @param adapter Adapter instance
 * @param phonemes Input phoneme sequence
 * @param num_phonemes Number of phonemes
 * @param target_word Correct word string
 * @param learning_rate Learning rate (0 = use default)
 * @return true on success
 */
bool wernicke_train_word(
    wernicke_adapter_t* adapter,
    const phoneme_t* phonemes,
    uint32_t num_phonemes,
    const char* target_word,
    float learning_rate
);

/**
 * @brief Train semantic association
 *
 * WHAT: Strengthen word-concept link
 * WHY:  Learn word meanings
 * HOW:  Hebbian update on embeddings
 *
 * @param adapter Adapter instance
 * @param word_id Word to train
 * @param concept_id Associated concept
 * @param strength Association strength [0,1]
 * @return true on success
 */
bool wernicke_train_semantic(
    wernicke_adapter_t* adapter,
    uint32_t word_id,
    uint32_t concept_id,
    float strength
);

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

/**
 * @brief Get current processing status
 *
 * @param adapter Adapter instance
 * @return Current status
 */
wernicke_status_t wernicke_get_status(const wernicke_adapter_t* adapter);

/**
 * @brief Get last error code
 *
 * @param adapter Adapter instance
 * @return Last error, or WERNICKE_ERROR_NONE
 */
wernicke_error_t wernicke_get_last_error(const wernicke_adapter_t* adapter);

/**
 * @brief Get error description string
 *
 * @param error Error code
 * @return Human-readable error description
 */
const char* wernicke_error_string(wernicke_error_t error);

/**
 * @brief Get status description string
 *
 * @param status Status code
 * @return Human-readable status description
 */
const char* wernicke_status_string(wernicke_status_t status);

/**
 * @brief Get adapter statistics
 *
 * @param adapter Adapter instance
 * @param stats Output statistics structure
 * @return true on success
 */
bool wernicke_get_stats(const wernicke_adapter_t* adapter, wernicke_stats_t* stats);

/**
 * @brief Get adapter configuration
 *
 * @param adapter Adapter instance
 * @param config Output configuration structure
 * @return true on success
 */
bool wernicke_get_config(const wernicke_adapter_t* adapter, wernicke_config_t* config);

/*=============================================================================
 * SUB-MODULE ACCESS (Advanced)
 *===========================================================================*/

/**
 * @brief Get phonological analyzer handle
 *
 * @param adapter Adapter instance
 * @return Phonological analyzer, or NULL
 */
phonological_analyzer_t* wernicke_get_phonological_analyzer(wernicke_adapter_t* adapter);

/**
 * @brief Get lexical access handle
 *
 * @param adapter Adapter instance
 * @return Lexical access, or NULL
 */
lexical_access_t* wernicke_get_lexical_access(wernicke_adapter_t* adapter);

/**
 * @brief Get semantic integrator handle
 *
 * @param adapter Adapter instance
 * @return Semantic integrator, or NULL
 */
semantic_integrator_t* wernicke_get_semantic_integrator(wernicke_adapter_t* adapter);

/**
 * @brief Get syntactic comprehension handle
 *
 * @param adapter Adapter instance
 * @return Syntactic comprehension, or NULL
 */
syntactic_comprehension_t* wernicke_get_syntactic_comprehension(wernicke_adapter_t* adapter);

/*=============================================================================
 * BIO-ASYNC COMMUNICATION
 *===========================================================================*/

/**
 * @brief Get bio-async module context
 *
 * @param adapter Adapter instance
 * @return Bio-async module context, or NULL if not enabled
 */
bio_module_context_t wernicke_get_bio_context(wernicke_adapter_t* adapter);

/**
 * @brief Process pending bio-async messages
 *
 * @param adapter Adapter instance
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t wernicke_process_bio_messages(wernicke_adapter_t* adapter, uint32_t max_messages);

/**
 * @brief Request word recognition asynchronously
 *
 * @param adapter Adapter instance
 * @param phonemes Phoneme sequence
 * @param count Number of phonemes
 * @return Future for word result, or NULL on failure
 */
nimcp_bio_future_t wernicke_request_word_async(
    wernicke_adapter_t* adapter,
    const phoneme_t* phonemes,
    uint32_t count
);

/**
 * @brief Broadcast word recognition event
 *
 * @param adapter Adapter instance
 * @param word Recognized word
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t wernicke_broadcast_word(
    wernicke_adapter_t* adapter,
    const wernicke_word_result_t* word
);

/**
 * @brief Broadcast comprehension complete event
 *
 * @param adapter Adapter instance
 * @param comprehension Comprehension result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t wernicke_broadcast_comprehension(
    wernicke_adapter_t* adapter,
    const wernicke_comprehension_t* comprehension
);

/*=============================================================================
 * KNOWLEDGE GRAPH INTEGRATION
 *===========================================================================*/

/**
 * @brief Register Wernicke's area with brain knowledge graph
 *
 * WHAT: Add Wernicke node and edges to KG
 * WHY:  Enable self-awareness of language capabilities
 * HOW:  Create node, add edges to Broca, semantic memory
 *
 * @param adapter Adapter instance
 * @param kg Brain knowledge graph
 * @return true on success
 */
bool wernicke_register_kg(
    wernicke_adapter_t* adapter,
    brain_kg_t* kg
);

/**
 * @brief Connect to semantic memory
 *
 * @param adapter Adapter instance
 * @param semantic Semantic memory instance
 * @return true on success
 */
bool wernicke_connect_semantic_memory(
    wernicke_adapter_t* adapter,
    semantic_memory_t* semantic
);

/*=============================================================================
 * SNN SUBSTRATE BINDING (wernicke_substrate population)
 *===========================================================================*/

/**
 * @brief Attach the SNN substrate population that backs Wernicke's area.
 *
 * WHAT: Stores a reference to the SNN network + the population id of the
 *       wernicke_substrate pop (created by nimcp_brain_factory_init_language_pops).
 * WHY:  Comprehension ticks need to read spike rates from the substrate pop
 *       (driven by L3_concept / L4_integr taps) so phonological / lexical
 *       processing can gate on actual cortical drive instead of running blind.
 *       Without this binding the 64K-neuron pop is floating — it spikes from
 *       hierarchy taps but the adapter logic can't observe it.
 * HOW:  Adapter caches (snn, pop_id). pop_id < 0 = unbound (no-op). Idempotent.
 *
 * @param adapter Wernicke adapter instance
 * @param snn     SNN network owning the substrate pop (may be NULL → unbind)
 * @param pop_id  Pop id from snn_network_add_population_lightweight (< 0 = unbind)
 * @return true on success, false if adapter is NULL
 */
bool wernicke_attach_snn_pop(
    wernicke_adapter_t* adapter,
    snn_network_t* snn,
    int pop_id
);

/**
 * @brief Get the bound SNN population id.
 *
 * @param adapter Wernicke adapter instance
 * @return Pop id (>= 0) if bound, -1 if unbound or adapter is NULL
 */
int wernicke_get_snn_pop_id(const wernicke_adapter_t* adapter);

/**
 * @brief Get the bound SNN network handle.
 *
 * @param adapter Wernicke adapter instance
 * @return SNN network pointer if bound, NULL otherwise
 */
snn_network_t* wernicke_get_snn_network(const wernicke_adapter_t* adapter);

/*=============================================================================
 * GROUNDED-LANGUAGE BINDING
 *===========================================================================*/

/**
 * @brief Attach the grounded_language handle. After this, downstream
 *        consumers can ingest wernicke comprehension results as
 *        auditory grounding events. Stored as opaque void* so callers
 *        don't need the GL header. NULL = unbind.
 *
 * @param adapter Wernicke adapter instance
 * @param gl      grounded_language_t* (opaque)
 * @return true on success, false if adapter is NULL
 */
bool wernicke_attach_grounded_language(wernicke_adapter_t* adapter, void* gl);

/** @brief Get the bound grounded_language handle (NULL if unbound). */
void* wernicke_get_grounded_language(const wernicke_adapter_t* adapter);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WERNICKE_ADAPTER_H */
