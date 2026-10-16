/**
 * @file nimcp_mesh_amygdala_integration.h
 * @brief Amygdala Mesh Network Integration
 *
 * WHAT: Connects the amygdala module to the mesh network
 * WHY:  Enable coordinated emotional processing via distributed consensus
 * HOW:  Register amygdala as SUBCORTICAL participant with VETO role
 *
 * BIOLOGICAL CONTEXT:
 * The amygdala is the brain's emotional processing center responsible for:
 * - Fear conditioning and fear responses
 * - Threat detection and vigilance
 * - Emotional memory tagging
 * - Anxiety state maintenance
 * - Fight/flight/freeze response initiation
 *
 * In the mesh network, the amygdala:
 * - Participates in SUBCORTICAL channel
 * - Has VETO role for emergency policy (can block dangerous actions)
 * - Coordinates with hippocampus for emotional memory
 * - Signals threat level to global workspace
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_AMYGDALA_INTEGRATION_H
#define NIMCP_MESH_AMYGDALA_INTEGRATION_H

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

typedef struct mesh_amygdala_integration mesh_amygdala_integration_t;
typedef struct mesh_bootstrap mesh_bootstrap_t;

/* Forward declaration for amygdala module */
typedef struct amygdala amygdala_t;

/* ============================================================================
 * Amygdala Transaction Types
 * ============================================================================ */

/**
 * @brief Amygdala-specific transaction types
 */
typedef enum mesh_amygdala_tx_type {
    /** Base for amygdala transactions (0x1700 = emotional range) */
    MESH_TX_AMYGDALA_BASE = 0x1700,

    /** Threat detected notification */
    MESH_TX_AMYGDALA_THREAT_DETECTED = 0x1701,

    /** Fear response triggered */
    MESH_TX_AMYGDALA_FEAR_RESPONSE = 0x1702,

    /** Fear conditioning event */
    MESH_TX_AMYGDALA_FEAR_CONDITIONING = 0x1703,

    /** Fear extinction event */
    MESH_TX_AMYGDALA_FEAR_EXTINCTION = 0x1704,

    /** Anxiety level change */
    MESH_TX_AMYGDALA_ANXIETY_CHANGE = 0x1705,

    /** Emotional valence update */
    MESH_TX_AMYGDALA_VALENCE_UPDATE = 0x1706,

    /** Emergency veto request */
    MESH_TX_AMYGDALA_VETO = 0x1707,

    /** Emotional memory tag */
    MESH_TX_AMYGDALA_MEMORY_TAG = 0x1708,

} mesh_amygdala_tx_type_t;

/* ============================================================================
 * Threat Levels (matches amygdala module)
 * ============================================================================ */

/**
 * @brief Threat level classification for mesh transactions
 */
typedef enum mesh_amygdala_threat_level {
    MESH_AMYGDALA_THREAT_NONE = 0,
    MESH_AMYGDALA_THREAT_LOW,
    MESH_AMYGDALA_THREAT_MODERATE,
    MESH_AMYGDALA_THREAT_HIGH,
    MESH_AMYGDALA_THREAT_SEVERE
} mesh_amygdala_threat_level_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Amygdala mesh integration configuration
 */
typedef struct mesh_amygdala_config {
    /* Initial state */
    mesh_amygdala_threat_level_t initial_threat_level;
    float initial_anxiety;

    /* Veto behavior */
    bool enable_veto_capability;             /**< Enable veto role for emergency */
    mesh_amygdala_threat_level_t veto_threshold; /**< Threat level to trigger veto */
    bool auto_veto_on_severe_threat;         /**< Auto-veto when severe threat detected */

    /* Timeouts */
    uint32_t threat_response_timeout_ms;     /**< Timeout for threat transactions */
    uint32_t veto_timeout_ms;                /**< Timeout for veto transactions */

    /* Health monitoring */
    bool enable_health_monitoring;           /**< Enable health agent integration */
    uint32_t heartbeat_interval_ms;          /**< Health heartbeat interval */

    /* Logging */
    bool verbose_logging;

} mesh_amygdala_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Amygdala integration statistics
 */
typedef struct mesh_amygdala_stats {
    /* Threat detection */
    uint64_t threats_detected;
    uint64_t threats_by_level[5];            /**< Count per threat level */

    /* Fear events */
    uint64_t fear_responses_triggered;
    uint64_t fear_conditioning_events;
    uint64_t fear_extinction_events;

    /* Veto actions */
    uint64_t veto_requests_issued;
    uint64_t veto_requests_applied;

    /* Current state */
    mesh_amygdala_threat_level_t current_threat_level;
    float current_anxiety;
    float current_fear;

    /* Mesh participation */
    uint64_t transactions_received;
    uint64_t transactions_endorsed;
    uint64_t transactions_vetoed;
    uint64_t endorsement_requests_handled;

    /* Health */
    uint64_t health_heartbeats_sent;

    /* Timing */
    uint64_t last_threat_detection_ns;
    uint64_t last_veto_ns;

} mesh_amygdala_stats_t;

/* ============================================================================
 * Transaction Payload Structures
 * ============================================================================ */

/**
 * @brief Threat detected payload
 */
typedef struct mesh_amygdala_threat_payload {
    mesh_amygdala_threat_level_t threat_level;
    float threat_intensity;
    uint32_t threat_source;                  /**< Source module/stimulus ID */
    char description[128];
} mesh_amygdala_threat_payload_t;

/**
 * @brief Veto request payload
 */
typedef struct mesh_amygdala_veto_payload {
    mesh_participant_id_t target_transaction;
    mesh_amygdala_threat_level_t threat_level;
    char reason[256];
    bool is_automatic;                       /**< Auto-veto vs deliberate */
} mesh_amygdala_veto_payload_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Called when threat level changes
 */
typedef void (*mesh_amygdala_threat_callback_t)(
    mesh_amygdala_threat_level_t old_level,
    mesh_amygdala_threat_level_t new_level,
    void* ctx
);

/**
 * @brief Called when veto is issued
 */
typedef void (*mesh_amygdala_veto_callback_t)(
    mesh_participant_id_t target_tx,
    const char* reason,
    void* ctx
);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default amygdala integration configuration
 */
nimcp_error_t mesh_amygdala_default_config(mesh_amygdala_config_t* config);

/**
 * @brief Create amygdala mesh integration
 */
mesh_amygdala_integration_t* mesh_amygdala_create(
    mesh_bootstrap_t* bootstrap,
    amygdala_t* amygdala,
    const mesh_amygdala_config_t* config
);

/**
 * @brief Destroy amygdala mesh integration
 */
void mesh_amygdala_destroy(mesh_amygdala_integration_t* integration);

/* ============================================================================
 * Participant Registration API
 * ============================================================================ */

/**
 * @brief Register amygdala as mesh participant with VETO role
 */
nimcp_error_t mesh_amygdala_register_participant(
    mesh_amygdala_integration_t* integration
);

/**
 * @brief Unregister amygdala from mesh
 */
nimcp_error_t mesh_amygdala_unregister_participant(
    mesh_amygdala_integration_t* integration
);

/**
 * @brief Get participant ID for the amygdala
 */
mesh_participant_id_t mesh_amygdala_get_participant_id(
    const mesh_amygdala_integration_t* integration
);

/**
 * @brief Check if amygdala is registered with mesh
 */
bool mesh_amygdala_is_registered(
    const mesh_amygdala_integration_t* integration
);

/* ============================================================================
 * Threat and Emotional API
 * ============================================================================ */

/**
 * @brief Report threat detection via mesh
 */
nimcp_error_t mesh_amygdala_report_threat(
    mesh_amygdala_integration_t* integration,
    mesh_amygdala_threat_level_t level,
    float intensity,
    const char* description
);

/**
 * @brief Issue veto for a transaction
 */
nimcp_error_t mesh_amygdala_issue_veto(
    mesh_amygdala_integration_t* integration,
    mesh_participant_id_t target_tx,
    const char* reason
);

/**
 * @brief Update anxiety level via mesh
 */
nimcp_error_t mesh_amygdala_update_anxiety(
    mesh_amygdala_integration_t* integration,
    float new_anxiety
);

/* ============================================================================
 * State Query API
 * ============================================================================ */

/**
 * @brief Get current threat level
 */
mesh_amygdala_threat_level_t mesh_amygdala_get_threat_level(
    const mesh_amygdala_integration_t* integration
);

/**
 * @brief Get current anxiety level
 */
float mesh_amygdala_get_anxiety(
    const mesh_amygdala_integration_t* integration
);

/**
 * @brief Get current fear level
 */
float mesh_amygdala_get_fear(
    const mesh_amygdala_integration_t* integration
);

/* ============================================================================
 * Callback Registration API
 * ============================================================================ */

/**
 * @brief Set threat level change callback
 */
nimcp_error_t mesh_amygdala_set_threat_callback(
    mesh_amygdala_integration_t* integration,
    mesh_amygdala_threat_callback_t callback,
    void* ctx
);

/**
 * @brief Set veto callback
 */
nimcp_error_t mesh_amygdala_set_veto_callback(
    mesh_amygdala_integration_t* integration,
    mesh_amygdala_veto_callback_t callback,
    void* ctx
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get amygdala integration statistics
 */
nimcp_error_t mesh_amygdala_get_stats(
    const mesh_amygdala_integration_t* integration,
    mesh_amygdala_stats_t* stats
);

/**
 * @brief Reset amygdala integration statistics
 */
nimcp_error_t mesh_amygdala_reset_stats(
    mesh_amygdala_integration_t* integration
);

/* ============================================================================
 * Health Agent Integration API
 * ============================================================================ */

/**
 * @brief Set health agent for amygdala integration
 */
nimcp_error_t mesh_amygdala_set_health_agent(
    mesh_amygdala_integration_t* integration,
    nimcp_health_agent_t* agent
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert threat level to string
 */
const char* mesh_amygdala_threat_to_string(mesh_amygdala_threat_level_t level);

/**
 * @brief Check if transaction type is amygdala-related
 */
bool mesh_amygdala_is_amygdala_transaction(mesh_tx_type_t tx_type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_AMYGDALA_INTEGRATION_H */
