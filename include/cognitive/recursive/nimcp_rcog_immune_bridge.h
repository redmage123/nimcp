/**
 * @file nimcp_rcog_immune_bridge.h
 * @brief Brain Immune System Integration Bridge for Recursive Cognition
 * @version 1.0.0
 * @date 2026-01-03
 *
 * WHAT: Bidirectional bridge connecting recursive cognition with brain immune system
 * WHY:  Immune system modulates cognitive capacity based on system health
 * HOW:  Full bridge pattern with cytokine effects and failure reporting
 *
 * BIOLOGICAL BASIS:
 * The immune system affects cognitive function through multiple pathways:
 * - Pro-inflammatory cytokines (IL-1β, IL-6, TNF-α) impair working memory
 * - Chronic inflammation reduces processing speed and depth
 * - Anti-inflammatory cytokines (IL-10) support recovery
 * - Repeated failures are "remembered" like antigens for avoidance
 *
 * ARCHITECTURE:
 * ```
 * +----------------------+                    +----------------------+
 * | RECURSIVE COGNITION  |                    |  BRAIN IMMUNE SYSTEM |
 * |                      |                    |                      |
 * | - Context Store      |<-- capacity ------>| - Cytokine Levels    |
 * |   (reduced size)     |    modulation      | - Inflammation       |
 * | - Orchestrator       |                    | - B/T Cells          |
 * |   (reduced depth)    |<-- quarantine ---->| - Antibodies         |
 * | - Delegation Pool    |    patterns        | - Microglia          |
 * |   (reduced parallel) |                    |                      |
 * +----------------------+                    +----------------------+
 *           |                                           |
 *           +---------------- BRIDGE -------------------+
 *                      (health-aware processing)
 * ```
 *
 * CYTOKINE EFFECTS:
 * - IL-1β (pro-inflammatory): ↓ Working memory capacity, ↓ context store size
 * - IL-6 (acute phase): ↓ Parallelism, ↑ timeouts
 * - TNF-α (severe): ↓ Max recursion depth, enable degraded mode
 * - IL-10 (anti-inflammatory): Gradual recovery of capacity
 * - IFN-γ (quarantine): Isolate suspicious subtasks
 */

#ifndef NIMCP_RCOG_IMMUNE_BRIDGE_H
#define NIMCP_RCOG_IMMUNE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/recursive/nimcp_rcog_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

struct rcog_engine;
struct rcog_orchestrator;
struct rcog_delegation_pool;
struct rcog_decomposition;
struct rcog_subtask;
struct brain_immune_system;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Maximum quarantined patterns */
#define RCOG_IMMUNE_MAX_QUARANTINED_PATTERNS    32

/** Default inflammation response threshold */
#define RCOG_IMMUNE_DEFAULT_INFLAMMATION_THRESHOLD  0.3f

/** Default capacity reduction per inflammation level */
#define RCOG_IMMUNE_DEFAULT_CAPACITY_REDUCTION      0.2f

/** Default recovery rate per update */
#define RCOG_IMMUNE_DEFAULT_RECOVERY_RATE           0.01f

/** Failure count threshold for quarantine */
#define RCOG_IMMUNE_DEFAULT_QUARANTINE_THRESHOLD    3

/*=============================================================================
 * INFLAMMATION LEVELS
 *===========================================================================*/

/**
 * @brief Inflammation level enumeration
 */
typedef enum {
    RCOG_INFLAMMATION_NONE = 0,      /**< Full capacity */
    RCOG_INFLAMMATION_LOCAL,         /**< Reduce parallelism by 20% */
    RCOG_INFLAMMATION_REGIONAL,      /**< Reduce depth, 40% parallelism */
    RCOG_INFLAMMATION_SYSTEMIC,      /**< Degraded mode, sequential only */
    RCOG_INFLAMMATION_STORM          /**< Emergency shutdown */
} rcog_inflammation_level_t;

/*=============================================================================
 * EFFECTS STRUCTURES
 *===========================================================================*/

/**
 * @brief Cytokine levels affecting recursive cognition
 */
typedef struct {
    float il1_beta;                  /**< IL-1β level [0.0-1.0] */
    float il6;                       /**< IL-6 level [0.0-1.0] */
    float tnf_alpha;                 /**< TNF-α level [0.0-1.0] */
    float il10;                      /**< IL-10 level [0.0-1.0] (anti-inflammatory) */
    float ifn_gamma;                 /**< IFN-γ level [0.0-1.0] (quarantine signal) */
} rcog_cytokine_levels_t;

/**
 * @brief Quarantine entry for failed patterns
 */
typedef struct {
    uint64_t pattern_hash;           /**< Hash of decomposition pattern */
    uint32_t failure_count;          /**< Number of failures */
    uint64_t first_failure_ms;       /**< First failure timestamp */
    uint64_t last_failure_ms;        /**< Last failure timestamp */
    rcog_error_t last_error;         /**< Last error code */
    float quarantine_strength;       /**< How strongly quarantined [0.0-1.0] */
} rcog_quarantine_entry_t;

/**
 * @brief Effects flowing from recursive cognition to immune system
 *
 * WHAT: Failure reports and stress signals
 * WHY:  Immune system learns to avoid problematic patterns
 */
typedef struct {
    /* Failure reports */
    bool report_subtask_failure;     /**< Report a subtask failure */
    uint64_t failed_subtask_id;      /**< ID of failed subtask */
    rcog_error_t failure_error;      /**< Error code */
    uint64_t failure_pattern_hash;   /**< Hash of decomposition pattern */

    /* Stress signals */
    float processing_stress;         /**< Current processing stress [0.0-1.0] */
    uint32_t depth_pressure;         /**< Pressure on depth limit */
    uint32_t timeout_pressure;       /**< Pressure from timeouts */
    float resource_exhaustion;       /**< Resource exhaustion level */

    /* Recovery signals */
    bool signal_recovery;            /**< Signal that recovery is happening */
    float recovery_rate;             /**< Rate of recovery */

    /* Statistics for immune learning */
    uint32_t total_failures;         /**< Total failures this session */
    uint32_t consecutive_failures;   /**< Consecutive failures */
    float failure_rate;              /**< Failure rate [0.0-1.0] */
} rcog_to_immune_effects_t;

/**
 * @brief Effects flowing from immune system to recursive cognition
 *
 * WHAT: Modulation of cognitive capacity based on health
 * WHY:  Protect system from damage during illness
 */
typedef struct {
    /* Capacity modulation (from rcog_immune_modulation_t) */
    float capacity_multiplier;       /**< Overall capacity [0.0-1.0] */
    float max_depth_multiplier;      /**< Depth limit multiplier [0.0-1.0] */
    float parallelism_multiplier;    /**< Parallel workers multiplier [0.0-1.0] */
    float timeout_multiplier;        /**< Timeout multiplier [1.0+] */
    bool enable_degraded_mode;       /**< Use simplified decomposition */

    /* Inflammation status */
    rcog_inflammation_level_t inflammation_level; /**< Current level */
    rcog_cytokine_levels_t cytokines; /**< Current cytokine levels */

    /* Quarantine information */
    uint32_t num_quarantined;        /**< Number of quarantined patterns */
    bool check_quarantine;           /**< Should check patterns against quarantine */

    /* Recovery status */
    bool recovery_active;            /**< Recovery is in progress */
    float recovery_progress;         /**< Recovery progress [0.0-1.0] */
    uint64_t estimated_recovery_ms;  /**< Estimated time to full recovery */

    /* Emergency flags */
    bool emergency_shutdown;         /**< Shut down immediately */
    bool partial_results_only;       /**< Return partial results only */
} immune_to_rcog_effects_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Immune bridge configuration
 */
typedef struct {
    /* Sensitivity to cytokines */
    float il1_sensitivity;           /**< Sensitivity to IL-1β */
    float il6_sensitivity;           /**< Sensitivity to IL-6 */
    float tnf_sensitivity;           /**< Sensitivity to TNF-α */
    float il10_sensitivity;          /**< Sensitivity to IL-10 */

    /* Capacity modulation */
    float min_capacity;              /**< Minimum capacity even when sick */
    float min_depth;                 /**< Minimum recursion depth */
    float min_parallelism;           /**< Minimum parallel workers */

    /* Quarantine settings */
    uint32_t quarantine_threshold;   /**< Failures before quarantine */
    float quarantine_decay_rate;     /**< Decay rate of quarantine */
    uint32_t max_quarantine_entries; /**< Max quarantined patterns */

    /* Recovery settings */
    float recovery_rate;             /**< Base recovery rate */
    bool enable_auto_recovery;       /**< Enable automatic recovery */
} rcog_immune_bridge_config_t;

/*=============================================================================
 * BRIDGE HANDLE
 *===========================================================================*/

/**
 * @brief Immune bridge opaque handle
 */
typedef struct rcog_immune_bridge rcog_immune_bridge_t;

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Create immune bridge with configuration
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
rcog_immune_bridge_t* rcog_immune_bridge_create(
    const rcog_immune_bridge_config_t* config
);

/**
 * @brief Create bridge with default configuration
 * @return Bridge handle or NULL on error
 */
rcog_immune_bridge_t* rcog_immune_bridge_create_default(void);

/**
 * @brief Destroy immune bridge
 * @param bridge Bridge handle (NULL safe)
 */
void rcog_immune_bridge_destroy(rcog_immune_bridge_t* bridge);

/**
 * @brief Get default configuration
 * @return Default configuration
 */
rcog_immune_bridge_config_t rcog_immune_bridge_default_config(void);

/*=============================================================================
 * CONNECTION
 *===========================================================================*/

/**
 * @brief Connect bridge to brain immune system
 * @param bridge Bridge handle
 * @param immune Brain immune system handle
 * @return 0 on success, error code on failure
 */
int rcog_immune_bridge_connect(
    rcog_immune_bridge_t* bridge,
    struct brain_immune_system* immune
);

/**
 * @brief Connect bridge to recursive cognition engine
 * @param bridge Bridge handle
 * @param engine Recursive cognition engine handle
 * @return 0 on success, error code on failure
 */
int rcog_immune_bridge_connect_engine(
    rcog_immune_bridge_t* bridge,
    struct rcog_engine* engine
);

/**
 * @brief Check if bridge is connected
 * @param bridge Bridge handle
 * @return true if connected
 */
bool rcog_immune_bridge_is_connected(const rcog_immune_bridge_t* bridge);

/*=============================================================================
 * UPDATE
 *===========================================================================*/

/**
 * @brief Update bridge state
 * @param bridge Bridge handle
 * @param delta_time_ms Time since last update
 * @return 0 on success, error code on failure
 */
int rcog_immune_bridge_update(
    rcog_immune_bridge_t* bridge,
    float delta_time_ms
);

/*=============================================================================
 * MODULATION
 *===========================================================================*/

/**
 * @brief Get current immune modulation effects
 * @param bridge Bridge handle
 * @param modulation Output modulation structure
 * @return 0 on success, error code on failure
 */
int rcog_immune_bridge_get_modulation(
    const rcog_immune_bridge_t* bridge,
    rcog_immune_modulation_t* modulation
);

/**
 * @brief Apply immune modulation to orchestrator
 * @param bridge Bridge handle
 * @param orchestrator Orchestrator to modify
 * @return 0 on success, error code on failure
 */
int rcog_immune_bridge_apply_modulation(
    rcog_immune_bridge_t* bridge,
    struct rcog_orchestrator* orchestrator
);

/**
 * @brief Get current inflammation level
 * @param bridge Bridge handle
 * @return Current inflammation level
 */
rcog_inflammation_level_t rcog_immune_bridge_get_inflammation_level(
    const rcog_immune_bridge_t* bridge
);

/**
 * @brief Get current cytokine levels
 * @param bridge Bridge handle
 * @param cytokines Output cytokine structure
 * @return 0 on success, error code on failure
 */
int rcog_immune_bridge_get_cytokines(
    const rcog_immune_bridge_t* bridge,
    rcog_cytokine_levels_t* cytokines
);

/*=============================================================================
 * FAILURE REPORTING
 *===========================================================================*/

/**
 * @brief Report subtask failure as potential "antigen"
 * @param bridge Bridge handle
 * @param subtask Failed subtask
 * @param error Error code
 * @return 0 on success, error code on failure
 */
int rcog_immune_bridge_report_failure(
    rcog_immune_bridge_t* bridge,
    const struct rcog_subtask* subtask,
    rcog_error_t error
);

/**
 * @brief Report decomposition pattern failure
 * @param bridge Bridge handle
 * @param decomposition Failed decomposition
 * @param error Error code
 * @return 0 on success, error code on failure
 */
int rcog_immune_bridge_report_pattern_failure(
    rcog_immune_bridge_t* bridge,
    const struct rcog_decomposition* decomposition,
    rcog_error_t error
);

/*=============================================================================
 * QUARANTINE
 *===========================================================================*/

/**
 * @brief Check if decomposition pattern is quarantined
 * @param bridge Bridge handle
 * @param decomposition Decomposition to check
 * @return true if quarantined
 */
bool rcog_immune_bridge_is_quarantined(
    const rcog_immune_bridge_t* bridge,
    const struct rcog_decomposition* decomposition
);

/**
 * @brief Get quarantine strength for a pattern
 * @param bridge Bridge handle
 * @param decomposition Decomposition to check
 * @return Quarantine strength [0.0-1.0], 0 if not quarantined
 */
float rcog_immune_bridge_get_quarantine_strength(
    const rcog_immune_bridge_t* bridge,
    const struct rcog_decomposition* decomposition
);

/**
 * @brief Get list of quarantined patterns
 * @param bridge Bridge handle
 * @param entries Output array of quarantine entries
 * @param max_entries Maximum entries to return
 * @param num_entries Output number of entries
 * @return 0 on success, error code on failure
 */
int rcog_immune_bridge_get_quarantine_list(
    const rcog_immune_bridge_t* bridge,
    rcog_quarantine_entry_t* entries,
    size_t max_entries,
    size_t* num_entries
);

/**
 * @brief Clear a pattern from quarantine
 * @param bridge Bridge handle
 * @param pattern_hash Hash of pattern to clear
 * @return 0 on success, error code on failure
 */
int rcog_immune_bridge_clear_quarantine(
    rcog_immune_bridge_t* bridge,
    uint64_t pattern_hash
);

/*=============================================================================
 * EFFECTS ACCESS
 *===========================================================================*/

/**
 * @brief Get current effects from rcog to immune
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int rcog_immune_bridge_get_outgoing_effects(
    const rcog_immune_bridge_t* bridge,
    rcog_to_immune_effects_t* effects
);

/**
 * @brief Get current effects from immune to rcog
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int rcog_immune_bridge_get_incoming_effects(
    const rcog_immune_bridge_t* bridge,
    immune_to_rcog_effects_t* effects
);

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t failures_reported;
    uint64_t patterns_quarantined;
    uint64_t patterns_blocked;
    uint64_t modulations_applied;
    float avg_capacity_reduction;
    float peak_inflammation;
    uint64_t emergency_shutdowns;
    float total_recovery_time_ms;
} rcog_immune_bridge_stats_t;

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int rcog_immune_bridge_get_stats(
    const rcog_immune_bridge_t* bridge,
    rcog_immune_bridge_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 * @param bridge Bridge handle
 */
void rcog_immune_bridge_reset_stats(rcog_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RCOG_IMMUNE_BRIDGE_H */
