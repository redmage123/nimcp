/**
 * @file nimcp_code_immune_self_repair.h
 * @brief Code Immune System Self-Repair Integration Extension
 * @version 1.0.0
 * @date 2025-01-20
 *
 * WHAT: Extends code immune system with auto-repair pipeline integration
 * WHY:  Enable automatic repair on recurring crash patterns
 * HOW:  Connect code immune antigens to self-repair coordinator
 *
 * ARCHITECTURE:
 * ```
 * ┌───────────────────────────────────────────────────────────────────────────┐
 * │                    CODE IMMUNE SELF-REPAIR INTEGRATION                    │
 * ├───────────────────────────────────────────────────────────────────────────┤
 * │                                                                            │
 * │   ┌──────────────┐     ┌─────────────────────┐     ┌─────────────────┐   │
 * │   │Code Immune   │     │  Code Immune        │     │  Self-Repair    │   │
 * │   │ code_antigen │────>│  Self-Repair Bridge │────>│  Coordinator    │   │
 * │   └──────────────┘     └─────────────────────┘     └────────┬────────┘   │
 * │                                                              │            │
 * │   AUTO-TRIGGER CONDITIONS:                                   │            │
 * │   - recurrence_count >= min_crash_count                      │            │
 * │   - severity >= min_severity                                 │            │
 * │   - confidence >= min_confidence                             │            │
 * │   - cooldown period elapsed                                  ▼            │
 * │                                                     ┌─────────────────┐   │
 * │   OUTCOME FEEDBACK:                                 │ Repair Outcome  │   │
 * │   - Update B cell success/failure counts <─────────│   + Learning    │   │
 * │   - Adjust affinity based on outcome               └─────────────────┘   │
 * │   - Notify health agent on failure                                        │
 * │                                                                            │
 * └───────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CODE_IMMUNE_SELF_REPAIR_H
#define NIMCP_CODE_IMMUNE_SELF_REPAIR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Dependencies */
#include "cognitive/immune/nimcp_code_immune.h"
#include "cognitive/fault_tolerance/nimcp_self_repair.h"
#include "utils/fault_tolerance/nimcp_diagnostics.h"

/* Forward declarations */
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define CODE_IMMUNE_SELF_REPAIR_VERSION     "1.0.0"
#define CODE_IMMUNE_SELF_REPAIR_MAGIC       0x43495352  /**< 'CISR' */

/* Default auto-repair thresholds */
#define CODE_IMMUNE_DEFAULT_MIN_CRASH_COUNT 3
#define CODE_IMMUNE_DEFAULT_MIN_SEVERITY    0.8f
#define CODE_IMMUNE_DEFAULT_MIN_CONFIDENCE  0.6f
#define CODE_IMMUNE_DEFAULT_COOLDOWN_MS     5000

/* ============================================================================
 * Configuration
 * ============================================================================ */

/* code_immune_auto_repair_config_t is defined in nimcp_code_immune.h */

/* ============================================================================
 * Tracking Structures
 * ============================================================================ */

/**
 * @brief Auto-repair tracking record
 */
typedef struct {
    uint64_t antigen_id;                /**< Source antigen ID */
    uint64_t repair_id;                 /**< Self-repair repair ID */
    uint64_t b_cell_id;                 /**< Associated B cell ID */
    uint64_t triggered_at;              /**< When repair was triggered (ms) */
    uint64_t completed_at;              /**< When repair completed */
    bool success;                       /**< Repair succeeded */
    char error_message[256];            /**< Error message if failed */
} code_immune_repair_tracking_t;

/**
 * @brief Statistics for code immune self-repair integration
 */
typedef struct {
    uint64_t repairs_triggered;         /**< Total repairs triggered */
    uint64_t repairs_succeeded;         /**< Successful repairs */
    uint64_t repairs_failed;            /**< Failed repairs */
    uint64_t repairs_skipped;           /**< Skipped (cooldown, threshold, etc.) */
    uint64_t by_crash_type[CODE_CRASH_ALL + 1]; /**< Count by crash type */
    float avg_repair_time_ms;           /**< Average repair time */
    float success_rate;                 /**< Overall success rate */
    uint64_t b_cells_updated;           /**< B cells updated from outcomes */
} code_immune_self_repair_stats_t;

/* ============================================================================
 * Bridge Handle
 * ============================================================================ */

/**
 * @brief Opaque code immune self-repair bridge handle
 */
typedef struct code_immune_self_repair_bridge code_immune_self_repair_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default auto-repair configuration
 *
 * WHAT: Initialize configuration with sensible defaults
 * WHY:  Easy setup with good starting values
 * HOW:  Return struct with balanced parameters
 *
 * DEFAULTS:
 * - enabled: true
 * - min_crash_count: 3
 * - min_severity: 0.8
 * - min_confidence: 0.6
 * - cooldown_ms: 5000
 *
 * @param config Output configuration (non-NULL)
 * @return 0 on success, -1 on error
 */
int code_immune_auto_repair_default_config(
    code_immune_auto_repair_config_t* config
);

/**
 * @brief Create code immune self-repair bridge
 *
 * WHAT: Initialize bridge connecting code immune to self-repair
 * WHY:  Enable automatic repair on recurring crashes
 * HOW:  Allocate bridge, initialize tracking
 *
 * @param config Configuration (NULL for defaults)
 * @param code_immune Code immune system (required)
 * @param self_repair Self-repair coordinator (required)
 * @return Bridge handle or NULL on failure
 */
code_immune_self_repair_bridge_t* code_immune_self_repair_bridge_create(
    const code_immune_auto_repair_config_t* config,
    code_immune_system_t* code_immune,
    self_repair_coordinator_t* self_repair
);

/**
 * @brief Destroy code immune self-repair bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structures, disconnect from code immune
 *
 * @param bridge Bridge handle (NULL safe)
 */
void code_immune_self_repair_bridge_destroy(
    code_immune_self_repair_bridge_t* bridge
);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect to health agent for failure notification
 *
 * WHAT: Establish connection to health agent
 * WHY:  Notify health agent of repair failures
 * HOW:  Store reference for callback
 *
 * @param bridge Bridge handle
 * @param health_agent Health agent to notify (can be NULL to disconnect)
 * @return 0 on success, -1 on error
 */
int code_immune_self_repair_connect_health_agent(
    code_immune_self_repair_bridge_t* bridge,
    nimcp_health_agent_t* health_agent
);

/* ============================================================================
 * Conversion API
 * ============================================================================ */

/**
 * @brief Convert code antigen to diagnostic result
 *
 * WHAT: Transform code_antigen_t to diagnostic_result_t
 * WHY:  Feed crash patterns into self-repair pipeline
 * HOW:  Map fields, translate types, enrich context
 *
 * @param antigen Source code antigen
 * @param result Output diagnostic result (caller must free)
 * @return 0 on success, -1 on error
 */
int code_immune_antigen_to_diagnostic(
    const code_antigen_t* antigen,
    diagnostic_result_t** result
);

/* ============================================================================
 * Auto-Trigger API
 * ============================================================================ */

/**
 * @brief Check if antigen should trigger auto-repair
 *
 * WHAT: Evaluate antigen against auto-repair criteria
 * WHY:  Determine if repair should be triggered
 * HOW:  Check thresholds, recurrence count, cooldown
 *
 * @param bridge Bridge handle
 * @param antigen Antigen to evaluate
 * @return true if should trigger repair, false otherwise
 */
bool code_immune_should_auto_repair(
    const code_immune_self_repair_bridge_t* bridge,
    const code_antigen_t* antigen
);

/**
 * @brief Trigger auto-repair for antigen
 *
 * WHAT: Initiate self-repair for crash pattern
 * WHY:  Auto-repair recurring crashes
 * HOW:  Convert antigen to diagnostic, submit to self-repair
 *
 * @param bridge Bridge handle
 * @param antigen_id Antigen ID to repair
 * @param repair_id Output: assigned repair ID (can be NULL)
 * @return 0 on success (triggered), 1 if skipped, -1 on error
 */
int code_immune_trigger_auto_repair(
    code_immune_self_repair_bridge_t* bridge,
    uint64_t antigen_id,
    uint64_t* repair_id
);

/**
 * @brief Process code immune update for auto-repair
 *
 * WHAT: Check pending antigens for auto-repair triggers
 * WHY:  Integrate with code_immune_update cycle
 * HOW:  Evaluate antigens, trigger repairs if criteria met
 *
 * Call this from within or after code_immune_update()
 *
 * @param bridge Bridge handle
 * @return Number of repairs triggered
 */
uint32_t code_immune_process_auto_repairs(
    code_immune_self_repair_bridge_t* bridge
);

/* ============================================================================
 * Outcome Notification API
 * ============================================================================ */

/**
 * @brief Notify bridge of repair outcome
 *
 * WHAT: Update B cell stats based on repair outcome
 * WHY:  Learn from repair success/failure
 * HOW:  Update B cell success/failure counts, adjust affinity
 *
 * @param bridge Bridge handle
 * @param repair_id Repair ID that completed
 * @param success True if repair succeeded
 * @param error_message Error message if failed (can be NULL)
 * @return 0 on success, -1 on error
 */
int code_immune_notify_repair_outcome(
    code_immune_self_repair_bridge_t* bridge,
    uint64_t repair_id,
    bool success,
    const char* error_message
);

/**
 * @brief Process pending repair outcomes
 *
 * WHAT: Check for completed async repairs and update tracking
 * WHY:  Process outcomes for learning
 * HOW:  Poll self-repair for completed items
 *
 * @param bridge Bridge handle
 * @param max_process Maximum outcomes to process
 * @return Number of outcomes processed
 */
uint32_t code_immune_process_repair_outcomes(
    code_immune_self_repair_bridge_t* bridge,
    uint32_t max_process
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get repair tracking record by repair ID
 *
 * @param bridge Bridge handle
 * @param repair_id Repair ID to look up
 * @return Tracking record or NULL if not found
 */
const code_immune_repair_tracking_t* code_immune_get_repair_tracking(
    const code_immune_self_repair_bridge_t* bridge,
    uint64_t repair_id
);

/**
 * @brief Get auto-repair statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int code_immune_self_repair_get_stats(
    const code_immune_self_repair_bridge_t* bridge,
    code_immune_self_repair_stats_t* stats
);

/**
 * @brief Reset auto-repair statistics
 *
 * @param bridge Bridge handle
 */
void code_immune_self_repair_reset_stats(
    code_immune_self_repair_bridge_t* bridge
);

/**
 * @brief Check if bridge is ready
 *
 * @param bridge Bridge handle
 * @return true if ready, false otherwise
 */
bool code_immune_self_repair_is_ready(
    const code_immune_self_repair_bridge_t* bridge
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Broadcast auto-repair trigger via bio-async
 *
 * @param bridge Bridge handle
 * @param antigen_id Antigen ID being repaired
 * @param repair_id Self-repair ID
 * @return 0 on success, -1 on error
 */
int code_immune_self_repair_broadcast_trigger(
    code_immune_self_repair_bridge_t* bridge,
    uint64_t antigen_id,
    uint64_t repair_id
);

/**
 * @brief Broadcast repair outcome via bio-async
 *
 * @param bridge Bridge handle
 * @param repair_id Repair ID
 * @param success True if repair succeeded
 * @return 0 on success, -1 on error
 */
int code_immune_self_repair_broadcast_outcome(
    code_immune_self_repair_bridge_t* bridge,
    uint64_t repair_id,
    bool success
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get bridge version string
 *
 * @return Version string
 */
const char* code_immune_self_repair_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CODE_IMMUNE_SELF_REPAIR_H */
