//=============================================================================
// nimcp_diagnostics.h - Self-Diagnostic Error Detection & Analysis System
//=============================================================================

#ifndef NIMCP_DIAGNOSTICS_H
#define NIMCP_DIAGNOSTICS_H

#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct brain_struct* brain_t;

//=============================================================================
// Error Classification Taxonomy
//=============================================================================

/**
 * @brief Error type classification for root cause analysis
 *
 * WHAT: Hierarchical taxonomy of error types NIMCP can encounter
 * WHY:  Enable precise error categorization for targeted recovery strategies
 * HOW:  Each error type maps to specific diagnostic patterns and recovery actions
 */
typedef enum {
    // Memory Errors (0x1000-0x1FFF)
    ERROR_TYPE_NULL_POINTER           = 0x1000,  /**< NULL pointer dereference */
    ERROR_TYPE_BUFFER_OVERFLOW        = 0x1001,  /**< Buffer bounds violation */
    ERROR_TYPE_BUFFER_UNDERFLOW       = 0x1002,  /**< Buffer underflow */
    ERROR_TYPE_MEMORY_LEAK            = 0x1003,  /**< Detected memory leak */
    ERROR_TYPE_DOUBLE_FREE            = 0x1004,  /**< Double free detected */
    ERROR_TYPE_USE_AFTER_FREE         = 0x1005,  /**< Use after free */
    ERROR_TYPE_STACK_OVERFLOW         = 0x1006,  /**< Stack exhaustion */
    ERROR_TYPE_HEAP_CORRUPTION        = 0x1007,  /**< Heap metadata corruption */
    ERROR_TYPE_ALIGNMENT_ERROR        = 0x1008,  /**< Memory alignment violation */

    // Numerical Errors (0x2000-0x2FFF)
    ERROR_TYPE_NAN_DETECTED           = 0x2000,  /**< NaN value detected */
    ERROR_TYPE_INF_DETECTED           = 0x2001,  /**< Infinity value detected */
    ERROR_TYPE_NUMERICAL_OVERFLOW     = 0x2002,  /**< Numerical overflow */
    ERROR_TYPE_NUMERICAL_UNDERFLOW    = 0x2003,  /**< Numerical underflow */
    ERROR_TYPE_DIVIDE_BY_ZERO         = 0x2004,  /**< Division by zero */
    ERROR_TYPE_MATRIX_SINGULAR        = 0x2005,  /**< Singular matrix */
    ERROR_TYPE_CONVERGENCE_FAILURE    = 0x2006,  /**< Algorithm failed to converge */
    ERROR_TYPE_PRECISION_LOSS         = 0x2007,  /**< Catastrophic precision loss */

    // Resource Errors (0x3000-0x3FFF)
    ERROR_TYPE_OUT_OF_MEMORY          = 0x3000,  /**< Memory allocation failed */
    ERROR_TYPE_OUT_OF_FILE_HANDLES    = 0x3001,  /**< File descriptor exhaustion */
    ERROR_TYPE_OUT_OF_THREADS         = 0x3002,  /**< Thread creation failed */
    ERROR_TYPE_DISK_FULL              = 0x3003,  /**< Disk space exhausted */
    ERROR_TYPE_NETWORK_TIMEOUT        = 0x3004,  /**< Network operation timeout */
    ERROR_TYPE_DEADLOCK               = 0x3005,  /**< Thread deadlock detected */
    ERROR_TYPE_RESOURCE_LEAK          = 0x3006,  /**< Resource handle leak */

    // Control Flow Errors (0x4000-0x4FFF)
    ERROR_TYPE_INFINITE_LOOP          = 0x4000,  /**< Infinite loop detected */
    ERROR_TYPE_ASSERTION_FAILED       = 0x4001,  /**< Runtime assertion failed */
    ERROR_TYPE_UNREACHABLE_CODE       = 0x4002,  /**< Unreachable code executed */
    ERROR_TYPE_INVALID_STATE          = 0x4003,  /**< Invalid state machine state */
    ERROR_TYPE_RACE_CONDITION         = 0x4004,  /**< Data race detected */
    ERROR_TYPE_STACK_CORRUPTION       = 0x4005,  /**< Stack frame corruption */

    // Brain-Specific Errors (0x5000-0x5FFF)
    ERROR_TYPE_INVALID_BRAIN_STATE    = 0x5000,  /**< Brain in invalid state */
    ERROR_TYPE_LAYER_MISMATCH         = 0x5001,  /**< Layer dimension mismatch */
    ERROR_TYPE_WEIGHT_CORRUPTION      = 0x5002,  /**< Neural weight corruption */
    ERROR_TYPE_ACTIVATION_ANOMALY     = 0x5003,  /**< Abnormal activation pattern */
    ERROR_TYPE_GRADIENT_EXPLOSION     = 0x5004,  /**< Gradient explosion */
    ERROR_TYPE_GRADIENT_VANISHING     = 0x5005,  /**< Gradient vanishing */
    ERROR_TYPE_PLASTICITY_FAILURE     = 0x5006,  /**< Plasticity mechanism failed */

    // Signal-Based Errors (0x6000-0x6FFF)
    ERROR_TYPE_SEGFAULT               = 0x6000,  /**< Segmentation fault (SIGSEGV) */
    ERROR_TYPE_ILLEGAL_INSTRUCTION    = 0x6001,  /**< Illegal instruction (SIGILL) */
    ERROR_TYPE_BUS_ERROR              = 0x6002,  /**< Bus error (SIGBUS) */
    ERROR_TYPE_FLOATING_POINT_ERROR   = 0x6003,  /**< FPE (SIGFPE) */
    ERROR_TYPE_ABORT                  = 0x6004,  /**< Abort signal (SIGABRT) */

    // Unknown/Other
    ERROR_TYPE_UNKNOWN                = 0x7000,  /**< Unknown error type */
    ERROR_TYPE_NONE                   = 0x7FFF   /**< No error detected */
} error_type_t;

/**
 * @brief Error severity classification for fault diagnostics
 *
 * NOTE: Uses DIAG_SEVERITY prefix to avoid conflict with wellbeing severity levels
 */
typedef enum {
    DIAG_SEVERITY_INFO,       /**< Informational, no action needed */
    DIAG_SEVERITY_WARNING,    /**< Warning, monitor but continue */
    DIAG_SEVERITY_ERROR,      /**< Error, recovery may be possible */
    DIAG_SEVERITY_CRITICAL,   /**< Critical, immediate action required */
    DIAG_SEVERITY_FATAL       /**< Fatal, system must shut down */
} diag_severity_t;

//=============================================================================
// Recovery Action Recommendations
//=============================================================================

/**
 * @brief Recovery action types
 */
typedef enum {
    RECOVERY_NONE,                    /**< No recovery action needed */
    RECOVERY_RETRY,                   /**< Retry the operation */
    RECOVERY_RESET_COMPONENT,         /**< Reset the affected component */
    RECOVERY_RELOAD_CHECKPOINT,       /**< Reload from checkpoint */
    RECOVERY_REDUCE_PRECISION,        /**< Switch to lower precision */
    RECOVERY_REDUCE_BATCH_SIZE,       /**< Reduce processing batch size */
    RECOVERY_CLEAR_CACHE,             /**< Clear internal caches */
    RECOVERY_RESTART_PROCESS,         /**< Restart entire process */
    RECOVERY_GRACEFUL_SHUTDOWN,       /**< Graceful shutdown required */
    RECOVERY_IMMEDIATE_SHUTDOWN,      /**< Immediate shutdown required */
    RECOVERY_CUSTOM                   /**< Custom recovery action */
} recovery_action_type_t;

/**
 * @brief Recovery action recommendation
 */
typedef struct {
    recovery_action_type_t type;      /**< Type of recovery action */
    float confidence;                  /**< Confidence in this action (0.0-1.0) */
    char description[256];             /**< Human-readable description */
    uint32_t estimated_cost_ms;        /**< Estimated recovery time (ms) */
    bool requires_user_intervention;   /**< Requires manual intervention */
} recovery_action_recommendation_t;

//=============================================================================
// Diagnostic Result Structure
//=============================================================================

#define MAX_STACK_DEPTH 32
#define MAX_RECOVERY_ACTIONS 8
#define MAX_RELATED_ERRORS 8

/**
 * @brief Stack frame information for crash analysis
 */
typedef struct {
    void* address;                     /**< Instruction pointer */
    char function_name[128];           /**< Function name (if available) */
    char file_name[256];               /**< Source file name (if available) */
    int line_number;                   /**< Line number (if available) */
    bool is_symbolicated;              /**< Whether symbol info is available */
} stack_frame_t;

/**
 * @brief Memory state snapshot
 */
typedef struct {
    size_t total_allocated_bytes;      /**< Total allocated memory */
    size_t peak_allocated_bytes;       /**< Peak memory usage */
    uint32_t allocation_count;         /**< Number of allocations */
    uint32_t deallocation_count;       /**< Number of deallocations */
    uint32_t leaked_blocks;            /**< Suspected leaked blocks */
    size_t leaked_bytes;               /**< Suspected leaked bytes */
    bool canary_corruption_detected;   /**< Memory canary check failed */
    void* corruption_address;          /**< Address of detected corruption */
} memory_snapshot_t;

/**
 * @brief Performance metrics snapshot
 */
typedef struct {
    double cpu_usage_percent;          /**< CPU usage (0-100) */
    double memory_usage_percent;       /**< Memory usage (0-100) */
    uint64_t instruction_count;        /**< Instructions executed */
    double execution_time_ms;          /**< Execution time (ms) */
    bool performance_anomaly_detected; /**< Abnormal performance detected */
    double anomaly_score;              /**< Anomaly severity (0.0-1.0) */
} performance_snapshot_t;

/**
 * @brief Complete diagnostic result
 *
 * WHAT: Comprehensive error analysis result with all diagnostic data
 * WHY:  Provide all information needed for root cause analysis and recovery
 * HOW:  Combines error classification, context, and recovery recommendations
 */
typedef struct diagnostic_result {
    // Error Classification
    error_type_t error_type;           /**< Primary error type */
    diag_severity_t severity;          /**< Error severity */
    float confidence;                  /**< Confidence in diagnosis (0.0-1.0) */

    // Root Cause Analysis
    char root_cause[512];              /**< Human-readable root cause */
    char symptoms[512];                /**< Observed symptoms */
    error_type_t related_errors[MAX_RELATED_ERRORS]; /**< Related error types */
    uint32_t related_error_count;      /**< Number of related errors */

    // Context Information
    int signal_number;                 /**< Signal that triggered (if any) */
    void* fault_address;               /**< Fault address (if available) */
    time_t timestamp;                  /**< When error occurred */
    uint64_t error_id;                 /**< Unique error ID */

    // Stack Trace Analysis
    stack_frame_t stack_trace[MAX_STACK_DEPTH]; /**< Stack trace */
    uint32_t stack_depth;              /**< Actual stack depth */
    char likely_faulty_function[128];  /**< Most likely faulty function */

    // Memory Analysis
    memory_snapshot_t memory_state;    /**< Memory state snapshot */
    bool memory_corruption_detected;   /**< Memory corruption flag */

    // Performance Analysis
    performance_snapshot_t performance; /**< Performance metrics */

    // Pattern Detection
    bool is_recurring;                 /**< Is this a recurring error? */
    uint32_t occurrence_count;         /**< Number of times this occurred */
    time_t first_occurrence;           /**< First occurrence timestamp */
    time_t last_occurrence;            /**< Last occurrence timestamp */

    // Recovery Recommendations
    recovery_action_recommendation_t recovery_actions[MAX_RECOVERY_ACTIONS]; /**< Recommended actions */
    uint32_t recovery_action_count;    /**< Number of actions */

    // Additional Metadata
    char diagnostic_version[32];       /**< Diagnostic system version */
    bool self_healing_attempted;       /**< Was self-healing tried? */
    bool self_healing_successful;      /**< Did self-healing succeed? */
} diagnostic_result_t;

//=============================================================================
// Diagnostic History for Pattern Detection
//=============================================================================

#define MAX_HISTORY_SIZE 100

/**
 * @brief Historical error record
 */
typedef struct {
    error_type_t error_type;
    time_t timestamp;
    void* fault_address;
    char function_name[128];
} error_record_t;

/**
 * @brief Diagnostic history for pattern detection
 */
typedef struct {
    error_record_t records[MAX_HISTORY_SIZE];
    uint32_t count;
    uint32_t write_index;
    bool is_full;
} diagnostic_history_t;

//=============================================================================
// Crash Context (Platform-Specific)
//=============================================================================

/**
 * @brief Crash context information from signal handler
 */
typedef struct {
    int signal;                        /**< Signal number */
    int code;                          /**< Signal code */
    void* fault_address;               /**< Fault address */
    void* instruction_pointer;         /**< Instruction pointer */
    void* stack_pointer;               /**< Stack pointer */
    void* frame_pointer;               /**< Frame pointer */
    time_t timestamp;                  /**< Crash timestamp */

    // Extended context (platform-specific)
    void* extended_context;            /**< Platform-specific context */
} crash_context_t;

//=============================================================================
// API Functions
//=============================================================================

/**
 * @brief Initialize the diagnostic system
 *
 * WHAT: Initialize diagnostic subsystem with memory tracking and logging
 * WHY:  Must be called before any diagnostic operations
 * HOW:  Sets up error history, memory tracking, and signal handlers
 *
 * @param log_file Path to diagnostic log file (NULL for default)
 * @return true on success, false on failure
 */
bool diagnostics_init(const char* log_file);

/**
 * @brief Shutdown the diagnostic system
 *
 * WHAT: Clean up diagnostic resources and flush logs
 * WHY:  Ensure all diagnostic data is saved before exit
 * HOW:  Writes final reports and frees resources
 */
void diagnostics_shutdown(void);

//=============================================================================
// Error Analysis Functions
//=============================================================================

/**
 * @brief Analyze a crash from signal handler context
 *
 * WHAT: Comprehensive crash analysis from signal handler
 * WHY:  Understand what caused a crash and how to recover
 * HOW:  Analyzes signal, stack trace, memory state, and patterns
 *
 * @param signal Signal number (SIGSEGV, SIGFPE, etc.)
 * @param crash_context Crash context from signal handler
 * @return Diagnostic result (caller must free with diagnostics_free_result)
 */
diagnostic_result_t* diagnostics_analyze_crash(int signal, crash_context_t* crash_context);

/**
 * @brief Analyze a stack trace for error patterns
 *
 * WHAT: Analyze stack trace to identify likely error location
 * WHY:  Pinpoint the function and call chain that caused the error
 * HOW:  Symbolicate addresses, identify patterns, check for known issues
 *
 * @param trace Array of instruction pointers
 * @param depth Number of frames in trace
 * @return Diagnostic result (caller must free)
 */
diagnostic_result_t* diagnostics_analyze_stack_trace(void** trace, int depth);

/**
 * @brief Analyze brain memory state for corruption
 *
 * WHAT: Deep inspection of brain structure for memory corruption
 * WHY:  Detect silent corruption before it causes crashes
 * HOW:  Checks canaries, bounds, pointer validity, numerical sanity
 *
 * @param brain Brain to analyze
 * @return Diagnostic result (caller must free)
 */
diagnostic_result_t* diagnostics_analyze_memory_state(brain_t brain);

/**
 * @brief Analyze brain for numerical instability
 *
 * WHAT: Check for NaN/Inf propagation and numerical errors
 * WHY:  Detect numerical instability before it corrupts results
 * HOW:  Scans weights, activations, gradients for invalid values
 *
 * @param brain Brain to analyze
 * @return Diagnostic result (caller must free)
 */
diagnostic_result_t* diagnostics_analyze_numerical_stability(brain_t brain);

//=============================================================================
// Pattern Detection Functions
//=============================================================================

/**
 * @brief Detect crash patterns in history
 *
 * WHAT: Identify recurring crash patterns for preventive action
 * WHY:  Detect systematic issues that require architectural fixes
 * HOW:  Analyzes error history for temporal/spatial patterns
 *
 * @param history Diagnostic history
 * @return true if pattern detected, false otherwise
 */
bool diagnostics_detect_crash_pattern(diagnostic_history_t* history);

/**
 * @brief Detect memory corruption in brain structure
 *
 * WHAT: Scan brain for memory corruption indicators
 * WHY:  Early detection prevents crashes and data loss
 * HOW:  Validates canaries, bounds, pointer chains, checksums
 *
 * @param brain Brain to check
 * @return true if corruption detected, false otherwise
 */
bool diagnostics_detect_memory_corruption(brain_t brain);

/**
 * @brief Detect numerical instability in brain
 *
 * WHAT: Check for NaN/Inf in brain numerical values
 * WHY:  Prevent numerical errors from propagating
 * HOW:  Scans all floating-point arrays for invalid values
 *
 * @param brain Brain to check
 * @return true if instability detected, false otherwise
 */
bool diagnostics_detect_numerical_instability(brain_t brain);

/**
 * @brief Detect infinite loop condition
 *
 * WHAT: Detect if execution is stuck in infinite loop
 * WHY:  Prevent system hang and resource exhaustion
 * HOW:  Monitors instruction pointer and execution patterns
 *
 * @param instruction_pointer Current instruction pointer
 * @param threshold_ms Time threshold for detection (ms)
 * @return true if infinite loop detected, false otherwise
 */
bool diagnostics_detect_infinite_loop(void* instruction_pointer, uint32_t threshold_ms);

/**
 * @brief Detect resource exhaustion
 *
 * WHAT: Check for resource exhaustion conditions
 * WHY:  Prevent OOM kills and resource starvation
 * HOW:  Monitors memory, file handles, threads
 *
 * @param threshold_percent Threshold percentage (0-100)
 * @return Error type if exhaustion detected, ERROR_TYPE_NONE otherwise
 */
error_type_t diagnostics_detect_resource_exhaustion(float threshold_percent);

//=============================================================================
// Reporting Functions
//=============================================================================

/**
 * @brief Report diagnostic result to log
 *
 * WHAT: Write formatted diagnostic report to log file
 * WHY:  Persist diagnostic data for debugging and analysis
 * HOW:  Formats result as structured log entry
 *
 * @param result Diagnostic result to log
 */
void diagnostics_report_to_log(const diagnostic_result_t* result);

/**
 * @brief Report diagnostic result to file
 *
 * WHAT: Write diagnostic report to specified file
 * WHY:  Create standalone diagnostic reports for sharing
 * HOW:  Generates detailed human-readable report
 *
 * @param result Diagnostic result to report
 * @param path Output file path
 * @return true on success, false on failure
 */
bool diagnostics_report_to_file(const diagnostic_result_t* result, const char* path);

/**
 * @brief Generate JSON diagnostic report
 *
 * WHAT: Serialize diagnostic result to JSON
 * WHY:  Enable programmatic analysis and integration
 * HOW:  Converts result to JSON string
 *
 * @param result Diagnostic result
 * @return JSON string (caller must free) or NULL on error
 */
char* diagnostics_report_to_json(const diagnostic_result_t* result);

//=============================================================================
// Recovery Suggestion Functions
//=============================================================================

/**
 * @brief Suggest recovery actions for diagnostic result
 *
 * WHAT: Generate prioritized recovery action recommendations
 * WHY:  Guide automatic or manual recovery efforts
 * HOW:  Maps error types to recovery strategies with confidence scores
 *
 * @param result Diagnostic result
 * @return Array of recovery actions (embedded in result)
 */
void diagnostics_suggest_recovery(diagnostic_result_t* result);

/**
 * @brief Execute automatic recovery if possible
 *
 * WHAT: Attempt automatic recovery based on diagnostic result
 * WHY:  Enable self-healing for known error patterns
 * HOW:  Executes top-ranked recovery action if confidence is high
 *
 * @param result Diagnostic result with recovery suggestions
 * @param brain Brain to recover (NULL for system-level recovery)
 * @return true if recovery successful, false otherwise
 */
bool diagnostics_auto_recover(diagnostic_result_t* result, brain_t brain);

//=============================================================================
// History Management Functions
//=============================================================================

/**
 * @brief Create diagnostic history tracker
 *
 * WHAT: Allocate and initialize diagnostic history
 * WHY:  Track error patterns over time
 * HOW:  Circular buffer for recent errors
 *
 * @return New history tracker (caller must free)
 */
diagnostic_history_t* diagnostics_create_history(void);

/**
 * @brief Add error to history
 *
 * WHAT: Record error in diagnostic history
 * WHY:  Build historical context for pattern detection
 * HOW:  Adds to circular buffer, updates statistics
 *
 * @param history History tracker
 * @param result Diagnostic result to add
 */
void diagnostics_add_to_history(diagnostic_history_t* history, const diagnostic_result_t* result);

/**
 * @brief Free diagnostic history
 *
 * WHAT: Deallocate history tracker
 * WHY:  Clean up resources
 * HOW:  Frees memory
 *
 * @param history History to free
 */
void diagnostics_free_history(diagnostic_history_t* history);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Free diagnostic result
 *
 * WHAT: Deallocate diagnostic result
 * WHY:  Prevent memory leaks
 * HOW:  Frees result and all nested allocations
 *
 * @param result Result to free
 */
void diagnostics_free_result(diagnostic_result_t* result);

/**
 * @brief Get error type name
 *
 * WHAT: Get human-readable error type name
 * WHY:  Enable readable error reporting
 * HOW:  Maps error code to string
 *
 * @param type Error type
 * @return String name (static, do not free)
 */
const char* diagnostics_get_error_type_name(error_type_t type);

/**
 * @brief Get severity name
 *
 * WHAT: Get human-readable severity name
 * WHY:  Enable readable severity reporting
 * HOW:  Maps severity code to string
 *
 * @param severity Severity level
 * @return String name (static, do not free)
 */
const char* diagnostics_get_severity_name(diag_severity_t severity);

/**
 * @brief Get recovery action name
 *
 * WHAT: Get human-readable recovery action name
 * WHY:  Enable readable action reporting
 * HOW:  Maps action type to string
 *
 * @param action Recovery action type
 * @return String name (static, do not free)
 */
const char* diagnostics_get_recovery_action_name(recovery_action_type_t action);

/**
 * @brief Capture current stack trace
 *
 * WHAT: Capture current execution stack
 * WHY:  Enable stack analysis at any point
 * HOW:  Uses platform-specific backtrace facilities
 *
 * @param buffer Output buffer for stack frames
 * @param max_depth Maximum depth to capture
 * @return Number of frames captured
 */
int diagnostics_capture_stack_trace(void** buffer, int max_depth);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_DIAGNOSTICS_H
