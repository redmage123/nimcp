/**
 * @file nimcp_heal_bridge.h
 * @brief Enhanced Self-Healing Bridge - Unified Crash Recovery Pipeline
 * @version 1.0.0
 * @date 2025-12-27
 *
 * WHAT: Unified bridge connecting self-healing engine with code immune system,
 *       adding validation sandbox, pattern evolution, and multi-step fix chains.
 * WHY:  Enable end-to-end automated crash recovery with verification and learning
 * HOW:  Coordinate self_heal, code_immune, hot_inject, and recompiler modules
 *
 * BIOLOGICAL BASIS:
 * ```
 * BIOLOGICAL CONCEPT              NIMCP IMPLEMENTATION
 * ─────────────────────────────────────────────────────────────────
 * Adaptive Immune Response     -> Full pipeline: detect -> analyze -> fix -> validate
 * Germinal Center Reaction     -> Pattern evolution from successful fixes
 * Affinity Maturation          -> Fix confidence improvement over time
 * Clonal Selection             -> Best fix candidate selection
 * Immune Tolerance             -> Validation sandbox prevents autoimmune damage
 * Complement Cascade           -> Multi-step fix chains for complex crashes
 * ```
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                     ENHANCED SELF-HEALING BRIDGE                          ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                    CRASH DETECTION                                  │  ║
 * ║   │        Signal Handler → Code Immune → Antigen Presentation         │  ║
 * ║   └────────────────────────────┬───────────────────────────────────────┘  ║
 * ║                                ▼                                          ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                    FIX GENERATION                                   │  ║
 * ║   │   Self-Heal Engine (Pattern + LNN) → Fix Candidates                 │  ║
 * ║   └────────────────────────────┬───────────────────────────────────────┘  ║
 * ║                                ▼                                          ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                    VALIDATION SANDBOX                               │  ║
 * ║   │   Fork → Apply Fix → Test → Report Success/Failure                  │  ║
 * ║   └────────────────────────────┬───────────────────────────────────────┘  ║
 * ║                                ▼                                          ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                    ANTIBODY PRODUCTION                              │  ║
 * ║   │   Code Immune B-Cell → Antibody → Hot Inject                        │  ║
 * ║   └────────────────────────────┬───────────────────────────────────────┘  ║
 * ║                                ▼                                          ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                    PATTERN EVOLUTION                                │  ║
 * ║   │   Successful LNN Fix → New Pattern → Memory B-Cell                  │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HEAL_BRIDGE_H
#define NIMCP_HEAL_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/immune/nimcp_self_heal.h"
#include "cognitive/immune/nimcp_code_immune.h"
#include "cognitive/immune/nimcp_heal_patterns.h"
#include "utils/bridge/nimcp_bridge_base.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define HEAL_BRIDGE_MAX_FIX_CHAIN        8    /**< Max fixes in a chain */
#define HEAL_BRIDGE_SANDBOX_TIMEOUT_MS   5000 /**< Sandbox test timeout */
#define HEAL_BRIDGE_EVOLUTION_THRESHOLD  3    /**< Successes before pattern evolution */
#define HEAL_BRIDGE_ROLLBACK_WINDOW_MS   60000 /**< Time window for rollback */
#define HEAL_BRIDGE_MODULE_NAME          "heal_bridge"

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct heal_bridge_s heal_bridge_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Fix chain dependency type
 */
typedef enum {
    FIX_DEP_NONE = 0,          /**< No dependency */
    FIX_DEP_SEQUENTIAL,        /**< Must apply in order */
    FIX_DEP_PREREQUISITE,      /**< Previous fix must succeed */
    FIX_DEP_MUTEX,             /**< Only one can be applied */
    FIX_DEP_COMPLEMENTARY      /**< Works better together */
} fix_dependency_t;

/**
 * @brief Sandbox validation result
 */
typedef enum {
    SANDBOX_RESULT_SUCCESS = 0,    /**< Fix validated successfully */
    SANDBOX_RESULT_CRASH,          /**< Fix caused crash in sandbox */
    SANDBOX_RESULT_TIMEOUT,        /**< Sandbox test timed out */
    SANDBOX_RESULT_REGRESSION,     /**< Fix caused regression */
    SANDBOX_RESULT_COMPILE_ERROR,  /**< Fix failed to compile */
    SANDBOX_RESULT_LOAD_ERROR,     /**< Fix failed to load */
    SANDBOX_RESULT_SKIPPED         /**< Validation skipped */
} sandbox_result_t;

/**
 * @brief Pattern evolution state
 */
typedef enum {
    PATTERN_EVO_CANDIDATE = 0,     /**< Potential new pattern */
    PATTERN_EVO_TESTING,           /**< Being tested for promotion */
    PATTERN_EVO_PROMOTED,          /**< Promoted to pattern library */
    PATTERN_EVO_REJECTED,          /**< Rejected (too low success rate) */
    PATTERN_EVO_DEPRECATED         /**< Deprecated (superseded) */
} pattern_evolution_state_t;

/**
 * @brief Fix chain execution status
 */
typedef enum {
    CHAIN_STATUS_PENDING = 0,      /**< Chain not started */
    CHAIN_STATUS_IN_PROGRESS,      /**< Chain executing */
    CHAIN_STATUS_COMPLETE,         /**< All fixes applied successfully */
    CHAIN_STATUS_PARTIAL,          /**< Some fixes applied */
    CHAIN_STATUS_FAILED,           /**< Chain failed, rolled back */
    CHAIN_STATUS_ROLLBACK          /**< Rollback in progress */
} chain_status_t;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

/**
 * @brief Single fix in a chain
 */
typedef struct {
    heal_result_t fix;                   /**< The fix itself */
    fix_dependency_t dependency;          /**< Dependency on previous fix */
    uint32_t depends_on_idx;              /**< Index of prerequisite fix */
    bool applied;                         /**< Has been applied */
    bool validated;                       /**< Passed sandbox validation */
    sandbox_result_t sandbox_result;      /**< Validation result */
    uint64_t antibody_id;                 /**< Code immune antibody ID */
} chain_fix_t;

/**
 * @brief Multi-step fix chain
 */
typedef struct {
    uint64_t id;                          /**< Chain ID */
    uint64_t antigen_id;                  /**< Target crash antigen */
    chain_fix_t fixes[HEAL_BRIDGE_MAX_FIX_CHAIN]; /**< Fixes in chain */
    size_t fix_count;                     /**< Number of fixes */
    chain_status_t status;                /**< Chain status */
    size_t applied_count;                 /**< Fixes successfully applied */
    uint64_t start_time;                  /**< When chain started */
    uint64_t complete_time;               /**< When chain completed */
    float overall_confidence;             /**< Combined confidence */
} fix_chain_t;

/**
 * @brief Pattern evolution candidate
 */
typedef struct {
    uint64_t id;                          /**< Candidate ID */
    crash_features_t features;            /**< Crash features that triggered */
    heal_result_t fix_template;           /**< LNN-generated fix */
    pattern_evolution_state_t state;      /**< Evolution state */
    uint32_t success_count;               /**< Successful applications */
    uint32_t failure_count;               /**< Failed applications */
    float avg_confidence;                 /**< Average confidence score */
    uint64_t first_seen;                  /**< First occurrence */
    uint64_t last_seen;                   /**< Most recent occurrence */
    uint32_t promoted_pattern_id;         /**< ID if promoted to pattern */
} pattern_candidate_t;

/**
 * @brief Sandbox validation context
 */
typedef struct {
    pid_t sandbox_pid;                    /**< Sandbox process PID */
    int result_pipe[2];                   /**< Result communication pipe */
    uint64_t timeout_ms;                  /**< Test timeout */
    bool use_fork;                        /**< Use fork isolation */
    bool run_regression;                  /**< Run regression tests */
    char test_command[512];               /**< Custom test command */
} sandbox_context_t;

/**
 * @brief Rollback entry for applied fixes
 */
typedef struct {
    uint64_t antibody_id;                 /**< Applied antibody */
    uint64_t apply_time;                  /**< When applied */
    char original_code[4096];             /**< Original code backup */
    void* original_function;              /**< Original function pointer */
    bool can_rollback;                    /**< Rollback possible */
} rollback_entry_t;

/**
 * @brief Heal bridge configuration
 */
typedef struct {
    /* Validation settings */
    bool enable_sandbox;                  /**< Enable sandbox validation */
    uint64_t sandbox_timeout_ms;          /**< Sandbox timeout */
    bool require_validation;              /**< Require validation before apply */

    /* Pattern evolution settings */
    bool enable_pattern_evolution;        /**< Enable pattern evolution */
    uint32_t evolution_threshold;         /**< Successes before promotion */
    float min_promotion_confidence;       /**< Min confidence for promotion */

    /* Fix chain settings */
    bool enable_fix_chains;               /**< Enable multi-step fixes */
    size_t max_chain_length;              /**< Max fixes per chain */
    bool atomic_chains;                   /**< Rollback all on failure */

    /* Rollback settings */
    bool enable_rollback;                 /**< Enable fix rollback */
    uint64_t rollback_window_ms;          /**< Time window for rollback */
    size_t max_rollback_entries;          /**< Max rollback history */

    /* Integration */
    bool auto_produce_antibodies;         /**< Auto-create antibodies */
    bool sync_with_brain_immune;          /**< Sync with brain immune */
    bool enable_logging;                  /**< Enable detailed logging */
} heal_bridge_config_t;

/**
 * @brief Heal bridge statistics
 */
typedef struct {
    /* Pipeline stats */
    uint64_t crashes_received;            /**< Crashes from code_immune */
    uint64_t fixes_generated;             /**< Fixes from self_heal */
    uint64_t fixes_validated;             /**< Fixes that passed sandbox */
    uint64_t fixes_applied;               /**< Fixes successfully applied */
    uint64_t fixes_failed;                /**< Fixes that failed */

    /* Sandbox stats */
    uint64_t sandbox_runs;                /**< Total sandbox tests */
    uint64_t sandbox_successes;           /**< Sandbox passes */
    uint64_t sandbox_crashes;             /**< Fixes that crashed sandbox */
    uint64_t sandbox_timeouts;            /**< Sandbox timeouts */
    uint64_t sandbox_regressions;         /**< Detected regressions */

    /* Pattern evolution stats */
    uint64_t patterns_evolved;            /**< LNN fixes promoted to patterns */
    uint64_t candidates_active;           /**< Active evolution candidates */
    uint64_t candidates_rejected;         /**< Rejected candidates */

    /* Fix chain stats */
    uint64_t chains_created;              /**< Fix chains created */
    uint64_t chains_completed;            /**< Chains fully applied */
    uint64_t chains_partial;              /**< Chains partially applied */
    uint64_t chains_failed;               /**< Chains that failed */

    /* Rollback stats */
    uint64_t rollbacks_performed;         /**< Fixes rolled back */
    uint64_t rollbacks_failed;            /**< Failed rollbacks */

    /* Performance */
    double avg_pipeline_time_ms;          /**< Avg crash-to-fix time */
    double avg_sandbox_time_ms;           /**< Avg sandbox validation time */
    float overall_success_rate;           /**< Fixes that prevented recurrence */
} heal_bridge_stats_t;

/**
 * @brief Heal bridge state
 */
struct heal_bridge_s {
    bridge_base_t base;                   /**< MUST be first: base bridge infrastructure */
    heal_bridge_config_t config;          /**< Configuration */

    /* Connected systems */
    self_heal_engine_t* self_heal;        /**< Self-healing engine */
    code_immune_system_t* code_immune;    /**< Code immune system */
    pattern_library_t* pattern_library;   /**< Pattern library */

    /* Pattern evolution */
    pattern_candidate_t* candidates;      /**< Evolution candidates */
    size_t candidate_count;
    size_t candidate_capacity;
    uint64_t next_candidate_id;

    /* Fix chains */
    fix_chain_t* active_chains;           /**< Active fix chains */
    size_t chain_count;
    size_t chain_capacity;
    uint64_t next_chain_id;

    /* Rollback history */
    rollback_entry_t* rollback_history;   /**< Rollback entries */
    size_t rollback_count;
    size_t rollback_capacity;

    /* Sandbox */
    sandbox_context_t sandbox;            /**< Sandbox context */

    /* Statistics */
    heal_bridge_stats_t stats;

    /* State */
    bool initialized;
    uint64_t start_time;
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default bridge configuration
 */
int heal_bridge_default_config(heal_bridge_config_t* config);

/**
 * @brief Create heal bridge
 *
 * WHAT: Initialize unified healing bridge
 * WHY:  Connect self_heal and code_immune pipelines
 * HOW:  Create bridge, link systems, set up sandbox
 *
 * @param self_heal Self-healing engine
 * @param code_immune Code immune system
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
heal_bridge_t* heal_bridge_create(
    self_heal_engine_t* self_heal,
    code_immune_system_t* code_immune,
    const heal_bridge_config_t* config
);

/**
 * @brief Destroy heal bridge
 */
void heal_bridge_destroy(heal_bridge_t* bridge);

/* ============================================================================
 * Unified Pipeline API
 * ============================================================================ */

/**
 * @brief Process crash through full pipeline
 *
 * WHAT: Full crash-to-fix pipeline
 * WHY:  Single entry point for automated healing
 * HOW:  Detect -> Analyze -> Generate -> Validate -> Apply
 *
 * @param bridge Heal bridge
 * @param antigen_id Crash antigen from code_immune
 * @param source_code Source code at crash site
 * @param antibody_id_out Output: applied antibody ID
 * @return 0 on success (fix applied), negative on error
 */
int heal_bridge_process_crash(
    heal_bridge_t* bridge,
    uint64_t antigen_id,
    const char* source_code,
    uint64_t* antibody_id_out
);

/**
 * @brief Process crash with fix chain
 *
 * WHAT: Process crash requiring multiple fixes
 * WHY:  Handle complex crashes needing coordinated fixes
 * HOW:  Generate chain, validate each, apply atomically
 *
 * @param bridge Heal bridge
 * @param antigen_id Crash antigen
 * @param source_code Source code
 * @param chain_id_out Output: fix chain ID
 * @return 0 on success, negative on error
 */
int heal_bridge_process_crash_chain(
    heal_bridge_t* bridge,
    uint64_t antigen_id,
    const char* source_code,
    uint64_t* chain_id_out
);

/* ============================================================================
 * Sandbox Validation API
 * ============================================================================ */

/**
 * @brief Validate fix in sandbox
 *
 * WHAT: Test fix in isolated sandbox
 * WHY:  Ensure fix doesn't cause new issues
 * HOW:  Fork, apply fix, run tests, report result
 *
 * @param bridge Heal bridge
 * @param fix Fix to validate
 * @param result_out Output: validation result
 * @return 0 on validation complete, negative on error
 */
int heal_bridge_validate_fix(
    heal_bridge_t* bridge,
    const heal_result_t* fix,
    sandbox_result_t* result_out
);

/**
 * @brief Run regression tests in sandbox
 *
 * WHAT: Run regression suite against fixed code
 * WHY:  Detect if fix breaks existing functionality
 * HOW:  Compile fix, load in sandbox, run test suite
 *
 * @param bridge Heal bridge
 * @param fix Fix to test
 * @param test_command Test command to run (NULL for default)
 * @return 0 if no regression, -1 if regression detected
 */
int heal_bridge_run_regression(
    heal_bridge_t* bridge,
    const heal_result_t* fix,
    const char* test_command
);

/* ============================================================================
 * Pattern Evolution API
 * ============================================================================ */

/**
 * @brief Register LNN fix as evolution candidate
 *
 * WHAT: Add LNN-generated fix to evolution tracking
 * WHY:  Track successful LNN fixes for pattern promotion
 * HOW:  Store features and fix, increment on success
 *
 * @param bridge Heal bridge
 * @param features Crash features
 * @param fix LNN-generated fix
 * @param candidate_id_out Output: candidate ID
 * @return 0 on success
 */
int heal_bridge_register_candidate(
    heal_bridge_t* bridge,
    const crash_features_t* features,
    const heal_result_t* fix,
    uint64_t* candidate_id_out
);

/**
 * @brief Record candidate fix outcome
 *
 * WHAT: Record success/failure of candidate fix
 * WHY:  Update statistics for promotion decision
 * HOW:  Update counters, check for promotion threshold
 *
 * @param bridge Heal bridge
 * @param candidate_id Candidate ID
 * @param success Whether fix succeeded
 * @param confidence Confidence of fix
 * @return 0 on success, 1 if promoted, negative on error
 */
int heal_bridge_record_candidate_outcome(
    heal_bridge_t* bridge,
    uint64_t candidate_id,
    bool success,
    float confidence
);

/**
 * @brief Promote candidate to pattern
 *
 * WHAT: Convert successful LNN fix to pattern
 * WHY:  Faster future response for similar crashes
 * HOW:  Create pattern from fix template, add to library
 *
 * @param bridge Heal bridge
 * @param candidate_id Candidate to promote
 * @param pattern_id_out Output: new pattern ID
 * @return 0 on success
 */
int heal_bridge_promote_candidate(
    heal_bridge_t* bridge,
    uint64_t candidate_id,
    uint32_t* pattern_id_out
);

/**
 * @brief Decay pattern candidates
 *
 * WHAT: Age out old candidates
 * WHY:  Remove stale candidates, prioritize recent
 * HOW:  Reduce confidence of old candidates, prune
 *
 * @param bridge Heal bridge
 * @return Number of candidates pruned
 */
int heal_bridge_decay_candidates(heal_bridge_t* bridge);

/* ============================================================================
 * Fix Chain API
 * ============================================================================ */

/**
 * @brief Create fix chain
 *
 * WHAT: Create multi-step fix chain
 * WHY:  Handle crashes requiring multiple fixes
 * HOW:  Analyze crash, generate related fixes
 *
 * @param bridge Heal bridge
 * @param antigen_id Target crash
 * @param source_code Source code
 * @param chain_id_out Output: chain ID
 * @return 0 on success
 */
int heal_bridge_create_chain(
    heal_bridge_t* bridge,
    uint64_t antigen_id,
    const char* source_code,
    uint64_t* chain_id_out
);

/**
 * @brief Add fix to chain
 *
 * WHAT: Add fix to existing chain
 * WHY:  Build up multi-step fix
 * HOW:  Append fix with dependency info
 *
 * @param bridge Heal bridge
 * @param chain_id Chain ID
 * @param fix Fix to add
 * @param dependency Dependency type
 * @param depends_on Index of prerequisite (-1 for none)
 * @return 0 on success
 */
int heal_bridge_add_to_chain(
    heal_bridge_t* bridge,
    uint64_t chain_id,
    const heal_result_t* fix,
    fix_dependency_t dependency,
    int depends_on
);

/**
 * @brief Execute fix chain
 *
 * WHAT: Apply all fixes in chain
 * WHY:  Coordinate multi-step repair
 * HOW:  Validate each, apply in order, rollback on failure
 *
 * @param bridge Heal bridge
 * @param chain_id Chain to execute
 * @return 0 on complete success, 1 on partial, negative on failure
 */
int heal_bridge_execute_chain(
    heal_bridge_t* bridge,
    uint64_t chain_id
);

/**
 * @brief Get chain status
 */
int heal_bridge_get_chain_status(
    heal_bridge_t* bridge,
    uint64_t chain_id,
    chain_status_t* status_out,
    size_t* applied_count_out
);

/* ============================================================================
 * Rollback API
 * ============================================================================ */

/**
 * @brief Rollback applied fix
 *
 * WHAT: Revert applied fix
 * WHY:  Fix caused unexpected issues
 * HOW:  Restore original code/function pointer
 *
 * @param bridge Heal bridge
 * @param antibody_id Antibody to rollback
 * @return 0 on success
 */
int heal_bridge_rollback(
    heal_bridge_t* bridge,
    uint64_t antibody_id
);

/**
 * @brief Rollback entire chain
 *
 * WHAT: Revert all fixes in chain
 * WHY:  Chain caused issues after partial success
 * HOW:  Rollback in reverse order
 *
 * @param bridge Heal bridge
 * @param chain_id Chain to rollback
 * @return 0 on success
 */
int heal_bridge_rollback_chain(
    heal_bridge_t* bridge,
    uint64_t chain_id
);

/**
 * @brief Cleanup old rollback entries
 *
 * WHAT: Remove old rollback entries outside window
 * WHY:  Free memory, entries no longer needed
 * HOW:  Remove entries older than rollback_window_ms
 *
 * @param bridge Heal bridge
 * @return Number of entries cleaned
 */
int heal_bridge_cleanup_rollback_history(heal_bridge_t* bridge);

/* ============================================================================
 * Integration Callbacks
 * ============================================================================ */

/**
 * @brief Callback for crash received from code_immune
 */
typedef void (*heal_bridge_crash_cb_t)(
    heal_bridge_t* bridge,
    const code_antigen_t* antigen,
    void* user_data
);

/**
 * @brief Callback for fix applied
 */
typedef void (*heal_bridge_fix_cb_t)(
    heal_bridge_t* bridge,
    const heal_result_t* fix,
    uint64_t antibody_id,
    bool success,
    void* user_data
);

/**
 * @brief Callback for pattern evolved
 */
typedef void (*heal_bridge_evolution_cb_t)(
    heal_bridge_t* bridge,
    uint64_t candidate_id,
    uint32_t pattern_id,
    void* user_data
);

/**
 * @brief Set crash callback
 */
int heal_bridge_set_crash_callback(
    heal_bridge_t* bridge,
    heal_bridge_crash_cb_t callback,
    void* user_data
);

/**
 * @brief Set fix callback
 */
int heal_bridge_set_fix_callback(
    heal_bridge_t* bridge,
    heal_bridge_fix_cb_t callback,
    void* user_data
);

/**
 * @brief Set evolution callback
 */
int heal_bridge_set_evolution_callback(
    heal_bridge_t* bridge,
    heal_bridge_evolution_cb_t callback,
    void* user_data
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 */
int heal_bridge_get_stats(
    heal_bridge_t* bridge,
    heal_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 */
int heal_bridge_reset_stats(heal_bridge_t* bridge);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* heal_bridge_sandbox_result_to_string(sandbox_result_t result);
const char* heal_bridge_chain_status_to_string(chain_status_t status);
const char* heal_bridge_evolution_state_to_string(pattern_evolution_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HEAL_BRIDGE_H */
