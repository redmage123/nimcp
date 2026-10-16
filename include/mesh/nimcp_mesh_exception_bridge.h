/**
 * @file nimcp_mesh_exception_bridge.h
 * @brief Exception to Immune System Mesh Routing Bridge
 *
 * WHAT: Routes exceptions through mesh network to immune system
 * WHY:  Enable coordinated immune response to system errors via mesh
 * HOW:  Exception -> Antigen conversion, mesh transaction for immune routing
 *
 * FLOW:
 * ```
 * Exception Thrown
 *       │
 *       ▼
 * ┌─────────────────┐
 * │ Exception Bridge │
 * │  - Classify      │
 * │  - To Antigen    │
 * │  - Create Tx     │
 * └────────┬────────┘
 *          │
 *          ▼
 * ┌─────────────────┐
 * │   BBB Gateway   │ ◄── Validate transaction
 * └────────┬────────┘
 *          │
 *          ▼
 * ┌─────────────────┐
 * │ Immune System   │ ◄── Present antigen, generate response
 * │  - B Cells      │
 * │  - T Cells      │
 * │  - Cytokines    │
 * └────────┬────────┘
 *          │
 *          ▼
 * ┌─────────────────┐
 * │      MSP        │ ◄── Quarantine/revoke if needed
 * └─────────────────┘
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_EXCEPTION_BRIDGE_H
#define NIMCP_MESH_EXCEPTION_BRIDGE_H

#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_transaction.h"
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

typedef struct mesh_bootstrap mesh_bootstrap_t;
typedef struct mesh_exception_bridge mesh_exception_bridge_t;
typedef struct brain_immune_system brain_immune_system_t;
typedef struct blood_brain_barrier blood_brain_barrier_t;
typedef struct nimcp_exception nimcp_exception_t;

/* ============================================================================
 * Exception Classification
 * ============================================================================ */

/**
 * @brief Exception severity levels for immune response
 */
typedef enum mesh_exception_severity {
    MESH_EXC_SEVERITY_TRACE = 0,          /**< Debug/trace level - no immune action */
    MESH_EXC_SEVERITY_INFO = 1,           /**< Informational - log only */
    MESH_EXC_SEVERITY_WARNING = 2,        /**< Warning - mild immune response */
    MESH_EXC_SEVERITY_ERROR = 3,          /**< Error - moderate immune response */
    MESH_EXC_SEVERITY_SEVERE = 4,         /**< Severe - strong immune response */
    MESH_EXC_SEVERITY_CRITICAL = 5,       /**< Critical - emergency immune response */
} mesh_exception_severity_t;

/**
 * @brief Exception category for antigen classification
 */
typedef enum mesh_exception_category {
    MESH_EXC_CAT_MEMORY = 0,              /**< Memory allocation/corruption */
    MESH_EXC_CAT_SECURITY,                /**< Security violation */
    MESH_EXC_CAT_NETWORK,                 /**< Network/communication error */
    MESH_EXC_CAT_RESOURCE,                /**< Resource exhaustion */
    MESH_EXC_CAT_LOGIC,                   /**< Logic/assertion failure */
    MESH_EXC_CAT_TIMING,                  /**< Timeout/deadline miss */
    MESH_EXC_CAT_DATA,                    /**< Data corruption/validation */
    MESH_EXC_CAT_SYSTEM,                  /**< System-level error */
    MESH_EXC_CAT_GPU,                     /**< GPU-specific error */
    MESH_EXC_CAT_UNKNOWN,                 /**< Unknown/unclassified */
} mesh_exception_category_t;

/* ============================================================================
 * Antigen Structure (for immune presentation)
 * ============================================================================ */

/**
 * @brief Antigen derived from exception
 */
typedef struct mesh_exception_antigen {
    uint32_t antigen_id;                  /**< Unique antigen ID */
    mesh_exception_category_t category;   /**< Exception category */
    mesh_exception_severity_t severity;   /**< Severity level */

    mesh_participant_id_t source_module;  /**< Module that raised exception */
    mesh_channel_id_t source_channel;     /**< Channel where exception occurred */

    nimcp_error_t error_code;             /**< Original error code */
    char error_message[256];              /**< Error message */
    char source_file[128];                /**< Source file location */
    uint32_t source_line;                 /**< Source line number */

    uint64_t timestamp_ns;                /**< When exception occurred */
    uint32_t occurrence_count;            /**< Repeated occurrence count */

    /* Pattern for pattern-based routing */
    float pattern[8];                     /**< Exception pattern signature */

} mesh_exception_antigen_t;

/* ============================================================================
 * Immune Response Structure
 * ============================================================================ */

/**
 * @brief Immune response action
 */
typedef enum mesh_immune_action {
    MESH_IMMUNE_ACTION_NONE = 0,          /**< No action needed */
    MESH_IMMUNE_ACTION_LOG,               /**< Log and monitor */
    MESH_IMMUNE_ACTION_WARN,              /**< Warning issued */
    MESH_IMMUNE_ACTION_QUARANTINE,        /**< Quarantine module */
    MESH_IMMUNE_ACTION_REVOKE,            /**< Revoke credentials */
    MESH_IMMUNE_ACTION_REPAIR,            /**< Trigger repair mechanism */
    MESH_IMMUNE_ACTION_RESTART,           /**< Restart module */
    MESH_IMMUNE_ACTION_SHUTDOWN,          /**< Emergency shutdown */
} mesh_immune_action_t;

/**
 * @brief Response from immune system
 */
typedef struct mesh_exception_response {
    mesh_immune_action_t primary_action;  /**< Primary recommended action */
    mesh_immune_action_t fallback_action; /**< Fallback if primary fails */

    uint32_t quarantine_duration_ms;      /**< Quarantine duration if applicable */
    bool credential_revoked;              /**< Whether credentials were revoked */

    float inflammation_level;             /**< Resulting inflammation [0-1] */
    float threat_score;                   /**< Computed threat score [0-1] */

    char explanation[256];                /**< Human-readable explanation */

} mesh_exception_response_t;

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

/**
 * @brief Exception bridge configuration
 */
typedef struct mesh_exception_bridge_config {
    /* Severity thresholds */
    mesh_exception_severity_t min_report_severity;  /**< Minimum severity to report */
    mesh_exception_severity_t quarantine_threshold; /**< Severity for auto-quarantine */

    /* Timing */
    uint32_t debounce_ms;                 /**< Debounce repeated exceptions */
    uint32_t escalation_window_ms;        /**< Window for severity escalation */
    uint32_t max_per_window;              /**< Max exceptions before escalation */

    /* Actions */
    bool enable_auto_quarantine;          /**< Auto-quarantine on threshold */
    bool enable_bbb_validation;           /**< Validate through BBB */
    bool route_through_mesh;              /**< Route via mesh transactions */

    /* Logging */
    bool verbose_logging;

} mesh_exception_bridge_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct mesh_exception_bridge_stats {
    uint64_t exceptions_received;         /**< Total exceptions received */
    uint64_t antigens_created;            /**< Antigens created */
    uint64_t mesh_transactions_sent;      /**< Transactions sent to mesh */
    uint64_t bbb_validations;             /**< BBB validations performed */
    uint64_t quarantine_actions;          /**< Quarantine actions taken */
    uint64_t revoke_actions;              /**< Revoke actions taken */
    uint64_t debounced_exceptions;        /**< Exceptions debounced */
    uint64_t escalations;                 /**< Severity escalations */

    /* Per-category counts */
    uint64_t category_counts[10];         /**< Counts per category */

    /* Per-severity counts */
    uint64_t severity_counts[6];          /**< Counts per severity */

} mesh_exception_bridge_stats_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default bridge configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_exception_bridge_default_config(
    mesh_exception_bridge_config_t* config
);

/**
 * @brief Create exception bridge
 *
 * @param bootstrap Mesh bootstrap handle
 * @param immune Immune system handle
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
mesh_exception_bridge_t* mesh_exception_bridge_create(
    mesh_bootstrap_t* bootstrap,
    brain_immune_system_t* immune,
    const mesh_exception_bridge_config_t* config
);

/**
 * @brief Destroy exception bridge
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void mesh_exception_bridge_destroy(mesh_exception_bridge_t* bridge);

/* ============================================================================
 * Exception Routing API
 * ============================================================================ */

/**
 * @brief Route an exception through mesh to immune system
 *
 * Main entry point for exception handling. Converts exception to antigen,
 * validates through BBB, routes via mesh, and returns immune response.
 *
 * @param bridge Exception bridge handle
 * @param exception Exception to route
 * @param response_out Output immune response
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_exception_bridge_route(
    mesh_exception_bridge_t* bridge,
    const nimcp_exception_t* exception,
    mesh_exception_response_t* response_out
);

/**
 * @brief Route exception with explicit source information
 *
 * @param bridge Exception bridge handle
 * @param error_code Error code
 * @param message Error message
 * @param source_module Source module ID
 * @param source_file Source file (can be __FILE__)
 * @param source_line Source line (can be __LINE__)
 * @param response_out Output immune response
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_exception_bridge_route_error(
    mesh_exception_bridge_t* bridge,
    nimcp_error_t error_code,
    const char* message,
    mesh_participant_id_t source_module,
    const char* source_file,
    uint32_t source_line,
    mesh_exception_response_t* response_out
);

/* ============================================================================
 * Antigen API
 * ============================================================================ */

/**
 * @brief Create antigen from exception
 *
 * @param bridge Exception bridge handle
 * @param exception Exception to convert
 * @param source_module Source module ID
 * @param antigen_out Output antigen
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_exception_bridge_create_antigen(
    mesh_exception_bridge_t* bridge,
    const nimcp_exception_t* exception,
    mesh_participant_id_t source_module,
    mesh_exception_antigen_t* antigen_out
);

/**
 * @brief Classify error code into category and severity
 *
 * @param error_code Error code to classify
 * @param category_out Output category
 * @param severity_out Output severity
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_exception_bridge_classify(
    nimcp_error_t error_code,
    mesh_exception_category_t* category_out,
    mesh_exception_severity_t* severity_out
);

/* ============================================================================
 * BBB Integration
 * ============================================================================ */

/**
 * @brief Set BBB for exception validation
 *
 * @param bridge Exception bridge handle
 * @param bbb BBB handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_exception_bridge_set_bbb(
    mesh_exception_bridge_t* bridge,
    blood_brain_barrier_t* bbb
);

/**
 * @brief Validate antigen through BBB
 *
 * @param bridge Exception bridge handle
 * @param antigen Antigen to validate
 * @param threat_score_out Output threat score
 * @return NIMCP_SUCCESS if valid (not a false positive)
 */
nimcp_error_t mesh_exception_bridge_bbb_validate(
    mesh_exception_bridge_t* bridge,
    const mesh_exception_antigen_t* antigen,
    float* threat_score_out
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Exception bridge handle
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_exception_bridge_get_stats(
    const mesh_exception_bridge_t* bridge,
    mesh_exception_bridge_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Exception bridge handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_exception_bridge_reset_stats(
    mesh_exception_bridge_t* bridge
);

/* ============================================================================
 * Bootstrap Integration
 * ============================================================================ */

/**
 * @brief Get exception bridge from bootstrap
 *
 * @param bootstrap Mesh bootstrap handle
 * @return Exception bridge or NULL
 */
mesh_exception_bridge_t* mesh_bootstrap_get_exception_bridge(
    mesh_bootstrap_t* bootstrap
);

/* ============================================================================
 * Convenience Macros
 * ============================================================================ */

/**
 * @brief Route exception from current location
 */
#define MESH_ROUTE_EXCEPTION(bridge, exc, response) \
    mesh_exception_bridge_route((bridge), (exc), (response))

/**
 * @brief Route error with automatic file/line
 */
#define MESH_ROUTE_ERROR(bridge, code, msg, module, response) \
    mesh_exception_bridge_route_error( \
        (bridge), (code), (msg), (module), \
        __FILE__, __LINE__, (response))

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_EXCEPTION_BRIDGE_H */
