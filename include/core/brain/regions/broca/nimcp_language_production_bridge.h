/**
 * @file nimcp_language_production_bridge.h
 * @brief Bridge connecting Broca's region with Speech Cortex and NLP systems
 *
 * WHAT: Unified interface for language production pipeline integration
 * WHY:  Connect comprehension (Wernicke) to production (Broca) with NLP semantics
 * HOW:  Orchestrates data flow between speech cortex, NLP, and Broca's region
 *
 * ARCHITECTURE:
 * ┌─────────────────────────────────────────────────────────────────────┐
 * │                     Language Production Bridge                       │
 * ├─────────────────────────────────────────────────────────────────────┤
 * │                                                                       │
 * │  [Semantic Intent]     [NLP Network]      [Working Memory]           │
 * │        │                    │                    │                   │
 * │        └────────────────────┼────────────────────┘                   │
 * │                             ▼                                        │
 * │                    ┌───────────────┐                                 │
 * │                    │  Lexical      │                                 │
 * │                    │  Selection    │◄─── Speech Cortex (Wernicke)   │
 * │                    └───────┬───────┘                                 │
 * │                            ▼                                         │
 * │                    ┌───────────────┐                                 │
 * │                    │  Broca's      │                                 │
 * │                    │  Region       │                                 │
 * │                    │  Adapter      │                                 │
 * │                    └───────┬───────┘                                 │
 * │                            ▼                                         │
 * │                    ┌───────────────┐                                 │
 * │                    │  Motor        │──► Motor Cortex                 │
 * │                    │  Commands     │                                 │
 * │                    └───────────────┘                                 │
 * │                                                                       │
 * └─────────────────────────────────────────────────────────────────────┘
 *
 * BIOLOGICAL BASIS:
 * - Wernicke-Broca connection via arcuate fasciculus
 * - Semantic representation from temporal/parietal cortices
 * - Motor output to primary motor cortex (M1) face area
 *
 * @version Phase B3: Language Production Integration
 * @date 2025-11-23
 */

#ifndef NIMCP_LANGUAGE_PRODUCTION_BRIDGE_H
#define NIMCP_LANGUAGE_PRODUCTION_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Forward declarations */
typedef struct broca_adapter broca_adapter_t;
typedef struct language_production_bridge language_production_bridge_t;

/* External system forward declarations (opaque) */
typedef struct speech_cortex speech_cortex_t;
typedef struct nlp_network nlp_network_t;
typedef struct working_memory working_memory_t;
typedef struct lexical_access lexical_access_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Default configuration values
 */
#define LPB_DEFAULT_MAX_TOKENS          128
#define LPB_DEFAULT_SEMANTIC_DIM        256
#define LPB_DEFAULT_COMPREHENSION_THRESHOLD 0.6f
#define LPB_DEFAULT_PRODUCTION_DELAY_MS 150.0f

/**
 * @brief Positional encoding configuration for motor sequences
 *
 * WHAT: Parameters for encoding position in motor command sequences
 * WHY:  Motor commands must preserve temporal ordering for fluent speech
 * HOW:  Configure sinusoidal or RoPE encoding for sequence modeling
 */
typedef struct {
    /* Positional encoding type */
    uint8_t motor_seq_pe_type;       /**< 0=sinusoidal, 2=RoPE for motor sequences */
    uint8_t gesture_pe_type;         /**< 0=sinusoidal, 2=RoPE for articulatory gestures */

    /* Motor sequence PE parameters */
    uint32_t motor_seq_max_length;   /**< Max motor command sequence length */
    uint32_t motor_seq_embedding_dim;/**< Motor embedding dimension */
    float motor_seq_pe_base;         /**< Frequency base for sinusoidal PE */

    /* Articulatory gesture PE parameters */
    uint32_t gesture_max_length;     /**< Max gesture sequence length */
    uint32_t gesture_embedding_dim;  /**< Gesture embedding dimension */
    float gesture_rope_base;         /**< RoPE base for gesture encoding */

    /* Feature flags */
    bool enable_motor_pe_cache;      /**< Pre-compute and cache motor PE */
    bool enable_gesture_pe_cache;    /**< Pre-compute and cache gesture PE */
} lpb_pe_config_t;

/**
 * @brief Language production bridge configuration
 */
typedef struct {
    /* Capacity */
    uint32_t max_tokens;             /**< Maximum tokens per utterance */
    uint32_t semantic_dim;           /**< Semantic vector dimension */

    /* Thresholds */
    float comprehension_threshold;   /**< Min comprehension score for echo */
    float production_delay_ms;       /**< Simulated production latency */

    /* Feature flags */
    bool enable_wernicke_connection; /**< Connect to speech cortex */
    bool enable_nlp_connection;      /**< Connect to NLP network */
    bool enable_working_memory;      /**< Connect to working memory */
    bool enable_semantic_priming;    /**< Use semantic context */
    bool enable_repetition;          /**< Allow verbatim repetition */
    bool enable_paraphrase;          /**< Generate paraphrased output */

    /* Feedback */
    bool enable_self_monitoring;     /**< Monitor own production */
    bool enable_error_correction;    /**< Attempt to correct errors */

    /* Neuromodulation */
    bool enable_second_messengers;   /**< Enable second messenger cascades */

    /* Positional encoding */
    bool enable_positional_encoding; /**< Enable positional encoding for sequences */
    lpb_pe_config_t pe_config;       /**< Positional encoding configuration */
} lpb_config_t;

/*=============================================================================
 * STATUS AND STATE
 *===========================================================================*/

/**
 * @brief Bridge processing status
 */
typedef enum {
    LPB_STATUS_IDLE = 0,
    LPB_STATUS_RECEIVING_INTENT,     /**< Getting semantic input */
    LPB_STATUS_LEXICAL_SELECTION,    /**< Choosing words */
    LPB_STATUS_SYNTACTIC_ENCODING,   /**< Building structure */
    LPB_STATUS_PHONOLOGICAL_ENCODING,/**< Planning sounds */
    LPB_STATUS_ARTICULATION,         /**< Generating motor commands */
    LPB_STATUS_SELF_MONITORING,      /**< Checking output */
    LPB_STATUS_READY,                /**< Output available */
    LPB_STATUS_ERROR
} lpb_status_t;

/**
 * @brief Error codes
 */
typedef enum {
    LPB_ERROR_NONE = 0,
    LPB_ERROR_INVALID_INPUT,
    LPB_ERROR_NO_BROCA,
    LPB_ERROR_NO_SPEECH_CORTEX,
    LPB_ERROR_NO_NLP,
    LPB_ERROR_LEXICAL_FAILURE,
    LPB_ERROR_PRODUCTION_FAILURE,
    LPB_ERROR_MONITORING_FAILURE,
    LPB_ERROR_INTERNAL
} lpb_error_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Semantic intent from comprehension systems
 */
typedef struct {
    float* semantic_vector;          /**< Semantic representation */
    uint32_t semantic_dim;           /**< Vector dimension */
    float confidence;                /**< Comprehension confidence */

    /* Intent classification */
    uint8_t intent_type;             /**< 0=statement, 1=question, 2=command */
    uint8_t speech_act;              /**< Pragmatic function */

    /* Source information */
    bool from_wernicke;              /**< Came from speech comprehension */
    bool from_nlp;                   /**< Came from NLP processing */
    bool from_internal;              /**< Internally generated */
} lpb_semantic_intent_t;

/**
 * @brief Token from lexical selection
 */
typedef struct {
    uint32_t token_id;               /**< Token/word identifier */
    char token_str[32];              /**< String representation */
    uint8_t pos;                     /**< Part of speech */
    float activation;                /**< Selection strength */
    float frequency;                 /**< Usage frequency */
} lpb_token_t;

/**
 * @brief Production result
 */
typedef struct {
    /* Token output */
    lpb_token_t* tokens;             /**< Selected tokens */
    uint32_t token_count;            /**< Number of tokens */

    /* Phoneme output */
    uint8_t* phonemes;               /**< Phoneme sequence */
    uint32_t phoneme_count;          /**< Number of phonemes */

    /* Motor output */
    uint32_t motor_command_count;    /**< Number of motor commands */
    float estimated_duration_ms;     /**< Estimated speech duration */

    /* Quality metrics */
    float fluency_score;             /**< Production fluency [0,1] */
    float semantic_match;            /**< Match to input intent [0,1] */
    bool self_monitoring_passed;     /**< Self-check result */
} lpb_production_result_t;

/**
 * @brief Statistics
 */
typedef struct {
    uint64_t productions_attempted;
    uint64_t productions_successful;
    uint64_t lexical_selections;
    uint64_t self_corrections;
    uint64_t wernicke_inputs;
    uint64_t nlp_inputs;
    float avg_production_latency_ms;
    float avg_fluency_score;
} lpb_stats_t;

/*=============================================================================
 * CALLBACK TYPES
 *===========================================================================*/

/**
 * @brief Callback for motor output (to motor cortex)
 */
typedef void (*lpb_motor_output_callback_t)(
    const void* motor_command,
    void* user_data
);

/**
 * @brief Callback for production events
 */
typedef void (*lpb_event_callback_t)(
    uint32_t event_type,
    const void* event_data,
    void* user_data
);

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
lpb_config_t lpb_default_config(void);

/**
 * @brief Create language production bridge
 *
 * @param config Configuration (NULL for defaults)
 * @param broca Broca's region adapter (required)
 * @return New bridge instance, or NULL on failure
 */
language_production_bridge_t* lpb_create(
    const lpb_config_t* config,
    broca_adapter_t* broca
);

/**
 * @brief Destroy bridge
 */
void lpb_destroy(language_production_bridge_t* bridge);

/**
 * @brief Reset bridge state
 */
bool lpb_reset(language_production_bridge_t* bridge);

/*=============================================================================
 * SYSTEM CONNECTIONS
 *===========================================================================*/

/**
 * @brief Connect to speech cortex (Wernicke's area)
 *
 * WHAT: Establish Wernicke-Broca connection
 * WHY:  Receive comprehended speech for repetition/response
 * HOW:  Store reference, enable arcuate fasciculus simulation
 *
 * @param bridge Bridge instance
 * @param speech_cortex Speech cortex instance
 * @return true on success
 */
bool lpb_connect_speech_cortex(
    language_production_bridge_t* bridge,
    speech_cortex_t* speech_cortex
);

/**
 * @brief Connect to NLP network
 *
 * WHAT: Establish NLP connection
 * WHY:  Receive semantic representations from language understanding
 * HOW:  Store reference, enable semantic-to-lexical mapping
 *
 * @param bridge Bridge instance
 * @param nlp NLP network instance
 * @return true on success
 */
bool lpb_connect_nlp(
    language_production_bridge_t* bridge,
    nlp_network_t* nlp
);

/**
 * @brief Connect to working memory
 *
 * WHAT: Establish working memory connection
 * WHY:  Buffer words during sentence construction
 * HOW:  Use WM for rehearsal loop (phonological loop)
 *
 * @param bridge Bridge instance
 * @param wm Working memory instance
 * @return true on success
 */
bool lpb_connect_working_memory(
    language_production_bridge_t* bridge,
    working_memory_t* wm
);

/**
 * @brief Connect lexical access module for word lookup during production
 *
 * @param bridge Bridge instance
 * @param lexical Lexical access module (from Wernicke's area)
 * @return true on success
 */
bool lpb_connect_lexical_access(
    language_production_bridge_t* bridge,
    lexical_access_t* lexical
);

/*=============================================================================
 * PRODUCTION PIPELINE
 *===========================================================================*/

/**
 * @brief Produce speech from semantic intent
 *
 * WHAT: Main production function - intent to speech
 * WHY:  Convert meaning to articulatory output
 * HOW:  Lexical selection → syntax → phonology → motor
 *
 * @param bridge Bridge instance
 * @param intent Semantic intent to express
 * @param result Output result (optional)
 * @return true on success
 */
bool lpb_produce_from_intent(
    language_production_bridge_t* bridge,
    const lpb_semantic_intent_t* intent,
    lpb_production_result_t* result
);

/**
 * @brief Produce speech from token sequence
 *
 * WHAT: Produce from pre-selected words
 * WHY:  Bypass lexical selection for direct production
 * HOW:  Tokens → syntax → phonology → motor
 *
 * @param bridge Bridge instance
 * @param tokens Token array
 * @param num_tokens Number of tokens
 * @param result Output result (optional)
 * @return true on success
 */
bool lpb_produce_from_tokens(
    language_production_bridge_t* bridge,
    const lpb_token_t* tokens,
    uint32_t num_tokens,
    lpb_production_result_t* result
);

/**
 * @brief Repeat last comprehended utterance
 *
 * WHAT: Verbatim repetition of heard speech
 * WHY:  Echoic repetition, phonological rehearsal
 * HOW:  Get phonemes from speech cortex, produce directly
 *
 * @param bridge Bridge instance
 * @param result Output result (optional)
 * @return true on success
 */
bool lpb_repeat_last_heard(
    language_production_bridge_t* bridge,
    lpb_production_result_t* result
);

/**
 * @brief Generate response to comprehended input
 *
 * WHAT: Produce contextually appropriate response
 * WHY:  Conversational turn-taking
 * HOW:  Use NLP to generate response intent, then produce
 *
 * @param bridge Bridge instance
 * @param result Output result (optional)
 * @return true on success
 */
bool lpb_generate_response(
    language_production_bridge_t* bridge,
    lpb_production_result_t* result
);

/*=============================================================================
 * LEXICAL ACCESS
 *===========================================================================*/

/**
 * @brief Select words from semantic vector
 *
 * WHAT: Map semantic representation to lexical items
 * WHY:  Convert meaning to words (lemma selection)
 * HOW:  Spreading activation in lexicon, select highest
 *
 * @param bridge Bridge instance
 * @param semantic_vector Input semantic representation
 * @param dim Vector dimension
 * @param tokens Output token buffer
 * @param max_tokens Buffer capacity
 * @param num_selected Output: actual count
 * @return true on success
 */
bool lpb_select_lexical_items(
    language_production_bridge_t* bridge,
    const float* semantic_vector,
    uint32_t dim,
    lpb_token_t* tokens,
    uint32_t max_tokens,
    uint32_t* num_selected
);

/**
 * @brief Prime lexical access with context
 *
 * WHAT: Bias word selection based on context
 * WHY:  Semantic priming improves retrieval speed/accuracy
 * HOW:  Pre-activate related concepts in lexicon
 *
 * @param bridge Bridge instance
 * @param context_vector Context semantic vector
 * @param dim Vector dimension
 * @param prime_strength Priming strength [0,1]
 * @return true on success
 */
bool lpb_prime_lexical_access(
    language_production_bridge_t* bridge,
    const float* context_vector,
    uint32_t dim,
    float prime_strength
);

/*=============================================================================
 * SELF-MONITORING
 *===========================================================================*/

/**
 * @brief Enable/disable self-monitoring
 *
 * WHAT: Control internal speech monitoring
 * WHY:  Detect and correct errors before articulation
 * HOW:  Compare planned output to intent
 *
 * @param bridge Bridge instance
 * @param enable Enable flag
 * @return true on success
 */
bool lpb_set_self_monitoring(
    language_production_bridge_t* bridge,
    bool enable
);

/**
 * @brief Check production against intent
 *
 * WHAT: Verify production matches intended meaning
 * WHY:  Error detection before output
 * HOW:  Semantic similarity between input and planned output
 *
 * @param bridge Bridge instance
 * @param match_score Output: semantic match score [0,1]
 * @return true if check passed
 */
bool lpb_check_production(
    language_production_bridge_t* bridge,
    float* match_score
);

/*=============================================================================
 * CALLBACKS
 *===========================================================================*/

/**
 * @brief Set motor output callback
 */
bool lpb_set_motor_callback(
    language_production_bridge_t* bridge,
    lpb_motor_output_callback_t callback,
    void* user_data
);

/**
 * @brief Set event callback
 */
bool lpb_set_event_callback(
    language_production_bridge_t* bridge,
    lpb_event_callback_t callback,
    void* user_data
);

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

/**
 * @brief Get current status
 */
lpb_status_t lpb_get_status(const language_production_bridge_t* bridge);

/**
 * @brief Get last error
 */
lpb_error_t lpb_get_last_error(const language_production_bridge_t* bridge);

/**
 * @brief Get error string
 */
const char* lpb_error_string(lpb_error_t error);

/**
 * @brief Get status string
 */
const char* lpb_status_string(lpb_status_t status);

/**
 * @brief Get statistics
 */
bool lpb_get_stats(const language_production_bridge_t* bridge, lpb_stats_t* stats);

/**
 * @brief Get configuration
 */
bool lpb_get_config(const language_production_bridge_t* bridge, lpb_config_t* config);

/*=============================================================================
 * POSITIONAL ENCODING INTEGRATION
 *===========================================================================*/

/**
 * @brief Set positional encoding configuration
 *
 * WHAT: Configure positional encoding for motor sequences and gestures
 * WHY:  Enable temporal ordering awareness in speech production
 * HOW:  Create PE encoders with specified type and parameters
 *
 * BIOLOGICAL BASIS:
 * - Broca's area generates sequential motor commands for speech articulation
 * - Temporal ordering is critical for fluent, intelligible speech production
 * - Position encoding preserves sequence information in motor planning
 *
 * @param bridge Bridge instance
 * @param pe_config Positional encoding configuration
 * @return true on success
 */
bool language_production_set_pe_config(
    language_production_bridge_t* bridge,
    const lpb_pe_config_t* pe_config
);

/**
 * @brief Apply positional encoding to motor command sequence
 *
 * WHAT: Add sinusoidal position encodings to motor command embeddings
 * WHY:  Motor commands must maintain precise temporal order
 * HOW:  Use sinusoidal PE to encode position in command sequence
 *
 * BIOLOGICAL BASIS:
 * - Motor cortex receives temporally ordered commands from Broca's area
 * - Precise timing is essential for coordinated articulatory movements
 * - PE helps model temporal dependencies in motor command sequences
 *
 * @param bridge Bridge instance
 * @param motor_embeddings Input motor command embeddings (seq_len * dim)
 * @param seq_length Number of motor commands in sequence
 * @param output Output with PE applied (can be same as input for in-place)
 * @return true on success
 */
bool language_production_encode_motor_sequence(
    language_production_bridge_t* bridge,
    const float* motor_embeddings,
    uint32_t seq_length,
    float* output
);

/**
 * @brief Apply RoPE encoding to articulatory gesture sequence
 *
 * WHAT: Apply rotary position embedding to gesture representations
 * WHY:  Gestures have relative temporal relationships
 * HOW:  Use RoPE to encode relative position information
 *
 * BIOLOGICAL BASIS:
 * - Articulatory gestures overlap and blend during speech production
 * - Relative timing between gestures determines coarticulation patterns
 * - RoPE captures relative position relationships naturally
 *
 * @param bridge Bridge instance
 * @param gesture_query Query representations (seq_len * dim)
 * @param gesture_key Key representations (seq_len * dim)
 * @param seq_length Number of gestures in sequence
 * @param query_out Output queries with RoPE applied
 * @param key_out Output keys with RoPE applied
 * @return true on success
 */
bool language_production_encode_gesture(
    language_production_bridge_t* bridge,
    const float* gesture_query,
    const float* gesture_key,
    uint32_t seq_length,
    float* query_out,
    float* key_out
);

/*=============================================================================
 * SECOND MESSENGER INTEGRATION
 *===========================================================================*/

/**
 * @brief Trigger receptor activation in second messenger system
 *
 * WHAT: Activate neuromodulator receptors in Broca's region
 * WHY:  Dopamine modulates speech production fluency and speed
 * HOW:  Forward receptor activation to second messenger cascades
 *
 * @param bridge Bridge instance
 * @param neuron_id Neuron ID (0 for all neurons)
 * @param receptor_type Receptor type (e.g., D1, D2)
 * @param occupancy Receptor occupancy [0, 1]
 * @param timestamp_ms Current timestamp
 * @return true on success
 */
bool lpb_trigger_receptor(
    language_production_bridge_t* bridge,
    uint32_t neuron_id,
    uint8_t receptor_type,
    float occupancy,
    uint64_t timestamp_ms
);

/**
 * @brief Get second messenger cascade state
 *
 * WHAT: Query current cascade state for a neuron
 * WHY:  Allows monitoring of neuromodulator effects
 * HOW:  Returns PKA, PKC, CaMKII activity levels
 *
 * @param bridge Bridge instance
 * @param neuron_id Neuron ID
 * @param pka_activity Output: PKA activity [0, 1]
 * @param pkc_activity Output: PKC activity [0, 1]
 * @param camkii_activity Output: CaMKII activity [0, 1]
 * @return true on success
 */
bool lpb_get_second_messenger_state(
    const language_production_bridge_t* bridge,
    uint32_t neuron_id,
    float* pka_activity,
    float* pkc_activity,
    float* camkii_activity
);

/*=============================================================================
 * DIRECT ACCESS
 *===========================================================================*/

/**
 * @brief Get underlying Broca adapter
 */
broca_adapter_t* lpb_get_broca_adapter(language_production_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LANGUAGE_PRODUCTION_BRIDGE_H */
