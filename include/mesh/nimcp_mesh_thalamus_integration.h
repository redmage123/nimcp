/**
 * @file nimcp_mesh_thalamus_integration.h
 * @brief Thalamus Mesh Network Integration
 *
 * WHAT: Connects the thalamus module to the mesh network
 * WHY:  Enable coordinated sensory relay and attention gating via distributed consensus
 * HOW:  Register thalamus as PERCEPTION category participant, handle relay transactions
 *
 * BIOLOGICAL CONTEXT:
 * The thalamus is the brain's sensory relay hub responsible for:
 * - Relaying sensory information to cortex (except olfaction)
 * - Attention gating via TRN (Thalamic Reticular Nucleus)
 * - Arousal state modulation (tonic vs burst firing)
 * - Cortical-thalamic-cortical loops for processing
 *
 * In the mesh network, the thalamus:
 * - Participates in SUBCORTICAL channel
 * - Has REQUIRED endorser role for sensory relay policy
 * - Coordinates with cortex for attention allocation
 * - Gates information flow based on arousal/attention
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_THALAMUS_INTEGRATION_H
#define NIMCP_MESH_THALAMUS_INTEGRATION_H

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

typedef struct mesh_thalamus_integration mesh_thalamus_integration_t;
typedef struct mesh_bootstrap mesh_bootstrap_t;

/* Forward declaration for thalamus module */
typedef struct thalamus thalamus_t;

/* ============================================================================
 * Thalamus Transaction Types
 * ============================================================================ */

/**
 * @brief Thalamus-specific transaction types
 */
typedef enum mesh_thalamus_tx_type {
    /** Base for thalamus transactions (0x1900 = sensory relay range) */
    MESH_TX_THALAMUS_BASE = 0x1900,

    /** Visual relay (LGN) */
    MESH_TX_THALAMUS_RELAY_VISUAL = 0x1901,

    /** Auditory relay (MGN) */
    MESH_TX_THALAMUS_RELAY_AUDITORY = 0x1902,

    /** Somatosensory relay (VPL/VPM) */
    MESH_TX_THALAMUS_RELAY_SOMATO = 0x1903,

    /** Motor relay (VA/VL) */
    MESH_TX_THALAMUS_RELAY_MOTOR = 0x1904,

    /** Executive relay (MD) */
    MESH_TX_THALAMUS_RELAY_EXECUTIVE = 0x1905,

    /** Attention update (Pulvinar) */
    MESH_TX_THALAMUS_ATTENTION_UPDATE = 0x1906,

    /** TRN inhibition change */
    MESH_TX_THALAMUS_TRN_INHIBIT = 0x1907,

    /** Arousal state change */
    MESH_TX_THALAMUS_AROUSAL_CHANGE = 0x1908,

    /** Firing mode change (tonic/burst) */
    MESH_TX_THALAMUS_MODE_CHANGE = 0x1909,

    /** Burst trigger */
    MESH_TX_THALAMUS_BURST_TRIGGER = 0x190A,

} mesh_thalamus_tx_type_t;

/* ============================================================================
 * Firing Mode (matches thalamus module)
 * ============================================================================ */

/**
 * @brief Thalamic firing modes for mesh transactions
 */
typedef enum mesh_thalamus_firing_mode {
    MESH_THALAMUS_MODE_TONIC = 0,   /**< Alert, relay mode */
    MESH_THALAMUS_MODE_BURST        /**< Sleep/drowsy, oscillatory mode */
} mesh_thalamus_firing_mode_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Thalamus mesh integration configuration
 */
typedef struct mesh_thalamus_config {
    /* Behavior settings */
    bool broadcast_relay_activity;           /**< Broadcast relay events to mesh */
    bool enable_distributed_gating;          /**< Enable distributed attention gating */
    bool sync_arousal_with_mesh;             /**< Sync arousal state with mesh */

    /* Timeouts */
    uint32_t relay_timeout_ms;               /**< Timeout for relay transactions */
    uint32_t attention_timeout_ms;           /**< Timeout for attention updates */

    /* Health monitoring */
    bool enable_health_monitoring;           /**< Enable health agent integration */
    uint32_t heartbeat_interval_ms;          /**< Health heartbeat interval */

    /* Logging */
    bool verbose_logging;

} mesh_thalamus_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Thalamus integration statistics
 */
typedef struct mesh_thalamus_stats {
    /* Relay counts */
    uint64_t visual_relays;
    uint64_t auditory_relays;
    uint64_t somatosensory_relays;
    uint64_t motor_relays;
    uint64_t executive_relays;

    /* Gating */
    uint64_t attention_updates;
    uint64_t trn_inhibitions;

    /* Mode changes */
    uint64_t arousal_changes;
    uint64_t mode_changes;
    uint64_t burst_triggers;

    /* Current state */
    float current_arousal;
    float current_attention;
    mesh_thalamus_firing_mode_t current_mode;

    /* Mesh participation */
    uint64_t transactions_received;
    uint64_t transactions_endorsed;
    uint64_t transactions_vetoed;
    uint64_t endorsement_requests_handled;

    /* Health */
    uint64_t health_heartbeats_sent;

    /* Timing */
    uint64_t last_relay_ns;
    uint64_t last_arousal_change_ns;

} mesh_thalamus_stats_t;

/* ============================================================================
 * Transaction Payload Structures
 * ============================================================================ */

/**
 * @brief Relay payload for sensory transactions
 */
typedef struct mesh_thalamus_relay_payload {
    uint32_t nucleus_type;        /**< Which nucleus (LGN, MGN, etc.) */
    uint32_t channel_count;       /**< Number of channels in relay */
    float attention_weight;       /**< Current attention weight */
    float arousal_level;          /**< Current arousal affecting relay */
} mesh_thalamus_relay_payload_t;

/**
 * @brief Arousal change payload
 */
typedef struct mesh_thalamus_arousal_payload {
    float old_arousal;
    float new_arousal;
    mesh_thalamus_firing_mode_t resulting_mode;
} mesh_thalamus_arousal_payload_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default thalamus integration configuration
 */
nimcp_error_t mesh_thalamus_default_config(mesh_thalamus_config_t* config);

/**
 * @brief Create thalamus mesh integration
 */
mesh_thalamus_integration_t* mesh_thalamus_create(
    mesh_bootstrap_t* bootstrap,
    thalamus_t* thalamus,
    const mesh_thalamus_config_t* config
);

/**
 * @brief Destroy thalamus mesh integration
 */
void mesh_thalamus_destroy(mesh_thalamus_integration_t* integration);

/* ============================================================================
 * Participant Registration API
 * ============================================================================ */

/**
 * @brief Register thalamus as mesh participant
 */
nimcp_error_t mesh_thalamus_register_participant(
    mesh_thalamus_integration_t* integration
);

/**
 * @brief Unregister thalamus from mesh
 */
nimcp_error_t mesh_thalamus_unregister_participant(
    mesh_thalamus_integration_t* integration
);

/**
 * @brief Get participant ID for the thalamus
 */
mesh_participant_id_t mesh_thalamus_get_participant_id(
    const mesh_thalamus_integration_t* integration
);

/**
 * @brief Check if thalamus is registered with mesh
 */
bool mesh_thalamus_is_registered(
    const mesh_thalamus_integration_t* integration
);

/* ============================================================================
 * Relay Operations API
 * ============================================================================ */

/**
 * @brief Report sensory relay activity via mesh
 */
nimcp_error_t mesh_thalamus_report_relay(
    mesh_thalamus_integration_t* integration,
    uint32_t nucleus_type,
    uint32_t channel_count
);

/**
 * @brief Update attention via mesh
 */
nimcp_error_t mesh_thalamus_update_attention(
    mesh_thalamus_integration_t* integration,
    float new_attention
);

/**
 * @brief Update arousal via mesh
 */
nimcp_error_t mesh_thalamus_update_arousal(
    mesh_thalamus_integration_t* integration,
    float new_arousal
);

/* ============================================================================
 * State Query API
 * ============================================================================ */

/**
 * @brief Get current arousal level
 */
float mesh_thalamus_get_arousal(
    const mesh_thalamus_integration_t* integration
);

/**
 * @brief Get current attention level
 */
float mesh_thalamus_get_attention(
    const mesh_thalamus_integration_t* integration
);

/**
 * @brief Get current firing mode
 */
mesh_thalamus_firing_mode_t mesh_thalamus_get_mode(
    const mesh_thalamus_integration_t* integration
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get thalamus integration statistics
 */
nimcp_error_t mesh_thalamus_get_stats(
    const mesh_thalamus_integration_t* integration,
    mesh_thalamus_stats_t* stats
);

/**
 * @brief Reset thalamus integration statistics
 */
nimcp_error_t mesh_thalamus_reset_stats(
    mesh_thalamus_integration_t* integration
);

/* ============================================================================
 * Health Agent Integration API
 * ============================================================================ */

/**
 * @brief Set health agent for thalamus integration
 */
nimcp_error_t mesh_thalamus_set_health_agent(
    mesh_thalamus_integration_t* integration,
    nimcp_health_agent_t* agent
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Check if transaction type is thalamus-related
 */
bool mesh_thalamus_is_thalamus_transaction(mesh_tx_type_t tx_type);

/**
 * @brief Convert firing mode to string
 */
const char* mesh_thalamus_mode_to_string(mesh_thalamus_firing_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_THALAMUS_INTEGRATION_H */
