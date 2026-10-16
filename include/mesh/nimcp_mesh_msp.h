/**
 * @file nimcp_mesh_msp.h
 * @brief Mesh Network Membership Service Provider (MSP)
 *
 * WHAT: Identity management, authentication, and authorization for mesh participants
 * WHY:  Secure access control with BBB and Immune system integration
 * HOW:  Credential issuance, verification, quarantine, and revocation
 *
 * MSP ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                    MEMBERSHIP SERVICE PROVIDER                          │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐        │
 * │  │  IDENTITY       │  │  ACCESS         │  │  CREDENTIAL     │        │
 * │  │  Verification   │  │  Policies       │  │  Management     │        │
 * │  │  (BBB Gateway)  │  │  (Per Channel)  │  │  (Issue/Revoke) │        │
 * │  └────────┬────────┘  └────────┬────────┘  └────────┬────────┘        │
 * │           │                    │                    │                  │
 * │           ▼                    ▼                    ▼                  │
 * │  ┌─────────────────────────────────────────────────────────────┐      │
 * │  │                    SECURITY ENGINE                          │      │
 * │  │  - Signature verification                                   │      │
 * │  │  - Permission checking                                      │      │
 * │  │  - Audit logging                                            │      │
 * │  └─────────────────────────────────────────────────────────────┘      │
 * │           │                                                            │
 * │           ▼                                                            │
 * │  ┌─────────────────────────────────────────────────────────────┐      │
 * │  │               IMMUNE INTEGRATION                            │      │
 * │  │  - Quarantine on threat                                     │      │
 * │  │  - Revocation on chronic failure                            │      │
 * │  │  - Recovery restoration                                      │      │
 * │  └─────────────────────────────────────────────────────────────┘      │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * CREDENTIAL LIFECYCLE:
 * NONE -> PENDING -> VALID -> SUSPENDED/EXPIRED -> REVOKED
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_MSP_H
#define NIMCP_MESH_MSP_H

#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_transaction.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct mesh_msp mesh_msp_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum credentials managed by MSP */
#define MESH_MSP_MAX_CREDENTIALS            1024

/** @brief Maximum channel memberships per participant */
#define MESH_MSP_MAX_MEMBERSHIPS            16

/** @brief Default quarantine duration (ms) */
#define MESH_MSP_DEFAULT_QUARANTINE_MS      30000

/** @brief Default credential expiration (ms) */
#define MESH_MSP_DEFAULT_EXPIRATION_MS      3600000  /* 1 hour */

/** @brief Maximum privilege level */
#define MESH_MSP_MAX_PRIVILEGE_LEVEL        10

/* ============================================================================
 * Capability Flags
 * ============================================================================ */

/** @brief Capability: Read channel state */
#define MESH_CAP_READ                       (1ULL << 0)

/** @brief Capability: Write to channel state */
#define MESH_CAP_WRITE                      (1ULL << 1)

/** @brief Capability: Propose transactions */
#define MESH_CAP_PROPOSE                    (1ULL << 2)

/** @brief Capability: Endorse transactions */
#define MESH_CAP_ENDORSE                    (1ULL << 3)

/** @brief Capability: Order transactions */
#define MESH_CAP_ORDER                      (1ULL << 4)

/** @brief Capability: Validate transactions */
#define MESH_CAP_VALIDATE                   (1ULL << 5)

/** @brief Capability: Cross-channel operations */
#define MESH_CAP_CROSS_CHANNEL              (1ULL << 6)

/** @brief Capability: Emergency override */
#define MESH_CAP_EMERGENCY                  (1ULL << 7)

/** @brief Capability: Admin operations */
#define MESH_CAP_ADMIN                      (1ULL << 8)

/** @brief All capabilities */
#define MESH_CAP_ALL                        0xFFFFFFFFFFFFFFFFULL

/* ============================================================================
 * Access Policy Types
 * ============================================================================ */

/**
 * @brief Access policy type
 */
typedef enum msp_policy_type {
    MSP_POLICY_NONE = 0,                /**< No policy */
    MSP_POLICY_ALLOW_ALL,               /**< Allow all (testing only) */
    MSP_POLICY_DENY_ALL,                /**< Deny all */
    MSP_POLICY_PRIVILEGE_LEVEL,         /**< Based on privilege level */
    MSP_POLICY_CAPABILITY_CHECK,        /**< Based on capabilities */
    MSP_POLICY_CHANNEL_MEMBERSHIP,      /**< Based on channel membership */
    MSP_POLICY_CUSTOM                   /**< Custom policy callback */
} msp_policy_type_t;

/**
 * @brief Access policy
 */
typedef struct msp_access_policy {
    const char* policy_name;            /**< Policy name */
    msp_policy_type_t type;             /**< Policy type */
    uint32_t min_privilege_level;       /**< Minimum privilege (if PRIVILEGE_LEVEL) */
    uint64_t required_capabilities;     /**< Required caps (if CAPABILITY_CHECK) */
    mesh_channel_id_t* required_channels; /**< Required channels (if CHANNEL_MEMBERSHIP) */
    size_t required_channel_count;      /**< Number of required channels */
} msp_access_policy_t;

/* ============================================================================
 * MSP Configuration
 * ============================================================================ */

/**
 * @brief MSP configuration
 */
typedef struct mesh_msp_config {
    const char* msp_name;               /**< MSP name */

    /* Credential settings */
    uint64_t credential_expiration_ms;  /**< Credential expiration */
    uint32_t default_privilege_level;   /**< Default privilege level */
    uint64_t default_capabilities;      /**< Default capabilities */

    /* Security settings */
    bool require_signature;             /**< Require signed requests */
    bool enable_quarantine;             /**< Enable quarantine on threat */
    uint64_t quarantine_duration_ms;    /**< Quarantine duration */

    /* BBB integration */
    void* bbb_handle;                   /**< BBB handle (optional) */

    /* Immune integration */
    void* immune_handle;                /**< Immune system handle (optional) */

    /* Logging */
    bool enable_logging;                /**< Enable MSP logging */
    bool enable_audit;                  /**< Enable audit trail */
} mesh_msp_config_t;

/**
 * @brief MSP statistics
 */
typedef struct mesh_msp_stats {
    uint64_t credentials_issued;        /**< Total credentials issued */
    uint64_t credentials_revoked;       /**< Total revocations */
    uint64_t credentials_suspended;     /**< Currently suspended */
    uint64_t credentials_active;        /**< Currently active */

    uint64_t auth_requests;             /**< Total auth requests */
    uint64_t auth_granted;              /**< Auth requests granted */
    uint64_t auth_denied;               /**< Auth requests denied */

    uint64_t quarantine_events;         /**< Total quarantine events */
    uint64_t recovery_events;           /**< Total recovery events */

    uint64_t threat_events;             /**< Threat events from immune */
} mesh_msp_stats_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Custom policy evaluation callback
 */
typedef bool (*msp_policy_callback_t)(
    mesh_msp_t* msp,
    const credential_t* credential,
    const mesh_transaction_t* tx,
    void* ctx
);

/**
 * @brief Immune event callback
 */
typedef void (*msp_immune_callback_t)(
    mesh_msp_t* msp,
    mesh_participant_id_t participant,
    int event_type,
    void* ctx
);

/* ============================================================================
 * MSP Lifecycle
 * ============================================================================ */

/**
 * @brief Get default MSP configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_msp_default_config(mesh_msp_config_t* config);

/**
 * @brief Create MSP
 *
 * WHAT: Allocate and initialize MSP
 * WHY:  Central authority for identity and access management
 *
 * @param config Configuration (NULL for defaults)
 * @param registry Participant registry
 * @return MSP handle or NULL on failure
 */
mesh_msp_t* mesh_msp_create(
    const mesh_msp_config_t* config,
    mesh_participant_registry_t* registry
);

/**
 * @brief Destroy MSP
 *
 * @param msp MSP to destroy (NULL-safe)
 */
void mesh_msp_destroy(mesh_msp_t* msp);

/**
 * @brief Get MSP name
 *
 * @param msp MSP handle
 * @return MSP name or NULL
 */
const char* mesh_msp_get_name(const mesh_msp_t* msp);

/* ============================================================================
 * BBB and Immune System Connection API
 * ============================================================================ */

/* Forward declarations for BBB and Immune types */
#ifndef BBB_SYSTEM_T_DEFINED
#define BBB_SYSTEM_T_DEFINED
typedef struct bbb_system_struct* bbb_system_t;
#endif
typedef struct brain_immune_system brain_immune_system_t;

/**
 * @brief Connect MSP to BBB instance
 *
 * WHAT: Wire MSP to BBB for credential validation
 * WHY:  Enable BBB-based transaction and credential validation
 *
 * @param msp MSP handle
 * @param bbb BBB system handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_msp_connect_bbb(
    mesh_msp_t* msp,
    bbb_system_t bbb
);

/**
 * @brief Connect MSP to immune system instance
 *
 * WHAT: Wire MSP to immune system for quarantine/revocation routing
 * WHY:  Enable immune-driven security responses
 *
 * @param msp MSP handle
 * @param immune Immune system handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_msp_connect_immune(
    mesh_msp_t* msp,
    brain_immune_system_t* immune
);

/**
 * @brief Get BBB handle from MSP
 *
 * @param msp MSP handle
 * @return BBB handle or NULL if not connected
 */
bbb_system_t mesh_msp_get_bbb(const mesh_msp_t* msp);

/**
 * @brief Get immune system handle from MSP
 *
 * @param msp MSP handle
 * @return Immune system handle or NULL if not connected
 */
brain_immune_system_t* mesh_msp_get_immune(const mesh_msp_t* msp);

/* ============================================================================
 * Credential Management API
 * ============================================================================ */

/**
 * @brief Issue credential to participant
 *
 * WHAT: Create and assign credential
 * WHY:  Participants need credentials to operate in mesh
 *
 * @param msp MSP handle
 * @param participant_id Participant to credential
 * @param privilege_level Privilege level (0-10)
 * @param capabilities Capability bitmask
 * @param credential_out Output: issued credential
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_msp_issue_credential(
    mesh_msp_t* msp,
    mesh_participant_id_t participant_id,
    uint32_t privilege_level,
    uint64_t capabilities,
    credential_t* credential_out
);

/**
 * @brief Revoke credential
 *
 * WHAT: Permanently revoke credential
 * WHY:  Remove access for malicious or departed participants
 *
 * @param msp MSP handle
 * @param participant_id Participant whose credential to revoke
 * @param reason Revocation reason
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_msp_revoke_credential(
    mesh_msp_t* msp,
    mesh_participant_id_t participant_id,
    const char* reason
);

/**
 * @brief Suspend credential
 *
 * WHAT: Temporarily suspend credential
 * WHY:  Investigation or temporary restriction
 *
 * @param msp MSP handle
 * @param participant_id Participant to suspend
 * @param duration_ms Suspension duration
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_msp_suspend_credential(
    mesh_msp_t* msp,
    mesh_participant_id_t participant_id,
    uint64_t duration_ms
);

/**
 * @brief Restore suspended credential
 *
 * @param msp MSP handle
 * @param participant_id Participant to restore
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_msp_restore_credential(
    mesh_msp_t* msp,
    mesh_participant_id_t participant_id
);

/**
 * @brief Get credential for participant
 *
 * @param msp MSP handle
 * @param participant_id Participant ID
 * @return Credential or NULL if not found
 */
const credential_t* mesh_msp_get_credential(
    const mesh_msp_t* msp,
    mesh_participant_id_t participant_id
);

/**
 * @brief Check if credential is valid
 *
 * @param msp MSP handle
 * @param participant_id Participant ID
 * @return true if credential is valid
 */
bool mesh_msp_is_credential_valid(
    const mesh_msp_t* msp,
    mesh_participant_id_t participant_id
);

/**
 * @brief Refresh credential expiration
 *
 * @param msp MSP handle
 * @param participant_id Participant ID
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_msp_refresh_credential(
    mesh_msp_t* msp,
    mesh_participant_id_t participant_id
);

/* ============================================================================
 * Channel Membership API
 * ============================================================================ */

/**
 * @brief Grant channel membership
 *
 * @param msp MSP handle
 * @param participant_id Participant ID
 * @param channel_id Channel to grant access
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_msp_grant_channel_membership(
    mesh_msp_t* msp,
    mesh_participant_id_t participant_id,
    mesh_channel_id_t channel_id
);

/**
 * @brief Revoke channel membership
 *
 * @param msp MSP handle
 * @param participant_id Participant ID
 * @param channel_id Channel to revoke access
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_msp_revoke_channel_membership(
    mesh_msp_t* msp,
    mesh_participant_id_t participant_id,
    mesh_channel_id_t channel_id
);

/**
 * @brief Check channel membership
 *
 * @param msp MSP handle
 * @param participant_id Participant ID
 * @param channel_id Channel ID
 * @return true if participant is member of channel
 */
bool mesh_msp_has_channel_membership(
    const mesh_msp_t* msp,
    mesh_participant_id_t participant_id,
    mesh_channel_id_t channel_id
);

/**
 * @brief Get all channel memberships
 *
 * @param msp MSP handle
 * @param participant_id Participant ID
 * @param channels_out Output array (caller allocates)
 * @param max_channels Max channels
 * @param count_out Output: actual count
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_msp_get_channel_memberships(
    const mesh_msp_t* msp,
    mesh_participant_id_t participant_id,
    mesh_channel_id_t* channels_out,
    size_t max_channels,
    size_t* count_out
);

/* ============================================================================
 * Authentication API
 * ============================================================================ */

/**
 * @brief Authenticate participant
 *
 * WHAT: Verify participant identity
 * WHY:  Ensure only authorized participants operate
 *
 * @param msp MSP handle
 * @param participant_id Participant to authenticate
 * @param signature Signature to verify (optional)
 * @param signature_len Signature length
 * @return NIMCP_SUCCESS if authenticated
 */
nimcp_error_t mesh_msp_authenticate(
    mesh_msp_t* msp,
    mesh_participant_id_t participant_id,
    const uint8_t* signature,
    size_t signature_len
);

/**
 * @brief Validate transaction authorization
 *
 * WHAT: Check if transaction is authorized
 * WHY:  Enforce access control on transactions
 *
 * @param msp MSP handle
 * @param tx Transaction to validate
 * @return NIMCP_SUCCESS if authorized
 */
nimcp_error_t mesh_msp_validate_transaction(
    mesh_msp_t* msp,
    const mesh_transaction_t* tx
);

/**
 * @brief Check capability
 *
 * @param msp MSP handle
 * @param participant_id Participant ID
 * @param capability Capability to check
 * @return true if participant has capability
 */
bool mesh_msp_check_capability(
    const mesh_msp_t* msp,
    mesh_participant_id_t participant_id,
    uint64_t capability
);

/**
 * @brief Check privilege level
 *
 * @param msp MSP handle
 * @param participant_id Participant ID
 * @param min_level Minimum required level
 * @return true if participant meets level
 */
bool mesh_msp_check_privilege(
    const mesh_msp_t* msp,
    mesh_participant_id_t participant_id,
    uint32_t min_level
);

/* ============================================================================
 * Policy API
 * ============================================================================ */

/**
 * @brief Add access policy
 *
 * @param msp MSP handle
 * @param policy Policy to add
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_msp_add_policy(
    mesh_msp_t* msp,
    const msp_access_policy_t* policy
);

/**
 * @brief Remove access policy
 *
 * @param msp MSP handle
 * @param policy_name Policy name to remove
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_msp_remove_policy(
    mesh_msp_t* msp,
    const char* policy_name
);

/**
 * @brief Evaluate policy
 *
 * @param msp MSP handle
 * @param policy_name Policy to evaluate
 * @param participant_id Participant ID
 * @param tx Transaction (optional)
 * @return true if policy allows
 */
bool mesh_msp_evaluate_policy(
    const mesh_msp_t* msp,
    const char* policy_name,
    mesh_participant_id_t participant_id,
    const mesh_transaction_t* tx
);

/**
 * @brief Set custom policy callback
 *
 * @param msp MSP handle
 * @param policy_name Policy name
 * @param callback Custom callback
 * @param ctx Callback context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_msp_set_policy_callback(
    mesh_msp_t* msp,
    const char* policy_name,
    msp_policy_callback_t callback,
    void* ctx
);

/* ============================================================================
 * Quarantine API (Immune Integration)
 * ============================================================================ */

/**
 * @brief Quarantine participant
 *
 * WHAT: Temporarily isolate participant
 * WHY:  Response to immune system threat detection
 *
 * @param msp MSP handle
 * @param participant_id Participant to quarantine
 * @param duration_ms Quarantine duration
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_msp_quarantine(
    mesh_msp_t* msp,
    mesh_participant_id_t participant_id,
    uint64_t duration_ms
);

/**
 * @brief Release from quarantine
 *
 * @param msp MSP handle
 * @param participant_id Participant to release
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_msp_release_quarantine(
    mesh_msp_t* msp,
    mesh_participant_id_t participant_id
);

/**
 * @brief Check if quarantined
 *
 * @param msp MSP handle
 * @param participant_id Participant ID
 * @return true if quarantined
 */
bool mesh_msp_is_quarantined(
    const mesh_msp_t* msp,
    mesh_participant_id_t participant_id
);

/**
 * @brief Set immune event callback
 *
 * @param msp MSP handle
 * @param callback Callback function
 * @param ctx Callback context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_msp_set_immune_callback(
    mesh_msp_t* msp,
    msp_immune_callback_t callback,
    void* ctx
);

/**
 * @brief Handle immune event
 *
 * WHAT: Process event from immune system
 * WHY:  Auto-quarantine or revoke on threats
 *
 * @param msp MSP handle
 * @param participant_id Source participant
 * @param event_type Event type
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_msp_handle_immune_event(
    mesh_msp_t* msp,
    mesh_participant_id_t participant_id,
    int event_type
);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update MSP
 *
 * WHAT: Perform periodic update
 * WHY:  Check expirations, process quarantines
 *
 * @param msp MSP handle
 * @param delta_ms Time since last update
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_msp_update(
    mesh_msp_t* msp,
    uint64_t delta_ms
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get MSP statistics
 *
 * @param msp MSP handle
 * @param stats Output: statistics
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_msp_get_stats(
    const mesh_msp_t* msp,
    mesh_msp_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param msp MSP handle
 */
void mesh_msp_reset_stats(mesh_msp_t* msp);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Print MSP status
 *
 * @param msp MSP handle
 */
void mesh_msp_print_status(const mesh_msp_t* msp);

/**
 * @brief Print credential info
 *
 * @param credential Credential to print
 */
void mesh_msp_print_credential(const credential_t* credential);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_MSP_H */
