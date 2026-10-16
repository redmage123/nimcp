/**
 * @file nimcp_kg_wiring_exception.c
 * @brief Exception handling integration for KG module wiring system
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Implementation of KG wiring exception handling
 * WHY:  Integrate KG wiring failures with NIMCP exception and immune system
 * HOW:  Provides exception creation, recovery mapping, and handler registration
 *
 * @author NIMCP Development Team
 */

#include "core/brain/nimcp_kg_wiring_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(kg_wiring_exception, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Module State
 * ============================================================================ */

/** Global flag for enabling/disabling exceptions */
static bool g_kg_wiring_exceptions_enabled = true;

/** System initialization flag */
static bool g_kg_wiring_exception_initialized = false;

/** Default handler registration */
static nimcp_handler_registration_t* g_default_handler_reg = NULL;

/** Custom handler storage */
#define KG_WIRING_MAX_HANDLERS 8

typedef struct {
    kg_wiring_exception_handler_fn handler;
    void* user_data;
    uint32_t id;
    bool active;
} kg_wiring_handler_entry_t;

static kg_wiring_handler_entry_t g_handlers[KG_WIRING_MAX_HANDLERS];
static uint32_t g_next_handler_id = 1;

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
 * @brief Safely copy string with truncation
 */
static void safe_strncpy(char* dest, const char* src, size_t max_len) {
    if (!dest || max_len == 0) return;
    if (!src) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, max_len - 1);
    dest[max_len - 1] = '\0';
}

/* ============================================================================
 * Enable/Disable API
 * ============================================================================ */

void kg_wiring_enable_exceptions(void) {
    g_kg_wiring_exceptions_enabled = true;
}

void kg_wiring_disable_exceptions(void) {
    g_kg_wiring_exceptions_enabled = false;
}

bool kg_wiring_exceptions_enabled(void) {
    return g_kg_wiring_exceptions_enabled;
}

/* ============================================================================
 * Recovery Action Mapping
 * ============================================================================ */

nimcp_exception_recovery_action_t kg_wiring_get_recovery_action(nimcp_error_t code) {
    switch (code) {
        /* Memory-related errors -> trigger GC */
        case NIMCP_ERROR_KG_WIRING_CREATE:
        case NIMCP_ERROR_KG_WIRING_WEIGHT_ALLOC:
            return EXCEPTION_RECOVERY_GC;

        /* Capacity errors -> reduce load */
        case NIMCP_ERROR_KG_WIRING_INPUTS_FULL:
        case NIMCP_ERROR_KG_WIRING_OUTPUTS_FULL:
        case NIMCP_ERROR_KG_WIRING_HANDLERS_FULL:
        case NIMCP_ERROR_KG_WIRING_METADATA_FULL:
            return EXCEPTION_RECOVERY_REDUCE_LOAD;

        /* Validation errors -> rollback */
        case NIMCP_ERROR_KG_WIRING_VALIDATION:
            return EXCEPTION_RECOVERY_ROLLBACK;

        /* Programmer errors -> no recovery, fix the code */
        case NIMCP_ERROR_KG_WIRING_NULL:
        case NIMCP_ERROR_KG_WIRING_STRING_TOO_LONG:
        case NIMCP_ERROR_KG_WIRING_INVALID_NAME:
        case NIMCP_ERROR_KG_WIRING_INVALID_TYPE:
        case NIMCP_ERROR_KG_WIRING_WEIGHT_INVALID:
        case NIMCP_ERROR_KG_WIRING_DUPLICATE:
        default:
            return EXCEPTION_RECOVERY_NONE;
    }
}

/* ============================================================================
 * Exception Creation
 * ============================================================================ */

nimcp_kg_wiring_exception_t* nimcp_kg_wiring_exception_create(
    nimcp_error_t code,
    nimcp_exception_severity_t severity,
    const char* file,
    int line,
    const char* func,
    const char* module_name,
    const char* operation,
    const char* format,
    ...
) {
    /* Lazy initialization */
    if (!g_kg_wiring_exception_initialized) {
        kg_wiring_exception_init();
    }

    nimcp_kg_wiring_exception_t* ex = nimcp_calloc(1, sizeof(nimcp_kg_wiring_exception_t));
    if (!ex) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ex is NULL");

        return NULL;

    }

    /* Initialize brain exception base */
    ex->base.base.type = EXCEPTION_TYPE_BRAIN;
    ex->base.base.category = EXCEPTION_CATEGORY_BRAIN;
    ex->base.base.code = code;
    ex->base.base.severity = severity;
    ex->base.base.file = file;
    ex->base.base.line = line;
    ex->base.base.function = func;
    ex->base.base.timestamp_us = get_timestamp_us();
    ex->base.base.ref_count = 1;
    ex->base.base.suggested_action = kg_wiring_get_recovery_action(code);

    /* Set brain-specific fields */
    ex->base.brain_id = 0;  /* Not associated with specific brain */
    ex->base.region_name = "KG_MODULE_WIRING";

    /* Set KG wiring-specific fields */
    safe_strncpy(ex->module_name, module_name ? module_name : "unknown",
                 KG_WIRING_EXCEPTION_MAX_MODULE_NAME);
    safe_strncpy(ex->operation, operation ? operation : "unknown",
                 KG_WIRING_EXCEPTION_MAX_OPERATION);

    /* Format message */
    if (format) {
        va_list args;
        va_start(args, format);
        vsnprintf(ex->base.base.message, NIMCP_EXCEPTION_MAX_MESSAGE, format, args);
        va_end(args);
    }

    /* Capture stack trace and generate epitope */
    nimcp_exception_capture_stack_trace(&ex->base.base.stack_trace, 2);
    nimcp_exception_generate_epitope(&ex->base.base);

    /* Add context entries for better debugging */
    nimcp_exception_set_context(&ex->base.base, "module", ex->module_name);
    nimcp_exception_set_context(&ex->base.base, "operation", ex->operation);

    return ex;
}

nimcp_kg_wiring_exception_t* nimcp_kg_wiring_exception_create_capacity(
    nimcp_error_t code,
    const char* file,
    int line,
    const char* func,
    const char* module_name,
    const char* operation,
    uint32_t current_count,
    uint32_t max_count
) {
    nimcp_kg_wiring_exception_t* ex = nimcp_kg_wiring_exception_create(
        code,
        EXCEPTION_SEVERITY_ERROR,
        file,
        line,
        func,
        module_name,
        operation,
        "Capacity limit reached: %u/%u",
        current_count,
        max_count
    );

    if (ex) {
        ex->current_count = current_count;
        ex->max_count = max_count;

        /* Add context for capacity info */
        char count_str[32];
        snprintf(count_str, sizeof(count_str), "%u", current_count);
        nimcp_exception_set_context(&ex->base.base, "current_count", count_str);
        snprintf(count_str, sizeof(count_str), "%u", max_count);
        nimcp_exception_set_context(&ex->base.base, "max_count", count_str);
    }

    return ex;
}

nimcp_kg_wiring_exception_t* nimcp_kg_wiring_exception_create_string(
    nimcp_error_t code,
    const char* file,
    int line,
    const char* func,
    const char* module_name,
    const char* operation,
    size_t string_length,
    size_t max_length
) {
    nimcp_kg_wiring_exception_t* ex = nimcp_kg_wiring_exception_create(
        code,
        EXCEPTION_SEVERITY_WARNING,
        file,
        line,
        func,
        module_name,
        operation,
        "String too long: %zu/%zu bytes",
        string_length,
        max_length
    );

    if (ex) {
        ex->string_length = string_length;
        ex->max_string_length = max_length;

        /* Add context for string info */
        char len_str[32];
        snprintf(len_str, sizeof(len_str), "%zu", string_length);
        nimcp_exception_set_context(&ex->base.base, "string_length", len_str);
        snprintf(len_str, sizeof(len_str), "%zu", max_length);
        nimcp_exception_set_context(&ex->base.base, "max_length", len_str);
    }

    return ex;
}

/* ============================================================================
 * Default Handler
 * ============================================================================ */

bool kg_wiring_default_exception_handler(
    nimcp_exception_t* ex,
    void* user_data
) {
    (void)user_data;

    if (!ex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_wiring_default_exception_handler: ex is NULL");
        return false;
    }

    /* Only handle KG wiring errors */
    if (ex->code < NIMCP_ERROR_KG_WIRING_BASE ||
        ex->code > NIMCP_ERROR_KG_WIRING_DUPLICATE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_wiring_default_exception_handler: ex is NULL");
        return false;  /* Not a KG wiring exception */
    }

    /* Log the exception */
    LOG_ERROR("[KG_WIRING] %s (code=%d, severity=%d)",
              ex->message, ex->code, ex->severity);

    /* Log context if available */
    const char* module = nimcp_exception_get_context(ex, "module");
    const char* operation = nimcp_exception_get_context(ex, "operation");
    if (module && operation) {
        LOG_ERROR("[KG_WIRING]   Module: %s, Operation: %s", module, operation);
    }

    /* Suggest recovery action */
    nimcp_exception_recovery_action_t action = kg_wiring_get_recovery_action(ex->code);
    if (action != EXCEPTION_RECOVERY_NONE) {
        LOG_INFO("[KG_WIRING]   Suggested recovery: %s",
                 nimcp_exception_recovery_action_to_string(action));
    }

    /* Don't mark as handled - let other handlers process too */
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_wiring_default_exception_handler: validation failed");
    return false;
}

/**
 * @brief Internal dispatcher for custom handlers
 */
static bool kg_wiring_dispatch_to_custom_handlers(
    nimcp_exception_t* ex,
    void* user_data
) {
    (void)user_data;

    /* Dispatch to all registered custom handlers */
    for (int i = 0; i < KG_WIRING_MAX_HANDLERS; i++) {
        if (g_handlers[i].active && g_handlers[i].handler) {
            if (g_handlers[i].handler(ex, g_handlers[i].user_data)) {
                return true;  /* Handler marked as handled */
            }
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_wiring_dispatch_to_custom_handlers: validation failed");
    return false;
}

/* ============================================================================
 * Handler Registration
 * ============================================================================ */

uint32_t kg_wiring_register_exception_handler(
    kg_wiring_exception_handler_fn handler,
    void* user_data
) {
    if (!handler) return 0;

    /* Find empty slot */
    for (int i = 0; i < KG_WIRING_MAX_HANDLERS; i++) {
        if (!g_handlers[i].active) {
            g_handlers[i].handler = handler;
            g_handlers[i].user_data = user_data;
            g_handlers[i].id = g_next_handler_id++;
            g_handlers[i].active = true;
            return g_handlers[i].id;
        }
    }

    LOG_ERROR("[KG_WIRING] Cannot register handler: max handlers reached");
    return 0;
}

int kg_wiring_unregister_exception_handler(uint32_t handler_id) {
    if (handler_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_wiring_unregister_exception_handler: handler_id is zero");
        return -1;
    }

    for (int i = 0; i < KG_WIRING_MAX_HANDLERS; i++) {
        if (g_handlers[i].active && g_handlers[i].id == handler_id) {
            g_handlers[i].active = false;
            g_handlers[i].handler = NULL;
            g_handlers[i].user_data = NULL;
            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_wiring_unregister_exception_handler: validation failed");
    return -1;  /* Not found */
}

/* ============================================================================
 * Initialization
 * ============================================================================ */

int kg_wiring_exception_init(void) {
    if (g_kg_wiring_exception_initialized) {
        return 0;  /* Already initialized */
    }

    /* Clear handler storage */
    memset(g_handlers, 0, sizeof(g_handlers));

    /* Register default handler with exception system */
    nimcp_handler_options_t opts = {
        .name = "kg_wiring_default",
        .handler = kg_wiring_default_exception_handler,
        .user_data = NULL,
        .priority = NIMCP_HANDLER_PRIORITY_NORMAL,
        .category_filter = EXCEPTION_CATEGORY_BRAIN,
        .min_severity = EXCEPTION_SEVERITY_WARNING,
        .type_filter = EXCEPTION_TYPE_BRAIN
    };

    g_default_handler_reg = nimcp_handler_register(&opts);

    /* Register custom handler dispatcher */
    nimcp_handler_options_t custom_opts = {
        .name = "kg_wiring_custom_dispatcher",
        .handler = kg_wiring_dispatch_to_custom_handlers,
        .user_data = NULL,
        .priority = NIMCP_HANDLER_PRIORITY_NORMAL + 10,  /* Before default */
        .category_filter = EXCEPTION_CATEGORY_BRAIN,
        .min_severity = EXCEPTION_SEVERITY_WARNING,
        .type_filter = EXCEPTION_TYPE_BRAIN
    };

    nimcp_handler_register(&custom_opts);

    g_kg_wiring_exception_initialized = true;
    LOG_DEBUG("[KG_WIRING] Exception system initialized");

    return 0;
}

void kg_wiring_exception_shutdown(void) {
    if (!g_kg_wiring_exception_initialized) {
        return;
    }

    /* Unregister default handler */
    if (g_default_handler_reg) {
        nimcp_handler_unregister(g_default_handler_reg);
        g_default_handler_reg = NULL;
    }

    /* Clear all custom handlers */
    memset(g_handlers, 0, sizeof(g_handlers));

    g_kg_wiring_exception_initialized = false;
    LOG_DEBUG("[KG_WIRING] Exception system shut down");
}
