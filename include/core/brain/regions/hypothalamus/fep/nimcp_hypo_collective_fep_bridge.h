/**
 * @file nimcp_hypo_collective_fep_bridge.h
 * @brief Free Energy Principle bridge for Hypothalamus Collective Integration
 * @version 1.0.0
 * @date 2025-01-10
 *
 * WHAT: FEP integration for collective cognition with hypothalamic drive modulation
 * WHY:  Collective goals translate to individual drives; group consensus as free energy
 * HOW:  Map collective state to drive priorities, use FEP for consensus alignment
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * COLLECTIVE COGNITION AS DRIVE-ALIGNED COORDINATION:
 * - SOCIAL drive --> Tendency to align with group
 * - AUTONOMY drive --> Tendency to maintain individual position
 * - SAFETY drive --> Risk assessment of group vs individual action
 * - Drive balance --> Individual-collective compromise
 *
 * FEP INTEGRATION:
 * ```
 * Collective State (C) --> Individual Drive Modulation
 *         |
 *         v
 * Group Consensus (g) --> Expected Collective Behavior
 *         |
 *         v
 * Deviation: epsilon = individual - consensus
 *         |
 *         v
 * Free Energy F = Alignment Cost + Autonomy Cost
 *         |
 *         v
 * Drive Adjustment --> Balance Individual vs Collective
 * ```
 *
 * COLLECTIVE-DRIVE MAPPING:
 * - High SOCIAL drive --> Strong collective alignment (low FE for conformity)
 * - High AUTONOMY drive --> Resistance to collective (low FE for independence)
 * - High SAFETY drive --> Follow collective when uncertain
 * - Drive conflict --> Negotiated compromise position
 *
 * KEY MAPPINGS:
 * - Collective goals --> Drive priority modulation
 * - Group consensus distance --> Prediction error
 * - Alignment pressure --> Free energy from deviation
 * - Individual-collective balance --> Active inference equilibrium
 *
 * ARCHITECTURE:
 * ```
 * +=========================================================================+
 * |  HYPOTHALAMUS COLLECTIVE - FEP BRIDGE (Drive-Aligned Coordination)      |
 * +=========================================================================+
 * |                                                                          |
 * |   +------------------------------------------------------------------+   |
 * |   |                   FEP SYSTEM                                     |   |
 * |   |                                                                  |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   |   | Free Energy     |     | Surprise        |                    |   |
 * |   |   | (alignment)     |     | (deviation)     |                    |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   |          |                       |                               |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   |   | Precision       |     | Belief Update   |                    |   |
 * |   |   | (confidence)    |     | (learning)      |                    |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   +------------------------------------------------------------------+   |
 * |                          ^                    |                          |
 * |                          |                    v                          |
 * |   +------------------------------------------------------------------+   |
 * |   |              BIDIRECTIONAL EFFECTS                               |   |
 * |   |                                                                  |   |
 * |   |  FEP --> Collective:                                             |   |
 * |   |    * Free energy --> Alignment strength                          |   |
 * |   |    * Surprise --> Collective deviation signal                    |   |
 * |   |    * Precision --> Consensus confidence                          |   |
 * |   |    * Active inference --> Individual contribution                |   |
 * |   |                                                                  |   |
 * |   |  Collective --> FEP:                                             |   |
 * |   |    * Group consensus --> Prior on individual position            |   |
 * |   |    * Collective success --> Reinforce alignment                  |   |
 * |   |    * Collective failure --> Weaken alignment                     |   |
 * |   |    * Peer signals --> Observation precision                      |   |
 * |   +------------------------------------------------------------------+   |
 * |                          ^                    |                          |
 * |                          |                    v                          |
 * |   +------------------------------------------------------------------+   |
 * |   |                 HYPOTHALAMUS DRIVE SYSTEM                        |   |
 * |   |                                                                  |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   |   | SOCIAL Drive    |     | AUTONOMY Drive  |                    |   |
 * |   |   | (alignment)     |     | (independence)  |                    |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   |          |                       |                               |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   |   | SAFETY Drive    |     | Drive Priority  |                    |   |
 * |   |   | (risk assess)   |     | Resolution      |                    |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   +------------------------------------------------------------------+   |
 * |                                                                          |
 * +=========================================================================+
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HYPO_COLLECTIVE_FEP_BRIDGE_H
#define NIMCP_HYPO_COLLECTIVE_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Free energy thresholds for alignment state */
#define HYPO_COL_FEP_ALIGNED_THRESHOLD        2.0f    /**< FE below: well-aligned */
#define HYPO_COL_FEP_MARGINAL_THRESHOLD       5.0f    /**< FE in range: marginal alignment */
#define HYPO_COL_FEP_DEVIANT_THRESHOLD        10.0f   /**< FE above: significant deviation */

/** Precision bounds for consensus confidence */
#define HYPO_COL_FEP_MIN_PRECISION            0.1f    /**< Minimum precision */
#define HYPO_COL_FEP_MAX_PRECISION            10.0f   /**< Maximum precision */
#define HYPO_COL_FEP_DEFAULT_PRECISION        1.0f    /**< Default precision */

/** Learning rate defaults */
#define HYPO_COL_FEP_DEFAULT_LEARNING_RATE    0.01f   /**< Belief update rate */
#define HYPO_COL_FEP_PRECISION_LEARNING_RATE  0.05f   /**< Precision adaptation rate */

/** Feature dimensions */
#define HYPO_COL_FEP_DRIVE_DIM                4       /**< Drive feature dimension */
#define HYPO_COL_FEP_COLLECTIVE_DIM           16      /**< Collective state dimension */
#define HYPO_COL_FEP_INDIVIDUAL_DIM           16      /**< Individual state dimension */

/** Bio-async module ID */
#define BIO_MODULE_HYPO_COL_FEP               0x0A13  /**< Hypothalamus collective FEP bridge */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Alignment state classification based on drive-FEP integration
 *
 * WHAT: Classification of individual-collective alignment
 * WHY:  Different alignment states require different balancing strategies
 * HOW:  Map free energy to alignment categories
 */
typedef enum {
    HYPO_COL_STATE_ALIGNED = 0,        /**< Low FE: individual aligned with collective */
    HYPO_COL_STATE_MARGINAL,           /**< Moderate FE: partial alignment */
    HYPO_COL_STATE_INDEPENDENT,        /**< High FE: maintaining independence */
    HYPO_COL_STATE_DEVIANT,            /**< Very high FE: significant deviation */
    HYPO_COL_STATE_LEADER              /**< Special: leading collective direction */
} hypo_col_state_t;

/**
 * @brief Collective action recommendation
 *
 * WHAT: Recommended action for individual-collective balance
 * WHY:  Different situations call for different balancing responses
 * HOW:  Map alignment + drives to action
 */
typedef enum {
    HYPO_COL_ACTION_ALIGN = 0,         /**< Move toward collective consensus */
    HYPO_COL_ACTION_MAINTAIN,          /**< Maintain current position */
    HYPO_COL_ACTION_ASSERT,            /**< Assert individual position */
    HYPO_COL_ACTION_NEGOTIATE,         /**< Seek compromise */
    HYPO_COL_ACTION_LEAD               /**< Attempt to lead collective */
} hypo_col_action_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Hypothalamus collective FEP configuration
 *
 * WHAT: Configuration for drive-modulated collective alignment via FEP
 * WHY:  Tune how drives influence individual-collective balance
 * HOW:  Adjustable weights, thresholds, and learning parameters
 */
typedef struct {
    /* Drive-FEP coupling weights */
    float drive_fe_weight;             /**< Weight of drive state on free energy */
    float prediction_error_gain;       /**< Gain for prediction error computation */
    float precision_modulation;        /**< Precision modulation factor */

    /* Alignment parameters */
    float alignment_threshold;         /**< FE threshold for alignment */
    float deviance_threshold;          /**< FE threshold for deviance */
    float consensus_weight;            /**< Weight of consensus in FE */

    /* Drive influence weights */
    float social_weight;               /**< SOCIAL drive influence (alignment) */
    float autonomy_weight;             /**< AUTONOMY drive influence (independence) */
    float safety_weight;               /**< SAFETY drive influence (risk) */

    /* Balance parameters */
    float conformity_pressure;         /**< Pressure to conform to collective */
    float independence_value;          /**< Value placed on independence */
    float leadership_threshold;        /**< Threshold for leadership behavior */

    /* Learning parameters */
    bool enable_online_learning;       /**< Enable alignment learning */
    float learning_rate;               /**< Alignment update rate */
    float precision_learning_rate;     /**< Precision adaptation rate */

    /* Active inference */
    bool enable_active_inference;      /**< Enable active alignment */
    float action_temperature;          /**< Softmax temperature for actions */

    /* Bio-async */
    bool enable_bio_async;             /**< Enable bio-async messaging */

    /* Sensitivity factors */
    float fep_sensitivity;             /**< FEP effect scaling [0.5-2.0] */
    float drive_sensitivity;           /**< Drive effect scaling [0.5-2.0] */
} hypo_col_fep_config_t;

/* ============================================================================
 * Effect Structures
 * ============================================================================ */

/**
 * @brief FEP effects on collective (FEP --> Collective)
 *
 * WHAT: How FEP analysis influences collective alignment
 * WHY:  Translate FEP metrics to alignment parameters
 * HOW:  Map free energy, surprise, precision to alignment strength
 */
typedef struct {
    /* Alignment state */
    hypo_col_state_t current_state;    /**< Current alignment state */
    float alignment_score;             /**< Current alignment [0-1] */
    float independence_score;          /**< Current independence [0-1] */

    /* FEP metrics */
    float free_energy;                 /**< Current free energy level */
    float prediction_error;            /**< Current prediction error */
    float precision;                   /**< Current precision level */
    float active_inference_strength;   /**< Strength of active inference */

    /* Consensus metrics */
    float consensus_distance;          /**< Distance from group consensus */
    float consensus_confidence;        /**< Confidence in consensus estimate */
    float group_coherence;             /**< Estimated group coherence */

    /* Recommended action */
    hypo_col_action_t recommended_action;  /**< Recommended collective action */
    float action_strength;             /**< Strength of action [0-1] */

    /* Drive influence */
    float social_influence;            /**< Current social influence */
    float autonomy_influence;          /**< Current autonomy influence */
    float safety_influence;            /**< Current safety influence */
} fep_to_col_effects_t;

/**
 * @brief Collective effects on FEP (Collective --> FEP)
 *
 * WHAT: How collective state feeds back to FEP
 * WHY:  Collective outcomes update alignment learning
 * HOW:  Convert collective events to FEP observations
 */
typedef struct {
    /* Alignment counts */
    uint64_t alignments;               /**< Times aligned with collective */
    uint64_t deviations;               /**< Times deviated from collective */
    uint64_t negotiations;             /**< Negotiation events */
    uint64_t leadership_attempts;      /**< Leadership attempts */

    /* Outcome tracking */
    uint64_t collective_successes;     /**< Collective successes */
    uint64_t collective_failures;      /**< Collective failures */
    uint64_t individual_successes;     /**< Individual position successes */
    uint64_t individual_failures;      /**< Individual position failures */

    /* Aggregated metrics */
    float avg_alignment;               /**< Average alignment score */
    float avg_consensus_distance;      /**< Average distance from consensus */
    float collective_utility;          /**< Utility from collective action */

    /* Drive updates */
    float social_drive_update;         /**< Suggested SOCIAL drive change */
    float autonomy_drive_update;       /**< Suggested AUTONOMY drive change */
    float safety_drive_update;         /**< Suggested SAFETY drive change */
} col_to_fep_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief Current FEP bridge state
 */
typedef struct {
    bool active;                       /**< Whether bridge is active */
    uint64_t update_count;             /**< Number of updates */
    uint64_t collective_events;        /**< Collective events processed */
    float current_precision;           /**< Current precision level */
    float avg_free_energy;             /**< Running average free energy */
    float avg_prediction_error;        /**< Running average prediction error */
    hypo_col_state_t current_state;    /**< Current alignment state */
    hypo_col_action_t last_action;     /**< Last recommended action */
    uint64_t last_update_time;         /**< Timestamp of last update */
} hypo_col_fep_state_t;

/**
 * @brief FEP bridge statistics
 */
typedef struct {
    /* Alignment state distribution */
    uint64_t state_aligned;            /**< Time in aligned state */
    uint64_t state_marginal;           /**< Time in marginal state */
    uint64_t state_independent;        /**< Time in independent state */
    uint64_t state_deviant;            /**< Time in deviant state */
    uint64_t state_leader;             /**< Time in leader state */

    /* Action distribution */
    uint64_t action_align;             /**< Align decisions */
    uint64_t action_maintain;          /**< Maintain decisions */
    uint64_t action_assert;            /**< Assert decisions */
    uint64_t action_negotiate;         /**< Negotiate decisions */
    uint64_t action_lead;              /**< Lead decisions */

    /* FEP metrics */
    float avg_free_energy;             /**< Average free energy */
    float max_free_energy;             /**< Maximum free energy observed */
    float avg_prediction_error;        /**< Average prediction error */
    float avg_precision;               /**< Average precision */

    /* Quality metrics */
    float avg_alignment;               /**< Average alignment score */
    float collective_success_rate;     /**< Collective success rate */
    float individual_success_rate;     /**< Individual success rate */

    /* Performance */
    uint64_t bridge_updates;           /**< Total bridge updates */
    float avg_update_time_us;          /**< Average update time */
} hypo_col_fep_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Hypothalamus collective FEP bridge
 *
 * WHAT: Main bridge structure connecting hypothalamic drives to collective via FEP
 * WHY:  Enable drive-modulated individual-collective alignment with FEP
 * HOW:  Maintain FEP system, drive refs, bidirectional effects
 */
typedef struct {
    bridge_base_t base;                /**< MUST be first: base bridge */

    /* Configuration */
    hypo_col_fep_config_t config;      /**< Bridge configuration */

    /* Connected systems */
    fep_system_t* fep_system;          /**< FEP system */
    hypo_drive_system_handle_t* drive_system;  /**< Hypothalamus drive system */

    /* Bidirectional effects */
    fep_to_col_effects_t fep_effects;  /**< FEP --> Collective effects */
    col_to_fep_effects_t col_effects;  /**< Collective --> FEP effects */

    /* State */
    hypo_col_fep_state_t state;        /**< Current state */

    /* Statistics */
    hypo_col_fep_stats_t stats;        /**< Statistics */

    /* Feature buffers */
    float* drive_features;             /**< Drive feature buffer */
    float* collective_features;        /**< Collective state feature buffer */
    float* individual_features;        /**< Individual state feature buffer */
} hypo_col_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default FEP bridge configuration
 *
 * WHAT: Provide sensible defaults for drive-collective FEP integration
 * WHY:  Simplify initialization with biologically-plausible defaults
 * HOW:  Populate config struct with tested default values
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int hypo_col_fep_default_config(hypo_col_fep_config_t* config);

/**
 * @brief Create hypothalamus collective FEP bridge
 *
 * WHAT: Initialize FEP integration for drive-modulated collective alignment
 * WHY:  Enable drive-based individual-collective balance with FEP
 * HOW:  Connect FEP system to drive system, allocate structures
 *
 * @param config Configuration (NULL for defaults)
 * @param drive_system Hypothalamus drive system handle
 * @param fep_system FEP system handle
 * @return Bridge handle or NULL on failure
 */
hypo_col_fep_bridge_t* hypo_col_fep_create(
    const hypo_col_fep_config_t* config,
    hypo_drive_system_handle_t* drive_system,
    fep_system_t* fep_system
);

/**
 * @brief Destroy FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Prevent memory leaks
 * HOW:  Free allocations, disconnect bio-async
 *
 * @param bridge Bridge handle (NULL safe)
 */
void hypo_col_fep_destroy(hypo_col_fep_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * WHAT: Reset all state and statistics
 * WHY:  Allow clean restart without reallocation
 * HOW:  Zero state/stats, reset precision to default
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_col_fep_reset(hypo_col_fep_bridge_t* bridge);

/* ============================================================================
 * Core Update API
 * ============================================================================ */

/**
 * @brief Update bridge from current drive and collective state
 *
 * WHAT: Compute FEP-based alignment parameters from drive and collective state
 * WHY:  Translate drives and collective to alignment strength
 * HOW:  Process through FEP, determine alignment state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_col_fep_update(hypo_col_fep_bridge_t* bridge);

/**
 * @brief Compute free energy for current alignment state
 *
 * WHAT: Calculate free energy representing alignment cost
 * WHY:  FE quantifies cost of deviation from collective
 * HOW:  Combine alignment distance with drive preferences
 *
 * @param bridge Bridge handle
 * @param free_energy Output free energy value
 * @return 0 on success, -1 on error
 */
int hypo_col_fep_compute_fe(
    hypo_col_fep_bridge_t* bridge,
    float* free_energy
);

/**
 * @brief Modulate precision based on collective success
 *
 * WHAT: Adjust precision based on collective outcomes
 * WHY:  Improve alignment calibration over time
 * HOW:  Increase precision when collective succeeds, decrease otherwise
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_col_fep_modulate_precision(hypo_col_fep_bridge_t* bridge);

/**
 * @brief Get current FEP effects
 *
 * WHAT: Retrieve current FEP effects on collective alignment
 * WHY:  Allow external systems to query alignment parameters
 * HOW:  Copy current effects to output structure
 *
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int hypo_col_fep_get_effects(
    const hypo_col_fep_bridge_t* bridge,
    fep_to_col_effects_t* effects
);

/* ============================================================================
 * Collective Alignment API
 * ============================================================================ */

/**
 * @brief Compute alignment with collective
 *
 * WHAT: Compute individual-collective alignment score
 * WHY:  Core function for collective coordination
 * HOW:  Compare individual position to group consensus
 *
 * @param bridge Bridge handle
 * @param collective_features Collective state features
 * @param collective_len Length of collective features
 * @param individual_features Individual state features
 * @param individual_len Length of individual features
 * @param alignment_score Output alignment score [0-1]
 * @return 0 on success, -1 on error
 */
int hypo_col_fep_compute_alignment(
    hypo_col_fep_bridge_t* bridge,
    const float* collective_features,
    uint32_t collective_len,
    const float* individual_features,
    uint32_t individual_len,
    float* alignment_score
);

/**
 * @brief Get recommended action for collective balance
 *
 * WHAT: Get recommended action based on current state
 * WHY:  Guide individual-collective balance decisions
 * HOW:  Combine alignment state with drive priorities
 *
 * @param bridge Bridge handle
 * @param action Output recommended action
 * @param strength Output action strength [0-1]
 * @return 0 on success, -1 on error
 */
int hypo_col_fep_get_recommended_action(
    hypo_col_fep_bridge_t* bridge,
    hypo_col_action_t* action,
    float* strength
);

/**
 * @brief Update from collective outcome
 *
 * WHAT: Process collective outcome for learning
 * WHY:  Enable FEP updates from collective success/failure
 * HOW:  Compute prediction error, update alignment model
 *
 * @param bridge Bridge handle
 * @param collective_success Whether collective action succeeded
 * @param individual_contribution Individual's contribution to outcome
 * @return 0 on success, -1 on error
 */
int hypo_col_fep_update_from_outcome(
    hypo_col_fep_bridge_t* bridge,
    bool collective_success,
    float individual_contribution
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current state
 *
 * @param bridge Bridge handle
 * @param state Output state
 * @return 0 on success, -1 on error
 */
int hypo_col_fep_get_state(
    const hypo_col_fep_bridge_t* bridge,
    hypo_col_fep_state_t* state
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int hypo_col_fep_get_stats(
    const hypo_col_fep_bridge_t* bridge,
    hypo_col_fep_stats_t* stats
);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async system
 * WHY:  Enable inter-module collective notifications
 * HOW:  Register module, setup inbox
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_col_fep_connect_bio_async(hypo_col_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_col_fep_disconnect_bio_async(hypo_col_fep_bridge_t* bridge);

/**
 * @brief Process pending bio-async messages
 *
 * WHAT: Handle incoming bio-async messages
 * WHY:  React to other modules' collective-relevant events
 * HOW:  Dequeue and process messages
 *
 * @param bridge Bridge handle
 * @param max_messages Maximum messages to process
 * @return Number of messages processed, or -1 on error
 */
int hypo_col_fep_process_messages(
    hypo_col_fep_bridge_t* bridge,
    uint32_t max_messages
);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Print bridge summary to stdout
 *
 * @param bridge Bridge handle
 */
void hypo_col_fep_print_summary(const hypo_col_fep_bridge_t* bridge);

/**
 * @brief Convert alignment state to string
 *
 * @param state Alignment state
 * @return Human-readable string
 */
const char* hypo_col_state_to_string(hypo_col_state_t state);

/**
 * @brief Convert collective action to string
 *
 * @param action Collective action
 * @return Human-readable string
 */
const char* hypo_col_action_to_string(hypo_col_action_t action);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPO_COLLECTIVE_FEP_BRIDGE_H */
