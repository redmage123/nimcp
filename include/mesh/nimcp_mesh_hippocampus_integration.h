/**
 * @file nimcp_mesh_hippocampus_integration.h
 * @brief Hippocampus Mesh Network Integration
 *
 * WHAT: Connects the hippocampus module to the mesh network
 * WHY:  Enable coordinated memory operations via distributed consensus
 * HOW:  Register hippocampus as MEMORY category participant, handle memory transactions
 *
 * BIOLOGICAL CONTEXT:
 * The hippocampus is the brain's memory hub responsible for:
 * - Episodic memory formation and retrieval
 * - Spatial navigation (place cells, grid cells)
 * - Memory consolidation during sleep
 * - Pattern separation and completion
 *
 * In the mesh network, the hippocampus:
 * - Participates in SUBCORTICAL channel (anatomically correct)
 * - Has REQUIRED endorser role for memory_store policy
 * - Coordinates with cortical areas for systems consolidation
 * - Integrates emotional tagging from amygdala
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_HIPPOCAMPUS_INTEGRATION_H
#define NIMCP_MESH_HIPPOCAMPUS_INTEGRATION_H

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

typedef struct mesh_hippocampus_integration mesh_hippocampus_integration_t;
typedef struct mesh_bootstrap mesh_bootstrap_t;

/* Forward declaration for hippocampus module */
typedef struct hippocampus_adapter hippocampus_adapter_t;

/* ============================================================================
 * Hippocampus Transaction Types
 * ============================================================================ */

/**
 * @brief Hippocampus-specific transaction types
 */
typedef enum mesh_hippocampus_tx_type {
    /** Base for hippocampus transactions (0x1600 = memory range) */
    MESH_TX_HIPPOCAMPUS_BASE = 0x1600,

    /** Memory encoding request */
    MESH_TX_HIPPOCAMPUS_ENCODE = 0x1601,

    /** Memory retrieval request */
    MESH_TX_HIPPOCAMPUS_RETRIEVE = 0x1602,

    /** Memory consolidation request */
    MESH_TX_HIPPOCAMPUS_CONSOLIDATE = 0x1603,

    /** Place cell update */
    MESH_TX_HIPPOCAMPUS_PLACE_UPDATE = 0x1604,

    /** Grid cell update */
    MESH_TX_HIPPOCAMPUS_GRID_UPDATE = 0x1605,

    /** Pattern completion request */
    MESH_TX_HIPPOCAMPUS_PATTERN_COMPLETE = 0x1606,

    /** Pattern separation request */
    MESH_TX_HIPPOCAMPUS_PATTERN_SEPARATE = 0x1607,

    /** Replay request (sleep consolidation) */
    MESH_TX_HIPPOCAMPUS_REPLAY = 0x1608,

    /** Systems consolidation to cortex */
    MESH_TX_HIPPOCAMPUS_SYSTEMS_CONSOLIDATE = 0x1609,

} mesh_hippocampus_tx_type_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Hippocampus mesh integration configuration
 */
typedef struct mesh_hippocampus_config {
    /* Behavior settings */
    bool require_consensus_for_encoding;     /**< Require mesh consensus for encoding */
    bool require_consensus_for_retrieval;    /**< Require consensus for retrieval */
    bool enable_distributed_consolidation;   /**< Enable distributed memory consolidation */

    /* Timeouts */
    uint32_t encoding_timeout_ms;            /**< Timeout for encoding transactions */
    uint32_t retrieval_timeout_ms;           /**< Timeout for retrieval transactions */
    uint32_t consolidation_timeout_ms;       /**< Timeout for consolidation */

    /* Health monitoring */
    bool enable_health_monitoring;           /**< Enable health agent integration */
    uint32_t heartbeat_interval_ms;          /**< Health heartbeat interval */

    /* Logging */
    bool verbose_logging;

} mesh_hippocampus_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Hippocampus integration statistics
 */
typedef struct mesh_hippocampus_stats {
    /* Transaction counts */
    uint64_t encodings_proposed;
    uint64_t encodings_committed;
    uint64_t encodings_rejected;

    uint64_t retrievals_proposed;
    uint64_t retrievals_committed;
    uint64_t retrievals_rejected;

    uint64_t consolidations_triggered;
    uint64_t replay_events;

    /* Mesh participation */
    uint64_t transactions_received;
    uint64_t transactions_endorsed;
    uint64_t transactions_vetoed;
    uint64_t endorsement_requests_handled;

    /* Health */
    uint64_t health_heartbeats_sent;

    /* Timing */
    uint64_t last_encoding_ns;
    uint64_t last_retrieval_ns;
    uint64_t last_consolidation_ns;

} mesh_hippocampus_stats_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default hippocampus integration configuration
 */
nimcp_error_t mesh_hippocampus_default_config(mesh_hippocampus_config_t* config);

/**
 * @brief Create hippocampus mesh integration
 */
mesh_hippocampus_integration_t* mesh_hippocampus_create(
    mesh_bootstrap_t* bootstrap,
    hippocampus_adapter_t* hippocampus,
    const mesh_hippocampus_config_t* config
);

/**
 * @brief Destroy hippocampus mesh integration
 */
void mesh_hippocampus_destroy(mesh_hippocampus_integration_t* integration);

/* ============================================================================
 * Participant Registration API
 * ============================================================================ */

/**
 * @brief Register hippocampus as mesh participant
 */
nimcp_error_t mesh_hippocampus_register_participant(
    mesh_hippocampus_integration_t* integration
);

/**
 * @brief Unregister hippocampus from mesh
 */
nimcp_error_t mesh_hippocampus_unregister_participant(
    mesh_hippocampus_integration_t* integration
);

/**
 * @brief Get participant ID for the hippocampus
 */
mesh_participant_id_t mesh_hippocampus_get_participant_id(
    const mesh_hippocampus_integration_t* integration
);

/**
 * @brief Check if hippocampus is registered with mesh
 */
bool mesh_hippocampus_is_registered(
    const mesh_hippocampus_integration_t* integration
);

/* ============================================================================
 * Memory Operations API
 * ============================================================================ */

/**
 * @brief Propose memory encoding via mesh
 */
nimcp_error_t mesh_hippocampus_propose_encoding(
    mesh_hippocampus_integration_t* integration,
    const void* memory_data,
    size_t data_size,
    const char* context
);

/**
 * @brief Propose memory retrieval via mesh
 */
nimcp_error_t mesh_hippocampus_propose_retrieval(
    mesh_hippocampus_integration_t* integration,
    const void* query_cue,
    size_t cue_size
);

/**
 * @brief Trigger memory consolidation via mesh
 */
nimcp_error_t mesh_hippocampus_trigger_consolidation(
    mesh_hippocampus_integration_t* integration
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get hippocampus integration statistics
 */
nimcp_error_t mesh_hippocampus_get_stats(
    const mesh_hippocampus_integration_t* integration,
    mesh_hippocampus_stats_t* stats
);

/**
 * @brief Reset hippocampus integration statistics
 */
nimcp_error_t mesh_hippocampus_reset_stats(
    mesh_hippocampus_integration_t* integration
);

/* ============================================================================
 * Health Agent Integration API
 * ============================================================================ */

/**
 * @brief Set health agent for hippocampus integration
 */
nimcp_error_t mesh_hippocampus_set_health_agent(
    mesh_hippocampus_integration_t* integration,
    nimcp_health_agent_t* agent
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Check if transaction type is hippocampus-related
 */
bool mesh_hippocampus_is_hippocampus_transaction(mesh_tx_type_t tx_type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_HIPPOCAMPUS_INTEGRATION_H */
