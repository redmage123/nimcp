/**
 * @file nimcp_mesh_cycle_coordinator.h
 * @brief Mesh-Cycle Coordinator Integration - Brain Cycle Timing for Mesh Transactions
 *
 * WHAT: Integrates brain cycle coordinator with mesh network for timing-aware
 *       transaction ordering, stall recovery, and distributed health consensus.
 * WHY:  Brain cycles provide biological timing constraints that should inform
 *       mesh transaction batching, commit deadlines, and recovery actions.
 *       This enables bio-plausible distributed cognition where neural rhythms
 *       coordinate multi-agent consensus.
 * HOW:  Bridges brain_cycle_coordinator_t to mesh ordering, resilience, and
 *       health systems. Cycle timing drives batch windows, stalls trigger
 *       recovery through mesh, health scores contribute to mesh consensus.
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────────┐
 * │                  MESH-CYCLE COORDINATOR INTEGRATION                         │
 * ├─────────────────────────────────────────────────────────────────────────────┤
 * │                                                                              │
 * │  ┌──────────────────────────────────────────────────────────────────────┐   │
 * │  │                    BRAIN CYCLE COORDINATOR                            │   │
 * │  │  ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌────────────┐         │   │
 * │  │  │ Immune     │ │ Health     │ │ Oscillation│ │ Brain      │  ...    │   │
 * │  │  │ Tick (50ms)│ │ Agent(100ms)│ │  (10ms)   │ │ Update(16ms)│        │   │
 * │  │  └──────┬─────┘ └──────┬─────┘ └──────┬─────┘ └──────┬─────┘         │   │
 * │  └─────────┼──────────────┼──────────────┼──────────────┼────────────────┘   │
 * │            │              │              │              │                    │
 * │            ▼              ▼              ▼              ▼                    │
 * │  ┌──────────────────────────────────────────────────────────────────────┐   │
 * │  │               MESH-CYCLE COORDINATOR INTEGRATION                      │   │
 * │  │  • Timing constraints from cycle intervals                            │   │
 * │  │  • Batch windows computed from cycle timing                           │   │
 * │  │  • Stall detection triggers mesh recovery                             │   │
 * │  │  • Health scores contribute to mesh consensus                         │   │
 * │  │  • Exceptions routed to immune via exception bridge                   │   │
 * │  │  • BBB validation for cycle-critical transactions                     │   │
 * │  └──────────────────────────────────────────────────────────────────────┘   │
 * │            │              │              │              │                    │
 * │            ▼              ▼              ▼              ▼                    │
 * │  ┌────────────┐  ┌────────────────┐  ┌────────────────┐  ┌────────────────┐ │
 * │  │  Ordering  │  │   Resilience   │  │ Health Bridge  │  │ Exception      │ │
 * │  │  Service   │  │   Integration  │  │                │  │ Bridge         │ │
 * │  │ (batching) │  │   (recovery)   │  │ (consensus)    │  │ (immune)       │ │
 * │  └────────────┘  └────────────────┘  └────────────────┘  └────────────────┘ │
 * │                                                                              │
 * └─────────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * BIOLOGICAL MOTIVATION:
 * - Brain cycles (oscillations, circadian, sleep-wake) coordinate neural activity
 * - Timing constraints ensure coherent processing across distributed regions
 * - Stalls in cycles indicate system distress requiring coordinated recovery
 * - Health is a distributed property requiring consensus across brain regions
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_CYCLE_COORDINATOR_H
#define NIMCP_MESH_CYCLE_COORDINATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_resilience_integration.h"
#include "core/brain/nimcp_brain_cycle_coordinator.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/** @brief Opaque mesh-cycle coordinator integration handle */
typedef struct mesh_cycle_coordinator_integration mesh_cycle_coordinator_integration_t;

typedef struct mesh_bootstrap mesh_bootstrap_t;
typedef struct mesh_ordering_service mesh_ordering_service_t;
typedef struct mesh_resilience_integration mesh_resilience_integration_t;
typedef struct mesh_health_bridge mesh_health_bridge_t;
typedef struct mesh_exception_bridge mesh_exception_bridge_t;
typedef struct mesh_msp mesh_msp_t;
typedef struct mesh_transaction mesh_transaction_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Magic number for structure validation */
#define MESH_CYCLE_COORDINATOR_MAGIC            0x4D434349  /* "MCCI" */

/** @brief Maximum timing constraints tracked */
#define MESH_CYCLE_MAX_TIMING_CONSTRAINTS       16

/** @brief Maximum stall events tracked */
#define MESH_CYCLE_MAX_STALL_EVENTS             64

/** @brief Default stall recovery threshold (consecutive stalls before recovery) */
#define MESH_CYCLE_DEFAULT_STALL_RECOVERY_THRESHOLD     3

/** @brief Default timing batch multiplier (batch_window = interval * multiplier) */
#define MESH_CYCLE_DEFAULT_TIMING_BATCH_MULTIPLIER      2.0f

/** @brief Default health endorsement weight */
#define MESH_CYCLE_DEFAULT_HEALTH_ENDORSEMENT_WEIGHT    0.5f

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Mesh-cycle coordinator integration configuration
 *
 * WHAT: Configuration for how brain cycles integrate with mesh network
 * WHY:  Tunable parameters for timing-aware transaction processing
 * HOW:  Enable flags control feature activation, thresholds control behavior
 */
typedef struct mesh_cycle_coordinator_config {
    /* Feature enables */
    bool enable_timing_constraints;     /**< Use cycle timing for ordering batches */
    bool enable_stall_recovery;         /**< Trigger mesh recovery on cycle stalls */
    bool enable_distributed_health;     /**< Participate in mesh health consensus */
    bool enable_cross_channel_sync;     /**< Sync cycles across mesh channels */

    /* Stall recovery settings */
    uint32_t stall_recovery_threshold;  /**< Consecutive stalls before recovery */

    /* Timing settings */
    float timing_batch_multiplier;      /**< Multiplier for batch window from cycle interval */

    /* Health consensus settings */
    float health_endorsement_weight;    /**< Weight of this node in health consensus [0-1] */

    /* Logging */
    bool verbose_logging;               /**< Enable verbose logging */
    bool enable_debug_logging;          /**< Enable debug-level logging */
} mesh_cycle_coordinator_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Mesh-cycle coordinator integration statistics
 *
 * WHAT: Counters for all integration activities
 * WHY:  Monitoring, debugging, and performance tuning
 */
typedef struct mesh_cycle_coordinator_stats {
    /* Cycle reporting */
    uint64_t cycles_reported;           /**< Total cycle ticks reported */
    uint64_t stalls_detected;           /**< Total stalls detected */
    uint64_t recoveries_triggered;      /**< Recovery actions triggered */

    /* Timing */
    uint64_t timing_constraints_applied;/**< Times timing constraints affected batching */
    uint64_t mesh_transactions_timed;   /**< Transactions with timing metadata */
    uint64_t ordering_batches_adjusted; /**< Batches adjusted due to cycle timing */

    /* Health */
    uint64_t health_endorsements;       /**< Health endorsements contributed */

    /* Exception/immune routing */
    uint64_t exceptions_routed_to_immune; /**< Exceptions routed via exception bridge */

    /* Security */
    uint64_t bbb_validations;           /**< BBB validations performed */

    /* Timing performance */
    uint64_t total_timing_violations;   /**< Total timing constraint violations */
    float avg_batch_window_us;          /**< Average batch window in microseconds */
    float avg_commit_deadline_us;       /**< Average commit deadline in microseconds */
} mesh_cycle_coordinator_stats_t;

/* ============================================================================
 * Cycle Timing Constraint
 * ============================================================================ */

/**
 * @brief Timing constraint derived from a brain cycle
 *
 * WHAT: Timing parameters for mesh transaction coordination
 * WHY:  Brain cycles have intervals/deadlines that should constrain mesh batching
 * HOW:  Extracted from cycle coordinator, used by ordering service
 */
typedef struct mesh_cycle_timing_constraint {
    brain_cycle_type_t cycle_type;      /**< Source brain cycle type */
    uint64_t interval_us;               /**< Cycle interval in microseconds */
    uint64_t deadline_us;               /**< Maximum time for operations (interval * multiplier) */
    uint32_t priority;                  /**< Priority level (lower = higher priority) */
    bool affects_ordering;              /**< Whether this constraint affects tx ordering */
} mesh_cycle_timing_constraint_t;

/* ============================================================================
 * Stall Event
 * ============================================================================ */

/**
 * @brief Stall event detected from brain cycle coordinator
 *
 * WHAT: Record of a detected cycle stall
 * WHY:  Stalls indicate system issues requiring mesh-coordinated recovery
 * HOW:  Populated when cycle coordinator reports stall, triggers recovery
 */
typedef struct mesh_cycle_stall_event {
    brain_cycle_type_t cycle_type;      /**< Which cycle stalled */
    uint64_t stall_duration_us;         /**< How long the stall lasted */
    uint32_t consecutive_stalls;        /**< Number of consecutive stalls */
    mesh_failure_severity_t severity;   /**< Computed failure severity */
    mesh_recovery_action_type_t recovery_action; /**< Recovery action taken/recommended */
    uint64_t timestamp_ns;              /**< When stall was detected */
} mesh_cycle_stall_event_t;

/* ============================================================================
 * Recovery Status
 * ============================================================================ */

/**
 * @brief Recovery status for a cycle type
 */
typedef struct mesh_cycle_recovery_status {
    brain_cycle_type_t cycle_type;      /**< Cycle type */
    bool recovery_in_progress;          /**< Whether recovery is active */
    mesh_recovery_action_type_t current_action; /**< Current recovery action */
    uint32_t recovery_attempts;         /**< Number of recovery attempts */
    uint64_t recovery_started_ns;       /**< When recovery started */
    bool last_recovery_succeeded;       /**< Whether last recovery succeeded */
} mesh_cycle_recovery_status_t;

/* ============================================================================
 * Health Endorsement
 * ============================================================================ */

/**
 * @brief Health endorsement from cycle coordinator for mesh consensus
 */
typedef struct mesh_cycle_health_endorsement {
    float overall_health;               /**< Overall health score [0-1] */
    float cycle_health[BRAIN_CYCLE_COUNT]; /**< Per-cycle health scores */
    uint32_t healthy_cycles;            /**< Number of healthy cycles */
    uint32_t degraded_cycles;           /**< Number of degraded cycles */
    uint32_t stalled_cycles;            /**< Number of stalled cycles */
    float endorsement_weight;           /**< Weight of this endorsement */
    uint64_t computed_at_ns;            /**< When endorsement was computed */
} mesh_cycle_health_endorsement_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Callback invoked when a cycle stall is detected
 *
 * @param cycle_type The cycle that stalled
 * @param stall_duration_us Duration of the stall in microseconds
 * @param consecutive_stalls Number of consecutive stalls
 * @param user_data User-provided context
 */
typedef void (*mesh_cycle_stall_callback_t)(
    brain_cycle_type_t cycle_type,
    uint64_t stall_duration_us,
    uint32_t consecutive_stalls,
    void* user_data
);

/**
 * @brief Callback invoked when recovery action is taken
 *
 * @param cycle_type The cycle being recovered
 * @param action The recovery action taken
 * @param success Whether the recovery succeeded
 * @param user_data User-provided context
 */
typedef void (*mesh_cycle_recovery_callback_t)(
    brain_cycle_type_t cycle_type,
    mesh_recovery_action_type_t action,
    bool success,
    void* user_data
);

/**
 * @brief Callback invoked when a timing constraint is violated
 *
 * @param cycle_type The cycle whose timing was violated
 * @param expected_us Expected timing in microseconds
 * @param actual_us Actual timing in microseconds
 * @param user_data User-provided context
 */
typedef void (*mesh_cycle_timing_callback_t)(
    brain_cycle_type_t cycle_type,
    uint64_t expected_us,
    uint64_t actual_us,
    void* user_data
);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Initialize config struct with safe defaults
 * WHY:  Provide sensible starting point for configuration
 *
 * @param config Configuration to initialize (must not be NULL)
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
nimcp_error_t mesh_cycle_coordinator_integration_default_config(
    mesh_cycle_coordinator_config_t* config
);

/**
 * @brief Create mesh-cycle coordinator integration
 *
 * WHAT: Create integration between brain cycle coordinator and mesh network
 * WHY:  Enable timing-aware transaction processing and coordinated recovery
 * HOW:  Wire cycle coordinator callbacks to mesh systems
 *
 * @param bootstrap Mesh bootstrap handle (required)
 * @param cycle_coordinator Brain cycle coordinator handle (required)
 * @param config Configuration (NULL for defaults)
 * @return Integration handle or NULL on failure
 */
mesh_cycle_coordinator_integration_t* mesh_cycle_coordinator_integration_create(
    mesh_bootstrap_t* bootstrap,
    brain_cycle_coordinator_t* cycle_coordinator,
    const mesh_cycle_coordinator_config_t* config
);

/**
 * @brief Destroy mesh-cycle coordinator integration
 *
 * WHAT: Clean up integration and release resources
 * WHY:  Proper cleanup prevents resource leaks
 *
 * @param integration Integration to destroy (NULL-safe)
 */
void mesh_cycle_coordinator_integration_destroy(
    mesh_cycle_coordinator_integration_t* integration
);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect to ordering service for timing-aware batching
 *
 * WHAT: Wire integration to ordering service
 * WHY:  Cycle timing informs batch windows and commit deadlines
 *
 * @param integration Integration handle
 * @param ordering_service Ordering service handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_cycle_coordinator_connect_ordering(
    mesh_cycle_coordinator_integration_t* integration,
    mesh_ordering_service_t* ordering_service
);

/**
 * @brief Connect to resilience integration for stall recovery
 *
 * WHAT: Wire integration to resilience system
 * WHY:  Cycle stalls trigger mesh recovery actions
 *
 * @param integration Integration handle
 * @param resilience Resilience integration handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_cycle_coordinator_connect_resilience(
    mesh_cycle_coordinator_integration_t* integration,
    mesh_resilience_integration_t* resilience
);

/**
 * @brief Connect to health bridge for distributed health
 *
 * WHAT: Wire integration to health bridge
 * WHY:  Cycle health contributes to mesh health consensus
 *
 * @param integration Integration handle
 * @param health_bridge Health bridge handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_cycle_coordinator_connect_health_bridge(
    mesh_cycle_coordinator_integration_t* integration,
    mesh_health_bridge_t* health_bridge
);

/**
 * @brief Connect to exception bridge for immune routing
 *
 * WHAT: Wire integration to exception bridge
 * WHY:  Cycle errors route through mesh to immune system
 *
 * @param integration Integration handle
 * @param exception_bridge Exception bridge handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_cycle_coordinator_connect_exception_bridge(
    mesh_cycle_coordinator_integration_t* integration,
    mesh_exception_bridge_t* exception_bridge
);

/**
 * @brief Connect to MSP for BBB validation
 *
 * WHAT: Wire integration to MSP for security validation
 * WHY:  Cycle-critical transactions may require BBB validation
 *
 * @param integration Integration handle
 * @param msp MSP handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_cycle_coordinator_connect_msp(
    mesh_cycle_coordinator_integration_t* integration,
    mesh_msp_t* msp
);

/* ============================================================================
 * Timing API
 * ============================================================================ */

/**
 * @brief Get timing constraint for a cycle type
 *
 * WHAT: Retrieve timing parameters derived from cycle
 * WHY:  External systems need timing info for coordination
 *
 * @param integration Integration handle
 * @param cycle_type Brain cycle type
 * @param constraint_out Output timing constraint
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_cycle_coordinator_get_timing_constraint(
    const mesh_cycle_coordinator_integration_t* integration,
    brain_cycle_type_t cycle_type,
    mesh_cycle_timing_constraint_t* constraint_out
);

/**
 * @brief Get optimal batch window for transaction ordering
 *
 * WHAT: Compute batch window based on active cycle timings
 * WHY:  Batching should respect the fastest active cycle
 *
 * @param integration Integration handle
 * @return Optimal batch window in microseconds, or 0 on error
 */
uint64_t mesh_cycle_coordinator_get_batch_window(
    const mesh_cycle_coordinator_integration_t* integration
);

/**
 * @brief Get commit deadline for transaction validation
 *
 * WHAT: Compute commit deadline based on cycle constraints
 * WHY:  Commits should complete within cycle constraints
 *
 * @param integration Integration handle
 * @return Commit deadline in microseconds, or 0 on error
 */
uint64_t mesh_cycle_coordinator_get_commit_deadline(
    const mesh_cycle_coordinator_integration_t* integration
);

/**
 * @brief Notify that a transaction was timed
 *
 * WHAT: Record timing information for a transaction
 * WHY:  Track whether transactions meet timing constraints
 *
 * @param integration Integration handle
 * @param tx Transaction that was timed
 * @param timing_met Whether timing constraints were met
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_cycle_coordinator_notify_transaction_timed(
    mesh_cycle_coordinator_integration_t* integration,
    const mesh_transaction_t* tx,
    bool timing_met
);

/* ============================================================================
 * Stall Recovery API
 * ============================================================================ */

/**
 * @brief Handle cycle stall notification
 *
 * WHAT: Process stall detected by cycle coordinator
 * WHY:  Stalls may require mesh-coordinated recovery
 * HOW:  Evaluate severity, potentially trigger recovery
 *
 * @param integration Integration handle
 * @param cycle_type Which cycle stalled
 * @param stall_duration_us Duration of the stall
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_cycle_coordinator_on_stall(
    mesh_cycle_coordinator_integration_t* integration,
    brain_cycle_type_t cycle_type,
    uint64_t stall_duration_us
);

/**
 * @brief Request recovery action for a cycle
 *
 * WHAT: Explicitly request recovery through mesh
 * WHY:  External systems may detect issues requiring recovery
 *
 * @param integration Integration handle
 * @param cycle_type Cycle to recover
 * @param action Requested recovery action
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_cycle_coordinator_request_recovery(
    mesh_cycle_coordinator_integration_t* integration,
    brain_cycle_type_t cycle_type,
    mesh_recovery_action_type_t action
);

/**
 * @brief Get recovery status for a cycle
 *
 * WHAT: Query current recovery state
 * WHY:  External systems need to know if recovery is in progress
 *
 * @param integration Integration handle
 * @param cycle_type Cycle type to query
 * @param status_out Output recovery status
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_cycle_coordinator_get_recovery_status(
    const mesh_cycle_coordinator_integration_t* integration,
    brain_cycle_type_t cycle_type,
    mesh_cycle_recovery_status_t* status_out
);

/* ============================================================================
 * Health API
 * ============================================================================ */

/**
 * @brief Get health endorsement for mesh consensus
 *
 * WHAT: Generate health endorsement from cycle coordinator state
 * WHY:  Cycle health contributes to distributed health consensus
 *
 * @param integration Integration handle
 * @param endorsement_out Output health endorsement
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_cycle_coordinator_get_health_endorsement(
    const mesh_cycle_coordinator_integration_t* integration,
    mesh_cycle_health_endorsement_t* endorsement_out
);

/**
 * @brief Contribute health score for a channel
 *
 * WHAT: Send health score to mesh for a specific channel
 * WHY:  Distributed health consensus across channels
 *
 * @param integration Integration handle
 * @param channel Channel to contribute health for
 * @param health_score Health score [0-1]
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_cycle_coordinator_contribute_health(
    mesh_cycle_coordinator_integration_t* integration,
    mesh_channel_id_t channel,
    float health_score
);

/**
 * @brief Get aggregate health from mesh consensus
 *
 * WHAT: Query the aggregated health from mesh network
 * WHY:  Get system-wide health view from distributed consensus
 *
 * @param integration Integration handle
 * @return Aggregate health score [0-1], or -1.0 on error
 */
float mesh_cycle_coordinator_get_aggregate_health(
    const mesh_cycle_coordinator_integration_t* integration
);

/* ============================================================================
 * Callback Registration API
 * ============================================================================ */

/**
 * @brief Register stall detection callback
 *
 * @param integration Integration handle
 * @param callback Callback function
 * @param user_data User context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_cycle_coordinator_set_stall_callback(
    mesh_cycle_coordinator_integration_t* integration,
    mesh_cycle_stall_callback_t callback,
    void* user_data
);

/**
 * @brief Register recovery action callback
 *
 * @param integration Integration handle
 * @param callback Callback function
 * @param user_data User context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_cycle_coordinator_set_recovery_callback(
    mesh_cycle_coordinator_integration_t* integration,
    mesh_cycle_recovery_callback_t callback,
    void* user_data
);

/**
 * @brief Register timing violation callback
 *
 * @param integration Integration handle
 * @param callback Callback function
 * @param user_data User context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_cycle_coordinator_set_timing_callback(
    mesh_cycle_coordinator_integration_t* integration,
    mesh_cycle_timing_callback_t callback,
    void* user_data
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get integration statistics
 *
 * WHAT: Retrieve all integration statistics
 * WHY:  Monitoring, debugging, performance analysis
 *
 * @param integration Integration handle
 * @param stats_out Output statistics
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_cycle_coordinator_get_stats(
    const mesh_cycle_coordinator_integration_t* integration,
    mesh_cycle_coordinator_stats_t* stats_out
);

/**
 * @brief Reset integration statistics
 *
 * WHAT: Reset all statistics counters to zero
 * WHY:  Fresh start for a new measurement period
 *
 * @param integration Integration handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_cycle_coordinator_reset_stats(
    mesh_cycle_coordinator_integration_t* integration
);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update integration state
 *
 * WHAT: Perform periodic update of integration
 * WHY:  Process pending events, check timeouts, aggregate health
 *
 * @param integration Integration handle
 * @param delta_ms Time since last update in milliseconds
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_cycle_coordinator_update(
    mesh_cycle_coordinator_integration_t* integration,
    uint64_t delta_ms
);

/* ============================================================================
 * Bootstrap Integration
 * ============================================================================ */

/**
 * @brief Get mesh-cycle coordinator integration from bootstrap
 *
 * @param bootstrap Bootstrap handle
 * @return Integration handle or NULL if not created
 */
mesh_cycle_coordinator_integration_t* mesh_bootstrap_get_cycle_coordinator_integration(
    mesh_bootstrap_t* bootstrap
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert cycle type to timing priority
 *
 * WHAT: Map brain cycle type to ordering priority
 * WHY:  Fast cycles (oscillations) have higher priority than slow (circadian)
 *
 * @param cycle_type Brain cycle type
 * @return Priority (0 = highest)
 */
uint32_t mesh_cycle_get_timing_priority(brain_cycle_type_t cycle_type);

/**
 * @brief Compute severity from stall characteristics
 *
 * WHAT: Map stall duration and count to failure severity
 * WHY:  Consistent severity assignment for recovery decisions
 *
 * @param stall_duration_us Duration of stall
 * @param consecutive_stalls Number of consecutive stalls
 * @param expected_interval_us Expected cycle interval
 * @return Failure severity level
 */
mesh_failure_severity_t mesh_cycle_compute_stall_severity(
    uint64_t stall_duration_us,
    uint32_t consecutive_stalls,
    uint64_t expected_interval_us
);

/**
 * @brief Print integration status
 *
 * @param integration Integration handle
 */
void mesh_cycle_coordinator_print_status(
    const mesh_cycle_coordinator_integration_t* integration
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_CYCLE_COORDINATOR_H */
