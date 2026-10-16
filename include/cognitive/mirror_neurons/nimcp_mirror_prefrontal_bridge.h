/**
 * @file nimcp_mirror_prefrontal_bridge.h
 * @brief Mirror-Prefrontal Cortex Integration Bridge
 * @version 1.0.0
 * @date 2026-01-05
 *
 * WHAT: Bidirectional integration between mirror neurons and prefrontal cortex
 * WHY:  PFC provides executive control over imitation, context-appropriate mirroring,
 *       and goal-directed action planning
 * HOW:  PFC inhibits/releases mirror neuron activity, working memory stores action
 *       sequences, and goals guide imitation planning
 *
 * BIOLOGICAL BASIS:
 * The prefrontal cortex (PFC) provides top-down control over mirror neuron activity:
 *
 * - Dorsolateral PFC (BA9/46): Working memory for observed action sequences
 * - Ventrolateral PFC (BA44/45): Action selection during imitation
 * - Orbitofrontal cortex (BA11/47): Social context evaluation
 * - Anterior cingulate (BA32): Conflict monitoring between self/other actions
 *
 * EXECUTIVE CONTROL OVER IMITATION:
 * 1. Inhibitory Control: PFC suppresses inappropriate automatic imitation
 *    - Social context determines whether mirroring should translate to action
 *    - Inhibition prevents unwanted mimicry in formal contexts
 *    - Release of inhibition enables learning through imitation
 *
 * 2. Context-Appropriate Mirroring:
 *    - Social norms modulate whether observed actions should be imitated
 *    - Goals determine which aspects of observed behavior are relevant
 *    - Working memory maintains context across action sequences
 *
 * 3. Goal-Directed Imitation Planning:
 *    - PFC generates multi-step imitation plans
 *    - Hierarchical goals guide selective imitation
 *    - Action sequences stored in working memory for later reproduction
 *
 * INTEGRATION EFFECTS:
 *
 * Mirror -> PFC:
 *   - Observed actions inform goal inference
 *   - Action predictions fed to planning systems
 *   - Empathic signals influence decision-making
 *   - Motor resonance patterns evaluated for imitation
 *
 * PFC -> Mirror:
 *   - Inhibitory control over motor resonance
 *   - Goal selection modulates action observation
 *   - Working memory guides action sequence replay
 *   - Social context gates empathic responses
 *
 * @see nimcp_mirror_neurons.h - Core mirror neuron system
 * @see nimcp_prefrontal_adapter.h - Prefrontal cortex adapter
 * @see Phase 10.11 - Mirror Neurons & Social Cognition
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_MIRROR_PREFRONTAL_BRIDGE_H
#define NIMCP_MIRROR_PREFRONTAL_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/bridge/nimcp_bridge_base.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Default inhibition threshold for blocking automatic imitation */
#define MIRROR_PFC_INHIBITION_THRESHOLD_DEFAULT     0.6f

/** Default working memory capacity for action sequences */
#define MIRROR_PFC_WM_SEQUENCE_CAPACITY_DEFAULT     8

/** Default maximum actions in a stored sequence */
#define MIRROR_PFC_WM_MAX_ACTIONS_PER_SEQUENCE      16

/** Default goal relevance threshold for selective mirroring */
#define MIRROR_PFC_GOAL_RELEVANCE_THRESHOLD         0.5f

/** Default social context sensitivity */
#define MIRROR_PFC_SOCIAL_SENSITIVITY_DEFAULT       0.7f

/** Bio-async module ID for this bridge */
#define BIO_MODULE_MIRROR_PREFRONTAL_BRIDGE_ID      0x027E

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Social context types affecting mirror-PFC interaction
 */
typedef enum {
    SOCIAL_CONTEXT_NONE = 0,        /**< No social context */
    SOCIAL_CONTEXT_FORMAL,          /**< Formal setting - high inhibition */
    SOCIAL_CONTEXT_CASUAL,          /**< Casual interaction - moderate inhibition */
    SOCIAL_CONTEXT_LEARNING,        /**< Learning context - low inhibition */
    SOCIAL_CONTEXT_PLAYFUL,         /**< Playful context - minimal inhibition */
    SOCIAL_CONTEXT_EMERGENCY        /**< Emergency - bypass inhibition */
} social_context_type_t;

/**
 * @brief Imitation mode determining PFC control level
 */
typedef enum {
    IMITATION_MODE_BLOCKED = 0,     /**< All imitation blocked by PFC */
    IMITATION_MODE_SELECTIVE,       /**< Goal-relevant imitation only */
    IMITATION_MODE_CONTEXTUAL,      /**< Context-appropriate imitation */
    IMITATION_MODE_UNRESTRICTED     /**< Learning mode - minimal blocking */
} imitation_mode_t;

/**
 * @brief Configuration for mirror-prefrontal bridge
 */
typedef struct {
    /* Inhibitory control parameters */
    float inhibition_threshold;         /**< Threshold for blocking imitation [0,1] */
    float inhibition_decay_rate;        /**< Rate of inhibition decay per step */
    bool enable_automatic_inhibition;   /**< Auto-inhibit based on context */

    /* Working memory integration */
    uint32_t wm_sequence_capacity;      /**< Max action sequences in working memory */
    uint32_t wm_max_actions_per_seq;    /**< Max actions per sequence */
    bool enable_wm_integration;         /**< Enable working memory storage */
    float wm_priority_boost;            /**< Priority boost for observed sequences */

    /* Goal-directed control */
    float goal_relevance_threshold;     /**< Min relevance for goal-guided mirroring */
    bool enable_goal_filtering;         /**< Filter mirroring by active goals */
    bool enable_hierarchical_goals;     /**< Use hierarchical goal structure */

    /* Social context */
    float social_sensitivity;           /**< Sensitivity to social context [0,1] */
    bool enable_social_modulation;      /**< Enable social context modulation */
    social_context_type_t default_context; /**< Default social context */

    /* Bio-async communication */
    bool enable_bio_async;              /**< Enable bio-async messaging */

    /* Debugging */
    bool enable_verbose_logging;        /**< Enable detailed logging */
} mirror_prefrontal_config_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Action sequence stored in working memory
 */
typedef struct {
    uint32_t sequence_id;               /**< Unique sequence identifier */
    uint32_t action_ids[MIRROR_PFC_WM_MAX_ACTIONS_PER_SEQUENCE]; /**< Action IDs */
    uint32_t action_count;              /**< Number of actions in sequence */
    uint32_t agent_id;                  /**< Agent who performed sequence */
    float confidence;                   /**< Confidence in sequence recognition */
    float priority;                     /**< Working memory priority */
    uint64_t timestamp;                 /**< When sequence was stored */
    uint32_t goal_id;                   /**< Associated goal (0 if none) */
} action_sequence_t;

/**
 * @brief Imitation request from mirror neurons to PFC
 */
typedef struct {
    uint32_t action_id;                 /**< Action to imitate */
    float resonance_strength;           /**< Motor resonance level */
    float goal_relevance;               /**< Relevance to current goals */
    uint32_t observed_agent_id;         /**< Who we observed */
    uint64_t timestamp;                 /**< When request was made */
} imitation_request_t;

/**
 * @brief Imitation decision from PFC
 */
typedef struct {
    bool allowed;                       /**< Whether imitation is allowed */
    float inhibition_applied;           /**< Amount of inhibition applied */
    imitation_mode_t mode;              /**< Current imitation mode */
    char reason[64];                    /**< Reason for decision */
    uint32_t goal_id;                   /**< Goal guiding decision (0 if none) */
} imitation_decision_t;

/**
 * @brief Current bridge effects on both systems
 */
typedef struct {
    /* PFC -> Mirror effects */
    float current_inhibition;           /**< Current inhibition level [0,1] */
    imitation_mode_t imitation_mode;    /**< Current imitation mode */
    social_context_type_t social_context; /**< Current social context */
    uint32_t active_goal_id;            /**< Goal guiding mirroring (0 if none) */

    /* Mirror -> PFC effects */
    float mirroring_activity;           /**< Current mirror neuron activity [0,1] */
    float empathic_signal;              /**< Empathic signal from mirror [0,1] */
    float action_prediction_confidence; /**< Confidence in predicted next action */
    uint32_t inferred_goal_id;          /**< Goal inferred from observations */

    /* Working memory state */
    uint32_t sequences_stored;          /**< Number of sequences in WM */
    bool wm_at_capacity;                /**< Whether WM is full */

    /* Integration state */
    bool bridge_active;                 /**< Whether bridge is active */
    uint64_t last_update_time;          /**< Last update timestamp */
} mirror_prefrontal_effects_t;

/**
 * @brief Statistics for mirror-prefrontal bridge
 */
typedef struct {
    /* Inhibition statistics */
    uint64_t imitations_allowed;        /**< Total imitations allowed */
    uint64_t imitations_blocked;        /**< Total imitations blocked */
    uint64_t inhibition_releases;       /**< Times inhibition was released */
    float avg_inhibition_level;         /**< Average inhibition level */

    /* Working memory statistics */
    uint64_t sequences_stored;          /**< Total sequences stored */
    uint64_t sequences_recalled;        /**< Total sequences recalled */
    uint64_t sequences_expired;         /**< Sequences expired from WM */
    float avg_sequence_length;          /**< Average actions per sequence */

    /* Goal-directed statistics */
    uint64_t goal_guided_imitations;    /**< Imitations guided by goals */
    uint64_t goals_inferred;            /**< Goals inferred from mirroring */
    float avg_goal_relevance;           /**< Average goal relevance score */

    /* Social context statistics */
    uint64_t context_switches;          /**< Social context switches */
    uint64_t context_inhibitions;       /**< Inhibitions due to context */

    /* Bio-async statistics */
    uint64_t messages_sent;             /**< Bio-async messages sent */
    uint64_t messages_received;         /**< Bio-async messages received */
} mirror_prefrontal_stats_t;

/*=============================================================================
 * OPAQUE HANDLE
 *===========================================================================*/

/**
 * @brief Opaque handle to mirror-prefrontal bridge
 */
typedef struct mirror_prefrontal_bridge_struct* mirror_prefrontal_bridge_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Initialize default configuration
 *
 * WHAT: Set sensible defaults for mirror-prefrontal integration
 * WHY:  Provide working configuration without manual setup
 *
 * @param config Configuration structure to initialize
 * @return 0 on success, -1 if config is NULL
 */
int mirror_prefrontal_default_config(mirror_prefrontal_config_t* config);

/**
 * @brief Create mirror-prefrontal bridge
 *
 * WHAT: Establish bidirectional integration between mirror neurons and PFC
 * WHY:  Enable executive control over imitation and goal-directed mirroring
 *
 * @param config Bridge configuration (NULL for defaults)
 * @param mirror Mirror neuron system handle
 * @param prefrontal Prefrontal adapter handle
 * @return Bridge handle on success, NULL on failure
 */
mirror_prefrontal_bridge_t mirror_prefrontal_bridge_create(
    const mirror_prefrontal_config_t* config,
    void* mirror,
    void* prefrontal
);

/**
 * @brief Destroy mirror-prefrontal bridge
 *
 * WHAT: Release resources and disconnect systems
 * WHY:  Prevent memory leaks, clean shutdown
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void mirror_prefrontal_bridge_destroy(mirror_prefrontal_bridge_t bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Reset bridge to initial state
 * WHY:  Clear effects without full reinitialization
 *
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int mirror_prefrontal_bridge_reset(mirror_prefrontal_bridge_t bridge);

/*=============================================================================
 * INHIBITORY CONTROL API
 *===========================================================================*/

/**
 * @brief Request permission to imitate observed action
 *
 * WHAT: Mirror neurons request PFC approval for imitation
 * WHY:  Allow PFC to block inappropriate automatic imitation
 * HOW:  Evaluate against current goals, context, and inhibition level
 *
 * @param bridge Bridge handle
 * @param request Imitation request from mirror neurons
 * @param decision Output: PFC decision on imitation
 * @return 0 on success, -1 on error
 */
int mirror_prefrontal_request_imitation(
    mirror_prefrontal_bridge_t bridge,
    const imitation_request_t* request,
    imitation_decision_t* decision
);

/**
 * @brief Set inhibition level
 *
 * WHAT: Manually set PFC inhibition over mirror neurons
 * WHY:  Allow external control of imitation suppression
 *
 * @param bridge Bridge handle
 * @param level Inhibition level [0,1]
 * @return 0 on success, -1 on error
 */
int mirror_prefrontal_set_inhibition(
    mirror_prefrontal_bridge_t bridge,
    float level
);

/**
 * @brief Get current inhibition level
 *
 * @param bridge Bridge handle
 * @return Current inhibition level [0,1], or -1.0 on error
 */
float mirror_prefrontal_get_inhibition(const mirror_prefrontal_bridge_t bridge);

/**
 * @brief Set imitation mode
 *
 * WHAT: Configure how PFC controls imitation
 * WHY:  Different modes for different contexts (learning vs formal)
 *
 * @param bridge Bridge handle
 * @param mode New imitation mode
 * @return 0 on success, -1 on error
 */
int mirror_prefrontal_set_imitation_mode(
    mirror_prefrontal_bridge_t bridge,
    imitation_mode_t mode
);

/**
 * @brief Get current imitation mode
 *
 * @param bridge Bridge handle
 * @return Current imitation mode
 */
imitation_mode_t mirror_prefrontal_get_imitation_mode(
    const mirror_prefrontal_bridge_t bridge
);

/*=============================================================================
 * SOCIAL CONTEXT API
 *===========================================================================*/

/**
 * @brief Set social context
 *
 * WHAT: Update social context affecting mirror-PFC interaction
 * WHY:  Different contexts require different inhibition levels
 *
 * BIOLOGICAL BASIS:
 * - Formal contexts: High inhibition, suppress mimicry
 * - Learning contexts: Low inhibition, allow imitation learning
 * - Emergency: Bypass inhibition for rapid response
 *
 * @param bridge Bridge handle
 * @param context New social context
 * @return 0 on success, -1 on error
 */
int mirror_prefrontal_set_social_context(
    mirror_prefrontal_bridge_t bridge,
    social_context_type_t context
);

/**
 * @brief Get current social context
 *
 * @param bridge Bridge handle
 * @return Current social context
 */
social_context_type_t mirror_prefrontal_get_social_context(
    const mirror_prefrontal_bridge_t bridge
);

/*=============================================================================
 * WORKING MEMORY INTEGRATION API
 *===========================================================================*/

/**
 * @brief Store observed action sequence in working memory
 *
 * WHAT: Store action sequence for later recall and imitation
 * WHY:  Enable delayed imitation and multi-step action learning
 *
 * @param bridge Bridge handle
 * @param sequence Action sequence to store
 * @return Sequence ID on success, 0 on failure
 */
uint32_t mirror_prefrontal_store_sequence(
    mirror_prefrontal_bridge_t bridge,
    const action_sequence_t* sequence
);

/**
 * @brief Recall action sequence from working memory
 *
 * WHAT: Retrieve stored action sequence by ID
 * WHY:  Enable reproduction of observed action sequences
 *
 * @param bridge Bridge handle
 * @param sequence_id ID of sequence to recall
 * @param sequence Output: Retrieved sequence
 * @return 0 on success, -1 if not found or error
 */
int mirror_prefrontal_recall_sequence(
    mirror_prefrontal_bridge_t bridge,
    uint32_t sequence_id,
    action_sequence_t* sequence
);

/**
 * @brief Get number of sequences in working memory
 *
 * @param bridge Bridge handle
 * @return Number of stored sequences, or 0 on error
 */
uint32_t mirror_prefrontal_get_sequence_count(
    const mirror_prefrontal_bridge_t bridge
);

/**
 * @brief Clear all sequences from working memory
 *
 * @param bridge Bridge handle
 * @return Number of sequences cleared, or -1 on error
 */
int mirror_prefrontal_clear_sequences(mirror_prefrontal_bridge_t bridge);

/*=============================================================================
 * GOAL-DIRECTED CONTROL API
 *===========================================================================*/

/**
 * @brief Set active goal for mirror-PFC integration
 *
 * WHAT: Specify goal that should guide mirror neuron activity
 * WHY:  Enable selective attention to goal-relevant actions
 *
 * @param bridge Bridge handle
 * @param goal_id Goal ID (0 to clear)
 * @return 0 on success, -1 on error
 */
int mirror_prefrontal_set_active_goal(
    mirror_prefrontal_bridge_t bridge,
    uint32_t goal_id
);

/**
 * @brief Get goal relevance for observed action
 *
 * WHAT: Evaluate how relevant an observed action is to current goals
 * WHY:  Guide selective mirroring based on goal relevance
 *
 * @param bridge Bridge handle
 * @param action_id Observed action ID
 * @return Relevance score [0,1], or -1.0 on error
 */
float mirror_prefrontal_get_goal_relevance(
    mirror_prefrontal_bridge_t bridge,
    uint32_t action_id
);

/**
 * @brief Notify PFC of goal inferred from mirroring
 *
 * WHAT: Mirror neuron hierarchy inferred a goal from observations
 * WHY:  Update PFC planning based on inferred agent intentions
 *
 * @param bridge Bridge handle
 * @param inferred_goal_id Goal ID inferred by mirror hierarchy
 * @param confidence Confidence in inference [0,1]
 * @return 0 on success, -1 on error
 */
int mirror_prefrontal_notify_goal_inference(
    mirror_prefrontal_bridge_t bridge,
    uint32_t inferred_goal_id,
    float confidence
);

/*=============================================================================
 * UPDATE AND EFFECTS API
 *===========================================================================*/

/**
 * @brief Update bridge state
 *
 * WHAT: Advance bridge simulation by one step
 * WHY:  Update effects, decay inhibition, expire WM items
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
int mirror_prefrontal_update(
    mirror_prefrontal_bridge_t bridge,
    float dt_ms
);

/**
 * @brief Get current bridge effects
 *
 * WHAT: Query current effects on both mirror and PFC systems
 * WHY:  Allow both systems to apply modulations
 *
 * @param bridge Bridge handle
 * @param effects Output: Current effects
 * @return 0 on success, -1 on error
 */
int mirror_prefrontal_get_effects(
    const mirror_prefrontal_bridge_t bridge,
    mirror_prefrontal_effects_t* effects
);

/*=============================================================================
 * BIO-ASYNC API
 *===========================================================================*/

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Enable inter-module messaging via bio-async
 * WHY:  Distributed communication with other brain modules
 * HOW:  Register with bio_router using BIO_MODULE_MIRROR_PREFRONTAL_BRIDGE
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int mirror_prefrontal_connect_bio_async(mirror_prefrontal_bridge_t bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int mirror_prefrontal_disconnect_bio_async(mirror_prefrontal_bridge_t bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool mirror_prefrontal_is_bio_async_connected(
    const mirror_prefrontal_bridge_t bridge
);

/**
 * @brief Process pending bio-async messages
 *
 * @param bridge Bridge handle
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t mirror_prefrontal_process_messages(
    mirror_prefrontal_bridge_t bridge,
    uint32_t max_messages
);

/*=============================================================================
 * STATISTICS API
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output: Statistics structure
 * @return 0 on success, -1 on error
 */
int mirror_prefrontal_get_stats(
    const mirror_prefrontal_bridge_t bridge,
    mirror_prefrontal_stats_t* stats
);

/**
 * @brief Reset statistics counters
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int mirror_prefrontal_reset_stats(mirror_prefrontal_bridge_t bridge);

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get string name for social context
 *
 * @param context Social context type
 * @return Human-readable context name
 */
const char* mirror_prefrontal_social_context_name(social_context_type_t context);

/**
 * @brief Get string name for imitation mode
 *
 * @param mode Imitation mode
 * @return Human-readable mode name
 */
const char* mirror_prefrontal_imitation_mode_name(imitation_mode_t mode);

/**
 * @brief Get default inhibition level for social context
 *
 * @param context Social context type
 * @return Default inhibition level [0,1]
 */
float mirror_prefrontal_default_inhibition_for_context(
    social_context_type_t context
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MIRROR_PREFRONTAL_BRIDGE_H */
