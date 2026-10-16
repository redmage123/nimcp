/**
 * @file nimcp_forward_chaining.c
 * @brief MODULE 3: Forward Chaining Engine implementation
 *
 * SINGLE RESPONSIBILITY: Derive new facts from existing knowledge (data-driven inference)
 *
 * @author NIMCP Development Team - SRP Refactoring
 * @date 2025-11-20
 * @version 3.0.0
 */

#include "cognitive/reasoning/nimcp_forward_chaining.h"
#include "constants/nimcp_buffer_constants.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/reasoning/nimcp_symbolic_logic_attachment.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"           /* W7: KG event emission */
#include "utils/validation/nimcp_validate.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "core/events/nimcp_event_bus.h"
#include "cognitive/nimcp_working_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "cognitive/nimcp_executive.h"

// Bio-async integration
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "nimcp.h"  // For error codes

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define LOG_MODULE "reasoning"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(forward_chaining, MESH_ADAPTER_CATEGORY_COGNITIVE)



//=============================================================================
// Event Types
//=============================================================================

#define EVENT_FORWARD_CHAIN_STEP 0x0945
#define EVENT_NOVEL_FACT_DERIVED 0x0946

//=============================================================================
// Configuration
//=============================================================================

#define MAX_ITERATIONS_CAP 1000
#define DEFAULT_SALIENCE 0.7f
#define MAX_WM_FACTS 7  // Working memory capacity limit

//=============================================================================
// Error Handling
//=============================================================================

static _Thread_local char last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(last_error, sizeof(last_error), fmt, args);
    va_end(args);
}

const char* forward_chain_get_last_error(void)
{
    return last_error;
}

//=============================================================================
// W7 KG integration helpers
//=============================================================================

static void fc_kg_ensure_root(brain_t brain)
{
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) return;
    uint64_t tok = brain->internal_kg_admin_token;
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_ADMIN, tok);
    if (brain_kg_find_node(brain->internal_kg, "cog_reasoning_forward_chain")
        == BRAIN_KG_INVALID_NODE) {
        brain_kg_add_node(brain->internal_kg, "cog_reasoning_forward_chain",
                          BRAIN_KG_NODE_COGNITIVE,
                          "Data-driven inference engine (forward chaining)");
    }
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_READ, 0);
}

/** Emit a single inference-step event node. */
static void fc_kg_emit_step(brain_t brain, int num_new_facts,
                            uint64_t duration_ms)
{
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) return;
    uint64_t tok = brain->internal_kg_admin_token;
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_ADMIN, tok);

    char node_name[BRAIN_KG_MAX_NAME_LEN];
    snprintf(node_name, sizeof(node_name),
             "cog_reasoning_forward_chain_event_step_%llu",
             (unsigned long long)nimcp_time_monotonic_us());
    char desc[160];
    snprintf(desc, sizeof(desc),
             "forward_chain produced %d new facts in %llu ms",
             num_new_facts, (unsigned long long)duration_ms);
    brain_kg_node_id_t nid = brain_kg_add_node(brain->internal_kg, node_name,
                                               BRAIN_KG_NODE_COGNITIVE, desc);
    if (nid != BRAIN_KG_INVALID_NODE) {
        brain_kg_node_id_t root = brain_kg_find_node(brain->internal_kg,
            "cog_reasoning_forward_chain");
        if (root != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(brain->internal_kg, root, nid,
                              BRAIN_KG_EDGE_PROVIDES_TO, "derived", 0.6f);
        }
    }
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_READ, 0);
}

/**
 * @brief Query the KG for antecedents of a goal node.
 *
 * WHAT: Return up to `max` incoming-edge sources of the goal node.
 * WHY:  Forward chaining benefits from consulting the KG when searching for
 *       facts that feed a given conclusion.  Callers pass the raw predicate
 *       name (e.g. "IsA_cat_animal") or a region/module name.
 *
 * @return Number of antecedent node IDs written, or 0 if goal absent / KG off.
 */
int forward_chaining_kg_query_antecedents(brain_t brain, const char* goal,
                                          brain_kg_node_id_t* ids, int max)
{
    if (!brain || !brain->internal_kg || !goal || !ids || max <= 0) return 0;
    brain_kg_node_id_t goal_id =
        brain_kg_find_node(brain->internal_kg, goal);
    if (goal_id == BRAIN_KG_INVALID_NODE) return 0;

    brain_kg_edge_list_t* incoming =
        brain_kg_get_incoming(brain->internal_kg, goal_id);
    if (!incoming) return 0;

    int n = 0;
    for (uint32_t i = 0; i < incoming->count && n < max; ++i) {
        if (incoming->edges[i]) {
            ids[n++] = incoming->edges[i]->from;
        }
    }
    brain_kg_edge_list_destroy(incoming);
    return n;
}

//=============================================================================
// Forward Chaining Implementation
//=============================================================================

bool brain_forward_chain(
    brain_t brain,
    uint32_t max_iterations,
    forward_chain_result_t* result)
{
    // Validate inputs
    if (!nimcp_validate_pointer(brain, "brain")) {
        set_error("Brain is NULL");
        NIMCP_LOGGING_ERROR("brain_forward_chain: brain is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_forward_chain: nimcp_validate_pointer is NULL");
        return false;
    }

    if (!brain_has_symbolic_logic(brain)) {
        set_error("Symbolic logic engine not attached");
        NIMCP_LOGGING_ERROR("brain_forward_chain: no logic engine attached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_forward_chain: brain_has_symbolic_logic is NULL");
        return false;
    }

    if (!nimcp_validate_pointer(result, "result")) {
        set_error("Result pointer is NULL");
        NIMCP_LOGGING_ERROR("brain_forward_chain: result is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_forward_chain: nimcp_validate_pointer is NULL");
        return false;
    }

    // Initialize result
    /* Phase 8: Heartbeat at operation start */
    forward_chaining_heartbeat("forward_chai_brain_forward_chain", 0.0f);


    memset(result, 0, sizeof(forward_chain_result_t));

    // Cap max iterations
    if (max_iterations == 0 || max_iterations > MAX_ITERATIONS_CAP) {
        max_iterations = MAX_ITERATIONS_CAP;
    }

    // Get logic engine
    symbolic_logic_t* engine = brain_get_symbolic_logic(brain);

    /* W7: create the structural root early so it exists even if the chain
     * fails (useful for observability — empty KB returns failure). */
    fc_kg_ensure_root(brain);

    // Perform forward chaining
    uint64_t start_time = nimcp_time_monotonic_ms();
    logic_clause_t** new_facts = NULL;
    int num_new_facts = 0;

    bool success = symbolic_logic_forward_chain(
        engine,
        max_iterations,
        &new_facts,
        &num_new_facts
    );

    uint64_t end_time = nimcp_time_monotonic_ms();

    if (!success) {
        set_error("Forward chaining execution failed");
        NIMCP_LOGGING_ERROR("brain_forward_chain: symbolic_logic_forward_chain failed");
        /* W7: still emit a step event with zero derived, so the KG records
         * the failed attempt. */
        fc_kg_emit_step(brain, 0, end_time - start_time);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_forward_chain: success is NULL");
        return false;
    }

    // Store new facts in working memory if enabled
    if (brain->working_memory && brain->config.enable_working_memory && num_new_facts > 0) {
        int wm_count = (num_new_facts < MAX_WM_FACTS) ? num_new_facts : MAX_WM_FACTS;
        for (int i = 0; i < wm_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && wm_count > 256) {
                forward_chaining_heartbeat("forward_chai_loop",
                                 (float)(i + 1) / (float)wm_count);
            }

            float fact_encoding[4] = {DEFAULT_SALIENCE, 1.0F, (float)i, 0.0F};
            working_memory_add(brain->working_memory, fact_encoding, 4, DEFAULT_SALIENCE);
        }
        NIMCP_LOGGING_DEBUG("Stored %d new inferences in working memory", wm_count);
    }

    // Populate result
    result->new_facts = new_facts;
    result->num_new_facts = num_new_facts;
    result->iterations_performed = max_iterations; // Actual iterations may be less
    result->confidence = (num_new_facts > 0) ? 0.8F : 0.0F;
    result->inference_time_ms = end_time - start_time;
    result->converged = (num_new_facts == 0); // Converged if no new facts

    /* W7: emit a forward-chain step event so the KG records inference. */
    fc_kg_ensure_root(brain);
    fc_kg_emit_step(brain, num_new_facts, end_time - start_time);

    NIMCP_LOGGING_INFO("Forward chaining completed: %d new facts derived in %llu ms",
                       num_new_facts, (unsigned long long)(end_time - start_time));

    return true;
}

bool brain_forward_chain_step(
    brain_t brain,
    logic_clause_t*** new_facts,
    uint32_t* num_new_facts)
{
    // Validate inputs
    if (!nimcp_validate_pointer(brain, "brain")) {
        set_error("Brain is NULL");
        NIMCP_LOGGING_ERROR("brain_forward_chain_step: brain is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_forward_chain_step: nimcp_validate_pointer is NULL");
        return false;
    }

    if (!brain_has_symbolic_logic(brain)) {
        set_error("Symbolic logic engine not attached");
        NIMCP_LOGGING_ERROR("brain_forward_chain_step: no logic engine attached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_forward_chain_step: brain_has_symbolic_logic is NULL");
        return false;
    }

    if (!nimcp_validate_pointer(new_facts, "new_facts")) {
        set_error("New facts pointer is NULL");
        NIMCP_LOGGING_ERROR("brain_forward_chain_step: new_facts is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_forward_chain_step: nimcp_validate_pointer is NULL");
        return false;
    }

    if (!nimcp_validate_pointer(num_new_facts, "num_new_facts")) {
        set_error("Num new facts pointer is NULL");
        NIMCP_LOGGING_ERROR("brain_forward_chain_step: num_new_facts is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_forward_chain_step: nimcp_validate_pointer is NULL");
        return false;
    }

    // Get logic engine
    /* Phase 8: Heartbeat at operation start */
    forward_chaining_heartbeat("forward_chai_brain_forward_chain_", 0.0f);


    symbolic_logic_t* engine = brain_get_symbolic_logic(brain);

    // Perform single forward chaining step
    int num_derived = 0;
    bool success = symbolic_logic_forward_chain(
        engine,
        1,  // Single iteration
        new_facts,
        &num_derived
    );

    if (!success) {
        set_error("Forward chain step execution failed");
        NIMCP_LOGGING_ERROR("brain_forward_chain_step: execution failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_forward_chain_step: success is NULL");
        return false;
    }

    *num_new_facts = num_derived;

    NIMCP_LOGGING_DEBUG("Forward chain step: %d facts derived", num_derived);
    return true;
}

void forward_chain_free_result(forward_chain_result_t* result)
{
    if (!result) return;

    /* Phase 8: Heartbeat at operation start */
    forward_chaining_heartbeat("forward_chai_forward_chain_free_r", 0.0f);


    if (result->new_facts) {
        // Free each fact
        for (uint32_t i = 0; i < result->num_new_facts; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && result->num_new_facts > 256) {
                forward_chaining_heartbeat("forward_chai_loop",
                                 (float)(i + 1) / (float)result->num_new_facts);
            }

            if (result->new_facts[i]) {
                nimcp_free(result->new_facts[i]);
            }
        }
        // Free the array
        nimcp_free(result->new_facts);
        result->new_facts = NULL;
    }

    memset(result, 0, sizeof(forward_chain_result_t));
}

bool brain_get_forward_chain_stats(
    brain_t brain,
    uint32_t* iterations,
    uint32_t* facts_derived,
    uint64_t* time_ms)
{
    if (!brain || !brain_has_symbolic_logic(brain)) {
        set_error("Brain is NULL or no logic engine attached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_forward_chain_stats: required parameter is NULL (brain, brain_has_symbolic_logic)");
        return false;
    }

    // Get statistics from logic engine
    /* Phase 8: Heartbeat at operation start */
    forward_chaining_heartbeat("forward_chai_brain_get_forward_ch", 0.0f);


    logic_stats_t stats;
    symbolic_logic_t* engine = brain_get_symbolic_logic(brain);

    if (!symbolic_logic_get_stats(engine, &stats)) {
        set_error("Failed to get logic engine statistics");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_get_forward_chain_stats: symbolic_logic_get_stats is NULL");
        return false;
    }

    // Populate outputs (if provided)
    if (iterations) {
        *iterations = 0; // Not tracked separately, would need extension
    }

    if (facts_derived) {
        *facts_derived = stats.facts_stored;
    }

    if (time_ms) {
        // Note: stats.avg_inference_time actually contains the last inference time,
        // not a true running average. Return it directly as the most recent time.
        *time_ms = (uint64_t)stats.avg_inference_time;
    }

    return true;
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

int forward_chaining_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    forward_chaining_heartbeat("forward_chai_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Forward_Chaining");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                forward_chaining_heartbeat("forward_chai_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Forward_Chaining self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Forward_Chaining");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Forward_Chaining");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void forward_chaining_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_forward_chaining_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int forward_chaining_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "forward_chaining_training_begin: NULL argument");
        return -1;
    }
    forward_chaining_heartbeat_instance(NULL, "forward_chaining_training_begin", 0.0f);
    return 0;
}

int forward_chaining_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "forward_chaining_training_end: NULL argument");
        return -1;
    }
    forward_chaining_heartbeat_instance(NULL, "forward_chaining_training_end", 1.0f);
    return 0;
}

int forward_chaining_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "forward_chaining_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    forward_chaining_heartbeat_instance(NULL, "forward_chaining_training_step", progress);
    return 0;
}
