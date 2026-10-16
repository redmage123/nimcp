/**
 * @file nimcp_mesh_global_workspace_integration.h
 * @brief Global Workspace Mesh Network Integration
 *
 * WHAT: Connects the global workspace module to the mesh network
 * WHY:  Enable coordinated conscious broadcasting via distributed consensus
 * HOW:  Register global workspace as COGNITIVE category participant, handle broadcast transactions
 *
 * BIOLOGICAL CONTEXT:
 * The global workspace theory posits that consciousness arises from:
 * - Competition between specialized processors for access to global broadcast
 * - Winners gain access to a "blackboard" where their content is shared widely
 * - This enables integration of information across brain regions
 *
 * In the mesh network, the global workspace:
 * - Participates in SYSTEM channel (coordinates all others)
 * - Has COORDINATOR role for cognitive transactions
 * - Manages broadcasting of winning content to all participants
 * - Integrates attention signals from thalamus/pulvinar
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_GLOBAL_WORKSPACE_INTEGRATION_H
#define NIMCP_MESH_GLOBAL_WORKSPACE_INTEGRATION_H

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

typedef struct mesh_global_workspace_integration mesh_global_workspace_integration_t;
typedef struct mesh_bootstrap mesh_bootstrap_t;

/* Forward declaration for global workspace module */
typedef struct global_workspace global_workspace_t;

/* ============================================================================
 * Global Workspace Transaction Types
 * ============================================================================ */

/**
 * @brief Global workspace-specific transaction types
 */
typedef enum mesh_gw_tx_type {
    /** Base for global workspace transactions (0x1A00 = cognitive range) */
    MESH_TX_GW_BASE = 0x1A00,

    /** Content broadcast request */
    MESH_TX_GW_BROADCAST = 0x1A01,

    /** Competition winner announcement */
    MESH_TX_GW_WINNER = 0x1A02,

    /** Ignition event (content reaches threshold) */
    MESH_TX_GW_IGNITION = 0x1A03,

    /** Content decay/fade */
    MESH_TX_GW_DECAY = 0x1A04,

    /** Attention spotlight shift */
    MESH_TX_GW_ATTENTION_SHIFT = 0x1A05,

    /** Module registration for competition */
    MESH_TX_GW_MODULE_REGISTER = 0x1A06,

    /** Coalition formation */
    MESH_TX_GW_COALITION_FORM = 0x1A07,

    /** Cognitive binding event */
    MESH_TX_GW_BINDING = 0x1A08,

    /** Working memory update */
    MESH_TX_GW_WM_UPDATE = 0x1A09,

    /** Metacognitive signal */
    MESH_TX_GW_METACOGNITION = 0x1A0A,

} mesh_gw_tx_type_t;

/* ============================================================================
 * Competition Strategy (matches global workspace module)
 * ============================================================================ */

/**
 * @brief Competition strategies for global workspace
 */
typedef enum mesh_gw_competition_strategy {
    MESH_GW_STRATEGY_WINNER_TAKE_ALL = 0,  /**< Single winner */
    MESH_GW_STRATEGY_SOFTMAX,               /**< Probabilistic selection */
    MESH_GW_STRATEGY_COALITION,             /**< Winners can form coalitions */
    MESH_GW_STRATEGY_HIERARCHICAL           /**< Nested competitions */
} mesh_gw_competition_strategy_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Global workspace mesh integration configuration
 */
typedef struct mesh_gw_config {
    /* Behavior settings */
    mesh_gw_competition_strategy_t strategy; /**< Competition strategy */
    bool broadcast_to_all_channels;          /**< Broadcast winners to all mesh channels */
    bool require_consensus_for_broadcast;    /**< Require mesh consensus for broadcast */

    /* Competition parameters */
    float ignition_threshold;                /**< Threshold for ignition event */
    float decay_rate;                        /**< Rate of content decay */
    uint32_t max_coalition_size;             /**< Maximum coalition size */

    /* Timeouts */
    uint32_t competition_timeout_ms;         /**< Timeout for competition */
    uint32_t broadcast_timeout_ms;           /**< Timeout for broadcast */

    /* Health monitoring */
    bool enable_health_monitoring;           /**< Enable health agent integration */
    uint32_t heartbeat_interval_ms;          /**< Health heartbeat interval */

    /* Logging */
    bool verbose_logging;

} mesh_gw_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Global workspace integration statistics
 */
typedef struct mesh_gw_stats {
    /* Broadcast counts */
    uint64_t broadcasts_initiated;
    uint64_t broadcasts_completed;
    uint64_t broadcasts_failed;

    /* Competition */
    uint64_t competitions_held;
    uint64_t winners_announced;
    uint64_t ignition_events;

    /* Coalition */
    uint64_t coalitions_formed;
    uint64_t binding_events;

    /* Current state */
    uint32_t current_winner_module;
    float current_winner_strength;
    uint32_t active_competitors;

    /* Mesh participation */
    uint64_t transactions_received;
    uint64_t transactions_endorsed;
    uint64_t transactions_vetoed;
    uint64_t endorsement_requests_handled;

    /* Health */
    uint64_t health_heartbeats_sent;

    /* Timing */
    uint64_t last_broadcast_ns;
    uint64_t last_ignition_ns;

} mesh_gw_stats_t;

/* ============================================================================
 * Transaction Payload Structures
 * ============================================================================ */

/**
 * @brief Broadcast payload
 */
typedef struct mesh_gw_broadcast_payload {
    uint32_t source_module;           /**< Module that won competition */
    float strength;                   /**< Strength of broadcast */
    uint32_t content_type;            /**< Type of content being broadcast */
    uint8_t content[256];             /**< Content data (limited size) */
    size_t content_size;              /**< Actual content size */
} mesh_gw_broadcast_payload_t;

/**
 * @brief Ignition payload
 */
typedef struct mesh_gw_ignition_payload {
    uint32_t source_module;
    float activation_level;
    bool sustained;                   /**< Is this a sustained ignition? */
} mesh_gw_ignition_payload_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Called when broadcast is received
 */
typedef void (*mesh_gw_broadcast_callback_t)(
    uint32_t source_module,
    const void* content,
    size_t content_size,
    void* ctx
);

/**
 * @brief Called when ignition occurs
 */
typedef void (*mesh_gw_ignition_callback_t)(
    uint32_t source_module,
    float activation_level,
    void* ctx
);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default global workspace integration configuration
 */
nimcp_error_t mesh_gw_default_config(mesh_gw_config_t* config);

/**
 * @brief Create global workspace mesh integration
 */
mesh_global_workspace_integration_t* mesh_gw_create(
    mesh_bootstrap_t* bootstrap,
    global_workspace_t* workspace,
    const mesh_gw_config_t* config
);

/**
 * @brief Destroy global workspace mesh integration
 */
void mesh_gw_destroy(mesh_global_workspace_integration_t* integration);

/* ============================================================================
 * Participant Registration API
 * ============================================================================ */

/**
 * @brief Register global workspace as mesh participant
 */
nimcp_error_t mesh_gw_register_participant(
    mesh_global_workspace_integration_t* integration
);

/**
 * @brief Unregister global workspace from mesh
 */
nimcp_error_t mesh_gw_unregister_participant(
    mesh_global_workspace_integration_t* integration
);

/**
 * @brief Get participant ID for the global workspace
 */
mesh_participant_id_t mesh_gw_get_participant_id(
    const mesh_global_workspace_integration_t* integration
);

/**
 * @brief Check if global workspace is registered with mesh
 */
bool mesh_gw_is_registered(
    const mesh_global_workspace_integration_t* integration
);

/* ============================================================================
 * Broadcast Operations API
 * ============================================================================ */

/**
 * @brief Initiate broadcast via mesh
 */
nimcp_error_t mesh_gw_initiate_broadcast(
    mesh_global_workspace_integration_t* integration,
    uint32_t source_module,
    const void* content,
    size_t content_size,
    float strength
);

/**
 * @brief Report ignition event via mesh
 */
nimcp_error_t mesh_gw_report_ignition(
    mesh_global_workspace_integration_t* integration,
    uint32_t source_module,
    float activation_level
);

/**
 * @brief Report competition winner via mesh
 */
nimcp_error_t mesh_gw_report_winner(
    mesh_global_workspace_integration_t* integration,
    uint32_t winner_module,
    float strength
);

/* ============================================================================
 * Callback Registration API
 * ============================================================================ */

/**
 * @brief Set broadcast callback
 */
nimcp_error_t mesh_gw_set_broadcast_callback(
    mesh_global_workspace_integration_t* integration,
    mesh_gw_broadcast_callback_t callback,
    void* ctx
);

/**
 * @brief Set ignition callback
 */
nimcp_error_t mesh_gw_set_ignition_callback(
    mesh_global_workspace_integration_t* integration,
    mesh_gw_ignition_callback_t callback,
    void* ctx
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get global workspace integration statistics
 */
nimcp_error_t mesh_gw_get_stats(
    const mesh_global_workspace_integration_t* integration,
    mesh_gw_stats_t* stats
);

/**
 * @brief Reset global workspace integration statistics
 */
nimcp_error_t mesh_gw_reset_stats(
    mesh_global_workspace_integration_t* integration
);

/* ============================================================================
 * Health Agent Integration API
 * ============================================================================ */

/**
 * @brief Set health agent for global workspace integration
 */
nimcp_error_t mesh_gw_set_health_agent(
    mesh_global_workspace_integration_t* integration,
    nimcp_health_agent_t* agent
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Check if transaction type is global workspace-related
 */
bool mesh_gw_is_gw_transaction(mesh_tx_type_t tx_type);

/**
 * @brief Convert competition strategy to string
 */
const char* mesh_gw_strategy_to_string(mesh_gw_competition_strategy_t strategy);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_GLOBAL_WORKSPACE_INTEGRATION_H */
