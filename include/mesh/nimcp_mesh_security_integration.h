/**
 * @file nimcp_mesh_security_integration.h
 * @brief Mesh Network Security Integration - Exception, Immune, and BBB Bridge
 *
 * WHAT: Unified integration layer connecting mesh network to security systems
 * WHY:  Enable coordinated security response across mesh via BBB and immune system
 * HOW:  Exception -> Antigen -> Immune pathway with BBB mesh validation hooks
 *
 * SECURITY INTEGRATION ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────────┐
 * │                   MESH SECURITY INTEGRATION                                  │
 * ├─────────────────────────────────────────────────────────────────────────────┤
 * │                                                                              │
 * │  ┌──────────────────────────────────────────────────────────────────────┐   │
 * │  │                    EXCEPTION PATHWAY                                  │   │
 * │  │  Exception Thrown -> Antigen Creation -> Immune Presentation          │   │
 * │  │                                                                        │   │
 * │  │  ┌────────────┐    ┌────────────────┐    ┌──────────────────────┐    │   │
 * │  │  │ Exception  │───>│ Mesh Exception │───>│   Brain Immune       │    │   │
 * │  │  │  Source    │    │    Bridge      │    │  System (Antigen)    │    │   │
 * │  │  └────────────┘    └────────────────┘    └──────────────────────┘    │   │
 * │  └──────────────────────────────────────────────────────────────────────┘   │
 * │                               │                                              │
 * │                               ▼                                              │
 * │  ┌──────────────────────────────────────────────────────────────────────┐   │
 * │  │                    BBB VALIDATION LAYER                               │   │
 * │  │  All mesh transactions validated through Blood-Brain Barrier          │   │
 * │  │                                                                        │   │
 * │  │  ┌────────────┐    ┌────────────────┐    ┌──────────────────────┐    │   │
 * │  │  │   Mesh     │───>│     BBB        │───>│  Threat Assessment   │    │   │
 * │  │  │Transaction │    │  Validation    │    │  (Pass/Quarantine)   │    │   │
 * │  │  └────────────┘    └────────────────┘    └──────────────────────┘    │   │
 * │  └──────────────────────────────────────────────────────────────────────┘   │
 * │                               │                                              │
 * │                               ▼                                              │
 * │  ┌──────────────────────────────────────────────────────────────────────┐   │
 * │  │                    MSP SECURITY HOOKS                                 │   │
 * │  │  Quarantine/Revocation notifications routed through mesh              │   │
 * │  │                                                                        │   │
 * │  │  Immune Event ──> MSP Quarantine ──> Channel Notification              │   │
 * │  └──────────────────────────────────────────────────────────────────────┘   │
 * │                                                                              │
 * └─────────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * IMMUNE EVENT FLOW:
 * 1. Exception occurs in mesh participant
 * 2. Exception bridge converts to antigen
 * 3. BBB validates antigen threat level
 * 4. Immune system processes antigen (B cells, T cells)
 * 5. If threat confirmed: MSP quarantines participant
 * 6. Quarantine notification broadcast via security channel
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_SECURITY_INTEGRATION_H
#define NIMCP_MESH_SECURITY_INTEGRATION_H

#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_msp.h"
#include "mesh/nimcp_mesh_exception_bridge.h"
#include "utils/error/nimcp_error_codes.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct mesh_security_integration mesh_security_integration_t;
typedef struct mesh_bootstrap mesh_bootstrap_t;
typedef struct mesh_channel mesh_channel_t;
typedef struct brain_immune_system brain_immune_system_t;

/* Note: bbb_system_t is declared in nimcp_mesh_msp.h or nimcp_blood_brain_barrier.h */

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum pending security events */
#define MESH_SECURITY_MAX_PENDING_EVENTS    256

/** @brief Maximum threat history entries */
#define MESH_SECURITY_MAX_THREAT_HISTORY    512

/** @brief Security channel for quarantine/revocation broadcasts */
#define MESH_CHANNEL_SECURITY               15

/** @brief Default BBB threat threshold for quarantine */
#define MESH_SECURITY_DEFAULT_THREAT_THRESHOLD  0.7f

/** @brief Default immune severity threshold for notification */
#define MESH_SECURITY_DEFAULT_SEVERITY_THRESHOLD  MESH_EXC_SEVERITY_ERROR

/* ============================================================================
 * Security Event Types
 * ============================================================================ */

/**
 * @brief Type of security event
 */
typedef enum mesh_security_event_type {
    MESH_SEC_EVENT_NONE = 0,              /**< No event */
    MESH_SEC_EVENT_EXCEPTION_DETECTED,    /**< Exception detected and routed */
    MESH_SEC_EVENT_ANTIGEN_PRESENTED,     /**< Antigen presented to immune */
    MESH_SEC_EVENT_BBB_THREAT,            /**< BBB detected threat */
    MESH_SEC_EVENT_QUARANTINE_ISSUED,     /**< Quarantine issued by MSP */
    MESH_SEC_EVENT_QUARANTINE_RELEASED,   /**< Quarantine released */
    MESH_SEC_EVENT_CREDENTIAL_REVOKED,    /**< Credential permanently revoked */
    MESH_SEC_EVENT_IMMUNE_RESPONSE,       /**< Immune response triggered */
    MESH_SEC_EVENT_INFLAMMATION,          /**< Inflammation level changed */
    MESH_SEC_EVENT_RECOVERY_COMPLETE,     /**< Recovery completed successfully */
    MESH_SEC_EVENT_SECURITY_BROADCAST     /**< Security notification broadcast */
} mesh_security_event_type_t;

/**
 * @brief Security event details
 */
typedef struct mesh_security_event {
    mesh_security_event_type_t type;      /**< Event type */
    mesh_participant_id_t participant;    /**< Affected participant */
    mesh_channel_id_t channel;            /**< Source channel */

    /* Event-specific data */
    union {
        struct {
            nimcp_error_t error_code;
            mesh_exception_severity_t severity;
            mesh_exception_category_t category;
        } exception;

        struct {
            uint32_t antigen_id;
            float threat_score;
        } antigen;

        struct {
            uint64_t duration_ms;
            const char* reason;
        } quarantine;

        struct {
            mesh_immune_action_t action;
            float inflammation_level;
        } immune;
    } data;

    uint64_t timestamp_ns;                /**< Event timestamp */
} mesh_security_event_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Security integration configuration
 */
typedef struct mesh_security_config {
    /* Thresholds */
    float bbb_threat_threshold;           /**< BBB threat score for quarantine */
    mesh_exception_severity_t severity_threshold; /**< Min severity for notification */
    float inflammation_threshold;         /**< Inflammation level for escalation */

    /* Timing */
    uint64_t quarantine_duration_ms;      /**< Default quarantine duration */
    uint64_t recovery_timeout_ms;         /**< Recovery timeout */
    uint64_t broadcast_interval_ms;       /**< Min interval between broadcasts */

    /* Features */
    bool enable_auto_quarantine;          /**< Auto-quarantine on threat */
    bool enable_bbb_validation;           /**< Validate all transactions via BBB */
    bool enable_immune_routing;           /**< Route exceptions to immune */
    bool enable_security_broadcasts;      /**< Broadcast security events */
    bool enable_credential_tracking;      /**< Track credential events */

    /* Logging */
    bool verbose_logging;                 /**< Verbose security logging */
} mesh_security_config_t;

/**
 * @brief Security integration statistics
 */
typedef struct mesh_security_stats {
    /* Event counts */
    uint64_t exceptions_routed;           /**< Exceptions routed to immune */
    uint64_t antigens_presented;          /**< Antigens presented */
    uint64_t bbb_validations;             /**< BBB validation calls */
    uint64_t bbb_threats_detected;        /**< BBB threats detected */

    /* Action counts */
    uint64_t quarantine_issued;           /**< Quarantines issued */
    uint64_t quarantine_released;         /**< Quarantines released */
    uint64_t credentials_revoked;         /**< Credentials revoked */

    /* Immune metrics */
    uint64_t immune_responses;            /**< Immune responses triggered */
    float current_inflammation;           /**< Current inflammation level */
    uint64_t inflammation_events;         /**< Inflammation level changes */

    /* Broadcast metrics */
    uint64_t security_broadcasts;         /**< Security broadcasts sent */
    uint64_t broadcast_failures;          /**< Failed broadcasts */
} mesh_security_stats_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Security event callback
 */
typedef void (*mesh_security_event_cb_t)(
    mesh_security_integration_t* integration,
    const mesh_security_event_t* event,
    void* user_data
);

/**
 * @brief Quarantine decision callback
 *
 * Return true to proceed with quarantine, false to override
 */
typedef bool (*mesh_security_quarantine_cb_t)(
    mesh_security_integration_t* integration,
    mesh_participant_id_t participant,
    float threat_score,
    const char* reason,
    void* user_data
);

/**
 * @brief BBB validation callback
 *
 * Called for each mesh transaction before processing
 */
typedef bool (*mesh_security_bbb_validate_cb_t)(
    mesh_security_integration_t* integration,
    const mesh_transaction_t* tx,
    float* threat_score_out,
    void* user_data
);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default security configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_security_default_config(mesh_security_config_t* config);

/**
 * @brief Create security integration
 *
 * WHAT: Create unified security integration layer
 * WHY:  Bridge mesh network to BBB and immune systems
 *
 * @param bootstrap Mesh bootstrap handle
 * @param bbb BBB system handle (optional, can set later)
 * @param immune Immune system handle (optional, can set later)
 * @param msp MSP handle (optional, can set later)
 * @param config Configuration (NULL for defaults)
 * @return Security integration handle or NULL on failure
 */
mesh_security_integration_t* mesh_security_create(
    mesh_bootstrap_t* bootstrap,
    bbb_system_t bbb,
    brain_immune_system_t* immune,
    mesh_msp_t* msp,
    const mesh_security_config_t* config
);

/**
 * @brief Destroy security integration
 *
 * @param integration Integration to destroy (NULL-safe)
 */
void mesh_security_destroy(mesh_security_integration_t* integration);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Set BBB system
 *
 * @param integration Security integration
 * @param bbb BBB system handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_security_set_bbb(
    mesh_security_integration_t* integration,
    bbb_system_t bbb
);

/**
 * @brief Set brain immune system
 *
 * @param integration Security integration
 * @param immune Immune system handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_security_set_immune(
    mesh_security_integration_t* integration,
    brain_immune_system_t* immune
);

/**
 * @brief Set MSP
 *
 * @param integration Security integration
 * @param msp MSP handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_security_set_msp(
    mesh_security_integration_t* integration,
    mesh_msp_t* msp
);

/**
 * @brief Set exception bridge
 *
 * @param integration Security integration
 * @param bridge Exception bridge handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_security_set_exception_bridge(
    mesh_security_integration_t* integration,
    mesh_exception_bridge_t* bridge
);

/* ============================================================================
 * Exception -> Antigen -> Immune Pathway
 * ============================================================================ */

/**
 * @brief Route exception through security integration
 *
 * WHAT: Full exception routing through BBB and immune system
 * WHY:  Coordinated security response to errors
 * HOW:  Exception -> Antigen -> BBB validate -> Immune present
 *
 * @param integration Security integration
 * @param error_code Error code
 * @param message Error message
 * @param source_module Source module participant ID
 * @param source_file Source file (can be __FILE__)
 * @param source_line Source line (can be __LINE__)
 * @param response_out Output: immune response (can be NULL)
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_security_route_exception(
    mesh_security_integration_t* integration,
    nimcp_error_t error_code,
    const char* message,
    mesh_participant_id_t source_module,
    const char* source_file,
    uint32_t source_line,
    mesh_exception_response_t* response_out
);

/**
 * @brief Present antigen directly to immune system via mesh
 *
 * @param integration Security integration
 * @param antigen Antigen to present
 * @param response_out Output: immune response (can be NULL)
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_security_present_antigen(
    mesh_security_integration_t* integration,
    const mesh_exception_antigen_t* antigen,
    mesh_exception_response_t* response_out
);

/* ============================================================================
 * BBB Mesh Validation Hooks
 * ============================================================================ */

/**
 * @brief Validate mesh transaction through BBB
 *
 * WHAT: BBB security check for mesh transactions
 * WHY:  Prevent malicious transactions from entering mesh
 *
 * @param integration Security integration
 * @param tx Transaction to validate
 * @param threat_score_out Output: threat score [0,1]
 * @return NIMCP_SUCCESS if valid, error code if blocked
 */
nimcp_error_t mesh_security_validate_transaction(
    mesh_security_integration_t* integration,
    const mesh_transaction_t* tx,
    float* threat_score_out
);

/**
 * @brief Validate participant credentials through BBB
 *
 * @param integration Security integration
 * @param participant Participant ID
 * @param credential Credential to validate
 * @return NIMCP_SUCCESS if valid
 */
nimcp_error_t mesh_security_validate_credential(
    mesh_security_integration_t* integration,
    mesh_participant_id_t participant,
    const credential_t* credential
);

/**
 * @brief Check participant security status
 *
 * Combines BBB, MSP, and immune status checks
 *
 * @param integration Security integration
 * @param participant Participant ID
 * @param is_quarantined_out Output: quarantine status
 * @param threat_level_out Output: current threat level
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_security_check_participant(
    mesh_security_integration_t* integration,
    mesh_participant_id_t participant,
    bool* is_quarantined_out,
    float* threat_level_out
);

/* ============================================================================
 * Quarantine and Revocation API
 * ============================================================================ */

/**
 * @brief Quarantine participant through mesh
 *
 * WHAT: Initiate quarantine via MSP with mesh notification
 * WHY:  Isolate threats with coordinated response
 *
 * @param integration Security integration
 * @param participant Participant to quarantine
 * @param duration_ms Quarantine duration
 * @param reason Quarantine reason
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_security_quarantine(
    mesh_security_integration_t* integration,
    mesh_participant_id_t participant,
    uint64_t duration_ms,
    const char* reason
);

/**
 * @brief Release participant from quarantine
 *
 * @param integration Security integration
 * @param participant Participant to release
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_security_release_quarantine(
    mesh_security_integration_t* integration,
    mesh_participant_id_t participant
);

/**
 * @brief Revoke participant credentials
 *
 * WHAT: Permanent credential revocation
 * WHY:  Handle chronic/severe threats
 *
 * @param integration Security integration
 * @param participant Participant whose credentials to revoke
 * @param reason Revocation reason
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_security_revoke_credential(
    mesh_security_integration_t* integration,
    mesh_participant_id_t participant,
    const char* reason
);

/* ============================================================================
 * Immune System Response API
 * ============================================================================ */

/**
 * @brief Handle immune response action
 *
 * WHAT: Execute immune system response through mesh
 * WHY:  Coordinate immune actions with MSP and channels
 *
 * @param integration Security integration
 * @param participant Target participant
 * @param action Immune action to take
 * @param inflammation_level Current inflammation level
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_security_handle_immune_response(
    mesh_security_integration_t* integration,
    mesh_participant_id_t participant,
    mesh_immune_action_t action,
    float inflammation_level
);

/**
 * @brief Notify recovery completion
 *
 * @param integration Security integration
 * @param participant Recovered participant
 * @param success Whether recovery succeeded
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_security_notify_recovery(
    mesh_security_integration_t* integration,
    mesh_participant_id_t participant,
    bool success
);

/* ============================================================================
 * Security Broadcast API
 * ============================================================================ */

/**
 * @brief Broadcast security event to mesh
 *
 * WHAT: Send security event via security channel
 * WHY:  Notify all participants of security state changes
 *
 * @param integration Security integration
 * @param event Event to broadcast
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_security_broadcast_event(
    mesh_security_integration_t* integration,
    const mesh_security_event_t* event
);

/**
 * @brief Broadcast quarantine notification
 *
 * @param integration Security integration
 * @param participant Quarantined participant
 * @param duration_ms Quarantine duration
 * @param reason Quarantine reason
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_security_broadcast_quarantine(
    mesh_security_integration_t* integration,
    mesh_participant_id_t participant,
    uint64_t duration_ms,
    const char* reason
);

/**
 * @brief Broadcast revocation notification
 *
 * @param integration Security integration
 * @param participant Revoked participant
 * @param reason Revocation reason
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_security_broadcast_revocation(
    mesh_security_integration_t* integration,
    mesh_participant_id_t participant,
    const char* reason
);

/* ============================================================================
 * Callback Registration API
 * ============================================================================ */

/**
 * @brief Set security event callback
 *
 * @param integration Security integration
 * @param callback Event callback
 * @param user_data User context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_security_set_event_callback(
    mesh_security_integration_t* integration,
    mesh_security_event_cb_t callback,
    void* user_data
);

/**
 * @brief Set quarantine decision callback
 *
 * @param integration Security integration
 * @param callback Quarantine callback
 * @param user_data User context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_security_set_quarantine_callback(
    mesh_security_integration_t* integration,
    mesh_security_quarantine_cb_t callback,
    void* user_data
);

/**
 * @brief Set BBB validation callback
 *
 * @param integration Security integration
 * @param callback Validation callback
 * @param user_data User context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_security_set_bbb_validate_callback(
    mesh_security_integration_t* integration,
    mesh_security_bbb_validate_cb_t callback,
    void* user_data
);

/* ============================================================================
 * Update and Statistics API
 * ============================================================================ */

/**
 * @brief Update security integration
 *
 * WHAT: Periodic update for timeouts and state checks
 * WHY:  Handle quarantine expirations and status updates
 *
 * @param integration Security integration
 * @param delta_ms Time since last update
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_security_update(
    mesh_security_integration_t* integration,
    uint64_t delta_ms
);

/**
 * @brief Get security statistics
 *
 * @param integration Security integration
 * @param stats Output: statistics
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_security_get_stats(
    const mesh_security_integration_t* integration,
    mesh_security_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param integration Security integration
 */
void mesh_security_reset_stats(mesh_security_integration_t* integration);

/**
 * @brief Print security status
 *
 * @param integration Security integration
 */
void mesh_security_print_status(const mesh_security_integration_t* integration);

/* ============================================================================
 * Convenience Macros
 * ============================================================================ */

/**
 * @brief Route exception with automatic file/line
 */
#define MESH_SECURITY_ROUTE_EXCEPTION(integration, code, msg, module, resp) \
    mesh_security_route_exception( \
        (integration), (code), (msg), (module), \
        __FILE__, __LINE__, (resp))

/**
 * @brief Check if participant can operate
 */
#define MESH_SECURITY_CAN_OPERATE(integration, participant) \
    ({ \
        bool _quarantined = false; \
        float _threat = 0.0f; \
        mesh_security_check_participant((integration), (participant), &_quarantined, &_threat); \
        !_quarantined && _threat < 0.8f; \
    })

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_SECURITY_INTEGRATION_H */
