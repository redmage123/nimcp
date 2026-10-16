/**
 * @file nimcp_exception_handlers.c
 * @brief Exception handler registration and dispatch implementation
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: Handler chain management and try/catch mechanism
 * WHY:  Enable structured exception handling with priority-based dispatch
 * HOW:  Registry of handlers sorted by priority; setjmp/longjmp for non-local returns
 *
 * @author NIMCP Development Team
 */

#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"

#include "utils/platform/nimcp_platform_once.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(exception_handlers)

/* ============================================================================
 * Module State
 * ============================================================================ */

/** Registered handlers (sorted by priority, highest first) */
static nimcp_handler_registration_t* g_handlers[NIMCP_HANDLER_MAX_REGISTERED];
static size_t g_handler_count = 0;
static uint32_t g_next_handler_id = 1;
static nimcp_platform_mutex_t  g_handler_mutex_storage;
static nimcp_platform_mutex_t* g_handler_mutex = NULL;
static bool g_handlers_initialized = false;

/** Recovery callbacks */
static struct {
    nimcp_recovery_callback_fn callback;
    void* user_data;
} g_recovery_callbacks[12];  /* One per RECOVERY_ACTION_* */

/** Thread-local try stack */
static _Thread_local nimcp_try_context_t* tl_try_stack[NIMCP_TRY_STACK_DEPTH];
static _Thread_local size_t tl_try_depth = 0;

/* P2-U25: Thread-safe one-time initialization via platform_once */
static nimcp_platform_once_t g_handler_init_once = NIMCP_PLATFORM_ONCE_INIT;

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Internal init callback for platform_once
 */
static void ensure_initialized_internal(void) {
    /* Use a statically-allocated mutex to avoid tracked nimcp_malloc.
     * This prevents a permanent ~40-byte leak in the memory tracker
     * since the handler mutex is never freed during normal operation. */
    nimcp_platform_mutex_init(&g_handler_mutex_storage, false);
    g_handler_mutex = &g_handler_mutex_storage;
    g_handlers_initialized = true;
}

static void ensure_initialized(void) {
    if (g_handlers_initialized) return;
    /* P2-U25: Use platform_once to prevent race condition where two threads
     * could both see g_handlers_initialized==false and double-init the mutex */
    nimcp_platform_once(&g_handler_init_once, ensure_initialized_internal);
}

/**
 * @brief Compare handlers by priority (for sorting)
 */
static int compare_handlers(const void* a, const void* b) {
    const nimcp_handler_registration_t* ha = *(const nimcp_handler_registration_t**)a;
    const nimcp_handler_registration_t* hb = *(const nimcp_handler_registration_t**)b;
    /* Higher priority first */
    if (hb->options.priority > ha->options.priority) return 1;
    if (hb->options.priority < ha->options.priority) return -1;
    return 0;
}

/**
 * @brief Sort handlers by priority
 */
static void sort_handlers(void) {
    if (g_handler_count > 1) {
        qsort(g_handlers, g_handler_count, sizeof(nimcp_handler_registration_t*), compare_handlers);
    }
}

/**
 * @brief Check if handler should process exception
 */
static bool handler_matches(const nimcp_handler_registration_t* reg, const nimcp_exception_t* ex) {
    if (!reg || !reg->active) return false;
    if (!ex) return false;

    /* Check category filter */
    if (reg->options.category_filter != 0 &&
        reg->options.category_filter != ex->category) {
        return false;
    }

    /* Check severity filter */
    if (ex->severity < reg->options.min_severity) {
        return false;
    }

    /* Check type filter */
    if (reg->options.type_filter != 0 &&
        reg->options.type_filter != ex->type) {
        return false;
    }

    return true;
}

/* ============================================================================
 * Handler Registration
 * ============================================================================ */

void nimcp_handler_default_options(nimcp_handler_options_t* options) {
    if (!options) return;

    memset(options, 0, sizeof(nimcp_handler_options_t));
    options->name = "anonymous";
    options->priority = NIMCP_HANDLER_PRIORITY_NORMAL;
    options->min_severity = EXCEPTION_SEVERITY_DEBUG;
}

nimcp_handler_registration_t* nimcp_handler_register(
    const nimcp_handler_options_t* options
) {
    ensure_initialized();

    if (!options || !options->handler) return NULL;

    nimcp_handler_registration_t* reg = nimcp_calloc(1, sizeof(nimcp_handler_registration_t));
    if (!reg) {
        return NULL;
    }

    /* P2-U27: Atomic ID generation to prevent duplicates under concurrency */
    reg->id = __atomic_fetch_add(&g_next_handler_id, 1, __ATOMIC_RELAXED);
    reg->options = *options;
    reg->active = true;

    if (g_handler_mutex) nimcp_platform_mutex_lock(g_handler_mutex);

    /* P2-U26: Check capacity inside mutex to prevent TOCTOU race where two
     * threads both pass the check and exceed NIMCP_HANDLER_MAX_REGISTERED */
    if (g_handler_count >= NIMCP_HANDLER_MAX_REGISTERED) {
        if (g_handler_mutex) nimcp_platform_mutex_unlock(g_handler_mutex);
        nimcp_free(reg);
        return NULL;
    }

    g_handlers[g_handler_count++] = reg;
    sort_handlers();

    if (g_handler_mutex) nimcp_platform_mutex_unlock(g_handler_mutex);

    LOG_DEBUG("Registered exception handler '%s' (id=%u, priority=%d)",
              options->name ? options->name : "anonymous",
              reg->id,
              options->priority);

    return reg;
}

int nimcp_handler_unregister(nimcp_handler_registration_t* registration) {
    if (!registration) return -1;

    if (g_handler_mutex) nimcp_platform_mutex_lock(g_handler_mutex);

    /* Find and remove */
    for (size_t i = 0; i < g_handler_count; i++) {
        if (g_handlers[i] == registration) {
            /* Shift remaining handlers */
            for (size_t j = i; j < g_handler_count - 1; j++) {
                g_handlers[j] = g_handlers[j + 1];
            }
            g_handler_count--;
            break;
        }
    }

    if (g_handler_mutex) nimcp_platform_mutex_unlock(g_handler_mutex);

    nimcp_free(registration);
    return 0;
}

void nimcp_handler_disable(nimcp_handler_registration_t* registration) {
    if (registration) {
        registration->active = false;
    }
}

void nimcp_handler_enable(nimcp_handler_registration_t* registration) {
    if (registration) {
        registration->active = true;
    }
}

/* ============================================================================
 * Exception Dispatch
 * ============================================================================ */

bool nimcp_exception_dispatch(nimcp_exception_t* ex) {
    if (!ex) return false;

    ensure_initialized();

    /* Set as current exception */
    nimcp_exception_set_current(ex);

    bool handled = false;

    if (g_handler_mutex) nimcp_platform_mutex_lock(g_handler_mutex);

    /* Dispatch to handlers in priority order */
    for (size_t i = 0; i < g_handler_count && !handled; i++) {
        nimcp_handler_registration_t* reg = g_handlers[i];

        if (handler_matches(reg, ex)) {
            /* Call handler */
            handled = reg->options.handler(ex, reg->options.user_data);
        }
    }

    if (g_handler_mutex) nimcp_platform_mutex_unlock(g_handler_mutex);

    /* If not handled, use default behavior (log) */
    if (!handled) {
        nimcp_exception_log(ex);
    }

    /* Clear current exception after dispatch to avoid holding a tracked reference.
     * Handlers had access to it during dispatch; keeping it afterward leaks memory
     * because nimcp_exception_set_current takes a ref via nimcp_exception_ref. */
    nimcp_exception_clear_current();

    return handled;
}

void nimcp_exception_raise(nimcp_exception_t* ex) {
    if (!ex) return;

    /* Check if inside try block */
    if (tl_try_depth > 0) {
        nimcp_try_context_t* ctx = tl_try_stack[tl_try_depth - 1];
        if (ctx) {
            ctx->exception = nimcp_exception_ref(ex);
            ctx->exception_caught = true;

            /* longjmp back to try block */
            longjmp(ctx->jmp_buffer, 1);
        }
    }

    /* Not in try block - dispatch to handlers */
    nimcp_exception_dispatch(ex);
}

void nimcp_exception_throw(
    nimcp_error_t code,
    const char* file,
    int line,
    const char* func,
    const char* format,
    ...
) {
    char message[NIMCP_EXCEPTION_MAX_MESSAGE];

    if (format) {
        va_list args;
        va_start(args, format);
        vsnprintf(message, sizeof(message), format, args);
        va_end(args);
    } else {
        message[0] = '\0';
    }

    nimcp_exception_t* ex = nimcp_exception_create(
        code,
        nimcp_exception_get_severity_from_code(code),
        file,
        line,
        func,
        "%s",
        message
    );

    if (ex) {
        nimcp_exception_raise(ex);
        nimcp_exception_unref(ex);
    }
}

/* ============================================================================
 * Try/Catch Stack
 * ============================================================================ */

int nimcp_try_push(nimcp_try_context_t* ctx) {
    if (!ctx) return -1;
    if (tl_try_depth >= NIMCP_TRY_STACK_DEPTH) return -1;

    tl_try_stack[tl_try_depth++] = ctx;
    return 0;
}

nimcp_try_context_t* nimcp_try_pop(void) {
    if (tl_try_depth == 0) return NULL;
    return tl_try_stack[--tl_try_depth];
}

nimcp_try_context_t* nimcp_try_current(void) {
    if (tl_try_depth == 0) return NULL;
    return tl_try_stack[tl_try_depth - 1];
}

bool nimcp_in_try_block(void) {
    return tl_try_depth > 0;
}

/* ============================================================================
 * Recovery Registration
 * ============================================================================ */

int nimcp_register_recovery_callback(
    nimcp_exception_recovery_action_t action,
    nimcp_recovery_callback_fn callback,
    void* user_data
) {
    if (action <= EXCEPTION_RECOVERY_NONE || action >= 12) return -1;

    g_recovery_callbacks[action].callback = callback;
    g_recovery_callbacks[action].user_data = user_data;
    return 0;
}

int nimcp_unregister_recovery_callback(nimcp_exception_recovery_action_t action) {
    if (action <= EXCEPTION_RECOVERY_NONE || action >= 12) return -1;

    g_recovery_callbacks[action].callback = NULL;
    g_recovery_callbacks[action].user_data = NULL;
    return 0;
}

int nimcp_execute_recovery(
    nimcp_exception_t* ex,
    nimcp_exception_recovery_action_t action
) {
    if (!ex) return -1;
    if (action <= EXCEPTION_RECOVERY_NONE || action >= 12) return -1;

    nimcp_recovery_callback_fn callback = g_recovery_callbacks[action].callback;
    void* user_data = g_recovery_callbacks[action].user_data;

    if (!callback) {
        LOG_WARNING("No recovery callback registered for action %s",
                    nimcp_exception_recovery_action_to_string(action));
        return -1;
    }

    LOG_INFO("Executing recovery action %s for exception code %d",
             nimcp_exception_recovery_action_to_string(action), ex->code);

    ex->recovery_attempted = true;
    int result = callback(ex, action, user_data);
    ex->recovery_succeeded = (result == 0);

    return result;
}

/* ============================================================================
 * Default Handlers
 * ============================================================================ */

bool nimcp_default_logging_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;

    if (!ex) return false;

    /* Log the exception */
    nimcp_exception_log(ex);

    /* Log stack trace for severe and above */
    if (ex->severity >= EXCEPTION_SEVERITY_SEVERE && ex->stack_trace.depth > 0) {
        char trace_buf[NIMCP_LOG_BUFFER_SIZE];
        nimcp_stack_trace_to_string(&ex->stack_trace, trace_buf, sizeof(trace_buf));
        LOG_ERROR("%s", trace_buf);
    }

    /* Don't consume - let other handlers process */
    return false;
}

bool nimcp_default_immune_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;

    if (!ex) return false;

    /* Only present to immune if severe enough */
    if (ex->severity >= EXCEPTION_SEVERITY_SEVERE && !ex->presented_to_immune) {
        /* Check if immune system is connected */
        if (nimcp_exception_immune_is_connected()) {
            nimcp_exception_present_to_immune(ex, NULL);
        }
    }

    /* Don't consume - let recovery handlers process */
    return false;
}

static nimcp_handler_registration_t* g_default_logging_reg = NULL;
static nimcp_handler_registration_t* g_default_immune_reg = NULL;

int nimcp_install_default_handlers(void) {
    ensure_initialized();

    /* Install logging handler (low priority - runs last) */
    if (!g_default_logging_reg) {
        nimcp_handler_options_t log_opts;
        nimcp_handler_default_options(&log_opts);
        log_opts.name = "default_logging";
        log_opts.handler = nimcp_default_logging_handler;
        log_opts.priority = NIMCP_HANDLER_PRIORITY_LOW;
        log_opts.min_severity = EXCEPTION_SEVERITY_DEBUG;

        g_default_logging_reg = nimcp_handler_register(&log_opts);
    }

    /* Install immune handler (normal priority) */
    if (!g_default_immune_reg) {
        nimcp_handler_options_t immune_opts;
        nimcp_handler_default_options(&immune_opts);
        immune_opts.name = "default_immune";
        immune_opts.handler = nimcp_default_immune_handler;
        immune_opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
        immune_opts.min_severity = EXCEPTION_SEVERITY_SEVERE;

        g_default_immune_reg = nimcp_handler_register(&immune_opts);
    }

    return 0;
}

/* ============================================================================
 * Handler Chain Query
 * ============================================================================ */

size_t nimcp_handler_count(void) {
    return g_handler_count;
}

const nimcp_handler_registration_t* nimcp_handler_get(size_t index) {
    if (index >= g_handler_count) return NULL;
    return g_handlers[index];
}

/* ============================================================================
 * System Shutdown
 * ============================================================================ */

void nimcp_exception_handlers_shutdown(void) {
    if (!g_handlers_initialized) return;

    /* Free all registered handlers */
    for (size_t i = 0; i < g_handler_count; i++) {
        if (g_handlers[i]) {
            nimcp_free(g_handlers[i]);
            g_handlers[i] = NULL;
        }
    }
    g_handler_count = 0;

    /* Clear default handler references */
    g_default_logging_reg = NULL;
    g_default_immune_reg = NULL;

    /* Destroy handler mutex (statically allocated, no free needed) */
    if (g_handler_mutex) {
        nimcp_platform_mutex_destroy(g_handler_mutex);
        g_handler_mutex = NULL;
    }

    g_handlers_initialized = false;

    /* P2-U25: Reset platform_once so handlers can be re-initialized after shutdown */
    g_handler_init_once = (nimcp_platform_once_t)NIMCP_PLATFORM_ONCE_INIT;
}
