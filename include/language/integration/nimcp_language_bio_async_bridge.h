/**
 * @file nimcp_language_bio_async_bridge.h
 * @brief Language Module Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Central bio-async integration for language processing that provides
 *       comprehensive message routing for Broca, Wernicke, semantics, syntax,
 *       and other language regions via the bio-router.
 *
 * WHY: The language network processes all linguistic input/output and needs to:
 *      - Coordinate comprehension (Wernicke) and production (Broca) pathways
 *      - Route semantic activation events to cognitive systems
 *      - Broadcast syntactic parse results to working memory
 *      - Coordinate phonological encoding/decoding with auditory cortex
 *      - Handle anomaly detection (N400/P600) signals
 *
 * HOW: Registers language as a bio-router module, maintains subscription
 *      registry, provides typed message broadcast APIs, and processes incoming
 *      perception and cognitive requests.
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * LANGUAGE OUTPUT PATHWAYS:
 * -------------------------
 * 1. Comprehension pathway (Wernicke -> distributed):
 *    - Word recognition events to lexical memory
 *    - Concept activation to semantic memory (ATL)
 *    - Parse results to prefrontal cortex
 *    - Mapped to: LANG_MSG_WORD_RECOGNIZED, LANG_MSG_CONCEPT_ACTIVATED
 *
 * 2. Production pathway (Broca -> motor):
 *    - Phonological encoding to motor cortex
 *    - Articulatory planning to premotor areas
 *    - Speech timing to basal ganglia
 *    - Mapped to: LANG_MSG_PRODUCTION_STARTED, LANG_MSG_UTTERANCE_READY
 *
 * 3. Error/anomaly signals:
 *    - N400-like semantic violations to attention systems
 *    - P600-like syntactic violations to prefrontal cortex
 *    - Ambiguity signals to cognitive control
 *    - Mapped to: LANG_MSG_SEMANTIC_ANOMALY, LANG_MSG_SYNTACTIC_ANOMALY
 *
 * LANGUAGE INPUT PATHWAYS:
 * ------------------------
 * 1. Perceptual (auditory/visual):
 *    - Phoneme sequences from auditory cortex
 *    - Orthographic input from visual cortex
 *
 * 2. Cognitive (top-down):
 *    - Context from prefrontal cortex
 *    - Attention modulation from parietal cortex
 *
 * 3. Motor feedback:
 *    - Articulation feedback from motor cortex
 *    - Timing from basal ganglia
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_LANGUAGE_BIO_ASYNC_BRIDGE_H
#define NIMCP_LANGUAGE_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module dependencies */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "language/nimcp_language_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Module ID for language bridge in bio-async system (0x0810 range) */
#define BIO_MODULE_ID_LANGUAGE_BRIDGE       0x0817

/** Maximum number of module subscriptions */
#define LANG_BIO_MAX_SUBSCRIPTIONS          64

/** Maximum pending messages in inbox */
#define LANG_BIO_MAX_INBOX_SIZE             256

/** Maximum pending messages in outbox */
#define LANG_BIO_MAX_OUTBOX_SIZE            128

/** Default broadcast interval for language state (ms) */
#define LANG_BIO_DEFAULT_BROADCAST_INTERVAL_MS  100

/** Message expiry time (ms) */
#define LANG_BIO_MESSAGE_TTL_MS             5000

/** Maximum word form length */
#define LANG_BIO_MAX_WORD_LEN               64

/** Maximum concept name length */
#define LANG_BIO_MAX_CONCEPT_NAME_LEN       64

/** Maximum error message length */
#define LANG_BIO_MAX_ERROR_MSG_LEN          128

/** Anomaly threshold for urgent notification */
#define LANG_BIO_ANOMALY_URGENCY_THRESHOLD  0.7f

/* ============================================================================
 * Error Codes
 * ============================================================================ */

/** Language bio-async error code base */
#define LANG_BIO_ERROR_BASE                 0x0900

/** Bridge not initialized */
#define LANG_BIO_ERROR_NOT_INITIALIZED      (LANG_BIO_ERROR_BASE + 1)

/** Bridge not connected */
#define LANG_BIO_ERROR_NOT_CONNECTED        (LANG_BIO_ERROR_BASE + 2)

/** Invalid parameter */
#define LANG_BIO_ERROR_INVALID_PARAM        (LANG_BIO_ERROR_BASE + 3)

/** Subscription limit reached */
#define LANG_BIO_ERROR_SUBSCRIPTION_FULL    (LANG_BIO_ERROR_BASE + 4)

/** Module not found */
#define LANG_BIO_ERROR_MODULE_NOT_FOUND     (LANG_BIO_ERROR_BASE + 5)

/** Message routing failed */
#define LANG_BIO_ERROR_ROUTING_FAILED       (LANG_BIO_ERROR_BASE + 6)

/** Memory allocation failed */
#define LANG_BIO_ERROR_MEMORY               (LANG_BIO_ERROR_BASE + 7)

/** Handler registration failed */
#define LANG_BIO_ERROR_HANDLER_FAILED       (LANG_BIO_ERROR_BASE + 8)

/* ============================================================================
 * Message Types
 * ============================================================================ */

/**
 * @brief Language bio-async message types
 *
 * WHAT: Message type enumeration for language bio-async routing
 * WHY:  Enables typed message handling and subscription filtering
 * HOW:  Each type corresponds to a specific language processing event
 */
typedef enum {
    /* Utterance lifecycle messages */
    LANG_MSG_UTTERANCE_START = 0,           /**< Utterance processing started */
    LANG_MSG_UTTERANCE_END,                 /**< Utterance processing ended */

    /* Comprehension messages */
    LANG_MSG_COMPREHENSION_START,           /**< Comprehension processing started */
    LANG_MSG_PHONEME_RECOGNIZED,            /**< Phoneme recognized */
    LANG_MSG_WORD_RECOGNIZED,               /**< Word recognized */
    LANG_MSG_CONCEPT_ACTIVATED,             /**< Concept activated in semantic memory */
    LANG_MSG_PARSE_UPDATE,                  /**< Syntactic parse update */
    LANG_MSG_COMPREHENSION_COMPLETE,        /**< Comprehension finished */

    /* Production messages */
    LANG_MSG_PRODUCTION_REQUEST,            /**< Request to produce utterance */
    LANG_MSG_PRODUCTION_STARTED,            /**< Production planning started */
    LANG_MSG_WORD_PLANNED,                  /**< Word planned for production */
    LANG_MSG_PHONEME_ENCODED,               /**< Phoneme encoded for motor output */
    LANG_MSG_MOTOR_COMMAND,                 /**< Motor command generated */
    LANG_MSG_UTTERANCE_READY,               /**< Utterance ready for articulation */
    LANG_MSG_PRODUCTION_COMPLETE,           /**< Production finished */

    /* Anomaly/error messages */
    LANG_MSG_SEMANTIC_ANOMALY,              /**< N400-like semantic violation */
    LANG_MSG_SYNTACTIC_ANOMALY,             /**< P600-like syntactic violation */
    LANG_MSG_AMBIGUITY_DETECTED,            /**< Ambiguous input detected */
    LANG_MSG_GARDEN_PATH,                   /**< Garden path sentence detected */

    /* State and control messages */
    LANG_MSG_STATE_CHANGE,                  /**< Language state machine transition */
    LANG_MSG_MODE_CHANGE,                   /**< Processing mode changed */
    LANG_MSG_ERROR,                         /**< Processing error occurred */

    /* Inter-region coordination messages */
    LANG_MSG_BROCA_WERNICKE_SYNC,           /**< Broca-Wernicke synchronization */
    LANG_MSG_ARCUATE_TRANSFER,              /**< Arcuate fasciculus data transfer */
    LANG_MSG_SEMANTIC_BROADCAST,            /**< Broadcast to semantic regions */
    LANG_MSG_SYNTACTIC_BROADCAST,           /**< Broadcast to syntax regions */
    LANG_MSG_PHONOLOGICAL_BROADCAST,        /**< Broadcast to phonological regions */

    /* Learning/training messages */
    LANG_MSG_WORD_LEARNED,                  /**< New word learned */
    LANG_MSG_GRAMMAR_LEARNED,               /**< Grammar pattern learned */
    LANG_MSG_TRAINING_UPDATE,               /**< Training parameters updated */

    /* External coordination messages */
    LANG_MSG_PERCEPTION_INPUT,              /**< Input from perception bridge */
    LANG_MSG_COGNITIVE_REQUEST,             /**< Request from cognitive systems */
    LANG_MSG_MOTOR_FEEDBACK,                /**< Feedback from motor systems */
    LANG_MSG_ATTENTION_MODULATION,          /**< Attention modulation request */

    LANG_MSG_COUNT
} lang_bio_msg_type_t;

/**
 * @brief Bitmask for message type subscriptions
 */
#define LANG_BIO_SUB_UTTERANCE_START        (1ULL << LANG_MSG_UTTERANCE_START)
#define LANG_BIO_SUB_UTTERANCE_END          (1ULL << LANG_MSG_UTTERANCE_END)
#define LANG_BIO_SUB_COMPREHENSION_START    (1ULL << LANG_MSG_COMPREHENSION_START)
#define LANG_BIO_SUB_PHONEME_RECOGNIZED     (1ULL << LANG_MSG_PHONEME_RECOGNIZED)
#define LANG_BIO_SUB_WORD_RECOGNIZED        (1ULL << LANG_MSG_WORD_RECOGNIZED)
#define LANG_BIO_SUB_CONCEPT_ACTIVATED      (1ULL << LANG_MSG_CONCEPT_ACTIVATED)
#define LANG_BIO_SUB_PARSE_UPDATE           (1ULL << LANG_MSG_PARSE_UPDATE)
#define LANG_BIO_SUB_COMPREHENSION_COMPLETE (1ULL << LANG_MSG_COMPREHENSION_COMPLETE)
#define LANG_BIO_SUB_PRODUCTION_REQUEST     (1ULL << LANG_MSG_PRODUCTION_REQUEST)
#define LANG_BIO_SUB_PRODUCTION_STARTED     (1ULL << LANG_MSG_PRODUCTION_STARTED)
#define LANG_BIO_SUB_PRODUCTION_COMPLETE    (1ULL << LANG_MSG_PRODUCTION_COMPLETE)
#define LANG_BIO_SUB_SEMANTIC_ANOMALY       (1ULL << LANG_MSG_SEMANTIC_ANOMALY)
#define LANG_BIO_SUB_SYNTACTIC_ANOMALY      (1ULL << LANG_MSG_SYNTACTIC_ANOMALY)
#define LANG_BIO_SUB_STATE_CHANGE           (1ULL << LANG_MSG_STATE_CHANGE)
#define LANG_BIO_SUB_ERROR                  (1ULL << LANG_MSG_ERROR)
#define LANG_BIO_SUB_SEMANTIC_BROADCAST     (1ULL << LANG_MSG_SEMANTIC_BROADCAST)
#define LANG_BIO_SUB_SYNTACTIC_BROADCAST    (1ULL << LANG_MSG_SYNTACTIC_BROADCAST)
#define LANG_BIO_SUB_ALL                    (0xFFFFFFFFFFFFFFFFULL)

/* ============================================================================
 * Language Region Identifiers
 * ============================================================================ */

/**
 * @brief Language region identifiers for inter-region messaging
 */
typedef enum {
    LANG_REGION_BROCA = 0,                  /**< Broca's area (BA44/45) */
    LANG_REGION_WERNICKE,                   /**< Wernicke's area (BA22) */
    LANG_REGION_STG,                        /**< Superior temporal gyrus (phonemes) */
    LANG_REGION_MTG,                        /**< Middle temporal gyrus (words) */
    LANG_REGION_ATL,                        /**< Anterior temporal lobe (concepts) */
    LANG_REGION_IFG,                        /**< Inferior frontal gyrus (syntax) */
    LANG_REGION_SMA,                        /**< Supplementary motor area (sequencing) */
    LANG_REGION_PREMOTOR,                   /**< Premotor cortex (articulation) */
    LANG_REGION_INSULA,                     /**< Insula (speech coordination) */
    LANG_REGION_ANGULAR_GYRUS,              /**< Angular gyrus (semantics) */
    LANG_REGION_ARCUATE,                    /**< Arcuate fasciculus (tract) */
    LANG_REGION_COUNT
} lang_region_id_t;

/* ============================================================================
 * Message Payload Structures
 * ============================================================================ */

/**
 * @brief Utterance start/end message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint64_t utterance_id;                  /**< Unique utterance identifier */
    language_input_type_t input_type;       /**< Type of input (audio/text/etc) */
    language_mode_t mode;                   /**< Processing mode */
    uint32_t expected_length;               /**< Expected word count (0 if unknown) */

    uint64_t timestamp_us;                  /**< Event timestamp */
} lang_bio_utterance_msg_t;

/**
 * @brief Phoneme recognition message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint32_t phoneme_id;                    /**< Recognized phoneme ID */
    phoneme_category_t category;            /**< Phoneme category */
    float confidence;                       /**< Recognition confidence [0, 1] */
    float duration_ms;                      /**< Phoneme duration */
    float formants[4];                      /**< Formant frequencies F1-F4 */

    bool is_word_boundary;                  /**< Word boundary marker */
    bool is_phrase_boundary;                /**< Phrase boundary marker */

    uint64_t utterance_id;                  /**< Parent utterance ID */
    uint32_t position;                      /**< Position in sequence */
    uint64_t timestamp_us;                  /**< Recognition timestamp */
} lang_bio_phoneme_msg_t;

/**
 * @brief Word recognition message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint32_t word_id;                       /**< Recognized word ID */
    char word_form[LANG_BIO_MAX_WORD_LEN];  /**< Orthographic form */
    part_of_speech_t pos;                   /**< Part of speech */
    float confidence;                       /**< Recognition confidence [0, 1] */
    float frequency;                        /**< Log word frequency */
    float activation;                       /**< Lexical activation level */

    uint32_t sense_id;                      /**< Selected sense (polysemy) */
    uint32_t num_senses;                    /**< Total senses available */

    uint64_t utterance_id;                  /**< Parent utterance ID */
    uint32_t position;                      /**< Word position in utterance */
    uint64_t timestamp_us;                  /**< Recognition timestamp */
} lang_bio_word_msg_t;

/**
 * @brief Concept activation message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint32_t concept_id;                    /**< Activated concept ID */
    char concept_name[LANG_BIO_MAX_CONCEPT_NAME_LEN]; /**< Concept name */
    float activation;                       /**< Activation level [0, 1] */
    float relevance;                        /**< Context relevance [0, 1] */
    thematic_role_t role;                   /**< Assigned thematic role */

    uint32_t source_word_id;                /**< Activating word (if any) */
    bool is_target;                         /**< Target of current utterance */

    float* semantic_vector;                 /**< Semantic embedding (optional) */
    uint32_t semantic_dim;                  /**< Embedding dimension */

    uint64_t utterance_id;                  /**< Parent utterance ID */
    uint64_t timestamp_us;                  /**< Activation timestamp */
} lang_bio_concept_msg_t;

/**
 * @brief Comprehension complete message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint64_t utterance_id;                  /**< Utterance ID */
    uint32_t word_count;                    /**< Words comprehended */
    uint32_t concept_count;                 /**< Concepts activated */

    float semantic_coherence;               /**< Semantic coherence score */
    float syntactic_score;                  /**< Syntactic wellformedness */
    float overall_confidence;               /**< Overall confidence */
    float processing_time_ms;               /**< Total processing time */

    /* Anomaly flags */
    float semantic_anomaly;                 /**< Semantic anomaly magnitude */
    float syntactic_anomaly;                /**< Syntactic anomaly magnitude */

    uint64_t timestamp_us;                  /**< Completion timestamp */
} lang_bio_comprehension_complete_msg_t;

/**
 * @brief Production request message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint64_t request_id;                    /**< Unique request identifier */
    language_output_type_t output_type;     /**< Desired output type */

    float* semantic_input;                  /**< Semantic vector to verbalize */
    uint32_t semantic_dim;                  /**< Semantic dimension */

    /* Optional constraints */
    uint32_t max_words;                     /**< Maximum word count */
    float urgency;                          /**< Production urgency [0, 1] */

    uint64_t timestamp_us;                  /**< Request timestamp */
} lang_bio_production_request_msg_t;

/**
 * @brief Production complete message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint64_t request_id;                    /**< Original request ID */
    uint32_t word_count;                    /**< Words produced */
    uint32_t phoneme_count;                 /**< Phonemes produced */

    float fluency_score;                    /**< Fluency score */
    float planning_time_ms;                 /**< Planning time */
    float articulation_time_ms;             /**< Articulation time */

    uint64_t timestamp_us;                  /**< Completion timestamp */
} lang_bio_production_complete_msg_t;

/**
 * @brief Anomaly detection message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    bool is_semantic;                       /**< Semantic (N400) vs syntactic (P600) */
    float anomaly_magnitude;                /**< Violation magnitude [0, 1] */
    float surprise;                         /**< Surprise/unexpectedness */

    uint32_t word_position;                 /**< Position of anomalous word */
    uint32_t word_id;                       /**< Anomalous word ID */
    uint32_t expected_id;                   /**< Expected word/concept ID */

    uint64_t utterance_id;                  /**< Parent utterance ID */
    uint64_t timestamp_us;                  /**< Detection timestamp */
} lang_bio_anomaly_msg_t;

/**
 * @brief State change message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    language_state_t old_state;             /**< Previous state */
    language_state_t new_state;             /**< New state */
    language_mode_t mode;                   /**< Current mode */

    uint64_t timestamp_us;                  /**< Transition timestamp */
} lang_bio_state_change_msg_t;

/**
 * @brief Error message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    int32_t error_code;                     /**< Error code */
    char message[LANG_BIO_MAX_ERROR_MSG_LEN]; /**< Error message */

    uint64_t utterance_id;                  /**< Associated utterance (if any) */
    lang_region_id_t source_region;         /**< Region that generated error */

    uint64_t timestamp_us;                  /**< Error timestamp */
} lang_bio_error_msg_t;

/**
 * @brief Inter-region synchronization message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    lang_region_id_t source_region;         /**< Source region */
    lang_region_id_t target_region;         /**< Target region */
    uint32_t sync_type;                     /**< Type of synchronization */

    float* data;                            /**< Transfer data */
    uint32_t data_size;                     /**< Data size in bytes */

    uint64_t utterance_id;                  /**< Associated utterance */
    uint64_t timestamp_us;                  /**< Sync timestamp */
} lang_bio_region_sync_msg_t;

/**
 * @brief Semantic broadcast message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint32_t concept_count;                 /**< Number of concepts */
    uint32_t* concept_ids;                  /**< Concept ID array */
    float* activations;                     /**< Activation levels */

    float context_coherence;                /**< Context coherence */
    uint32_t topic_id;                      /**< Current topic */

    uint64_t utterance_id;                  /**< Associated utterance */
    uint64_t timestamp_us;                  /**< Broadcast timestamp */
} lang_bio_semantic_broadcast_msg_t;

/**
 * @brief Syntactic broadcast message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    parse_state_t parse_state;              /**< Current parse state */
    phrase_type_t current_phrase;           /**< Current phrase type */
    uint32_t parse_depth;                   /**< Current parse depth */

    float parse_probability;                /**< Parse probability */
    bool is_complete;                       /**< Parse complete flag */

    uint64_t utterance_id;                  /**< Associated utterance */
    uint64_t timestamp_us;                  /**< Broadcast timestamp */
} lang_bio_syntactic_broadcast_msg_t;

/**
 * @brief Training update message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    float vocabulary_lr;                    /**< Vocabulary learning rate */
    float grammar_weight;                   /**< Grammar learning weight */
    float phoneme_plasticity;               /**< Phoneme plasticity */

    uint64_t training_step;                 /**< Current training step */
    uint64_t timestamp_us;                  /**< Update timestamp */
} lang_bio_training_msg_t;

/* ============================================================================
 * Subscription Structure
 * ============================================================================ */

/**
 * @brief Module subscription entry
 */
typedef struct {
    bio_module_id_t module_id;              /**< Subscribed module ID */
    uint64_t msg_type_mask;                 /**< Bitmask of subscribed types */
    bool active;                            /**< Subscription active */
    uint64_t subscription_time;             /**< When subscribed */
    uint64_t messages_sent;                 /**< Messages sent to this sub */
} lang_bio_subscription_t;

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

/**
 * @brief Language bio-async bridge configuration
 */
typedef struct {
    /* Broadcast timing */
    uint32_t comprehension_broadcast_interval_ms; /**< Comprehension state broadcast */
    uint32_t production_broadcast_interval_ms;    /**< Production state broadcast */
    bool enable_auto_broadcast;                   /**< Auto-broadcast language state */

    /* Message handling */
    uint32_t max_inbox_process_per_update;        /**< Max inbox messages per update */
    uint32_t message_ttl_ms;                      /**< Message time-to-live */

    /* Priority settings */
    float anomaly_urgency_threshold;              /**< Threshold for urgent anomaly messages */
    nimcp_bio_channel_type_t default_channel;     /**< Default channel */
    nimcp_bio_channel_type_t urgent_channel;      /**< Channel for urgent messages */
    nimcp_bio_channel_type_t semantic_channel;    /**< Channel for semantic messages */

    /* Subscription limits */
    uint32_t max_subscriptions;                   /**< Maximum module subscriptions */

    /* Feature flags */
    bool enable_comprehension_routing;            /**< Enable comprehension message routing */
    bool enable_production_routing;               /**< Enable production message routing */
    bool enable_anomaly_routing;                  /**< Enable anomaly message routing */
    bool enable_semantic_broadcast;               /**< Enable semantic broadcasts */
    bool enable_syntactic_broadcast;              /**< Enable syntactic broadcasts */
    bool enable_phonological_broadcast;           /**< Enable phonological broadcasts */
    bool enable_region_sync;                      /**< Enable inter-region sync */
    bool enable_logging;                          /**< Enable message logging */
} language_bio_bridge_config_t;

/* ============================================================================
 * Bridge Statistics
 * ============================================================================ */

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Message counts */
    uint64_t messages_sent;                       /**< Total messages sent */
    uint64_t messages_received;                   /**< Total messages received */
    uint64_t messages_dropped;                    /**< Messages dropped (queue full) */
    uint64_t broadcasts_sent;                     /**< Broadcast messages sent */

    /* Per-type counts */
    uint64_t utterances_started;                  /**< Utterance start events */
    uint64_t utterances_completed;                /**< Utterance completions */
    uint64_t words_recognized;                    /**< Word recognition events */
    uint64_t concepts_activated;                  /**< Concept activation events */
    uint64_t productions_completed;               /**< Production completions */
    uint64_t semantic_anomalies;                  /**< Semantic anomaly events */
    uint64_t syntactic_anomalies;                 /**< Syntactic anomaly events */
    uint64_t errors_reported;                     /**< Error events */

    /* Subscription stats */
    uint32_t active_subscriptions;                /**< Currently active subs */
    uint32_t peak_subscriptions;                  /**< Peak subscription count */

    /* Timing stats */
    uint64_t last_broadcast_time_us;              /**< Last broadcast timestamp */
    float avg_message_latency_us;                 /**< Average message latency */
    float max_message_latency_us;                 /**< Peak message latency */

    /* Error counts */
    uint64_t handler_errors;                      /**< Message handler errors */
    uint64_t routing_errors;                      /**< Routing failures */
} language_bio_bridge_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Language bio-async bridge handle
 */
typedef struct language_bio_bridge_struct language_bio_bridge_t;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct language_orchestrator language_orchestrator_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Initialize configuration structure with sensible defaults
 * WHY:  Provide consistent baseline configuration
 * HOW:  Set all fields to recommended default values
 *
 * @param config Configuration structure to initialize
 * @return 0 on success, -1 on error
 */
int language_bio_bridge_default_config(language_bio_bridge_config_t* config);

/**
 * @brief Create language bio-async bridge
 *
 * WHAT: Allocate and initialize the language bio-async bridge
 * WHY:  Create central routing hub for language messages
 * HOW:  Allocate structure, initialize subscriptions, set defaults
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
language_bio_bridge_t* language_bio_bridge_create(
    const language_bio_bridge_config_t* config
);

/**
 * @brief Destroy language bio-async bridge
 *
 * WHAT: Cleanup and deallocate the bridge
 * WHY:  Release all resources properly
 * HOW:  Disconnect, free subscriptions, free structure
 *
 * @param bridge Bridge handle (NULL safe)
 */
void language_bio_bridge_destroy(language_bio_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect bridge to language orchestrator and router
 *
 * WHAT: Establish connection to language system and bio-router
 * WHY:  Enable message routing between language and other modules
 * HOW:  Store references, register with router, setup handlers
 *
 * @param bridge Bridge handle
 * @param orchestrator Language orchestrator instance
 * @param router Bio-router instance
 * @return 0 on success, -1 on error
 */
int language_bio_bridge_connect(
    language_bio_bridge_t* bridge,
    language_orchestrator_t* orchestrator,
    bio_router_t router
);

/**
 * @brief Disconnect bridge from router
 *
 * WHAT: Disconnect from bio-router
 * WHY:  Clean disconnection before shutdown
 * HOW:  Unregister handlers, clear references
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int language_bio_bridge_disconnect(language_bio_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool language_bio_bridge_is_connected(const language_bio_bridge_t* bridge);

/**
 * @brief Get module context from bridge
 *
 * WHAT: Get the bio-router module context
 * WHY:  Allow external callers to use router directly
 * HOW:  Return stored module context
 *
 * @param bridge Bridge handle
 * @return Module context or NULL if not connected
 */
bio_module_context_t language_bio_bridge_get_context(
    const language_bio_bridge_t* bridge
);

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

/**
 * @brief Process incoming messages from inbox
 *
 * WHAT: Process pending messages in the bridge inbox
 * WHY:  Handle incoming requests from other modules
 * HOW:  Dequeue messages, dispatch to handlers
 *
 * @param bridge Bridge handle
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed, or -1 on error
 */
int language_bio_bridge_process_inbox(
    language_bio_bridge_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Update bridge state and auto-broadcasts
 *
 * WHAT: Periodic update of bridge state
 * WHY:  Handle auto-broadcasts and housekeeping
 * HOW:  Check timers, send scheduled broadcasts
 *
 * @param bridge Bridge handle
 * @param delta_ms Time since last update
 * @return 0 on success, -1 on error
 */
int language_bio_bridge_update(
    language_bio_bridge_t* bridge,
    uint32_t delta_ms
);

/* ============================================================================
 * Utterance Coordination API
 * ============================================================================ */

/**
 * @brief Broadcast utterance start event
 *
 * WHAT: Notify all subscribers that utterance processing started
 * WHY:  Coordinate language processing across regions
 * HOW:  Create message, send to all subscribed modules
 *
 * @param bridge Bridge handle
 * @param utterance_id Unique utterance identifier
 * @param input_type Type of input
 * @param mode Processing mode
 * @return 0 on success, -1 on error
 */
int language_bio_bridge_broadcast_utterance_start(
    language_bio_bridge_t* bridge,
    uint64_t utterance_id,
    language_input_type_t input_type,
    language_mode_t mode
);

/**
 * @brief Broadcast utterance end event
 *
 * @param bridge Bridge handle
 * @param utterance_id Utterance identifier
 * @return 0 on success, -1 on error
 */
int language_bio_bridge_broadcast_utterance_end(
    language_bio_bridge_t* bridge,
    uint64_t utterance_id
);

/* ============================================================================
 * Comprehension Coordination API
 * ============================================================================ */

/**
 * @brief Broadcast phoneme recognition event
 *
 * WHAT: Notify subscribers of phoneme recognition
 * WHY:  Route phoneme events to interested regions
 * HOW:  Create phoneme message, broadcast to subscribers
 *
 * @param bridge Bridge handle
 * @param phoneme Phoneme information
 * @param utterance_id Parent utterance ID
 * @return 0 on success, -1 on error
 */
int language_bio_bridge_broadcast_phoneme(
    language_bio_bridge_t* bridge,
    const language_phoneme_t* phoneme,
    uint64_t utterance_id
);

/**
 * @brief Broadcast word recognition event
 *
 * WHAT: Notify subscribers of word recognition
 * WHY:  Route lexical events to semantic/syntactic regions
 * HOW:  Create word message, broadcast to subscribers
 *
 * @param bridge Bridge handle
 * @param word Word information
 * @param utterance_id Parent utterance ID
 * @return 0 on success, -1 on error
 */
int language_bio_bridge_broadcast_word(
    language_bio_bridge_t* bridge,
    const language_word_t* word,
    uint64_t utterance_id
);

/**
 * @brief Broadcast concept activation event
 *
 * WHAT: Notify subscribers of concept activation
 * WHY:  Route semantic events to cognitive systems
 * HOW:  Create concept message, broadcast to subscribers
 *
 * @param bridge Bridge handle
 * @param concept Concept information
 * @param utterance_id Parent utterance ID
 * @return 0 on success, -1 on error
 */
int language_bio_bridge_broadcast_concept(
    language_bio_bridge_t* bridge,
    const language_concept_t* concept,
    uint64_t utterance_id
);

/**
 * @brief Broadcast comprehension complete event
 *
 * WHAT: Notify subscribers that comprehension finished
 * WHY:  Signal completion to cognitive and production systems
 * HOW:  Create completion message, broadcast to subscribers
 *
 * @param bridge Bridge handle
 * @param result Comprehension result
 * @return 0 on success, -1 on error
 */
int language_bio_bridge_broadcast_comprehension_complete(
    language_bio_bridge_t* bridge,
    const language_comprehension_result_t* result
);

/* ============================================================================
 * Production Coordination API
 * ============================================================================ */

/**
 * @brief Send production request
 *
 * WHAT: Request language production from semantic input
 * WHY:  Initiate speech/text generation
 * HOW:  Create request message, send to production system
 *
 * @param bridge Bridge handle
 * @param semantic_input Semantic vector to verbalize
 * @param semantic_dim Dimension of semantic vector
 * @param output_type Desired output type
 * @return Request ID on success, 0 on error
 */
uint64_t language_bio_bridge_request_production(
    language_bio_bridge_t* bridge,
    const float* semantic_input,
    uint32_t semantic_dim,
    language_output_type_t output_type
);

/**
 * @brief Broadcast production complete event
 *
 * WHAT: Notify subscribers that production finished
 * WHY:  Signal completion to motor and cognitive systems
 * HOW:  Create completion message, broadcast to subscribers
 *
 * @param bridge Bridge handle
 * @param plan Production plan result
 * @return 0 on success, -1 on error
 */
int language_bio_bridge_broadcast_production_complete(
    language_bio_bridge_t* bridge,
    const language_production_plan_t* plan
);

/* ============================================================================
 * Anomaly/Error Broadcast API
 * ============================================================================ */

/**
 * @brief Broadcast semantic anomaly (N400-like)
 *
 * WHAT: Notify subscribers of semantic violation
 * WHY:  Alert attention and cognitive control systems
 * HOW:  Create anomaly message, use urgent channel if severe
 *
 * @param bridge Bridge handle
 * @param word_position Position of anomalous word
 * @param word_id Anomalous word ID
 * @param expected_id Expected concept/word ID
 * @param anomaly_magnitude Violation magnitude [0, 1]
 * @param utterance_id Parent utterance ID
 * @return 0 on success, -1 on error
 */
int language_bio_bridge_broadcast_semantic_anomaly(
    language_bio_bridge_t* bridge,
    uint32_t word_position,
    uint32_t word_id,
    uint32_t expected_id,
    float anomaly_magnitude,
    uint64_t utterance_id
);

/**
 * @brief Broadcast syntactic anomaly (P600-like)
 *
 * WHAT: Notify subscribers of syntactic violation
 * WHY:  Alert prefrontal and reanalysis systems
 * HOW:  Create anomaly message, broadcast to subscribers
 *
 * @param bridge Bridge handle
 * @param word_position Position of anomalous word
 * @param word_id Anomalous word ID
 * @param anomaly_magnitude Violation magnitude [0, 1]
 * @param utterance_id Parent utterance ID
 * @return 0 on success, -1 on error
 */
int language_bio_bridge_broadcast_syntactic_anomaly(
    language_bio_bridge_t* bridge,
    uint32_t word_position,
    uint32_t word_id,
    float anomaly_magnitude,
    uint64_t utterance_id
);

/**
 * @brief Broadcast ambiguity detection
 *
 * @param bridge Bridge handle
 * @param word_position Position of ambiguous word
 * @param word_id Ambiguous word ID
 * @param num_interpretations Number of possible interpretations
 * @param utterance_id Parent utterance ID
 * @return 0 on success, -1 on error
 */
int language_bio_bridge_broadcast_ambiguity(
    language_bio_bridge_t* bridge,
    uint32_t word_position,
    uint32_t word_id,
    uint32_t num_interpretations,
    uint64_t utterance_id
);

/**
 * @brief Broadcast error event
 *
 * @param bridge Bridge handle
 * @param error_code Error code
 * @param message Error message
 * @param source_region Source region
 * @return 0 on success, -1 on error
 */
int language_bio_bridge_broadcast_error(
    language_bio_bridge_t* bridge,
    int32_t error_code,
    const char* message,
    lang_region_id_t source_region
);

/* ============================================================================
 * Semantic/Syntactic Broadcast API
 * ============================================================================ */

/**
 * @brief Broadcast semantic state to all subscribers
 *
 * WHAT: Broadcast current semantic context to interested modules
 * WHY:  Keep cognitive systems informed of semantic state
 * HOW:  Package semantic data, broadcast on semantic channel
 *
 * @param bridge Bridge handle
 * @param concept_ids Array of active concept IDs
 * @param activations Array of activation levels
 * @param concept_count Number of concepts
 * @param context_coherence Overall context coherence
 * @param utterance_id Associated utterance ID
 * @return 0 on success, -1 on error
 */
int language_bio_bridge_broadcast_semantic_state(
    language_bio_bridge_t* bridge,
    const uint32_t* concept_ids,
    const float* activations,
    uint32_t concept_count,
    float context_coherence,
    uint64_t utterance_id
);

/**
 * @brief Broadcast syntactic state to all subscribers
 *
 * WHAT: Broadcast current parse state to interested modules
 * WHY:  Keep cognitive systems informed of syntactic analysis
 * HOW:  Package parse data, broadcast to subscribers
 *
 * @param bridge Bridge handle
 * @param parse_state Current parse state
 * @param current_phrase Current phrase type
 * @param parse_depth Parse tree depth
 * @param parse_probability Parse probability
 * @param utterance_id Associated utterance ID
 * @return 0 on success, -1 on error
 */
int language_bio_bridge_broadcast_syntactic_state(
    language_bio_bridge_t* bridge,
    parse_state_t parse_state,
    phrase_type_t current_phrase,
    uint32_t parse_depth,
    float parse_probability,
    uint64_t utterance_id
);

/* ============================================================================
 * Inter-Region Synchronization API
 * ============================================================================ */

/**
 * @brief Synchronize Broca and Wernicke regions
 *
 * WHAT: Send synchronization message between Broca and Wernicke
 * WHY:  Coordinate comprehension and production pathways
 * HOW:  Create sync message, route to appropriate region
 *
 * @param bridge Bridge handle
 * @param source_region Source region
 * @param target_region Target region
 * @param data Transfer data
 * @param data_size Data size in bytes
 * @param utterance_id Associated utterance ID
 * @return 0 on success, -1 on error
 */
int language_bio_bridge_sync_regions(
    language_bio_bridge_t* bridge,
    lang_region_id_t source_region,
    lang_region_id_t target_region,
    const void* data,
    uint32_t data_size,
    uint64_t utterance_id
);

/**
 * @brief Transfer data via arcuate fasciculus (Broca <-> Wernicke)
 *
 * WHAT: Transfer data along the arcuate fasciculus tract
 * WHY:  Model biological language pathway connectivity
 * HOW:  Create transfer message, route via arcuate channel
 *
 * @param bridge Bridge handle
 * @param to_broca Direction (true = Wernicke->Broca, false = Broca->Wernicke)
 * @param data Transfer data
 * @param data_size Data size in bytes
 * @return 0 on success, -1 on error
 */
int language_bio_bridge_arcuate_transfer(
    language_bio_bridge_t* bridge,
    bool to_broca,
    const void* data,
    uint32_t data_size
);

/* ============================================================================
 * State Change API
 * ============================================================================ */

/**
 * @brief Broadcast state change event
 *
 * @param bridge Bridge handle
 * @param old_state Previous state
 * @param new_state New state
 * @param mode Current mode
 * @return 0 on success, -1 on error
 */
int language_bio_bridge_broadcast_state_change(
    language_bio_bridge_t* bridge,
    language_state_t old_state,
    language_state_t new_state,
    language_mode_t mode
);

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

/**
 * @brief Subscribe module to language messages
 *
 * WHAT: Register module for language message notifications
 * WHY:  Allow selective subscription to message types
 * HOW:  Add module to subscription list with type mask
 *
 * @param bridge Bridge handle
 * @param module_id Module to subscribe
 * @param msg_types Bitmask of message types
 * @return 0 on success, -1 on error
 */
int language_bio_bridge_subscribe(
    language_bio_bridge_t* bridge,
    bio_module_id_t module_id,
    uint64_t msg_types
);

/**
 * @brief Unsubscribe module from language messages
 *
 * @param bridge Bridge handle
 * @param module_id Module to unsubscribe
 * @return 0 on success, -1 on error
 */
int language_bio_bridge_unsubscribe(
    language_bio_bridge_t* bridge,
    bio_module_id_t module_id
);

/**
 * @brief Update module subscription types
 *
 * @param bridge Bridge handle
 * @param module_id Module to update
 * @param msg_types New message type bitmask
 * @return 0 on success, -1 on error
 */
int language_bio_bridge_update_subscription(
    language_bio_bridge_t* bridge,
    bio_module_id_t module_id,
    uint64_t msg_types
);

/**
 * @brief Get subscription count for message type
 *
 * @param bridge Bridge handle
 * @param msg_type Message type to query
 * @return Number of subscribers for this type
 */
uint32_t language_bio_bridge_get_subscriber_count(
    const language_bio_bridge_t* bridge,
    lang_bio_msg_type_t msg_type
);

/* ============================================================================
 * Statistics and Diagnostics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int language_bio_bridge_get_stats(
    const language_bio_bridge_t* bridge,
    language_bio_bridge_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int language_bio_bridge_reset_stats(language_bio_bridge_t* bridge);

/**
 * @brief Get message type name
 *
 * @param msg_type Message type
 * @return Human-readable name string
 */
const char* language_bio_msg_type_name(lang_bio_msg_type_t msg_type);

/**
 * @brief Get region name
 *
 * @param region Region identifier
 * @return Human-readable name string
 */
const char* language_bio_region_name(lang_region_id_t region);

/**
 * @brief Print bridge summary to stdout
 *
 * @param bridge Bridge handle
 */
void language_bio_bridge_print_summary(const language_bio_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LANGUAGE_BIO_ASYNC_BRIDGE_H */
