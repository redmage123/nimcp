//=============================================================================
// nimcp_language_prefrontal_bridge.h - Language-Prefrontal Executive Bridge
//=============================================================================
/**
 * @file nimcp_language_prefrontal_bridge.h
 * @brief Bidirectional bridge integrating Language Layer with Prefrontal Cortex
 *
 * WHAT: Bridge connecting language processing with executive control
 * WHY:  Enable goal-directed speech, discourse planning, language switching,
 *       and inhibitory control over inappropriate utterances
 * HOW:  Prefrontal provides goals and inhibition; Language reports production
 *       status and requests executive decisions
 *
 * BIOLOGICAL BASIS:
 * - Dorsolateral PFC (BA9/46): Discourse planning, verbal working memory
 * - Broca's Area (BA44/45): Speech production under executive control
 * - Anterior Cingulate (BA32): Speech error monitoring, conflict detection
 * - Inferior Frontal Gyrus: Language selection, bilingual control
 *
 * KEY CONNECTIONS:
 * - Prefrontal → Broca: Communication goals, utterance planning, inhibition
 * - Broca → Prefrontal: Production status, conflict signals, completion
 * - Prefrontal → Wernicke: Attention direction, comprehension goals
 * - Wernicke → Prefrontal: Comprehension status, ambiguity signals
 *
 * EXECUTIVE FUNCTIONS FOR LANGUAGE:
 * - Goal-directed speech: What to communicate and why
 * - Discourse planning: Multi-utterance conversation structure
 * - Language switching: Bilingual/register control
 * - Inhibitory control: Suppress inappropriate words/topics
 * - Conflict resolution: Competing word choices, ambiguity
 *
 * @version 1.0.0 - Phase LP1: Language-Prefrontal Integration
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#ifndef NIMCP_LANGUAGE_PREFRONTAL_BRIDGE_H
#define NIMCP_LANGUAGE_PREFRONTAL_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

/* Include prefrontal adapter first to avoid include order issues */
#include "core/brain/regions/prefrontal/nimcp_prefrontal_adapter.h"

/* Language types after bio_messages */
#include "language/nimcp_language_types.h"
#include "language/nimcp_language_config.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct language_prefrontal_bridge language_prefrontal_bridge_t;
typedef struct language_orchestrator language_orchestrator_t;
typedef struct broca_adapter broca_adapter_t;
typedef struct wernicke_adapter wernicke_adapter_t;

#ifndef NIMCP_BIO_ROUTER_H
typedef void* bio_router_t;
#endif

//=============================================================================
// Constants
//=============================================================================

#define LANGUAGE_PREFRONTAL_MODULE_NAME      "language_prefrontal_bridge"
#define LANGUAGE_PREFRONTAL_MODULE_VERSION   "1.0.0"
#define LANGUAGE_PREFRONTAL_BIO_MODULE_ID    0x0821

/* Default configuration values */
#define LP_DEFAULT_UPDATE_INTERVAL_MS        20
#define LP_DEFAULT_MAX_DISCOURSE_GOALS       8
#define LP_DEFAULT_MAX_UTTERANCE_QUEUE       16
#define LP_DEFAULT_INHIBITION_THRESHOLD      0.7f
#define LP_DEFAULT_CONFLICT_THRESHOLD        0.5f
#define LP_DEFAULT_PLANNING_HORIZON          5

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Communication goal types
 */
typedef enum {
    COMM_GOAL_INFORM = 0,         /**< Convey information */
    COMM_GOAL_REQUEST,            /**< Request action/information */
    COMM_GOAL_COMMAND,            /**< Issue directive */
    COMM_GOAL_QUESTION,           /**< Seek information */
    COMM_GOAL_ACKNOWLEDGE,        /**< Acknowledge receipt */
    COMM_GOAL_SOCIALIZE,          /**< Social interaction */
    COMM_GOAL_EXPRESS,            /**< Express emotion/opinion */
    COMM_GOAL_CLARIFY,            /**< Clarify previous utterance */
    COMM_GOAL_COUNT
} communication_goal_type_t;

/**
 * @brief Discourse state
 */
typedef enum {
    DISCOURSE_IDLE = 0,           /**< No active discourse */
    DISCOURSE_INITIATING,         /**< Starting conversation */
    DISCOURSE_TURN_TAKING,        /**< Active turn exchange */
    DISCOURSE_HOLDING_FLOOR,      /**< Extended speaking turn */
    DISCOURSE_YIELDING,           /**< Yielding speaking turn */
    DISCOURSE_CLOSING,            /**< Ending conversation */
    DISCOURSE_COUNT
} discourse_state_t;

/**
 * @brief Language inhibition type
 */
typedef enum {
    INHIBIT_NONE = 0,             /**< No inhibition */
    INHIBIT_WORD,                 /**< Inhibit specific word */
    INHIBIT_TOPIC,                /**< Inhibit topic area */
    INHIBIT_REGISTER,             /**< Inhibit language register */
    INHIBIT_EMOTION,              /**< Inhibit emotional expression */
    INHIBIT_ALL,                  /**< Full speech inhibition */
    INHIBIT_COUNT
} inhibition_type_t;

/**
 * @brief Conflict type in language production
 */
typedef enum {
    CONFLICT_NONE = 0,            /**< No conflict */
    CONFLICT_WORD_SELECTION,      /**< Competing word choices */
    CONFLICT_GOAL_PRIORITY,       /**< Competing communication goals */
    CONFLICT_REGISTER_MISMATCH,   /**< Register inappropriate for context */
    CONFLICT_TOPIC_TABOO,         /**< Topic violates social norms */
    CONFLICT_TIMING,              /**< Turn-taking conflict */
    CONFLICT_COUNT
} language_conflict_type_t;

/**
 * @brief Bridge operating state
 */
typedef enum {
    LP_STATE_IDLE = 0,            /**< No active processing */
    LP_STATE_GOAL_ACTIVE,         /**< Communication goal active */
    LP_STATE_PLANNING,            /**< Planning utterance */
    LP_STATE_MONITORING,          /**< Monitoring production */
    LP_STATE_INHIBITING,          /**< Inhibition active */
    LP_STATE_CONFLICT,            /**< Conflict resolution */
    LP_STATE_ERROR,               /**< Error state */
    LP_STATE_COUNT
} lp_bridge_state_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Configuration for Language-Prefrontal bridge
 */
typedef struct {
    /* Operating parameters */
    uint32_t update_interval_ms;          /**< Update cycle interval */
    uint32_t max_discourse_goals;         /**< Maximum concurrent goals */
    uint32_t max_utterance_queue;         /**< Utterance queue size */

    /* Executive control */
    float inhibition_threshold;           /**< Threshold to trigger inhibition */
    float conflict_threshold;             /**< Threshold for conflict detection */
    uint32_t planning_horizon;            /**< Discourse planning steps */

    /* Features */
    bool enable_goal_directed_speech;     /**< Enable goal-based production */
    bool enable_discourse_planning;       /**< Enable multi-turn planning */
    bool enable_inhibitory_control;       /**< Enable speech inhibition */
    bool enable_conflict_monitoring;      /**< Enable conflict detection */
    bool enable_language_switching;       /**< Enable code-switching control */

    /* Bio-async */
    bool enable_bio_async;                /**< Enable bio-async messaging */
} language_prefrontal_config_t;

/**
 * @brief Communication goal from prefrontal
 */
typedef struct {
    uint32_t goal_id;                     /**< Unique goal identifier */
    communication_goal_type_t type;       /**< Goal type */
    char content[256];                    /**< Goal content/topic */
    float priority;                       /**< Priority [0-1] */
    float urgency;                        /**< Time pressure [0-1] */
    uint64_t deadline_ms;                 /**< Deadline (0 = none) */
    uint32_t target_audience;             /**< Intended recipient ID */
    uint8_t register_level;               /**< Formality register [0-4] */
    bool requires_response;               /**< Expects reply */
} communication_goal_t;

/**
 * @brief Utterance plan from executive control
 */
typedef struct {
    uint32_t plan_id;                     /**< Plan identifier */
    uint32_t goal_id;                     /**< Associated goal */
    char outline[512];                    /**< Utterance outline */
    float confidence;                     /**< Plan confidence [0-1] */
    uint32_t estimated_words;             /**< Estimated word count */
    uint32_t estimated_duration_ms;       /**< Estimated duration */
    bool is_question;                     /**< Ends with question */
    bool allows_interruption;             /**< Can be interrupted */
} utterance_plan_t;

/**
 * @brief Inhibition signal
 */
typedef struct {
    inhibition_type_t type;               /**< Inhibition type */
    char target[64];                      /**< What to inhibit */
    float strength;                       /**< Inhibition strength [0-1] */
    uint64_t duration_ms;                 /**< Duration (0 = indefinite) */
    char reason[128];                     /**< Reason for inhibition */
} inhibition_signal_t;

/**
 * @brief Conflict report from language to prefrontal
 */
typedef struct {
    language_conflict_type_t type;        /**< Conflict type */
    char description[256];                /**< Conflict description */
    float severity;                       /**< Severity [0-1] */
    uint32_t option_count;                /**< Number of competing options */
    char options[4][64];                  /**< Competing options */
    float option_values[4];               /**< Value of each option */
    bool needs_resolution;                /**< Requires executive decision */
} language_conflict_t;

/**
 * @brief Production status report to prefrontal
 */
typedef struct {
    uint32_t goal_id;                     /**< Goal being addressed */
    float progress;                       /**< Completion progress [0-1] */
    bool is_speaking;                     /**< Currently producing speech */
    uint32_t words_produced;              /**< Words produced so far */
    float fluency;                        /**< Fluency score [0-1] */
    bool encountered_error;               /**< Production error occurred */
    char current_word[32];                /**< Current word being produced */
} production_status_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Counts */
    uint64_t goals_received;              /**< Communication goals received */
    uint64_t goals_completed;             /**< Goals successfully completed */
    uint64_t plans_generated;             /**< Utterance plans created */
    uint64_t inhibitions_triggered;       /**< Times inhibition activated */
    uint64_t conflicts_detected;          /**< Conflicts detected */
    uint64_t conflicts_resolved;          /**< Conflicts resolved */

    /* Timing */
    float avg_goal_completion_ms;         /**< Average goal completion time */
    float avg_planning_latency_ms;        /**< Average planning time */
    float avg_conflict_resolution_ms;     /**< Average conflict resolution */

    /* Quality */
    float avg_production_fluency;         /**< Average fluency score */
    float avg_goal_success_rate;          /**< Goal completion rate */

    /* Current state */
    uint32_t active_goals;                /**< Currently active goals */
    lp_bridge_state_t current_state;      /**< Current bridge state */
} language_prefrontal_stats_t;

//=============================================================================
// Callback Types
//=============================================================================

typedef void (*lp_goal_callback_t)(const communication_goal_t* goal, void* user_data);
typedef void (*lp_inhibition_callback_t)(const inhibition_signal_t* signal, void* user_data);
typedef void (*lp_conflict_callback_t)(const language_conflict_t* conflict, void* user_data);

//=============================================================================
// Lifecycle Functions
//=============================================================================

language_prefrontal_config_t language_prefrontal_default_config(void);

language_prefrontal_bridge_t* language_prefrontal_bridge_create(
    language_orchestrator_t* language,
    prefrontal_adapter_t* prefrontal,
    const language_prefrontal_config_t* config
);

void language_prefrontal_bridge_destroy(language_prefrontal_bridge_t* bridge);

int language_prefrontal_bridge_reset(language_prefrontal_bridge_t* bridge);

//=============================================================================
// Connection Functions
//=============================================================================

int language_prefrontal_connect_broca(
    language_prefrontal_bridge_t* bridge,
    broca_adapter_t* broca
);

int language_prefrontal_connect_wernicke(
    language_prefrontal_bridge_t* bridge,
    wernicke_adapter_t* wernicke
);

int language_prefrontal_connect_bio_async(
    language_prefrontal_bridge_t* bridge,
    bio_router_t router
);

//=============================================================================
// Update Functions
//=============================================================================

int language_prefrontal_bridge_update(
    language_prefrontal_bridge_t* bridge,
    uint64_t timestamp_ms
);

//=============================================================================
// Goal Management (Prefrontal → Language)
//=============================================================================

int language_prefrontal_set_communication_goal(
    language_prefrontal_bridge_t* bridge,
    const communication_goal_t* goal
);

int language_prefrontal_cancel_goal(
    language_prefrontal_bridge_t* bridge,
    uint32_t goal_id
);

int language_prefrontal_get_active_goals(
    language_prefrontal_bridge_t* bridge,
    communication_goal_t* goals,
    uint32_t max_goals
);

//=============================================================================
// Utterance Planning (Prefrontal → Language)
//=============================================================================

int language_prefrontal_submit_utterance_plan(
    language_prefrontal_bridge_t* bridge,
    const utterance_plan_t* plan
);

int language_prefrontal_modify_plan(
    language_prefrontal_bridge_t* bridge,
    uint32_t plan_id,
    const utterance_plan_t* modified_plan
);

//=============================================================================
// Inhibitory Control (Prefrontal → Language)
//=============================================================================

int language_prefrontal_apply_inhibition(
    language_prefrontal_bridge_t* bridge,
    const inhibition_signal_t* signal
);

int language_prefrontal_release_inhibition(
    language_prefrontal_bridge_t* bridge,
    inhibition_type_t type
);

bool language_prefrontal_is_inhibited(
    const language_prefrontal_bridge_t* bridge,
    const char* word_or_topic
);

//=============================================================================
// Status Reporting (Language → Prefrontal)
//=============================================================================

int language_prefrontal_report_production_status(
    language_prefrontal_bridge_t* bridge,
    const production_status_t* status
);

int language_prefrontal_report_goal_complete(
    language_prefrontal_bridge_t* bridge,
    uint32_t goal_id,
    bool success
);

int language_prefrontal_report_conflict(
    language_prefrontal_bridge_t* bridge,
    const language_conflict_t* conflict
);

int language_prefrontal_request_decision(
    language_prefrontal_bridge_t* bridge,
    const language_conflict_t* conflict,
    uint32_t* selected_option
);

//=============================================================================
// Discourse Management
//=============================================================================

discourse_state_t language_prefrontal_get_discourse_state(
    const language_prefrontal_bridge_t* bridge
);

int language_prefrontal_set_discourse_state(
    language_prefrontal_bridge_t* bridge,
    discourse_state_t state
);

//=============================================================================
// Callback Registration
//=============================================================================

int language_prefrontal_set_goal_callback(
    language_prefrontal_bridge_t* bridge,
    lp_goal_callback_t callback,
    void* user_data
);

int language_prefrontal_set_inhibition_callback(
    language_prefrontal_bridge_t* bridge,
    lp_inhibition_callback_t callback,
    void* user_data
);

int language_prefrontal_set_conflict_callback(
    language_prefrontal_bridge_t* bridge,
    lp_conflict_callback_t callback,
    void* user_data
);

//=============================================================================
// Status and Statistics
//=============================================================================

lp_bridge_state_t language_prefrontal_get_state(
    const language_prefrontal_bridge_t* bridge
);

int language_prefrontal_get_stats(
    const language_prefrontal_bridge_t* bridge,
    language_prefrontal_stats_t* stats
);

void language_prefrontal_reset_stats(language_prefrontal_bridge_t* bridge);

int language_prefrontal_get_config(
    const language_prefrontal_bridge_t* bridge,
    language_prefrontal_config_t* config
);

int language_prefrontal_set_config(
    language_prefrontal_bridge_t* bridge,
    const language_prefrontal_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LANGUAGE_PREFRONTAL_BRIDGE_H */
