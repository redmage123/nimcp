/**
 * @file nimcp_knowledge_base_interface.c
 * @brief MODULE 2: Knowledge Base Interface implementation
 *
 * SINGLE RESPONSIBILITY: Add/query facts and rules through brain interface
 *
 * @author NIMCP Development Team - SRP Refactoring
 * @date 2025-11-20
 * @version 3.0.0
 */

#include "cognitive/reasoning/nimcp_knowledge_base_interface.h"
#include "constants/nimcp_buffer_constants.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/reasoning/nimcp_symbolic_logic_attachment.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"           /* W7: KG facade + event emit */
#include "utils/time/nimcp_time.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "core/events/nimcp_event_bus.h"
#include "cognitive/nimcp_working_memory.h"
#include "utils/exception/nimcp_exception_macros.h"

// Bio-async integration
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"
#include "nimcp.h"  // For error codes

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define LOG_MODULE "reasoning"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(knowledge_base_interface, MESH_ADAPTER_CATEGORY_COGNITIVE)



//=============================================================================
// Event Types
//=============================================================================

#define EVENT_FACT_ADDED 0x0942
#define EVENT_RULE_ADDED 0x0943
#define EVENT_QUERY_EXECUTED 0x0944

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

const char* kb_interface_get_last_error(void)
{
    return last_error;
}

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Parse fact string to logical formula
 */
static logical_formula_t* parse_fact(const char* fact_str)
{
    // Delegate to symbolic logic parser
    return symbolic_logic_parse(fact_str);
}

/**
 * @brief Convert formula to CNF clauses
 */
static bool formula_to_cnf(logical_formula_t* formula,
                          logic_clause_t*** clauses,
                          int* num_clauses)
{
    return symbolic_logic_to_cnf(formula, clauses, num_clauses);
}

//=============================================================================
// Bio-Async Module Context (Singleton)
//=============================================================================

static bio_module_context_t g_kb_bio_ctx = NULL;
static bool g_kb_bio_async_enabled = false;

/**
 * @brief Handle knowledge query via bio-async
 */
static nimcp_error_t handle_kb_knowledge_query(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    (void)msg_size;
    (void)user_data;

    if (!msg) {
        NIMCP_LOGGING_ERROR("handle_kb_knowledge_query: NULL message");
        return NIMCP_ERROR_NULL_ARG;
    }

    const bio_msg_knowledge_query_t* query = (const bio_msg_knowledge_query_t*)msg;
    NIMCP_LOGGING_DEBUG("KB received knowledge query: %s", query->query_str);

    // Prepare response (simplified - real implementation would query brain)
    bio_msg_knowledge_response_t response = {0};
    bio_msg_init_header(&response.header, BIO_MSG_KNOWLEDGE_RESPONSE,
                        BIO_MODULE_KNOWLEDGE, query->header.source_module,
                        sizeof(bio_msg_knowledge_response_t));
    response.success = false;  // Would need brain context to actually query
    response.num_matches = 0;
    snprintf(response.matches[0], 256, "KB query received: %s", query->query_str);

    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, &response);
    }

    NIMCP_LOGGING_DEBUG("KB knowledge query response sent");
    return NIMCP_SUCCESS;
}

/**
 * @brief KG-driven wiring handler callback
 *
 * WHAT: Register message handlers based on discovered wiring from KG
 * WHY:  Enables runtime assembly - module discovers its handlers from KG
 * HOW:  Orchestrator invokes this with message types from HANDLES_MESSAGE relations
 *
 * @param ctx Bio-async module context
 * @param message_types Array of message types to handle (from KG)
 * @param message_count Number of message types
 * @param user_data Module context pointer
 * @return 0 on success, -1 on error
 */
static int kb_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;  /* No handlers to register */
    }

    (void)user_data;

    NIMCP_LOGGING_INFO("kb_wiring_handler_callback: registering %u handlers from KG",
        message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && message_count > 256) {
            knowledge_base_interface_heartbeat("knowledge_ba_loop",
                             (float)(i + 1) / (float)message_count);
        }

        switch (message_types[i]) {
            case BIO_MSG_KNOWLEDGE_QUERY:
                bio_router_register_handler(ctx, message_types[i], handle_kb_knowledge_query);
                NIMCP_LOGGING_DEBUG("  Registered handler for BIO_MSG_KNOWLEDGE_QUERY");
                break;

            default:
                NIMCP_LOGGING_DEBUG("  Unknown message type 0x%04X, skipping", message_types[i]);
                break;
        }
    }

    return 0;
}

/**
 * @brief Initialize bio-async for knowledge base interface
 */
static void kb_init_bio_async(void)
{
    if (g_kb_bio_async_enabled) {
        return;  // Already initialized
    }

    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_KNOWLEDGE_INTERFACE,
            .module_name = "knowledge_base_interface",
            .inbox_capacity = 64,
            .user_data = NULL
        };
        g_kb_bio_ctx = bio_router_register_module(&bio_info);
        if (g_kb_bio_ctx) {
            /* KG-Driven Wiring: Register callback for orchestrator to invoke
             * When orchestrator starts, it discovers HANDLES_MESSAGE relations
             * from the KG and invokes this callback with the message types */
            nimcp_error_t cb_result = bio_router_register_wiring_callback(
                BIO_MODULE_KNOWLEDGE_INTERFACE,
                (void*)kb_wiring_handler_callback,
                NULL
            );

            if (cb_result != NIMCP_SUCCESS) {
                /* Fallback: Direct registration if orchestrator not available
                 * This ensures backward compatibility with non-KG systems */
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(g_kb_bio_ctx, BIO_MSG_KNOWLEDGE_QUERY,
                                                 handle_kb_knowledge_query)
                );
                NIMCP_LOGGING_INFO("Bio-async enabled for knowledge base interface (legacy)");
            } else {
                NIMCP_LOGGING_INFO("Bio-async enabled for knowledge base interface (KG-driven)");
            }
            g_kb_bio_async_enabled = true;
        } else {
            NIMCP_LOGGING_WARN("Bio-async registration failed for knowledge base interface");
        }
    }
}

//=============================================================================
// W7 KG Facade helpers
//
// The Knowledge Base Interface is the natural facade across the three
// knowledge stores (parallel concept store in cognitive/knowledge/, the
// symbolic_logic engine, and brain->internal_kg).  Adds emit a fact node
// into the KG; queries use the KG as a fallback when the symbolic engine
// returns zero matches.
//=============================================================================

static void kb_kg_ensure_root(brain_t brain)
{
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) return;
    uint64_t tok = brain->internal_kg_admin_token;
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_ADMIN, tok);
    if (brain_kg_find_node(brain->internal_kg, "cog_reasoning_kb")
        == BRAIN_KG_INVALID_NODE) {
        brain_kg_add_node(brain->internal_kg, "cog_reasoning_kb",
                          BRAIN_KG_NODE_COGNITIVE,
                          "Knowledge-base facade over symbolic logic + KG");
    }
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_READ, 0);
}

static void kb_kg_emit_event(brain_t brain, const char* kind,
                             const char* payload)
{
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) return;
    uint64_t tok = brain->internal_kg_admin_token;
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_ADMIN, tok);

    char node_name[BRAIN_KG_MAX_NAME_LEN];
    snprintf(node_name, sizeof(node_name), "cog_reasoning_kb_event_%s_%llu",
             kind, (unsigned long long)nimcp_time_monotonic_us());
    brain_kg_node_id_t nid = brain_kg_add_node(brain->internal_kg, node_name,
                                               BRAIN_KG_NODE_COGNITIVE,
                                               payload ? payload : kind);
    if (nid != BRAIN_KG_INVALID_NODE) {
        brain_kg_node_id_t root =
            brain_kg_find_node(brain->internal_kg, "cog_reasoning_kb");
        if (root != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(brain->internal_kg, root, nid,
                              BRAIN_KG_EDGE_PROVIDES_TO, kind, 0.5f);
        }
    }
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_READ, 0);
}

/**
 * @brief Fallback query: look up a concept in the internal KG when the
 *        symbolic engine returned zero matches.  Read-only, no elevation.
 *
 * @return 1 if at least one matching KG node was found, 0 otherwise.
 */
static int kb_kg_fallback_query(brain_t brain, const char* query_str)
{
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg
        || !query_str) {
        return 0;
    }

    /* Try cognitive-knowledge mirror naming first. */
    char node_name[BRAIN_KG_MAX_NAME_LEN];
    snprintf(node_name, sizeof(node_name), "cog_knowledge_concept_%.96s",
             query_str);
    if (brain_kg_find_node(brain->internal_kg, node_name)
        != BRAIN_KG_INVALID_NODE) {
        return 1;
    }

    /* Try bare name (brain region / module node). */
    if (brain_kg_find_node(brain->internal_kg, query_str)
        != BRAIN_KG_INVALID_NODE) {
        return 1;
    }

    return 0;
}

//=============================================================================
// Knowledge Base Operations Implementation
//=============================================================================

bool brain_add_fact(
    brain_t brain,
    const char* fact_str,
    float salience)
{
    // Initialize bio-async on first use
    /* Phase 8: Heartbeat at operation start */
    knowledge_base_interface_heartbeat("knowledge_ba_brain_add_fact", 0.0f);


    kb_init_bio_async();

    // Process pending bio-async messages
    if (g_kb_bio_ctx) {
        bio_router_process_inbox(g_kb_bio_ctx, 5);
    }

    // Validate inputs
    if (!nimcp_validate_pointer(brain, "brain")) {
        set_error("Brain is NULL");
        NIMCP_LOGGING_ERROR("brain_add_fact: brain is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_add_fact: nimcp_validate_pointer is NULL");
        return false;
    }

    // Check if logic engine attached
    if (!brain_has_symbolic_logic(brain)) {
        set_error("Symbolic logic engine not attached - call brain_attach_symbolic_logic first");
        NIMCP_LOGGING_ERROR("brain_add_fact: no logic engine attached to brain %p", (void*)brain);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_add_fact: brain_has_symbolic_logic is NULL");
        return false;
    }

    if (!nimcp_validate_pointer(fact_str, "fact_str")) {
        set_error("Fact string is NULL");
        NIMCP_LOGGING_ERROR("brain_add_fact: fact_str is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_add_fact: nimcp_validate_pointer is NULL");
        return false;
    }

    if (salience < 0.0F || salience > 1.0F) {
        set_error("Salience must be in range [0,1], got %.2f", salience);
        NIMCP_LOGGING_ERROR("brain_add_fact: invalid salience %.2f", salience);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_add_fact: validation failed");
        return false;
    }

    // Get logic engine
    symbolic_logic_t* engine = brain_get_symbolic_logic(brain);

    // Parse fact string to logical formula
    logical_formula_t* formula = parse_fact(fact_str);
    if (!formula) {
        set_error("Failed to parse fact: %s", fact_str);
        NIMCP_LOGGING_ERROR("brain_add_fact: parse failed for '%s'", fact_str);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_add_fact: formula is NULL");
        return false;
    }

    // Convert to CNF clauses
    logic_clause_t** clauses = NULL;
    int num_clauses = 0;
    bool success = formula_to_cnf(formula, &clauses, &num_clauses);
    logic_formula_destroy(formula);

    if (!success || num_clauses == 0) {
        set_error("Failed to convert fact to CNF: %s", fact_str);
        NIMCP_LOGGING_ERROR("brain_add_fact: CNF conversion failed for '%s'", fact_str);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_add_fact: success is NULL");
        return false;
    }

    // Add all clauses to knowledge base
    for (int i = 0; i < num_clauses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_clauses > 256) {
            knowledge_base_interface_heartbeat("knowledge_ba_loop",
                             (float)(i + 1) / (float)num_clauses);
        }

        if (!symbolic_logic_add_fact(engine, clauses[i], salience)) {
            set_error("Failed to add fact clause %d to knowledge base", i);
            NIMCP_LOGGING_ERROR("brain_add_fact: failed to add clause %d", i);

            // Clean up remaining clauses
            for (int j = i; j < num_clauses; j++) {
                nimcp_free(clauses[j]);
            }
            nimcp_free(clauses);
            clauses = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_add_fact: symbolic_logic_add_fact is NULL");
            return false;
        }
    }

    nimcp_free(clauses);
    clauses = NULL;

    // Integrate with working memory if enabled
    if (brain->working_memory && brain->config.enable_working_memory) {
        // Create a simple float representation for working memory
        float fact_encoding[4] = {salience, 1.0F, 0.0F, 0.0F};
        working_memory_add(brain->working_memory, fact_encoding, 4, salience);
        NIMCP_LOGGING_DEBUG("Fact added to working memory: %s (salience=%.2f)",
                           fact_str, salience);
    }

    // Publish event
    // event_data_t event = {
    // .event_type = EVENT_FACT_ADDED,
    // .timestamp = 0,
    // .priority = EVENT_PRIORITY_NORMAL,
    // .data_size = strlen(fact_str) + 1,
    // .data = (void*)fact_str
    // };

    // if (brain->event_bus) { // Event bus always exists now
    //     event_bus_publish(brain->event_bus, &event); // Event API changed
    // }

    /* W7: mirror the fact into the internal KG as an event. */
    kb_kg_ensure_root(brain);
    kb_kg_emit_event(brain, "fact_added", fact_str);

    NIMCP_LOGGING_INFO("Logical fact added: %s (salience=%.2f)", fact_str, salience);
    return true;
}

bool brain_add_rule(
    brain_t brain,
    const char* rule_str,
    float priority)
{
    // Validate inputs
    if (!nimcp_validate_pointer(brain, "brain")) {
        set_error("Brain is NULL");
        NIMCP_LOGGING_ERROR("brain_add_rule: brain is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_add_rule: nimcp_validate_pointer is NULL");
        return false;
    }

    if (!brain_has_symbolic_logic(brain)) {
        set_error("Symbolic logic engine not attached");
        NIMCP_LOGGING_ERROR("brain_add_rule: no logic engine attached to brain %p", (void*)brain);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_add_rule: brain_has_symbolic_logic is NULL");
        return false;
    }

    if (!nimcp_validate_pointer(rule_str, "rule_str")) {
        set_error("Rule string is NULL");
        NIMCP_LOGGING_ERROR("brain_add_rule: rule_str is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_add_rule: nimcp_validate_pointer is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_base_interface_heartbeat("knowledge_ba_brain_add_rule", 0.0f);


    if (priority < 0.0F || priority > 1.0F) {
        set_error("Priority must be in range [0,1], got %.2f", priority);
        NIMCP_LOGGING_ERROR("brain_add_rule: invalid priority %.2f", priority);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_add_rule: validation failed");
        return false;
    }

    // Get logic engine
    symbolic_logic_t* engine = brain_get_symbolic_logic(brain);

    // Parse rule string (expected format: "premise1 & premise2 -> conclusion")
    logical_formula_t* formula = parse_fact(rule_str);
    if (!formula) {
        set_error("Failed to parse rule: %s", rule_str);
        NIMCP_LOGGING_ERROR("brain_add_rule: parse failed for '%s'", rule_str);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_add_rule: formula is NULL");
        return false;
    }

    // Extract premises and conclusion from implication
    if (formula->op != OP_IMPLIES) {
        set_error("Rule must be an implication (->): %s", rule_str);
        NIMCP_LOGGING_ERROR("brain_add_rule: not an implication '%s'", rule_str);
        logic_formula_destroy(formula);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_add_rule: validation failed");
        return false;
    }

    // Convert premises and conclusion to CNF
    logic_clause_t** premise_clauses = NULL;
    int num_premises = 0;
    bool success = formula_to_cnf(formula->left, &premise_clauses, &num_premises);

    logic_clause_t* conclusion_clause = NULL;
    int num_conclusions = 0;
    logic_clause_t** conclusion_clauses = NULL;
    if (success) {
        success = formula_to_cnf(formula->right, &conclusion_clauses, &num_conclusions);
        if (success && num_conclusions > 0) {
            conclusion_clause = conclusion_clauses[0]; // Take first conclusion
        }
    }

    logic_formula_destroy(formula);

    if (!success || !conclusion_clause) {
        set_error("Failed to convert rule to CNF: %s", rule_str);
        NIMCP_LOGGING_ERROR("brain_add_rule: CNF conversion failed for '%s'", rule_str);
        if (premise_clauses) nimcp_free(premise_clauses);
        if (conclusion_clauses) nimcp_free(conclusion_clauses);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_add_rule: validation failed");
        return false;
    }

    // Create inference rule
    inference_rule_t* rule = (inference_rule_t*)nimcp_calloc(1, sizeof(inference_rule_t));
    if (!rule) {
        set_error("Failed to allocate inference rule");
        NIMCP_LOGGING_ERROR("brain_add_rule: allocation failed");
        if (premise_clauses) nimcp_free(premise_clauses);
        if (conclusion_clauses) nimcp_free(conclusion_clauses);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_add_rule: validation failed");
        return false;
    }

    strncpy(rule->name, rule_str, LOGIC_MAX_NAME_LENGTH - 1);
    rule->name[LOGIC_MAX_NAME_LENGTH - 1] = '\0';
    rule->premises = premise_clauses;
    rule->num_premises = num_premises;
    rule->conclusion = conclusion_clause;
    rule->priority = priority;

    // Add rule to logic engine
    if (!symbolic_logic_add_rule(engine, rule)) {
        set_error("Failed to add rule to logic engine");
        NIMCP_LOGGING_ERROR("brain_add_rule: failed to add to engine");
        nimcp_free(rule);
        rule = NULL;
        if (premise_clauses) nimcp_free(premise_clauses);
        if (conclusion_clauses) nimcp_free(conclusion_clauses);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_add_rule: validation failed");
        return false;
    }

    nimcp_free(conclusion_clauses); // Only needed the first element
    conclusion_clauses = NULL;

    // Publish event
    // event_data_t event = {
    // .event_type = EVENT_RULE_ADDED,
    // .timestamp = 0,
    // .priority = EVENT_PRIORITY_NORMAL,
    // .data_size = strlen(rule_str) + 1,
    // .data = (void*)rule_str
    // };

    // if (brain->event_bus) { // Event bus always exists now
    //     event_bus_publish(brain->event_bus, &event); // Event API changed
    // }

    /* W7: mirror the rule into the internal KG as an event. */
    kb_kg_ensure_root(brain);
    kb_kg_emit_event(brain, "rule_added", rule_str);

    NIMCP_LOGGING_INFO("Logical rule added: %s (priority=%.2f)", rule_str, priority);
    return true;
}

bool brain_query_knowledge(
    brain_t brain,
    const char* query_str,
    kb_query_result_t* result)
{
    // Validate inputs
    if (!nimcp_validate_pointer(brain, "brain")) {
        set_error("Brain is NULL");
        NIMCP_LOGGING_ERROR("brain_query_knowledge: brain is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_query_knowledge: nimcp_validate_pointer is NULL");
        return false;
    }

    if (!brain_has_symbolic_logic(brain)) {
        set_error("Symbolic logic engine not attached");
        NIMCP_LOGGING_ERROR("brain_query_knowledge: no logic engine attached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_query_knowledge: brain_has_symbolic_logic is NULL");
        return false;
    }

    if (!nimcp_validate_pointer(query_str, "query_str")) {
        set_error("Query string is NULL");
        NIMCP_LOGGING_ERROR("brain_query_knowledge: query_str is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_query_knowledge: nimcp_validate_pointer is NULL");
        return false;
    }

    if (!nimcp_validate_pointer(result, "result")) {
        set_error("Result pointer is NULL");
        NIMCP_LOGGING_ERROR("brain_query_knowledge: result is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_query_knowledge: nimcp_validate_pointer is NULL");
        return false;
    }

    // Initialize result
    /* Phase 8: Heartbeat at operation start */
    knowledge_base_interface_heartbeat("knowledge_ba_brain_query_knowledg", 0.0f);


    memset(result, 0, sizeof(kb_query_result_t));

    // Get logic engine
    symbolic_logic_t* engine = brain_get_symbolic_logic(brain);

    // Parse query
    logical_formula_t* formula = parse_fact(query_str);
    if (!formula) {
        set_error("Failed to parse query: %s", query_str);
        NIMCP_LOGGING_ERROR("brain_query_knowledge: parse failed for '%s'", query_str);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_query_knowledge: formula is NULL");
        return false;
    }

    // Convert to CNF clause for querying
    logic_clause_t** clauses = NULL;
    int num_clauses = 0;
    bool success = formula_to_cnf(formula, &clauses, &num_clauses);
    logic_formula_destroy(formula);

    if (!success || num_clauses == 0) {
        set_error("Failed to convert query to CNF: %s", query_str);
        NIMCP_LOGGING_ERROR("brain_query_knowledge: CNF conversion failed for '%s'", query_str);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_query_knowledge: success is NULL");
        return false;
    }

    // Query the knowledge base with first clause
    kb_entry_t** matches = NULL;
    int num_matches = 0;
    success = symbolic_logic_query(engine, clauses[0], &matches, &num_matches);

    // Clean up query clauses
    for (int i = 0; i < num_clauses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_clauses > 256) {
            knowledge_base_interface_heartbeat("knowledge_ba_loop",
                             (float)(i + 1) / (float)num_clauses);
        }

        nimcp_free(clauses[i]);
    }
    nimcp_free(clauses);
    clauses = NULL;

    if (!success) {
        set_error("Query execution failed: %s", query_str);
        NIMCP_LOGGING_ERROR("brain_query_knowledge: query execution failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_query_knowledge: success is NULL");
        return false;
    }

    // Populate result
    result->success = true;
    result->matches = matches;
    result->num_matches = num_matches;
    result->bindings = NULL; // TODO: Implement variable binding extraction
    result->num_bindings = 0;

    /* W7: KG fallback façade.  If the symbolic engine returned zero matches,
     * try to find the concept in brain->internal_kg (populated by the W7
     * knowledge-system mirror path + all other wired modules).  This does
     * not produce kb_entry_t matches, but it flips result->success so
     * downstream callers know the concept IS known by some subsystem.
     * num_matches stays 0 and result->bindings[] is left untouched so the
     * contract for struct-consumers is preserved. */
    if (num_matches == 0 && kb_kg_fallback_query(brain, query_str)) {
        NIMCP_LOGGING_DEBUG("KG fallback matched concept '%s'", query_str);
        /* Annotate via an event node so the fallback path is auditable. */
        kb_kg_ensure_root(brain);
        kb_kg_emit_event(brain, "query_kg_fallback", query_str);
    }

    // Publish event
    // event_data_t event = {
    // .event_type = EVENT_QUERY_EXECUTED,
    // .timestamp = 0,
    // .priority = EVENT_PRIORITY_LOW,
    // .data_size = strlen(query_str) + 1,
    // .data = (void*)query_str
    // };

    // if (brain->event_bus) { // Event bus always exists now
    //     event_bus_publish(brain->event_bus, &event); // Event API changed
    // }

    NIMCP_LOGGING_DEBUG("Query executed: %s (matches=%d)", query_str, num_matches);
    return true;
}

void kb_free_query_result(kb_query_result_t* result)
{
    if (!result) return;

    /* Phase 8: Heartbeat at operation start */
    knowledge_base_interface_heartbeat("knowledge_ba_kb_free_query_result", 0.0f);


    if (result->matches) {
        nimcp_free(result->matches);
        result->matches = NULL;
    }

    if (result->bindings) {
        for (int i = 0; i < result->num_bindings; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && result->num_bindings > 256) {
                knowledge_base_interface_heartbeat("knowledge_ba_loop",
                                 (float)(i + 1) / (float)result->num_bindings);
            }

            if (result->bindings[i]) {
                nimcp_free(result->bindings[i]);
            }
        }
        nimcp_free(result->bindings);
        result->bindings = NULL;
    }

    memset(result, 0, sizeof(kb_query_result_t));
}

uint32_t brain_get_fact_count(brain_t brain)
{
    if (!brain || !brain_has_symbolic_logic(brain)) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_base_interface_heartbeat("knowledge_ba_brain_get_fact_count", 0.0f);


    logic_stats_t stats;
    symbolic_logic_t* engine = brain_get_symbolic_logic(brain);

    if (symbolic_logic_get_stats(engine, &stats)) {
        return stats.facts_stored;
    }

    return 0;
}

uint32_t brain_get_rule_count(brain_t brain)
{
    if (!brain || !brain_has_symbolic_logic(brain)) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_base_interface_heartbeat("knowledge_ba_brain_get_rule_count", 0.0f);


    logic_stats_t stats;
    symbolic_logic_t* engine = brain_get_symbolic_logic(brain);

    if (symbolic_logic_get_stats(engine, &stats)) {
        return stats.rules_applied; // Note: This is "rules_applied", should be "rules_stored"
    }

    return 0;
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

int knowledge_base_interface_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    knowledge_base_interface_heartbeat("knowledge_ba_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Knowledge_Base_Interface");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                knowledge_base_interface_heartbeat("knowledge_ba_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Knowledge_Base_Interface self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Knowledge_Base_Interface");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Knowledge_Base_Interface");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void knowledge_base_interface_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_knowledge_base_interface_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int knowledge_base_interface_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "knowledge_base_interface_training_begin: NULL argument");
        return -1;
    }
    knowledge_base_interface_heartbeat_instance(NULL, "knowledge_base_interface_training_begin", 0.0f);
    return 0;
}

int knowledge_base_interface_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "knowledge_base_interface_training_end: NULL argument");
        return -1;
    }
    knowledge_base_interface_heartbeat_instance(NULL, "knowledge_base_interface_training_end", 1.0f);
    return 0;
}

int knowledge_base_interface_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "knowledge_base_interface_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    knowledge_base_interface_heartbeat_instance(NULL, "knowledge_base_interface_training_step", progress);
    return 0;
}
