/**
 * @file nimcp_unification_engine.c
 * @brief MODULE 5: Unification Engine implementation
 *
 * @author NIMCP Development Team - SRP Refactoring
 * @date 2025-11-20
 * @version 3.0.0
 */

#include "cognitive/reasoning/nimcp_unification_engine.h"
#include "constants/nimcp_buffer_constants.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/reasoning/nimcp_symbolic_logic_attachment.h"
#include "cognitive/nimcp_symbolic_logic.h"
#include "core/brain/nimcp_brain_internal.h"       /* W7: KG access */
#include "core/brain/nimcp_brain_kg.h"             /* W7: KG event emission */
#include "utils/validation/nimcp_validate.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "api/nimcp_api_exception.h"
#include "core/events/nimcp_event_bus.h"

// Bio-async integration
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "nimcp.h"  // For error codes
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define LOG_MODULE "reasoning"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(unification_engine, MESH_ADAPTER_CATEGORY_COGNITIVE)


static _Thread_local char last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

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

/* ============================================================================
 * W7 KG integration helpers
 * ============================================================================ */

static void unif_kg_ensure_root(brain_t brain)
{
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) return;
    uint64_t tok = brain->internal_kg_admin_token;
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_ADMIN, tok);
    if (brain_kg_find_node(brain->internal_kg, "cog_reasoning_unification")
        == BRAIN_KG_INVALID_NODE) {
        brain_kg_add_node(brain->internal_kg, "cog_reasoning_unification",
                          BRAIN_KG_NODE_COGNITIVE,
                          "First-order logic unification engine");
    }
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_READ, 0);
}

static void unif_kg_emit_binding(brain_t brain, bool success,
                                 uint32_t num_bindings)
{
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) return;
    uint64_t tok = brain->internal_kg_admin_token;
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_ADMIN, tok);

    char node_name[BRAIN_KG_MAX_NAME_LEN];
    snprintf(node_name, sizeof(node_name),
             "cog_reasoning_unification_event_%s_%llu",
             success ? "ok" : "fail",
             (unsigned long long)nimcp_time_monotonic_us());
    char desc[128];
    snprintf(desc, sizeof(desc),
             "unify %s; %u bindings",
             success ? "success" : "failure", (unsigned)num_bindings);
    brain_kg_node_id_t nid = brain_kg_add_node(brain->internal_kg, node_name,
                                               BRAIN_KG_NODE_COGNITIVE, desc);
    if (nid != BRAIN_KG_INVALID_NODE) {
        brain_kg_node_id_t root = brain_kg_find_node(brain->internal_kg,
            "cog_reasoning_unification");
        if (root != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(brain->internal_kg, root, nid,
                              BRAIN_KG_EDGE_PROVIDES_TO,
                              success ? "bound" : "unbound",
                              success ? 0.7f : 0.2f);
        }
    }
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_READ, 0);
}

bool brain_unify_terms(
    brain_t brain,
    const logical_term_t* term1,
    const logical_term_t* term2,
    unification_t* result)
{
    if (!nimcp_validate_pointer(brain, "brain") || !brain_has_symbolic_logic(brain)) {
        set_error("Brain is NULL or no logic engine attached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_unify_terms: required parameter is NULL (nimcp_validate_pointer, brain_has_symbolic_logic)");
        return false;
    }

    if (!nimcp_validate_pointer(term1, "term1") || !nimcp_validate_pointer(term2, "term2") ||
        !nimcp_validate_pointer(result, "result")) {
        set_error("NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_unify_terms: required parameter is NULL (nimcp_validate_pointer, nimcp_validate_pointer)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    unification_engine_heartbeat("unification_brain_unify_terms", 0.0f);

    // Delegate to symbolic logic unification
    // Note: symbolic_logic_unify returns unification_t* rather than taking output param
    (void)brain; // Brain reference not needed for current API
    unification_t* unif_result = symbolic_logic_unify(
        (logical_term_t*)term1,
        (logical_term_t*)term2
    );

    bool success = false;
    if (unif_result) {
        // Copy result
        result->success = unif_result->success;
        result->bindings = unif_result->bindings;
        result->num_bindings = unif_result->num_bindings;
        success = unif_result->success;

        // Don't destroy - caller owns the bindings now
        nimcp_free(unif_result);
        unif_result = NULL;
    } else {
        result->success = false;
        result->bindings = NULL;
        result->num_bindings = 0;
    }

    /* W7: emit binding event to KG. */
    unif_kg_ensure_root(brain);
    unif_kg_emit_binding(brain, success, result->num_bindings);

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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_apply_substitution: required parameter is NULL (nimcp_validate_pointer, brain_has_symbolic_logic)");
        return false;
    }

    if (!nimcp_validate_pointer(term, "term") || !nimcp_validate_pointer(bindings, "bindings") ||
        !nimcp_validate_pointer(result, "result")) {
        set_error("NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_apply_substitution: required parameter is NULL (nimcp_validate_pointer, nimcp_validate_pointer)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    unification_engine_heartbeat("unification_brain_apply_substitution", 0.0f);

    // Delegate to symbolic logic substitution
    // Note: symbolic_logic_substitute takes (term, subst) and returns new term
    (void)brain; // Brain reference not needed for current API

    // Apply each binding in sequence
    logical_term_t* current = logic_term_create(term->type, term->name);
    if (!current) {
        set_error("Failed to copy term");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_apply_substitution: current is NULL");
        return false;
    }

    for (uint32_t i = 0; i < bindings->num_bindings; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bindings->num_bindings > 256) {
            unification_engine_heartbeat("unification__loop",
                             (float)(i + 1) / (float)bindings->num_bindings);
        }

        if (bindings->bindings[i]) {
            logical_term_t* substituted = symbolic_logic_substitute(
                current,
                bindings->bindings[i]
            );
            if (substituted) {
                logic_term_destroy(current);
                current = substituted;
            }
        }
    }

    *result = current;
    return true;
}

void unification_free_result(unification_t* result)
{
    if (!result) return;
    // Free implementation
    /* Phase 8: Heartbeat at operation start */
    unification_engine_heartbeat("unification__unification_free_res", 0.0f);


    memset(result, 0, sizeof(unification_t));
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

int unification_engine_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    unification_engine_heartbeat("unification__query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Unification_Engine");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                unification_engine_heartbeat("unification__loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Unification_Engine self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Unification_Engine");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Unification_Engine");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void unification_engine_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_unification_engine_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int unification_engine_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "unification_engine_training_begin: NULL argument");
        return -1;
    }
    unification_engine_heartbeat_instance(NULL, "unification_engine_training_begin", 0.0f);
    return 0;
}

int unification_engine_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "unification_engine_training_end: NULL argument");
        return -1;
    }
    unification_engine_heartbeat_instance(NULL, "unification_engine_training_end", 1.0f);
    return 0;
}

int unification_engine_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "unification_engine_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    unification_engine_heartbeat_instance(NULL, "unification_engine_training_step", progress);
    return 0;
}
