/**
 * @file nimcp_hypothalamus_hippocampus_bridge.h
 * @brief Hypothalamus <-> Hippocampus Bridge for Memory-Drive Integration
 *
 * WHAT: Bidirectional bridge between hypothalamus drives and hippocampal memory
 * WHY:  Drives influence what memories are encoded; memories can trigger drives
 * HOW:  Maps drive urgencies to memory encoding priority and vice versa
 *
 * BYRNES MODEL CONTEXT:
 * The steering subsystem (hypothalamus) needs to interact with the learning
 * subsystem's memory component (hippocampus). Drives bias what gets remembered,
 * and memories can trigger or modulate drives.
 *
 * HYPOTHALAMUS -> HIPPOCAMPUS:
 * - Drive urgency -> memory encoding priority (hungry = encode food-related)
 * - Emotional valence -> emotional tagging of new memories
 * - Circadian phase -> consolidation timing signals
 * - Goal state -> navigation goal context
 *
 * HIPPOCAMPUS -> HYPOTHALAMUS:
 * - Memory retrieval -> drive modulation (food memory can increase hunger)
 * - Spatial context -> drive-relevant location awareness
 * - Pattern completion -> reward prediction (anticipatory drive changes)
 * - Replay events -> reinforcement of drive-relevant sequences
 *
 * BIO-ASYNC MESSAGES:
 * - Sends: BIO_MSG_HIPPOCAMPUS_ENCODING_PRIORITY, BIO_MSG_HIPPOCAMPUS_CONSOLIDATE
 * - Receives: BIO_MSG_HIPPOCAMPUS_MEMORY_RETRIEVED, BIO_MSG_HIPPOCAMPUS_CONTEXT
 *
 * @version Phase 12: Cognitive Layer Integration
 * @date 2026-01-04
 */

#ifndef NIMCP_HYPOTHALAMUS_HIPPOCAMPUS_BRIDGE_H
#define NIMCP_HYPOTHALAMUS_HIPPOCAMPUS_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "core/brain/regions/hippocampus/nimcp_hippocampus_adapter.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/bridge/nimcp_bridge_base.h"

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Memory encoding priority scale from drives */
#define HYPO_HIPP_ENCODING_SCALE         0.5f

/** Emotional valence scale */
#define HYPO_HIPP_VALENCE_SCALE          0.3f

/** Memory-to-drive influence scale */
#define HYPO_HIPP_MEMORY_DRIVE_SCALE     0.2f

/** Maximum number of drive-memory associations */
#define HYPO_HIPP_MAX_ASSOCIATIONS       64

/** Replay-to-reward scale */
#define HYPO_HIPP_REPLAY_REWARD_SCALE    0.15f

/*=============================================================================
 * DRIVE-MEMORY RELEVANCE MAPPING
 *===========================================================================*/

/**
 * @brief Memory category for drive relevance
 */
typedef enum {
    HYPO_MEM_CAT_FOOD = 0,          /**< Food-related memories */
    HYPO_MEM_CAT_WATER,             /**< Water/drink-related memories */
    HYPO_MEM_CAT_SHELTER,           /**< Shelter/temperature-related */
    HYPO_MEM_CAT_REST,              /**< Rest/sleep-related */
    HYPO_MEM_CAT_SOCIAL,            /**< Social interaction memories */
    HYPO_MEM_CAT_THREAT,            /**< Threat/danger memories */
    HYPO_MEM_CAT_EXPLORATION,       /**< Novel/curiosity-related */
    HYPO_MEM_CAT_SKILL,             /**< Skill/competence-related */
    HYPO_MEM_CAT_NEUTRAL,           /**< No strong drive association */
    HYPO_MEM_CAT_COUNT
} hypo_memory_category_t;

/**
 * @brief Drive-to-memory category mapping
 *
 * Maps each drive type to its relevant memory category
 */
static const hypo_memory_category_t hypo_drive_to_memory_cat[HYPO_DRIVE_COUNT] = {
    HYPO_MEM_CAT_FOOD,              /* HUNGER -> Food memories */
    HYPO_MEM_CAT_WATER,             /* THIRST -> Water memories */
    HYPO_MEM_CAT_SHELTER,           /* TEMPERATURE -> Shelter memories */
    HYPO_MEM_CAT_REST,              /* FATIGUE -> Rest memories */
    HYPO_MEM_CAT_SOCIAL,            /* SOCIAL -> Social memories */
    HYPO_MEM_CAT_EXPLORATION,       /* CURIOSITY -> Exploration memories */
    HYPO_MEM_CAT_THREAT,            /* SAFETY -> Threat memories */
    HYPO_MEM_CAT_NEUTRAL,           /* AUTONOMY -> Neutral */
    HYPO_MEM_CAT_SKILL              /* COMPETENCE -> Skill memories */
};

/*=============================================================================
 * ENCODING PRIORITY STRUCTURES
 *===========================================================================*/

/**
 * @brief Memory encoding priority signal to hippocampus
 */
typedef struct {
    float priorities[HYPO_MEM_CAT_COUNT];  /**< Priority per category [0, 1] */
    float emotional_valence;                /**< Current emotional state [-1, 1] */
    float arousal_level;                    /**< Current arousal [0, 1] */
    hypo_drive_type_t dominant_drive;       /**< Currently dominant drive */
    float dominant_urgency;                 /**< Urgency of dominant drive */
    uint64_t timestamp_us;
} hypo_hipp_encoding_priority_t;

/**
 * @brief Memory consolidation signal
 */
typedef struct {
    float consolidation_urgency;           /**< How urgent consolidation is [0, 1] */
    float circadian_phase;                 /**< Current circadian phase [0, 24) */
    bool sleep_pressure_high;              /**< Sleep pressure indicates consolidation window */
    hypo_memory_category_t priority_categories[3]; /**< Top 3 categories to consolidate */
    uint64_t timestamp_us;
} hypo_hipp_consolidation_signal_t;

/**
 * @brief Navigation goal context from drives
 */
typedef struct {
    hypo_drive_type_t goal_drive;          /**< Drive motivating navigation */
    float goal_urgency;                    /**< How urgent goal is */
    hippocampus_location_t target_location; /**< Target location (if known) */
    bool location_valid;                   /**< Whether target is valid */
    float exploration_bonus;               /**< Bonus for novel locations */
    uint64_t timestamp_us;
} hypo_hipp_nav_goal_t;

/*=============================================================================
 * MEMORY FEEDBACK STRUCTURES
 *===========================================================================*/

/**
 * @brief Memory retrieval notification from hippocampus
 */
typedef struct {
    uint32_t memory_id;                    /**< Retrieved memory ID */
    hypo_memory_category_t category;       /**< Memory category */
    float emotional_valence;               /**< Memory's emotional tag */
    float strength;                        /**< Memory strength */
    float relevance_score;                 /**< Relevance to current context */
    uint64_t timestamp_us;
} hypo_hipp_memory_retrieval_t;

/**
 * @brief Spatial context from hippocampus
 */
typedef struct {
    hippocampus_location_t current_location; /**< Current position */
    float familiarity;                       /**< Location familiarity [0, 1] */
    float safety_estimate;                   /**< Estimated safety of location */
    float resource_estimate[HYPO_MEM_CAT_COUNT]; /**< Estimated resources nearby */
    uint64_t timestamp_us;
} hypo_hipp_spatial_context_t;

/**
 * @brief Replay event notification
 */
typedef struct {
    uint32_t sequence_id;                  /**< Replay sequence identifier */
    uint32_t num_memories;                 /**< Memories in replay */
    float total_reward;                    /**< Total reward in sequence */
    hypo_drive_type_t relevant_drive;      /**< Most relevant drive */
    bool forward_replay;                   /**< Forward (planning) vs reverse (credit) */
    uint64_t timestamp_us;
} hypo_hipp_replay_event_t;

/*=============================================================================
 * BRIDGE CONFIGURATION
 *===========================================================================*/

/**
 * @brief Hippocampus bridge configuration
 */
typedef struct {
    /* Encoding priority computation */
    float encoding_scale;                  /**< Drive->encoding scale factor */
    float valence_scale;                   /**< Drive->valence scale factor */
    float baseline_encoding_priority;      /**< Minimum encoding priority */

    /* Memory-to-drive influence */
    float memory_drive_scale;              /**< Memory->drive influence scale */
    bool enable_memory_drive_modulation;   /**< Allow memories to affect drives */
    float memory_drive_decay;              /**< Decay rate of memory influence */

    /* Navigation integration */
    bool enable_navigation_goals;          /**< Use drives to set nav goals */
    float exploration_curiosity_weight;    /**< Curiosity contribution to exploration */

    /* Consolidation control */
    bool enable_consolidation_signals;     /**< Send consolidation timing signals */
    float circadian_consolidation_phase;   /**< Optimal consolidation phase (hours) */

    /* Replay integration */
    bool enable_replay_rewards;            /**< Convert replay to reward signals */
    float replay_reward_scale;             /**< Replay->reward scale */

    /* Bio-async */
    bool broadcast_enabled;                /**< Enable bio-async broadcasts */
} hypo_hipp_bridge_config_t;

/**
 * @brief Drive-memory association
 */
typedef struct {
    hypo_drive_type_t drive;               /**< Associated drive */
    uint32_t memory_id;                    /**< Associated memory */
    float association_strength;            /**< Strength of association */
    uint64_t last_activation_us;           /**< Last time association was activated */
} hypo_hipp_association_t;

/**
 * @brief Hippocampus bridge context
 */
typedef struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    hypo_hipp_bridge_config_t config;

    /* Connected modules */
    hypo_drive_system_handle_t* drives;    /**< Hypothalamus drives */
    hippocampus_adapter_t* hippocampus;    /**< Hippocampus adapter (optional) */

    /* Current state */
    hypo_hipp_encoding_priority_t current_priority;
    hypo_hipp_spatial_context_t current_context;
    hypo_hipp_nav_goal_t current_goal;

    /* Memory-drive associations */
    hypo_hipp_association_t associations[HYPO_HIPP_MAX_ASSOCIATIONS];
    uint32_t num_associations;

    /* Accumulated memory influence on drives */
    float memory_drive_influence[HYPO_DRIVE_COUNT];

    /* Timing */
    uint64_t last_update_us;
    uint64_t last_consolidation_signal_us;

    /* Bio-async context */
    bio_module_context_t bio_ctx;

    /* Statistics */
    uint64_t encoding_signals_sent;
    uint64_t memory_retrievals_processed;
    uint64_t replay_events_processed;
    uint64_t consolidation_signals_sent;
    uint64_t nav_goals_set;

} hypo_hipp_bridge_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default hippocampus bridge configuration
 *
 * @return Default configuration
 */
hypo_hipp_bridge_config_t hypo_hipp_bridge_default_config(void);

/**
 * @brief Create hippocampus bridge
 *
 * @param drives Hypothalamus drive system handle
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge context, or NULL on failure
 */
hypo_hipp_bridge_t* hypo_hipp_bridge_create(
    hypo_drive_system_handle_t* drives,
    const hypo_hipp_bridge_config_t* config);

/**
 * @brief Destroy hippocampus bridge
 *
 * @param bridge Bridge to destroy
 */
void hypo_hipp_bridge_destroy(hypo_hipp_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * @param bridge Bridge to reset
 */
void hypo_hipp_bridge_reset(hypo_hipp_bridge_t* bridge);

/*=============================================================================
 * CORE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Update bridge and compute encoding priorities
 *
 * WHAT: Compute memory encoding priorities from drive states
 * WHY:  Drive urgencies should bias what memories are encoded
 * HOW:  Map drive urgencies to category priorities
 *
 * @param bridge Bridge context
 * @param dt_ms Time delta in milliseconds
 * @return 0 on success, -1 on error
 */
int hypo_hipp_bridge_update(hypo_hipp_bridge_t* bridge, float dt_ms);

/**
 * @brief Compute encoding priority from drives
 *
 * @param bridge Bridge context
 * @return Encoding priority signal
 */
hypo_hipp_encoding_priority_t hypo_hipp_bridge_compute_encoding_priority(
    hypo_hipp_bridge_t* bridge);

/**
 * @brief Compute consolidation signal
 *
 * @param bridge Bridge context
 * @param circadian_phase Current circadian phase [0, 24)
 * @param sleep_pressure Current sleep pressure [0, 1]
 * @return Consolidation signal
 */
hypo_hipp_consolidation_signal_t hypo_hipp_bridge_compute_consolidation(
    hypo_hipp_bridge_t* bridge,
    float circadian_phase,
    float sleep_pressure);

/**
 * @brief Compute navigation goal from drives
 *
 * @param bridge Bridge context
 * @return Navigation goal context
 */
hypo_hipp_nav_goal_t hypo_hipp_bridge_compute_nav_goal(
    hypo_hipp_bridge_t* bridge);

/**
 * @brief Get current encoding priority
 *
 * @param bridge Bridge context
 * @param priority Output: current priority
 * @return true if valid
 */
bool hypo_hipp_bridge_get_encoding_priority(
    const hypo_hipp_bridge_t* bridge,
    hypo_hipp_encoding_priority_t* priority);

/*=============================================================================
 * MEMORY FEEDBACK PROCESSING
 *===========================================================================*/

/**
 * @brief Process memory retrieval event
 *
 * WHAT: Handle notification of memory retrieval from hippocampus
 * WHY:  Retrieved memories can trigger or modulate drives
 * HOW:  Update drive influence based on memory category and valence
 *
 * @param bridge Bridge context
 * @param retrieval Retrieval information
 * @return Drive influence generated
 */
float hypo_hipp_bridge_process_retrieval(
    hypo_hipp_bridge_t* bridge,
    const hypo_hipp_memory_retrieval_t* retrieval);

/**
 * @brief Process spatial context update
 *
 * @param bridge Bridge context
 * @param context Spatial context from hippocampus
 */
void hypo_hipp_bridge_process_context(
    hypo_hipp_bridge_t* bridge,
    const hypo_hipp_spatial_context_t* context);

/**
 * @brief Process replay event
 *
 * WHAT: Handle replay event notification
 * WHY:  Replay sequences can generate anticipatory reward signals
 * HOW:  Convert replay reward to drive satisfaction prediction
 *
 * @param bridge Bridge context
 * @param replay Replay event information
 * @return Reward prediction generated
 */
float hypo_hipp_bridge_process_replay(
    hypo_hipp_bridge_t* bridge,
    const hypo_hipp_replay_event_t* replay);

/**
 * @brief Get accumulated memory influence on drives
 *
 * @param bridge Bridge context
 * @param influences Output array (size HYPO_DRIVE_COUNT)
 * @return true on success
 */
bool hypo_hipp_bridge_get_memory_influences(
    const hypo_hipp_bridge_t* bridge,
    float* influences);

/*=============================================================================
 * HIPPOCAMPUS CONNECTION
 *===========================================================================*/

/**
 * @brief Connect to hippocampus adapter directly
 *
 * @param bridge Bridge context
 * @param hippocampus Hippocampus adapter handle
 * @return true on success
 */
bool hypo_hipp_bridge_connect(
    hypo_hipp_bridge_t* bridge,
    hippocampus_adapter_t* hippocampus);

/**
 * @brief Send encoding priority to hippocampus
 *
 * @param bridge Bridge context
 * @param priority Encoding priority signal
 * @return 0 on success, -1 on error
 */
int hypo_hipp_bridge_send_encoding_priority(
    hypo_hipp_bridge_t* bridge,
    const hypo_hipp_encoding_priority_t* priority);

/**
 * @brief Send consolidation signal to hippocampus
 *
 * @param bridge Bridge context
 * @param signal Consolidation signal
 * @return 0 on success, -1 on error
 */
int hypo_hipp_bridge_send_consolidation(
    hypo_hipp_bridge_t* bridge,
    const hypo_hipp_consolidation_signal_t* signal);

/**
 * @brief Set navigation goal in hippocampus
 *
 * @param bridge Bridge context
 * @param goal Navigation goal
 * @return 0 on success, -1 on error
 */
int hypo_hipp_bridge_set_nav_goal(
    hypo_hipp_bridge_t* bridge,
    const hypo_hipp_nav_goal_t* goal);

/*=============================================================================
 * DRIVE-MEMORY ASSOCIATIONS
 *===========================================================================*/

/**
 * @brief Create drive-memory association
 *
 * @param bridge Bridge context
 * @param drive Associated drive
 * @param memory_id Memory to associate
 * @param strength Association strength
 * @return true on success
 */
bool hypo_hipp_bridge_create_association(
    hypo_hipp_bridge_t* bridge,
    hypo_drive_type_t drive,
    uint32_t memory_id,
    float strength);

/**
 * @brief Strengthen existing association
 *
 * @param bridge Bridge context
 * @param drive Associated drive
 * @param memory_id Associated memory
 * @param delta Strength change
 * @return New strength, or -1 if not found
 */
float hypo_hipp_bridge_strengthen_association(
    hypo_hipp_bridge_t* bridge,
    hypo_drive_type_t drive,
    uint32_t memory_id,
    float delta);

/**
 * @brief Get associations for drive
 *
 * @param bridge Bridge context
 * @param drive Drive to query
 * @param associations Output array
 * @param max_associations Maximum to return
 * @return Number of associations found
 */
uint32_t hypo_hipp_bridge_get_associations(
    const hypo_hipp_bridge_t* bridge,
    hypo_drive_type_t drive,
    hypo_hipp_association_t* associations,
    uint32_t max_associations);

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

/**
 * @brief Register with bio-async router
 *
 * @param bridge Bridge context
 * @param use_kg_wiring Use KG-driven wiring (true) or legacy (false)
 * @return true on success
 */
bool hypo_hipp_bridge_register_bio(
    hypo_hipp_bridge_t* bridge,
    bool use_kg_wiring);

/**
 * @brief Process incoming bio-async messages
 *
 * @param bridge Bridge context
 * @param max_messages Maximum messages to process
 * @return Number of messages processed
 */
uint32_t hypo_hipp_bridge_process_bio(
    hypo_hipp_bridge_t* bridge,
    uint32_t max_messages);

/**
 * @brief Broadcast encoding priority
 *
 * @param bridge Bridge context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t hypo_hipp_bridge_broadcast_encoding_priority(
    hypo_hipp_bridge_t* bridge);

/**
 * @brief Broadcast consolidation signal
 *
 * @param bridge Bridge context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t hypo_hipp_bridge_broadcast_consolidation(
    hypo_hipp_bridge_t* bridge);

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get memory category name
 *
 * @param category Memory category
 * @return Human-readable name
 */
const char* hypo_memory_category_string(hypo_memory_category_t category);

/**
 * @brief Get drive's relevant memory category
 *
 * @param drive Drive type
 * @return Relevant memory category
 */
hypo_memory_category_t hypo_drive_to_memory_category(hypo_drive_type_t drive);

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge context
 * @param encoding_signals Output: encoding signals sent
 * @param retrievals_processed Output: memory retrievals processed
 * @param replay_events Output: replay events processed
 */
void hypo_hipp_bridge_get_stats(
    const hypo_hipp_bridge_t* bridge,
    uint64_t* encoding_signals,
    uint64_t* retrievals_processed,
    uint64_t* replay_events);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_HIPPOCAMPUS_BRIDGE_H */
