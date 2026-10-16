/**
 * @file nimcp_cognitive_recovery.h
 * @brief Cognitive Recovery Coordinator - End-to-End Brain-Driven Recovery
 *
 * WHAT: High-level coordinator integrating all brain-driven recovery components
 * WHY:  Provide simple API for complete cognitive recovery workflow
 * HOW:  Orchestrate diagnostics → brain analysis → adaptation → learning
 *
 * COGNITIVE RECOVERY WORKFLOW:
 *
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    ERROR DETECTED                                │
 * └───────────────────┬─────────────────────────────────────────────┘
 *                     │
 *                     ▼
 * ┌─────────────────────────────────────────────────────────────────┐
 * │  DIAGNOSTICS: Analyze error, generate diagnostic result         │
 * └───────────────────┬─────────────────────────────────────────────┘
 *                     │
 *                     ▼
 * ┌─────────────────────────────────────────────────────────────────┐
 * │  BRAIN ANALYSIS:                                                 │
 * │  - Working Memory: Check for similar past failures               │
 * │  - Executive Function: Evaluate recovery options                 │
 * │  - Reasoning: Predict success probability                        │
 * │  - Knowledge: Retrieve domain expertise                          │
 * └───────────────────┬─────────────────────────────────────────────┘
 *                     │
 *                     ▼
 * ┌─────────────────────────────────────────────────────────────────┐
 * │  STRATEGY SELECTION: Brain chooses optimal recovery strategy    │
 * └───────────────────┬─────────────────────────────────────────────┘
 *                     │
 *                     ▼
 * ┌─────────────────────────────────────────────────────────────────┐
 * │  RUNTIME ADAPTATION: Apply parameter adjustments                │
 * │  (Learning rate, batch size, features, etc.)                     │
 * └───────────────────┬─────────────────────────────────────────────┘
 *                     │
 *                     ▼
 * ┌─────────────────────────────────────────────────────────────────┐
 * │  EXECUTE RECOVERY: Run selected strategy                        │
 * └───────────────────┬─────────────────────────────────────────────┘
 *                     │
 *                     ▼
 * ┌─────────────────────────────────────────────────────────────────┐
 * │  LEARN OUTCOME:                                                  │
 * │  - Store result in episodic memory                               │
 * │  - Update strategy success rates                                 │
 * │  - Adjust future predictions                                     │
 * │  - Extract generalizable patterns                                │
 * └───────────────────┬─────────────────────────────────────────────┘
 *                     │
 *                     ▼
 * ┌─────────────────────────────────────────────────────────────────┐
 * │  MONITOR & VERIFY: Confirm recovery success                     │
 * └─────────────────────────────────────────────────────────────────┘
 *
 * @author NIMCP Team
 * @date 2025-11-19
 * @version 1.0.0
 */

#ifndef NIMCP_COGNITIVE_RECOVERY_H
#define NIMCP_COGNITIVE_RECOVERY_H

#include <stdint.h>
#include <stdbool.h>

// Import all fault tolerance components
#include "utils/fault_tolerance/nimcp_brain_recovery_integration.h"
#include "utils/fault_tolerance/nimcp_runtime_adaptation.h"
#include "utils/fault_tolerance/nimcp_recovery.h"
#include "utils/fault_tolerance/nimcp_diagnostics.h"
#include "utils/fault_tolerance/nimcp_health_monitor.h"

#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Cognitive Recovery Coordinator
//=============================================================================

/**
 * @brief Opaque cognitive recovery coordinator handle
 *
 * WHAT: Central coordinator for brain-driven recovery
 * WHY:  Simplify integration, provide unified API
 * HOW:  Manages all recovery subsystems
 */
typedef struct cognitive_recovery_coordinator_internal* cognitive_recovery_coordinator_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Cognitive recovery configuration
 */
typedef struct {
    // Health Monitoring
    bool enable_health_monitoring;     /**< Continuous health monitoring */
    uint32_t health_check_interval_ms; /**< Health check frequency */
    float health_threshold;            /**< Minimum healthy score (0-100) */

    // Brain Integration
    bool enable_brain_decisions;       /**< Use brain for strategy selection */
    float brain_confidence_threshold;  /**< Minimum confidence for brain decisions */
    bool enable_learning;              /**< Learn from recovery outcomes */

    // Runtime Adaptation
    bool enable_auto_adaptation;       /**< Automatic parameter adjustment */
    bool conservative_adaptation;      /**< Use conservative adjustments */

    // Recovery Policies
    bool enable_immediate_tier;        /**< Enable immediate (<1ms) recovery */
    bool enable_tactical_tier;         /**< Enable tactical (<100ms) recovery */
    bool enable_strategic_tier;        /**< Enable strategic (<1s) recovery */
    bool enable_preventive_tier;       /**< Enable preventive recovery */

    // Safety & Limits
    uint32_t max_recovery_attempts;    /**< Max attempts before giving up */
    uint32_t recovery_timeout_ms;      /**< Timeout for recovery operations */
    bool require_user_confirmation;    /**< Require confirmation for risky actions */

    // Logging & Debugging
    bool verbose_logging;              /**< Enable detailed logging */
    const char* log_file_path;         /**< Path to recovery log file */
} cognitive_recovery_config_t;

/**
 * @brief Get default cognitive recovery configuration
 *
 * @param config Output configuration structure
 */
void cognitive_recovery_default_config(cognitive_recovery_config_t* config);

//=============================================================================
// Initialization & Lifecycle
//=============================================================================

/**
 * @brief Create cognitive recovery coordinator
 *
 * WHAT: Initialize complete brain-driven recovery system
 * WHY:  Enable intelligent, adaptive recovery
 * HOW:  Initialize all subsystems:
 *   - Health monitor for continuous monitoring
 *   - Brain recovery integration for cognitive decisions
 *   - Runtime adaptation for parameter tuning
 *   - Diagnostic system for failure analysis
 *   - Recovery engine for execution
 *
 * @param brain Brain instance
 * @param config Configuration (NULL for defaults)
 * @return Coordinator handle, NULL on error
 *
 * COMPLEXITY: O(1) initialization
 * THREAD-SAFE: Yes
 * MALLOC: Yes (coordinator + subsystems)
 */
cognitive_recovery_coordinator_t cognitive_recovery_create(
    brain_t brain,
    cognitive_recovery_config_t* config
);

/**
 * @brief Destroy cognitive recovery coordinator
 *
 * WHAT: Clean shutdown, save learned knowledge
 * WHY:  Graceful cleanup, preserve learning
 * HOW:  Save recovery history, free all subsystems
 *
 * @param coordinator Coordinator handle (can be NULL)
 *
 * COMPLEXITY: O(n) where n = learned patterns
 * THREAD-SAFE: No (exclusive access required)
 */
void cognitive_recovery_destroy(cognitive_recovery_coordinator_t coordinator);

/**
 * @brief Start cognitive recovery system
 *
 * WHAT: Begin health monitoring and background recovery
 * WHY:  Enable proactive recovery
 * HOW:  Start health monitor thread, register signal handlers
 *
 * @param coordinator Coordinator handle
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
bool cognitive_recovery_start(cognitive_recovery_coordinator_t coordinator);

/**
 * @brief Stop cognitive recovery system
 *
 * @param coordinator Coordinator handle
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
bool cognitive_recovery_stop(cognitive_recovery_coordinator_t coordinator);

//=============================================================================
// Main Recovery API
//=============================================================================

/**
 * @brief Complete recovery result
 */
typedef struct {
    bool success;                      /**< Overall success */
    recovery_tier_t tier_used;         /**< Tier that succeeded */
    recovery_action_t action_taken;    /**< Action executed */

    uint32_t total_time_us;            /**< Total recovery time */
    uint32_t num_attempts;             /**< Attempts before success */

    brain_recovery_decision_t* brain_decision; /**< Brain's decision (if used) */
    recovery_result_t* recovery_result; /**< Detailed recovery result */

    uint32_t num_parameters_adjusted;  /**< Parameters modified */
    parameter_adjustment_t* adjustments; /**< Parameter changes made */

    char summary[512];                 /**< Human-readable summary */
} cognitive_recovery_result_t;

/**
 * @brief Execute complete cognitive recovery workflow
 *
 * WHAT: Full brain-driven recovery from detection to learning
 * WHY:  Single API for intelligent, adaptive recovery
 * HOW:
 *   1. Run diagnostics on failure
 *   2. Query brain for strategy selection
 *   3. Apply runtime adaptations
 *   4. Execute recovery strategy
 *   5. Verify success
 *   6. Learn from outcome
 *
 * COGNITIVE FLOW:
 * - Working Memory: "Have I seen this failure before?"
 * - Executive Function: "What are my options?"
 * - Reasoning: "What's most likely to work?"
 * - Decision: "I'll try strategy X"
 * - Execution: Apply strategy
 * - Learning: "Did it work? Update my knowledge"
 *
 * @param coordinator Coordinator handle
 * @param diagnosis Optional pre-computed diagnosis (NULL = auto-diagnose)
 * @return Recovery result (caller must free)
 *
 * COMPLEXITY: O(log n + k) where n=history, k=strategy evaluation
 * THREAD-SAFE: No
 * MALLOC: Yes (result structure)
 */
cognitive_recovery_result_t* cognitive_recovery_execute(
    cognitive_recovery_coordinator_t coordinator,
    diagnostic_result_t* diagnosis
);

/**
 * @brief Free cognitive recovery result
 *
 * @param result Result to free (can be NULL)
 */
void cognitive_recovery_free_result(cognitive_recovery_result_t* result);

//=============================================================================
// Targeted Recovery APIs
//=============================================================================

/**
 * @brief Recover from specific error type
 *
 * WHAT: Recovery tailored to error type
 * WHY:  Different errors need different approaches
 * HOW:  Use error type to select appropriate strategy
 *
 * @param coordinator Coordinator handle
 * @param error_type Type of error
 * @param context Error context (optional)
 * @return Recovery result
 *
 * COMPLEXITY: O(1) + recovery complexity
 * THREAD-SAFE: No
 */
cognitive_recovery_result_t* cognitive_recovery_from_error(
    cognitive_recovery_coordinator_t coordinator,
    error_type_t error_type,
    void* context
);

/**
 * @brief Recover from signal (crash)
 *
 * WHAT: Handle crash signals (SIGSEGV, SIGFPE, etc.)
 * WHY:  Crash recovery requires special handling
 * HOW:  Analyze crash context, attempt recovery or graceful shutdown
 *
 * @param coordinator Coordinator handle
 * @param signal Signal number
 * @param crash_ctx Crash context from signal handler
 * @return Recovery result
 *
 * COMPLEXITY: Depends on recovery strategy
 * THREAD-SAFE: Signal-safe (limited allocation)
 */
cognitive_recovery_result_t* cognitive_recovery_from_signal(
    cognitive_recovery_coordinator_t coordinator,
    int signal,
    crash_context_t* crash_ctx
);

/**
 * @brief Preventive recovery based on health degradation
 *
 * WHAT: Proactive recovery before failure
 * WHY:  Prevention better than cure
 * HOW:  Monitor health, trigger recovery on degradation
 *
 * @param coordinator Coordinator handle
 * @param health Current health status
 * @return Recovery result
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
cognitive_recovery_result_t* cognitive_recovery_preventive(
    cognitive_recovery_coordinator_t coordinator,
    health_status_snapshot_t* health
);

//=============================================================================
// Health & Monitoring
//=============================================================================

/**
 * @brief Get current system health
 *
 * @param coordinator Coordinator handle
 * @param health Output health status
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool cognitive_recovery_get_health(
    cognitive_recovery_coordinator_t coordinator,
    health_status_snapshot_t* health
);

/**
 * @brief Check if recovery is needed
 *
 * WHAT: Determine if proactive recovery should run
 * WHY:  Detect degradation before failure
 * HOW:  Check health score, anomalies, predictions
 *
 * @param coordinator Coordinator handle
 * @return true if recovery recommended
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool cognitive_recovery_is_needed(cognitive_recovery_coordinator_t coordinator);

/**
 * @brief Get recovery readiness
 *
 * WHAT: Check if recovery system is ready
 * WHY:  Ensure recovery can execute
 * HOW:  Verify all subsystems initialized and healthy
 *
 * @param coordinator Coordinator handle
 * @return true if ready for recovery
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool cognitive_recovery_is_ready(cognitive_recovery_coordinator_t coordinator);

//=============================================================================
// Learning & Analytics
//=============================================================================

/**
 * @brief Cognitive recovery statistics
 */
typedef struct {
    // Recovery Metrics
    uint32_t total_recoveries;         /**< Total recovery attempts */
    uint32_t successful_recoveries;    /**< Successful recoveries */
    uint32_t failed_recoveries;        /**< Failed recoveries */
    float success_rate;                /**< Overall success rate */

    // Brain Decision Metrics
    uint32_t brain_decisions;          /**< Decisions made by brain */
    uint32_t brain_correct;            /**< Brain predictions correct */
    float brain_accuracy;              /**< Brain prediction accuracy */
    float avg_brain_confidence;        /**< Average decision confidence */

    // Adaptation Metrics
    uint32_t parameters_adjusted;      /**< Total parameter adjustments */
    uint32_t features_toggled;         /**< Features enabled/disabled */
    uint32_t policies_applied;         /**< Adaptation policies applied */

    // Performance Metrics
    uint32_t avg_recovery_time_us;     /**< Average recovery time */
    uint32_t fastest_recovery_us;      /**< Fastest recovery */
    uint32_t slowest_recovery_us;      /**< Slowest recovery */

    // Learning Metrics
    uint32_t patterns_learned;         /**< Unique failure patterns */
    uint32_t novel_failures;           /**< Never-seen-before failures */
    uint32_t recurring_failures;       /**< Repeated failures */
} cognitive_recovery_stats_t;

/**
 * @brief Get cognitive recovery statistics
 *
 * @param coordinator Coordinator handle
 * @param stats Output statistics structure
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool cognitive_recovery_get_stats(
    cognitive_recovery_coordinator_t coordinator,
    cognitive_recovery_stats_t* stats
);

/**
 * @brief Get learned recovery patterns
 *
 * @param coordinator Coordinator handle
 * @param patterns Output array for patterns
 * @param max_patterns Maximum patterns to return
 * @return Number of patterns returned
 *
 * COMPLEXITY: O(n log n) for sorting
 * THREAD-SAFE: Yes
 */
uint32_t cognitive_recovery_get_learned_patterns(
    cognitive_recovery_coordinator_t coordinator,
    recovery_pattern_t* patterns,
    uint32_t max_patterns
);

//=============================================================================
// Configuration & Tuning
//=============================================================================

/**
 * @brief Update configuration at runtime
 *
 * @param coordinator Coordinator handle
 * @param config New configuration
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
bool cognitive_recovery_update_config(
    cognitive_recovery_coordinator_t coordinator,
    cognitive_recovery_config_t* config
);

/**
 * @brief Get current configuration
 *
 * @param coordinator Coordinator handle
 * @param config Output configuration structure
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool cognitive_recovery_get_config(
    cognitive_recovery_coordinator_t coordinator,
    cognitive_recovery_config_t* config
);

//=============================================================================
// Persistence
//=============================================================================

/**
 * @brief Save cognitive recovery state
 *
 * WHAT: Persist all learned knowledge and history
 * WHY:  Resume learning across sessions
 * HOW:  Save:
 *   - Learned recovery patterns
 *   - Recovery history
 *   - Parameter configurations
 *   - Statistics
 *
 * @param coordinator Coordinator handle
 * @param filepath Output file path
 * @return true on success
 *
 * COMPLEXITY: O(n) where n = patterns + history
 * THREAD-SAFE: Yes
 */
bool cognitive_recovery_save(
    cognitive_recovery_coordinator_t coordinator,
    const char* filepath
);

/**
 * @brief Load cognitive recovery state
 *
 * @param brain Brain to associate with
 * @param filepath Input file path
 * @param config Configuration (NULL for saved config)
 * @return Coordinator handle or NULL on error
 *
 * COMPLEXITY: O(n)
 * THREAD-SAFE: Yes
 */
cognitive_recovery_coordinator_t cognitive_recovery_load(
    brain_t brain,
    const char* filepath,
    cognitive_recovery_config_t* config
);

//=============================================================================
// Reporting & Debugging
//=============================================================================

/**
 * @brief Generate comprehensive recovery report
 *
 * @param coordinator Coordinator handle
 * @param output Output stream (stdout, file, etc.)
 *
 * COMPLEXITY: O(n)
 * THREAD-SAFE: Yes
 */
void cognitive_recovery_report(
    cognitive_recovery_coordinator_t coordinator,
    FILE* output
);

/**
 * @brief Export recovery data to JSON
 *
 * @param coordinator Coordinator handle
 * @param json_buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written, -1 on error
 *
 * COMPLEXITY: O(n)
 * THREAD-SAFE: Yes
 */
int32_t cognitive_recovery_export_json(
    cognitive_recovery_coordinator_t coordinator,
    char* json_buffer,
    size_t buffer_size
);

//=============================================================================
// Signal Handler Integration
//=============================================================================

/**
 * @brief Install signal handlers for cognitive recovery
 *
 * WHAT: Register signal handlers for crash recovery
 * WHY:  Catch crashes and attempt intelligent recovery
 * HOW:  Install handlers for SIGSEGV, SIGFPE, SIGABRT, etc.
 *
 * SIGNALS HANDLED:
 * - SIGSEGV: Segmentation fault
 * - SIGFPE: Floating point exception
 * - SIGABRT: Abort signal
 * - SIGBUS: Bus error
 * - SIGILL: Illegal instruction
 *
 * @param coordinator Coordinator handle
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
bool cognitive_recovery_install_signal_handlers(
    cognitive_recovery_coordinator_t coordinator
);

/**
 * @brief Uninstall signal handlers
 *
 * @param coordinator Coordinator handle
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
void cognitive_recovery_uninstall_signal_handlers(
    cognitive_recovery_coordinator_t coordinator
);

/**
 * @brief Install the crash-log backtrace handler WITHOUT a cognitive
 *        recovery coordinator. Writes a signal-safe backtrace to
 *        /var/log/nimcp_crash.log (fallback /tmp/nimcp_crash.log) on
 *        any SIGSEGV/SIGABRT/SIGBUS/SIGFPE/SIGILL, then exits.
 *
 * Safe to call at library init. Idempotent — subsequent calls are
 * no-ops if handlers are already installed. If a cognitive_recovery
 * coordinator is later installed, its handler replaces this one and
 * still provides the same backtrace dump plus recovery attempt.
 *
 * @return true on success (handlers installed), false on failure.
 */
bool nimcp_install_crash_handler(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_COGNITIVE_RECOVERY_H
