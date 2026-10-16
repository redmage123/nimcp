/**
 * @file nimcp_code_immune.h
 * @brief Code Immune System - Runtime Crash Defense and Hot-Patching
 * @version 1.0.0
 * @date 2025-12-27
 *
 * WHAT: Immune system that detects code crashes, learns crash signatures,
 *       and generates runtime patches to prevent recurrence.
 * WHY:  Enable self-healing software that can survive and adapt to bugs.
 * HOW:  Integrate with signal handler for crash detection, pattern-match
 *       crash signatures (epitopes), produce code antibodies (patches).
 *
 * BIOLOGICAL MODEL:
 * ```
 * BIOLOGICAL CONCEPT              CODE IMMUNE IMPLEMENTATION
 * ───────────────────────────────────────────────────────────────────
 * Antigen presentation         -> Crash signal -> code_antigen_t
 * B cells (antibody production)-> Pattern matching -> fix generation
 * Memory B cells               -> Learned crash patterns for fast response
 * Antibodies                   -> Hot patches / .so injection
 * Apoptosis                    -> Old code cleanup / memory reclaim
 * ```
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    CODE IMMUNE SYSTEM                                      ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                 SIGNAL HANDLER INTEGRATION                          │  ║
 * ║   │   ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐    │  ║
 * ║   │   │   SIGSEGV    │  │   SIGBUS     │  │     SIGABRT          │    │  ║
 * ║   │   └──────┬───────┘  └──────┬───────┘  └──────────┬───────────┘    │  ║
 * ║   │          └─────────────────┼──────────────────────┘                │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │                 [code_immune_present_crash]                        │  ║
 * ║   │                    (Crash as Antigen)                              │  ║
 * ║   └────────────────────────────┬───────────────────────────────────────┘  ║
 * ║                                ▼                                          ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                    PATTERN MATCHING                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌─────────────┐    ┌─────────────┐    ┌─────────────────────┐   │  ║
 * ║   │   │  B CELLS    │    │   MEMORY    │    │    FIX TEMPLATES    │   │  ║
 * ║   │   │ ──────────  │    │  B CELLS    │    │ ─────────────────   │   │  ║
 * ║   │   │ Crash       │    │  Learned    │    │ Null check, bounds  │   │  ║
 * ║   │   │ Signatures  │    │  Patterns   │    │ Alignment, div/0    │   │  ║
 * ║   │   └─────────────┘    └─────────────┘    └─────────────────────┘   │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                │                                          ║
 * ║                                ▼                                          ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                    ANTIBODY PRODUCTION                              │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌─────────────────────────────────────────────────────────────┐  │  ║
 * ║   │   │                     code_antibody_t                          │  │  ║
 * ║   │   │    Hot patch generation (.so) / Function redirection         │  │  ║
 * ║   │   │    (IGM = quick/untested, IGG = verified, IGE = emergency)   │  │  ║
 * ║   │   └─────────────────────────────────────────────────────────────┘  │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CODE_IMMUNE_H
#define NIMCP_CODE_IMMUNE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <signal.h>

/* Integration with parent immune system */
#include "cognitive/immune/nimcp_brain_immune.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define CODE_IMMUNE_MAX_ANTIGENS       128   /**< Max pending crash antigens */
#define CODE_IMMUNE_MAX_B_CELLS        256   /**< Max code B cells */
#define CODE_IMMUNE_MAX_ANTIBODIES     512   /**< Max active code antibodies */
#define CODE_IMMUNE_EPITOPE_SIZE       64    /**< Crash signature hash size */
#define CODE_IMMUNE_BACKTRACE_DEPTH    32    /**< Max backtrace frames */
#define CODE_IMMUNE_MODULE_NAME        "code_immune"
#define CODE_IMMUNE_PERSIST_MAX_PATH   512   /**< Max filepath length (moved here for struct) */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct code_immune_system code_immune_system_t;
typedef struct self_repair_coordinator self_repair_coordinator_t;
typedef struct diagnostic_result diagnostic_result_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Crash signal types (bitmask for pattern matching)
 *
 * BIOLOGICAL BASIS:
 * Different crash types are like different pathogen classes.
 * B cells can specialize in handling specific crash types.
 */
typedef enum {
    CODE_CRASH_NONE     = 0,
    CODE_CRASH_SIGSEGV  = 1,       /**< Segmentation fault - NULL/invalid ptr */
    CODE_CRASH_SIGBUS   = 2,       /**< Bus error - misaligned access */
    CODE_CRASH_SIGILL   = 4,       /**< Illegal instruction */
    CODE_CRASH_SIGFPE   = 8,       /**< Floating point exception */
    CODE_CRASH_SIGABRT  = 16,      /**< Abort - assertion/explicit abort */
    CODE_CRASH_ALL      = 31       /**< All crash types */
} code_crash_type_t;

/**
 * @brief Code B cell states (parallel to brain_b_cell_state_t)
 *
 * BIOLOGICAL BASIS:
 * B cells progress through activation states as they learn
 * to recognize and respond to crash patterns.
 */
typedef enum {
    CODE_B_CELL_NAIVE = 0,         /**< Unactivated, no bound crash */
    CODE_B_CELL_ACTIVATED,         /**< Recognized crash pattern */
    CODE_B_CELL_PLASMA,            /**< Actively producing antibodies */
    CODE_B_CELL_MEMORY,            /**< Long-term pattern memory */
    CODE_B_CELL_APOPTOTIC          /**< Marked for cleanup */
} code_b_cell_state_t;

/**
 * @brief Code antibody classes (response intensity)
 *
 * BIOLOGICAL BASIS:
 * IgM = first response (quick but unverified)
 * IgG = mature/tested response
 * IgE = emergency override
 */
typedef enum {
    CODE_ANTIBODY_IGM = 0,         /**< First response - untested patch */
    CODE_ANTIBODY_IGG = 1,         /**< Mature - validated patch */
    CODE_ANTIBODY_IGE = 2          /**< Emergency - bypass validation */
} code_antibody_class_t;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

/**
 * @brief Code antigen - crash event presented for immune processing
 *
 * Represents a crash detected by signal handler, normalized
 * for code immune processing. The epitope is a hash of the
 * crash signature for pattern matching.
 */
typedef struct {
    uint64_t id;                              /**< Unique antigen ID */
    char epitope[CODE_IMMUNE_EPITOPE_SIZE];   /**< Hash of crash signature */

    /* Crash details */
    int signal;                               /**< Signal number */
    void* fault_address;                      /**< Address that caused fault */
    void* instruction_pointer;                /**< IP at time of crash */

    /* Source location (if available via debug info) */
    char source_file[256];                    /**< Source file path */
    uint32_t line_number;                     /**< Line number */
    char function_name[128];                  /**< Function that crashed */

    /* Backtrace */
    void* backtrace[CODE_IMMUNE_BACKTRACE_DEPTH]; /**< Stack trace */
    int backtrace_depth;                      /**< Number of frames */

    /* Severity and confidence */
    float severity;                           /**< 0-1 severity score */
    float confidence;                         /**< 0-1 pattern match confidence */
    float danger_signal;                      /**< 0-1 cumulative danger */

    /* State */
    bool processed;                           /**< Processed by B cells */
    bool neutralized;                         /**< Fix applied successfully */
    uint64_t timestamp;                       /**< Detection time */
    uint32_t recurrence_count;                /**< How many times seen */
} code_antigen_t;

/**
 * @brief Code B cell - crash pattern recognition and fix generation
 *
 * B cells learn to recognize crash patterns (via receptor)
 * and can generate fixes (antibodies) when activated.
 */
typedef struct {
    uint64_t id;                              /**< Unique B cell ID */
    char receptor[CODE_IMMUNE_EPITOPE_SIZE];  /**< Pattern to match */
    code_crash_type_t crash_types;            /**< Crash types this cell handles */

    /* Pattern specifics */
    char source_pattern[256];                 /**< Source file/function pattern */
    char fix_template[1024];                  /**< Template for generating fix */

    /* State */
    code_b_cell_state_t state;                /**< Current state */
    float affinity;                           /**< Pattern match strength 0-1 */
    uint64_t bound_antigen_id;                /**< Currently bound antigen */

    /* Success tracking */
    uint32_t successful_fixes;                /**< Fixes that worked */
    uint32_t failed_fixes;                    /**< Fixes that failed */

    /* Timing */
    uint64_t creation_time;                   /**< When created */
    uint64_t last_activation;                 /**< Last activation time */
} code_b_cell_t;

/**
 * @brief Code antibody - runtime patch for crash prevention
 *
 * Antibodies are actual fixes that can be injected at runtime
 * to prevent crash recurrence. They can be hot-loaded .so files
 * or function pointer redirections.
 */
typedef struct {
    uint64_t id;                              /**< Unique antibody ID */
    uint64_t target_antigen_id;               /**< Antigen this fixes */
    uint64_t producer_b_cell_id;              /**< B cell that produced this */

    /* Code patch content */
    char original_code[4096];                 /**< Original problematic code */
    char fixed_code[4096];                    /**< Patched code */

    /* Location */
    char source_file[256];                    /**< File containing bug */
    uint32_t start_line;                      /**< Start line of patch */
    uint32_t end_line;                        /**< End line of patch */

    /* Hot-patch info */
    char patch_so_path[256];                  /**< Path to compiled .so patch */
    void* patch_handle;                       /**< dlopen handle */
    void* new_function;                       /**< Patched function pointer */
    void* old_function;                       /**< Original function pointer */
    char fn_name[128];                        /**< Function being patched */

    /* Classification */
    code_antibody_class_t ab_class;           /**< Antibody class */
    float effectiveness;                      /**< 0-1 how well it works */

    /* State */
    bool validated;                           /**< Passed validation tests */
    bool injected;                            /**< Currently active in runtime */

    /* Timing */
    uint64_t creation_time;                   /**< When created */
    uint64_t injection_time;                  /**< When injected */
    uint32_t prevention_count;                /**< Crashes prevented */
} code_antibody_t;

/* ============================================================================
 * Auto-Repair Integration Configuration
 * ============================================================================ */

/**
 * @brief Configuration for automatic self-repair triggering
 *
 * WHAT: Controls when and how code immune triggers the self-repair pipeline
 * WHY:  Enable autonomous crash recovery without manual intervention
 * HOW:  Configure thresholds for severity, confidence, crash count, and cooldown
 *
 * When code_immune detects recurring crash patterns with sufficient severity
 * and confidence, it can automatically trigger the self-repair coordinator
 * to generate and deploy fixes.
 */
typedef struct {
    bool enabled;                         /**< Auto-repair enabled (default: true) */
    uint32_t min_crash_count;             /**< Min crash occurrences before repair (default: 3) */
    float min_severity;                   /**< Min crash severity to trigger (default: 0.8) */
    float min_confidence;                 /**< Min pattern confidence to trigger (default: 0.6) */
    uint32_t cooldown_ms;                 /**< Cooldown between repair attempts (default: 5000) */
    bool notify_health_agent_on_failure;  /**< Notify health agent on repair failure */
    bool async_repair;                    /**< Trigger repair asynchronously (default: true) */
    float min_fix_confidence;             /**< Min fix confidence to accept (default: 0.7) */
    float max_fix_risk;                   /**< Max acceptable fix risk (default: 0.3) */
    bool learn_from_outcomes;             /**< Update B cell stats from repair outcomes */
    bool enable_bio_async;                /**< Enable bio-async messaging */
} code_immune_auto_repair_config_t;

/**
 * @brief Repair outcome notification for learning
 */
typedef struct {
    uint64_t antigen_id;                  /**< Antigen that was repaired */
    uint64_t repair_id;                   /**< Self-repair repair ID */
    bool success;                         /**< Was repair successful? */
    bool hot_patched;                     /**< Was hot-patch applied? */
    bool source_committed;                /**< Was source committed? */
    float fix_confidence;                 /**< Confidence of the fix */
    float fix_effectiveness;              /**< Measured effectiveness (post-deployment) */
    char error_message[256];              /**< Error message if failed */
} code_immune_repair_outcome_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Code immune system configuration
 */
typedef struct {
    /* Population limits */
    size_t max_antigens;                      /**< Max pending crash antigens */
    size_t max_b_cells;                       /**< Max B cells */
    size_t max_antibodies;                    /**< Max antibodies */

    /* Timing (milliseconds) */
    uint64_t activation_delay_ms;             /**< Delay before response */
    uint64_t memory_formation_delay_ms;       /**< Delay for memory formation */
    uint64_t antibody_half_life_ms;           /**< Antibody decay time */

    /* Thresholds */
    float recognition_threshold;              /**< Pattern match threshold 0-1 */
    float activation_threshold;               /**< Activation threshold 0-1 */
    float memory_formation_threshold;         /**< Threshold for memory cell 0-1 */

    /* Patching options */
    bool enable_hot_patching;                 /**< Allow runtime .so injection */
    bool enable_function_redirect;            /**< Allow function ptr redirect */
    bool require_validation;                  /**< Require patch validation */
    const char* patch_build_dir;              /**< Directory for compiled patches */

    /* Integration */
    bool enable_logging;                      /**< Enable security logging */
    bool sync_with_brain_immune;              /**< Sync with parent immune */

    /* Self-repair integration */
    code_immune_auto_repair_config_t auto_repair; /**< Auto-repair configuration */
} code_immune_config_t;

/**
 * @brief Code immune system statistics
 */
typedef struct {
    /* Cell counts */
    uint32_t active_b_cells;
    uint32_t memory_b_cells;
    uint32_t active_antibodies;

    /* Activity */
    uint64_t crashes_detected;
    uint64_t crashes_neutralized;
    uint64_t patches_generated;
    uint64_t patches_applied;
    uint64_t patches_validated;
    uint64_t patches_failed;

    /* Effectiveness */
    float avg_response_time_ms;
    float prevention_rate;                    /**< % of recurrences prevented */
    float system_health;                      /**< Overall health 0-1 */

    /* By crash type */
    uint64_t sigsegv_count;
    uint64_t sigbus_count;
    uint64_t sigill_count;
    uint64_t sigfpe_count;
    uint64_t sigabrt_count;
} code_immune_stats_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Callback for crash antigen detection
 */
typedef void (*code_immune_crash_cb_t)(
    code_immune_system_t* system,
    const code_antigen_t* antigen,
    void* user_data
);

/**
 * @brief Callback for patch application
 */
typedef void (*code_immune_patch_cb_t)(
    code_immune_system_t* system,
    const code_antibody_t* antibody,
    bool success,
    void* user_data
);

/**
 * @brief Callback for memory cell formation
 */
typedef void (*code_immune_memory_cb_t)(
    code_immune_system_t* system,
    const code_b_cell_t* memory_cell,
    void* user_data
);

/* ============================================================================
 * Main System Structure
 * ============================================================================ */

/**
 * @brief Code immune system state
 */
struct code_immune_system {
    code_immune_config_t config;              /**< Configuration */

    /* Parent immune system integration */
    brain_immune_system_t* parent_immune;     /**< Brain immune system */

    /* Antigen pool */
    code_antigen_t* antigens;
    size_t antigen_count;
    size_t antigen_capacity;
    uint64_t next_antigen_id;

    /* B cells */
    code_b_cell_t* b_cells;
    size_t b_cell_count;
    size_t b_cell_capacity;
    uint64_t next_b_cell_id;

    /* Antibodies */
    code_antibody_t* antibodies;
    size_t antibody_count;
    size_t antibody_capacity;
    uint64_t next_antibody_id;

    /* Callbacks */
    code_immune_crash_cb_t on_crash;
    code_immune_patch_cb_t on_patch;
    code_immune_memory_cb_t on_memory;
    void* callback_user_data;

    /* Statistics */
    code_immune_stats_t stats;

    /* Thread safety */
    void* mutex;                              /**< Platform mutex */

    /* State */
    bool running;
    bool signal_handler_connected;
    uint64_t start_time;

    /* Persistence state */
    bool auto_save_enabled;                   /**< Auto-save on validation */
    bool auto_load_enabled;                   /**< Auto-load on startup */
    char auto_save_path[CODE_IMMUNE_PERSIST_MAX_PATH]; /**< Auto-save filepath */
    char auto_load_path[CODE_IMMUNE_PERSIST_MAX_PATH]; /**< Auto-load filepath */

    /* Self-repair integration */
    self_repair_coordinator_t* self_repair;   /**< Connected self-repair coordinator */
    uint64_t last_repair_trigger_ms;          /**< Last repair trigger timestamp */
    uint32_t pending_repairs;                 /**< Number of repairs in progress */
    uint64_t total_repairs_triggered;         /**< Total repairs triggered */
    uint64_t total_repairs_successful;        /**< Total successful repairs */
    uint64_t total_repairs_failed;            /**< Total failed repairs */
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with good defaults
 * HOW:  Return struct with balanced parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int code_immune_default_config(code_immune_config_t* config);

/**
 * @brief Create code immune system
 *
 * WHAT: Initialize code immune coordination layer
 * WHY:  Set up crash defense infrastructure
 * HOW:  Allocate pools, optionally link to brain immune
 *
 * @param parent_immune Parent brain immune system (can be NULL)
 * @return New code immune system or NULL on failure
 */
code_immune_system_t* code_immune_create(brain_immune_system_t* parent_immune);

/**
 * @brief Create code immune system with config
 *
 * WHAT: Initialize with custom configuration
 * WHY:  Allow fine-grained control
 * HOW:  Use provided config instead of defaults
 *
 * @param parent_immune Parent brain immune system (can be NULL)
 * @param config Custom configuration
 * @return New code immune system or NULL on failure
 */
code_immune_system_t* code_immune_create_with_config(
    brain_immune_system_t* parent_immune,
    const code_immune_config_t* config
);

/**
 * @brief Destroy code immune system
 *
 * WHAT: Clean up code immune system resources
 * WHY:  Proper resource deallocation
 * HOW:  Unload patches, free pools, unregister from signal handler
 *
 * @param system System to destroy
 */
void code_immune_destroy(code_immune_system_t* system);

/**
 * @brief Start code immune system
 *
 * WHAT: Activate crash monitoring
 * WHY:  Begin crash detection and response
 * HOW:  Register with signal handler
 *
 * @param system Code immune system
 * @return 0 on success
 */
int code_immune_start(code_immune_system_t* system);

/**
 * @brief Stop code immune system
 *
 * WHAT: Deactivate code immune system
 * WHY:  Graceful shutdown
 * HOW:  Unregister from signal handler, keep learned patterns
 *
 * @param system Code immune system
 * @return 0 on success
 */
int code_immune_stop(code_immune_system_t* system);

/* ============================================================================
 * Signal Handler Integration API
 * ============================================================================ */

/**
 * @brief Connect to signal handler
 *
 * WHAT: Register with signal handler for crash notification
 * WHY:  Receive crashes as antigens for processing
 * HOW:  Set callback in signal handler
 *
 * @param system Code immune system
 * @return 0 on success
 */
int code_immune_connect_signal_handler(code_immune_system_t* system);

/**
 * @brief Disconnect from signal handler
 *
 * WHAT: Unregister from signal handler
 * WHY:  Stop receiving crash notifications
 * HOW:  Clear callback in signal handler
 *
 * @param system Code immune system
 * @return 0 on success
 */
int code_immune_disconnect_signal_handler(code_immune_system_t* system);

/**
 * @brief Process pending crashes from async-signal-safe queue
 *
 * WHAT: Drain and process crashes queued by signal handler
 * WHY:  Signal handlers cannot safely call most functions (malloc, mutex, etc.)
 * HOW:  Read from volatile queue, call normal crash processing in safe context
 *
 * Call this periodically from the main thread or update loop to ensure
 * crashes queued by signal handlers are processed.
 *
 * @param system Code immune system
 */
void code_immune_process_pending_crashes(code_immune_system_t* system);

/**
 * @brief Present crash as antigen
 *
 * WHAT: Process crash signal as immune antigen
 * WHY:  Unified crash handling through immune system
 * HOW:  Create antigen from signal context
 *
 * Note: Do NOT call this directly from signal handler - use the async-safe
 * callback which queues crashes for later processing.
 *
 * @param system Code immune system
 * @param signal Signal number (SIGSEGV, etc)
 * @param ucontext Signal ucontext (can be NULL)
 * @param fault_addr Faulting address (can be NULL)
 * @return 0 on success (antigen queued), -1 on error
 */
int code_immune_present_crash(
    code_immune_system_t* system,
    int signal,
    void* ucontext,
    void* fault_addr
);

/**
 * @brief Present crash with full details
 *
 * WHAT: Present crash with complete diagnostic info
 * WHY:  Better pattern matching with full context
 * HOW:  Create detailed antigen record
 *
 * Note: NOT signal-safe - call from deferred context
 *
 * @param system Code immune system
 * @param signal Signal number
 * @param fault_addr Faulting address
 * @param ip Instruction pointer
 * @param source_file Source file (can be NULL)
 * @param line Line number
 * @param function Function name (can be NULL)
 * @param backtrace Backtrace array
 * @param backtrace_depth Number of frames
 * @param antigen_id Output: assigned antigen ID
 * @return 0 on success
 */
int code_immune_present_crash_detailed(
    code_immune_system_t* system,
    int signal,
    void* fault_addr,
    void* ip,
    const char* source_file,
    uint32_t line,
    const char* function,
    void** backtrace,
    int backtrace_depth,
    uint64_t* antigen_id
);

/* ============================================================================
 * B Cell API
 * ============================================================================ */

/**
 * @brief Find matching B cell for antigen
 *
 * WHAT: Search for B cell that recognizes crash pattern
 * WHY:  Reuse existing pattern knowledge
 * HOW:  Compare epitope against B cell receptors
 *
 * @param system Code immune system
 * @param antigen_id Antigen to match
 * @param b_cell_id Output: matching B cell ID (if found)
 * @return 0 if found, -1 if no match
 */
int code_immune_find_matching_b_cell(
    code_immune_system_t* system,
    uint64_t antigen_id,
    uint64_t* b_cell_id
);

/**
 * @brief Activate B cell for antigen
 *
 * WHAT: Activate B cell that recognizes antigen
 * WHY:  Begin antibody production process
 * HOW:  Set B cell state, bind to antigen
 *
 * @param system Code immune system
 * @param b_cell_id B cell to activate
 * @param antigen_id Triggering antigen
 * @return 0 on success
 */
int code_immune_activate_b_cell(
    code_immune_system_t* system,
    uint64_t b_cell_id,
    uint64_t antigen_id
);

/**
 * @brief Create new B cell for novel crash pattern
 *
 * WHAT: Create B cell for unrecognized crash
 * WHY:  Learn new crash patterns
 * HOW:  Create B cell with receptor matching antigen epitope
 *
 * @param system Code immune system
 * @param antigen_id Antigen to create B cell for
 * @param b_cell_id Output: new B cell ID
 * @return 0 on success
 */
int code_immune_create_b_cell(
    code_immune_system_t* system,
    uint64_t antigen_id,
    uint64_t* b_cell_id
);

/**
 * @brief Form memory B cell
 *
 * WHAT: Convert activated B cell to memory cell
 * WHY:  Persist learned crash patterns for fast future response
 * HOW:  Update B cell state, persist pattern
 *
 * @param system Code immune system
 * @param b_cell_id B cell to convert
 * @return 0 on success
 */
int code_immune_form_memory(
    code_immune_system_t* system,
    uint64_t b_cell_id
);

/**
 * @brief Set fix template for B cell
 *
 * WHAT: Associate fix template with B cell
 * WHY:  Enable antibody generation
 * HOW:  Store template for use in antibody production
 *
 * @param system Code immune system
 * @param b_cell_id B cell ID
 * @param fix_template Fix code template
 * @return 0 on success
 */
int code_immune_set_fix_template(
    code_immune_system_t* system,
    uint64_t b_cell_id,
    const char* fix_template
);

/* ============================================================================
 * Antibody API
 * ============================================================================ */

/**
 * @brief Produce antibody from B cell
 *
 * WHAT: Generate code patch from activated B cell
 * WHY:  Create fix for crash
 * HOW:  Apply fix template to generate patch
 *
 * @param system Code immune system
 * @param b_cell_id Producing B cell
 * @param ab_class Antibody class (IGM=quick, IGG=verified, IGE=emergency)
 * @param antibody_id Output: new antibody ID
 * @return 0 on success
 */
int code_immune_produce_antibody(
    code_immune_system_t* system,
    uint64_t b_cell_id,
    code_antibody_class_t ab_class,
    uint64_t* antibody_id
);

/**
 * @brief Validate antibody
 *
 * WHAT: Test that patch works correctly
 * WHY:  Ensure fix doesn't cause other problems
 * HOW:  Run validation tests on patch
 *
 * @param system Code immune system
 * @param antibody_id Antibody to validate
 * @return 0 if valid, -1 if invalid
 */
int code_immune_validate_antibody(
    code_immune_system_t* system,
    uint64_t antibody_id
);

/**
 * @brief Apply antibody
 *
 * WHAT: Inject patch into running system
 * WHY:  Prevent crash recurrence
 * HOW:  Load .so or redirect function pointer
 *
 * @param system Code immune system
 * @param antibody_id Antibody to apply
 * @return 0 on success
 */
int code_immune_apply_antibody(
    code_immune_system_t* system,
    uint64_t antibody_id
);

/**
 * @brief Remove antibody (apoptosis)
 *
 * WHAT: Remove patch from running system
 * WHY:  Clean up old/superseded patches
 * HOW:  Unload .so, restore original function
 *
 * @param system Code immune system
 * @param antibody_id Antibody to remove
 * @return 0 on success
 */
int code_immune_apoptosis(
    code_immune_system_t* system,
    uint64_t antibody_id
);

/**
 * @brief Upgrade antibody class
 *
 * WHAT: Promote IGM to IGG after validation
 * WHY:  Track antibody maturation
 * HOW:  Update antibody class field
 *
 * @param system Code immune system
 * @param antibody_id Antibody to upgrade
 * @param new_class New antibody class
 * @return 0 on success
 */
int code_immune_upgrade_antibody(
    code_immune_system_t* system,
    uint64_t antibody_id,
    code_antibody_class_t new_class
);

/* ============================================================================
 * Brain Immune Integration API
 * ============================================================================ */

/**
 * @brief Sync crash to brain immune as antigen
 *
 * WHAT: Present code crash to parent brain immune system
 * WHY:  Unified immune response across all subsystems
 * HOW:  Create brain antigen from code crash
 *
 * @param system Code immune system
 * @param antigen_id Code antigen to sync
 * @return 0 on success
 */
int code_immune_sync_to_brain(
    code_immune_system_t* system,
    uint64_t antigen_id
);

/**
 * @brief Request cytokine release from brain immune
 *
 * WHAT: Trigger brain immune cytokine for code crash
 * WHY:  Coordinate system-wide response
 * HOW:  Call brain immune cytokine API
 *
 * @param system Code immune system
 * @param cytokine_type Cytokine type to release
 * @param concentration Signal strength 0-1
 * @return 0 on success
 */
int code_immune_request_cytokine(
    code_immune_system_t* system,
    brain_cytokine_type_t cytokine_type,
    float concentration
);

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

/**
 * @brief Set crash detection callback
 */
int code_immune_set_crash_callback(
    code_immune_system_t* system,
    code_immune_crash_cb_t callback,
    void* user_data
);

/**
 * @brief Set patch application callback
 */
int code_immune_set_patch_callback(
    code_immune_system_t* system,
    code_immune_patch_cb_t callback,
    void* user_data
);

/**
 * @brief Set memory formation callback
 */
int code_immune_set_memory_callback(
    code_immune_system_t* system,
    code_immune_memory_cb_t callback,
    void* user_data
);

/* ============================================================================
 * Update and Query API
 * ============================================================================ */

/**
 * @brief Update code immune system state
 *
 * WHAT: Process pending crashes and decay antibodies
 * WHY:  Advance immune state machine
 * HOW:  Process antigen queue, update statistics
 *
 * @param system Code immune system
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int code_immune_update(
    code_immune_system_t* system,
    uint64_t delta_ms
);

/**
 * @brief Get code immune system statistics
 *
 * @param system Code immune system
 * @param stats Output statistics
 * @return 0 on success
 */
int code_immune_get_stats(
    code_immune_system_t* system,
    code_immune_stats_t* stats
);

/**
 * @brief Get antigen by ID
 *
 * @param system Code immune system
 * @param antigen_id Antigen ID
 * @return Antigen or NULL if not found
 */
const code_antigen_t* code_immune_get_antigen(
    code_immune_system_t* system,
    uint64_t antigen_id
);

/**
 * @brief Get B cell by ID
 *
 * @param system Code immune system
 * @param b_cell_id B cell ID
 * @return B cell or NULL if not found
 */
const code_b_cell_t* code_immune_get_b_cell(
    code_immune_system_t* system,
    uint64_t b_cell_id
);

/**
 * @brief Get antibody by ID
 *
 * @param system Code immune system
 * @param antibody_id Antibody ID
 * @return Antibody or NULL if not found
 */
const code_antibody_t* code_immune_get_antibody(
    code_immune_system_t* system,
    uint64_t antibody_id
);

/**
 * @brief Check if crash type is handled
 *
 * @param system Code immune system
 * @param crash_type Crash type to check
 * @return true if memory exists for this crash type
 */
bool code_immune_has_memory_for(
    code_immune_system_t* system,
    code_crash_type_t crash_type
);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* code_immune_crash_type_to_string(code_crash_type_t type);
const char* code_immune_b_cell_state_to_string(code_b_cell_state_t state);
const char* code_immune_antibody_class_to_string(code_antibody_class_t ab_class);

/* ============================================================================
 * Epitope/Pattern Matching Utilities
 * ============================================================================ */

/**
 * @brief Compute crash epitope (hash signature)
 *
 * WHAT: Generate unique signature for crash
 * WHY:  Enable pattern matching across crashes
 * HOW:  Hash crash details into epitope string
 *
 * @param signal Signal number
 * @param fault_addr Faulting address
 * @param ip Instruction pointer
 * @param backtrace Backtrace frames
 * @param backtrace_depth Number of frames
 * @param epitope Output buffer (CODE_IMMUNE_EPITOPE_SIZE)
 * @return 0 on success
 */
int code_immune_compute_epitope(
    int signal,
    void* fault_addr,
    void* ip,
    void** backtrace,
    int backtrace_depth,
    char* epitope
);

/**
 * @brief Compute pattern affinity
 *
 * WHAT: Calculate similarity between epitopes
 * WHY:  Determine if B cell matches antigen
 * HOW:  Fuzzy string matching
 *
 * @param pattern1 First pattern
 * @param pattern2 Second pattern
 * @return Affinity score 0-1
 */
float code_immune_compute_affinity(
    const char* pattern1,
    const char* pattern2
);

/* ============================================================================
 * Self-Repair Integration API
 * ============================================================================ */

/**
 * @brief Get default auto-repair configuration
 *
 * WHAT: Provide sensible default configuration for auto-repair
 * WHY:  Easy initialization with good defaults
 * HOW:  Return struct with balanced parameters
 *
 * DEFAULTS:
 * - enabled: true
 * - min_crash_count: 3
 * - min_severity: 0.8
 * - min_confidence: 0.6
 * - cooldown_ms: 5000
 * - async_repair: true
 *
 * @param config Output configuration (non-NULL)
 * @return 0 on success, -1 on error
 */
int code_immune_auto_repair_default_config(code_immune_auto_repair_config_t* config);

/**
 * @brief Connect to self-repair coordinator
 *
 * WHAT: Link code immune to self-repair pipeline for automatic repairs
 * WHY:  Enable autonomous crash recovery
 * HOW:  Store coordinator reference, set up callbacks
 *
 * @param system Code immune system
 * @param coordinator Self-repair coordinator
 * @return 0 on success, -1 on error
 */
int code_immune_connect_self_repair(
    code_immune_system_t* system,
    self_repair_coordinator_t* coordinator
);

/**
 * @brief Disconnect from self-repair coordinator
 *
 * WHAT: Unlink code immune from self-repair pipeline
 * WHY:  Disable automatic repairs or switch coordinator
 * HOW:  Clear coordinator reference
 *
 * @param system Code immune system
 * @return 0 on success, -1 on error
 */
int code_immune_disconnect_self_repair(code_immune_system_t* system);

/**
 * @brief Check if self-repair is connected
 *
 * @param system Code immune system
 * @return true if connected, false otherwise
 */
bool code_immune_is_self_repair_connected(const code_immune_system_t* system);

/**
 * @brief Manually trigger repair for an antigen
 *
 * WHAT: Initiate self-repair for a specific crash pattern
 * WHY:  Allow manual repair triggering for known issues
 * HOW:  Convert antigen to diagnostic, call self-repair
 *
 * @param system Code immune system
 * @param antigen_id Antigen ID to repair
 * @param repair_id Output: repair ID for tracking (can be NULL)
 * @return 0 on success (repair initiated), -1 on error
 */
int code_immune_trigger_repair(
    code_immune_system_t* system,
    uint64_t antigen_id,
    uint64_t* repair_id
);

/**
 * @brief Convert antigen to diagnostic result (integrated API)
 *
 * WHAT: Transform code antigen into diagnostic_result_t format
 * WHY:  Self-repair coordinator expects diagnostic_result_t input
 * HOW:  Map crash details to error type, severity, stack trace
 *
 * @param system Code immune system
 * @param antigen_id Antigen to convert
 * @param diag Output diagnostic result (must be pre-allocated)
 * @return 0 on success, -1 on error
 */
int code_immune_get_antigen_diagnostic(
    code_immune_system_t* system,
    uint64_t antigen_id,
    diagnostic_result_t* diag
);

/**
 * @brief Notify code immune of repair outcome (integrated API)
 *
 * WHAT: Report repair result for learning and feedback
 * WHY:  Update B cell/antibody effectiveness based on repair success
 * HOW:  Update internal statistics, adjust affinity scores
 *
 * @param system Code immune system
 * @param outcome Repair outcome details
 * @return 0 on success, -1 on error
 */
int code_immune_handle_repair_outcome(
    code_immune_system_t* system,
    const code_immune_repair_outcome_t* outcome
);

/**
 * @brief Check if antigen should trigger auto-repair (integrated API)
 *
 * WHAT: Evaluate if antigen meets auto-repair thresholds
 * WHY:  Determine if repair should be automatically triggered
 * HOW:  Check crash count, severity, confidence against config
 *
 * @param system Code immune system
 * @param antigen_id Antigen to evaluate
 * @return true if repair should be triggered, false otherwise
 */
bool code_immune_check_auto_repair_eligible(
    code_immune_system_t* system,
    uint64_t antigen_id
);

/**
 * @brief Get self-repair statistics
 *
 * WHAT: Get statistics about self-repair integration
 * WHY:  Monitor repair effectiveness and system health
 * HOW:  Return count of repairs triggered, successful, failed
 *
 * @param system Code immune system
 * @param triggered Output: total repairs triggered (can be NULL)
 * @param successful Output: successful repairs (can be NULL)
 * @param failed Output: failed repairs (can be NULL)
 * @return 0 on success, -1 on error
 */
int code_immune_get_repair_stats(
    code_immune_system_t* system,
    uint64_t* triggered,
    uint64_t* successful,
    uint64_t* failed
);

/* ============================================================================
 * Persistence API
 * ============================================================================ */

/* Persistence constants (CODE_IMMUNE_PERSIST_MAX_PATH defined above for struct) */
#define CODE_IMMUNE_PERSIST_MAGIC        "NIMCPCIM"  /**< File magic header */
#define CODE_IMMUNE_PERSIST_MAGIC_LEN    8           /**< Magic header length */
#define CODE_IMMUNE_PERSIST_VERSION      1           /**< Current format version */
#define CODE_IMMUNE_PERSIST_DEFAULT_DIR  "~/.nimcp"  /**< Default directory */
#define CODE_IMMUNE_PERSIST_DEFAULT_FILE "code_immune_memory.bin" /**< Default file */

/* Persistence format flags */
#define CODE_IMMUNE_FORMAT_FLAG_COMPRESSED   0x00000001  /**< Data is compressed */
#define CODE_IMMUNE_FORMAT_FLAG_ENCRYPTED    0x00000002  /**< Data is encrypted */
#define CODE_IMMUNE_FORMAT_FLAG_MEMORY_ONLY  0x00000004  /**< Memory cells only */

/* Minimum confidence for persistence (prune below this) */
#define CODE_IMMUNE_PERSIST_MIN_CONFIDENCE  0.3f

/**
 * @brief Persistence file header for code immune memory
 *
 * WHAT: Metadata header for code immune persistence files
 * WHY:  Version checking, validation, format detection
 * HOW:  Written at start of every persistence file
 */
typedef struct {
    char magic[CODE_IMMUNE_PERSIST_MAGIC_LEN];  /**< Magic identifier */
    uint32_t version;                            /**< Format version */
    uint32_t flags;                              /**< Format flags */
    uint64_t timestamp;                          /**< Save timestamp (ms) */
    uint32_t checksum;                           /**< CRC32 checksum of data */
    uint32_t reserved1;                          /**< Reserved for future use */
    uint64_t file_size;                          /**< Total file size */
    uint64_t reserved2[3];                       /**< Reserved for future use */
} code_immune_persist_header_t;

/**
 * @brief Persistence section counts for code immune
 *
 * WHAT: Counts of each code immune component for array allocation
 * WHY:  Pre-allocation before loading data arrays
 * HOW:  Read after header, before data sections
 */
typedef struct {
    uint32_t b_cell_count;           /**< Number of B cells */
    uint32_t antibody_count;         /**< Number of antibodies */
    uint32_t pattern_stat_count;     /**< Number of pattern statistics */
    uint32_t reserved[5];            /**< Reserved for future use */
} code_immune_persist_counts_t;

/**
 * @brief Pattern match statistics for persistence
 *
 * WHAT: Aggregated pattern match statistics
 * WHY:  Track which patterns are most valuable
 * HOW:  Stored per unique crash signature
 */
typedef struct {
    char epitope[CODE_IMMUNE_EPITOPE_SIZE];  /**< Crash pattern signature */
    code_crash_type_t crash_type;            /**< Primary crash type */
    uint32_t match_count;                    /**< Times this pattern matched */
    uint32_t successful_fixes;               /**< Successful fixes applied */
    uint32_t failed_fixes;                   /**< Failed fixes */
    float avg_affinity;                      /**< Average affinity score */
    uint64_t first_seen;                     /**< First occurrence timestamp */
    uint64_t last_seen;                      /**< Last occurrence timestamp */
} code_immune_pattern_stat_t;

/**
 * @brief Persistence configuration for code immune
 *
 * WHAT: Configuration for code immune memory persistence
 * WHY:  Customize save/load behavior
 * HOW:  Pass to save/load functions to control behavior
 */
typedef struct {
    /* Compression/Encryption */
    bool enable_compression;         /**< Use zlib compression if available */
    bool enable_encryption;          /**< Use AES-256 encryption if available */
    uint8_t encryption_key[32];      /**< Encryption key (256-bit) */
    bool encryption_key_set;         /**< Whether encryption key is valid */

    /* Selective save options */
    bool save_b_cells;               /**< Save B cells */
    bool save_antibodies;            /**< Save antibodies */
    bool save_pattern_stats;         /**< Save pattern statistics */

    /* Memory-only mode */
    bool memory_cells_only;          /**< Save only memory B cells */

    /* Validation */
    bool verify_on_load;             /**< Verify checksum on load */
    bool strict_version_check;       /**< Require exact version match */

    /* Backup */
    bool create_backup;              /**< Create .bak before overwriting */
    char backup_suffix[16];          /**< Backup file suffix (default ".bak") */

    /* Pruning */
    bool auto_prune;                 /**< Auto-prune low-confidence patterns */
    float min_confidence;            /**< Minimum confidence to keep */
    uint32_t min_successful_fixes;   /**< Minimum fixes to be kept */

    /* Auto-save */
    bool auto_save_on_validation;    /**< Auto-save after successful validation */

    /* File paths */
    char memory_file_path[CODE_IMMUNE_PERSIST_MAX_PATH];  /**< Custom memory file path */
} code_immune_persist_config_t;

/**
 * @brief Persistence operation result
 *
 * WHAT: Detailed result of save/load operation
 * WHY:  Provide diagnostics and statistics to caller
 * HOW:  Filled by save/load functions
 */
typedef struct {
    bool success;                    /**< Operation succeeded */
    uint32_t version_loaded;         /**< Version of loaded file */
    uint64_t bytes_written;          /**< Bytes written to disk */
    uint64_t bytes_read;             /**< Bytes read from disk */
    uint64_t save_time_ms;           /**< Time taken for save (ms) */
    uint64_t load_time_ms;           /**< Time taken for load (ms) */
    uint32_t b_cells_saved;          /**< B cells saved/loaded */
    uint32_t antibodies_saved;       /**< Antibodies saved/loaded */
    uint32_t patterns_saved;         /**< Pattern stats saved/loaded */
    uint32_t items_pruned;           /**< Items pruned during consolidation */
    char error_message[256];         /**< Error description if failed */
} code_immune_persist_result_t;

/**
 * @brief Get default persistence configuration
 *
 * WHAT: Provide sensible default configuration for code immune persistence
 * WHY:  Easy initialization with good defaults
 * HOW:  Return struct with balanced parameters
 *
 * DEFAULTS:
 * - Compression: disabled (for speed)
 * - Encryption: disabled (no key)
 * - Save all components: enabled
 * - Memory cells only: false
 * - Auto-prune: enabled with 0.3 min confidence
 * - Auto-save on validation: enabled
 * - Default path: ~/.nimcp/code_immune_memory.bin
 *
 * @param config Output configuration (non-NULL)
 * @return 0 on success, -1 on error
 */
int code_immune_persist_default_config(code_immune_persist_config_t* config);

/**
 * @brief Save code immune memory to file
 *
 * WHAT: Serialize code immune state (B cells, antibodies, patterns) to disk
 * WHY:  Enable cross-session crash pattern learning
 * HOW:  Write header -> counts -> data sections with optional compression
 *
 * THREAD SAFETY: Acquires system mutex during save
 * ATOMICITY: Writes to temp file, then renames (atomic on POSIX)
 *
 * SAVED DATA:
 * - Memory B cells (learned crash patterns with fix templates)
 * - Validated antibodies (proven fixes with effectiveness scores)
 * - Pattern match statistics (aggregated crash pattern data)
 *
 * @param system Code immune system (non-NULL)
 * @param filepath Path to save to (NULL for default path)
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int code_immune_save_memory(
    code_immune_system_t* system,
    const char* filepath,
    const code_immune_persist_config_t* config
);

/**
 * @brief Load code immune memory from file
 *
 * WHAT: Restore code immune state from disk
 * WHY:  Restore learned crash patterns from previous sessions
 * HOW:  Read header -> validate -> read counts -> load data
 *
 * THREAD SAFETY: Acquires system mutex during load
 * VALIDATION:
 * - Header magic and version checked
 * - Counts checked against capacity limits
 * - Checksum verified (if enabled)
 *
 * BEHAVIOR:
 * - Does NOT clear existing state (merges with current)
 * - Updates affinity for existing patterns if higher
 * - Preserves system configuration
 *
 * @param system Code immune system (non-NULL)
 * @param filepath Path to load from (NULL for default path)
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int code_immune_load_memory(
    code_immune_system_t* system,
    const char* filepath,
    const code_immune_persist_config_t* config
);

/**
 * @brief Save with detailed result information
 *
 * WHAT: Save code immune memory with detailed diagnostics
 * WHY:  Get statistics and error details
 * HOW:  Same as code_immune_save_memory but fills result struct
 *
 * @param system Code immune system (non-NULL)
 * @param filepath Path to save to (NULL for default)
 * @param config Configuration (NULL for defaults)
 * @param result Output result structure (non-NULL)
 * @return 0 on success, -1 on error
 */
int code_immune_save_memory_ex(
    code_immune_system_t* system,
    const char* filepath,
    const code_immune_persist_config_t* config,
    code_immune_persist_result_t* result
);

/**
 * @brief Load with detailed result information
 *
 * WHAT: Load code immune memory with detailed diagnostics
 * WHY:  Get statistics and error details
 * HOW:  Same as code_immune_load_memory but fills result struct
 *
 * @param system Code immune system (non-NULL)
 * @param filepath Path to load from (NULL for default)
 * @param config Configuration (NULL for defaults)
 * @param result Output result structure (non-NULL)
 * @return 0 on success, -1 on error
 */
int code_immune_load_memory_ex(
    code_immune_system_t* system,
    const char* filepath,
    const code_immune_persist_config_t* config,
    code_immune_persist_result_t* result
);

/**
 * @brief Consolidate immune memory
 *
 * WHAT: Compact and optimize stored immune memory
 * WHY:  Prune low-confidence patterns, merge similar B cells
 * HOW:  Remove stale entries, update statistics, compact storage
 *
 * CONSOLIDATION ACTIONS:
 * - Prune B cells with affinity < min_confidence
 * - Prune antibodies with effectiveness < min_confidence
 * - Merge B cells with similar receptors (affinity > 0.95)
 * - Update pattern statistics
 * - Remove orphaned entries
 *
 * @param system Code immune system (non-NULL)
 * @param config Configuration (NULL for defaults)
 * @param items_pruned Output: number of items removed (can be NULL)
 * @return 0 on success, -1 on error
 */
int code_immune_consolidate_memory(
    code_immune_system_t* system,
    const code_immune_persist_config_t* config,
    uint32_t* items_pruned
);

/**
 * @brief Get default memory file path
 *
 * WHAT: Resolve default persistence file path
 * WHY:  Provide consistent default location
 * HOW:  Expand ~ and create directory if needed
 *
 * DEFAULT: ~/.nimcp/code_immune_memory.bin
 *
 * @param path_out Output buffer for path (must be CODE_IMMUNE_PERSIST_MAX_PATH)
 * @param create_dir Create directory if it doesn't exist
 * @return 0 on success, -1 on error
 */
int code_immune_get_default_memory_path(
    char* path_out,
    bool create_dir
);

/**
 * @brief Validate persistence file
 *
 * WHAT: Check if file is valid code immune persistence file
 * WHY:  Verify file before loading, detect corruption
 * HOW:  Read header, validate magic/version/checksum
 *
 * @param filepath Path to validate (non-NULL)
 * @param verify_checksum Whether to verify checksum
 * @return 0 if valid, -1 if invalid
 */
int code_immune_validate_memory_file(
    const char* filepath,
    bool verify_checksum
);

/**
 * @brief Check version compatibility
 *
 * WHAT: Check if file version is compatible with current version
 * WHY:  Determine if file can be loaded
 * HOW:  Compare version numbers
 *
 * @param file_version Version from file header
 * @return true if compatible, false otherwise
 */
bool code_immune_is_version_compatible(uint32_t file_version);

/**
 * @brief Create backup of persistence file
 *
 * WHAT: Copy persistence file to backup location
 * WHY:  Prevent data loss on save failure
 * HOW:  Copy file to {filepath}.bak before save
 *
 * @param filepath Original file path (non-NULL)
 * @param backup_suffix Backup suffix (NULL for ".bak")
 * @return 0 on success, -1 on error
 */
int code_immune_create_backup(
    const char* filepath,
    const char* backup_suffix
);

/**
 * @brief Get file information without loading
 *
 * WHAT: Read file header and counts without loading data
 * WHY:  Preview file contents, check size before loading
 * HOW:  Read header and counts section only
 *
 * @param filepath Path to file (non-NULL)
 * @param header Output header (can be NULL)
 * @param counts Output counts (can be NULL)
 * @return 0 on success, -1 on error
 */
int code_immune_get_memory_file_info(
    const char* filepath,
    code_immune_persist_header_t* header,
    code_immune_persist_counts_t* counts
);

/**
 * @brief Enable auto-save on fix validation
 *
 * WHAT: Configure automatic saving after successful fix validation
 * WHY:  Persist newly learned fixes immediately
 * HOW:  Set internal flag and callback
 *
 * @param system Code immune system (non-NULL)
 * @param enable Enable or disable auto-save
 * @param filepath Path for auto-saves (NULL for default)
 * @return 0 on success, -1 on error
 */
int code_immune_enable_auto_save(
    code_immune_system_t* system,
    bool enable,
    const char* filepath
);

/**
 * @brief Enable auto-load on startup
 *
 * WHAT: Load persisted memory on system start
 * WHY:  Restore learned patterns automatically
 * HOW:  Called internally by code_immune_start if enabled
 *
 * @param system Code immune system (non-NULL)
 * @param enable Enable or disable auto-load
 * @param filepath Path for auto-load (NULL for default)
 * @return 0 on success, -1 on error
 */
int code_immune_enable_auto_load(
    code_immune_system_t* system,
    bool enable,
    const char* filepath
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CODE_IMMUNE_H */
