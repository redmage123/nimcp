/**
 * @file nimcp_mesh_medulla_integration.h
 * @brief Medulla Oblongata Mesh Network Integration
 *
 * WHAT: Connects the medulla oblongata module to the mesh network
 * WHY:  Enable coordinated brainstem control via distributed consensus
 * HOW:  Register medulla as subcortical participant, handle arousal/protection transactions
 *
 * BIOLOGICAL CONTEXT:
 * The medulla oblongata is the lower brainstem region that controls:
 * - Arousal state and consciousness level
 * - Autonomic functions (heart rate, breathing)
 * - Protective reflexes (cough, sneeze, gag, vomiting)
 * - Sleep-wake transitions
 * - Emergency shutdown procedures
 *
 * In the mesh network, the medulla:
 * - Participates in SUBCORTICAL channel
 * - Has REQUIRED endorser role for arousal state changes
 * - Can veto transactions during emergency shutdown
 * - Coordinates with hypothalamus for autonomic control
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────────┐
 * │                    MEDULLA MESH INTEGRATION LAYER                            │
 * ├─────────────────────────────────────────────────────────────────────────────┤
 * │                                                                              │
 * │  ┌──────────────────────────────────────────────────────────────────────┐   │
 * │  │                      MEDULLA OBLONGATA                                │   │
 * │  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐    │   │
 * │  │  │ Arousal  │ │ Autonomic│ │Protection│ │  Sleep   │ │Emergency │    │   │
 * │  │  │  State   │ │ Control  │ │ Reflexes │ │   Wake   │ │ Shutdown │    │   │
 * │  │  └─────┬─────┘ └─────┬────┘ └────┬─────┘ └────┬────┘ └─────┬────┘    │   │
 * │  └────────│─────────────│───────────│────────────│────────────│─────────┘   │
 * │           │             │           │            │            │              │
 * │           ▼             ▼           ▼            ▼            ▼              │
 * │  ┌────────────────────────────────────────────────────────────────────────┐ │
 * │  │              MESH MEDULLA INTEGRATION                                  │ │
 * │  │  • Participant registration in SUBCORTICAL channel                    │ │
 * │  │  • Transaction handlers for state changes                             │ │
 * │  │  • Receptive field for brainstem patterns                            │ │
 * │  │  • Health agent heartbeat integration                                 │ │
 * │  └────────────────────────────────────────────────────────────────────────┘ │
 * │                                    │                                        │
 * │                                    ▼                                        │
 * │  ┌────────────────────────────────────────────────────────────────────────┐ │
 * │  │                        MESH BOOTSTRAP                                  │ │
 * │  │  ┌──────────────┐  ┌───────────────┐  ┌─────────────────┐             │ │
 * │  │  │SUBCORTICAL   │  │Pattern Router │  │ Health Bridge   │             │ │
 * │  │  │  Channel     │  │               │  │                 │             │ │
 * │  │  └──────────────┘  └───────────────┘  └─────────────────┘             │ │
 * │  └────────────────────────────────────────────────────────────────────────┘ │
 * │                                                                              │
 * └─────────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_MEDULLA_INTEGRATION_H
#define NIMCP_MESH_MEDULLA_INTEGRATION_H

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

typedef struct mesh_medulla_integration mesh_medulla_integration_t;
typedef struct mesh_bootstrap mesh_bootstrap_t;

/* Forward declaration for medulla module (if/when it exists) */
typedef struct medulla_oblongata medulla_oblongata_t;

/* ============================================================================
 * Medulla Transaction Types
 * ============================================================================ */

/**
 * @brief Medulla-specific transaction types
 *
 * These are in the MESH_TX_CUSTOM range for brainstem functions
 */
typedef enum mesh_medulla_tx_type {
    /** Base for medulla transactions (0x1500 = brainstem range) */
    MESH_TX_MEDULLA_BASE = 0x1500,

    /** Arousal state change request */
    MESH_TX_MEDULLA_AROUSAL_CHANGE = 0x1501,

    /** Protection level change (reflex sensitivity) */
    MESH_TX_MEDULLA_PROTECTION_CHANGE = 0x1502,

    /** Sleep-wake state transition */
    MESH_TX_MEDULLA_SLEEP_WAKE = 0x1503,

    /** Autonomic function update */
    MESH_TX_MEDULLA_AUTONOMIC_UPDATE = 0x1504,

    /** Emergency shutdown request (highest priority) */
    MESH_TX_MEDULLA_EMERGENCY_SHUTDOWN = 0x1505,

    /** Emergency recovery request */
    MESH_TX_MEDULLA_EMERGENCY_RECOVERY = 0x1506,

    /** Reflex trigger notification */
    MESH_TX_MEDULLA_REFLEX_TRIGGER = 0x1507,

    /** Heartbeat/vital signs update */
    MESH_TX_MEDULLA_VITALS_UPDATE = 0x1508,

} mesh_medulla_tx_type_t;

/* ============================================================================
 * Arousal and Protection States
 * ============================================================================ */

/**
 * @brief Arousal state levels
 */
typedef enum mesh_medulla_arousal_state {
    MEDULLA_AROUSAL_COMA = 0,         /**< Completely unresponsive */
    MEDULLA_AROUSAL_DEEP_SLEEP,        /**< Deep sleep, minimal response */
    MEDULLA_AROUSAL_LIGHT_SLEEP,       /**< Light sleep, can be awakened */
    MEDULLA_AROUSAL_DROWSY,            /**< Transitional state */
    MEDULLA_AROUSAL_RELAXED,           /**< Relaxed wakefulness */
    MEDULLA_AROUSAL_ALERT,             /**< Normal alert state */
    MEDULLA_AROUSAL_VIGILANT,          /**< Heightened alertness */
    MEDULLA_AROUSAL_HYPERVIGILANT,     /**< Extreme alertness (stress) */
    MEDULLA_AROUSAL_EMERGENCY,         /**< Emergency response mode */
} mesh_medulla_arousal_state_t;

/**
 * @brief Protection level (reflex sensitivity)
 */
typedef enum mesh_medulla_protection_level {
    MEDULLA_PROTECT_DISABLED = 0,      /**< Reflexes disabled (dangerous) */
    MEDULLA_PROTECT_MINIMAL,           /**< Minimal protection */
    MEDULLA_PROTECT_NORMAL,            /**< Normal reflex sensitivity */
    MEDULLA_PROTECT_HEIGHTENED,        /**< Heightened sensitivity */
    MEDULLA_PROTECT_MAXIMUM,           /**< Maximum protection (all reflexes active) */
} mesh_medulla_protection_level_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Medulla mesh integration configuration
 */
typedef struct mesh_medulla_config {
    /* Initial state */
    mesh_medulla_arousal_state_t initial_arousal;
    mesh_medulla_protection_level_t initial_protection;

    /* Behavior settings */
    bool auto_emergency_on_critical_health;  /**< Auto-trigger emergency on critical health */
    bool require_consensus_for_arousal;      /**< Require mesh consensus for arousal changes */
    bool require_consensus_for_protection;   /**< Require mesh consensus for protection changes */
    bool allow_external_shutdown;            /**< Allow external emergency shutdown requests */

    /* Timeouts */
    uint32_t arousal_change_timeout_ms;      /**< Timeout for arousal change transactions */
    uint32_t emergency_response_timeout_ms;  /**< Timeout for emergency transactions */

    /* Health monitoring */
    bool enable_health_monitoring;           /**< Enable health agent integration */
    uint32_t heartbeat_interval_ms;          /**< Health heartbeat interval */

    /* Logging */
    bool verbose_logging;

} mesh_medulla_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Medulla integration statistics
 */
typedef struct mesh_medulla_stats {
    /* Transaction counts */
    uint64_t arousal_changes_proposed;
    uint64_t arousal_changes_committed;
    uint64_t arousal_changes_rejected;

    uint64_t protection_changes_proposed;
    uint64_t protection_changes_committed;
    uint64_t protection_changes_rejected;

    uint64_t emergency_shutdowns_triggered;
    uint64_t emergency_recoveries_completed;

    uint64_t reflex_triggers_notified;

    /* Current state */
    mesh_medulla_arousal_state_t current_arousal;
    mesh_medulla_protection_level_t current_protection;
    bool in_emergency_mode;

    /* Mesh participation */
    uint64_t transactions_received;
    uint64_t transactions_endorsed;
    uint64_t transactions_vetoed;
    uint64_t endorsement_requests_handled;

    /* Health */
    uint64_t health_heartbeats_sent;
    uint64_t health_alerts_raised;

    /* Timing */
    uint64_t last_arousal_change_ns;
    uint64_t last_protection_change_ns;
    uint64_t last_emergency_ns;

} mesh_medulla_stats_t;

/* ============================================================================
 * Transaction Payload Structures
 * ============================================================================ */

/**
 * @brief Arousal change request payload
 */
typedef struct mesh_medulla_arousal_payload {
    mesh_medulla_arousal_state_t target_state;
    mesh_medulla_arousal_state_t current_state;
    char reason[128];
    bool urgent;
} mesh_medulla_arousal_payload_t;

/**
 * @brief Protection change request payload
 */
typedef struct mesh_medulla_protection_payload {
    mesh_medulla_protection_level_t target_level;
    mesh_medulla_protection_level_t current_level;
    char reason[128];
} mesh_medulla_protection_payload_t;

/**
 * @brief Emergency shutdown payload
 */
typedef struct mesh_medulla_emergency_payload {
    uint32_t emergency_code;
    char reason[256];
    mesh_participant_id_t requesting_module;
    bool force_immediate;
} mesh_medulla_emergency_payload_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Called when arousal state changes
 *
 * @param old_state Previous arousal state
 * @param new_state New arousal state
 * @param ctx User context
 */
typedef void (*mesh_medulla_arousal_callback_t)(
    mesh_medulla_arousal_state_t old_state,
    mesh_medulla_arousal_state_t new_state,
    void* ctx
);

/**
 * @brief Called when protection level changes
 *
 * @param old_level Previous protection level
 * @param new_level New protection level
 * @param ctx User context
 */
typedef void (*mesh_medulla_protection_callback_t)(
    mesh_medulla_protection_level_t old_level,
    mesh_medulla_protection_level_t new_level,
    void* ctx
);

/**
 * @brief Called when emergency is triggered
 *
 * @param emergency_code Emergency code
 * @param reason Emergency reason
 * @param ctx User context
 */
typedef void (*mesh_medulla_emergency_callback_t)(
    uint32_t emergency_code,
    const char* reason,
    void* ctx
);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default medulla integration configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_medulla_default_config(mesh_medulla_config_t* config);

/**
 * @brief Create medulla mesh integration
 *
 * WHAT: Connect medulla module to mesh network
 * WHY:  Enable coordinated brainstem control via consensus
 * HOW:  Register as subcortical participant, install handlers
 *
 * @param bootstrap Mesh bootstrap handle
 * @param medulla Medulla module handle (can be NULL for standalone)
 * @param config Configuration (NULL for defaults)
 * @return Integration handle or NULL on failure
 */
mesh_medulla_integration_t* mesh_medulla_create(
    mesh_bootstrap_t* bootstrap,
    medulla_oblongata_t* medulla,
    const mesh_medulla_config_t* config
);

/**
 * @brief Destroy medulla mesh integration
 *
 * @param integration Integration to destroy (NULL-safe)
 */
void mesh_medulla_destroy(mesh_medulla_integration_t* integration);

/* ============================================================================
 * Participant Registration API
 * ============================================================================ */

/**
 * @brief Register medulla as mesh participant
 *
 * WHAT: Register medulla with mesh network as subcortical participant
 * WHY:  Enable participation in mesh transactions and consensus
 * HOW:  Create participant interface, register with coordinator pool
 *
 * @param integration Medulla integration handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_medulla_register_participant(
    mesh_medulla_integration_t* integration
);

/**
 * @brief Unregister medulla from mesh
 *
 * @param integration Medulla integration handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_medulla_unregister_participant(
    mesh_medulla_integration_t* integration
);

/**
 * @brief Get participant ID for the medulla
 *
 * @param integration Medulla integration handle
 * @return Participant ID or 0 if not registered
 */
mesh_participant_id_t mesh_medulla_get_participant_id(
    const mesh_medulla_integration_t* integration
);

/**
 * @brief Check if medulla is registered with mesh
 *
 * @param integration Medulla integration handle
 * @return true if registered
 */
bool mesh_medulla_is_registered(
    const mesh_medulla_integration_t* integration
);

/* ============================================================================
 * Transaction Handling API
 * ============================================================================ */

/**
 * @brief Handle incoming mesh transaction
 *
 * WHAT: Process transactions relevant to medulla
 * WHY:  React to arousal/protection requests from other modules
 * HOW:  Check transaction type, validate, execute or endorse
 *
 * Called by mesh network when transactions arrive for medulla.
 *
 * @param integration Medulla integration handle
 * @param tx Transaction to handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_medulla_on_transaction(
    mesh_medulla_integration_t* integration,
    const mesh_transaction_t* tx
);

/**
 * @brief Endorse a transaction
 *
 * Medulla may be required to endorse certain transaction types,
 * especially those affecting system arousal or protection.
 *
 * @param integration Medulla integration handle
 * @param tx Transaction to endorse
 * @param endorsement Output endorsement
 * @return NIMCP_SUCCESS if endorsed, error if rejected
 */
nimcp_error_t mesh_medulla_endorse_transaction(
    mesh_medulla_integration_t* integration,
    const mesh_transaction_t* tx,
    mesh_endorsement_t* endorsement
);

/* ============================================================================
 * State Change Proposal API
 * ============================================================================ */

/**
 * @brief Propose arousal state change via mesh
 *
 * WHAT: Request arousal state change through mesh consensus
 * WHY:  Coordinate arousal changes with other brain regions
 * HOW:  Create transaction, submit for endorsement and ordering
 *
 * @param integration Medulla integration handle
 * @param target_state Desired arousal state
 * @param reason Reason for change
 * @param urgent If true, use emergency path
 * @return NIMCP_SUCCESS if proposal submitted
 */
nimcp_error_t mesh_medulla_propose_arousal_change(
    mesh_medulla_integration_t* integration,
    mesh_medulla_arousal_state_t target_state,
    const char* reason,
    bool urgent
);

/**
 * @brief Propose protection level change via mesh
 *
 * @param integration Medulla integration handle
 * @param target_level Desired protection level
 * @param reason Reason for change
 * @return NIMCP_SUCCESS if proposal submitted
 */
nimcp_error_t mesh_medulla_propose_protection_change(
    mesh_medulla_integration_t* integration,
    mesh_medulla_protection_level_t target_level,
    const char* reason
);

/**
 * @brief Trigger emergency shutdown via mesh
 *
 * WHAT: Request emergency shutdown of the system
 * WHY:  Critical situation requiring immediate response
 * HOW:  Emergency transaction with bypass endorsement
 *
 * @param integration Medulla integration handle
 * @param emergency_code Emergency code
 * @param reason Emergency reason
 * @param force_immediate Force immediate shutdown without consensus
 * @return NIMCP_SUCCESS if shutdown initiated
 */
nimcp_error_t mesh_medulla_emergency_shutdown(
    mesh_medulla_integration_t* integration,
    uint32_t emergency_code,
    const char* reason,
    bool force_immediate
);

/**
 * @brief Initiate emergency recovery via mesh
 *
 * @param integration Medulla integration handle
 * @return NIMCP_SUCCESS if recovery initiated
 */
nimcp_error_t mesh_medulla_emergency_recovery(
    mesh_medulla_integration_t* integration
);

/* ============================================================================
 * State Query API
 * ============================================================================ */

/**
 * @brief Get current arousal state
 *
 * @param integration Medulla integration handle
 * @return Current arousal state
 */
mesh_medulla_arousal_state_t mesh_medulla_get_arousal_state(
    const mesh_medulla_integration_t* integration
);

/**
 * @brief Get current protection level
 *
 * @param integration Medulla integration handle
 * @return Current protection level
 */
mesh_medulla_protection_level_t mesh_medulla_get_protection_level(
    const mesh_medulla_integration_t* integration
);

/**
 * @brief Check if in emergency mode
 *
 * @param integration Medulla integration handle
 * @return true if in emergency mode
 */
bool mesh_medulla_is_emergency_mode(
    const mesh_medulla_integration_t* integration
);

/* ============================================================================
 * Callback Registration API
 * ============================================================================ */

/**
 * @brief Set arousal change callback
 *
 * @param integration Medulla integration handle
 * @param callback Callback function
 * @param ctx User context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_medulla_set_arousal_callback(
    mesh_medulla_integration_t* integration,
    mesh_medulla_arousal_callback_t callback,
    void* ctx
);

/**
 * @brief Set protection change callback
 *
 * @param integration Medulla integration handle
 * @param callback Callback function
 * @param ctx User context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_medulla_set_protection_callback(
    mesh_medulla_integration_t* integration,
    mesh_medulla_protection_callback_t callback,
    void* ctx
);

/**
 * @brief Set emergency callback
 *
 * @param integration Medulla integration handle
 * @param callback Callback function
 * @param ctx User context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_medulla_set_emergency_callback(
    mesh_medulla_integration_t* integration,
    mesh_medulla_emergency_callback_t callback,
    void* ctx
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get medulla integration statistics
 *
 * @param integration Medulla integration handle
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_medulla_get_stats(
    const mesh_medulla_integration_t* integration,
    mesh_medulla_stats_t* stats
);

/**
 * @brief Reset medulla integration statistics
 *
 * @param integration Medulla integration handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_medulla_reset_stats(
    mesh_medulla_integration_t* integration
);

/* ============================================================================
 * Health Agent Integration API
 * ============================================================================ */

/**
 * @brief Set health agent for medulla integration
 *
 * @param integration Medulla integration handle
 * @param agent Health agent (NULL to disable)
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_medulla_set_health_agent(
    mesh_medulla_integration_t* integration,
    nimcp_health_agent_t* agent
);

/**
 * @brief Send health heartbeat
 *
 * @param integration Medulla integration handle
 * @param operation Current operation
 * @param progress Progress [0-1]
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_medulla_heartbeat(
    mesh_medulla_integration_t* integration,
    const char* operation,
    float progress
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert arousal state to string
 *
 * @param state Arousal state
 * @return State name string
 */
const char* mesh_medulla_arousal_to_string(mesh_medulla_arousal_state_t state);

/**
 * @brief Convert protection level to string
 *
 * @param level Protection level
 * @return Level name string
 */
const char* mesh_medulla_protection_to_string(mesh_medulla_protection_level_t level);

/**
 * @brief Check if transaction type is medulla-related
 *
 * @param tx_type Transaction type
 * @return true if medulla transaction type
 */
bool mesh_medulla_is_medulla_transaction(mesh_tx_type_t tx_type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_MEDULLA_INTEGRATION_H */
