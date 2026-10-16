/**
 * @file nimcp_unification_engine.c
 * @brief MODULE 5: Unification Engine implementation
 *
 * @author NIMCP Development Team - SRP Refactoring
 * @date 2025-11-20
 * @version 3.0.0
 */

#include "cognitive/reasoning/nimcp_unification_engine.h"
#include "cognitive/reasoning/nimcp_symbolic_logic_attachment.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/logging/nimcp_logging.h"
#include "core/events/nimcp_event_bus.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

static __thread char last_error[256] = {0};

static void set_error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(last_error, sizeof(last_error), fmt, args);
    va_end(args);
}

const char* unification_get_last_error(void)
{
    return last_error;
}

bool brain_unify_terms(
    brain_t brain,
    const logical_term_t* term1,
    const logical_term_t* term2,
    unification_t* result)
{
    if (!nimcp_validate_pointer(brain, "brain") || !brain_has_symbolic_logic(brain)) {
        set_error("Brain is NULL or no logic engine attached");
        return false;
    }

    if (!nimcp_validate_pointer(term1, "term1") || !nimcp_validate_pointer(term2, "term2") ||
        !nimcp_validate_pointer(result, "result")) {
        set_error("NULL parameter");
        return false;
    }

    // Delegate to symbolic logic unification
    bool success = symbolic_logic_unify(brain_get_symbolic_logic(brain), term1, term2, result);

    if (brain->event_bus) {
        event_data_t event = {
            .event_type = success ? EVENT_UNIFICATION_SUCCEEDED : EVENT_UNIFICATION_FAILED,
            .timestamp = 0,
            .priority = EVENT_PRIORITY_LOW,
            .data_size = 0,
            .data = NULL
        };
        event_bus_publish(brain->event_bus, &event);
    }

    return success;
}

bool brain_apply_substitution(
    brain_t brain,
    const logical_term_t* term,
    const unification_t* bindings,
    logical_term_t** result)
{
    if (!nimcp_validate_pointer(brain, "brain") || !brain_has_symbolic_logic(brain)) {
        set_error("Brain is NULL or no logic engine attached");
        return false;
    }

    if (!nimcp_validate_pointer(term, "term") || !nimcp_validate_pointer(bindings, "bindings") ||
        !nimcp_validate_pointer(result, "result")) {
        set_error("NULL parameter");
        return false;
    }

    // Delegate to symbolic logic substitution
    return symbolic_logic_substitute(brain_get_symbolic_logic(brain), term, bindings, result);
}

void unification_free_result(unification_t* result)
{
    if (!result) return;
    // Free implementation
    memset(result, 0, sizeof(unification_t));
}
