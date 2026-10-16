/**
 * @file nimcp_pragmatics_processor.h
 * @brief Pragmatics processor for contextual language understanding
 *
 * WHAT: Speech act recognition and Gricean maxim processing
 * WHY:  Enable understanding of speaker intent beyond literal meaning
 * HOW:  Classify speech acts, detect implicature, and track conversational context
 *
 * ARCHITECTURE:
 * - Speech Act Classifier: Identifies illocutionary force (assertive, directive, etc.)
 * - Gricean Maxim Analyzer: Evaluates quantity, quality, relation, and manner
 * - Implicature Detector: Infers implied meanings from context
 * - Indirect Speech Handler: Processes requests like "Can you...?" as directives
 *
 * BIOLOGICAL BASIS:
 * - Models right hemisphere's role in pragmatic processing
 * - Integrates prefrontal cortex for theory of mind aspects
 * - Connections to emotional processing for social cognition
 *
 * SPEECH ACT TYPES (Austin/Searle):
 * - Assertive: Stating facts, claiming, concluding
 * - Directive: Commands, requests, suggestions
 * - Commissive: Promises, offers, threats
 * - Expressive: Thanking, apologizing, congratulating
 * - Declarative: Declaring, announcing, naming
 *
 * @version Phase B4: Speech Enhancement
 * @date 2026-01-15
 */

#ifndef NIMCP_PRAGMATICS_PROCESSOR_H
#define NIMCP_PRAGMATICS_PROCESSOR_H

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

/* Forward declaration for opaque type */
typedef struct pragmatics_processor pragmatics_processor_t;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define PRAGMATICS_DEFAULT_CONTEXT_DEPTH       8
#define PRAGMATICS_DEFAULT_MAX_UTTERANCES     64
#define PRAGMATICS_DEFAULT_MAX_SPEECH_ACTS    32
#define PRAGMATICS_DEFAULT_IMPLICATURE_DEPTH   4
#define PRAGMATICS_MAX_CONTEXT_FEATURES       16
#define PRAGMATICS_MAX_PARTICIPANTS            8

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Speech act types (Austin/Searle classification)
 */
typedef enum {
    SPEECH_ACT_UNKNOWN = 0,

    /* Assertives - commit speaker to truth of proposition */
    SPEECH_ACT_ASSERT,           /**< State a fact */
    SPEECH_ACT_CLAIM,            /**< Make a claim */
    SPEECH_ACT_CONCLUDE,         /**< Draw a conclusion */
    SPEECH_ACT_REPORT,           /**< Report information */
    SPEECH_ACT_SUGGEST_FACT,     /**< Suggest something is true */

    /* Directives - attempt to get hearer to do something */
    SPEECH_ACT_COMMAND,          /**< Give a command */
    SPEECH_ACT_REQUEST,          /**< Make a request */
    SPEECH_ACT_SUGGEST_ACTION,   /**< Suggest an action */
    SPEECH_ACT_ADVISE,           /**< Give advice */
    SPEECH_ACT_QUESTION,         /**< Ask a question */
    SPEECH_ACT_INVITE,           /**< Extend an invitation */

    /* Commissives - commit speaker to future action */
    SPEECH_ACT_PROMISE,          /**< Make a promise */
    SPEECH_ACT_OFFER,            /**< Make an offer */
    SPEECH_ACT_THREAT,           /**< Make a threat */
    SPEECH_ACT_REFUSE,           /**< Refuse something */
    SPEECH_ACT_PLEDGE,           /**< Make a pledge */

    /* Expressives - express psychological state */
    SPEECH_ACT_THANK,            /**< Express gratitude */
    SPEECH_ACT_APOLOGIZE,        /**< Express regret */
    SPEECH_ACT_CONGRATULATE,     /**< Express congratulations */
    SPEECH_ACT_COMPLAIN,         /**< Express dissatisfaction */
    SPEECH_ACT_WELCOME,          /**< Express welcome */
    SPEECH_ACT_GREET,            /**< Greet someone */

    /* Declaratives - change state of affairs */
    SPEECH_ACT_DECLARE,          /**< Make a declaration */
    SPEECH_ACT_ANNOUNCE,         /**< Make an announcement */
    SPEECH_ACT_NAME,             /**< Name or label */
    SPEECH_ACT_APPOINT,          /**< Appoint to position */

    SPEECH_ACT_COUNT
} speech_act_type_t;

/**
 * @brief Gricean maxim categories
 */
typedef enum {
    GRICE_MAXIM_QUANTITY = 0,    /**< Be as informative as required */
    GRICE_MAXIM_QUALITY,         /**< Be truthful */
    GRICE_MAXIM_RELATION,        /**< Be relevant */
    GRICE_MAXIM_MANNER,          /**< Be clear and orderly */
    GRICE_MAXIM_COUNT
} grice_maxim_t;

/**
 * @brief Maxim violation types for implicature
 */
typedef enum {
    MAXIM_OBSERVED = 0,          /**< Maxim is being followed */
    MAXIM_VIOLATED_FLOUTED,      /**< Intentionally violated for effect */
    MAXIM_VIOLATED_CLASH,        /**< Violated due to conflict */
    MAXIM_VIOLATED_OPTED_OUT,    /**< Speaker opted out */
    MAXIM_VIOLATED_INFRINGED     /**< Unintentional violation */
} maxim_violation_t;

/**
 * @brief Implicature types
 */
typedef enum {
    IMPLICATURE_NONE = 0,
    IMPLICATURE_CONVERSATIONAL,  /**< Context-dependent inference */
    IMPLICATURE_CONVENTIONAL,    /**< Word-meaning based inference */
    IMPLICATURE_SCALAR,          /**< Scale-based inference (some->not all) */
    IMPLICATURE_PARTICULARIZED,  /**< Highly context-dependent */
    IMPLICATURE_GENERALIZED      /**< Context-independent */
} implicature_type_t;

/**
 * @brief Processing status
 */
typedef enum {
    PRAGMATICS_STATUS_IDLE = 0,
    PRAGMATICS_STATUS_ANALYZING,
    PRAGMATICS_STATUS_CLASSIFYING,
    PRAGMATICS_STATUS_INFERRING,
    PRAGMATICS_STATUS_READY,
    PRAGMATICS_STATUS_ERROR
} pragmatics_status_t;

/**
 * @brief Error codes
 */
typedef enum {
    PRAGMATICS_ERROR_NONE = 0,
    PRAGMATICS_ERROR_INVALID_INPUT,
    PRAGMATICS_ERROR_CONTEXT_FULL,
    PRAGMATICS_ERROR_CLASSIFICATION_FAILED,
    PRAGMATICS_ERROR_INTERNAL
} pragmatics_error_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Pragmatics processor configuration
 */
typedef struct {
    /* Context tracking */
    uint32_t context_depth;          /**< How many turns to remember */
    uint32_t max_utterances;         /**< Maximum utterances in buffer */

    /* Speech act classification */
    uint32_t max_speech_acts;        /**< Maximum speech acts per utterance */
    float classification_threshold;   /**< Confidence threshold for classification */

    /* Implicature detection */
    uint32_t implicature_depth;      /**< Depth of inference chain */
    bool enable_scalar_implicature;  /**< Enable "some" -> "not all" inferences */
    bool enable_indirect_speech;     /**< Enable indirect speech act detection */

    /* Gricean analysis */
    bool enable_grice_analysis;      /**< Enable maxim violation detection */
    float grice_sensitivity;         /**< Sensitivity to violations (0-1) */

    /* Integration */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    bool enable_emotion_integration; /**< Integrate with emotion system */
    bool enable_working_memory;      /**< Use working memory for context */
} pragmatics_config_t;

/**
 * @brief Speech act analysis result
 */
typedef struct {
    speech_act_type_t primary_act;   /**< Primary speech act */
    speech_act_type_t secondary_act; /**< Secondary act (if indirect) */
    float primary_confidence;        /**< Confidence in primary classification */
    float secondary_confidence;      /**< Confidence in secondary classification */
    bool is_indirect;                /**< Is this an indirect speech act */
    bool is_performative;            /**< Uses explicit performative verb */
    uint32_t target_participant;     /**< Who the act is directed at */
} speech_act_result_t;

/**
 * @brief Gricean maxim analysis result
 */
typedef struct {
    maxim_violation_t violations[GRICE_MAXIM_COUNT];  /**< Violation per maxim */
    float adherence_scores[GRICE_MAXIM_COUNT];        /**< Adherence score (0-1) */
    float overall_cooperativeness;                     /**< Overall cooperative score */
    bool flouting_detected;                            /**< Intentional flouting */
} grice_analysis_result_t;

/**
 * @brief Implicature detection result
 */
typedef struct {
    implicature_type_t type;         /**< Type of implicature */
    float confidence;                /**< Confidence in inference */
    char implied_content[256];       /**< Inferred meaning */
    grice_maxim_t triggered_by;      /**< Which maxim violation triggered it */
} implicature_result_t;

/**
 * @brief Utterance context entry
 */
typedef struct {
    uint32_t utterance_id;           /**< Unique identifier */
    uint32_t speaker_id;             /**< Who said it */
    uint64_t timestamp_ms;           /**< When it was said */
    speech_act_result_t speech_act;  /**< Speech act analysis */
    float salience;                  /**< Contextual salience (0-1) */
    char content[256];               /**< Utterance content */
} utterance_context_t;

/**
 * @brief Full pragmatic analysis result
 */
typedef struct {
    /* Speech act analysis */
    speech_act_result_t speech_act;

    /* Gricean analysis */
    grice_analysis_result_t grice_analysis;

    /* Implicature */
    implicature_result_t implicatures[PRAGMATICS_DEFAULT_IMPLICATURE_DEPTH];
    uint32_t implicature_count;

    /* Context relevance */
    float context_relevance;         /**< How relevant to current context */
    uint32_t relevant_context_ids[4]; /**< IDs of relevant prior utterances */
    uint32_t relevant_context_count;

    /* Processing metadata */
    double processing_time_ms;
} pragmatic_analysis_t;

/**
 * @brief Statistics
 */
typedef struct {
    uint64_t utterances_processed;
    uint64_t speech_acts_classified;
    uint64_t implicatures_detected;
    uint64_t indirect_acts_detected;
    uint64_t maxim_violations_found;
    double avg_processing_time_ms;
    double total_processing_time_ms;

    /* Per-act type counts */
    uint64_t act_type_counts[SPEECH_ACT_COUNT];
} pragmatics_stats_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default configuration
 * @return Default configuration structure
 */
pragmatics_config_t pragmatics_default_config(void);

/**
 * @brief Create pragmatics processor
 * @param config Configuration (NULL for defaults)
 * @return New processor instance or NULL on failure
 */
pragmatics_processor_t* pragmatics_create(const pragmatics_config_t* config);

/**
 * @brief Destroy pragmatics processor
 * @param processor Processor to destroy
 */
void pragmatics_destroy(pragmatics_processor_t* processor);

/**
 * @brief Reset processor state
 * @param processor Processor to reset
 * @return true on success
 */
bool pragmatics_reset(pragmatics_processor_t* processor);

/*=============================================================================
 * SPEECH ACT CLASSIFICATION
 *===========================================================================*/

/**
 * @brief Classify speech act of an utterance
 * @param processor Processor instance
 * @param utterance Utterance text
 * @param speaker_id Speaker identifier
 * @param result Output result structure
 * @return true on success
 */
bool pragmatics_classify_speech_act(
    pragmatics_processor_t* processor,
    const char* utterance,
    uint32_t speaker_id,
    speech_act_result_t* result
);

/**
 * @brief Detect indirect speech acts
 * @param processor Processor instance
 * @param utterance Utterance text
 * @param surface_act Apparent speech act
 * @param indirect_act Output: detected indirect act
 * @return Confidence in indirect act detection (0-1)
 */
float pragmatics_detect_indirect_act(
    pragmatics_processor_t* processor,
    const char* utterance,
    speech_act_type_t surface_act,
    speech_act_type_t* indirect_act
);

/**
 * @brief Get speech act name string
 * @param act Speech act type
 * @return String name
 */
const char* pragmatics_speech_act_name(speech_act_type_t act);

/*=============================================================================
 * GRICEAN ANALYSIS
 *===========================================================================*/

/**
 * @brief Analyze Gricean maxim adherence
 * @param processor Processor instance
 * @param utterance Utterance to analyze
 * @param context Current conversation context
 * @param result Output analysis result
 * @return true on success
 */
bool pragmatics_analyze_grice(
    pragmatics_processor_t* processor,
    const char* utterance,
    const utterance_context_t* context,
    grice_analysis_result_t* result
);

/**
 * @brief Get maxim name string
 * @param maxim Maxim type
 * @return String name
 */
const char* pragmatics_grice_maxim_name(grice_maxim_t maxim);

/*=============================================================================
 * IMPLICATURE DETECTION
 *===========================================================================*/

/**
 * @brief Detect conversational implicatures
 * @param processor Processor instance
 * @param utterance Utterance to analyze
 * @param grice_result Gricean analysis result
 * @param implicatures Output array
 * @param max_implicatures Maximum to detect
 * @return Number of implicatures detected
 */
uint32_t pragmatics_detect_implicatures(
    pragmatics_processor_t* processor,
    const char* utterance,
    const grice_analysis_result_t* grice_result,
    implicature_result_t* implicatures,
    uint32_t max_implicatures
);

/**
 * @brief Detect scalar implicatures specifically
 * @param processor Processor instance
 * @param utterance Utterance to analyze
 * @param result Output implicature result
 * @return true if scalar implicature detected
 */
bool pragmatics_detect_scalar_implicature(
    pragmatics_processor_t* processor,
    const char* utterance,
    implicature_result_t* result
);

/*=============================================================================
 * CONTEXT MANAGEMENT
 *===========================================================================*/

/**
 * @brief Add utterance to context
 * @param processor Processor instance
 * @param utterance Utterance text
 * @param speaker_id Speaker identifier
 * @param timestamp_ms Timestamp
 * @return Assigned utterance ID
 */
uint32_t pragmatics_add_to_context(
    pragmatics_processor_t* processor,
    const char* utterance,
    uint32_t speaker_id,
    uint64_t timestamp_ms
);

/**
 * @brief Get recent context
 * @param processor Processor instance
 * @param context Output array
 * @param max_entries Maximum entries to retrieve
 * @return Number of entries retrieved
 */
uint32_t pragmatics_get_context(
    pragmatics_processor_t* processor,
    utterance_context_t* context,
    uint32_t max_entries
);

/**
 * @brief Clear context history
 * @param processor Processor instance
 */
void pragmatics_clear_context(pragmatics_processor_t* processor);

/**
 * @brief Register conversation participant
 * @param processor Processor instance
 * @param participant_id Participant identifier
 * @param name Participant name
 * @return true on success
 */
bool pragmatics_register_participant(
    pragmatics_processor_t* processor,
    uint32_t participant_id,
    const char* name
);

/*=============================================================================
 * FULL ANALYSIS
 *===========================================================================*/

/**
 * @brief Perform full pragmatic analysis
 * @param processor Processor instance
 * @param utterance Utterance to analyze
 * @param speaker_id Speaker identifier
 * @param timestamp_ms Timestamp
 * @param analysis Output full analysis
 * @return true on success
 */
bool pragmatics_analyze(
    pragmatics_processor_t* processor,
    const char* utterance,
    uint32_t speaker_id,
    uint64_t timestamp_ms,
    pragmatic_analysis_t* analysis
);

/*=============================================================================
 * STATUS AND STATISTICS
 *===========================================================================*/

/**
 * @brief Get current status
 * @param processor Processor instance
 * @return Current status
 */
pragmatics_status_t pragmatics_get_status(const pragmatics_processor_t* processor);

/**
 * @brief Get last error
 * @param processor Processor instance
 * @return Last error code
 */
pragmatics_error_t pragmatics_get_last_error(const pragmatics_processor_t* processor);

/**
 * @brief Get statistics
 * @param processor Processor instance
 * @param stats Output statistics
 * @return true on success
 */
bool pragmatics_get_stats(const pragmatics_processor_t* processor, pragmatics_stats_t* stats);

/**
 * @brief Reset statistics
 * @param processor Processor instance
 */
void pragmatics_reset_stats(pragmatics_processor_t* processor);

/**
 * @brief Get configuration
 * @param processor Processor instance
 * @param config Output configuration
 * @return true on success
 */
bool pragmatics_get_config(const pragmatics_processor_t* processor, pragmatics_config_t* config);

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

/**
 * @brief Register bio-async message handler
 * @param processor Processor instance
 * @param router Bio router instance
 * @return true on success
 */
bool pragmatics_register_bio_handler(
    pragmatics_processor_t* processor,
    bio_router_t* router
);

/**
 * @brief Send pragmatic analysis via bio-async
 * @param processor Processor instance
 * @param analysis Analysis to send
 * @return true on success
 */
bool pragmatics_send_analysis(
    pragmatics_processor_t* processor,
    const pragmatic_analysis_t* analysis
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PRAGMATICS_PROCESSOR_H */
