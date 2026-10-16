//=============================================================================
// nimcp_language_bio_async.h - Language Layer Bio-Async Integration
//=============================================================================
/**
 * @file nimcp_language_bio_async.h
 * @brief Bio-async messaging integration for the Language Layer
 *
 * WHAT: Message handlers and bio-async registration for language processing
 * WHY:  Enable asynchronous event-driven language processing
 * HOW:  Register handlers with bio-async router, process language events
 *
 * BIOLOGICAL BASIS:
 * - Models neural event propagation in language network
 * - Word recognition events broadcast to connected regions
 * - Comprehension/production events trigger cognitive responses
 * - Prediction errors propagate for learning
 *
 * MESSAGE TYPES:
 * - Utterance start/complete
 * - Phoneme/word recognition
 * - Concept activation
 * - Production events
 * - Anomaly detection (N400-like)
 * - Training updates
 *
 * @version 1.0.0 - Phase L1: Language Layer Core Infrastructure
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#ifndef NIMCP_LANGUAGE_BIO_ASYNC_H
#define NIMCP_LANGUAGE_BIO_ASYNC_H

#include "language/nimcp_language_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct language_orchestrator language_orchestrator_t;

/* bio_router_t and bio_module_context_t are pointer types defined in bio_router.h
 * We use opaque void* here to avoid type conflicts when bio_router.h is included */
#ifndef NIMCP_BIO_ROUTER_H
typedef void* bio_router_t;
typedef void* bio_module_context_t;
#endif

//=============================================================================
// Message Type Definitions (for bio_messages.h integration)
//=============================================================================

/**
 * @brief Bio-async message types for language layer
 *
 * These should be added to nimcp_bio_messages.h
 */
typedef enum {
    /* Comprehension messages */
    BIO_MSG_LANG_UTTERANCE_START = 0x1E00,    /**< Utterance processing started */
    BIO_MSG_LANG_PHONEME_RECOGNIZED,          /**< Phoneme recognized */
    BIO_MSG_LANG_WORD_RECOGNIZED,             /**< Word recognized */
    BIO_MSG_LANG_CONCEPT_ACTIVATED,           /**< Concept activated */
    BIO_MSG_LANG_COMPREHENSION_COMPLETE,      /**< Comprehension finished */

    /* Production messages */
    BIO_MSG_LANG_PRODUCTION_REQUEST,          /**< Request to produce utterance */
    BIO_MSG_LANG_PRODUCTION_START,            /**< Production started */
    BIO_MSG_LANG_WORD_PRODUCED,               /**< Word produced */
    BIO_MSG_LANG_PRODUCTION_COMPLETE,         /**< Production finished */

    /* Anomaly messages */
    BIO_MSG_LANG_SEMANTIC_ANOMALY,            /**< N400-like semantic violation */
    BIO_MSG_LANG_SYNTACTIC_ANOMALY,           /**< P600-like syntactic violation */
    BIO_MSG_LANG_AMBIGUITY_DETECTED,          /**< Ambiguous input */

    /* State messages */
    BIO_MSG_LANG_STATE_CHANGE,                /**< State machine transition */
    BIO_MSG_LANG_ERROR,                       /**< Processing error */

    /* Training messages */
    BIO_MSG_LANG_TRAINING_UPDATE,             /**< Training update received */
    BIO_MSG_LANG_WORD_LEARNED,                /**< New word learned */
    BIO_MSG_LANG_GRAMMAR_LEARNED,             /**< Grammar pattern learned */

    /* Bridge messages */
    BIO_MSG_LANG_PERCEPTION_UPDATE,           /**< Perception bridge update */
    BIO_MSG_LANG_COGNITIVE_UPDATE,            /**< Cognitive bridge update */
    BIO_MSG_LANG_IMMUNE_UPDATE,               /**< Immune bridge update */

    BIO_MSG_LANG_COUNT
} language_bio_message_type_t;

//=============================================================================
// Message Payload Structures
//=============================================================================

/**
 * @brief Phoneme recognition message payload
 */
typedef struct {
    uint32_t phoneme_id;              /**< Recognized phoneme ID */
    float confidence;                 /**< Recognition confidence */
    float duration_ms;                /**< Phoneme duration */
    uint64_t timestamp_ms;            /**< Recognition timestamp */
    bool is_word_boundary;            /**< Word boundary marker */
} language_msg_phoneme_t;

/**
 * @brief Word recognition message payload
 */
typedef struct {
    uint32_t word_id;                 /**< Recognized word ID */
    char word_form[64];               /**< Word orthographic form */
    float confidence;                 /**< Recognition confidence */
    uint32_t sense_id;                /**< Selected sense (for polysemy) */
    uint64_t timestamp_ms;            /**< Recognition timestamp */
} language_msg_word_t;

/**
 * @brief Concept activation message payload
 */
typedef struct {
    uint32_t concept_id;              /**< Activated concept ID */
    char concept_name[64];            /**< Concept name */
    float activation;                 /**< Activation level */
    float relevance;                  /**< Context relevance */
    uint32_t source_word_id;          /**< Activating word */
    uint64_t timestamp_ms;            /**< Activation timestamp */
} language_msg_concept_t;

/**
 * @brief Comprehension complete message payload
 */
typedef struct {
    uint32_t word_count;              /**< Number of words comprehended */
    uint32_t concept_count;           /**< Number of concepts activated */
    float semantic_coherence;         /**< Semantic coherence score */
    float syntactic_score;            /**< Syntactic wellformedness */
    float overall_confidence;         /**< Overall confidence */
    float processing_time_ms;         /**< Processing time */
    uint64_t timestamp_ms;            /**< Completion timestamp */
} language_msg_comprehension_t;

/**
 * @brief Production request message payload
 */
typedef struct {
    float* semantic_input;            /**< Semantic vector to verbalize */
    uint32_t semantic_dim;            /**< Semantic dimension */
    language_output_type_t output_type;/**< Desired output type */
    uint64_t request_id;              /**< Request identifier */
} language_msg_production_request_t;

/**
 * @brief Production complete message payload
 */
typedef struct {
    uint32_t word_count;              /**< Number of words produced */
    float fluency_score;              /**< Fluency score */
    float planning_time_ms;           /**< Planning time */
    uint64_t request_id;              /**< Original request ID */
    uint64_t timestamp_ms;            /**< Completion timestamp */
} language_msg_production_t;

/**
 * @brief Anomaly detection message payload
 */
typedef struct {
    bool is_semantic;                 /**< Semantic (true) or syntactic (false) */
    float anomaly_magnitude;          /**< Violation magnitude */
    uint32_t word_position;           /**< Position of anomalous word */
    uint32_t word_id;                 /**< Anomalous word ID */
    uint32_t expected_id;             /**< Expected word/concept ID */
    uint64_t timestamp_ms;            /**< Detection timestamp */
} language_msg_anomaly_t;

/**
 * @brief State change message payload
 */
typedef struct {
    language_state_t old_state;       /**< Previous state */
    language_state_t new_state;       /**< New state */
    uint64_t timestamp_ms;            /**< Transition timestamp */
} language_msg_state_change_t;

/**
 * @brief Error message payload
 */
typedef struct {
    int error_code;                   /**< Error code */
    char message[128];                /**< Error message */
    uint64_t timestamp_ms;            /**< Error timestamp */
} language_msg_error_t;

/**
 * @brief Training update message payload
 */
typedef struct {
    float vocabulary_lr;              /**< Vocabulary learning rate */
    float grammar_weight;             /**< Grammar learning weight */
    float phoneme_plasticity;         /**< Phoneme plasticity */
    uint64_t training_step;           /**< Current training step */
    uint64_t timestamp_ms;            /**< Update timestamp */
} language_msg_training_t;

/**
 * @brief Word learned message payload
 */
typedef struct {
    uint32_t word_id;                 /**< New word ID */
    char word_form[64];               /**< Word form */
    float novelty;                    /**< Novelty score */
    uint64_t timestamp_ms;            /**< Learning timestamp */
} language_msg_word_learned_t;

//=============================================================================
// Bio-Async Registration API
//=============================================================================

/**
 * @brief Register language layer with bio-async router
 *
 * WHAT: Register language orchestrator with bio-async system
 * WHY:  Enable message-based communication with other brain modules
 * HOW:  Register module and message handlers with router
 *
 * @param orchestrator Language orchestrator instance
 * @param router Bio-async router instance
 * @return 0 on success, -1 on error
 */
int language_bio_async_register(
    language_orchestrator_t* orchestrator,
    bio_router_t router
);

/**
 * @brief Unregister language layer from bio-async router
 *
 * @param orchestrator Language orchestrator instance
 * @return 0 on success, -1 on error
 */
int language_bio_async_unregister(language_orchestrator_t* orchestrator);

/**
 * @brief Check if language layer is registered with bio-async
 *
 * @param orchestrator Language orchestrator instance
 * @return true if registered, false otherwise
 */
bool language_bio_async_is_registered(const language_orchestrator_t* orchestrator);

/**
 * @brief Get bio-async module context
 *
 * @param orchestrator Language orchestrator instance
 * @return Bio module context or NULL if not registered
 */
bio_module_context_t language_bio_async_get_context(
    const language_orchestrator_t* orchestrator
);

//=============================================================================
// Message Sending API
//=============================================================================

/**
 * @brief Broadcast phoneme recognition event
 *
 * @param orchestrator Language orchestrator instance
 * @param phoneme Phoneme information
 * @return 0 on success, -1 on error
 */
int language_bio_async_send_phoneme(
    language_orchestrator_t* orchestrator,
    const language_msg_phoneme_t* phoneme
);

/**
 * @brief Broadcast word recognition event
 *
 * @param orchestrator Language orchestrator instance
 * @param word Word information
 * @return 0 on success, -1 on error
 */
int language_bio_async_send_word(
    language_orchestrator_t* orchestrator,
    const language_msg_word_t* word
);

/**
 * @brief Broadcast concept activation event
 *
 * @param orchestrator Language orchestrator instance
 * @param concept Concept information
 * @return 0 on success, -1 on error
 */
int language_bio_async_send_concept(
    language_orchestrator_t* orchestrator,
    const language_msg_concept_t* concept
);

/**
 * @brief Broadcast comprehension complete event
 *
 * @param orchestrator Language orchestrator instance
 * @param result Comprehension result
 * @return 0 on success, -1 on error
 */
int language_bio_async_send_comprehension(
    language_orchestrator_t* orchestrator,
    const language_msg_comprehension_t* result
);

/**
 * @brief Broadcast production complete event
 *
 * @param orchestrator Language orchestrator instance
 * @param result Production result
 * @return 0 on success, -1 on error
 */
int language_bio_async_send_production(
    language_orchestrator_t* orchestrator,
    const language_msg_production_t* result
);

/**
 * @brief Broadcast anomaly detection event
 *
 * @param orchestrator Language orchestrator instance
 * @param anomaly Anomaly information
 * @return 0 on success, -1 on error
 */
int language_bio_async_send_anomaly(
    language_orchestrator_t* orchestrator,
    const language_msg_anomaly_t* anomaly
);

/**
 * @brief Broadcast state change event
 *
 * @param orchestrator Language orchestrator instance
 * @param state_change State change information
 * @return 0 on success, -1 on error
 */
int language_bio_async_send_state_change(
    language_orchestrator_t* orchestrator,
    const language_msg_state_change_t* state_change
);

/**
 * @brief Broadcast error event
 *
 * @param orchestrator Language orchestrator instance
 * @param error Error information
 * @return 0 on success, -1 on error
 */
int language_bio_async_send_error(
    language_orchestrator_t* orchestrator,
    const language_msg_error_t* error
);

//=============================================================================
// Message Handler Types
//=============================================================================

/**
 * @brief Handler for incoming language messages
 */
typedef void (*language_message_handler_t)(
    language_orchestrator_t* orchestrator,
    uint32_t message_type,
    const void* payload,
    uint32_t payload_size
);

/**
 * @brief Register custom message handler
 *
 * @param orchestrator Language orchestrator instance
 * @param message_type Message type to handle
 * @param handler Handler function
 * @return 0 on success, -1 on error
 */
int language_bio_async_register_handler(
    language_orchestrator_t* orchestrator,
    uint32_t message_type,
    language_message_handler_t handler
);

//=============================================================================
// Message Processing
//=============================================================================

/**
 * @brief Process pending bio-async messages
 *
 * @param orchestrator Language orchestrator instance
 * @return Number of messages processed, or -1 on error
 */
int language_bio_async_process_messages(language_orchestrator_t* orchestrator);

/**
 * @brief Get number of pending messages
 *
 * @param orchestrator Language orchestrator instance
 * @return Number of pending messages
 */
uint32_t language_bio_async_pending_count(const language_orchestrator_t* orchestrator);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LANGUAGE_BIO_ASYNC_H */
