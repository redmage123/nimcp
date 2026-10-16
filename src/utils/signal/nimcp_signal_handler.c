/**
 * @file nimcp_signal_handler.c
 * @brief Signal handling implementation with graceful recovery and code immune integration
 *
 * WHAT: Comprehensive signal handling with code immune system integration
 * WHY:  Enable self-healing through immune-based crash detection and response
 * HOW:  Capture full crash context (ucontext, /proc/self/maps) and present to
 *       code immune system for potential hot-patching and recovery
 *
 * @author NIMCP Team
 * @date 2025-11-09
 */

#define _GNU_SOURCE  /* Required for REG_RIP, REG_RSP, REG_RBP on Linux */

#include "utils/signal/nimcp_signal_handler.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

/* Alias crash_context_t to the public signal_crash_context_t from header */
typedef signal_crash_context_t crash_context_t;

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "core/brain/persistence/nimcp_brain_persistence.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <execinfo.h>  /* For backtrace() */
#include <time.h>
#include <sys/stat.h>  /* For directory operations */
#include <dirent.h>    /* For directory scanning */
#include <setjmp.h>    /* For siglongjmp/sigsetjmp recovery */
#include <fcntl.h>     /* For open() */
#include <ucontext.h>  /* For ucontext_t */
#include "utils/memory/nimcp_unified_memory.h"

/* Code immune system integration — always included.
 * Signal handler MUST report crashes to the immune system. */
#include "cognitive/immune/nimcp_code_immune.h"

/* Signal exception queue for bridging to exception hierarchy */
#include "utils/signal/nimcp_signal_exception_queue.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stddef.h>  /* for NULL */
//=============================================================================
// Signal-Safe Globals (accessed from signal handlers)
//=============================================================================

// IMPORTANT: These must be volatile sig_atomic_t for signal safety
static volatile sig_atomic_t g_shutdown_requested = 0;
static volatile sig_atomic_t g_reload_requested = 0;
static volatile sig_atomic_t g_last_signal = 0;

// Signal statistics — must be volatile sig_atomic_t for async-signal-safety.
// The C standard guarantees that sig_atomic_t can be read/written atomically
// from a signal handler; volatile uint64_t increment (++) is NOT async-signal-safe.
static volatile sig_atomic_t g_sigsegv_count = 0;
static volatile sig_atomic_t g_sigabrt_count = 0;
static volatile sig_atomic_t g_sigbus_count = 0;
static volatile sig_atomic_t g_sigfpe_count = 0;
static volatile sig_atomic_t g_sigill_count = 0;
static volatile sig_atomic_t g_sigterm_count = 0;
static volatile sig_atomic_t g_sigint_count = 0;
static volatile sig_atomic_t g_sighup_count = 0;
static volatile sig_atomic_t g_recoveries = 0;
static volatile sig_atomic_t g_fatal_crashes = 0;
static volatile sig_atomic_t g_checkpoint_saves = 0;
static volatile sig_atomic_t g_recovery_failures = 0;

// Configuration (set once during initialization)
static signal_handler_config_t g_config;
static bool g_installed = false;

// Recovery configuration
static bool g_auto_recovery_enabled = true;
static int g_max_recovery_attempts = 3;
static int g_checkpoint_retention = 5;
static volatile sig_atomic_t g_recovery_attempt_count = 0;

// Brain instance for crash recovery (signal-safe pointer)
static brain_t g_registered_brain = NULL;

// Previous signal handlers (for restoration)
static struct sigaction g_old_sigsegv;
static struct sigaction g_old_sigabrt;
static struct sigaction g_old_sigbus;
static struct sigaction g_old_sigfpe;
static struct sigaction g_old_sigill;
static struct sigaction g_old_sigterm;
static struct sigaction g_old_sigint;
static struct sigaction g_old_sighup;

//=============================================================================
// Code Immune System Integration
//=============================================================================

/**
 * @brief Global code immune system reference
 *
 * WHAT: Pointer to code immune system for crash handling
 * WHY:  Enable hot-patching and self-healing on crashes
 * HOW:  Set via signal_handler_set_code_immune()
 */
static code_immune_system_t* g_code_immune = NULL;

/**
 * @brief Recovery jump buffer for siglongjmp
 *
 * WHAT: Jump buffer for non-local goto from signal handler
 * WHY:  Allow resumption after code immune handles crash
 * HOW:  Set by sigsetjmp in main code, jumped to from handler
 */
static __thread volatile sig_atomic_t g_recovery_jump_valid = 0;

/**
 * Wrapper struct to apply volatile qualifier to sigjmp_buf.
 * Direct `volatile sigjmp_buf` causes warnings on some compilers because
 * sigjmp_buf is an array type. The wrapper avoids that issue while ensuring
 * the compiler does not optimize away reads/writes from signal handlers.
 */
typedef struct {
    volatile sigjmp_buf buf;
} volatile_jmpbuf_t;
static __thread volatile_jmpbuf_t g_recovery_jump_wrapper;
/* Cast away volatile when passing to sigsetjmp/siglongjmp, which don't
 * accept volatile-qualified arguments. The volatile on the wrapper field
 * ensures compiler-level memory ordering around the jmp_buf storage;
 * the runtime functions operate on the raw bytes regardless. */
#define g_recovery_jump_buf (*(sigjmp_buf*)(void*)&g_recovery_jump_wrapper.buf)

/**
 * @brief Flag indicating crash is being handled by code immune
 *
 * WHAT: Reentrance guard for code immune handling
 * WHY:  Prevent recursive crash handling during recovery
 * HOW:  Set before calling code immune, cleared after
 */
static volatile sig_atomic_t g_in_immune_handling = 0;

/**
 * @brief Pending crash context for deferred processing
 *
 * WHAT: Crash details captured in signal handler
 * WHY:  Signal handler should be minimal; defer processing
 * HOW:  Captured in handler, processed in main thread
 */
static volatile sig_atomic_t g_pending_crash = 0;
static crash_context_t g_pending_crash_context;

//=============================================================================
// Thread-Local Recovery State
//=============================================================================

/**
 * @brief Thread-local key for per-thread recovery context
 *
 * WHAT: pthread_key_t for thread-local storage
 * WHY:  Each thread needs its own recovery point
 * HOW:  Created once, stores signal_recovery_ctx_t per thread
 */
#include <pthread.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(signal_handler)

static pthread_key_t g_recovery_ctx_key;
static pthread_once_t g_recovery_ctx_key_once = PTHREAD_ONCE_INIT;
static volatile sig_atomic_t g_recovery_ctx_key_initialized = 0;

/**
 * @brief Destructor for thread-local recovery context
 */
static void recovery_ctx_destructor(void* ctx)
{
    if (ctx) {
        nimcp_free(ctx);
    }
}

/**
 * @brief Initialize thread-local key (called once)
 */
static void init_recovery_ctx_key(void)
{
    if (pthread_key_create(&g_recovery_ctx_key, recovery_ctx_destructor) == 0) {
        g_recovery_ctx_key_initialized = 1;
    }
}

//=============================================================================
// Signal-Safe Utility Functions
//=============================================================================

/**
 * WHAT: Compute string length (signal-safe version)
 * WHY:  strlen() may not be async-signal-safe on all systems
 * HOW:  Simple manual loop
 */
static size_t safe_strlen(const char* s)
{
    size_t len = 0;
    if (s) {
        while (s[len] != '\0') {
            len++;
        }
    }
    return len;
}

/**
 * WHAT: Write string to stderr (signal-safe)
 * WHY:  printf() is NOT signal-safe
 * HOW:  Use write() syscall with signal-safe strlen
 */
static void safe_write(const char* msg)
{
    if (msg) {
        /* Discard return value: we're in a signal handler with no safe
         * way to recover from a failed write. The `_r; (void)_r` dance
         * satisfies gcc's __warn_unused_result__ on glibc write(). */
        ssize_t _r = write(STDERR_FILENO, msg, safe_strlen(msg));
        (void)_r;
    }
}

/**
 * WHAT: Attempt checkpoint save (signal-safe version)
 * WHY:  Preserve brain state before termination
 * HOW:  Use brain_save() if safe and enabled
 * NOTE: This is NOT fully signal-safe but necessary for recovery
 */
static void attempt_checkpoint_save_unsafe(void)
{
    if (!g_config.enable_checkpoint_save || !g_registered_brain) {
        return;
    }

    // Make checkpoint directory if needed
    mkdir(g_config.checkpoint_path, 0755);

    // Generate timestamped checkpoint filename
    char checkpoint_file[NIMCP_METRICS_PATH_SIZE];
    time_t now = time(NULL);
    snprintf(checkpoint_file, sizeof(checkpoint_file),
             "%s/crash_checkpoint_%ld.nimcp",
             g_config.checkpoint_path, now);

    // Attempt to save (may fail in signal handler)
    if (brain_save(g_registered_brain, checkpoint_file)) {
        g_checkpoint_saves++;
        safe_write("\n*** Checkpoint saved to: ");
        safe_write(checkpoint_file);
        safe_write("\n");
    } else {
        safe_write("\n*** Failed to save checkpoint\n");
    }
}

/**
 * WHAT: Write integer to stderr (signal-safe)
 * WHY:  sprintf() is NOT signal-safe
 * HOW:  Manual digit extraction and write()
 */
static void safe_write_int(int value)
{
    char buf[32];
    int i = 0;
    int is_negative = (value < 0);

    if (is_negative) {
        value = -value;
    }

    // Extract digits in reverse
    do {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    } while (value > 0 && i < 31);

    if (is_negative) {
        buf[i++] = '-';
    }

    // Write in forward order (signal handler — discard return value)
    for (int j = i - 1; j >= 0; j--) {
        ssize_t _r = write(STDERR_FILENO, &buf[j], 1);
        (void)_r;
    }
}

/**
 * WHAT: Log stack trace (best-effort, may not be fully signal-safe)
 * WHY:  Helps diagnose crash location
 * HOW:  Use backtrace() and backtrace_symbols_fd()
 */
static void log_stack_trace(void)
{
    void* buffer[100];
    int nptrs = backtrace(buffer, 100);

    safe_write("\n=== STACK TRACE ===\n");
    // backtrace_symbols_fd() is async-signal-safe according to man pages
    backtrace_symbols_fd(buffer, nptrs, STDERR_FILENO);
    safe_write("===================\n");
}

/**
 * WHAT: Write hex pointer to stderr (signal-safe)
 * WHY:  Display addresses without sprintf
 * HOW:  Manual hex digit extraction
 */
static void safe_write_hex(uintptr_t value)
{
    char buf[32];
    const char* hex = "0123456789abcdef";
    int i = 0;

    /* Handle zero specially (signal handler — discard return value) */
    if (value == 0) {
        ssize_t _r = write(STDERR_FILENO, "0x0", 3);
        (void)_r;
        return;
    }

    /* Extract hex digits in reverse */
    while (value > 0 && i < 16) {
        buf[i++] = hex[value & 0xF];
        value >>= 4;
    }

    /* Write "0x" prefix. We're in a signal handler — if the write fails
     * there's nothing safe we can do about it (no LOG, no errno handling),
     * so we explicitly discard the return value via a comma expression.
     * Plain `(void)write(...)` is rejected by gcc's __warn_unused_result__
     * attribute on glibc's write(); the `if (r) {}` idiom satisfies it. */
    { ssize_t _r = write(STDERR_FILENO, "0x", 2); (void)_r; }

    /* Write in forward order */
    for (int j = i - 1; j >= 0; j--) {
        ssize_t _r = write(STDERR_FILENO, &buf[j], 1); (void)_r;
    }
}

//=============================================================================
// Crash Context Capture Functions
//=============================================================================

/**
 * WHAT: Parse /proc/self/maps to find memory region for address
 * WHY:  Understand what memory region fault address belongs to
 * HOW:  Read /proc/self/maps line by line, parse address ranges
 *
 * NOTE: This is NOT fully async-signal-safe (uses open/read/close)
 *       but is called in best-effort mode for diagnostics
 *
 * @param addr Address to look up
 * @param out_buf Buffer to write region info
 * @param buf_size Size of output buffer
 * @return true if region found, false otherwise
 */
static bool parse_proc_maps_for_address(void* addr, char* out_buf, size_t buf_size)
{
    if (!out_buf || buf_size == 0) {
        /* NO NIMCP_THROW_TO_IMMUNE here — this function is called from signal
         * handlers where longjmp (used by THROW) is NOT async-signal-safe */
        return false;
    }

    out_buf[0] = '\0';

    int fd = open("/proc/self/maps", O_RDONLY);
    if (fd < 0) {
        /* No THROW — signal handler context, not async-signal-safe */
        return false;
    }

    char line_buf[NIMCP_ERROR_BUFFER_LARGE];
    ssize_t bytes_read;
    size_t line_pos = 0;
    uintptr_t target = (uintptr_t)addr;

    while ((bytes_read = read(fd, line_buf + line_pos,
                              sizeof(line_buf) - line_pos - 1)) > 0) {
        line_pos += bytes_read;
        line_buf[line_pos] = '\0';

        /* Process complete lines */
        char* line_start = line_buf;
        char* newline;

        while ((newline = strchr(line_start, '\n')) != NULL) {
            *newline = '\0';

            /* Parse line: "start-end perms offset dev inode pathname" */
            uintptr_t start = 0, end = 0;
            char perms[8] = {0};

            /* Manual parsing to avoid sscanf (not async-signal-safe) */
            char* p = line_start;

            /* Parse start address */
            while (*p && *p != '-') {
                if (*p >= '0' && *p <= '9') {
                    start = (start << 4) | (*p - '0');
                } else if (*p >= 'a' && *p <= 'f') {
                    start = (start << 4) | (*p - 'a' + 10);
                }
                p++;
            }
            if (*p == '-') p++;

            /* Parse end address */
            while (*p && *p != ' ') {
                if (*p >= '0' && *p <= '9') {
                    end = (end << 4) | (*p - '0');
                } else if (*p >= 'a' && *p <= 'f') {
                    end = (end << 4) | (*p - 'a' + 10);
                }
                p++;
            }
            if (*p == ' ') p++;

            /* Parse permissions */
            int perm_idx = 0;
            while (*p && *p != ' ' && perm_idx < 7) {
                perms[perm_idx++] = *p++;
            }
            perms[perm_idx] = '\0';

            /* Check if target is in this range */
            if (target >= start && target < end) {
                /* Find pathname at end of line */
                char* pathname = strrchr(line_start, ' ');
                if (pathname) pathname++;
                else pathname = "";

                /* Format output */
                snprintf(out_buf, buf_size,
                         "0x%lx-0x%lx %s %s",
                         (unsigned long)start,
                         (unsigned long)end,
                         perms, pathname);

                close(fd);
                return true;
            }

            line_start = newline + 1;
        }

        /* Move remaining partial line to start of buffer */
        size_t remaining = line_pos - (line_start - line_buf);
        if (remaining > 0 && remaining < sizeof(line_buf)) {
            memmove(line_buf, line_start, remaining);
        }
        line_pos = remaining;
    }

    close(fd);
    /* No THROW — signal handler context */
    return false;
}

/**
 * WHAT: Capture full crash context from signal handler
 * WHY:  Provide complete crash info for code immune processing
 * HOW:  Extract registers from ucontext, capture backtrace, lookup memory region
 *
 * @param sig Signal number
 * @param info Signal info structure
 * @param uc Ucontext structure
 * @param ctx Output crash context
 * @return true on success
 */
static bool capture_crash_context(int sig, siginfo_t* info,
                                   ucontext_t* uc, crash_context_t* ctx)
{
    if (!ctx) {
        /* No THROW — called from signal handler, not async-signal-safe */
        return false;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->signal = sig;

    /* Extract fault address from siginfo */
    if (info) {
        ctx->fault_address = info->si_addr;
    }

    /* Extract register state from ucontext (platform-specific) */
#if defined(__linux__) && defined(__x86_64__)
    if (uc) {
        mcontext_t* mctx = &uc->uc_mcontext;
        ctx->instruction_pointer = (void*)mctx->gregs[REG_RIP];
        ctx->stack_pointer = (void*)mctx->gregs[REG_RSP];
        ctx->base_pointer = (void*)mctx->gregs[REG_RBP];
    }
#elif defined(__linux__) && defined(__i386__)
    if (uc) {
        mcontext_t* mctx = &uc->uc_mcontext;
        ctx->instruction_pointer = (void*)mctx->gregs[REG_EIP];
        ctx->stack_pointer = (void*)mctx->gregs[REG_ESP];
        ctx->base_pointer = (void*)mctx->gregs[REG_EBP];
    }
#elif defined(__linux__) && defined(__aarch64__)
    if (uc) {
        mcontext_t* mctx = &uc->uc_mcontext;
        ctx->instruction_pointer = (void*)mctx->pc;
        ctx->stack_pointer = (void*)mctx->sp;
        ctx->base_pointer = (void*)mctx->regs[29];  /* x29/fp */
    }
#else
    /* Fallback: mark registers as unavailable */
    if (uc) {
        (void)uc;  /* Suppress unused warning */
    }
#endif

    /* Capture backtrace */
    ctx->backtrace_depth = backtrace(ctx->backtrace, SIGNAL_HANDLER_BACKTRACE_DEPTH);

    /* Get memory region info for fault address (best-effort) */
    if (ctx->fault_address) {
        parse_proc_maps_for_address(ctx->fault_address,
                                    ctx->memory_region,
                                    sizeof(ctx->memory_region));
    }

    return true;
}

/**
 * WHAT: Log crash context details (signal-safe)
 * WHY:  Provide diagnostic output for crash analysis
 * HOW:  Use signal-safe write functions
 *
 * @param ctx Crash context to log
 */
static void log_crash_context(const crash_context_t* ctx)
{
    if (!ctx) return;

    safe_write("\n=== CRASH CONTEXT ===\n");

    safe_write("  Fault Address: ");
    safe_write_hex((uintptr_t)ctx->fault_address);
    safe_write("\n");

    safe_write("  Instruction Pointer: ");
    safe_write_hex((uintptr_t)ctx->instruction_pointer);
    safe_write("\n");

    safe_write("  Stack Pointer: ");
    safe_write_hex((uintptr_t)ctx->stack_pointer);
    safe_write("\n");

    safe_write("  Base Pointer: ");
    safe_write_hex((uintptr_t)ctx->base_pointer);
    safe_write("\n");

    if (ctx->memory_region[0] != '\0') {
        safe_write("  Memory Region: ");
        safe_write(ctx->memory_region);
        safe_write("\n");
    }

    safe_write("=====================\n");
}

//=============================================================================
// Code Immune Integration
//=============================================================================

/**
 * WHAT: Present crash to code immune system
 * WHY:  Allow immune system to potentially fix and recover from crash
 * HOW:  Call code_immune_present_crash with captured context
 *       Always active — signal handler MUST report crashes to immune system
 *
 * @param ctx Crash context
 * @return true if immune system handled the crash
 */
static bool present_crash_to_code_immune(const crash_context_t* ctx)
{
    if (!g_code_immune || !ctx) {
        /* Cannot use NIMCP_THROW_TO_IMMUNE in signal handler (not async-signal-safe).
         * Just return false — the caller will handle the fallthrough. */
        return false;
    }

    /* Prevent recursive handling */
    if (g_in_immune_handling) {
        return false;
    }
    g_in_immune_handling = 1;

    int result = code_immune_present_crash(
        g_code_immune,
        ctx->signal,
        NULL,  /* ucontext not stored in ctx */
        ctx->fault_address
    );

    g_in_immune_handling = 0;

    return (result == 0);
}

//=============================================================================
// SA_SIGINFO Signal Handlers (Enhanced with full context)
//=============================================================================

/**
 * WHAT: Try thread-local recovery jump
 * WHY:  Attempt to recover using per-thread recovery context
 * HOW:  Check thread-local context first, then global
 *
 * @param sig Signal number that caused crash
 * @param handled Whether code immune handled the crash
 * @return 0 if recovery jump was triggered (does not return), -1 otherwise
 */
static int try_recovery_jump(int sig, bool handled)
{
    signal_recovery_result_t result = handled ?
        RECOVERY_CRASH_HANDLED : RECOVERY_CRASH_UNHANDLED;

    /* Try thread-local recovery first (signal-safe pthread_getspecific) */
    if (g_recovery_ctx_key_initialized) {
        signal_recovery_ctx_t* ctx = pthread_getspecific(g_recovery_ctx_key);
        if (ctx != NULL && ctx->valid) {
            safe_write("!!! Attempting thread-local recovery via siglongjmp\n");
            ctx->result = result;
            ctx->crash_signal = sig;
            ctx->valid = 0;
            siglongjmp(ctx->jmp_buf, result);
            /* NOTREACHED */
        }
    }

    /* Fall back to global recovery point */
    if (g_recovery_jump_valid) {
        safe_write("!!! Attempting global recovery via siglongjmp\n");
        g_recovery_jump_valid = 0;
        siglongjmp(g_recovery_jump_buf, sig);
        /* NOTREACHED */
    }

    /* P1 fix: Cannot use NIMCP_THROW_TO_IMMUNE from signal handler (not async-signal-safe) */
    safe_write("!!! try_recovery_jump: no valid recovery point\n");
    return -1;
}

/**
 * WHAT: Enhanced fatal signal handler with full context capture
 * WHY:  Capture complete crash info for immune system integration
 * HOW:  Use SA_SIGINFO to get siginfo_t and ucontext_t
 *
 * @param sig Signal number
 * @param info Signal info structure
 * @param context Ucontext (cast to void* by sigaction)
 */
static void handle_fatal_signal_extended(int sig, siginfo_t* info, void* context)
{
    ucontext_t* uc = (ucontext_t*)context;
    crash_context_t ctx;
    bool immune_handled = false;

    g_last_signal = sig;
    g_fatal_crashes++;

    /* Update signal-specific counter */
    switch (sig) {
        case SIGSEGV: g_sigsegv_count++; break;
        case SIGABRT: g_sigabrt_count++; break;
        case SIGBUS:  g_sigbus_count++; break;
        case SIGFPE:  g_sigfpe_count++; break;
        case SIGILL:  g_sigill_count++; break;
    }

    /* Log crash (signal-safe) */
    safe_write("\n!!! FATAL SIGNAL RECEIVED: ");
    safe_write_int(sig);
    safe_write(" (");
    safe_write(signal_handler_get_signal_name(sig));
    safe_write(")\n");

    /* Capture full crash context */
    if (capture_crash_context(sig, info, uc, &ctx)) {
        /* Store for potential deferred processing */
        memcpy((void*)&g_pending_crash_context, &ctx, sizeof(ctx));
        g_pending_crash = 1;

        /* Log crash context */
        log_crash_context(&ctx);

        /* Enqueue to exception queue for deferred processing
         * This is signal-safe (uses lock-free SPSC queue) */
        if (signal_exception_queue_is_initialized()) {
            if (signal_exception_queue_enqueue(sig, &ctx)) {
                safe_write("!!! Crash queued for exception processing\n");
            } else {
                safe_write("!!! Warning: Exception queue full, crash not queued\n");
            }
        }

        /* Always try code immune system (no ifdef guard — always active) */
        if (g_code_immune && present_crash_to_code_immune(&ctx)) {
            safe_write("!!! Code immune system handling crash...\n");
            g_recoveries++;
            immune_handled = true;
        }

        /* Attempt recovery jump (thread-local or global) */
        /* This will not return if a valid recovery point exists */
        if (try_recovery_jump(sig, immune_handled) == 0) {
            /* Should not reach here - siglongjmp does not return */
        }

        /* If immune handled but no recovery point, try to continue */
        if (immune_handled) {
            safe_write("!!! Code immune handled crash but no recovery point\n");
            safe_write("!!! Attempting to continue (may cause re-crash)\n");
            return;
        }
    }

    /* No recovery point — terminal crash. Still report to immune system. */
    safe_write("!!! No recovery point available — crash is terminal\n");

    /* Log stack trace if enabled */
    if (g_config.enable_stack_trace) {
        log_stack_trace();
    }

    /* Call custom crash callback if set */
    if (g_config.on_fatal_signal) {
        g_config.on_fatal_signal(sig);
    }

    /* Attempt checkpoint save if enabled */
    if (g_config.enable_checkpoint_save && g_registered_brain) {
        safe_write("\n!!! Attempting checkpoint save (signal handler context)\n");
        attempt_checkpoint_save_unsafe();
    }

    /* Restore default handler and re-raise to get core dump.
     * NOTE: We do NOT call _exit() — we let the OS generate a core dump
     * via the default SIGSEGV handler for post-mortem debugging. */
    signal(sig, SIG_DFL);
    raise(sig);
}

/**
 * WHAT: Enhanced SIGFPE handler with context capture
 * WHY:  Allow FPE recovery with full diagnostic info
 * HOW:  Capture context, try recovery or continue if configured
 *
 * @param sig Signal number
 * @param info Signal info structure
 * @param context Ucontext
 */
static void handle_sigfpe_extended(int sig, siginfo_t* info, void* context)
{
    ucontext_t* uc = (ucontext_t*)context;
    crash_context_t ctx;
    bool immune_handled = false;

    g_last_signal = sig;
    g_sigfpe_count++;

    safe_write("\n*** FLOATING POINT EXCEPTION (SIGFPE) ***\n");

    /* Capture context for diagnostics */
    if (capture_crash_context(sig, info, uc, &ctx)) {
        /* Store for deferred processing */
        memcpy((void*)&g_pending_crash_context, &ctx, sizeof(ctx));
        g_pending_crash = 1;

        log_crash_context(&ctx);

        /* Enqueue to exception queue for deferred processing */
        if (signal_exception_queue_is_initialized()) {
            signal_exception_queue_enqueue(sig, &ctx);
        }

        /* Always try code immune system */
        if (g_code_immune && present_crash_to_code_immune(&ctx)) {
            g_recoveries++;
            immune_handled = true;
            safe_write("*** Code immune handled FPE\n");
        }

        /* Attempt recovery jump if available */
        if (try_recovery_jump(sig, immune_handled) == 0) {
            /* NOTREACHED - siglongjmp does not return */
        }

        /* If immune handled, return */
        if (immune_handled) {
            return;
        }
    }

    if (g_config.sigfpe_mode == SIGNAL_MODE_LOG_CONTINUE) {
        safe_write("*** Attempting to continue (may propagate NaN/Inf)\n");
        g_recoveries++;
        return;
    }

    /* Treat as fatal */
    handle_fatal_signal_extended(sig, info, context);
}

//=============================================================================
// Signal Handlers (MUST BE ASYNC-SIGNAL-SAFE)
//=============================================================================

/**
 * WHAT: Handle fatal signals (SIGSEGV, SIGBUS, SIGABRT, etc.)
 * WHY:  Log crash info before termination
 * HOW:  Write to stderr, optionally log stack trace, save checkpoint, then exit
 */
static void handle_fatal_signal(int sig)
{
    g_last_signal = sig;
    g_fatal_crashes++;

    // Update signal-specific counter
    switch (sig) {
        case SIGSEGV: g_sigsegv_count++; break;
        case SIGABRT: g_sigabrt_count++; break;
        case SIGBUS:  g_sigbus_count++; break;
        case SIGFPE:  g_sigfpe_count++; break;
        case SIGILL:  g_sigill_count++; break;
    }

    // Log crash (signal-safe)
    safe_write("\n!!! FATAL SIGNAL RECEIVED: ");
    safe_write_int(sig);
    safe_write(" (");
    safe_write(signal_handler_get_signal_name(sig));
    safe_write(")\n");

    // Log stack trace if enabled
    if (g_config.enable_stack_trace) {
        log_stack_trace();
    }

    // Call custom crash callback if set
    // WARNING: The callback MUST be async-signal-safe!
    // If callback uses non-async-signal-safe functions, behavior is undefined.
    if (g_config.on_fatal_signal) {
        g_config.on_fatal_signal(sig);
    }

    // TODO COMPLETED: Attempt checkpoint save if enabled
    // NOTE: This is risky in a signal handler but necessary for recovery
    // We attempt it with proper error handling and logging
    if (g_config.enable_checkpoint_save && g_registered_brain) {
        safe_write("\n!!! Attempting checkpoint save (signal handler context)\n");
        attempt_checkpoint_save_unsafe();
    }

    safe_write("!!! Process terminating due to fatal signal\n");

    // Restore default handler and re-raise to generate core dump
    signal(sig, SIG_DFL);
    raise(sig);
}

/**
 * WHAT: Handle floating point exceptions
 * WHY:  Attempt to continue processing with NaN/Inf
 * HOW:  Log error and return (may cause propagation of NaN)
 */
static void handle_sigfpe(int sig)
{
    g_last_signal = sig;
    g_sigfpe_count++;

    safe_write("\n*** FLOATING POINT EXCEPTION (SIGFPE) ***\n");

    if (g_config.sigfpe_mode == SIGNAL_MODE_LOG_CONTINUE) {
        safe_write("*** Attempting to continue (may propagate NaN/Inf)\n");
        g_recoveries++;
        // Return from handler - FPU state restored by OS
        return;
    } else {
        // Treat as fatal
        handle_fatal_signal(sig);
    }
}

/**
 * WHAT: Handle termination signals (SIGTERM, SIGINT)
 * WHY:  Graceful shutdown with state save
 * HOW:  Set flag for main loop to check
 */
static void handle_shutdown_signal(int sig)
{
    g_last_signal = sig;
    g_shutdown_requested = 1;

    if (sig == SIGTERM) {
        g_sigterm_count++;
    } else if (sig == SIGINT) {
        g_sigint_count++;
    }

    safe_write("\n=== SHUTDOWN SIGNAL RECEIVED: ");
    safe_write_int(sig);
    safe_write(" ===\n");
    safe_write("=== Graceful shutdown initiated ===\n");

    // Call custom shutdown callback if set
    // WARNING: The callback MUST be async-signal-safe!
    // If callback uses non-async-signal-safe functions, behavior is undefined.
    if (g_config.on_graceful_shutdown) {
        g_config.on_graceful_shutdown();
    }
}

/**
 * WHAT: Handle config reload signal (SIGHUP)
 * WHY:  Reload configuration without restarting
 * HOW:  Set flag for main loop to check
 */
static void handle_sighup(int sig)
{
    (void)sig;  // Unused
    g_last_signal = SIGHUP;
    g_sighup_count++;
    g_reload_requested = 1;

    safe_write("\n=== CONFIG RELOAD REQUESTED (SIGHUP) ===\n");

    // Call custom reload callback if set
    // WARNING: The callback MUST be async-signal-safe!
    // If callback uses non-async-signal-safe functions, behavior is undefined.
    if (g_config.on_reload_config) {
        g_config.on_reload_config();
    }
}

//=============================================================================
// Public API Implementation
//=============================================================================

signal_handler_config_t signal_handler_default_config(void)
{
    signal_handler_config_t config = {
        // Fatal signals → log and shutdown
        .sigsegv_mode = SIGNAL_MODE_LOG_SHUTDOWN,
        .sigabrt_mode = SIGNAL_MODE_LOG_SHUTDOWN,
        .sigbus_mode = SIGNAL_MODE_LOG_SHUTDOWN,
        .sigfpe_mode = SIGNAL_MODE_LOG_CONTINUE,  // Try to continue with NaN
        .sigill_mode = SIGNAL_MODE_LOG_SHUTDOWN,

        // Termination signals → graceful shutdown
        .sigterm_mode = SIGNAL_MODE_LOG_SHUTDOWN,
        .sigint_mode = SIGNAL_MODE_LOG_SHUTDOWN,

        // Config reload
        .sighup_mode = SIGNAL_MODE_LOG_CONTINUE,

        // Diagnostics
        .enable_stack_trace = true,
        .enable_state_dump = false,      // Disabled by default (not signal-safe)
        .enable_checkpoint_save = false, // Disabled by default (not signal-safe)

        // Paths
        .crash_log_path = "/tmp/nimcp_crash.log",
        .checkpoint_path = "/tmp/nimcp_checkpoint",

        // Callbacks
        .on_fatal_signal = NULL,
        .on_reload_config = NULL,
        .on_graceful_shutdown = NULL
    };
    return config;
}

bool signal_handler_install(const signal_handler_config_t* config)
{
    if (g_installed) {
        return true;  /* Already installed — idempotent */
    }

    /* Use defaults if config is NULL */
    if (config) {
        g_config = *config;
    } else {
        g_config = signal_handler_default_config();
    }

    /* Initialize exception queue before installing handlers
     * This ensures the queue is ready before any signals can occur */
    if (signal_exception_queue_init() != 0) {
        /* Non-fatal - continue without exception queue integration */
        LOG_WARNING("Failed to initialize signal exception queue");
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);

    /*
     * Install crash signal handlers with SA_SIGINFO for full context capture.
     * This enables code immune system integration with register state and
     * detailed fault information.
     */

    /* Install SIGSEGV handler with SA_SIGINFO */
    if (g_config.sigsegv_mode != SIGNAL_MODE_IGNORE) {
        memset(&sa, 0, sizeof(sa));
        sigemptyset(&sa.sa_mask);
        sa.sa_sigaction = handle_fatal_signal_extended;
        sa.sa_flags = SA_SIGINFO | SA_RESETHAND;  /* Get siginfo, reset after first signal */
        if (sigaction(SIGSEGV, &sa, &g_old_sigsegv) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "signal_handler_install: validation failed");
            return false;
        }
    }

    /* Install SIGABRT handler with SA_SIGINFO.
     * NOTE: Do NOT use SA_RESETHAND for SIGABRT — glibc's free() heap
     * corruption check calls abort() which raises SIGABRT. With SA_RESETHAND,
     * the handler fires once then resets to SIG_DFL, so subsequent SIGABRT
     * from heap corruption bypasses our handler entirely. Without SA_RESETHAND,
     * abort() will still terminate after our handler runs (it unblocks SIGABRT
     * and re-raises), but we get to log the backtrace and save a checkpoint. */
    if (g_config.sigabrt_mode != SIGNAL_MODE_IGNORE) {
        memset(&sa, 0, sizeof(sa));
        sigemptyset(&sa.sa_mask);
        sa.sa_sigaction = handle_fatal_signal_extended;
        sa.sa_flags = SA_SIGINFO;  /* No SA_RESETHAND — persist across signals */
        if (sigaction(SIGABRT, &sa, &g_old_sigabrt) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "signal_handler_install: validation failed");
            return false;
        }
    }

    /* Install SIGBUS handler with SA_SIGINFO */
    if (g_config.sigbus_mode != SIGNAL_MODE_IGNORE) {
        memset(&sa, 0, sizeof(sa));
        sigemptyset(&sa.sa_mask);
        sa.sa_sigaction = handle_fatal_signal_extended;
        sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
        if (sigaction(SIGBUS, &sa, &g_old_sigbus) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "signal_handler_install: validation failed");
            return false;
        }
    }

    /* Install SIGFPE handler with SA_SIGINFO (no reset - want to recover) */
    if (g_config.sigfpe_mode != SIGNAL_MODE_IGNORE) {
        memset(&sa, 0, sizeof(sa));
        sigemptyset(&sa.sa_mask);
        sa.sa_sigaction = handle_sigfpe_extended;
        sa.sa_flags = SA_SIGINFO;  /* No SA_RESETHAND - allow recovery */
        if (sigaction(SIGFPE, &sa, &g_old_sigfpe) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "signal_handler_install: validation failed");
            return false;
        }
    }

    /* Install SIGILL handler with SA_SIGINFO */
    if (g_config.sigill_mode != SIGNAL_MODE_IGNORE) {
        memset(&sa, 0, sizeof(sa));
        sigemptyset(&sa.sa_mask);
        sa.sa_sigaction = handle_fatal_signal_extended;
        sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
        if (sigaction(SIGILL, &sa, &g_old_sigill) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "signal_handler_install: validation failed");
            return false;
        }
    }

    /* Install SIGTERM handler (simple handler, no SA_SIGINFO needed) */
    if (g_config.sigterm_mode != SIGNAL_MODE_IGNORE) {
        memset(&sa, 0, sizeof(sa));
        sigemptyset(&sa.sa_mask);
        sa.sa_handler = handle_shutdown_signal;
        sa.sa_flags = 0;
        if (sigaction(SIGTERM, &sa, &g_old_sigterm) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "signal_handler_install: validation failed");
            return false;
        }
    }

    /* Install SIGINT handler (simple handler) */
    if (g_config.sigint_mode != SIGNAL_MODE_IGNORE) {
        memset(&sa, 0, sizeof(sa));
        sigemptyset(&sa.sa_mask);
        sa.sa_handler = handle_shutdown_signal;
        sa.sa_flags = 0;
        if (sigaction(SIGINT, &sa, &g_old_sigint) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "signal_handler_install: validation failed");
            return false;
        }
    }

    /* Install SIGHUP handler (simple handler) */
    if (g_config.sighup_mode != SIGNAL_MODE_IGNORE) {
        memset(&sa, 0, sizeof(sa));
        sigemptyset(&sa.sa_mask);
        sa.sa_handler = handle_sighup;
        sa.sa_flags = 0;
        if (sigaction(SIGHUP, &sa, &g_old_sighup) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "signal_handler_install: validation failed");
            return false;
        }
    }

    g_installed = true;
    return true;
}

bool signal_handler_uninstall(void)
{
    if (!g_installed) {
        return false;  // Not installed - nothing to uninstall
    }

    /* Process any remaining exceptions before shutdown */
    signal_handler_process_pending_exceptions(0);

    /* Shutdown exception queue */
    signal_exception_queue_shutdown();

    // Restore previous handlers
    sigaction(SIGSEGV, &g_old_sigsegv, NULL);
    sigaction(SIGABRT, &g_old_sigabrt, NULL);
    sigaction(SIGBUS, &g_old_sigbus, NULL);
    sigaction(SIGFPE, &g_old_sigfpe, NULL);
    sigaction(SIGILL, &g_old_sigill, NULL);
    sigaction(SIGTERM, &g_old_sigterm, NULL);
    sigaction(SIGINT, &g_old_sigint, NULL);
    sigaction(SIGHUP, &g_old_sighup, NULL);

    g_installed = false;
    return true;
}

void signal_handler_register_brain(brain_t brain)
{
    g_registered_brain = brain;
}

void signal_handler_unregister_brain(void)
{
    g_registered_brain = NULL;
}

signal_handler_stats_t signal_handler_get_stats(void)
{
    signal_handler_stats_t stats = {
        .sigsegv_count = g_sigsegv_count,
        .sigabrt_count = g_sigabrt_count,
        .sigbus_count = g_sigbus_count,
        .sigfpe_count = g_sigfpe_count,
        .sigill_count = g_sigill_count,
        .sigterm_count = g_sigterm_count,
        .sigint_count = g_sigint_count,
        .sighup_count = g_sighup_count,
        .recoveries = g_recoveries,
        .fatal_crashes = g_fatal_crashes
    };
    return stats;
}

void signal_handler_reset_stats(void)
{
    g_sigsegv_count = 0;
    g_sigabrt_count = 0;
    g_sigbus_count = 0;
    g_sigfpe_count = 0;
    g_sigill_count = 0;
    g_sigterm_count = 0;
    g_sigint_count = 0;
    g_sighup_count = 0;
    g_recoveries = 0;
    g_fatal_crashes = 0;
}

bool signal_handler_shutdown_requested(void)
{
    return (g_shutdown_requested != 0);
}

bool signal_handler_reload_requested(void)
{
    /* Use atomic compare-exchange to avoid race condition in read-modify-write */
    int expected = 1;
    return __atomic_compare_exchange_n(&g_reload_requested, &expected, 0,
                                        false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED);
}

void signal_handler_set_crash_callback(void (*callback)(int sig))
{
    g_config.on_fatal_signal = callback;
}

void signal_handler_set_reload_callback(void (*callback)(void))
{
    g_config.on_reload_config = callback;
}

bool signal_handler_force_checkpoint(void)
{
    // Deprecated: use signal_handler_checkpoint_save() instead
    return signal_handler_checkpoint_save(NULL);
}

int signal_handler_get_last_signal(void)
{
    return (int)g_last_signal;
}

const char* signal_handler_get_signal_name(int sig)
{
    switch (sig) {
        case SIGSEGV: return "SIGSEGV";
        case SIGABRT: return "SIGABRT";
        case SIGBUS:  return "SIGBUS";
        case SIGFPE:  return "SIGFPE";
        case SIGILL:  return "SIGILL";
        case SIGTERM: return "SIGTERM";
        case SIGINT:  return "SIGINT";
        case SIGHUP:  return "SIGHUP";
        default:      return "UNKNOWN";
    }
}

//=============================================================================
// Enhanced Recovery & Diagnostics Implementation
//=============================================================================

/**
 * WHAT: Count checkpoints in retention directory
 * WHY:  Monitor checkpoint storage and manage retention
 * HOW:  Scan directory and count .nimcp files
 */
static int count_checkpoints_in_dir(const char* dir_path)
{
    if (!dir_path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dir_path is NULL");

        return -1;
    }

    DIR* dir = opendir(dir_path);
    if (!dir) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dir is NULL");

        return -1;
    }

    int count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Count .nimcp checkpoint files (excluding backup files)
        if (entry->d_type == DT_REG && strstr(entry->d_name, ".nimcp")) {
            count++;
        }
    }

    closedir(dir);
    return count;
}

/**
 * WHAT: Cleanup old checkpoints to maintain retention limit
 * WHY:  Prevent disk space exhaustion from checkpoint accumulation
 * HOW:  Delete oldest files when limit exceeded
 */
static void cleanup_old_checkpoints(const char* dir_path, int max_count)
{
    if (!dir_path || max_count <= 0) {
        return;
    }

    DIR* dir = opendir(dir_path);
    if (!dir) {
        return;
    }

    // Collect checkpoint files with timestamps
    typedef struct {
        char* filename;
        time_t mtime;
    } checkpoint_entry_t;

    checkpoint_entry_t* entries = NULL;
    int entry_count = 0;
    int capacity = 16;

    entries = nimcp_malloc(sizeof(checkpoint_entry_t) * capacity);
    if (!entries) {
        closedir(dir);
        return;
    }

    // Scan directory
    struct dirent* entry;
    char fullpath[NIMCP_METRICS_PATH_SIZE];
    struct stat st;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && strstr(entry->d_name, ".nimcp")) {
            // Get file modification time
            snprintf(fullpath, sizeof(fullpath), "%s/%s", dir_path, entry->d_name);
            if (stat(fullpath, &st) == 0) {
                // Resize if needed
                if (entry_count >= capacity) {
                    capacity *= 2;
                    checkpoint_entry_t* new_entries = nimcp_realloc(entries,
                        sizeof(checkpoint_entry_t) * capacity);
                    if (!new_entries) {
                        break;
                    }
                    entries = new_entries;
                }

                size_t name_len = strlen(entry->d_name);
                entries[entry_count].filename = nimcp_malloc(name_len + 1);
                if (entries[entry_count].filename) {
                    memcpy(entries[entry_count].filename, entry->d_name, name_len + 1);
                    entries[entry_count].mtime = st.st_mtime;
                    entry_count++;
                }
            }
        }
    }

    closedir(dir);

    // Sort by modification time (oldest first)
    for (int i = 0; i < entry_count - 1; i++) {
        for (int j = i + 1; j < entry_count; j++) {
            if (entries[j].mtime < entries[i].mtime) {
                // Swap
                checkpoint_entry_t temp = entries[i];
                entries[i] = entries[j];
                entries[j] = temp;
            }
        }
    }

    // Delete oldest files beyond retention limit
    for (int i = 0; i < entry_count && i < (entry_count - max_count); i++) {
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir_path, entries[i].filename);
        remove(fullpath);
    }

    // Cleanup
    for (int i = 0; i < entry_count; i++) {
        nimcp_free(entries[i].filename);
    }
    nimcp_free(entries);
}

signal_health_info_t signal_handler_get_health_status(void)
{
    signal_health_info_t health = {0};

    // Calculate totals
    uint64_t total_signals = g_sigsegv_count + g_sigabrt_count + g_sigbus_count +
                            g_sigfpe_count + g_sigill_count + g_sigterm_count +
                            g_sigint_count + g_sighup_count;

    health.total_signals = total_signals;
    health.fatal_crashes = g_fatal_crashes;
    health.successful_recoveries = g_recoveries;
    health.failed_recoveries = g_recovery_failures;
    health.checkpoint_saves = g_checkpoint_saves;
    health.last_signal = g_last_signal;
    health.last_signal_name = signal_handler_get_signal_name(g_last_signal);
    health.is_in_recovery = (g_recovery_attempt_count > 0);

    // Calculate recovery success rate
    uint64_t total_attempts = g_recoveries + g_recovery_failures;
    health.recovery_success_rate = (total_attempts > 0) ?
        (100.0F * g_recoveries / total_attempts) : 0.0F;

    // Determine health status based on metrics
    if (g_fatal_crashes == 0 && g_recovery_failures == 0 && g_sigsegv_count == 0) {
        health.status = SIGNAL_HEALTH_HEALTHY;
    } else if (g_recovery_failures > 0 || g_sigsegv_count > 5) {
        health.status = SIGNAL_HEALTH_CRITICAL;
    } else if (g_sigsegv_count > 3 || health.recovery_success_rate < 50.0F) {
        health.status = SIGNAL_HEALTH_COMPROMISED;
    } else if (g_fatal_crashes > 0 || g_recovery_failures > 0) {
        health.status = SIGNAL_HEALTH_DEGRADED;
    } else {
        health.status = SIGNAL_HEALTH_HEALTHY;
    }

    return health;
}

bool signal_handler_checkpoint_save(const char* checkpoint_path)
{
    // Use provided path or fall back to config default
    const char* path = checkpoint_path ? checkpoint_path : g_config.checkpoint_path;

    if (!g_registered_brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "signal_handler_checkpoint_save: g_registered_brain is NULL");
        return false;
    }

    if (!path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "signal_handler_checkpoint_save: path is NULL");
        return false;
    }

    // Create checkpoint directory if it doesn't exist
    mkdir(path, 0755);

    // Generate timestamped checkpoint filename
    char checkpoint_file[NIMCP_METRICS_PATH_SIZE];
    time_t now = time(NULL);
    snprintf(checkpoint_file, sizeof(checkpoint_file),
             "%s/checkpoint_%ld.nimcp",
             path, now);

    // Attempt save
    if (brain_save(g_registered_brain, checkpoint_file)) {
        g_checkpoint_saves++;

        // Cleanup old checkpoints if retention limit is set
        if (g_checkpoint_retention > 0) {
            cleanup_old_checkpoints(path, g_checkpoint_retention);
        }

        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "signal_handler_checkpoint_save: validation failed");
    return false;
}

void signal_handler_set_checkpoint_retention(int max_checkpoints)
{
    g_checkpoint_retention = max_checkpoints;
}

int signal_handler_get_checkpoint_count(void)
{
    return count_checkpoints_in_dir(g_config.checkpoint_path);
}

void signal_handler_set_auto_recovery(bool enable)
{
    g_auto_recovery_enabled = enable;
}

bool signal_handler_is_auto_recovery_enabled(void)
{
    return g_auto_recovery_enabled;
}

void signal_handler_set_max_recovery_attempts(int max_attempts)
{
    g_max_recovery_attempts = max_attempts;
}

//=============================================================================
// Code Immune System Integration API
//=============================================================================

/**
 * WHAT: Register code immune system with signal handler
 * WHY:  Enable immune-based crash handling and recovery
 * HOW:  Store reference to immune system for use in crash handlers
 *       Always active — no ifdef guard. Crashes MUST be reported to immune.
 *
 * @param sys Code immune system instance
 */
void signal_handler_set_code_immune(code_immune_system_t* sys)
{
    g_code_immune = sys;
}

/**
 * WHAT: Get registered code immune system
 * WHY:  Allow external code to check immune system status
 * HOW:  Return stored reference
 *
 * @return Code immune system or NULL
 */
code_immune_system_t* signal_handler_get_code_immune(void)
{
    return g_code_immune;
}

/**
 * WHAT: Set recovery point for siglongjmp-based recovery
 * WHY:  Enable resumption after code immune fixes a crash
 * HOW:  Use sigsetjmp to save execution context
 *
 * NOTE: Call this in a safe location before entering crash-prone code.
 *       If a crash occurs and code immune handles it, execution will
 *       resume from this point with the signal number as return value.
 *
 * @return 0 on initial call, signal number on recovery jump
 */
int signal_handler_set_recovery_point(void)
{
    int result = sigsetjmp(g_recovery_jump_buf, 1);
    if (result == 0) {
        g_recovery_jump_valid = 1;
    }
    return result;
}

/**
 * WHAT: Clear recovery point
 * WHY:  Disable recovery jump when exiting crash-prone code
 * HOW:  Clear validity flag
 */
void signal_handler_clear_recovery_point(void)
{
    g_recovery_jump_valid = 0;
}

/**
 * WHAT: Check if a crash is pending deferred processing
 * WHY:  Allow main thread to process crash after signal handler returns
 * HOW:  Check pending flag
 *
 * @return true if crash is pending
 */
bool signal_handler_has_pending_crash(void)
{
    return (g_pending_crash != 0);
}

/**
 * WHAT: Get pending crash context
 * WHY:  Access crash details for analysis or recovery
 * HOW:  Copy pending context to output buffer
 *
 * @param ctx Output buffer for crash context
 * @return true if crash was pending and copied
 */
bool signal_handler_get_pending_crash(signal_crash_context_t* ctx)
{
    if (!ctx || !g_pending_crash) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "signal_handler_get_pending_crash: required parameter is NULL (ctx, g_pending_crash)");
        return false;
    }

    memcpy(ctx, (void*)&g_pending_crash_context, sizeof(*ctx));
    return true;
}

/**
 * WHAT: Clear pending crash flag
 * WHY:  Acknowledge crash has been processed
 * HOW:  Reset pending flag
 */
void signal_handler_clear_pending_crash(void)
{
    g_pending_crash = 0;
}

/**
 * WHAT: Check if code immune integration is enabled
 * WHY:  Allow runtime check of immune system availability
 * HOW:  Check compile-time flag and runtime registration
 *
 * @return true if code immune is available
 */
bool signal_handler_has_code_immune(void)
{
    return (g_code_immune != NULL);
}

/**
 * WHAT: Get immune system recovery statistics
 * WHY:  Monitor immune-based recovery effectiveness
 * HOW:  Return counts from global state
 *
 * @param immune_recoveries Output for immune-handled recoveries
 * @param total_crashes Output for total crash count
 */
void signal_handler_get_immune_stats(uint64_t* immune_recoveries, uint64_t* total_crashes)
{
    if (immune_recoveries) {
        *immune_recoveries = g_recoveries;
    }
    if (total_crashes) {
        *total_crashes = g_fatal_crashes;
    }
}

//=============================================================================
// Thread-Local Recovery API Implementation
//=============================================================================

/**
 * WHAT: Initialize thread-local recovery context
 * WHY:  Each thread needs its own recovery state
 * HOW:  Allocate/init thread-local storage
 */
int signal_handler_init_thread_recovery(void)
{
    /* Initialize pthread key (once across all threads) */
    pthread_once(&g_recovery_ctx_key_once, init_recovery_ctx_key);

    if (!g_recovery_ctx_key_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "g_recovery_ctx_key_initialized is NULL");


        return -1;
    }

    /* Check if already initialized for this thread */
    signal_recovery_ctx_t* ctx = pthread_getspecific(g_recovery_ctx_key);
    if (ctx != NULL) {
        return 0;  /* Already initialized */
    }

    /* Allocate new context for this thread */
    ctx = nimcp_calloc(1, sizeof(signal_recovery_ctx_t));
    if (ctx == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "signal_handler_init_thread_recovery: validation failed");
        return -1;
    }

    /* Initialize context fields */
    ctx->valid = 0;
    ctx->result = RECOVERY_INITIAL;
    ctx->crash_signal = 0;
    ctx->retry_count = 0;
    ctx->max_retries = 0;
    ctx->user_data = NULL;
    ctx->label = NULL;

    /* Store in thread-local storage */
    if (pthread_setspecific(g_recovery_ctx_key, ctx) != 0) {
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "signal_handler_init_thread_recovery: validation failed");
        return -1;
    }

    return 0;
}

/**
 * WHAT: Cleanup thread-local recovery context
 * WHY:  Clean up before thread exit
 * HOW:  Free thread-local storage
 */
void signal_handler_cleanup_thread_recovery(void)
{
    if (!g_recovery_ctx_key_initialized) {
        return;
    }

    signal_recovery_ctx_t* ctx = pthread_getspecific(g_recovery_ctx_key);
    if (ctx != NULL) {
        nimcp_free(ctx);
        pthread_setspecific(g_recovery_ctx_key, NULL);
    }
}

/**
 * WHAT: Get thread-local recovery context
 * WHY:  Access recovery state for current thread
 * HOW:  Return pointer to thread-local context
 */
signal_recovery_ctx_t* signal_handler_get_recovery_ctx(void)
{
    /* Ensure key is initialized */
    pthread_once(&g_recovery_ctx_key_once, init_recovery_ctx_key);

    if (!g_recovery_ctx_key_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "g_recovery_ctx_key_initialized is NULL");


        return NULL;
    }

    signal_recovery_ctx_t* ctx = pthread_getspecific(g_recovery_ctx_key);

    /* Auto-initialize if not yet done for this thread */
    if (ctx == NULL) {
        if (signal_handler_init_thread_recovery() == 0) {
            ctx = pthread_getspecific(g_recovery_ctx_key);
        }
    }

    return ctx;
}

/**
 * WHAT: Set recovery point with extended options
 * WHY:  Save execution context for crash recovery
 * HOW:  Use sigsetjmp with thread-local context
 *
 * NOTE: This function uses sigsetjmp internally. The caller should
 *       check the return value to determine if this is the initial
 *       call or a recovery jump.
 */
int signal_handler_set_recovery_point_ex(
    signal_recovery_ctx_t* ctx,
    int max_retries,
    const char* label)
{
    /* Use global context if thread context not provided */
    if (ctx == NULL) {
        ctx = signal_handler_get_recovery_ctx();
        if (ctx == NULL) {
            /* Fallback to legacy global recovery point */
            return signal_handler_set_recovery_point();
        }
    }

    /* Store configuration */
    ctx->max_retries = max_retries;
    ctx->label = label;

    /* Save execution context with signal mask */
    int result = sigsetjmp(ctx->jmp_buf, 1);

    if (result == 0) {
        /* Initial call - mark recovery point as valid */
        ctx->valid = 1;
        ctx->result = RECOVERY_INITIAL;
        return RECOVERY_INITIAL;
    } else {
        /* Returned from siglongjmp - crash was handled */
        ctx->retry_count++;

        /* Return the recovery result that was passed to siglongjmp */
        return ctx->result;
    }
}

/**
 * WHAT: Clear thread-local recovery point
 * WHY:  Disable recovery jump after safe code completes
 * HOW:  Clear validity flag in thread context
 */
void signal_handler_clear_recovery_point_ex(signal_recovery_ctx_t* ctx)
{
    if (ctx == NULL) {
        ctx = signal_handler_get_recovery_ctx();
        if (ctx == NULL) {
            /* Fallback to legacy global recovery point */
            signal_handler_clear_recovery_point();
            return;
        }
    }

    ctx->valid = 0;
    ctx->result = RECOVERY_INITIAL;
    ctx->crash_signal = 0;
}

/**
 * WHAT: Trigger recovery jump from signal handler
 * WHY:  Resume execution after crash handling
 * HOW:  Use siglongjmp to thread's recovery point
 *
 * NOTE: Called from signal handler context. Does not return if
 *       a valid recovery point exists.
 */
int signal_handler_trigger_recovery(signal_recovery_result_t result)
{
    signal_recovery_ctx_t* ctx = NULL;

    /* Try to get thread-local context (signal-safe) */
    if (g_recovery_ctx_key_initialized) {
        ctx = pthread_getspecific(g_recovery_ctx_key);
    }

    if (ctx != NULL && ctx->valid) {
        /* Store result and signal for post-recovery access */
        ctx->result = result;
        ctx->crash_signal = g_last_signal;

        /* Clear validity before jump to prevent re-entry */
        ctx->valid = 0;

        /* Jump to recovery point - does not return */
        siglongjmp(ctx->jmp_buf, result);
        /* NOTREACHED */
    }

    /* Fall back to global recovery point for backward compatibility */
    if (g_recovery_jump_valid) {
        g_recovery_jump_valid = 0;
        siglongjmp(g_recovery_jump_buf, g_last_signal);
        /* NOTREACHED */
    }

    /* No valid recovery point */
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "signal_handler_trigger_recovery: validation failed");
    return -1;
}

/**
 * WHAT: Check if current thread has valid recovery point
 * WHY:  Query recovery availability before crash
 * HOW:  Check thread-local and global recovery state
 */
bool signal_handler_can_recover(void)
{
    /* Check thread-local recovery */
    if (g_recovery_ctx_key_initialized) {
        signal_recovery_ctx_t* ctx = pthread_getspecific(g_recovery_ctx_key);
        if (ctx != NULL && ctx->valid) {
            return true;
        }
    }

    /* Check global recovery */
    return (g_recovery_jump_valid != 0);
}

/**
 * WHAT: Get crash signal from last recovery
 * WHY:  Access crash details after recovery jump
 * HOW:  Return signal from thread context or global state
 */
int signal_handler_get_crash_signal(void)
{
    /* Try thread-local first */
    if (g_recovery_ctx_key_initialized) {
        signal_recovery_ctx_t* ctx = pthread_getspecific(g_recovery_ctx_key);
        if (ctx != NULL && ctx->crash_signal != 0) {
            return ctx->crash_signal;
        }
    }

    /* Fall back to global last signal */
    return (int)g_last_signal;
}

//=============================================================================
// Exception Queue Integration
//=============================================================================

/**
 * WHAT: Process pending signal exceptions
 * WHY:  Bridge between signal handler queue and exception system
 * HOW:  Delegate to signal_exception_queue_process
 */
size_t signal_handler_process_pending_exceptions(size_t max_count)
{
    if (!signal_exception_queue_is_initialized()) {
        return 0;
    }
    return signal_exception_queue_process(max_count);
}

/**
 * WHAT: Get count of pending signal exceptions
 * WHY:  Monitor exception queue depth
 * HOW:  Delegate to signal_exception_queue_pending_count
 */
size_t signal_handler_get_pending_exception_count(void)
{
    if (!signal_exception_queue_is_initialized()) {
        return 0;
    }
    return signal_exception_queue_pending_count();
}
