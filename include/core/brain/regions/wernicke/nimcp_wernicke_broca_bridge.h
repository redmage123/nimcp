/**
 * @file nimcp_wernicke_broca_bridge.h
 * @brief Arcuate fasciculus bridge connecting Wernicke's and Broca's areas
 *
 * WHAT: Bidirectional fiber tract connecting language comprehension and production
 * WHY:  Enable repetition, self-monitoring, and language production from comprehension
 * HOW:  Forward comprehension to production, return efference copies for monitoring
 *
 * BIOLOGICAL BASIS:
 * - Arcuate fasciculus (AF): Major white matter tract connecting Wernicke to Broca
 * - Superior longitudinal fasciculus (SLF): Additional dorsal pathway
 * - Lesion causes conduction aphasia: good comprehension, poor repetition
 * - Bidirectional: dorsal (phonological) and ventral (semantic) streams
 *
 * PROCESSING PATHWAYS:
 * 1. Dorsal Stream (Phonological): Wernicke STG → Arcuate → Broca pars opercularis
 *    - Sound-to-motor mapping for repetition
 *    - Phonological working memory rehearsal
 * 2. Ventral Stream (Semantic): Wernicke → MTG → ATL → Broca pars triangularis
 *    - Semantic comprehension to production
 *    - Concept-to-word mapping
 *
 * FUNCTIONS:
 * - Repetition: Echo heard speech (phonological loop)
 * - Response generation: Comprehension → semantic intent → production
 * - Self-monitoring: Efference copy from Broca for error detection
 * - Working memory: Phonological rehearsal via subvocal articulation
 *
 * @version Phase W3: Wernicke's Area Bridges
 * @date 2026-01-04
 */

#ifndef NIMCP_WERNICKE_BROCA_BRIDGE_H
#define NIMCP_WERNICKE_BROCA_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Default configuration values
 */
#define WBB_DEFAULT_BUFFER_SIZE          64
#define WBB_DEFAULT_PHONEME_BUFFER       128
#define WBB_DEFAULT_SEMANTIC_DIM         128
#define WBB_DEFAULT_TRANSMISSION_DELAY_MS 50
#define WBB_DEFAULT_REPETITION_THRESHOLD 0.7f
#define WBB_DEFAULT_MONITORING_THRESHOLD 0.8f

/**
 * @brief Bio-async module ID for this bridge
 */
#define BIO_MODULE_WERNICKE_BROCA 0x1270

/**
 * @brief Stream type for routing
 */
typedef enum {
    WBB_STREAM_DORSAL,      /**< Phonological (repetition, articulation) */
    WBB_STREAM_VENTRAL,     /**< Semantic (meaning-based production) */
    WBB_STREAM_BOTH         /**< Combined pathway */
} wbb_stream_t;

/**
 * @brief Message type for bridge communication
 */
typedef enum {
    WBB_MSG_COMPREHENSION,      /**< Wernicke → Broca: comprehension result */
    WBB_MSG_REPETITION_REQUEST, /**< Wernicke → Broca: repeat what was heard */
    WBB_MSG_RESPONSE_INTENT,    /**< Wernicke → Broca: generate response */
    WBB_MSG_EFFERENCE_COPY,     /**< Broca → Wernicke: self-monitoring feedback */
    WBB_MSG_REHEARSAL,          /**< Bidirectional: working memory rehearsal */
    WBB_MSG_ERROR_SIGNAL,       /**< Wernicke → Broca: production error detected */
    WBB_MSG_ACKNOWLEDGMENT      /**< Receipt confirmation */
} wbb_message_type_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Capacity */
    uint32_t buffer_size;            /**< Message buffer size */
    uint32_t phoneme_buffer_size;    /**< Phoneme sequence buffer */
    uint32_t semantic_dim;           /**< Semantic vector dimension */

    /* Timing */
    float transmission_delay_ms;     /**< Simulated tract delay */
    float dorsal_delay_ms;           /**< Dorsal pathway specific delay */
    float ventral_delay_ms;          /**< Ventral pathway specific delay */

    /* Thresholds */
    float repetition_threshold;      /**< Min confidence for repetition */
    float monitoring_threshold;      /**< Self-monitoring sensitivity */
    float error_detection_threshold; /**< Error signal threshold */

    /* Feature flags */
    bool enable_dorsal_stream;       /**< Enable phonological pathway */
    bool enable_ventral_stream;      /**< Enable semantic pathway */
    bool enable_self_monitoring;     /**< Enable efference copy processing */
    bool enable_working_memory;      /**< Enable rehearsal loop */
    bool enable_error_correction;    /**< Enable production error feedback */
    bool enable_bio_async;           /**< Enable bio-async messaging */
} wbb_config_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Comprehension result to forward to Broca
 */
typedef struct {
    /* Word/concept identification */
    uint32_t word_id;                /**< Recognized word ID */
    uint32_t concept_id;             /**< Activated concept ID */
    char word_string[32];            /**< Word orthography */

    /* Semantic representation */
    float* semantic_vector;          /**< Semantic features */
    uint32_t semantic_dim;           /**< Vector dimension */

    /* Phonological representation */
    uint8_t* phonemes;               /**< Phoneme sequence */
    uint32_t num_phonemes;           /**< Number of phonemes */

    /* Confidence and context */
    float confidence;                /**< Recognition confidence */
    float context_fit;               /**< Contextual appropriateness */
    uint8_t thematic_role;           /**< Assigned thematic role */

    /* Timing */
    uint64_t recognition_time_ms;    /**< When word was recognized */
} wbb_comprehension_t;

/**
 * @brief Efference copy from Broca for self-monitoring
 */
typedef struct {
    /* Planned production */
    uint32_t planned_word_id;        /**< Word Broca plans to produce */
    uint8_t* planned_phonemes;       /**< Planned phoneme sequence */
    uint32_t num_planned_phonemes;   /**< Number of planned phonemes */

    /* Motor command preview */
    float* motor_plan;               /**< Motor command sequence preview */
    uint32_t motor_plan_length;      /**< Motor sequence length */

    /* Production state */
    uint8_t production_stage;        /**< Current production stage */
    float fluency_estimate;          /**< Expected fluency */

    /* Timing */
    uint64_t production_time_ms;     /**< When production will occur */
} wbb_efference_copy_t;

/**
 * @brief Self-monitoring comparison result
 */
typedef struct {
    /* Match assessment */
    bool phoneme_match;              /**< Phonemes match expected */
    bool semantic_match;             /**< Semantics match intended */
    float match_score;               /**< Overall match [0,1] */

    /* Error details */
    bool error_detected;             /**< Production error detected */
    uint8_t error_type;              /**< 0=none, 1=phoneme, 2=semantic, 3=both */
    uint32_t error_position;         /**< Position of error (if any) */

    /* Correction suggestion */
    bool correction_available;       /**< Correction can be suggested */
    uint32_t suggested_word_id;      /**< Corrected word (if available) */
} wbb_monitoring_result_t;

/**
 * @brief Rehearsal request for working memory
 */
typedef struct {
    uint8_t* phonemes;               /**< Phonemes to rehearse */
    uint32_t num_phonemes;           /**< Number of phonemes */
    uint32_t repetitions;            /**< Number of rehearsal cycles */
    float decay_rate;                /**< Memory decay per cycle */
} wbb_rehearsal_t;

/**
 * @brief Bridge message envelope
 */
typedef struct {
    wbb_message_type_t type;         /**< Message type */
    wbb_stream_t stream;             /**< Routing pathway */
    uint64_t timestamp_ms;           /**< Message timestamp */
    uint32_t sequence_num;           /**< Sequence number */

    /* Payload (union for space efficiency) */
    union {
        wbb_comprehension_t comprehension;
        wbb_efference_copy_t efference;
        wbb_rehearsal_t rehearsal;
    } payload;
} wbb_message_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Message counts */
    uint64_t messages_sent;          /**< Total messages sent */
    uint64_t messages_received;      /**< Total messages received */
    uint64_t comprehensions_forwarded; /**< Comprehensions sent to Broca */
    uint64_t efference_copies_received; /**< Efference copies from Broca */

    /* Repetition stats */
    uint64_t repetition_requests;    /**< Repetition requests sent */
    uint64_t successful_repetitions; /**< Successfully repeated */

    /* Monitoring stats */
    uint64_t monitoring_checks;      /**< Self-monitoring checks */
    uint64_t errors_detected;        /**< Production errors detected */
    uint64_t corrections_made;       /**< Successful corrections */

    /* Latency */
    float avg_transmission_ms;       /**< Average transmission time */
    float avg_monitoring_latency_ms; /**< Average monitoring latency */
} wbb_stats_t;

/*=============================================================================
 * OPAQUE TYPE
 *===========================================================================*/

typedef struct wernicke_broca_bridge wernicke_broca_bridge_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * @return Default bridge configuration
 */
wbb_config_t wbb_default_config(void);

/**
 * @brief Create Wernicke-Broca bridge
 *
 * @param wernicke Wernicke adapter handle (void* for flexibility)
 * @param broca Broca adapter handle (void* for flexibility)
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
wernicke_broca_bridge_t* wbb_create(
    void* wernicke,
    void* broca,
    const wbb_config_t* config
);

/**
 * @brief Destroy bridge
 *
 * @param bridge Bridge to destroy
 */
void wbb_destroy(wernicke_broca_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int wbb_reset(wernicke_broca_bridge_t* bridge);

/*=============================================================================
 * CONNECTION MANAGEMENT
 *===========================================================================*/

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Bridge instance
 * @param router Bio-async router (void* for flexibility)
 * @return 0 on success, -1 on error
 */
int wbb_connect_bio_async(
    wernicke_broca_bridge_t* bridge,
    void* router
);

/**
 * @brief Set Wernicke endpoint
 *
 * @param bridge Bridge instance
 * @param wernicke Wernicke adapter handle
 * @return 0 on success, -1 on error
 */
int wbb_set_wernicke(
    wernicke_broca_bridge_t* bridge,
    void* wernicke
);

/**
 * @brief Set Broca endpoint
 *
 * @param bridge Bridge instance
 * @param broca Broca adapter handle
 * @return 0 on success, -1 on error
 */
int wbb_set_broca(
    wernicke_broca_bridge_t* bridge,
    void* broca
);

/*=============================================================================
 * WERNICKE → BROCA (Comprehension to Production)
 *===========================================================================*/

/**
 * @brief Forward comprehension result to Broca
 *
 * WHAT: Send recognized word/concept for potential production
 * WHY:  Enable response generation based on comprehension
 * HOW:  Package comprehension and route via appropriate stream
 *
 * @param bridge Bridge instance
 * @param comprehension Comprehension result to forward
 * @param stream Routing pathway (dorsal/ventral/both)
 * @return 0 on success, -1 on error
 */
int wbb_forward_comprehension(
    wernicke_broca_bridge_t* bridge,
    const wbb_comprehension_t* comprehension,
    wbb_stream_t stream
);

/**
 * @brief Request repetition of heard speech
 *
 * WHAT: Ask Broca to repeat what Wernicke recognized
 * WHY:  Model echolalia/repetition via phonological loop
 * HOW:  Forward phonemes via dorsal stream
 *
 * @param bridge Bridge instance
 * @param phonemes Phoneme sequence to repeat
 * @param num_phonemes Number of phonemes
 * @return 0 on success, -1 on error
 */
int wbb_request_repetition(
    wernicke_broca_bridge_t* bridge,
    const uint8_t* phonemes,
    uint32_t num_phonemes
);

/**
 * @brief Send semantic intent for response generation
 *
 * WHAT: Send semantic representation for Broca to verbalize
 * WHY:  Generate spoken response from understood meaning
 * HOW:  Forward via ventral stream
 *
 * @param bridge Bridge instance
 * @param semantic_vector Semantic representation
 * @param semantic_dim Vector dimension
 * @param intent_type Type of response (statement/question/command)
 * @return 0 on success, -1 on error
 */
int wbb_send_response_intent(
    wernicke_broca_bridge_t* bridge,
    const float* semantic_vector,
    uint32_t semantic_dim,
    uint8_t intent_type
);

/*=============================================================================
 * BROCA → WERNICKE (Self-Monitoring)
 *===========================================================================*/

/**
 * @brief Receive efference copy from Broca
 *
 * WHAT: Get copy of planned production for self-monitoring
 * WHY:  Detect production errors before/during articulation
 * HOW:  Compare efference copy with intended output
 *
 * @param bridge Bridge instance
 * @param efference Output efference copy (if available)
 * @return 0 on success, 1 if no efference available, -1 on error
 */
int wbb_receive_efference_copy(
    wernicke_broca_bridge_t* bridge,
    wbb_efference_copy_t* efference
);

/**
 * @brief Compare production plan with comprehension
 *
 * WHAT: Self-monitoring comparison
 * WHY:  Detect mismatches between intended and planned
 * HOW:  Compare phoneme sequences and semantic vectors
 *
 * @param bridge Bridge instance
 * @param intended Intended comprehension output
 * @param efference Planned production from Broca
 * @param result Output monitoring result
 * @return 0 on success, -1 on error
 */
int wbb_compare_production(
    wernicke_broca_bridge_t* bridge,
    const wbb_comprehension_t* intended,
    const wbb_efference_copy_t* efference,
    wbb_monitoring_result_t* result
);

/**
 * @brief Send error signal to Broca
 *
 * WHAT: Signal production error detected
 * WHY:  Enable correction before or during articulation
 * HOW:  Send error signal with optional correction
 *
 * @param bridge Bridge instance
 * @param error_type Type of error detected
 * @param position Position of error
 * @param correction Suggested correction (NULL if none)
 * @return 0 on success, -1 on error
 */
int wbb_send_error_signal(
    wernicke_broca_bridge_t* bridge,
    uint8_t error_type,
    uint32_t position,
    const uint8_t* correction
);

/*=============================================================================
 * WORKING MEMORY REHEARSAL
 *===========================================================================*/

/**
 * @brief Request phonological rehearsal
 *
 * WHAT: Initiate subvocal rehearsal loop
 * WHY:  Maintain phonological working memory
 * HOW:  Wernicke → Broca → (subvocal) → Wernicke
 *
 * @param bridge Bridge instance
 * @param phonemes Phonemes to rehearse
 * @param num_phonemes Number of phonemes
 * @param repetitions Number of rehearsal cycles
 * @return 0 on success, -1 on error
 */
int wbb_request_rehearsal(
    wernicke_broca_bridge_t* bridge,
    const uint8_t* phonemes,
    uint32_t num_phonemes,
    uint32_t repetitions
);

/**
 * @brief Process rehearsal feedback
 *
 * WHAT: Handle returning rehearsal from Broca
 * WHY:  Refresh phonological memory trace
 * HOW:  Reactivate phoneme representations
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int wbb_process_rehearsal(wernicke_broca_bridge_t* bridge);

/*=============================================================================
 * MESSAGE HANDLING
 *===========================================================================*/

/**
 * @brief Process pending messages
 *
 * WHAT: Handle queued messages from both directions
 * WHY:  Event-driven bridge operation
 * HOW:  Dequeue and dispatch messages
 *
 * @param bridge Bridge instance
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed, -1 on error
 */
int wbb_process_messages(
    wernicke_broca_bridge_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Get pending message count
 *
 * @param bridge Bridge instance
 * @return Number of pending messages
 */
uint32_t wbb_pending_count(const wernicke_broca_bridge_t* bridge);

/**
 * @brief Peek at next message without dequeuing
 *
 * @param bridge Bridge instance
 * @param message Output message (copied)
 * @return 0 on success, 1 if queue empty, -1 on error
 */
int wbb_peek_message(
    const wernicke_broca_bridge_t* bridge,
    wbb_message_t* message
);

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int wbb_get_stats(
    const wernicke_broca_bridge_t* bridge,
    wbb_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge instance
 */
void wbb_reset_stats(wernicke_broca_bridge_t* bridge);

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Get current configuration
 *
 * @param bridge Bridge instance
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int wbb_get_config(
    const wernicke_broca_bridge_t* bridge,
    wbb_config_t* config
);

/**
 * @brief Update configuration
 *
 * @param bridge Bridge instance
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int wbb_set_config(
    wernicke_broca_bridge_t* bridge,
    const wbb_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WERNICKE_BROCA_BRIDGE_H */
