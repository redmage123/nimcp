/**
 * @file nimcp_exception.h
 * @brief Exception handling system with brain immune integration
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: Comprehensive exception handling system for NIMCP
 * WHY:  Integrate error handling with brain immune system for automatic recovery
 * HOW:  Exceptions become antigens presented to immune system; antibodies
 *       execute recovery actions (GC, restart, rollback, quarantine)
 *
 * ARCHITECTURE:
 * ```
 * Exception Raised -> Log -> Present as Antigen -> Immune Response -> Recovery
 *                                    |
 *                    B Cell Activation -> Antibody Production
 *                                    |
 *                    Execute Antibody (GC, restart, rollback, quarantine)
 *                                    |
 *                    Memory Formation (learn pattern for future)
 * ```
 *
 * EXCEPTION HIERARCHY (C polymorphism via embedded base struct):
 * - nimcp_exception_t (base)
 *   - nimcp_memory_exception_t
 *   - nimcp_brain_exception_t
 *   - nimcp_io_exception_t
 *   - nimcp_threading_exception_t
 *   - nimcp_security_exception_t
 *   - nimcp_cognitive_exception_t
 *   - nimcp_gpu_exception_t
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_EXCEPTION_H
#define NIMCP_EXCEPTION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define NIMCP_EXCEPTION_MAX_MESSAGE         256   /**< Max exception message length */
#define NIMCP_EXCEPTION_MAX_STACK_DEPTH     32    /**< Max stack trace depth */
#define NIMCP_EXCEPTION_EPITOPE_SIZE        64    /**< Immune epitope size */
#define NIMCP_EXCEPTION_MAX_CONTEXT         512   /**< Max context data size */
#define NIMCP_EXCEPTION_MAX_CHILDREN        16    /**< Max child exceptions in aggregate */
#define NIMCP_EXCEPTION_MAX_CONTEXT_ENTRIES 16    /**< Max context key-value entries */
#define NIMCP_EXCEPTION_MAX_CONTEXT_KEY     32    /**< Max context key length */
#define NIMCP_EXCEPTION_MAX_CONTEXT_VALUE   128   /**< Max context value length */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct nimcp_exception nimcp_exception_t;
typedef struct nimcp_exception_handler nimcp_exception_handler_t;
typedef struct nimcp_aggregate_exception nimcp_aggregate_exception_t;
typedef struct nimcp_exception_context_entry nimcp_exception_context_entry_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Exception severity levels
 *
 * Maps to immune system severity (1-10 scale for integration)
 */
typedef enum {
    EXCEPTION_SEVERITY_DEBUG = 1,      /**< Debug info, no immune action */
    EXCEPTION_SEVERITY_INFO = 2,       /**< Informational, log only */
    EXCEPTION_SEVERITY_WARNING = 3,    /**< Warning, may need attention */
    EXCEPTION_SEVERITY_ERROR = 5,      /**< Error, needs handling */
    EXCEPTION_SEVERITY_SEVERE = 7,     /**< Severe, triggers immune response */
    EXCEPTION_SEVERITY_CRITICAL = 9,   /**< Critical, immediate immune action */
    EXCEPTION_SEVERITY_FATAL = 10      /**< Fatal, emergency response */
} nimcp_exception_severity_t;

/**
 * @brief Exception categories (maps to error code ranges)
 */
typedef enum {
    EXCEPTION_CATEGORY_GENERIC = 1,    /**< Generic errors (1000-1999) */
    EXCEPTION_CATEGORY_MEMORY = 2,     /**< Memory errors (2000-2999) */
    EXCEPTION_CATEGORY_BRAIN = 3,      /**< Brain/Network errors (3000-3999) */
    EXCEPTION_CATEGORY_IO = 4,         /**< I/O errors (4000-4999) */
    EXCEPTION_CATEGORY_CONFIG = 5,     /**< Configuration errors (5000-5999) */
    EXCEPTION_CATEGORY_THREADING = 6,  /**< Threading errors (6000-6999) */
    EXCEPTION_CATEGORY_SIGNAL = 7,     /**< Signal/Crash errors (7000-7999) */
    EXCEPTION_CATEGORY_COGNITIVE = 8,  /**< Cognitive errors (8000-8999) */
    EXCEPTION_CATEGORY_GPU = 11,       /**< GPU errors (1100-1199) */
    EXCEPTION_CATEGORY_BRAIN_REGION = 10, /**< Brain region errors (10000-19999) */
    EXCEPTION_CATEGORY_SECURITY = 20   /**< Security-related exceptions */
} nimcp_exception_category_t;

/**
 * @brief Exception type identifiers (for C polymorphism)
 */
typedef enum {
    EXCEPTION_TYPE_BASE = 0,           /**< Base exception */
    EXCEPTION_TYPE_MEMORY,             /**< Memory exception */
    EXCEPTION_TYPE_BRAIN,              /**< Brain/neural exception */
    EXCEPTION_TYPE_IO,                 /**< I/O exception */
    EXCEPTION_TYPE_THREADING,          /**< Threading exception */
    EXCEPTION_TYPE_SECURITY,           /**< Security exception */
    EXCEPTION_TYPE_COGNITIVE,          /**< Cognitive exception */
    EXCEPTION_TYPE_GPU,                /**< GPU exception */
    EXCEPTION_TYPE_AGGREGATE,          /**< Aggregate exception (contains children) */
    EXCEPTION_TYPE_SIGNAL              /**< Signal/crash exception */
} nimcp_exception_type_t;

/**
 * @brief Exception recovery action types (maps to immune antibody responses)
 *
 * NOTE: These use EXCEPTION_RECOVERY_* prefix to avoid conflicts with
 * fault_tolerance/nimcp_recovery.h which defines RECOVERY_ACTION_* values.
 */
typedef enum {
    EXCEPTION_RECOVERY_NONE = 0,          /**< No recovery needed */
    EXCEPTION_RECOVERY_RETRY,             /**< Retry the operation */
    EXCEPTION_RECOVERY_GC,                /**< Trigger garbage collection */
    EXCEPTION_RECOVERY_COMPACT,           /**< Memory compaction */
    EXCEPTION_RECOVERY_ROLLBACK,          /**< Rollback to checkpoint */
    EXCEPTION_RECOVERY_RESTART_THREAD,    /**< Restart affected thread */
    EXCEPTION_RECOVERY_RESTART_COMPONENT, /**< Restart affected component */
    EXCEPTION_RECOVERY_QUARANTINE,        /**< Quarantine affected region */
    EXCEPTION_RECOVERY_REDUCE_LOAD,       /**< Reduce system load */
    EXCEPTION_RECOVERY_CLEAR_CACHE,       /**< Clear caches */
    EXCEPTION_RECOVERY_EMERGENCY_SAVE,    /**< Emergency state save */
    EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN  /**< Graceful shutdown */
} nimcp_exception_recovery_action_t;

/* ============================================================================
 * Context Key-Value Entry
 * ============================================================================ */

/**
 * @brief Context key-value entry for structured exception data
 *
 * Allows attaching arbitrary key-value metadata to exceptions beyond
 * the message string.
 */
struct nimcp_exception_context_entry {
    char key[NIMCP_EXCEPTION_MAX_CONTEXT_KEY];     /**< Context key */
    char value[NIMCP_EXCEPTION_MAX_CONTEXT_VALUE]; /**< Context value */
};

/* ============================================================================
 * Stack Trace
 * ============================================================================ */

/**
 * @brief Stack frame information
 */
typedef struct {
    void* address;                     /**< Return address */
    const char* function;              /**< Function name (may be NULL) */
    const char* file;                  /**< Source file (may be NULL) */
    int line;                          /**< Source line (0 if unknown) */
} nimcp_stack_frame_t;

/**
 * @brief Stack trace
 */
typedef struct {
    nimcp_stack_frame_t frames[NIMCP_EXCEPTION_MAX_STACK_DEPTH];
    size_t depth;                      /**< Number of frames captured */
} nimcp_stack_trace_t;

/* ============================================================================
 * Base Exception Structure
 * ============================================================================ */

/**
 * @brief Base exception structure
 *
 * All derived exception types embed this as the first member,
 * allowing C polymorphism via casting.
 */
struct nimcp_exception {
    /* Type identification */
    nimcp_exception_type_t type;       /**< Exception type for polymorphism */
    nimcp_exception_category_t category; /**< Exception category */

    /* Error information */
    nimcp_error_t code;                /**< NIMCP error code */
    nimcp_exception_severity_t severity; /**< Severity level */
    char message[NIMCP_EXCEPTION_MAX_MESSAGE]; /**< Human-readable message */

    /* Source location */
    const char* file;                  /**< Source file (__FILE__) */
    int line;                          /**< Source line (__LINE__) */
    const char* function;              /**< Function name (__func__) */

    /* Timing */
    uint64_t timestamp_us;             /**< Timestamp in microseconds */

    /* Stack trace */
    nimcp_stack_trace_t stack_trace;   /**< Captured stack trace */

    /* Immune integration */
    uint8_t epitope[NIMCP_EXCEPTION_EPITOPE_SIZE]; /**< Immune fingerprint */
    size_t epitope_len;                /**< Epitope length */
    bool presented_to_immune;          /**< Already presented to immune */
    uint32_t antigen_id;               /**< Assigned antigen ID (if presented) */

    /* Recovery */
    nimcp_exception_recovery_action_t suggested_action; /**< Suggested recovery action */
    bool recovery_attempted;           /**< Recovery was attempted */
    bool recovery_succeeded;           /**< Recovery succeeded */

    /* Reference counting */
    volatile int32_t ref_count;        /**< Reference count for sharing */

    /* Chaining (cause) */
    nimcp_exception_t* cause;          /**< Underlying cause (may be NULL) */

    /* Structured context data */
    nimcp_exception_context_entry_t context[NIMCP_EXCEPTION_MAX_CONTEXT_ENTRIES];
    size_t context_count;              /**< Number of context entries */
};

/* ============================================================================
 * Derived Exception Types
 * ============================================================================ */

/**
 * @brief Memory exception with allocation details
 */
typedef struct {
    nimcp_exception_t base;            /**< Base exception (must be first) */

    /* Memory-specific fields */
    size_t requested_size;             /**< Requested allocation size */
    size_t available_size;             /**< Available memory at time of error */
    void* failed_address;              /**< Address that caused the error */
    const char* allocator_name;        /**< Name of allocator (pool, arena, etc.) */
    bool is_heap;                      /**< Heap vs. stack allocation */
} nimcp_memory_exception_t;

/**
 * @brief Brain/neural network exception
 */
typedef struct {
    nimcp_exception_t base;            /**< Base exception (must be first) */

    /* Brain-specific fields */
    uint32_t brain_id;                 /**< Affected brain instance ID */
    uint32_t network_id;               /**< Affected network ID */
    uint32_t layer_id;                 /**< Affected layer ID */
    const char* region_name;           /**< Brain region name */
    float gradient_norm;               /**< Gradient norm (for NaN detection) */
    bool has_nan_weights;              /**< NaN detected in weights */
    bool learning_diverged;            /**< Learning process diverged */
} nimcp_brain_exception_t;

/**
 * @brief I/O exception with file/network details
 */
typedef struct {
    nimcp_exception_t base;            /**< Base exception (must be first) */

    /* I/O-specific fields */
    const char* path;                  /**< File path or network address */
    int errno_value;                   /**< System errno value */
    size_t bytes_transferred;          /**< Bytes transferred before error */
    size_t bytes_expected;             /**< Expected transfer size */
    bool is_network;                   /**< Network vs. file I/O */
    int socket_fd;                     /**< Socket FD (if network) */
} nimcp_io_exception_t;

/**
 * @brief Threading exception
 */
typedef struct {
    nimcp_exception_t base;            /**< Base exception (must be first) */

    /* Threading-specific fields */
    uint64_t thread_id;                /**< Affected thread ID */
    const char* thread_name;           /**< Thread name (if named) */
    void* mutex_address;               /**< Mutex involved (if any) */
    uint64_t lock_wait_time_us;        /**< Time spent waiting for lock */
    bool is_deadlock;                  /**< Deadlock detected */
    uint32_t deadlock_cycle_len;       /**< Deadlock cycle length */
} nimcp_threading_exception_t;

/**
 * @brief Security exception
 */
typedef struct {
    nimcp_exception_t base;            /**< Base exception (must be first) */

    /* Security-specific fields */
    uint32_t threat_type;              /**< BBB threat type */
    uint32_t source_node_id;           /**< Source of security threat */
    const char* threat_signature;      /**< Threat signature */
    bool quarantine_required;          /**< Requires quarantine */
    uint8_t severity_score;            /**< Security severity (1-10) */
} nimcp_security_exception_t;

/**
 * @brief Cognitive/processing exception
 */
typedef struct {
    nimcp_exception_t base;            /**< Base exception (must be first) */

    /* Cognitive-specific fields */
    uint32_t module_id;                /**< Cognitive module ID */
    const char* module_name;           /**< Module name */
    uint32_t task_id;                  /**< Current task ID */
    float confidence_score;            /**< Processing confidence */
    bool is_timeout;                   /**< Processing timeout */
    uint64_t processing_time_us;       /**< Time spent processing */
} nimcp_cognitive_exception_t;

/**
 * @brief GPU exception
 */
typedef struct {
    nimcp_exception_t base;            /**< Base exception (must be first) */

    /* GPU-specific fields */
    int device_id;                     /**< GPU device ID */
    int cuda_error;                    /**< CUDA error code */
    const char* kernel_name;           /**< Failed kernel name */
    size_t gpu_memory_used;            /**< GPU memory in use */
    size_t gpu_memory_total;           /**< Total GPU memory */
    uint32_t sm_id;                    /**< Streaming multiprocessor ID */
} nimcp_gpu_exception_t;

/**
 * @brief Signal/crash exception with crash context
 *
 * WHAT: Exception for signal-based crashes (SIGSEGV, SIGFPE, etc.)
 * WHY:  Integrate signal handling with exception hierarchy for unified error handling
 * HOW:  Capture crash context in signal handler, convert to exception for processing
 *
 * USAGE:
 *   // After siglongjmp recovery:
 *   nimcp_signal_exception_t* ex = nimcp_signal_exception_create_from_context(&ctx);
 *   nimcp_exception_present_to_immune((nimcp_exception_t*)ex, NULL);
 *   nimcp_exception_dispatch((nimcp_exception_t*)ex);
 */
#define NIMCP_SIGNAL_EXCEPTION_MEMORY_REGION_SIZE 256

typedef struct {
    nimcp_exception_t base;            /**< Base exception (must be first) */

    /* Signal-specific fields */
    int signal_number;                 /**< Signal number (SIGSEGV, SIGFPE, etc.) */
    void* fault_address;               /**< Address that caused the fault */
    void* instruction_pointer;         /**< Instruction pointer at crash time */
    void* stack_pointer;               /**< Stack pointer at crash time */
    void* base_pointer;                /**< Base pointer at crash time */
    char memory_region[NIMCP_SIGNAL_EXCEPTION_MEMORY_REGION_SIZE]; /**< Memory region from /proc/self/maps */
    bool recovery_attempted;           /**< Whether immune recovery was attempted */
    bool siglongjmp_executed;          /**< Whether we recovered via siglongjmp */
    int retry_count;                   /**< Number of retry attempts made */
} nimcp_signal_exception_t;

/**
 * @brief Aggregate exception for batch processing
 *
 * WHAT: Container for multiple related exceptions
 * WHY:  Batch operations may produce multiple errors; aggregate them
 * HOW:  Embed base exception + array of child exception pointers
 *
 * Example usage:
 *   nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
 *       __FILE__, __LINE__, __func__, "Batch operation failed");
 *   nimcp_aggregate_exception_add(agg, child1);
 *   nimcp_aggregate_exception_add(agg, child2);
 */
struct nimcp_aggregate_exception {
    nimcp_exception_t base;            /**< Base exception (must be first) */

    /* Aggregate-specific fields */
    nimcp_exception_t* children[NIMCP_EXCEPTION_MAX_CHILDREN]; /**< Child exceptions */
    size_t child_count;                /**< Number of child exceptions */
};

/* ============================================================================
 * Exception Creation API
 * ============================================================================ */

/**
 * @brief Create a base exception
 *
 * WHAT: Allocate and initialize a new exception
 * WHY:  Primary exception creation entry point
 * HOW:  Allocate, set fields, capture stack trace
 *
 * @param code NIMCP error code
 * @param severity Severity level
 * @param file Source file (__FILE__)
 * @param line Source line (__LINE__)
 * @param func Function name (__func__)
 * @param format Printf-style message format
 * @param ... Format arguments
 * @return New exception or NULL on allocation failure
 */
nimcp_exception_t* nimcp_exception_create(
    nimcp_error_t code,
    nimcp_exception_severity_t severity,
    const char* file,
    int line,
    const char* func,
    const char* format,
    ...
);

/**
 * @brief Create a memory exception
 */
nimcp_memory_exception_t* nimcp_memory_exception_create(
    nimcp_error_t code,
    nimcp_exception_severity_t severity,
    const char* file,
    int line,
    const char* func,
    size_t requested_size,
    const char* format,
    ...
);

/**
 * @brief Create a brain exception
 */
nimcp_brain_exception_t* nimcp_brain_exception_create(
    nimcp_error_t code,
    nimcp_exception_severity_t severity,
    const char* file,
    int line,
    const char* func,
    uint32_t brain_id,
    const char* region_name,
    const char* format,
    ...
);

/**
 * @brief Create an I/O exception
 */
nimcp_io_exception_t* nimcp_io_exception_create(
    nimcp_error_t code,
    nimcp_exception_severity_t severity,
    const char* file,
    int line,
    const char* func,
    const char* path,
    const char* format,
    ...
);

/**
 * @brief Create a threading exception
 */
nimcp_threading_exception_t* nimcp_threading_exception_create(
    nimcp_error_t code,
    nimcp_exception_severity_t severity,
    const char* file,
    int line,
    const char* func,
    uint64_t thread_id,
    const char* format,
    ...
);

/**
 * @brief Create a security exception
 */
nimcp_security_exception_t* nimcp_security_exception_create(
    nimcp_error_t code,
    nimcp_exception_severity_t severity,
    const char* file,
    int line,
    const char* func,
    uint32_t threat_type,
    const char* format,
    ...
);

/**
 * @brief Create a GPU exception
 */
nimcp_gpu_exception_t* nimcp_gpu_exception_create(
    nimcp_error_t code,
    nimcp_exception_severity_t severity,
    const char* file,
    int line,
    const char* func,
    int device_id,
    int cuda_error,
    const char* format,
    ...
);

/* ============================================================================
 * Signal Exception API
 * ============================================================================ */

/* Forward declaration for signal crash context */
struct signal_crash_context;

/**
 * @brief Create a signal exception
 *
 * WHAT: Create exception from a signal number with crash details
 * WHY:  Convert signal crashes to unified exception handling
 * HOW:  Allocate signal exception struct, populate from parameters
 *
 * @param signal_number Signal number (SIGSEGV, SIGFPE, etc.)
 * @param fault_address Address that caused the fault (may be NULL)
 * @param file Source file (__FILE__)
 * @param line Source line (__LINE__)
 * @param func Function name (__func__)
 * @param format Printf-style message format
 * @param ... Format arguments
 * @return New signal exception or NULL on allocation failure
 */
nimcp_signal_exception_t* nimcp_signal_exception_create(
    int signal_number,
    void* fault_address,
    const char* file,
    int line,
    const char* func,
    const char* format,
    ...
);

/**
 * @brief Create a signal exception from crash context
 *
 * WHAT: Create exception from full crash context captured by signal handler
 * WHY:  Preserve all crash information for diagnostics and immune processing
 * HOW:  Copy crash context fields into signal exception structure
 *
 * @param ctx Crash context from signal handler (must not be NULL)
 * @return New signal exception or NULL on allocation failure or NULL ctx
 */
nimcp_signal_exception_t* nimcp_signal_exception_create_from_context(
    const struct signal_crash_context* ctx
);

/**
 * @brief Get error code from signal number
 *
 * Maps signal numbers to NIMCP error codes:
 * - SIGSEGV -> NIMCP_ERROR_SIGSEGV (7001)
 * - SIGABRT -> NIMCP_ERROR_SIGABRT (7002)
 * - SIGFPE  -> NIMCP_ERROR_SIGFPE  (7003)
 * - SIGBUS  -> NIMCP_ERROR_SIGBUS  (7004)
 * - SIGILL  -> NIMCP_ERROR_SIGILL  (7005)
 *
 * @param signal_number Signal number
 * @return Corresponding NIMCP error code
 */
nimcp_error_t nimcp_signal_to_error_code(int signal_number);

/**
 * @brief Get signal name as string
 *
 * @param signal_number Signal number
 * @return Static string name (e.g., "SIGSEGV")
 */
const char* nimcp_signal_name(int signal_number);

/* ============================================================================
 * Aggregate Exception API
 * ============================================================================ */

/**
 * @brief Create an aggregate exception
 *
 * WHAT: Create a container for multiple related exceptions
 * WHY:  Batch operations may produce multiple errors
 * HOW:  Allocate aggregate structure, initialize base exception
 *
 * @param file Source file (__FILE__)
 * @param line Source line (__LINE__)
 * @param func Function name (__func__)
 * @param format Printf-style message format
 * @param ... Format arguments
 * @return New aggregate exception or NULL on allocation failure
 */
nimcp_aggregate_exception_t* nimcp_aggregate_exception_create(
    nimcp_error_t code,
    nimcp_exception_severity_t severity,
    const char* file,
    int line,
    const char* func,
    const char* format,
    ...
);

/**
 * @brief Add a child exception to an aggregate
 *
 * Takes ownership of a reference to the child exception.
 *
 * @param agg Aggregate exception
 * @param child Child exception to add (reference is taken)
 * @return 0 on success, -1 if aggregate is full or NULL parameters
 */
int nimcp_aggregate_exception_add(nimcp_aggregate_exception_t* agg, nimcp_exception_t* child);

/**
 * @brief Get the number of child exceptions in an aggregate
 *
 * @param agg Aggregate exception
 * @return Number of child exceptions, 0 if NULL
 */
size_t nimcp_aggregate_exception_count(const nimcp_aggregate_exception_t* agg);

/**
 * @brief Get a child exception by index
 *
 * @param agg Aggregate exception
 * @param index Child index (0-based)
 * @return Child exception or NULL if index out of bounds
 */
nimcp_exception_t* nimcp_aggregate_exception_get(const nimcp_aggregate_exception_t* agg, size_t index);

/* ============================================================================
 * Exception Context API
 * ============================================================================ */

/**
 * @brief Set a context key-value pair on an exception
 *
 * WHAT: Attach structured metadata to an exception
 * WHY:  Provide additional debugging context beyond the message
 * HOW:  Find or create entry in context array
 *
 * @param ex Exception to modify
 * @param key Context key (max NIMCP_EXCEPTION_MAX_CONTEXT_KEY-1 chars)
 * @param value Context value (max NIMCP_EXCEPTION_MAX_CONTEXT_VALUE-1 chars)
 * @return 0 on success, -1 if context is full or NULL parameters
 */
int nimcp_exception_set_context(nimcp_exception_t* ex, const char* key, const char* value);

/**
 * @brief Get a context value by key
 *
 * @param ex Exception to query
 * @param key Context key to look up
 * @return Context value or NULL if not found
 */
const char* nimcp_exception_get_context(const nimcp_exception_t* ex, const char* key);

/**
 * @brief Remove a context entry by key
 *
 * @param ex Exception to modify
 * @param key Context key to remove
 * @return 0 if removed, -1 if not found or NULL parameters
 */
int nimcp_exception_remove_context(nimcp_exception_t* ex, const char* key);

/**
 * @brief Get the number of context entries
 *
 * @param ex Exception to query
 * @return Number of context entries, 0 if NULL
 */
size_t nimcp_exception_context_count(const nimcp_exception_t* ex);

/* ============================================================================
 * Exception Lifecycle API
 * ============================================================================ */

/**
 * @brief Add reference to exception
 *
 * @param ex Exception
 * @return Same exception (for chaining)
 */
nimcp_exception_t* nimcp_exception_ref(nimcp_exception_t* ex);

/**
 * @brief Release reference to exception
 *
 * Frees exception when reference count reaches zero.
 *
 * @param ex Exception to release
 */
void nimcp_exception_unref(nimcp_exception_t* ex);

/**
 * @brief Chain exceptions (set cause)
 *
 * @param ex Exception to modify
 * @param cause Underlying cause (takes ownership of reference)
 */
void nimcp_exception_set_cause(nimcp_exception_t* ex, nimcp_exception_t* cause);

/**
 * @brief Get exception cause chain
 *
 * @param ex Exception
 * @return Cause exception or NULL
 */
nimcp_exception_t* nimcp_exception_get_cause(nimcp_exception_t* ex);

/* ============================================================================
 * Exception Information API
 * ============================================================================ */

/**
 * @brief Get exception category from error code
 *
 * @param code NIMCP error code
 * @return Exception category
 */
nimcp_exception_category_t nimcp_exception_get_category_from_code(nimcp_error_t code);

/**
 * @brief Get severity from error code (heuristic)
 *
 * @param code NIMCP error code
 * @return Suggested severity
 */
nimcp_exception_severity_t nimcp_exception_get_severity_from_code(nimcp_error_t code);

/**
 * @brief Capture current stack trace
 *
 * @param trace Output trace structure
 * @param skip_frames Number of frames to skip (caller frames)
 * @return Number of frames captured
 */
size_t nimcp_exception_capture_stack_trace(nimcp_stack_trace_t* trace, int skip_frames);

/**
 * @brief Generate immune epitope from exception
 *
 * Creates a 64-byte fingerprint for immune system pattern matching.
 *
 * @param ex Exception
 * @return Length of generated epitope
 */
size_t nimcp_exception_generate_epitope(nimcp_exception_t* ex);

/**
 * @brief Get suggested recovery action for exception
 *
 * @param ex Exception
 * @return Suggested recovery action
 */
nimcp_exception_recovery_action_t nimcp_exception_get_suggested_recovery(nimcp_exception_t* ex);

/* ============================================================================
 * Exception Logging/Printing API
 * ============================================================================ */

/**
 * @brief Log exception to system logger
 *
 * @param ex Exception to log
 */
void nimcp_exception_log(const nimcp_exception_t* ex);

/**
 * @brief Print exception to stderr
 *
 * @param ex Exception to print
 */
void nimcp_exception_print(const nimcp_exception_t* ex);

/**
 * @brief Format exception as string
 *
 * @param ex Exception
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of characters written
 */
size_t nimcp_exception_to_string(
    const nimcp_exception_t* ex,
    char* buffer,
    size_t buffer_size
);

/**
 * @brief Format stack trace as string
 *
 * @param trace Stack trace
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of characters written
 */
size_t nimcp_stack_trace_to_string(
    const nimcp_stack_trace_t* trace,
    char* buffer,
    size_t buffer_size
);

/* ============================================================================
 * String Conversion
 * ============================================================================ */

const char* nimcp_exception_severity_to_string(nimcp_exception_severity_t severity);
const char* nimcp_exception_category_to_string(nimcp_exception_category_t category);
const char* nimcp_exception_type_to_string(nimcp_exception_type_t type);
const char* nimcp_exception_recovery_action_to_string(nimcp_exception_recovery_action_t action);

/* ============================================================================
 * Thread-Local Exception Context
 * ============================================================================ */

/**
 * @brief Set thread-local current exception
 *
 * @param ex Exception (takes reference)
 */
void nimcp_exception_set_current(nimcp_exception_t* ex);

/**
 * @brief Get thread-local current exception
 *
 * @return Current exception or NULL
 */
nimcp_exception_t* nimcp_exception_get_current(void);

/**
 * @brief Clear thread-local current exception
 */
void nimcp_exception_clear_current(void);

/* ============================================================================
 * Exception System Initialization
 * ============================================================================ */

/**
 * @brief Initialize exception system
 *
 * @return 0 on success, -1 on error
 */
int nimcp_exception_system_init(void);

/**
 * @brief Shutdown exception system
 */
void nimcp_exception_system_shutdown(void);

/**
 * @brief Reset exception rate limiter (thread-local)
 *
 * Resets the per-thread exception rate limiting counters.
 * Useful in test environments where many exceptions are expected
 * across multiple test cases within the same second.
 */
void nimcp_exception_reset_rate_limit(void);

/**
 * @brief Check if exception system is initialized
 *
 * @return true if initialized
 */
bool nimcp_exception_system_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EXCEPTION_H */
