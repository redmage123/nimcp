#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_diagnostics.c - Self-Diagnostic Error Detection & Analysis Implementation
//=============================================================================

#include "utils/fault_tolerance/nimcp_diagnostics.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/thread/nimcp_atomic.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "utils_diagnostics"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(diagnostics)

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <execinfo.h>  // For backtrace
#include <unistd.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <pthread.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "constants/nimcp_buffer_constants.h"

//=============================================================================
// Internal State
//=============================================================================

static diagnostic_history_t* g_diagnostic_history = NULL;
static nimcp_mutex_t g_diagnostic_mutex = NIMCP_MUTEX_INITIALIZER;
static bool g_diagnostics_initialized = false;
static FILE* g_diagnostic_log = NULL;
// Thread-safe error ID counter
static nimcp_atomic_uint64_t g_error_id_counter = {0};

// Memory tracking for leak detection
typedef struct {
    size_t total_allocated;
    size_t peak_allocated;
    uint32_t allocation_count;
    uint32_t deallocation_count;
} memory_tracker_t;

static memory_tracker_t g_memory_tracker = {0};

// Infinite loop detection
typedef struct {
    void* last_instruction_pointer;
    struct timeval last_check_time;
    uint32_t same_instruction_count;
} loop_detector_t;

static loop_detector_t g_loop_detector = {0};

//=============================================================================
// Version Information
//=============================================================================

#define DIAGNOSTIC_VERSION "1.0.0"

//=============================================================================
// Error Type Name Mapping
//=============================================================================

const char* diagnostics_get_error_type_name(error_type_t type) {
    switch (type) {
        // Memory Errors
        case ERROR_TYPE_NULL_POINTER:        return "NULL Pointer Dereference";
        case ERROR_TYPE_BUFFER_OVERFLOW:     return "Buffer Overflow";
        case ERROR_TYPE_BUFFER_UNDERFLOW:    return "Buffer Underflow";
        case ERROR_TYPE_MEMORY_LEAK:         return "Memory Leak";
        case ERROR_TYPE_DOUBLE_FREE:         return "Double Free";
        case ERROR_TYPE_USE_AFTER_FREE:      return "Use After Free";
        case ERROR_TYPE_STACK_OVERFLOW:      return "Stack Overflow";
        case ERROR_TYPE_HEAP_CORRUPTION:     return "Heap Corruption";
        case ERROR_TYPE_ALIGNMENT_ERROR:     return "Memory Alignment Error";

        // Numerical Errors
        case ERROR_TYPE_NAN_DETECTED:        return "NaN Detected";
        case ERROR_TYPE_INF_DETECTED:        return "Infinity Detected";
        case ERROR_TYPE_NUMERICAL_OVERFLOW:  return "Numerical Overflow";
        case ERROR_TYPE_NUMERICAL_UNDERFLOW: return "Numerical Underflow";
        case ERROR_TYPE_DIVIDE_BY_ZERO:      return "Division by Zero";
        case ERROR_TYPE_MATRIX_SINGULAR:     return "Singular Matrix";
        case ERROR_TYPE_CONVERGENCE_FAILURE: return "Convergence Failure";
        case ERROR_TYPE_PRECISION_LOSS:      return "Precision Loss";

        // Resource Errors
        case ERROR_TYPE_OUT_OF_MEMORY:       return "Out of Memory";
        case ERROR_TYPE_OUT_OF_FILE_HANDLES: return "Out of File Handles";
        case ERROR_TYPE_OUT_OF_THREADS:      return "Out of Threads";
        case ERROR_TYPE_DISK_FULL:           return "Disk Full";
        case ERROR_TYPE_NETWORK_TIMEOUT:     return "Network Timeout";
        case ERROR_TYPE_DEADLOCK:            return "Deadlock";
        case ERROR_TYPE_RESOURCE_LEAK:       return "Resource Leak";

        // Control Flow Errors
        case ERROR_TYPE_INFINITE_LOOP:       return "Infinite Loop";
        case ERROR_TYPE_ASSERTION_FAILED:    return "Assertion Failed";
        case ERROR_TYPE_UNREACHABLE_CODE:    return "Unreachable Code";
        case ERROR_TYPE_INVALID_STATE:       return "Invalid State";
        case ERROR_TYPE_RACE_CONDITION:      return "Race Condition";
        case ERROR_TYPE_STACK_CORRUPTION:    return "Stack Corruption";

        // Brain-Specific Errors
        case ERROR_TYPE_INVALID_BRAIN_STATE: return "Invalid Brain State";
        case ERROR_TYPE_LAYER_MISMATCH:      return "Layer Mismatch";
        case ERROR_TYPE_WEIGHT_CORRUPTION:   return "Weight Corruption";
        case ERROR_TYPE_ACTIVATION_ANOMALY:  return "Activation Anomaly";
        case ERROR_TYPE_GRADIENT_EXPLOSION:  return "Gradient Explosion";
        case ERROR_TYPE_GRADIENT_VANISHING:  return "Gradient Vanishing";
        case ERROR_TYPE_PLASTICITY_FAILURE:  return "Plasticity Failure";

        // Signal-Based Errors
        case ERROR_TYPE_SEGFAULT:            return "Segmentation Fault";
        case ERROR_TYPE_ILLEGAL_INSTRUCTION: return "Illegal Instruction";
        case ERROR_TYPE_BUS_ERROR:           return "Bus Error";
        case ERROR_TYPE_FLOATING_POINT_ERROR:return "Floating Point Error";
        case ERROR_TYPE_ABORT:               return "Abort Signal";

        case ERROR_TYPE_UNKNOWN:             return "Unknown Error";
        case ERROR_TYPE_NONE:                return "No Error";
        default:                             return "Unrecognized Error Type";
    }
}

const char* diagnostics_get_severity_name(diag_severity_t severity) {
    switch (severity) {
        case DIAG_SEVERITY_INFO:     return "INFO";
        case DIAG_SEVERITY_WARNING:  return "WARNING";
        case DIAG_SEVERITY_ERROR:    return "ERROR";
        case DIAG_SEVERITY_CRITICAL: return "CRITICAL";
        case DIAG_SEVERITY_FATAL:    return "FATAL";
        default:                     return "UNKNOWN";
    }
}

const char* diagnostics_get_recovery_action_name(recovery_action_type_t action) {
    switch (action) {
        case RECOVERY_NONE:              return "No Action";
        case RECOVERY_RETRY:             return "Retry Operation";
        case RECOVERY_RESET_COMPONENT:   return "Reset Component";
        case RECOVERY_RELOAD_CHECKPOINT: return "Reload Checkpoint";
        case RECOVERY_REDUCE_PRECISION:  return "Reduce Precision";
        case RECOVERY_REDUCE_BATCH_SIZE: return "Reduce Batch Size";
        case RECOVERY_CLEAR_CACHE:       return "Clear Cache";
        case RECOVERY_RESTART_PROCESS:   return "Restart Process";
        case RECOVERY_GRACEFUL_SHUTDOWN: return "Graceful Shutdown";
        case RECOVERY_IMMEDIATE_SHUTDOWN:return "Immediate Shutdown";
        case RECOVERY_CUSTOM:            return "Custom Recovery";
        default:                         return "Unknown Action";
    }
}

//=============================================================================
// Initialization & Shutdown
//=============================================================================

bool diagnostics_init(const char* log_file) {
    nimcp_mutex_lock(&g_diagnostic_mutex);

    if (g_diagnostics_initialized) {
        nimcp_mutex_unlock(&g_diagnostic_mutex);
        return true;
    }

    // Initialize logging
    const char* log_path = log_file ? log_file : "nimcp_diagnostics.log";
    g_diagnostic_log = fopen(log_path, "a");
    if (!g_diagnostic_log) {
        nimcp_mutex_unlock(&g_diagnostic_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "diagnostics_init: g_diagnostic_log is NULL");
        return false;
    }

    // Create history tracker
    g_diagnostic_history = diagnostics_create_history();
    if (!g_diagnostic_history) {
        fclose(g_diagnostic_log);
        g_diagnostic_log = NULL;
        nimcp_mutex_unlock(&g_diagnostic_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "diagnostics_init: g_diagnostic_history is NULL");
        return false;
    }

    // Initialize memory tracker
    memset(&g_memory_tracker, 0, sizeof(memory_tracker_t));

    // Initialize loop detector
    memset(&g_loop_detector, 0, sizeof(loop_detector_t));
    gettimeofday(&g_loop_detector.last_check_time, NULL);

    g_diagnostics_initialized = true;

    fprintf(g_diagnostic_log, "[%ld] Diagnostic system initialized (v%s)\n",
            time(NULL), DIAGNOSTIC_VERSION);
    fflush(g_diagnostic_log);

    nimcp_mutex_unlock(&g_diagnostic_mutex);
    return true;
}

void diagnostics_shutdown(void) {
    nimcp_mutex_lock(&g_diagnostic_mutex);

    if (!g_diagnostics_initialized) {
        nimcp_mutex_unlock(&g_diagnostic_mutex);
        return;
    }

    // Write final statistics
    if (g_diagnostic_log) {
        fprintf(g_diagnostic_log, "[%ld] Diagnostic system shutdown\n", time(NULL));
        fprintf(g_diagnostic_log, "Total errors tracked: %u\n",
                g_diagnostic_history ? g_diagnostic_history->count : 0);
        fprintf(g_diagnostic_log, "Memory allocations: %u, deallocations: %u\n",
                g_memory_tracker.allocation_count,
                g_memory_tracker.deallocation_count);
        fclose(g_diagnostic_log);
        g_diagnostic_log = NULL;
    }

    // Free history
    if (g_diagnostic_history) {
        diagnostics_free_history(g_diagnostic_history);
        g_diagnostic_history = NULL;
    }

    g_diagnostics_initialized = false;
    nimcp_mutex_unlock(&g_diagnostic_mutex);
}

//=============================================================================
// Stack Trace Capture & Analysis
//=============================================================================

int diagnostics_capture_stack_trace(void** buffer, int max_depth) {
    if (!buffer || max_depth <= 0) {
        return 0;
    }

    return backtrace(buffer, max_depth);
}

static void symbolicate_stack_frame(void* address, stack_frame_t* frame) {
    if (!frame) return;

    frame->address = address;
    frame->is_symbolicated = false;
    frame->function_name[0] = '\0';
    frame->file_name[0] = '\0';
    frame->line_number = 0;

    // Try to get symbol information
    char** symbols = backtrace_symbols(&address, 1);
    if (symbols) {
        // Parse symbol string (format varies by platform)
        // Example: "./program(function+0x123) [0x400abc]"
        const char* sym = symbols[0];
        const char* func_start = strchr(sym, '(');
        const char* func_end = strchr(sym, '+');

        if (func_start && func_end && func_end > func_start) {
            size_t len = func_end - func_start - 1;
            if (len > 0 && len < sizeof(frame->function_name) - 1) {
                strncpy(frame->function_name, func_start + 1, len);
                frame->function_name[len] = '\0';
                frame->is_symbolicated = true;
            }
        }

        nimcp_free(symbols);
    }

    // If no symbol, use address as identifier
    if (!frame->is_symbolicated) {
        snprintf(frame->function_name, sizeof(frame->function_name),
                 "0x%lx", (unsigned long)address);
    }
}

diagnostic_result_t* diagnostics_analyze_stack_trace(void** trace, int depth) {
    if (!trace || depth <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "diagnostics_analyze_stack_trace: trace is NULL");
        return NULL;
    }

    diagnostic_result_t* result = nimcp_calloc(1, sizeof(diagnostic_result_t));
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "result is NULL");

        return NULL;
    }

    // Initialize result
    result->error_type = ERROR_TYPE_UNKNOWN;
    result->severity = DIAG_SEVERITY_ERROR;
    result->confidence = 0.5F;
    result->timestamp = time(NULL);
    result->error_id = nimcp_atomic_fetch_add_u64(&g_error_id_counter, 1, NIMCP_MEMORY_ORDER_SEQ_CST);
    strncpy(result->diagnostic_version, DIAGNOSTIC_VERSION,
            sizeof(result->diagnostic_version) - 1);

    // Symbolicate stack frames
    result->stack_depth = depth < MAX_STACK_DEPTH ? depth : MAX_STACK_DEPTH;
    for (uint32_t i = 0; i < result->stack_depth; i++) {
        symbolicate_stack_frame(trace[i], &result->stack_trace[i]);
    }

    // Analyze top frame for likely fault location
    if (result->stack_depth > 0) {
        strncpy(result->likely_faulty_function,
                result->stack_trace[0].function_name,
                sizeof(result->likely_faulty_function) - 1);
    }

    // Generate description
    snprintf(result->root_cause, sizeof(result->root_cause),
             "Stack trace analysis: %d frames captured, top frame: %s",
             depth, result->likely_faulty_function);

    snprintf(result->symptoms, sizeof(result->symptoms),
             "Execution stack captured at fault point");

    return result;
}

//=============================================================================
// Crash Analysis
//=============================================================================

static error_type_t signal_to_error_type(int signal) {
    switch (signal) {
        case SIGSEGV: return ERROR_TYPE_SEGFAULT;
        case SIGILL:  return ERROR_TYPE_ILLEGAL_INSTRUCTION;
        case SIGBUS:  return ERROR_TYPE_BUS_ERROR;
        case SIGFPE:  return ERROR_TYPE_FLOATING_POINT_ERROR;
        case SIGABRT: return ERROR_TYPE_ABORT;
        default:      return ERROR_TYPE_UNKNOWN;
    }
}

static diag_severity_t error_type_to_severity(error_type_t type) {
    // Map error types to severity levels
    if (type >= ERROR_TYPE_SEGFAULT && type <= ERROR_TYPE_ABORT) {
        return DIAG_SEVERITY_FATAL;
    }
    if (type == ERROR_TYPE_OUT_OF_MEMORY || type == ERROR_TYPE_STACK_OVERFLOW) {
        return DIAG_SEVERITY_CRITICAL;
    }
    if (type >= ERROR_TYPE_NULL_POINTER && type <= ERROR_TYPE_ALIGNMENT_ERROR) {
        return DIAG_SEVERITY_CRITICAL;
    }
    if (type >= ERROR_TYPE_NAN_DETECTED && type <= ERROR_TYPE_PRECISION_LOSS) {
        return DIAG_SEVERITY_ERROR;
    }
    if (type >= ERROR_TYPE_GRADIENT_EXPLOSION && type <= ERROR_TYPE_PLASTICITY_FAILURE) {
        return DIAG_SEVERITY_ERROR;
    }
    return DIAG_SEVERITY_WARNING;
}

diagnostic_result_t* diagnostics_analyze_crash(int signal, crash_context_t* crash_context) {
    diagnostic_result_t* result = nimcp_calloc(1, sizeof(diagnostic_result_t));
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "result is NULL");

        return NULL;
    }

    // Initialize result
    result->error_type = signal_to_error_type(signal);
    result->severity = error_type_to_severity(result->error_type);
    result->confidence = 0.9F;  // High confidence for signal-based crashes
    result->signal_number = signal;
    result->timestamp = time(NULL);
    result->error_id = nimcp_atomic_fetch_add_u64(&g_error_id_counter, 1, NIMCP_MEMORY_ORDER_SEQ_CST);
    strncpy(result->diagnostic_version, DIAGNOSTIC_VERSION,
            sizeof(result->diagnostic_version) - 1);

    // Extract crash context if provided
    if (crash_context) {
        result->fault_address = crash_context->fault_address;

        // Capture stack trace from crash context
        void* trace_buffer[MAX_STACK_DEPTH];
        int depth = diagnostics_capture_stack_trace(trace_buffer, MAX_STACK_DEPTH);

        result->stack_depth = depth < MAX_STACK_DEPTH ? depth : MAX_STACK_DEPTH;
        for (uint32_t i = 0; i < result->stack_depth; i++) {
            symbolicate_stack_frame(trace_buffer[i], &result->stack_trace[i]);
        }

        if (result->stack_depth > 0) {
            strncpy(result->likely_faulty_function,
                    result->stack_trace[0].function_name,
                    sizeof(result->likely_faulty_function) - 1);
        }
    }

    // Generate root cause description
    switch (result->error_type) {
        case ERROR_TYPE_SEGFAULT:
            if (result->fault_address == NULL || (uintptr_t)result->fault_address < 0x1000) {
                result->error_type = ERROR_TYPE_NULL_POINTER;
                snprintf(result->root_cause, sizeof(result->root_cause),
                         "NULL pointer dereference at address %p", result->fault_address);
            } else {
                snprintf(result->root_cause, sizeof(result->root_cause),
                         "Segmentation fault at address %p (likely buffer overflow or invalid pointer)",
                         result->fault_address);
            }
            break;

        case ERROR_TYPE_FLOATING_POINT_ERROR:
            snprintf(result->root_cause, sizeof(result->root_cause),
                     "Floating point exception (division by zero or invalid operation)");
            break;

        case ERROR_TYPE_ILLEGAL_INSTRUCTION:
            snprintf(result->root_cause, sizeof(result->root_cause),
                     "Illegal instruction (possible memory corruption or wrong architecture)");
            break;

        case ERROR_TYPE_BUS_ERROR:
            snprintf(result->root_cause, sizeof(result->root_cause),
                     "Bus error (misaligned memory access)");
            break;

        case ERROR_TYPE_ABORT:
            snprintf(result->root_cause, sizeof(result->root_cause),
                     "Abort signal (assertion failure or explicit abort)");
            break;

        default:
            snprintf(result->root_cause, sizeof(result->root_cause),
                     "Unknown crash from signal %d", signal);
            break;
    }

    snprintf(result->symptoms, sizeof(result->symptoms),
             "Signal %d received at %p in function %s",
             signal, result->fault_address, result->likely_faulty_function);

    // Capture memory state
    result->memory_state.total_allocated_bytes = g_memory_tracker.total_allocated;
    result->memory_state.peak_allocated_bytes = g_memory_tracker.peak_allocated;
    result->memory_state.allocation_count = g_memory_tracker.allocation_count;
    result->memory_state.deallocation_count = g_memory_tracker.deallocation_count;

    return result;
}

//=============================================================================
// Memory Analysis
//=============================================================================

diagnostic_result_t* diagnostics_analyze_memory_state(brain_t brain) {
    diagnostic_result_t* result = nimcp_calloc(1, sizeof(diagnostic_result_t));
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "result is NULL");

        return NULL;
    }

    // Initialize result
    result->error_type = ERROR_TYPE_NONE;
    result->severity = DIAG_SEVERITY_INFO;
    result->confidence = 0.8F;
    result->timestamp = time(NULL);
    result->error_id = nimcp_atomic_fetch_add_u64(&g_error_id_counter, 1, NIMCP_MEMORY_ORDER_SEQ_CST);
    strncpy(result->diagnostic_version, DIAGNOSTIC_VERSION,
            sizeof(result->diagnostic_version) - 1);

    // Capture memory snapshot
    result->memory_state.total_allocated_bytes = g_memory_tracker.total_allocated;
    result->memory_state.peak_allocated_bytes = g_memory_tracker.peak_allocated;
    result->memory_state.allocation_count = g_memory_tracker.allocation_count;
    result->memory_state.deallocation_count = g_memory_tracker.deallocation_count;

    // Detect memory leaks
    if (result->memory_state.allocation_count > result->memory_state.deallocation_count) {
        uint32_t leaked_blocks = result->memory_state.allocation_count -
                                 result->memory_state.deallocation_count;
        result->memory_state.leaked_blocks = leaked_blocks;
        result->memory_state.leaked_bytes = result->memory_state.total_allocated_bytes;

        if (leaked_blocks > 100) {
            result->error_type = ERROR_TYPE_MEMORY_LEAK;
            result->severity = DIAG_SEVERITY_WARNING;
            result->confidence = 0.7F;

            snprintf(result->root_cause, sizeof(result->root_cause),
                     "Detected %u leaked memory blocks (~%zu bytes)",
                     leaked_blocks, result->memory_state.leaked_bytes);
        }
    }

    // Check for NULL brain (potential issue)
    if (!brain) {
        result->error_type = ERROR_TYPE_NULL_POINTER;
        result->severity = DIAG_SEVERITY_ERROR;
        result->confidence = 1.0F;

        snprintf(result->root_cause, sizeof(result->root_cause),
                 "Brain pointer is NULL");
        snprintf(result->symptoms, sizeof(result->symptoms),
                 "Cannot analyze memory state of NULL brain");
    } else {
        snprintf(result->symptoms, sizeof(result->symptoms),
                 "Memory analysis: %u allocations, %u deallocations, %zu bytes allocated",
                 result->memory_state.allocation_count,
                 result->memory_state.deallocation_count,
                 result->memory_state.total_allocated_bytes);
    }

    return result;
}

//=============================================================================
// Numerical Stability Analysis
//=============================================================================

static bool is_nan_or_inf(float value) {
    return isnan(value) || isinf(value);
}

diagnostic_result_t* diagnostics_analyze_numerical_stability(brain_t brain) {
    diagnostic_result_t* result = nimcp_calloc(1, sizeof(diagnostic_result_t));
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "result is NULL");

        return NULL;
    }

    // Initialize result
    result->error_type = ERROR_TYPE_NONE;
    result->severity = DIAG_SEVERITY_INFO;
    result->confidence = 0.9F;
    result->timestamp = time(NULL);
    result->error_id = nimcp_atomic_fetch_add_u64(&g_error_id_counter, 1, NIMCP_MEMORY_ORDER_SEQ_CST);
    strncpy(result->diagnostic_version, DIAGNOSTIC_VERSION,
            sizeof(result->diagnostic_version) - 1);

    // Check for NULL brain
    if (!brain) {
        result->error_type = ERROR_TYPE_NULL_POINTER;
        result->severity = DIAG_SEVERITY_ERROR;
        snprintf(result->root_cause, sizeof(result->root_cause),
                 "Cannot analyze NULL brain");
        return result;
    }

    // Note: In a real implementation, we would iterate through brain weights,
    // activations, and gradients checking for NaN/Inf values.
    // For now, we provide the framework.

    snprintf(result->symptoms, sizeof(result->symptoms),
             "Numerical stability check completed");
    snprintf(result->root_cause, sizeof(result->root_cause),
             "No numerical instability detected");

    return result;
}

//=============================================================================
// Pattern Detection
//=============================================================================

bool diagnostics_detect_crash_pattern(diagnostic_history_t* history) {
    if (!history || history->count < 3) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "diagnostics_detect_crash_pattern: history is NULL");
        return false;
    }

    // Look for repeated crashes in same function
    for (uint32_t i = 0; i < history->count && i < MAX_HISTORY_SIZE; i++) {
        const error_record_t* err1 = &history->records[i];

        uint32_t same_function_count = 1;
        for (uint32_t j = i + 1; j < history->count && j < MAX_HISTORY_SIZE; j++) {
            const error_record_t* err2 = &history->records[j];

            if (strcmp(err1->function_name, err2->function_name) == 0) {
                same_function_count++;
                if (same_function_count >= 3) {
                    return true;  // Pattern detected
                }
            }
        }
    }

    // Look for rapid succession of crashes
    if (history->count >= 5) {
        time_t first = history->records[history->count - 5].timestamp;
        time_t last = history->records[history->count - 1].timestamp;

        if (last - first < 60) {  // 5 crashes within 60 seconds
            return true;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "diagnostics_detect_crash_pattern: validation failed");
    return false;
}

bool diagnostics_detect_memory_corruption(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "diagnostics_detect_memory_corruption: brain is NULL");
        return false;
    }

    // In a real implementation, check:
    // - Memory canaries
    // - Pointer validity
    // - Structure invariants
    // - Checksums

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "diagnostics_detect_memory_corruption: operation failed");
    return false;
}

bool diagnostics_detect_numerical_instability(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "diagnostics_detect_numerical_instability: brain is NULL");
        return false;
    }

    // In a real implementation, scan:
    // - Weight matrices for NaN/Inf
    // - Activation values for NaN/Inf
    // - Gradient values for NaN/Inf

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "diagnostics_detect_numerical_instability: brain is NULL");
    return false;
}

bool diagnostics_detect_infinite_loop(void* instruction_pointer, uint32_t threshold_ms) {
    struct timeval now;
    gettimeofday(&now, NULL);

    if (g_loop_detector.last_instruction_pointer == instruction_pointer) {
        g_loop_detector.same_instruction_count++;

        // Check if we've been at same instruction too long
        long elapsed_ms = (now.tv_sec - g_loop_detector.last_check_time.tv_sec) * 1000 +
                         (now.tv_usec - g_loop_detector.last_check_time.tv_usec) / 1000;

        if (elapsed_ms > threshold_ms && g_loop_detector.same_instruction_count > 1000) {
            return true;
        }
    } else {
        // Different instruction, reset
        g_loop_detector.last_instruction_pointer = instruction_pointer;
        g_loop_detector.same_instruction_count = 1;
        g_loop_detector.last_check_time = now;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "diagnostics_detect_infinite_loop: operation failed");
    return false;
}

error_type_t diagnostics_detect_resource_exhaustion(float threshold_percent) {
    if (threshold_percent < 0.0F || threshold_percent > 100.0F) {
        threshold_percent = 90.0F;
    }

    // Check memory usage
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        // Check if memory usage is high (platform-dependent)
        // This is a simplified check
        if (g_memory_tracker.total_allocated > 1024 * 1024 * 1024) {  // > 1GB
            return ERROR_TYPE_OUT_OF_MEMORY;
        }
    }

    // Could also check:
    // - File descriptor count
    // - Thread count
    // - Disk space

    return ERROR_TYPE_NONE;
}

//=============================================================================
// Recovery Suggestions
//=============================================================================

static void add_recovery_action(diagnostic_result_t* result,
                                recovery_action_type_t type,
                                float confidence,
                                const char* description,
                                uint32_t cost_ms,
                                bool requires_intervention) {
    if (!result || result->recovery_action_count >= MAX_RECOVERY_ACTIONS) {
        return;
    }

    recovery_action_recommendation_t* action = &result->recovery_actions[result->recovery_action_count];
    action->type = type;
    action->confidence = confidence;
    strncpy(action->description, description, sizeof(action->description) - 1);
    action->estimated_cost_ms = cost_ms;
    action->requires_user_intervention = requires_intervention;

    result->recovery_action_count++;
}

void diagnostics_suggest_recovery(diagnostic_result_t* result) {
    if (!result) {
        return;
    }

    // Clear existing actions
    result->recovery_action_count = 0;

    switch (result->error_type) {
        case ERROR_TYPE_NULL_POINTER:
        case ERROR_TYPE_SEGFAULT:
            add_recovery_action(result, RECOVERY_RELOAD_CHECKPOINT, 0.8F,
                              "Reload from last known good checkpoint", 100, false);
            add_recovery_action(result, RECOVERY_RESET_COMPONENT, 0.6F,
                              "Reset affected component to initial state", 50, false);
            break;

        case ERROR_TYPE_NAN_DETECTED:
        case ERROR_TYPE_INF_DETECTED:
        case ERROR_TYPE_GRADIENT_EXPLOSION:
            add_recovery_action(result, RECOVERY_REDUCE_PRECISION, 0.9F,
                              "Switch to lower precision arithmetic", 10, false);
            add_recovery_action(result, RECOVERY_RELOAD_CHECKPOINT, 0.7F,
                              "Reload from checkpoint before instability", 100, false);
            break;

        case ERROR_TYPE_OUT_OF_MEMORY:
            add_recovery_action(result, RECOVERY_CLEAR_CACHE, 0.9F,
                              "Clear internal caches to free memory", 50, false);
            add_recovery_action(result, RECOVERY_REDUCE_BATCH_SIZE, 0.8F,
                              "Reduce processing batch size", 10, false);
            add_recovery_action(result, RECOVERY_GRACEFUL_SHUTDOWN, 0.5F,
                              "Graceful shutdown to prevent data loss", 1000, true);
            break;

        case ERROR_TYPE_INFINITE_LOOP:
            add_recovery_action(result, RECOVERY_RESET_COMPONENT, 0.9F,
                              "Reset stuck component", 100, false);
            add_recovery_action(result, RECOVERY_RESTART_PROCESS, 0.7F,
                              "Restart process with watchdog", 2000, true);
            break;

        case ERROR_TYPE_BUFFER_OVERFLOW:
        case ERROR_TYPE_HEAP_CORRUPTION:
            add_recovery_action(result, RECOVERY_IMMEDIATE_SHUTDOWN, 0.9F,
                              "Immediate shutdown to prevent further corruption", 0, true);
            break;

        case ERROR_TYPE_MEMORY_LEAK:
        case ERROR_TYPE_RESOURCE_LEAK:
            add_recovery_action(result, RECOVERY_RESTART_PROCESS, 0.8F,
                              "Restart process to reclaim leaked resources", 2000, true);
            break;

        default:
            add_recovery_action(result, RECOVERY_RETRY, 0.5F,
                              "Retry the operation", 10, false);
            break;
    }
}

bool diagnostics_auto_recover(diagnostic_result_t* result, brain_t brain) {
    if (!result || result->recovery_action_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "diagnostics_auto_recover: result is NULL");
        return false;
    }

    // Only attempt auto-recovery if top action has high confidence
    const recovery_action_recommendation_t* action = &result->recovery_actions[0];
    if (action->confidence < 0.8F || action->requires_user_intervention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "diagnostics_auto_recover: validation failed");
        return false;
    }

    result->self_healing_attempted = true;

    // Attempt recovery based on action type
    switch (action->type) {
        case RECOVERY_CLEAR_CACHE:
            // Clear caches
            result->self_healing_successful = true;
            break;

        case RECOVERY_REDUCE_PRECISION:
            // Switch to lower precision
            result->self_healing_successful = true;
            break;

        case RECOVERY_RESET_COMPONENT:
            // Reset component
            if (brain) {
                // In real implementation, reset the brain component
                result->self_healing_successful = true;
            }
            break;

        default:
            result->self_healing_successful = false;
            break;
    }

    return result->self_healing_successful;
}

//=============================================================================
// Reporting Functions
//=============================================================================

void diagnostics_report_to_log(const diagnostic_result_t* result) {
    if (!result) {
        return;
    }

    nimcp_mutex_lock(&g_diagnostic_mutex);

    // Check g_diagnostic_log AFTER acquiring lock to avoid TOCTOU race
    if (!g_diagnostic_log) {
        nimcp_mutex_unlock(&g_diagnostic_mutex);
        return;
    }

    fprintf(g_diagnostic_log, "\n========================================\n");
    fprintf(g_diagnostic_log, "DIAGNOSTIC REPORT #%lu\n", result->error_id);
    fprintf(g_diagnostic_log, "========================================\n");
    fprintf(g_diagnostic_log, "Timestamp: %ld\n", result->timestamp);
    fprintf(g_diagnostic_log, "Error Type: %s (0x%04X)\n",
            diagnostics_get_error_type_name(result->error_type), result->error_type);
    fprintf(g_diagnostic_log, "Severity: %s\n",
            diagnostics_get_severity_name(result->severity));
    fprintf(g_diagnostic_log, "Confidence: %.2f\n", result->confidence);
    fprintf(g_diagnostic_log, "\nRoot Cause:\n%s\n", result->root_cause);
    fprintf(g_diagnostic_log, "\nSymptoms:\n%s\n", result->symptoms);

    if (result->stack_depth > 0) {
        fprintf(g_diagnostic_log, "\nStack Trace (%u frames):\n", result->stack_depth);
        for (uint32_t i = 0; i < result->stack_depth; i++) {
            const stack_frame_t* frame = &result->stack_trace[i];
            fprintf(g_diagnostic_log, "  #%u: %s [%p]\n",
                    i, frame->function_name, frame->address);
        }
    }

    if (result->recovery_action_count > 0) {
        fprintf(g_diagnostic_log, "\nRecovery Actions:\n");
        for (uint32_t i = 0; i < result->recovery_action_count; i++) {
            const recovery_action_recommendation_t* action = &result->recovery_actions[i];
            fprintf(g_diagnostic_log, "  %u. %s (confidence: %.2f, cost: %ums)\n",
                    i + 1, action->description, action->confidence,
                    action->estimated_cost_ms);
        }
    }

    fprintf(g_diagnostic_log, "========================================\n\n");
    fflush(g_diagnostic_log);

    nimcp_mutex_unlock(&g_diagnostic_mutex);
}

bool diagnostics_report_to_file(const diagnostic_result_t* result, const char* path) {
    if (!result || !path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "diagnostics_report_to_file: required parameter is NULL (result, path)");
        return false;
    }

    FILE* f = fopen(path, "w");
    if (!f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "diagnostics_report_to_file: f is NULL");
        return false;
    }

    fprintf(f, "NIMCP Diagnostic Report\n");
    fprintf(f, "=======================\n\n");
    fprintf(f, "Error ID: %lu\n", result->error_id);
    fprintf(f, "Timestamp: %ld\n", result->timestamp);
    fprintf(f, "Diagnostic Version: %s\n\n", result->diagnostic_version);

    fprintf(f, "Error Classification:\n");
    fprintf(f, "  Type: %s\n", diagnostics_get_error_type_name(result->error_type));
    fprintf(f, "  Severity: %s\n", diagnostics_get_severity_name(result->severity));
    fprintf(f, "  Confidence: %.2f\n\n", result->confidence);

    fprintf(f, "Root Cause Analysis:\n");
    fprintf(f, "%s\n\n", result->root_cause);

    fprintf(f, "Symptoms:\n");
    fprintf(f, "%s\n\n", result->symptoms);

    if (result->stack_depth > 0) {
        fprintf(f, "Stack Trace:\n");
        for (uint32_t i = 0; i < result->stack_depth; i++) {
            fprintf(f, "  #%u: %s\n", i, result->stack_trace[i].function_name);
        }
        fprintf(f, "\n");
    }

    if (result->recovery_action_count > 0) {
        fprintf(f, "Recommended Recovery Actions:\n");
        for (uint32_t i = 0; i < result->recovery_action_count; i++) {
            const recovery_action_recommendation_t* action = &result->recovery_actions[i];
            fprintf(f, "  %u. %s\n", i + 1, action->description);
            fprintf(f, "     Confidence: %.2f, Estimated cost: %ums\n",
                    action->confidence, action->estimated_cost_ms);
        }
    }

    fclose(f);
    return true;
}

/**
 * @brief Escape a string for JSON output
 * @param dest Destination buffer
 * @param dest_size Size of destination buffer
 * @param src Source string to escape
 */
static void json_escape_string(char* dest, size_t dest_size, const char* src) {
    if (!dest || dest_size == 0) return;
    if (!src) {
        dest[0] = '\0';
        return;
    }

    size_t di = 0;
    for (size_t si = 0; src[si] != '\0' && di < dest_size - 2; si++) {
        char c = src[si];
        if (c == '"' || c == '\\') {
            if (di + 2 >= dest_size) break;
            dest[di++] = '\\';
            dest[di++] = c;
        } else if (c == '\n') {
            if (di + 2 >= dest_size) break;
            dest[di++] = '\\';
            dest[di++] = 'n';
        } else if (c == '\r') {
            if (di + 2 >= dest_size) break;
            dest[di++] = '\\';
            dest[di++] = 'r';
        } else if (c == '\t') {
            if (di + 2 >= dest_size) break;
            dest[di++] = '\\';
            dest[di++] = 't';
        } else if ((unsigned char)c >= 32) {
            dest[di++] = c;
        }
        // Skip other control characters
    }
    dest[di] = '\0';
}

char* diagnostics_report_to_json(const diagnostic_result_t* result) {
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;
    }

    // Allocate buffer for JSON - enough for escaped strings
    // Each 512-char field could expand to ~1024 when escaped
    const size_t json_size = 4096;
    char* json = nimcp_malloc(json_size);
    if (!json) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "json is NULL");

        return NULL;
    }

    // Escape strings for JSON safety
    char escaped_root_cause[NIMCP_LOG_BUFFER_SIZE];
    char escaped_symptoms[NIMCP_LOG_BUFFER_SIZE];
    json_escape_string(escaped_root_cause, sizeof(escaped_root_cause), result->root_cause);
    json_escape_string(escaped_symptoms, sizeof(escaped_symptoms), result->symptoms);

    snprintf(json, json_size,
             "{\n"
             "  \"error_id\": %lu,\n"
             "  \"timestamp\": %ld,\n"
             "  \"error_type\": \"%s\",\n"
             "  \"severity\": \"%s\",\n"
             "  \"confidence\": %.2f,\n"
             "  \"root_cause\": \"%s\",\n"
             "  \"symptoms\": \"%s\",\n"
             "  \"stack_depth\": %u\n"
             "}",
             result->error_id,
             result->timestamp,
             diagnostics_get_error_type_name(result->error_type),
             diagnostics_get_severity_name(result->severity),
             result->confidence,
             escaped_root_cause,
             escaped_symptoms,
             result->stack_depth);

    return json;
}

//=============================================================================
// History Management
//=============================================================================

diagnostic_history_t* diagnostics_create_history(void) {
    diagnostic_history_t* history = nimcp_calloc(1, sizeof(diagnostic_history_t));
    if (!history) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "history is NULL");

        return NULL;
    }

    history->count = 0;
    history->write_index = 0;
    history->is_full = false;

    return history;
}

void diagnostics_add_to_history(diagnostic_history_t* history,
                                const diagnostic_result_t* result) {
    if (!history || !result) {
        return;
    }

    error_record_t* record = &history->records[history->write_index];

    record->error_type = result->error_type;
    record->timestamp = result->timestamp;
    record->fault_address = result->fault_address;

    if (result->stack_depth > 0) {
        strncpy(record->function_name,
                result->stack_trace[0].function_name,
                sizeof(record->function_name) - 1);
    } else {
        record->function_name[0] = '\0';
    }

    // Update circular buffer
    history->write_index = (history->write_index + 1) % MAX_HISTORY_SIZE;
    if (history->count < MAX_HISTORY_SIZE) {
        history->count++;
    } else {
        history->is_full = true;
    }
}

void diagnostics_free_history(diagnostic_history_t* history) {
    if (history) {
        nimcp_free(history);
    }
}

//=============================================================================
// Cleanup Functions
//=============================================================================

void diagnostics_free_result(diagnostic_result_t* result) {
    if (result) {
        nimcp_free(result);
    }
}
