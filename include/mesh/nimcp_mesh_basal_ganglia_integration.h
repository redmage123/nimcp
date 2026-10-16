/**
 * @file nimcp_mesh_basal_ganglia_integration.h
 * @brief Basal Ganglia Mesh Network Integration
 *
 * WHAT: Connects the basal ganglia module to the mesh network
 * WHY:  Enable coordinated action selection via distributed consensus
 * HOW:  Register basal ganglia as MOTOR category participant, handle RL transactions
 *
 * BIOLOGICAL CONTEXT:
 * The basal ganglia is the brain's action selection system responsible for:
 * - Action selection and inhibition (Go/NoGo pathways)
 * - Reinforcement learning and reward prediction
 * - Procedural/habit learning
 * - Motor program coordination with cerebellum
 *
 * In the mesh network, the basal ganglia:
 * - Participates in SUBCORTICAL channel (anatomically correct)
 * - Has REQUIRED endorser role for motor policy
 * - Coordinates with cortex for action selection
 * - Integrates dopamine signals for learning
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_BASAL_GANGLIA_INTEGRATION_H
#define NIMCP_MESH_BASAL_GANGLIA_INTEGRATION_H

#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_transaction.h"
#include "mesh/nimcp_mesh_participant.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct mesh_basal_ganglia_integration mesh_basal_ganglia_integration_t;
typedef struct mesh_bootstrap mesh_bootstrap_t;

/* Forward declaration for basal ganglia module */
typedef struct bg_enhanced bg_enhanced_t;

/* ============================================================================
 * Basal Ganglia Transaction Types
 * ============================================================================ */

/**
 * @brief Basal ganglia-specific transaction types
 */
typedef enum mesh_basal_ganglia_tx_type {
    /** Base for basal ganglia transactions (0x1800 = motor range) */
    MESH_TX_BG_BASE = 0x1800,

    /** Action selection request */
    MESH_TX_BG_ACTION_SELECT = 0x1801,

    /** Action inhibition request */
    MESH_TX_BG_ACTION_INHIBIT = 0x1802,

    /** Reward prediction update */
    MESH_TX_BG_REWARD_PREDICTION = 0x1803,

    /** RPE (Reward Prediction Error) signal */
    MESH_TX_BG_RPE_SIGNAL = 0x1804,

    /** Dopamine burst signal */
    MESH_TX_BG_DOPAMINE_BURST = 0x1805,

    /** Go pathway activation */
    MESH_TX_BG_GO_PATHWAY = 0x1806,

    /** NoGo pathway activation */
    MESH_TX_BG_NOGO_PATHWAY = 0x1807,

    /** Habit formation update */
    MESH_TX_BG_HABIT_UPDATE = 0x1808,

    /** Goal-directed vs habitual transition */
    MESH_TX_BG_BEHAVIOR_MODE = 0x1809,

    /** Beta oscillation synchronization */
    MESH_TX_BG_BETA_SYNC = 0x180A,

} mesh_basal_ganglia_tx_type_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Basal ganglia mesh integration configuration
 */
typedef struct mesh_basal_ganglia_config {
    /* Behavior settings */
    bool require_consensus_for_action;       /**< Require mesh consensus for action selection */
    bool broadcast_rpe_signals;              /**< Broadcast RPE to mesh */
    bool enable_distributed_learning;        /**< Enable distributed RL */

    /* Timeouts */
    uint32_t action_selection_timeout_ms;    /**< Timeout for action selection */
    uint32_t learning_timeout_ms;            /**< Timeout for learning updates */

    /* Health monitoring */
    bool enable_health_monitoring;           /**< Enable health agent integration */
    uint32_t heartbeat_interval_ms;          /**< Health heartbeat interval */

    /* Logging */
    bool verbose_logging;

} mesh_basal_ganglia_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Basal ganglia integration statistics
 */
typedef struct mesh_basal_ganglia_stats {
    /* Action selection */
    uint64_t actions_proposed;
    uint64_t actions_selected;
    uint64_t actions_inhibited;

    /* Learning */
    uint64_t rpe_signals_sent;
    uint64_t dopamine_bursts;
    uint64_t learning_updates;

    /* Pathway activations */
    uint64_t go_activations;
    uint64_t nogo_activations;

    /* Current state */
    uint32_t current_action;
    float current_rpe;
    float current_dopamine;

    /* Mesh participation */
    uint64_t transactions_received;
    uint64_t transactions_endorsed;
    uint64_t transactions_vetoed;
    uint64_t endorsement_requests_handled;

    /* Health */
    uint64_t health_heartbeats_sent;

    /* Timing */
    uint64_t last_action_ns;
    uint64_t last_rpe_ns;

} mesh_basal_ganglia_stats_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default basal ganglia integration configuration
 */
nimcp_error_t mesh_basal_ganglia_default_config(mesh_basal_ganglia_config_t* config);

/**
 * @brief Create basal ganglia mesh integration
 */
mesh_basal_ganglia_integration_t* mesh_basal_ganglia_create(
    mesh_bootstrap_t* bootstrap,
    bg_enhanced_t* basal_ganglia,
    const mesh_basal_ganglia_config_t* config
);

/**
 * @brief Destroy basal ganglia mesh integration
 */
void mesh_basal_ganglia_destroy(mesh_basal_ganglia_integration_t* integration);

/* ============================================================================
 * Participant Registration API
 * ============================================================================ */

/**
 * @brief Register basal ganglia as mesh participant
 */
nimcp_error_t mesh_basal_ganglia_register_participant(
    mesh_basal_ganglia_integration_t* integration
);

/**
 * @brief Unregister basal ganglia from mesh
 */
nimcp_error_t mesh_basal_ganglia_unregister_participant(
    mesh_basal_ganglia_integration_t* integration
);

/**
 * @brief Get participant ID for the basal ganglia
 */
mesh_participant_id_t mesh_basal_ganglia_get_participant_id(
    const mesh_basal_ganglia_integration_t* integration
);

/**
 * @brief Check if basal ganglia is registered with mesh
 */
bool mesh_basal_ganglia_is_registered(
    const mesh_basal_ganglia_integration_t* integration
);

/* ============================================================================
 * Action Selection API
 * ============================================================================ */

/**
 * @brief Propose action selection via mesh
 */
nimcp_error_t mesh_basal_ganglia_propose_action(
    mesh_basal_ganglia_integration_t* integration,
    uint32_t action_id,
    float confidence
);

/**
 * @brief Report RPE signal via mesh
 */
nimcp_error_t mesh_basal_ganglia_report_rpe(
    mesh_basal_ganglia_integration_t* integration,
    float rpe_value
);

/**
 * @brief Report dopamine burst via mesh
 */
nimcp_error_t mesh_basal_ganglia_report_dopamine(
    mesh_basal_ganglia_integration_t* integration,
    float dopamine_level
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get basal ganglia integration statistics
 */
nimcp_error_t mesh_basal_ganglia_get_stats(
    const mesh_basal_ganglia_integration_t* integration,
    mesh_basal_ganglia_stats_t* stats
);

/**
 * @brief Reset basal ganglia integration statistics
 */
nimcp_error_t mesh_basal_ganglia_reset_stats(
    mesh_basal_ganglia_integration_t* integration
);

/* ============================================================================
 * Health Agent Integration API
 * ============================================================================ */

/**
 * @brief Set health agent for basal ganglia integration
 */
nimcp_error_t mesh_basal_ganglia_set_health_agent(
    mesh_basal_ganglia_integration_t* integration,
    nimcp_health_agent_t* agent
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Check if transaction type is basal ganglia-related
 */
bool mesh_basal_ganglia_is_bg_transaction(mesh_tx_type_t tx_type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_BASAL_GANGLIA_INTEGRATION_H */
