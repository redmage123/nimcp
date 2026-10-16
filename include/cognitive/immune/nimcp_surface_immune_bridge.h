/**
 * @file nimcp_surface_immune_bridge.h
 * @brief Surface Geometry Immune Integration Bridge
 *
 * WHAT: Immune system integration for surface geometry anomaly detection
 * WHY:  Reports geometry violations as antigens for immune response
 * HOW:  Integrates with brain immune system B cells, antibodies, cytokines
 *
 * ANTIGEN TYPES:
 * - Invalid chi: Chi outside valid range [0, 2]
 * - Impossible trifurcation: k=4 where chi < 0.83
 * - Angle violation: Branch angle outside predictions
 * - Material overflow: Exceeded material budget
 * - Topology error: Invalid manifold configuration
 *
 * IMMUNE RESPONSES:
 * - B cell activation for persistent anomalies
 * - Antibody production for known violation patterns
 * - Cytokine release for system-wide alerts
 *
 * @version 1.0.0
 * @date 2026-01-13
 */

#ifndef NIMCP_SURFACE_IMMUNE_BRIDGE_H
#define NIMCP_SURFACE_IMMUNE_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include "core/geometry/nimcp_surface_geometry_types.h"

//=============================================================================
// MODULE IDENTIFIER
//=============================================================================

#define BIO_MODULE_SURFACE_IMMUNE       0x1430

//=============================================================================
// ANTIGEN TYPES
//=============================================================================

/**
 * @brief Surface geometry antigen types
 */
typedef enum surface_antigen_type_enum {
    SURFACE_ANTIGEN_NONE = 0,
    SURFACE_ANTIGEN_INVALID_CHI,            /**< Chi out of range [0, 2] */
    SURFACE_ANTIGEN_IMPOSSIBLE_TRIFURCATION,/**< k=4 where chi < 0.83 */
    SURFACE_ANTIGEN_ANGLE_VIOLATION,        /**< Angle outside predictions */
    SURFACE_ANTIGEN_MATERIAL_OVERFLOW,      /**< Exceeded budget */
    SURFACE_ANTIGEN_TOPOLOGY_ERROR,         /**< Invalid manifold */
    SURFACE_ANTIGEN_RHO_OUT_OF_RANGE,       /**< Rho outside [0, 1] */
    SURFACE_ANTIGEN_DEGENERATE_GEOMETRY,    /**< Degenerate configuration */
    SURFACE_ANTIGEN_DISCONNECTED_NETWORK,   /**< Disconnected topology */
    SURFACE_ANTIGEN_COUNT
} surface_antigen_type_t;

/**
 * @brief Antigen severity levels
 */
typedef enum surface_antigen_severity_enum {
    SURFACE_SEVERITY_INFO = 0,              /**< Informational */
    SURFACE_SEVERITY_WARNING,               /**< Warning - may need attention */
    SURFACE_SEVERITY_ERROR,                 /**< Error - needs correction */
    SURFACE_SEVERITY_CRITICAL               /**< Critical - immediate action */
} surface_antigen_severity_t;

//=============================================================================
// ANTIGEN DATA
//=============================================================================

/**
 * @brief Surface geometry antigen
 */
typedef struct surface_antigen_struct {
    uint32_t id;                            /**< Unique antigen ID */
    surface_antigen_type_t type;            /**< Antigen type */
    surface_antigen_severity_t severity;    /**< Severity level */

    /* Location */
    uint32_t branch_point_id;               /**< Related branch point */
    float position[3];                      /**< Position where detected */

    /* Violation details */
    float expected_value;                   /**< Expected value */
    float actual_value;                     /**< Actual value */
    float deviation;                        /**< abs(actual - expected) */

    /* Timing */
    uint64_t timestamp_ms;                  /**< Detection time */
    uint64_t duration_ms;                   /**< How long anomaly persisted */

    /* State */
    bool active;                            /**< Still active */
    bool acknowledged;                      /**< Acknowledged by immune */
    uint32_t occurrence_count;              /**< Times this occurred */
} surface_antigen_t;

//=============================================================================
// ANTIBODY TYPES
//=============================================================================

/**
 * @brief Antibody for geometry correction
 */
typedef struct surface_antibody_struct {
    uint32_t id;                            /**< Unique antibody ID */
    surface_antigen_type_t target_type;     /**< Target antigen type */

    /* Correction action */
    float correction_factor;                /**< Correction multiplier */
    float max_correction;                   /**< Maximum correction */

    /* Effectiveness */
    float affinity;                         /**< Binding affinity [0, 1] */
    uint32_t successful_corrections;        /**< Times successfully corrected */
    uint32_t failed_corrections;            /**< Times failed */

    /* Timing */
    uint64_t created_ms;                    /**< Creation time */
    uint64_t last_used_ms;                  /**< Last use time */
    bool active;                            /**< Still active */
} surface_antibody_t;

//=============================================================================
// CONFIGURATION
//=============================================================================

/**
 * @brief Immune bridge configuration
 */
typedef struct surface_immune_config_struct {
    /* Detection thresholds */
    float chi_min;                          /**< Min valid chi (default: 0) */
    float chi_max;                          /**< Max valid chi (default: 2) */
    float angle_tolerance;                  /**< Angle tolerance degrees (default: 5) */
    float material_budget;                  /**< Max material budget */
    float rho_min;                          /**< Min valid rho (default: 0) */
    float rho_max;                          /**< Max valid rho (default: 1) */

    /* Immune response */
    uint32_t b_cell_activation_threshold;   /**< Occurrences before B cell */
    uint32_t antibody_production_threshold; /**< Occurrences before antibody */
    float cytokine_release_severity;        /**< Min severity for cytokine */

    /* Timing */
    uint32_t antigen_persistence_ms;        /**< Time before deactivation */
    uint32_t antibody_lifetime_ms;          /**< Antibody lifetime */

    /* Limits */
    uint32_t max_active_antigens;           /**< Max concurrent antigens */
    uint32_t max_antibodies;                /**< Max antibodies */
} surface_immune_config_t;

//=============================================================================
// STATISTICS
//=============================================================================

/**
 * @brief Immune bridge statistics
 */
typedef struct surface_immune_stats_struct {
    /* Detection */
    uint64_t total_validations;
    uint64_t anomalies_detected;
    uint64_t anomalies_resolved;

    /* Antigens by type */
    uint64_t antigen_counts[SURFACE_ANTIGEN_COUNT];

    /* Immune response */
    uint64_t b_cells_activated;
    uint64_t antibodies_produced;
    uint64_t cytokines_released;

    /* Corrections */
    uint64_t corrections_attempted;
    uint64_t corrections_successful;
    float correction_success_rate;

    /* Current state */
    uint32_t active_antigens;
    uint32_t active_antibodies;
} surface_immune_stats_t;

//=============================================================================
// BRIDGE STRUCTURE
//=============================================================================

/**
 * @brief Surface geometry immune integration bridge
 */
typedef struct surface_immune_bridge_struct {
    /* Base bridge (MUST be first) */
    bridge_base_t base;

    /* Connected systems */
    void* geometry_ctx;                     /**< Surface geometry context */
    void* immune_system;                    /**< Brain immune system */

    /* Configuration */
    surface_immune_config_t config;

    /* Active antigens */
    surface_antigen_t* active_antigens;
    uint32_t num_active_antigens;
    uint32_t max_antigens;

    /* Antibodies */
    surface_antibody_t* antibodies;
    uint32_t num_antibodies;
    uint32_t max_antibodies;

    /* Statistics */
    surface_immune_stats_t stats;

    /* Antigen ID counter */
    uint32_t next_antigen_id;
    uint32_t next_antibody_id;
} surface_immune_bridge_t;

//=============================================================================
// LIFECYCLE
//=============================================================================

/**
 * @brief Initialize configuration with defaults
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
int surface_immune_default_config(surface_immune_config_t* config);

/**
 * @brief Create immune bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Created bridge or NULL on failure
 */
surface_immune_bridge_t* surface_immune_bridge_create(
    const surface_immune_config_t* config
);

/**
 * @brief Destroy immune bridge
 *
 * @param bridge Bridge to destroy
 */
void surface_immune_bridge_destroy(surface_immune_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int surface_immune_bridge_reset(surface_immune_bridge_t* bridge);

//=============================================================================
// CONNECTION
//=============================================================================

/**
 * @brief Connect to geometry context
 *
 * @param bridge Bridge
 * @param ctx Geometry context
 * @return 0 on success, -1 on error
 */
int surface_immune_bridge_connect_geometry(
    surface_immune_bridge_t* bridge,
    void* ctx
);

/**
 * @brief Connect to immune system
 *
 * @param bridge Bridge
 * @param immune Brain immune system
 * @return 0 on success, -1 on error
 */
int surface_immune_bridge_connect_immune(
    surface_immune_bridge_t* bridge,
    void* immune
);

/**
 * @brief Check connection status
 *
 * @param bridge Bridge
 * @return true if connected
 */
bool surface_immune_bridge_is_connected(const surface_immune_bridge_t* bridge);

//=============================================================================
// VALIDATION
//=============================================================================

/**
 * @brief Validate geometry parameters
 *
 * Checks all parameters against thresholds and presents
 * antigens for any violations.
 *
 * @param bridge Bridge
 * @param params Parameters to validate
 * @param is_valid Output: true if valid
 * @param violation_type Output: type of violation (if any)
 * @return 0 on success, -1 on error
 */
int surface_immune_validate_geometry(
    surface_immune_bridge_t* bridge,
    const surface_geometry_params_t* params,
    bool* is_valid,
    surface_antigen_type_t* violation_type
);

/**
 * @brief Validate branch point
 *
 * @param bridge Bridge
 * @param branch Branch point to validate
 * @param is_valid Output: true if valid
 * @param antigen_id Output: antigen ID if violation (0 if valid)
 * @return 0 on success, -1 on error
 */
int surface_immune_validate_branch(
    surface_immune_bridge_t* bridge,
    const surface_branch_point_t* branch,
    bool* is_valid,
    uint32_t* antigen_id
);

//=============================================================================
// ANTIGEN MANAGEMENT
//=============================================================================

/**
 * @brief Present anomaly as antigen
 *
 * @param bridge Bridge
 * @param type Antigen type
 * @param branch Related branch point
 * @param expected Expected value
 * @param actual Actual value
 * @param antigen_id Output: created antigen ID
 * @return 0 on success, -1 on error
 */
int surface_immune_present_anomaly(
    surface_immune_bridge_t* bridge,
    surface_antigen_type_t type,
    const surface_branch_point_t* branch,
    float expected,
    float actual,
    uint32_t* antigen_id
);

/**
 * @brief Acknowledge antigen (mark as handled)
 *
 * @param bridge Bridge
 * @param antigen_id Antigen to acknowledge
 * @return 0 on success, -1 on error
 */
int surface_immune_acknowledge_antigen(
    surface_immune_bridge_t* bridge,
    uint32_t antigen_id
);

/**
 * @brief Resolve antigen (anomaly fixed)
 *
 * @param bridge Bridge
 * @param antigen_id Antigen to resolve
 * @return 0 on success, -1 on error
 */
int surface_immune_resolve_antigen(
    surface_immune_bridge_t* bridge,
    uint32_t antigen_id
);

/**
 * @brief Get active antigens
 *
 * @param bridge Bridge
 * @param antigens Output: active antigens
 * @param max_antigens Maximum to return
 * @param num_antigens Output: actual count
 * @return 0 on success, -1 on error
 */
int surface_immune_get_active_antigens(
    const surface_immune_bridge_t* bridge,
    surface_antigen_t* antigens,
    uint32_t max_antigens,
    uint32_t* num_antigens
);

//=============================================================================
// ANTIBODY MANAGEMENT
//=============================================================================

/**
 * @brief Produce antibody for antigen type
 *
 * @param bridge Bridge
 * @param target_type Target antigen type
 * @param antibody_id Output: created antibody ID
 * @return 0 on success, -1 on error
 */
int surface_immune_produce_antibody(
    surface_immune_bridge_t* bridge,
    surface_antigen_type_t target_type,
    uint32_t* antibody_id
);

/**
 * @brief Apply antibody correction
 *
 * @param bridge Bridge
 * @param antibody_id Antibody to use
 * @param params Parameters to correct (modified in place)
 * @param success Output: true if correction successful
 * @return 0 on success, -1 on error
 */
int surface_immune_apply_antibody(
    surface_immune_bridge_t* bridge,
    uint32_t antibody_id,
    surface_geometry_params_t* params,
    bool* success
);

//=============================================================================
// IMMUNE RESPONSE
//=============================================================================

/**
 * @brief Trigger cytokine release for critical anomaly
 *
 * Sends system-wide alert via bio-async.
 *
 * @param bridge Bridge
 * @param antigen_id Related antigen
 * @param message Alert message
 * @return 0 on success, -1 on error
 */
int surface_immune_release_cytokine(
    surface_immune_bridge_t* bridge,
    uint32_t antigen_id,
    const char* message
);

/**
 * @brief Activate B cell for persistent anomaly
 *
 * @param bridge Bridge
 * @param antigen_id Related antigen
 * @return 0 on success, -1 on error
 */
int surface_immune_activate_b_cell(
    surface_immune_bridge_t* bridge,
    uint32_t antigen_id
);

//=============================================================================
// STATISTICS
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge
 * @param stats Output: statistics
 * @return 0 on success, -1 on error
 */
int surface_immune_get_stats(
    const surface_immune_bridge_t* bridge,
    surface_immune_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int surface_immune_reset_stats(surface_immune_bridge_t* bridge);

//=============================================================================
// UTILITY
//=============================================================================

/**
 * @brief Get antigen type name
 *
 * @param type Antigen type
 * @return Human-readable name
 */
const char* surface_antigen_type_name(surface_antigen_type_t type);

/**
 * @brief Get severity name
 *
 * @param severity Severity level
 * @return Human-readable name
 */
const char* surface_antigen_severity_name(surface_antigen_severity_t severity);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SURFACE_IMMUNE_BRIDGE_H */
