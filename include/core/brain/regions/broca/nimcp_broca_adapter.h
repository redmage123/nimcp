/**
 * @file nimcp_broca_adapter.h
 * @brief Brain adapter for Broca's region integration
 *
 * WHAT: Unified adapter connecting Broca's region sub-modules to the brain system
 * WHY:  Enable seamless integration with cognitive layers, training, and event system
 * HOW:  Orchestrates syntax, phonological, and speech motor processors as a cohesive unit
 *
 * ARCHITECTURE:
 * - Wraps all three Broca sub-modules (syntax, phonological, speech motor)
 * - Provides high-level API for language production pipeline
 * - Integrates with working memory for lexical access
 * - Connects to event bus for inter-module communication
 * - Supports training through backpropagation adapters
 *
 * BIOLOGICAL BASIS:
 * - Models Brodmann areas 44 (pars opercularis) and 45 (pars triangularis)
 * - Syntax processing in BA45, motor planning in BA44
 * - Connections to Wernicke's area, motor cortex, and prefrontal cortex
 *
 * @version Phase B2: Broca's Region Brain Integration
 * @date 2025-11-23
 */

#ifndef NIMCP_BROCA_ADAPTER_H
#define NIMCP_BROCA_ADAPTER_H

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

/* Forward declarations for sub-modules */
typedef struct syntax_processor syntax_processor_t;
typedef struct phonological_processor phonological_processor_t;
typedef struct speech_motor_planner speech_motor_planner_t;

/* Forward declaration for opaque adapter type */
typedef struct broca_adapter broca_adapter_t;

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
#define BROCA_DEFAULT_MAX_WORDS          64
#define BROCA_DEFAULT_MAX_PHONEMES       256
#define BROCA_DEFAULT_MAX_COMMANDS       512
#define BROCA_DEFAULT_WORKING_MEMORY_SLOTS 7
/* Phase 2C: bumped 1000→32768 to fit the full WordNet+GloVe lexicon
 * (~29K entries) when the communication cascade mirrors GL's vocab
 * into Broca. The pre-cascade default of 1000 was sized for hand-
 * curated function-word lexicons; without the bump, the GL→Broca
 * mirror caps at ~1000 words and Broca's CYK parser sees POS_UNKNOWN
 * for the other ~28K words. 32768 matches the SNN bridge's word_pop
 * capacity so both subsystems address the same hash space. Memory
 * cost: ~256KB for the bucket array, plus ~120 bytes per registered
 * entry — bounded around 4MB total at full WordNet load. */
#define BROCA_DEFAULT_LEXICON_SIZE       32768
#define BROCA_DEFAULT_PLANNING_WINDOW_MS 200.0f

/**
 * @brief Broca's region adapter configuration
 */
typedef struct {
    /* Capacity limits */
    uint32_t max_words;              /**< Maximum words per utterance */
    uint32_t max_phonemes;           /**< Maximum phonemes in buffer */
    uint32_t max_motor_commands;     /**< Maximum motor commands in queue */

    /* Working memory integration */
    uint32_t working_memory_slots;   /**< Slots for lexical retrieval */
    bool enable_working_memory;      /**< Enable WM integration */

    /* Lexical access */
    uint32_t lexicon_size;           /**< Size of internal lexicon */
    bool enable_lexicon;             /**< Enable built-in lexicon */

    /* Processing options */
    bool enable_coarticulation;      /**< Enable coarticulation planning */
    bool enable_prosody;             /**< Enable prosodic processing */
    bool enable_morphology;          /**< Enable morphological analysis */

    /* Event system */
    bool enable_events;              /**< Enable event bus integration */

    /* Training */
    bool enable_training;            /**< Enable learning capabilities */
    float learning_rate;             /**< Base learning rate */

    /* Timing */
    float planning_window_ms;        /**< Motor planning window */

    /* Bio-async communication */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    nimcp_bio_channel_type_t default_channel; /**< Default neuromodulator channel */
} broca_config_t;

/*=============================================================================
 * STATUS AND STATE
 *===========================================================================*/

/**
 * @brief Processing status of the adapter
 */
typedef enum {
    BROCA_STATUS_IDLE = 0,           /**< Ready for input */
    BROCA_STATUS_LEXICAL_ACCESS,     /**< Retrieving word forms */
    BROCA_STATUS_SYNTACTIC,          /**< Building syntactic structure */
    BROCA_STATUS_PHONOLOGICAL,       /**< Planning phoneme sequence */
    BROCA_STATUS_MOTOR_PLANNING,     /**< Generating motor commands */
    BROCA_STATUS_READY,              /**< Output ready for retrieval */
    BROCA_STATUS_ERROR               /**< Error state */
} broca_status_t;

/**
 * @brief Error codes for Broca's region operations
 */
typedef enum {
    BROCA_ERROR_NONE = 0,
    BROCA_ERROR_INVALID_INPUT,
    BROCA_ERROR_SYNTAX_FAILURE,
    BROCA_ERROR_PHONOLOGICAL_FAILURE,
    BROCA_ERROR_MOTOR_PLANNING_FAILURE,
    BROCA_ERROR_WORKING_MEMORY_FULL,
    BROCA_ERROR_LEXICON_MISS,
    BROCA_ERROR_BUFFER_OVERFLOW,
    BROCA_ERROR_INTERNAL
} broca_error_t;

/*=============================================================================
 * INPUT/OUTPUT STRUCTURES
 *===========================================================================*/

/**
 * @brief Lexical entry for word-to-phoneme mapping
 */
typedef struct {
    uint32_t word_id;                /**< Unique word identifier */
    char word[32];                   /**< Word string */
    uint8_t phonemes[16];            /**< Phoneme sequence */
    uint32_t phoneme_count;          /**< Number of phonemes */
    uint8_t pos;                     /**< Part of speech */
    float frequency;                 /**< Usage frequency (0-1) */
} broca_lexical_entry_t;

/**
 * @brief Input word for production
 */
typedef struct {
    uint32_t word_id;                /**< Lexicon word ID (0 = use string) */
    char word[32];                   /**< Word string (if word_id = 0) */
    uint8_t pos;                     /**< Part of speech hint */
    uint8_t number;                  /**< Grammatical number */
    uint8_t person;                  /**< Grammatical person */
    uint8_t tense;                   /**< Verb tense */
} broca_input_word_t;

/**
 * @brief Output motor command for articulation
 */
typedef struct {
    uint8_t articulator;             /**< Target articulator */
    float position;                  /**< Target position [0, 1] */
    float velocity;                  /**< Movement velocity */
    double timestamp_ms;             /**< Execution time */
    uint8_t phoneme;                 /**< Associated phoneme */
} broca_output_command_t;

/**
 * @brief Complete utterance result
 */
typedef struct {
    /* Syntactic result */
    bool syntax_valid;               /**< Syntax validation passed */
    bool agreement_valid;            /**< Agreement constraints satisfied */
    uint32_t word_count;             /**< Number of words processed */

    /* Phonological result */
    uint32_t syllable_count;         /**< Number of syllables */
    uint32_t phoneme_count;          /**< Total phonemes */
    float total_duration_ms;         /**< Estimated utterance duration */

    /* Motor result */
    uint32_t command_count;          /**< Number of motor commands */
    bool ready_for_articulation;     /**< Commands ready for execution */
} broca_utterance_result_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Adapter statistics
 */
typedef struct {
    /* Processing counts */
    uint64_t utterances_processed;   /**< Total utterances */
    uint64_t words_processed;        /**< Total words */
    uint64_t phonemes_generated;     /**< Total phonemes */
    uint64_t commands_generated;     /**< Total motor commands */

    /* Success/failure */
    uint64_t successful_productions; /**< Successful utterances */
    uint64_t syntax_errors;          /**< Syntax failures */
    uint64_t phonological_errors;    /**< Phonological failures */
    uint64_t motor_errors;           /**< Motor planning failures */
    uint64_t lexicon_misses;         /**< Words not in lexicon */

    /* Timing */
    float avg_latency_ms;            /**< Average processing latency */
    float max_latency_ms;            /**< Maximum latency observed */

    /* Training */
    uint64_t training_iterations;    /**< Training updates */
    float training_loss;             /**< Current training loss */
} broca_stats_t;

/*=============================================================================
 * CALLBACK TYPES
 *===========================================================================*/

/**
 * @brief Callback for lexical access (integration with external lexicon)
 */
typedef bool (*broca_lexical_callback_t)(
    uint32_t word_id,
    const char* word,
    broca_lexical_entry_t* entry,
    void* user_data
);

/**
 * @brief Callback for motor output (integration with motor cortex)
 */
typedef void (*broca_motor_callback_t)(
    const broca_output_command_t* command,
    void* user_data
);

/**
 * @brief Callback for event notification
 */
typedef void (*broca_event_callback_t)(
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
 * WHAT: Returns default configuration for Broca's region adapter
 * WHY:  Provide sensible defaults for common use cases
 * HOW:  Initialize all fields with biologically-motivated values
 *
 * @return Default configuration structure
 */
broca_config_t broca_default_config(void);

/**
 * @brief Create Broca's region adapter
 *
 * WHAT: Allocate and initialize the adapter with all sub-modules
 * WHY:  Central point for language production initialization
 * HOW:  Create syntax, phonological, and motor processors; initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New adapter instance, or NULL on failure
 */
broca_adapter_t* broca_create(const broca_config_t* config);

/**
 * @brief Destroy Broca's region adapter
 *
 * WHAT: Free all resources associated with the adapter
 * WHY:  Prevent memory leaks
 * HOW:  Destroy sub-modules, free buffers and lexicon
 *
 * @param adapter Adapter to destroy
 */
void broca_destroy(broca_adapter_t* adapter);

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
bool broca_reset(broca_adapter_t* adapter);

/*=============================================================================
 * LEXICON MANAGEMENT
 *===========================================================================*/

/**
 * @brief Add entry to internal lexicon
 *
 * WHAT: Register word-to-phoneme mapping
 * WHY:  Build vocabulary for speech production
 * HOW:  Store entry in lexicon hash table
 *
 * @param adapter Adapter instance
 * @param entry Lexical entry to add
 * @return true on success, false if lexicon full
 */
bool broca_add_lexical_entry(broca_adapter_t* adapter,
                              const broca_lexical_entry_t* entry);

/**
 * @brief Phase 2C shim: add a lexical entry from individual fields.
 *
 * Used by the cascade's GL→Broca lexicon mirror so callers in the
 * language subsystem don't have to include broca's full header.
 * adapter_v is a void* aliasing broca_adapter_t* (avoids a forward decl
 * cascade). word is copied; phonemes are left empty (Broca's
 * phonological_processor generates them on-demand from spelling).
 *
 * Returns false on null/empty word, lexicon at capacity, or alloc
 * failure. Otherwise true.
 */
bool broca_add_lexical_entry_v(void* adapter_v,
                                uint32_t word_id,
                                const char* word,
                                uint8_t pos,
                                float frequency);

/**
 * @brief Look up word in lexicon
 *
 * WHAT: Retrieve phoneme sequence for word
 * WHY:  Convert semantic representation to phonological form
 * HOW:  Hash lookup, fallback to callback if not found
 *
 * @param adapter Adapter instance
 * @param word_id Word identifier
 * @param word Word string (used if word_id is 0)
 * @param entry Output entry (filled on success)
 * @return true if found, false if not in lexicon
 */
bool broca_lookup_word(const broca_adapter_t* adapter,
                        uint32_t word_id,
                        const char* word,
                        broca_lexical_entry_t* entry);

/**
 * @brief Set external lexical access callback
 *
 * WHAT: Register callback for lexicon lookups
 * WHY:  Allow integration with external word databases
 * HOW:  Store callback, call when internal lookup fails
 *
 * @param adapter Adapter instance
 * @param callback Lexical access function
 * @param user_data User context passed to callback
 * @return true on success
 */
bool broca_set_lexical_callback(broca_adapter_t* adapter,
                                 broca_lexical_callback_t callback,
                                 void* user_data);

/*=============================================================================
 * PRODUCTION PIPELINE
 *===========================================================================*/

/**
 * @brief Begin new utterance production
 *
 * WHAT: Initialize pipeline for new utterance
 * WHY:  Clear previous state, prepare buffers
 * HOW:  Reset sub-modules, set status to idle
 *
 * @param adapter Adapter instance
 * @return true on success
 */
bool broca_begin_utterance(broca_adapter_t* adapter);

/**
 * @brief Add word to current utterance
 *
 * WHAT: Queue word for syntactic processing
 * WHY:  Build up utterance incrementally
 * HOW:  Look up in lexicon, add to syntax buffer
 *
 * @param adapter Adapter instance
 * @param word Input word specification
 * @return true on success, false if buffer full or word not found
 */
bool broca_add_word(broca_adapter_t* adapter, const broca_input_word_t* word);

/**
 * @brief Process complete utterance through pipeline
 *
 * WHAT: Run full production pipeline (syntax → phonology → motor)
 * WHY:  Generate motor commands from word sequence
 * HOW:  Build syntax tree, plan phonemes, generate motor commands
 *
 * @param adapter Adapter instance
 * @param result Output result structure (optional, can be NULL)
 * @return true on success, false on any pipeline failure
 */
bool broca_process_utterance(broca_adapter_t* adapter,
                              broca_utterance_result_t* result);

/**
 * @brief Get next motor command
 *
 * WHAT: Retrieve next command from output queue
 * WHY:  Feed motor cortex incrementally
 * HOW:  Pop from command queue
 *
 * @param adapter Adapter instance
 * @param command Output command (filled on success)
 * @return true if command available, false if queue empty
 */
bool broca_get_next_command(broca_adapter_t* adapter,
                             broca_output_command_t* command);

/**
 * @brief Get all motor commands
 *
 * WHAT: Retrieve all commands from output queue
 * WHY:  Batch retrieval for offline processing
 * HOW:  Copy entire queue to output buffer
 *
 * @param adapter Adapter instance
 * @param commands Output buffer (must be pre-allocated)
 * @param count Input: buffer capacity; Output: actual count
 * @return true on success
 */
bool broca_get_all_commands(broca_adapter_t* adapter,
                             broca_output_command_t* commands,
                             uint32_t* count);

/*=============================================================================
 * HIGH-LEVEL CONVENIENCE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Produce utterance from word IDs
 *
 * WHAT: Complete production from word ID array
 * WHY:  Simple API for common case
 * HOW:  Begin, add each word, process, return success
 *
 * @param adapter Adapter instance
 * @param word_ids Array of word IDs
 * @param num_words Number of words
 * @param result Output result (optional)
 * @return true on success
 */
bool broca_produce_from_ids(broca_adapter_t* adapter,
                             const uint32_t* word_ids,
                             uint32_t num_words,
                             broca_utterance_result_t* result);

/**
 * @brief Produce utterance from word strings
 *
 * WHAT: Complete production from word string array
 * WHY:  Simple API for text input
 * HOW:  Look up each word, build utterance, process
 *
 * @param adapter Adapter instance
 * @param words Array of word strings
 * @param num_words Number of words
 * @param result Output result (optional)
 * @return true on success
 */
bool broca_produce_from_strings(broca_adapter_t* adapter,
                                 const char* const* words,
                                 uint32_t num_words,
                                 broca_utterance_result_t* result);

/*=============================================================================
 * WORKING MEMORY INTEGRATION
 *===========================================================================*/

/**
 * @brief Push word to working memory
 *
 * WHAT: Store word in working memory buffer
 * WHY:  Enable rehearsal and manipulation
 * HOW:  Add to circular buffer with decay
 *
 * @param adapter Adapter instance
 * @param word_id Word to remember
 * @return true on success, false if WM full
 */
bool broca_wm_push(broca_adapter_t* adapter, uint32_t word_id);

/**
 * @brief Pop word from working memory
 *
 * WHAT: Retrieve and remove word from WM
 * WHY:  Serial recall, sentence building
 * HOW:  Remove from buffer
 *
 * @param adapter Adapter instance
 * @param word_id Output word ID
 * @return true if word available, false if empty
 */
bool broca_wm_pop(broca_adapter_t* adapter, uint32_t* word_id);

/**
 * @brief Get working memory contents
 *
 * WHAT: Retrieve current WM buffer
 * WHY:  Inspection, debugging
 * HOW:  Copy buffer to output
 *
 * @param adapter Adapter instance
 * @param word_ids Output buffer
 * @param count Input: buffer capacity; Output: actual count
 * @return true on success
 */
bool broca_wm_get_contents(const broca_adapter_t* adapter,
                            uint32_t* word_ids,
                            uint32_t* count);

/*=============================================================================
 * EVENT INTEGRATION
 *===========================================================================*/

/**
 * @brief Set event callback
 *
 * WHAT: Register callback for production events
 * WHY:  Allow external monitoring and reaction
 * HOW:  Store callback, invoke on significant events
 *
 * Event types:
 * - BROCA_EVENT_WORD_SELECTED (word chosen from lexicon)
 * - BROCA_EVENT_SYNTAX_COMPLETE (tree built)
 * - BROCA_EVENT_PHONEMES_READY (phoneme sequence planned)
 * - BROCA_EVENT_MOTOR_READY (commands generated)
 * - BROCA_EVENT_ERROR (production failed)
 *
 * @param adapter Adapter instance
 * @param callback Event handler function
 * @param user_data User context passed to callback
 * @return true on success
 */
bool broca_set_event_callback(broca_adapter_t* adapter,
                               broca_event_callback_t callback,
                               void* user_data);

/*=============================================================================
 * TRAINING INTERFACE
 *===========================================================================*/

/**
 * @brief Provide feedback for training
 *
 * WHAT: Supply error signal for learning
 * WHY:  Enable supervised learning of production
 * HOW:  Compute gradients, update weights
 *
 * @param adapter Adapter instance
 * @param target_phonemes Expected phoneme sequence
 * @param num_phonemes Number of target phonemes
 * @param learning_rate Learning rate (0 = use config default)
 * @return true on success
 */
bool broca_train_phonemes(broca_adapter_t* adapter,
                           const uint8_t* target_phonemes,
                           uint32_t num_phonemes,
                           float learning_rate);

/**
 * @brief Train on word-phoneme pair
 *
 * WHAT: Learn association between word and phonemes
 * WHY:  Expand vocabulary through exposure
 * HOW:  Add to lexicon if new, reinforce if existing
 *
 * @param adapter Adapter instance
 * @param word Word string
 * @param phonemes Phoneme sequence
 * @param num_phonemes Number of phonemes
 * @return true on success
 */
bool broca_train_word(broca_adapter_t* adapter,
                       const char* word,
                       const uint8_t* phonemes,
                       uint32_t num_phonemes);

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

/**
 * @brief Get current processing status
 *
 * @param adapter Adapter instance
 * @return Current status
 */
broca_status_t broca_get_status(const broca_adapter_t* adapter);

/**
 * @brief Get last error code
 *
 * @param adapter Adapter instance
 * @return Last error, or BROCA_ERROR_NONE
 */
broca_error_t broca_get_last_error(const broca_adapter_t* adapter);

/**
 * @brief Get error description string
 *
 * @param error Error code
 * @return Human-readable error description
 */
const char* broca_error_string(broca_error_t error);

/**
 * @brief Get status description string
 *
 * @param status Status code
 * @return Human-readable status description
 */
const char* broca_status_string(broca_status_t status);

/**
 * @brief Get adapter statistics
 *
 * @param adapter Adapter instance
 * @param stats Output statistics structure
 * @return true on success
 */
bool broca_get_stats(const broca_adapter_t* adapter, broca_stats_t* stats);

/**
 * @brief Get adapter configuration
 *
 * @param adapter Adapter instance
 * @param config Output configuration structure
 * @return true on success
 */
bool broca_get_config(const broca_adapter_t* adapter, broca_config_t* config);

/*=============================================================================
 * SUB-MODULE ACCESS (Advanced)
 *===========================================================================*/

/**
 * @brief Get syntax processor handle
 *
 * WHAT: Access underlying syntax processor
 * WHY:  Advanced configuration, direct manipulation
 * HOW:  Return internal pointer (still owned by adapter)
 *
 * @param adapter Adapter instance
 * @return Syntax processor, or NULL
 */
syntax_processor_t* broca_get_syntax_processor(broca_adapter_t* adapter);

/**
 * @brief Get phonological processor handle
 *
 * @param adapter Adapter instance
 * @return Phonological processor, or NULL
 */
phonological_processor_t* broca_get_phonological_processor(broca_adapter_t* adapter);

/**
 * @brief Get speech motor planner handle
 *
 * @param adapter Adapter instance
 * @return Speech motor planner, or NULL
 */
speech_motor_planner_t* broca_get_speech_motor_planner(broca_adapter_t* adapter);

/*=============================================================================
 * BIO-ASYNC COMMUNICATION
 *===========================================================================*/

/**
 * @brief Get bio-async module context
 *
 * WHAT: Returns the bio-async module context for Broca's region
 * WHY:  Allow external modules to send messages to Broca
 * HOW:  Returns internal bio_module_context_t
 *
 * @param adapter Adapter instance
 * @return Bio-async module context, or NULL if not enabled
 */
bio_module_context_t broca_get_bio_context(broca_adapter_t* adapter);

/**
 * @brief Process pending bio-async messages
 *
 * WHAT: Process messages in Broca's inbox
 * WHY:  Handle incoming requests from other modules
 * HOW:  Calls bio_router_process_inbox and invokes handlers
 *
 * @param adapter Adapter instance
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t broca_process_bio_messages(broca_adapter_t* adapter, uint32_t max_messages);

/**
 * @brief Request lexical access asynchronously
 *
 * WHAT: Send lexical access request via bio-async
 * WHY:  Async communication with external lexicon (Wernicke's area)
 * HOW:  Sends BIO_MSG_LEXICAL_ACCESS_REQUEST, returns future
 *
 * @param adapter Adapter instance
 * @param word_id Word ID (0 = use string)
 * @param word Word string (if word_id = 0)
 * @return Future for lexical response, or NULL on failure
 */
nimcp_bio_future_t broca_request_lexical_access_async(
    broca_adapter_t* adapter,
    uint32_t word_id,
    const char* word
);

/**
 * @brief Request syntax parsing asynchronously
 *
 * WHAT: Send syntax parse request via bio-async
 * WHY:  Allow external syntax validation or parallel processing
 * HOW:  Sends BIO_MSG_SYNTAX_PARSE_REQUEST, returns future
 *
 * @param adapter Adapter instance
 * @param word_ids Word sequence to parse
 * @param word_count Number of words
 * @return Future for parse result, or NULL on failure
 */
nimcp_bio_future_t broca_request_syntax_parse_async(
    broca_adapter_t* adapter,
    const uint32_t* word_ids,
    uint8_t word_count
);

/**
 * @brief Request motor command generation asynchronously
 *
 * WHAT: Send motor command request via bio-async
 * WHY:  Communicate with motor cortex for articulation
 * HOW:  Sends BIO_MSG_MOTOR_COMMAND_REQUEST, returns future
 *
 * @param adapter Adapter instance
 * @param phoneme Target phoneme
 * @param duration_ms Target duration
 * @param pitch_hz Target pitch
 * @return Future for motor command result, or NULL on failure
 */
nimcp_bio_future_t broca_request_motor_command_async(
    broca_adapter_t* adapter,
    uint8_t phoneme,
    float duration_ms,
    float pitch_hz
);

/**
 * @brief Broadcast utterance production complete
 *
 * WHAT: Notify all modules that utterance production is done
 * WHY:  Allow speech cortex, feedback systems to sync
 * HOW:  Broadcasts BIO_MSG_UTTERANCE_PRODUCTION_COMPLETE
 *
 * @param adapter Adapter instance
 * @param result Utterance result to broadcast
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t broca_broadcast_utterance_complete(
    broca_adapter_t* adapter,
    const broca_utterance_result_t* result
);

/**
 * @brief Handle incoming speech feedback
 *
 * WHAT: Process feedback from speech perception
 * WHY:  Enable auditory feedback loop for self-monitoring
 * HOW:  Callback invoked by bio-async on BIO_MSG_SPEECH_FEEDBACK
 *
 * @param adapter Adapter instance
 * @param phoneme_id Recognized phoneme
 * @param confidence Recognition confidence
 * @param timing_error Timing error (ms) from expected
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t broca_handle_speech_feedback(
    broca_adapter_t* adapter,
    uint8_t phoneme_id,
    float confidence,
    float timing_error
);

/*=============================================================================
 * SNN SUBSTRATE BINDING (broca_substrate population)
 *===========================================================================*/

/**
 * @brief Attach the SNN substrate population that backs Broca's area.
 *
 * WHAT: Stores a reference to the SNN network + the population id of the
 *       broca_substrate pop (created by nimcp_brain_factory_init_language_pops).
 * WHY:  Production / motor-planning ticks need to read spike rates from the
 *       substrate pop (drive level) and write back excitatory bias when the
 *       adapter wants to bias word selection. Without this binding the
 *       64K-neuron pop is floating — it spikes from hierarchy taps but the
 *       adapter logic can't observe or steer it.
 * HOW:  Adapter caches (snn, pop_id). Lookup helpers in the adapter use the
 *       pop id directly; pop_id < 0 = unbound (no-op). Idempotent — safe to
 *       call multiple times.
 *
 * @param adapter Broca adapter instance
 * @param snn     SNN network owning the substrate pop (may be NULL → unbind)
 * @param pop_id  Pop id from snn_network_add_population_lightweight (< 0 = unbind)
 * @return true on success, false if adapter is NULL
 */
bool broca_attach_snn_pop(
    broca_adapter_t* adapter,
    snn_network_t* snn,
    int pop_id
);

/**
 * @brief Get the bound SNN population id.
 *
 * @param adapter Broca adapter instance
 * @return Pop id (>= 0) if bound, -1 if unbound or adapter is NULL
 */
int broca_get_snn_pop_id(const broca_adapter_t* adapter);

/**
 * @brief Get the bound SNN network handle.
 *
 * @param adapter Broca adapter instance
 * @return SNN network pointer if bound, NULL otherwise
 */
snn_network_t* broca_get_snn_network(const broca_adapter_t* adapter);

/*=============================================================================
 * GROUNDED-LANGUAGE BINDING
 *===========================================================================*/

/**
 * @brief Attach the grounded_language handle. Used by future production-
 *        path code that wants to fall through to GL when the broca
 *        local lexicon misses. Stored as opaque void*. NULL = unbind.
 *
 * @param adapter Broca adapter instance
 * @param gl      grounded_language_t* (opaque)
 * @return true on success, false if adapter is NULL
 */
bool broca_attach_grounded_language(broca_adapter_t* adapter, void* gl);

/** @brief Get the bound grounded_language handle (NULL if unbound). */
void* broca_get_grounded_language(const broca_adapter_t* adapter);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BROCA_ADAPTER_H */
