/**
 * @file nimcp_mesh_endorsement.h
 * @brief Brain-Inspired Endorsement Collection
 *
 * WHAT: Endorsement collection using pattern-based self-selection
 * WHY:  The brain doesn't use static "policies" - modules self-select
 *       based on pattern recognition and learned associations
 * HOW:  Pattern routing determines endorsers, this module collects signatures
 *
 * DESIGN PRINCIPLE:
 * ```
 * OLD WAY (Static Policies):
 *   Transaction Type → Policy Lookup → Fixed Endorser List
 *   "motor_command" → "motor_cortex AND cerebellum" → [M1, CB]
 *
 * NEW WAY (Brain-Inspired):
 *   Pattern Vector → Self-Selection → Dynamic Endorser Set
 *   [movement intent pattern] → modules with matching receptive fields activate
 *   High activation = endorser, very high = required, negative = veto
 * ```
 *
 * ENDORSER SELECTION FLOW:
 * ```
 * ┌─────────────┐     ┌─────────────────┐     ┌─────────────────┐
 * │ Transaction │────►│  Pattern Router │────►│ Self-Selection  │
 * │  + Pattern  │     │  (Activation)   │     │ (Endorser Set)  │
 * └─────────────┘     └─────────────────┘     └────────┬────────┘
 *                                                       │
 *        ┌──────────────────────────────────────────────┘
 *        ▼
 * ┌──────────────┐     ┌─────────────────┐     ┌─────────────────┐
 * │   Request    │────►│    Collect      │────►│    Validate     │
 * │ Endorsements │     │  Signatures     │     │    Quorum       │
 * └──────────────┘     └─────────────────┘     └────────┬────────┘
 *                                                       │
 *        ┌──────────────────────────────────────────────┘
 *        ▼
 * ┌─────────────────┐
 * │  Quorum Met?    │───► [YES] ───► Submit to Ordering
 * │                 │───► [NO]  ───► Wait/Timeout
 * └─────────────────┘
 * ```
 *
 * ACTIVATION-TO-ROLE MAPPING:
 *   activation > 0.9  → REQUIRED (critical module for this pattern)
 *   activation > 0.7  → HIGHLY_PREFERRED (should endorse)
 *   activation > 0.5  → OPTIONAL (can endorse)
 *   activation < -0.5 → VETO (can block)
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 2.0.0
 */

#ifndef NIMCP_MESH_ENDORSEMENT_H
#define NIMCP_MESH_ENDORSEMENT_H

#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_pattern_routing.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct mesh_endorsement_collector mesh_endorsement_collector_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum endorsers per transaction */
#define MESH_MAX_ENDORSERS                  16

/** @brief Default endorsement timeout (ms) */
#define MESH_DEFAULT_ENDORSEMENT_TIMEOUT    100

/** @brief Activation threshold for REQUIRED role */
#define MESH_REQUIRED_THRESHOLD             0.9f

/** @brief Activation threshold for PREFERRED role */
#define MESH_PREFERRED_THRESHOLD            0.7f

/** @brief Activation threshold for OPTIONAL role */
#define MESH_OPTIONAL_THRESHOLD             0.5f

/** @brief Negative activation threshold for VETO capability */
#define MESH_VETO_THRESHOLD                 -0.5f

/* ============================================================================
 * Endorser Role Types
 * ============================================================================ */

/**
 * @brief Endorser role (derived from activation level)
 *
 * BRAIN ANALOGY:
 *   REQUIRED = Module is essential for this pattern (like M1 for movement)
 *   PREFERRED = Module strongly activated (like cerebellum for coordination)
 *   OPTIONAL = Module somewhat relevant (like visual for visually-guided movement)
 *   VETO = Module can block (like amygdala for threat detection)
 */
typedef enum endorser_role {
    ENDORSER_ROLE_REQUIRED = 0,     /**< Must endorse (activation > 0.9) */
    ENDORSER_ROLE_PREFERRED,        /**< Should endorse (activation > 0.7) */
    ENDORSER_ROLE_OPTIONAL,         /**< Can endorse (activation > 0.5) */
    ENDORSER_ROLE_VETO              /**< Can block (activation < -0.5) */
} endorser_role_t;

/**
 * @brief Selected endorser with role
 */
typedef struct selected_endorser {
    mesh_participant_id_t id;       /**< Endorser participant ID */
    endorser_role_t role;           /**< Role from activation level */
    float activation;               /**< Activation level */
    float similarity;               /**< Pattern similarity */
} selected_endorser_t;

/**
 * @brief Endorser set (from pattern routing)
 */
typedef struct endorser_set {
    selected_endorser_t endorsers[MESH_MAX_ENDORSERS];
    size_t count;
    size_t required_count;          /**< Number of REQUIRED endorsers */
    size_t preferred_count;         /**< Number of PREFERRED endorsers */
    size_t optional_count;          /**< Number of OPTIONAL endorsers */
    size_t veto_count;              /**< Number of VETO endorsers */
} endorser_set_t;

/* ============================================================================
 * Collection State
 * ============================================================================ */

/**
 * @brief Collection state for a single transaction
 */
typedef struct endorsement_collection {
    mesh_tx_id_t tx_id;                 /**< Transaction being endorsed */
    mesh_pattern_t pattern;             /**< Pattern used for selection */

    endorser_set_t selected;            /**< Selected endorsers (from pattern) */
    endorsement_set_t received;         /**< Received endorsements */

    uint64_t start_time_ns;             /**< When collection started */
    uint64_t deadline_ns;               /**< Collection deadline */

    bool quorum_met;                    /**< Quorum reached */
    bool collection_complete;           /**< Collection finished */
    bool timed_out;                     /**< Collection timed out */
    bool vetoed;                        /**< Vetoed by VETO endorser */
} endorsement_collection_t;

/* ============================================================================
 * Quorum Configuration
 * ============================================================================ */

/**
 * @brief Quorum requirements (brain-inspired thresholds)
 */
typedef struct endorsement_quorum {
    float required_ratio;               /**< Ratio of REQUIRED that must endorse */
    float preferred_ratio;              /**< Ratio of PREFERRED that must endorse */
    size_t min_endorsers;               /**< Minimum total endorsers */
    bool allow_veto;                    /**< Allow VETO to block */
} endorsement_quorum_t;

/**
 * @brief Collector configuration
 */
typedef struct mesh_endorsement_collector_config {
    size_t max_concurrent;              /**< Max concurrent collections */
    float default_timeout_ms;           /**< Default collection timeout */
    endorsement_quorum_t default_quorum; /**< Default quorum requirements */
    bool enable_logging;                /**< Enable logging */
} mesh_endorsement_collector_config_t;

/* ============================================================================
 * Collector Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default collector configuration
 */
nimcp_error_t mesh_endorsement_collector_default_config(
    mesh_endorsement_collector_config_t* config
);

/**
 * @brief Create endorsement collector
 *
 * @param config Configuration (NULL for defaults)
 * @param router Pattern router (for endorser selection)
 * @return Collector or NULL on failure
 */
mesh_endorsement_collector_t* mesh_endorsement_collector_create(
    const mesh_endorsement_collector_config_t* config,
    mesh_pattern_router_t* router
);

/**
 * @brief Destroy endorsement collector
 */
void mesh_endorsement_collector_destroy(mesh_endorsement_collector_t* collector);

/* ============================================================================
 * Endorser Selection API (Pattern-Based)
 * ============================================================================ */

/**
 * @brief Select endorsers for transaction pattern
 *
 * WHAT: Use pattern routing to determine who should endorse
 * WHY:  Brain-inspired self-selection replaces static policies
 *
 * @param collector Collector
 * @param pattern Transaction pattern
 * @param endorsers_out Output: selected endorsers
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_endorsement_select_endorsers(
    mesh_endorsement_collector_t* collector,
    const mesh_pattern_t* pattern,
    endorser_set_t* endorsers_out
);

/**
 * @brief Get endorser role from activation level
 *
 * Maps activation to role:
 *   > 0.9 → REQUIRED
 *   > 0.7 → PREFERRED
 *   > 0.5 → OPTIONAL
 *   < -0.5 → VETO
 */
endorser_role_t mesh_endorsement_role_from_activation(float activation);

/* ============================================================================
 * Collection API
 * ============================================================================ */

/**
 * @brief Start endorsement collection for transaction
 *
 * WHAT: Begin collecting endorsements using pattern-selected endorsers
 * WHY:  Transactions need endorsement before ordering
 *
 * @param collector Collector
 * @param tx Transaction to endorse
 * @param pattern Transaction pattern (for endorser selection)
 * @param quorum Quorum requirements (NULL for defaults)
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_endorsement_start_collection(
    mesh_endorsement_collector_t* collector,
    mesh_transaction_t* tx,
    const mesh_pattern_t* pattern,
    const endorsement_quorum_t* quorum
);

/**
 * @brief Add endorsement to collection
 */
nimcp_error_t mesh_endorsement_add(
    mesh_endorsement_collector_t* collector,
    const mesh_tx_id_t* tx_id,
    const mesh_endorsement_t* endorsement
);

/**
 * @brief Request endorsement from specific endorser
 */
nimcp_error_t mesh_endorsement_request(
    mesh_endorsement_collector_t* collector,
    const mesh_tx_id_t* tx_id,
    mesh_participant_id_t endorser_id
);

/**
 * @brief Request endorsements from all selected endorsers
 */
nimcp_error_t mesh_endorsement_request_all(
    mesh_endorsement_collector_t* collector,
    const mesh_tx_id_t* tx_id
);

/* ============================================================================
 * Collection Status API
 * ============================================================================ */

/**
 * @brief Check if collection is complete
 */
bool mesh_endorsement_is_complete(
    const mesh_endorsement_collector_t* collector,
    const mesh_tx_id_t* tx_id
);

/**
 * @brief Check if quorum is met
 */
bool mesh_endorsement_quorum_met(
    const mesh_endorsement_collector_t* collector,
    const mesh_tx_id_t* tx_id
);

/**
 * @brief Check if transaction was vetoed
 */
bool mesh_endorsement_is_vetoed(
    const mesh_endorsement_collector_t* collector,
    const mesh_tx_id_t* tx_id
);

/**
 * @brief Get collected endorsements
 */
const endorsement_set_t* mesh_endorsement_get_collected(
    const mesh_endorsement_collector_t* collector,
    const mesh_tx_id_t* tx_id
);

/**
 * @brief Get selected endorsers
 */
const endorser_set_t* mesh_endorsement_get_selected(
    const mesh_endorsement_collector_t* collector,
    const mesh_tx_id_t* tx_id
);

/**
 * @brief Cancel collection
 */
nimcp_error_t mesh_endorsement_cancel_collection(
    mesh_endorsement_collector_t* collector,
    const mesh_tx_id_t* tx_id
);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update collector (check timeouts, cleanup)
 */
nimcp_error_t mesh_endorsement_collector_update(
    mesh_endorsement_collector_t* collector,
    uint64_t delta_ms
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Create endorsement
 */
nimcp_error_t mesh_endorsement_create(
    mesh_participant_id_t endorser_id,
    endorsement_result_t result,
    mesh_endorsement_t* endorsement_out
);

/**
 * @brief Validate endorsement signature
 */
bool mesh_endorsement_verify_signature(
    const mesh_endorsement_t* endorsement,
    const mesh_transaction_t* tx,
    mesh_participant_registry_t* registry
);

/**
 * @brief Print endorser set
 */
void mesh_endorsement_print_selected(const endorser_set_t* selected);

/**
 * @brief Print collection status
 */
void mesh_endorsement_print_collection(
    const mesh_endorsement_collector_t* collector,
    const mesh_tx_id_t* tx_id
);

/**
 * @brief Initialize endorser set
 */
void endorser_set_init(endorser_set_t* set);

/**
 * @brief Get endorser role name
 */
const char* endorser_role_to_string(endorser_role_t role);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_ENDORSEMENT_H */
