/**
 * @file nimcp_discourse_manager.h
 * @brief Discourse management for conversation tracking
 *
 * WHAT: Conversation structure tracking, anaphora resolution, and topic management
 * WHY:  Enable coherent multi-turn conversations with proper reference tracking
 * HOW:  Maintain discourse model, resolve pronouns, and track topic transitions
 *
 * ARCHITECTURE:
 * - Topic Tracker: Monitors current and recent topics of discussion
 * - Anaphora Resolver: Resolves pronouns and references to antecedents
 * - Turn Manager: Tracks speaker turns and conversation flow
 * - Coherence Analyzer: Evaluates discourse coherence
 *
 * BIOLOGICAL BASIS:
 * - Models prefrontal cortex role in maintaining discourse context
 * - Integrates with working memory for reference tracking
 * - Right hemisphere's role in coherence and discourse processing
 *
 * KEY CONCEPTS:
 * - Discourse Referent: Entity introduced in conversation
 * - Anaphora: Reference to previously mentioned entity
 * - Topic: Current subject of discussion
 * - Focus: Most salient entity at given point
 *
 * @version Phase B4: Speech Enhancement
 * @date 2026-01-15
 */

#ifndef NIMCP_DISCOURSE_MANAGER_H
#define NIMCP_DISCOURSE_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Bio-async communication system */
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

/* Forward declaration for opaque type */
typedef struct discourse_manager discourse_manager_t;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define DISCOURSE_DEFAULT_MAX_TURNS          64
#define DISCOURSE_DEFAULT_MAX_REFERENTS      128
#define DISCOURSE_DEFAULT_MAX_TOPICS          16
#define DISCOURSE_DEFAULT_HISTORY_DEPTH        8
#define DISCOURSE_MAX_PARTICIPANTS            8
#define DISCOURSE_MAX_ENTITY_NAME            64
#define DISCOURSE_MAX_TOPIC_NAME            128

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Referent types
 */
typedef enum {
    REFERENT_TYPE_UNKNOWN = 0,
    REFERENT_TYPE_PERSON,         /**< Human referent */
    REFERENT_TYPE_OBJECT,         /**< Physical object */
    REFERENT_TYPE_LOCATION,       /**< Place or location */
    REFERENT_TYPE_TIME,           /**< Temporal reference */
    REFERENT_TYPE_EVENT,          /**< Event or action */
    REFERENT_TYPE_ABSTRACT,       /**< Abstract concept */
    REFERENT_TYPE_PROPOSITION,    /**< Propositional content */
    REFERENT_TYPE_COUNT
} referent_type_t;

/**
 * @brief Anaphora types
 */
typedef enum {
    ANAPHORA_TYPE_NONE = 0,
    ANAPHORA_TYPE_PRONOUN,        /**< He, she, it, they */
    ANAPHORA_TYPE_DEMONSTRATIVE,  /**< This, that, these, those */
    ANAPHORA_TYPE_DEFINITE,       /**< The [noun] */
    ANAPHORA_TYPE_ZERO,           /**< Elided reference */
    ANAPHORA_TYPE_REFLEXIVE,      /**< Himself, herself */
    ANAPHORA_TYPE_RELATIVE,       /**< Who, which, that */
    ANAPHORA_TYPE_COUNT
} anaphora_type_t;

/**
 * @brief Topic shift types
 */
typedef enum {
    TOPIC_SHIFT_NONE = 0,
    TOPIC_SHIFT_CONTINUATION,     /**< Same topic continues */
    TOPIC_SHIFT_ELABORATION,      /**< Elaborating on subtopic */
    TOPIC_SHIFT_DIGRESSION,       /**< Temporary departure */
    TOPIC_SHIFT_CHANGE,           /**< Complete topic change */
    TOPIC_SHIFT_RETURN,           /**< Return to previous topic */
    TOPIC_SHIFT_COUNT
} topic_shift_t;

/**
 * @brief Coherence relation types
 */
typedef enum {
    COHERENCE_RELATION_NONE = 0,
    COHERENCE_RELATION_ELABORATION,  /**< Adds detail */
    COHERENCE_RELATION_EXPLANATION,  /**< Explains cause */
    COHERENCE_RELATION_CONTRAST,     /**< Contrasts with */
    COHERENCE_RELATION_RESULT,       /**< Shows result */
    COHERENCE_RELATION_PARALLEL,     /**< Parallel structure */
    COHERENCE_RELATION_TEMPORAL,     /**< Temporal sequence */
    COHERENCE_RELATION_COUNT
} coherence_relation_t;

/**
 * @brief Processing status
 */
typedef enum {
    DISCOURSE_STATUS_IDLE = 0,
    DISCOURSE_STATUS_PROCESSING,
    DISCOURSE_STATUS_RESOLVING,
    DISCOURSE_STATUS_READY,
    DISCOURSE_STATUS_ERROR
} discourse_status_t;

/**
 * @brief Error codes
 */
typedef enum {
    DISCOURSE_ERROR_NONE = 0,
    DISCOURSE_ERROR_INVALID_INPUT,
    DISCOURSE_ERROR_REFERENT_NOT_FOUND,
    DISCOURSE_ERROR_BUFFER_FULL,
    DISCOURSE_ERROR_RESOLUTION_FAILED,
    DISCOURSE_ERROR_INTERNAL
} discourse_error_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Discourse manager configuration
 */
typedef struct {
    /* Capacity */
    uint32_t max_turns;              /**< Maximum conversation turns */
    uint32_t max_referents;          /**< Maximum tracked referents */
    uint32_t max_topics;             /**< Maximum concurrent topics */
    uint32_t history_depth;          /**< How far back to look for antecedents */

    /* Resolution settings */
    float salience_decay;            /**< Salience decay rate per turn */
    float resolution_threshold;       /**< Confidence threshold for resolution */
    bool enable_zero_anaphora;       /**< Detect elided references */

    /* Topic tracking */
    bool enable_topic_tracking;      /**< Track topic changes */
    float topic_shift_threshold;     /**< Threshold for topic shift detection */

    /* Integration */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    bool enable_working_memory;      /**< Use working memory */
} discourse_config_t;

/**
 * @brief Discourse referent (entity in discourse)
 */
typedef struct {
    uint32_t referent_id;            /**< Unique identifier */
    referent_type_t type;            /**< Referent type */
    char name[DISCOURSE_MAX_ENTITY_NAME];  /**< Entity name/description */

    /* Properties */
    uint8_t gender;                  /**< 0=unknown, 1=masc, 2=fem, 3=neut */
    uint8_t number;                  /**< 0=unknown, 1=singular, 2=plural */
    uint8_t person;                  /**< 0=unknown, 1=first, 2=second, 3=third */
    bool is_animate;                 /**< Animate vs inanimate */

    /* Discourse status */
    float salience;                  /**< Current salience (0-1) */
    uint32_t introduction_turn;      /**< Turn when introduced */
    uint32_t last_mentioned_turn;    /**< Last turn mentioned */
    uint32_t mention_count;          /**< How many times mentioned */
} discourse_referent_t;

/**
 * @brief Anaphoric expression
 */
typedef struct {
    anaphora_type_t type;            /**< Type of anaphor */
    char expression[32];             /**< The anaphoric expression */
    uint32_t position;               /**< Position in utterance */

    /* Resolution result */
    uint32_t antecedent_id;          /**< Resolved referent ID (0 if unresolved) */
    float resolution_confidence;      /**< Confidence in resolution */
    bool is_resolved;                /**< Whether successfully resolved */
} anaphora_resolution_t;

/**
 * @brief Topic information
 */
typedef struct {
    uint32_t topic_id;               /**< Topic identifier */
    char name[DISCOURSE_MAX_TOPIC_NAME];  /**< Topic description */
    float salience;                  /**< Current topic salience */
    uint32_t introduction_turn;      /**< Turn when introduced */
    uint32_t last_active_turn;       /**< Last turn when active */
    bool is_current;                 /**< Is this the current topic */
} discourse_topic_t;

/**
 * @brief Turn record
 */
typedef struct {
    uint32_t turn_id;                /**< Turn identifier */
    uint32_t speaker_id;             /**< Speaker identifier */
    uint64_t timestamp_ms;           /**< Timestamp */
    char content[256];               /**< Turn content */

    /* Topic */
    uint32_t topic_id;               /**< Associated topic */
    topic_shift_t topic_shift;       /**< Type of topic shift */

    /* Coherence */
    coherence_relation_t coherence;  /**< Relation to previous turn */
    float coherence_score;           /**< Coherence score */

    /* Referents */
    uint32_t new_referent_count;     /**< New referents introduced */
    uint32_t referenced_count;       /**< Existing referents mentioned */
} discourse_turn_t;

/**
 * @brief Discourse analysis result
 */
typedef struct {
    /* Anaphora resolutions */
    anaphora_resolution_t resolutions[8];
    uint32_t resolution_count;

    /* Topic analysis */
    uint32_t current_topic_id;
    topic_shift_t topic_shift;
    float topic_coherence;

    /* Coherence */
    coherence_relation_t coherence_relation;
    float coherence_score;

    /* Referents */
    uint32_t new_referents[4];
    uint32_t new_referent_count;
    uint32_t referenced_entities[8];
    uint32_t referenced_count;

    /* Processing */
    double processing_time_ms;
} discourse_analysis_t;

/**
 * @brief Statistics
 */
typedef struct {
    uint64_t turns_processed;
    uint64_t referents_created;
    uint64_t anaphora_resolved;
    uint64_t anaphora_failed;
    uint64_t topic_shifts_detected;
    double avg_coherence_score;
    double avg_resolution_confidence;
} discourse_stats_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default configuration
 * @return Default configuration structure
 */
discourse_config_t discourse_default_config(void);

/**
 * @brief Create discourse manager
 * @param config Configuration (NULL for defaults)
 * @return New manager instance or NULL on failure
 */
discourse_manager_t* discourse_create(const discourse_config_t* config);

/**
 * @brief Destroy discourse manager
 * @param manager Manager to destroy
 */
void discourse_destroy(discourse_manager_t* manager);

/**
 * @brief Reset discourse state (clear conversation)
 * @param manager Manager to reset
 * @return true on success
 */
bool discourse_reset(discourse_manager_t* manager);

/*=============================================================================
 * REFERENT MANAGEMENT
 *===========================================================================*/

/**
 * @brief Introduce a new discourse referent
 * @param manager Manager instance
 * @param name Referent name/description
 * @param type Referent type
 * @param gender Grammatical gender
 * @param number Grammatical number
 * @return Referent ID or 0 on failure
 */
uint32_t discourse_introduce_referent(
    discourse_manager_t* manager,
    const char* name,
    referent_type_t type,
    uint8_t gender,
    uint8_t number
);

/**
 * @brief Get referent by ID
 * @param manager Manager instance
 * @param referent_id Referent identifier
 * @param referent Output referent data
 * @return true if found
 */
bool discourse_get_referent(
    const discourse_manager_t* manager,
    uint32_t referent_id,
    discourse_referent_t* referent
);

/**
 * @brief Find referent by name
 * @param manager Manager instance
 * @param name Name to search for
 * @param referent Output referent data
 * @return true if found
 */
bool discourse_find_referent(
    const discourse_manager_t* manager,
    const char* name,
    discourse_referent_t* referent
);

/**
 * @brief Get most salient referents
 * @param manager Manager instance
 * @param referents Output array
 * @param max_count Maximum to retrieve
 * @return Number of referents retrieved
 */
uint32_t discourse_get_salient_referents(
    const discourse_manager_t* manager,
    discourse_referent_t* referents,
    uint32_t max_count
);

/**
 * @brief Update referent salience (e.g., when mentioned)
 * @param manager Manager instance
 * @param referent_id Referent identifier
 * @param salience_boost Boost to apply (0-1)
 * @return true on success
 */
bool discourse_boost_referent_salience(
    discourse_manager_t* manager,
    uint32_t referent_id,
    float salience_boost
);

/*=============================================================================
 * ANAPHORA RESOLUTION
 *===========================================================================*/

/**
 * @brief Resolve an anaphoric expression
 * @param manager Manager instance
 * @param expression The anaphoric expression (e.g., "he", "it", "this")
 * @param context Surrounding context
 * @param result Output resolution result
 * @return true if resolved
 */
bool discourse_resolve_anaphora(
    discourse_manager_t* manager,
    const char* expression,
    const char* context,
    anaphora_resolution_t* result
);

/**
 * @brief Resolve all anaphora in an utterance
 * @param manager Manager instance
 * @param utterance Full utterance
 * @param resolutions Output array
 * @param max_resolutions Maximum to resolve
 * @return Number of resolutions
 */
uint32_t discourse_resolve_all_anaphora(
    discourse_manager_t* manager,
    const char* utterance,
    anaphora_resolution_t* resolutions,
    uint32_t max_resolutions
);

/**
 * @brief Get candidate antecedents for an anaphor
 * @param manager Manager instance
 * @param anaphor_type Type of anaphor
 * @param gender Required gender (0 for any)
 * @param number Required number (0 for any)
 * @param candidates Output array
 * @param max_candidates Maximum to return
 * @return Number of candidates found
 */
uint32_t discourse_get_antecedent_candidates(
    const discourse_manager_t* manager,
    anaphora_type_t anaphor_type,
    uint8_t gender,
    uint8_t number,
    discourse_referent_t* candidates,
    uint32_t max_candidates
);

/*=============================================================================
 * TOPIC MANAGEMENT
 *===========================================================================*/

/**
 * @brief Introduce a new topic
 * @param manager Manager instance
 * @param name Topic name/description
 * @return Topic ID or 0 on failure
 */
uint32_t discourse_introduce_topic(
    discourse_manager_t* manager,
    const char* name
);

/**
 * @brief Get current topic
 * @param manager Manager instance
 * @param topic Output topic data
 * @return true if there is a current topic
 */
bool discourse_get_current_topic(
    const discourse_manager_t* manager,
    discourse_topic_t* topic
);

/**
 * @brief Set current topic
 * @param manager Manager instance
 * @param topic_id Topic to make current
 * @return true on success
 */
bool discourse_set_current_topic(
    discourse_manager_t* manager,
    uint32_t topic_id
);

/**
 * @brief Detect topic shift
 * @param manager Manager instance
 * @param utterance New utterance
 * @param shift_type Output shift type
 * @return Confidence in shift detection
 */
float discourse_detect_topic_shift(
    discourse_manager_t* manager,
    const char* utterance,
    topic_shift_t* shift_type
);

/**
 * @brief Get recent topics
 * @param manager Manager instance
 * @param topics Output array
 * @param max_topics Maximum to retrieve
 * @return Number of topics retrieved
 */
uint32_t discourse_get_recent_topics(
    const discourse_manager_t* manager,
    discourse_topic_t* topics,
    uint32_t max_topics
);

/*=============================================================================
 * TURN MANAGEMENT
 *===========================================================================*/

/**
 * @brief Add a turn to the discourse
 * @param manager Manager instance
 * @param speaker_id Speaker identifier
 * @param content Turn content
 * @param timestamp_ms Timestamp
 * @return Turn ID or 0 on failure
 */
uint32_t discourse_add_turn(
    discourse_manager_t* manager,
    uint32_t speaker_id,
    const char* content,
    uint64_t timestamp_ms
);

/**
 * @brief Get turn by ID
 * @param manager Manager instance
 * @param turn_id Turn identifier
 * @param turn Output turn data
 * @return true if found
 */
bool discourse_get_turn(
    const discourse_manager_t* manager,
    uint32_t turn_id,
    discourse_turn_t* turn
);

/**
 * @brief Get recent turns
 * @param manager Manager instance
 * @param turns Output array
 * @param max_turns Maximum to retrieve
 * @return Number of turns retrieved
 */
uint32_t discourse_get_recent_turns(
    const discourse_manager_t* manager,
    discourse_turn_t* turns,
    uint32_t max_turns
);

/**
 * @brief Get current turn count
 * @param manager Manager instance
 * @return Number of turns in discourse
 */
uint32_t discourse_get_turn_count(const discourse_manager_t* manager);

/*=============================================================================
 * COHERENCE ANALYSIS
 *===========================================================================*/

/**
 * @brief Analyze coherence between turns
 * @param manager Manager instance
 * @param turn1_id First turn
 * @param turn2_id Second turn
 * @param relation Output coherence relation
 * @return Coherence score (0-1)
 */
float discourse_analyze_coherence(
    const discourse_manager_t* manager,
    uint32_t turn1_id,
    uint32_t turn2_id,
    coherence_relation_t* relation
);

/**
 * @brief Get overall discourse coherence
 * @param manager Manager instance
 * @return Overall coherence score (0-1)
 */
float discourse_get_overall_coherence(const discourse_manager_t* manager);

/*=============================================================================
 * FULL ANALYSIS
 *===========================================================================*/

/**
 * @brief Perform full discourse analysis on an utterance
 * @param manager Manager instance
 * @param speaker_id Speaker identifier
 * @param utterance Utterance to analyze
 * @param timestamp_ms Timestamp
 * @param analysis Output analysis result
 * @return true on success
 */
bool discourse_analyze(
    discourse_manager_t* manager,
    uint32_t speaker_id,
    const char* utterance,
    uint64_t timestamp_ms,
    discourse_analysis_t* analysis
);

/*=============================================================================
 * STATUS AND STATISTICS
 *===========================================================================*/

/**
 * @brief Get current status
 * @param manager Manager instance
 * @return Current status
 */
discourse_status_t discourse_get_status(const discourse_manager_t* manager);

/**
 * @brief Get last error
 * @param manager Manager instance
 * @return Last error code
 */
discourse_error_t discourse_get_last_error(const discourse_manager_t* manager);

/**
 * @brief Get statistics
 * @param manager Manager instance
 * @param stats Output statistics
 * @return true on success
 */
bool discourse_get_stats(const discourse_manager_t* manager, discourse_stats_t* stats);

/**
 * @brief Reset statistics
 * @param manager Manager instance
 */
void discourse_reset_stats(discourse_manager_t* manager);

/**
 * @brief Get configuration
 * @param manager Manager instance
 * @param config Output configuration
 * @return true on success
 */
bool discourse_get_config(const discourse_manager_t* manager, discourse_config_t* config);

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

/**
 * @brief Register bio-async message handler
 * @param manager Manager instance
 * @param router Bio router instance
 * @return true on success
 */
bool discourse_register_bio_handler(
    discourse_manager_t* manager,
    bio_router_t* router
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DISCOURSE_MANAGER_H */
