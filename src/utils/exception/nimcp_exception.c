/**
 * @file nimcp_exception.c
 * @brief Exception handling system implementation
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: Core exception creation, lifecycle, and stack trace capture
 * WHY:  Provide rich exception information for debugging and immune integration
 * HOW:  Allocate exception structs, capture stack traces, manage ref counting
 *
 * @author NIMCP Development Team
 */

#include "utils/exception/nimcp_exception.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/signal/nimcp_signal_handler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#ifdef __linux__
#include <execinfo.h>
#endif

/* ============================================================================
 * Module State
 * ============================================================================ */

/* P2-U24: Use volatile + __atomic builtins for thread-safe visibility.
 * Multiple threads may call nimcp_exception_system_is_initialized() concurrently. */
static volatile bool g_exception_system_initialized = false;
static nimcp_platform_mutex_t* g_exception_mutex = NULL;

/* Thread-local current exception */
static _Thread_local nimcp_exception_t* tl_current_exception = NULL;

/* Rate limiter for non-severe exceptions to prevent performance degradation.
 * When NIMCP_THROW_TO_IMMUNE was added to ~28,000 error returns, hot paths
 * like neural_network_add_connection and brain_create generate thousands of
 * exceptions during normal operation, each requiring allocation + dispatch. */
static _Thread_local uint64_t tl_exception_window_start = 0;
static _Thread_local uint32_t tl_exception_count_in_window = 0;
#define EXCEPTION_RATE_LIMIT_WINDOW_US  1000000  /* 1 second window */
#define EXCEPTION_RATE_LIMIT_MAX        5000     /* max 5000 non-severe per second */

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Get current timestamp in microseconds
 */
static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Get category from error code
 */
nimcp_exception_category_t nimcp_exception_get_category_from_code(nimcp_error_t code) {
    if (code < 1000) return EXCEPTION_CATEGORY_GENERIC;

    /* GPU errors are in 1100-1199 range */
    if (code >= 1100 && code < 1200) return EXCEPTION_CATEGORY_GPU;

    int category = code / 1000;
    switch (category) {
        case 1:  return EXCEPTION_CATEGORY_GENERIC;
        case 2:  return EXCEPTION_CATEGORY_MEMORY;
        case 3:  return EXCEPTION_CATEGORY_BRAIN;
        case 4:  return EXCEPTION_CATEGORY_IO;
        case 5:  return EXCEPTION_CATEGORY_CONFIG;
        case 6:  return EXCEPTION_CATEGORY_THREADING;
        case 7:  return EXCEPTION_CATEGORY_SIGNAL;
        case 8:  return EXCEPTION_CATEGORY_COGNITIVE;
        case 9:  return EXCEPTION_CATEGORY_SECURITY;
        default:
            if (code >= 10000 && code < 20000) {
                return EXCEPTION_CATEGORY_BRAIN_REGION;
            }
            return EXCEPTION_CATEGORY_GENERIC;
    }
}

/**
 * @brief Get severity from error code
 */
nimcp_exception_severity_t nimcp_exception_get_severity_from_code(nimcp_error_t code) {
    /* Fatal signals */
    if (code >= 7001 && code <= 7005) return EXCEPTION_SEVERITY_FATAL;

    /* Critical errors */
    if (code == NIMCP_ERROR_MEMORY_CORRUPTION ||
        code == NIMCP_ERROR_DEADLOCK ||
        code == NIMCP_ERROR_CRASH_RECOVERY) {
        return EXCEPTION_SEVERITY_CRITICAL;
    }

    /* Severe errors */
    if (code == NIMCP_ERROR_NO_MEMORY ||
        code == NIMCP_ERROR_BUFFER_OVERFLOW ||
        code == NIMCP_ERROR_DOUBLE_FREE ||
        code == NIMCP_ERROR_SIGSEGV) {
        return EXCEPTION_SEVERITY_SEVERE;
    }

    /* General errors */
    int category = code / 1000;
    switch (category) {
        case 0:  return EXCEPTION_SEVERITY_DEBUG;    /* Success codes */
        case 1:  return EXCEPTION_SEVERITY_ERROR;    /* Generic */
        case 2:  return EXCEPTION_SEVERITY_SEVERE;   /* Memory */
        case 3:  return EXCEPTION_SEVERITY_ERROR;    /* Brain */
        case 4:  return EXCEPTION_SEVERITY_ERROR;    /* I/O */
        case 5:  return EXCEPTION_SEVERITY_WARNING;  /* Config */
        case 6:  return EXCEPTION_SEVERITY_SEVERE;   /* Threading */
        case 7:  return EXCEPTION_SEVERITY_CRITICAL; /* Signal */
        case 8:  return EXCEPTION_SEVERITY_ERROR;    /* Cognitive */
        case 9:  return EXCEPTION_SEVERITY_SEVERE;   /* Security */
        default:
            if (code >= 10000 && code < 20000) {
                return EXCEPTION_SEVERITY_ERROR;     /* Brain region */
            }
            return EXCEPTION_SEVERITY_ERROR;
    }
}

/**
 * @brief Get suggested recovery action
 */
nimcp_exception_recovery_action_t nimcp_exception_get_suggested_recovery(nimcp_exception_t* ex) {
    if (!ex) return EXCEPTION_RECOVERY_NONE;

    switch (ex->category) {
        case EXCEPTION_CATEGORY_MEMORY:
            return EXCEPTION_RECOVERY_GC;

        case EXCEPTION_CATEGORY_BRAIN:
        case EXCEPTION_CATEGORY_BRAIN_REGION:
            if (ex->code == NIMCP_ERROR_FORWARD_PASS ||
                ex->code == NIMCP_ERROR_BACKWARD_PASS) {
                return EXCEPTION_RECOVERY_ROLLBACK;
            }
            return EXCEPTION_RECOVERY_REDUCE_LOAD;

        case EXCEPTION_CATEGORY_IO:
            return EXCEPTION_RECOVERY_RETRY;

        case EXCEPTION_CATEGORY_THREADING:
            if (ex->code == NIMCP_ERROR_DEADLOCK) {
                return EXCEPTION_RECOVERY_RESTART_THREAD;
            }
            return EXCEPTION_RECOVERY_RETRY;

        case EXCEPTION_CATEGORY_SIGNAL:
            return EXCEPTION_RECOVERY_EMERGENCY_SAVE;

        case EXCEPTION_CATEGORY_GPU:
            return EXCEPTION_RECOVERY_CLEAR_CACHE;

        case EXCEPTION_CATEGORY_SECURITY:
            return EXCEPTION_RECOVERY_QUARANTINE;

        case EXCEPTION_CATEGORY_COGNITIVE:
            return EXCEPTION_RECOVERY_REDUCE_LOAD;

        default:
            return EXCEPTION_RECOVERY_NONE;
    }
}

/* ============================================================================
 * Stack Trace
 * ============================================================================ */

size_t nimcp_exception_capture_stack_trace(nimcp_stack_trace_t* trace, int skip_frames) {
    if (!trace) return 0;

    memset(trace, 0, sizeof(nimcp_stack_trace_t));

#ifdef __linux__
    void* addresses[NIMCP_EXCEPTION_MAX_STACK_DEPTH + 8];
    int count = backtrace(addresses, NIMCP_EXCEPTION_MAX_STACK_DEPTH + skip_frames + 1);

    /* Skip requested frames plus our own frame */
    int start = skip_frames + 1;
    if (start >= count) return 0;

    char** symbols = backtrace_symbols(addresses + start, count - start);

    for (int i = 0; i < count - start && trace->depth < NIMCP_EXCEPTION_MAX_STACK_DEPTH; i++) {
        trace->frames[trace->depth].address = addresses[start + i];
        /* P2-U23: Copy symbol strings so we can free the backtrace_symbols result.
         * Previously we stored direct pointers into the symbols array, causing a
         * memory leak since the array was never freed. Use strdup to copy. */
        trace->frames[trace->depth].function = (symbols && symbols[i]) ? strdup(symbols[i]) : NULL;
        trace->frames[trace->depth].file = NULL;
        trace->frames[trace->depth].line = 0;
        trace->depth++;
    }

    /* P2-U23: Free the backtrace_symbols result now that strings are copied */
    free(symbols);
#endif

    return trace->depth;
}

/* ============================================================================
 * Epitope Generation
 * ============================================================================ */

/**
 * @brief Simple hash function for epitope generation
 */
static uint32_t hash_string(const char* str, size_t len) {
    uint32_t hash = 5381;
    for (size_t i = 0; i < len && str[i]; i++) {
        hash = ((hash << 5) + hash) + (uint8_t)str[i];
    }
    return hash;
}

size_t nimcp_exception_generate_epitope(nimcp_exception_t* ex) {
    if (!ex) return 0;

    memset(ex->epitope, 0, NIMCP_EXCEPTION_EPITOPE_SIZE);
    size_t offset = 0;

    /* Error code (4 bytes) */
    memcpy(ex->epitope + offset, &ex->code, sizeof(ex->code));
    offset += sizeof(ex->code);

    /* Category (4 bytes) */
    uint32_t cat = (uint32_t)ex->category;
    memcpy(ex->epitope + offset, &cat, sizeof(cat));
    offset += sizeof(cat);

    /* Severity (4 bytes) */
    uint32_t sev = (uint32_t)ex->severity;
    memcpy(ex->epitope + offset, &sev, sizeof(sev));
    offset += sizeof(sev);

    /* File hash (4 bytes) */
    uint32_t file_hash = ex->file ? hash_string(ex->file, 256) : 0;
    memcpy(ex->epitope + offset, &file_hash, sizeof(file_hash));
    offset += sizeof(file_hash);

    /* Function hash (4 bytes) */
    uint32_t func_hash = ex->function ? hash_string(ex->function, 128) : 0;
    memcpy(ex->epitope + offset, &func_hash, sizeof(func_hash));
    offset += sizeof(func_hash);

    /* Line number (4 bytes) */
    uint32_t line = (uint32_t)ex->line;
    memcpy(ex->epitope + offset, &line, sizeof(line));
    offset += sizeof(line);

    /* P3-U9: Stack trace hash - use first few addresses.
     * We deliberately truncate to uint32_t to fit within the fixed-size epitope buffer
     * (NIMCP_EXCEPTION_EPITOPE_SIZE = 64 bytes). On 64-bit systems, the lower 32 bits
     * of addresses still provide sufficient entropy for pattern matching since ASLR
     * randomizes page offsets within the lower address space. Using full uint64_t
     * would halve the number of frames we can include. */
    for (size_t i = 0; i < 8 && i < ex->stack_trace.depth && offset < NIMCP_EXCEPTION_EPITOPE_SIZE - 4; i++) {
        uint32_t addr = (uint32_t)(uintptr_t)ex->stack_trace.frames[i].address;
        memcpy(ex->epitope + offset, &addr, sizeof(addr));
        offset += sizeof(addr);
    }

    ex->epitope_len = offset;
    return offset;
}

/* ============================================================================
 * Exception Creation
 * ============================================================================ */

nimcp_exception_t* nimcp_exception_create(
    nimcp_error_t code,
    nimcp_exception_severity_t severity,
    const char* file,
    int line,
    const char* func,
    const char* format,
    ...
) {
    /* Rate limit non-severe exceptions to prevent performance degradation.
     * Severe/critical/fatal exceptions always pass through. */
    if (severity < EXCEPTION_SEVERITY_SEVERE) {
        uint64_t now = get_timestamp_us();
        if (now - tl_exception_window_start > EXCEPTION_RATE_LIMIT_WINDOW_US) {
            /* New window */
            tl_exception_window_start = now;
            tl_exception_count_in_window = 1;
        } else {
            tl_exception_count_in_window++;
            if (tl_exception_count_in_window > EXCEPTION_RATE_LIMIT_MAX) {
                if (tl_exception_count_in_window == EXCEPTION_RATE_LIMIT_MAX + 1) {
                    /* Log once per window so callers know exceptions are being dropped */
                    LOG_WARN("Exception rate limit reached (%u/s) — suppressing non-severe exceptions",
                             EXCEPTION_RATE_LIMIT_MAX);
                }
                return NULL;  /* Rate limited - NIMCP_THROW_TO_IMMUNE checks for NULL */
            }
        }
    }

    nimcp_exception_t* ex = nimcp_calloc(1, sizeof(nimcp_exception_t));
    if (!ex) return NULL;

    ex->type = EXCEPTION_TYPE_BASE;
    ex->category = nimcp_exception_get_category_from_code(code);
    ex->code = code;
    ex->severity = severity;
    ex->file = file;
    ex->line = line;
    ex->function = func;
    ex->timestamp_us = get_timestamp_us();
    ex->ref_count = 1;

    /* Format message */
    if (format) {
        va_list args;
        va_start(args, format);
        vsnprintf(ex->message, NIMCP_EXCEPTION_MAX_MESSAGE, format, args);
        va_end(args);
    }

    /* Capture stack trace only for severe+ exceptions (backtrace is expensive) */
    if (severity >= EXCEPTION_SEVERITY_SEVERE) {
        nimcp_exception_capture_stack_trace(&ex->stack_trace, 2);
    }

    /* Generate immune epitope */
    nimcp_exception_generate_epitope(ex);

    /* Determine suggested recovery */
    ex->suggested_action = nimcp_exception_get_suggested_recovery(ex);

    return ex;
}

nimcp_memory_exception_t* nimcp_memory_exception_create(
    nimcp_error_t code,
    nimcp_exception_severity_t severity,
    const char* file,
    int line,
    const char* func,
    size_t requested_size,
    const char* format,
    ...
) {
    nimcp_memory_exception_t* ex = nimcp_calloc(1, sizeof(nimcp_memory_exception_t));
    if (!ex) return NULL;

    ex->base.type = EXCEPTION_TYPE_MEMORY;
    ex->base.category = EXCEPTION_CATEGORY_MEMORY;
    ex->base.code = code;
    ex->base.severity = severity;
    ex->base.file = file;
    ex->base.line = line;
    ex->base.function = func;
    ex->base.timestamp_us = get_timestamp_us();
    ex->base.ref_count = 1;
    ex->base.suggested_action = EXCEPTION_RECOVERY_GC;

    ex->requested_size = requested_size;
    ex->is_heap = true;

    /* Format message */
    if (format) {
        va_list args;
        va_start(args, format);
        vsnprintf(ex->base.message, NIMCP_EXCEPTION_MAX_MESSAGE, format, args);
        va_end(args);
    }

    nimcp_exception_capture_stack_trace(&ex->base.stack_trace, 2);
    nimcp_exception_generate_epitope(&ex->base);

    return ex;
}

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
) {
    nimcp_brain_exception_t* ex = nimcp_calloc(1, sizeof(nimcp_brain_exception_t));
    if (!ex) return NULL;

    ex->base.type = EXCEPTION_TYPE_BRAIN;
    ex->base.category = EXCEPTION_CATEGORY_BRAIN;
    ex->base.code = code;
    ex->base.severity = severity;
    ex->base.file = file;
    ex->base.line = line;
    ex->base.function = func;
    ex->base.timestamp_us = get_timestamp_us();
    ex->base.ref_count = 1;
    ex->base.suggested_action = EXCEPTION_RECOVERY_ROLLBACK;

    ex->brain_id = brain_id;
    ex->region_name = region_name;

    if (format) {
        va_list args;
        va_start(args, format);
        vsnprintf(ex->base.message, NIMCP_EXCEPTION_MAX_MESSAGE, format, args);
        va_end(args);
    }

    nimcp_exception_capture_stack_trace(&ex->base.stack_trace, 2);
    nimcp_exception_generate_epitope(&ex->base);

    return ex;
}

nimcp_io_exception_t* nimcp_io_exception_create(
    nimcp_error_t code,
    nimcp_exception_severity_t severity,
    const char* file,
    int line,
    const char* func,
    const char* path,
    const char* format,
    ...
) {
    nimcp_io_exception_t* ex = nimcp_calloc(1, sizeof(nimcp_io_exception_t));
    if (!ex) return NULL;

    ex->base.type = EXCEPTION_TYPE_IO;
    ex->base.category = EXCEPTION_CATEGORY_IO;
    ex->base.code = code;
    ex->base.severity = severity;
    ex->base.file = file;
    ex->base.line = line;
    ex->base.function = func;
    ex->base.timestamp_us = get_timestamp_us();
    ex->base.ref_count = 1;
    ex->base.suggested_action = EXCEPTION_RECOVERY_RETRY;

    ex->path = path;

    if (format) {
        va_list args;
        va_start(args, format);
        vsnprintf(ex->base.message, NIMCP_EXCEPTION_MAX_MESSAGE, format, args);
        va_end(args);
    }

    nimcp_exception_capture_stack_trace(&ex->base.stack_trace, 2);
    nimcp_exception_generate_epitope(&ex->base);

    return ex;
}

nimcp_threading_exception_t* nimcp_threading_exception_create(
    nimcp_error_t code,
    nimcp_exception_severity_t severity,
    const char* file,
    int line,
    const char* func,
    uint64_t thread_id,
    const char* format,
    ...
) {
    nimcp_threading_exception_t* ex = nimcp_calloc(1, sizeof(nimcp_threading_exception_t));
    if (!ex) return NULL;

    ex->base.type = EXCEPTION_TYPE_THREADING;
    ex->base.category = EXCEPTION_CATEGORY_THREADING;
    ex->base.code = code;
    ex->base.severity = severity;
    ex->base.file = file;
    ex->base.line = line;
    ex->base.function = func;
    ex->base.timestamp_us = get_timestamp_us();
    ex->base.ref_count = 1;
    ex->base.suggested_action = EXCEPTION_RECOVERY_RESTART_THREAD;

    ex->thread_id = thread_id;
    ex->is_deadlock = (code == NIMCP_ERROR_DEADLOCK);

    if (format) {
        va_list args;
        va_start(args, format);
        vsnprintf(ex->base.message, NIMCP_EXCEPTION_MAX_MESSAGE, format, args);
        va_end(args);
    }

    nimcp_exception_capture_stack_trace(&ex->base.stack_trace, 2);
    nimcp_exception_generate_epitope(&ex->base);

    return ex;
}

nimcp_security_exception_t* nimcp_security_exception_create(
    nimcp_error_t code,
    nimcp_exception_severity_t severity,
    const char* file,
    int line,
    const char* func,
    uint32_t threat_type,
    const char* format,
    ...
) {
    nimcp_security_exception_t* ex = nimcp_calloc(1, sizeof(nimcp_security_exception_t));
    if (!ex) return NULL;

    ex->base.type = EXCEPTION_TYPE_SECURITY;
    ex->base.category = EXCEPTION_CATEGORY_SECURITY;
    ex->base.code = code;
    ex->base.severity = severity;
    ex->base.file = file;
    ex->base.line = line;
    ex->base.function = func;
    ex->base.timestamp_us = get_timestamp_us();
    ex->base.ref_count = 1;
    ex->base.suggested_action = EXCEPTION_RECOVERY_QUARANTINE;

    ex->threat_type = threat_type;
    ex->quarantine_required = (severity >= EXCEPTION_SEVERITY_SEVERE);

    if (format) {
        va_list args;
        va_start(args, format);
        vsnprintf(ex->base.message, NIMCP_EXCEPTION_MAX_MESSAGE, format, args);
        va_end(args);
    }

    nimcp_exception_capture_stack_trace(&ex->base.stack_trace, 2);
    nimcp_exception_generate_epitope(&ex->base);

    return ex;
}

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
) {
    nimcp_gpu_exception_t* ex = nimcp_calloc(1, sizeof(nimcp_gpu_exception_t));
    if (!ex) return NULL;

    ex->base.type = EXCEPTION_TYPE_GPU;
    ex->base.category = EXCEPTION_CATEGORY_GPU;
    ex->base.code = code;
    ex->base.severity = severity;
    ex->base.file = file;
    ex->base.line = line;
    ex->base.function = func;
    ex->base.timestamp_us = get_timestamp_us();
    ex->base.ref_count = 1;
    ex->base.suggested_action = EXCEPTION_RECOVERY_CLEAR_CACHE;

    ex->device_id = device_id;
    ex->cuda_error = cuda_error;

    if (format) {
        va_list args;
        va_start(args, format);
        vsnprintf(ex->base.message, NIMCP_EXCEPTION_MAX_MESSAGE, format, args);
        va_end(args);
    }

    nimcp_exception_capture_stack_trace(&ex->base.stack_trace, 2);
    nimcp_exception_generate_epitope(&ex->base);

    return ex;
}

/* ============================================================================
 * Signal Exception API
 * ============================================================================ */

#include <signal.h>
#include "utils/signal/nimcp_signal_handler.h"

#include <stddef.h>  /* for NULL */

/**
 * @brief Map signal number to NIMCP error code
 */
static nimcp_error_t signal_to_error_code(int signal_number) {
    switch (signal_number) {
        case SIGSEGV: return NIMCP_ERROR_SIGSEGV;
        case SIGABRT: return NIMCP_ERROR_SIGABRT;
        case SIGFPE:  return NIMCP_ERROR_SIGFPE;
        case SIGBUS:  return NIMCP_ERROR_SIGBUS;
        case SIGILL:  return NIMCP_ERROR_SIGILL;
        default:      return NIMCP_ERROR_SIGNAL_RECEIVED;
    }
}

/**
 * @brief Determine if a signal is fatal (SIGSEGV, SIGABRT, SIGFPE, SIGBUS, SIGILL)
 */
static bool is_fatal_signal(int signal_number) {
    switch (signal_number) {
        case SIGSEGV:
        case SIGABRT:
        case SIGFPE:
        case SIGBUS:
        case SIGILL:
            return true;
        default:
            return false;
    }
}

nimcp_signal_exception_t* nimcp_signal_exception_create(
    int signal_number,
    void* fault_address,
    const char* file,
    int line,
    const char* func,
    const char* format,
    ...
) {
    nimcp_signal_exception_t* ex = nimcp_calloc(1, sizeof(nimcp_signal_exception_t));
    if (!ex) return NULL;

    ex->base.type = EXCEPTION_TYPE_SIGNAL;
    ex->base.category = EXCEPTION_CATEGORY_SIGNAL;
    ex->base.code = signal_to_error_code(signal_number);
    /* Fatal signals get FATAL severity, others get CRITICAL */
    ex->base.severity = is_fatal_signal(signal_number) ? EXCEPTION_SEVERITY_FATAL : EXCEPTION_SEVERITY_CRITICAL;
    ex->base.file = file;
    ex->base.line = line;
    ex->base.function = func;
    ex->base.timestamp_us = get_timestamp_us();
    ex->base.ref_count = 1;
    ex->base.suggested_action = EXCEPTION_RECOVERY_RETRY;

    ex->signal_number = signal_number;
    ex->fault_address = fault_address;
    ex->instruction_pointer = NULL;
    ex->stack_pointer = NULL;
    ex->base_pointer = NULL;
    ex->recovery_attempted = false;
    ex->siglongjmp_executed = false;
    ex->retry_count = 0;

    if (format) {
        va_list args;
        va_start(args, format);
        vsnprintf(ex->base.message, NIMCP_EXCEPTION_MAX_MESSAGE, format, args);
        va_end(args);
    } else {
        /* Generate default message when format is NULL */
        snprintf(ex->base.message, NIMCP_EXCEPTION_MAX_MESSAGE,
                 "Signal %d (%s) at %p",
                 signal_number,
                 strsignal(signal_number),
                 fault_address);
    }

    nimcp_exception_capture_stack_trace(&ex->base.stack_trace, 2);
    nimcp_exception_generate_epitope(&ex->base);

    return ex;
}

nimcp_signal_exception_t* nimcp_signal_exception_create_from_context(
    const struct signal_crash_context* ctx
) {
    if (!ctx) return NULL;

    nimcp_signal_exception_t* ex = nimcp_calloc(1, sizeof(nimcp_signal_exception_t));
    if (!ex) return NULL;

    ex->base.type = EXCEPTION_TYPE_SIGNAL;
    ex->base.category = EXCEPTION_CATEGORY_SIGNAL;
    ex->base.code = signal_to_error_code(ctx->signal);
    /* Fatal signals get FATAL severity, others get CRITICAL */
    ex->base.severity = is_fatal_signal(ctx->signal) ? EXCEPTION_SEVERITY_FATAL : EXCEPTION_SEVERITY_CRITICAL;
    ex->base.file = NULL;  /* Not available in crash context */
    ex->base.line = 0;
    ex->base.function = NULL;
    ex->base.timestamp_us = get_timestamp_us();
    ex->base.ref_count = 1;
    ex->base.suggested_action = EXCEPTION_RECOVERY_RETRY;

    ex->signal_number = ctx->signal;
    ex->fault_address = ctx->fault_address;
    ex->instruction_pointer = ctx->instruction_pointer;
    ex->stack_pointer = ctx->stack_pointer;
    ex->base_pointer = ctx->base_pointer;
    ex->recovery_attempted = false;  /* Not tracked in crash context */
    ex->siglongjmp_executed = false;
    ex->retry_count = 0;

    /* Copy memory region info */
    if (ctx->memory_region[0] != '\0') {
        strncpy(ex->memory_region, ctx->memory_region, NIMCP_SIGNAL_EXCEPTION_MEMORY_REGION_SIZE - 1);
        ex->memory_region[NIMCP_SIGNAL_EXCEPTION_MEMORY_REGION_SIZE - 1] = '\0';
    }

    snprintf(ex->base.message, NIMCP_EXCEPTION_MAX_MESSAGE,
             "Signal %d (%s) at %p",
             ctx->signal,
             strsignal(ctx->signal),
             ctx->fault_address);

    /* Use backtrace from crash context if available, otherwise capture current */
    if (ctx->backtrace_depth > 0) {
        /* Copy backtrace from crash context */
        ex->base.stack_trace.depth = 0;
        for (int i = 0; i < ctx->backtrace_depth &&
             ex->base.stack_trace.depth < NIMCP_EXCEPTION_MAX_STACK_DEPTH; i++) {
            ex->base.stack_trace.frames[ex->base.stack_trace.depth].address = ctx->backtrace[i];
            ex->base.stack_trace.frames[ex->base.stack_trace.depth].function = NULL;
            ex->base.stack_trace.frames[ex->base.stack_trace.depth].file = NULL;
            ex->base.stack_trace.frames[ex->base.stack_trace.depth].line = 0;
            ex->base.stack_trace.depth++;
        }
    } else {
        /* No backtrace in context, capture current stack */
        nimcp_exception_capture_stack_trace(&ex->base.stack_trace, 2);
    }
    nimcp_exception_generate_epitope(&ex->base);

    return ex;
}
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(exception)

/* ============================================================================
 * Context API
 * ============================================================================ */

int nimcp_exception_set_context(nimcp_exception_t* ex, const char* key, const char* value) {
    if (!ex || !key || !value) return -1;

    /* Check if key already exists */
    for (size_t i = 0; i < ex->context_count; i++) {
        if (strncmp(ex->context[i].key, key, NIMCP_EXCEPTION_MAX_CONTEXT_KEY) == 0) {
            strncpy(ex->context[i].value, value, NIMCP_EXCEPTION_MAX_CONTEXT_VALUE - 1);
            ex->context[i].value[NIMCP_EXCEPTION_MAX_CONTEXT_VALUE - 1] = '\0';
            return 0;
        }
    }

    /* Add new entry */
    if (ex->context_count >= NIMCP_EXCEPTION_MAX_CONTEXT_ENTRIES) {
        return -1;  /* Context full */
    }

    strncpy(ex->context[ex->context_count].key, key, NIMCP_EXCEPTION_MAX_CONTEXT_KEY - 1);
    ex->context[ex->context_count].key[NIMCP_EXCEPTION_MAX_CONTEXT_KEY - 1] = '\0';
    strncpy(ex->context[ex->context_count].value, value, NIMCP_EXCEPTION_MAX_CONTEXT_VALUE - 1);
    ex->context[ex->context_count].value[NIMCP_EXCEPTION_MAX_CONTEXT_VALUE - 1] = '\0';
    ex->context_count++;

    return 0;
}

const char* nimcp_exception_get_context(const nimcp_exception_t* ex, const char* key) {
    if (!ex || !key) return NULL;

    for (size_t i = 0; i < ex->context_count; i++) {
        if (strncmp(ex->context[i].key, key, NIMCP_EXCEPTION_MAX_CONTEXT_KEY) == 0) {
            return ex->context[i].value;
        }
    }
    return NULL;
}

int nimcp_exception_remove_context(nimcp_exception_t* ex, const char* key) {
    if (!ex || !key) return -1;

    for (size_t i = 0; i < ex->context_count; i++) {
        if (strncmp(ex->context[i].key, key, NIMCP_EXCEPTION_MAX_CONTEXT_KEY) == 0) {
            /* Shift remaining entries */
            for (size_t j = i; j < ex->context_count - 1; j++) {
                ex->context[j] = ex->context[j + 1];
            }
            ex->context_count--;
            return 0;
        }
    }
    return -1;
}

size_t nimcp_exception_context_count(const nimcp_exception_t* ex) {
    if (!ex) return 0;
    return ex->context_count;
}

/* ============================================================================
 * Exception Lifecycle (ref counting, cause chain)
 * ============================================================================ */

nimcp_exception_t* nimcp_exception_ref(nimcp_exception_t* ex) {
    if (!ex) return NULL;
    __atomic_add_fetch(&ex->ref_count, 1, __ATOMIC_SEQ_CST);
    return ex;
}

void nimcp_exception_unref(nimcp_exception_t* ex) {
    if (!ex) return;
    int32_t new_count = __atomic_sub_fetch(&ex->ref_count, 1, __ATOMIC_SEQ_CST);
    if (new_count < 0) {
        // Already freed by another thread or over-released — do not free again.
        // This prevents double-free when two threads race on the last unref.
        return;
    }
    if (new_count == 0) {
        /* Free cause chain */
        if (ex->cause) {
            nimcp_exception_unref(ex->cause);
            ex->cause = NULL;
        }
        /* Unref aggregate exception children */
        if (ex->type == EXCEPTION_TYPE_AGGREGATE) {
            nimcp_aggregate_exception_t* agg = (nimcp_aggregate_exception_t*)ex;
            for (size_t i = 0; i < agg->child_count; i++) {
                if (agg->children[i]) {
                    nimcp_exception_unref(agg->children[i]);
                    agg->children[i] = NULL;
                }
            }
            agg->child_count = 0;
        }
        /* Free strdup'd stack trace function name strings.
         * These were allocated by strdup() in nimcp_exception_capture_stack_trace()
         * and must be freed with free() (not nimcp_free) since strdup uses malloc. */
        for (size_t i = 0; i < ex->stack_trace.depth; i++) {
            if (ex->stack_trace.frames[i].function) {
                free((void*)ex->stack_trace.frames[i].function);
                ex->stack_trace.frames[i].function = NULL;
            }
        }
        nimcp_free(ex);
    }
}

void nimcp_exception_set_cause(nimcp_exception_t* ex, nimcp_exception_t* cause) {
    if (!ex) return;
    if (ex->cause) {
        nimcp_exception_unref(ex->cause);
    }
    ex->cause = cause;  /* Takes ownership of reference */
}

nimcp_exception_t* nimcp_exception_get_cause(nimcp_exception_t* ex) {
    if (!ex) return NULL;
    return ex->cause;
}

/* ============================================================================
 * String Conversion
 * ============================================================================ */

const char* nimcp_exception_severity_to_string(nimcp_exception_severity_t severity) {
    switch (severity) {
        case EXCEPTION_SEVERITY_DEBUG:    return "DEBUG";
        case EXCEPTION_SEVERITY_INFO:     return "INFO";
        case EXCEPTION_SEVERITY_WARNING:  return "WARNING";
        case EXCEPTION_SEVERITY_ERROR:    return "ERROR";
        case EXCEPTION_SEVERITY_SEVERE:   return "SEVERE";
        case EXCEPTION_SEVERITY_CRITICAL: return "CRITICAL";
        case EXCEPTION_SEVERITY_FATAL:    return "FATAL";
        default:                          return "UNKNOWN";
    }
}

const char* nimcp_exception_category_to_string(nimcp_exception_category_t category) {
    switch (category) {
        case EXCEPTION_CATEGORY_GENERIC:      return "GENERIC";
        case EXCEPTION_CATEGORY_MEMORY:       return "MEMORY";
        case EXCEPTION_CATEGORY_BRAIN:        return "BRAIN";
        case EXCEPTION_CATEGORY_IO:           return "IO";
        case EXCEPTION_CATEGORY_CONFIG:       return "CONFIG";
        case EXCEPTION_CATEGORY_THREADING:    return "THREADING";
        case EXCEPTION_CATEGORY_SIGNAL:       return "SIGNAL";
        case EXCEPTION_CATEGORY_COGNITIVE:    return "COGNITIVE";
        case EXCEPTION_CATEGORY_GPU:          return "GPU";
        case EXCEPTION_CATEGORY_BRAIN_REGION: return "BRAIN_REGION";
        case EXCEPTION_CATEGORY_SECURITY:     return "SECURITY";
        default:                              return "UNKNOWN";
    }
}

const char* nimcp_exception_type_to_string(nimcp_exception_type_t type) {
    switch (type) {
        case EXCEPTION_TYPE_BASE:      return "BASE";
        case EXCEPTION_TYPE_MEMORY:    return "MEMORY";
        case EXCEPTION_TYPE_BRAIN:     return "BRAIN";
        case EXCEPTION_TYPE_IO:        return "IO";
        case EXCEPTION_TYPE_THREADING: return "THREADING";
        case EXCEPTION_TYPE_SECURITY:  return "SECURITY";
        case EXCEPTION_TYPE_COGNITIVE: return "COGNITIVE";
        case EXCEPTION_TYPE_GPU:       return "GPU";
        case EXCEPTION_TYPE_AGGREGATE: return "AGGREGATE";
        case EXCEPTION_TYPE_SIGNAL:    return "SIGNAL";
        default:                       return "UNKNOWN";
    }
}

const char* nimcp_exception_recovery_action_to_string(nimcp_exception_recovery_action_t action) {
    switch (action) {
        case EXCEPTION_RECOVERY_NONE:              return "NONE";
        case EXCEPTION_RECOVERY_RETRY:             return "RETRY";
        case EXCEPTION_RECOVERY_GC:                return "GC";
        case EXCEPTION_RECOVERY_COMPACT:           return "COMPACT";
        case EXCEPTION_RECOVERY_ROLLBACK:          return "ROLLBACK";
        case EXCEPTION_RECOVERY_RESTART_THREAD:    return "RESTART_THREAD";
        case EXCEPTION_RECOVERY_RESTART_COMPONENT: return "RESTART_COMPONENT";
        case EXCEPTION_RECOVERY_QUARANTINE:        return "QUARANTINE";
        case EXCEPTION_RECOVERY_REDUCE_LOAD:       return "REDUCE_LOAD";
        case EXCEPTION_RECOVERY_CLEAR_CACHE:       return "CLEAR_CACHE";
        case EXCEPTION_RECOVERY_EMERGENCY_SAVE:    return "EMERGENCY_SAVE";
        case EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN: return "GRACEFUL_SHUTDOWN";
        default:                                   return "UNKNOWN";
    }
}

/* ============================================================================
 * Exception Logging/Printing
 * ============================================================================ */

size_t nimcp_stack_trace_to_string(
    const nimcp_stack_trace_t* trace,
    char* buffer,
    size_t buffer_size
) {
    if (!trace || !buffer || buffer_size == 0) return 0;

    size_t offset = 0;
    for (size_t i = 0; i < trace->depth && offset < buffer_size - 1; i++) {
        int written = snprintf(buffer + offset, buffer_size - offset,
            "  #%zu %p %s\n",
            i,
            trace->frames[i].address,
            trace->frames[i].function ? trace->frames[i].function : "(unknown)");
        if (written > 0) {
            /* Cap at available space to prevent offset exceeding buffer_size */
            size_t available = buffer_size - 1 - offset;
            offset += ((size_t)written < available) ? (size_t)written : available;
        }
    }
    return offset;
}

size_t nimcp_exception_to_string(
    const nimcp_exception_t* ex,
    char* buffer,
    size_t buffer_size
) {
    if (!ex || !buffer || buffer_size == 0) return 0;

    size_t offset = 0;
    int written = snprintf(buffer + offset, buffer_size - offset,
        "[%s/%s] Error %d (%s): %s\n  at %s:%d in %s()\n",
        nimcp_exception_severity_to_string(ex->severity),
        nimcp_exception_category_to_string(ex->category),
        ex->code,
        nimcp_exception_type_to_string(ex->type),
        ex->message,
        ex->file ? ex->file : "(unknown)",
        ex->line,
        ex->function ? ex->function : "(unknown)");

    if (written > 0) {
        /* Cap at available space to prevent offset exceeding buffer_size */
        size_t available = buffer_size - 1 - offset;
        offset += ((size_t)written < available) ? (size_t)written : available;
    }

    /* Stack trace */
    if (ex->stack_trace.depth > 0 && offset < buffer_size - 1) {
        written = snprintf(buffer + offset, buffer_size - offset, "Stack trace:\n");
        if (written > 0) {
            size_t available = buffer_size - 1 - offset;
            offset += ((size_t)written < available) ? (size_t)written : available;
        }
        offset += nimcp_stack_trace_to_string(&ex->stack_trace, buffer + offset, buffer_size - offset);
    }

    return offset;
}

void nimcp_exception_log(const nimcp_exception_t* ex) {
    if (!ex) return;

    char buf[NIMCP_DEBUG_BUFFER_SIZE];
    nimcp_exception_to_string(ex, buf, sizeof(buf));
    LOG_ERROR("Exception: %s", buf);
}

void nimcp_exception_print(const nimcp_exception_t* ex) {
    if (!ex) return;

    char buf[NIMCP_DEBUG_BUFFER_SIZE];
    nimcp_exception_to_string(ex, buf, sizeof(buf));
    fprintf(stderr, "%s", buf);
}

/* ============================================================================
 * Signal Handling Functions
 * ============================================================================ */

nimcp_error_t nimcp_signal_to_error_code(int signal_number) {
    switch (signal_number) {
#ifdef SIGSEGV
        case SIGSEGV: return NIMCP_ERROR_SIGSEGV;
#endif
#ifdef SIGABRT
        case SIGABRT: return NIMCP_ERROR_SIGABRT;
#endif
#ifdef SIGFPE
        case SIGFPE:  return NIMCP_ERROR_SIGFPE;
#endif
#ifdef SIGBUS
        case SIGBUS:  return NIMCP_ERROR_SIGBUS;
#endif
#ifdef SIGILL
        case SIGILL:  return NIMCP_ERROR_SIGILL;
#endif
        default:      return NIMCP_ERROR_SIGNAL_RECEIVED;
    }
}

const char* nimcp_signal_name(int signal_number) {
    switch (signal_number) {
#ifdef SIGSEGV
        case SIGSEGV: return "SIGSEGV";
#endif
#ifdef SIGABRT
        case SIGABRT: return "SIGABRT";
#endif
#ifdef SIGFPE
        case SIGFPE:  return "SIGFPE";
#endif
#ifdef SIGBUS
        case SIGBUS:  return "SIGBUS";
#endif
#ifdef SIGILL
        case SIGILL:  return "SIGILL";
#endif
#ifdef SIGTERM
        case SIGTERM: return "SIGTERM";
#endif
#ifdef SIGINT
        case SIGINT:  return "SIGINT";
#endif
#ifdef SIGHUP
        case SIGHUP:  return "SIGHUP";
#endif
        default:      return "UNKNOWN";
    }
}

/* ============================================================================
 * Aggregate Exception Functions
 * ============================================================================ */

nimcp_aggregate_exception_t* nimcp_aggregate_exception_create(
    nimcp_error_t code,
    nimcp_exception_severity_t severity,
    const char* file,
    int line,
    const char* func,
    const char* format,
    ...
) {
    nimcp_aggregate_exception_t* agg = nimcp_calloc(1, sizeof(nimcp_aggregate_exception_t));
    if (!agg) return NULL;

    agg->base.type = EXCEPTION_TYPE_AGGREGATE;
    agg->base.category = nimcp_exception_get_category_from_code(code);
    agg->base.code = code;
    agg->base.severity = severity;
    agg->base.file = file;
    agg->base.line = line;
    agg->base.function = func;
    agg->base.timestamp_us = get_timestamp_us();
    agg->base.ref_count = 1;
    agg->child_count = 0;

    /* Initialize children array */
    for (size_t i = 0; i < NIMCP_EXCEPTION_MAX_CHILDREN; i++) {
        agg->children[i] = NULL;
    }

    /* Format message */
    if (format) {
        va_list args;
        va_start(args, format);
        vsnprintf(agg->base.message, NIMCP_EXCEPTION_MAX_MESSAGE, format, args);
        va_end(args);
    }

    /* Capture stack trace (skip 2 frames: this function + caller) */
    nimcp_exception_capture_stack_trace(&agg->base.stack_trace, 2);

    /* Generate immune epitope */
    nimcp_exception_generate_epitope(&agg->base);

    /* Determine suggested recovery */
    agg->base.suggested_action = nimcp_exception_get_suggested_recovery(&agg->base);

    return agg;
}

int nimcp_aggregate_exception_add(nimcp_aggregate_exception_t* agg, nimcp_exception_t* child) {
    if (!agg || !child) return -1;
    if (agg->child_count >= NIMCP_EXCEPTION_MAX_CHILDREN) return -1;

    /* Take ownership of the child reference */
    nimcp_exception_ref(child);
    agg->children[agg->child_count++] = child;

    /* Update severity if child is more severe */
    if (child->severity > agg->base.severity) {
        agg->base.severity = child->severity;
    }

    return 0;
}

size_t nimcp_aggregate_exception_count(const nimcp_aggregate_exception_t* agg) {
    if (!agg) return 0;
    return agg->child_count;
}

nimcp_exception_t* nimcp_aggregate_exception_get(const nimcp_aggregate_exception_t* agg, size_t index) {
    if (!agg || index >= agg->child_count) return NULL;
    return agg->children[index];
}

/* ============================================================================
 * Thread-Local Exception Context
 * ============================================================================ */

void nimcp_exception_set_current(nimcp_exception_t* ex) {
    if (tl_current_exception) {
        nimcp_exception_unref(tl_current_exception);
    }
    tl_current_exception = ex;
    if (ex) {
        nimcp_exception_ref(ex);
    }
}

nimcp_exception_t* nimcp_exception_get_current(void) {
    return tl_current_exception;
}

void nimcp_exception_clear_current(void) {
    if (tl_current_exception) {
        nimcp_exception_unref(tl_current_exception);
        tl_current_exception = NULL;
    }
}

/* ============================================================================
 * Exception System Initialization
 * ============================================================================ */

void nimcp_exception_reset_rate_limit(void) {
    tl_exception_window_start = 0;
    tl_exception_count_in_window = 0;
}

int nimcp_exception_system_init(void) {
    if (__atomic_load_n(&g_exception_system_initialized, __ATOMIC_ACQUIRE)) {
        /* Reset rate limiter even if already initialized, so tests
         * that call init/shutdown per test case get fresh limits */
        nimcp_exception_reset_rate_limit();
        return 0;  /* Already initialized */
    }

    /* Reset rate limiter on fresh init */
    nimcp_exception_reset_rate_limit();

    g_exception_mutex = nimcp_calloc(1, sizeof(nimcp_platform_mutex_t));
    if (!g_exception_mutex) {
        return -1;
    }

    if (nimcp_platform_mutex_init(g_exception_mutex, false) != 0) {
        nimcp_free(g_exception_mutex);
        g_exception_mutex = NULL;
        return -1;
    }

    __atomic_store_n(&g_exception_system_initialized, true, __ATOMIC_RELEASE);
    return 0;
}

void nimcp_exception_system_shutdown(void) {
    if (!__atomic_load_n(&g_exception_system_initialized, __ATOMIC_ACQUIRE)) {
        return;
    }

    /* Clear thread-local exception */
    if (tl_current_exception) {
        nimcp_exception_unref(tl_current_exception);
        tl_current_exception = NULL;
    }

    if (g_exception_mutex) {
        nimcp_platform_mutex_destroy(g_exception_mutex);
        nimcp_free(g_exception_mutex);
        g_exception_mutex = NULL;
    }

    /* Reset rate limiter so re-initialization works correctly */
    nimcp_exception_reset_rate_limit();

    __atomic_store_n(&g_exception_system_initialized, false, __ATOMIC_RELEASE);
}

bool nimcp_exception_system_is_initialized(void) {
    /* P2-U24: Atomic load for cross-thread visibility */
    return __atomic_load_n(&g_exception_system_initialized, __ATOMIC_ACQUIRE);
}
