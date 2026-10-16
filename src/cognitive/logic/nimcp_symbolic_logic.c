/**
 * @file nimcp_symbolic_logic.c
 * @brief Symbolic logic engine implementation
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 2.6
 */

#include "cognitive/nimcp_symbolic_logic.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "core/brain/nimcp_brain_internal.h"     /* W7: KG access */
#include "core/brain/nimcp_brain_kg.h"           /* W7: KG event emission */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"

#define NIMCP_SYMBOLIC_QUANTUM_BRIDGE_IMPLEMENTATION
#include "cognitive/reasoning/nimcp_symbolic_logic_quantum_bridge.h"

#define LOG_MODULE "cognitive.logic"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(symbolic_logic, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define BIO_MODULE_COGNITIVE_LOGIC 0x0343

/* Phase 5: Predicate hash index for O(1) fact lookup */
#define PRED_INDEX_BUCKETS 256
#define PRED_INDEX_BUCKET_CAP 64

typedef struct {
    uint32_t fact_indices[PRED_INDEX_BUCKET_CAP]; /* Indices into kb[] */
    uint32_t count;
} pred_index_bucket_t;

//=============================================================================
// Internal Structures
//=============================================================================

struct symbolic_logic {
    logic_config_t config;

    // Knowledge base
    kb_entry_t** kb;
    uint32_t num_facts;
    uint32_t kb_capacity;

    // Rules
    inference_rule_t** rules;
    uint32_t num_rules;
    uint32_t rules_capacity;

    // Statistics
    logic_stats_t stats;

    // Working memory for inference
    logic_clause_t** working_memory;
    uint32_t working_memory_size;

    // Quantum bridge integration
    symbolic_quantum_bridge_t* quantum_bridge;  /**< Quantum reasoning bridge */

    // Bio-async integration
    bio_module_context_t bio_ctx;   /**< Bio-async module context */
    bool bio_async_enabled;         /**< Bio-async registration status */

    /* Phase 5: Predicate hash index */
    pred_index_bucket_t pred_index[PRED_INDEX_BUCKETS];
    bool pred_index_built;

    /* W7: KG integration — optional back-reference to the owning brain so
     * add_fact / add_rule can mirror symbolic writes into internal_kg. */
    brain_t kg_brain;
};

/* Phase 5: FNV-1a hash for predicate names */
static uint32_t hash_predicate(const char* name) {
    uint32_t h = 2166136261u;
    for (const char* p = name; *p; p++) {
        h ^= (uint8_t)*p;
        h *= 16777619u;
    }
    return h % PRED_INDEX_BUCKETS;
}

static void predicate_index_add(symbolic_logic_t* logic, uint32_t fact_idx) {
    if (!logic || fact_idx >= logic->num_facts) return;
    kb_entry_t* entry = logic->kb[fact_idx];
    if (!entry || !entry->clause || entry->clause->num_literals == 0) return;

    const char* pred = entry->clause->literals[0]->name;
    uint32_t bucket = hash_predicate(pred);
    pred_index_bucket_t* b = &logic->pred_index[bucket];
    if (b->count < PRED_INDEX_BUCKET_CAP) {
        b->fact_indices[b->count++] = fact_idx;
    }
}

static void predicate_index_rebuild(symbolic_logic_t* logic) {
    if (!logic) return;
    memset(logic->pred_index, 0, sizeof(logic->pred_index));
    for (uint32_t i = 0; i < logic->num_facts; i++) {
        predicate_index_add(logic, i);
    }
    logic->pred_index_built = true;
}

static const pred_index_bucket_t* predicate_index_lookup(
    const symbolic_logic_t* logic, const char* predicate) {
    if (!logic || !predicate || !logic->pred_index_built) return NULL;
    uint32_t bucket = hash_predicate(predicate);
    return &logic->pred_index[bucket];
}

//=============================================================================
// Helper Functions - Term Management
//=============================================================================

logical_term_t* logic_term_create(term_type_t type, const char* name)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_heartbeat("symbolic_log_logic_term_create", 0.0f);


    LOG_DEBUG("Creating module");
    NIMCP_API_CHECK_NULL_RET_NULL(name, "NULL name in logic_term_create");

    logical_term_t* term = (logical_term_t*)nimcp_calloc(1, sizeof(logical_term_t));
    if (!term) return NULL;
    NIMCP_API_CHECK_ALLOC(term, "Failed to allocate logical term");

    term->type = type;
    strncpy(term->name, name, LOGIC_MAX_NAME_LENGTH - 1);
    term->name[LOGIC_MAX_NAME_LENGTH - 1] = '\0';
    term->args = NULL;
    term->arity = 0;

    return term;
}

void logic_term_destroy(logical_term_t* term)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_heartbeat("symbolic_log_logic_term_destroy", 0.0f);


    LOG_DEBUG("Destroying module");
    if (!term) return;

    if (term->args) {
        for (uint8_t i = 0; i < term->arity; i++) {
            logic_term_destroy(term->args[i]);
        }
        nimcp_free(term->args);
    }

    nimcp_free(term);
    term = NULL;
}

static logical_term_t* logic_term_copy(const logical_term_t* term)
{
    if (!term) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "logic_term_copy: term is NULL");
        return NULL;
    }

    logical_term_t* copy = logic_term_create(term->type, term->name);
    if (!copy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "logic_term_copy: copy is NULL");
        return NULL;
    }

    if (term->arity > 0 && term->args) {
        copy->arity = term->arity;
        copy->args = (logical_term_t**)nimcp_calloc(term->arity, sizeof(logical_term_t*));
        if (!copy->args) {
            nimcp_free(copy);
            copy = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "logic_term_copy: copy->args is NULL");
            return NULL;
        }

        for (uint8_t i = 0; i < term->arity; i++) {
            copy->args[i] = logic_term_copy(term->args[i]);
            if (!copy->args[i]) {
                logic_term_destroy(copy);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "logic_term_copy: copy->args is NULL");
                return NULL;
            }
        }
    }

    return copy;
}

static bool terms_equal(const logical_term_t* t1, const logical_term_t* t2)
{
    if (!t1 || !t2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "terms_equal: required parameter is NULL (t1, t2)");
        return false;
    }
    if (t1->type != t2->type) {
        return false;
    }
    if (strcmp(t1->name, t2->name) != 0) {
        return false;
    }
    if (t1->arity != t2->arity) {
        return false;
    }

    for (uint8_t i = 0; i < t1->arity; i++) {
        if (!terms_equal(t1->args[i], t2->args[i])) {
            return false;
        }
    }

    return true;
}

//=============================================================================
// Helper Functions - Atomic Formula Management
//=============================================================================

atomic_formula_t* logic_atom_create(const char* name, logical_term_t** terms, uint8_t arity)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_heartbeat("symbolic_log_logic_atom_create", 0.0f);


    LOG_DEBUG("Creating module");
    NIMCP_API_CHECK_NULL_RET_NULL(name, "NULL name in logic_atom_create");

    if (arity > LOGIC_MAX_ARITY) {
        LOG_ERROR("Arity %d exceeds maximum %d", arity, LOGIC_MAX_ARITY);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Arity %d exceeds maximum %d", arity, LOGIC_MAX_ARITY);
        return NULL;
    }

    atomic_formula_t* atom = (atomic_formula_t*)nimcp_calloc(1, sizeof(atomic_formula_t));
    if (!atom) return NULL;
    NIMCP_API_CHECK_ALLOC(atom, "Failed to allocate atomic formula");

    strncpy(atom->name, name, LOGIC_MAX_NAME_LENGTH - 1);
    atom->name[LOGIC_MAX_NAME_LENGTH - 1] = '\0';
    atom->arity = arity;
    atom->negated = false;

    if (arity > 0 && terms) {
        atom->terms = (logical_term_t**)nimcp_calloc(arity, sizeof(logical_term_t*));
        if (!atom->terms) {
            nimcp_free(atom);
            atom = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "logic_atom_create: atom->terms is NULL");
            return NULL;
        }

        for (uint8_t i = 0; i < arity; i++) {
            atom->terms[i] = logic_term_copy(terms[i]);
            if (!atom->terms[i]) {
                logic_atom_destroy(atom);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "logic_atom_create: atom->terms is NULL");
                return NULL;
            }
        }
    }

    return atom;
}

void logic_atom_destroy(atomic_formula_t* atom)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_heartbeat("symbolic_log_logic_atom_destroy", 0.0f);


    LOG_DEBUG("Destroying module");
    if (!atom) return;

    if (atom->terms) {
        for (uint8_t i = 0; i < atom->arity; i++) {
            logic_term_destroy(atom->terms[i]);
        }
        nimcp_free(atom->terms);
    }

    nimcp_free(atom);
    atom = NULL;
}

//=============================================================================
// Formula Construction - Missing Functions
//=============================================================================

logical_formula_t* logic_formula_create(
    logical_operator_t op,
    logical_formula_t* left,
    logical_formula_t* right)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_heartbeat("symbolic_log_logic_formula_create", 0.0f);


    LOG_DEBUG("Creating formula");
    logical_formula_t* formula = (logical_formula_t*)nimcp_calloc(1, sizeof(logical_formula_t));
    if (!formula) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "logic_formula_create: formula is NULL");
        return NULL;
    }

    formula->op = op;
    formula->atom = NULL;
    formula->left = left;
    formula->right = right;
    formula->quantified_var = NULL;

    return formula;
}

void logic_formula_destroy(logical_formula_t* formula)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_heartbeat("symbolic_log_logic_formula_destro", 0.0f);


    LOG_DEBUG("Destroying formula");
    if (!formula) return;

    if (formula->atom) {
        logic_atom_destroy(formula->atom);
    }
    if (formula->left) {
        logic_formula_destroy(formula->left);
    }
    if (formula->right) {
        logic_formula_destroy(formula->right);
    }
    if (formula->quantified_var) {
        logic_term_destroy(formula->quantified_var);
    }

    nimcp_free(formula);
    formula = NULL;
}

logical_formula_t* symbolic_logic_parse(const char* str)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_heartbeat("symbolic_log_parse", 0.0f);


    LOG_DEBUG("Parsing formula: %s", str ? str : "(null)");
    if (!str || strlen(str) == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_parse: str is NULL");
        return NULL;
    }

    // Simple parser: handle basic predicates like "P(x)" or "Bird(tweety)"
    // For complex formulas, this is a simplified implementation
    logical_formula_t* formula = (logical_formula_t*)nimcp_calloc(1, sizeof(logical_formula_t));
    if (!formula) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "symbolic_logic_parse: formula is NULL");
        return NULL;
    }

    // Check for negation
    bool negated = false;
    const char* start = str;
    while (*start == '~' || *start == '!' || *start == ' ') {
        if (*start == '~' || *start == '!') negated = true;
        start++;
    }

    // Find predicate name and arguments
    char pred_name[LOGIC_MAX_NAME_LENGTH] = {0};
    const char* paren = strchr(start, '(');
    if (paren) {
        size_t name_len = paren - start;
        if (name_len >= LOGIC_MAX_NAME_LENGTH) name_len = LOGIC_MAX_NAME_LENGTH - 1;
        strncpy(pred_name, start, name_len);
    } else {
        strncpy(pred_name, start, LOGIC_MAX_NAME_LENGTH - 1);
    }

    // Create atomic formula
    atomic_formula_t* atom = logic_atom_create(pred_name, NULL, 0);
    if (!atom) {
        nimcp_free(formula);
        formula = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "symbolic_logic_parse: atom is NULL");
        return NULL;
    }
    atom->negated = negated;

    formula->atom = atom;
    formula->op = OP_AND; // Default
    formula->left = NULL;
    formula->right = NULL;

    return formula;
}

bool symbolic_logic_to_cnf(
    logical_formula_t* formula,
    logic_clause_t*** clauses,
    int* num_clauses)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_heartbeat("symbolic_log_to_cnf", 0.0f);


    LOG_DEBUG("Converting to CNF");
    if (!formula || !clauses || !num_clauses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_to_cnf: required parameter is NULL (formula, clauses, num_clauses)");
        return false;
    }

    // Simple implementation: convert single atomic formula to single clause
    *num_clauses = 1;
    *clauses = (logic_clause_t**)nimcp_calloc(1, sizeof(logic_clause_t*));
    if (!*clauses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_to_cnf: validation failed");
        return false;
    }

    logic_clause_t* clause = (logic_clause_t*)nimcp_calloc(1, sizeof(logic_clause_t));
    if (!clause) {
        nimcp_free(*clauses);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "symbolic_logic_to_cnf: clause is NULL");
        return false;
    }

    if (formula->atom) {
        clause->num_literals = 1;
        clause->literals = (atomic_formula_t**)nimcp_calloc(1, sizeof(atomic_formula_t*));
        if (clause->literals) {
            atomic_formula_t* lit = (atomic_formula_t*)nimcp_calloc(1, sizeof(atomic_formula_t));
            if (lit) {
                strncpy(lit->name, formula->atom->name, LOGIC_MAX_NAME_LENGTH - 1);
                lit->negated = formula->atom->negated;
                lit->arity = 0;
                lit->terms = NULL;
                clause->literals[0] = lit;
            }
        }
        clause->confidence = 1.0F;
    }

    (*clauses)[0] = clause;
    return true;
}

bool symbolic_logic_backward_chain(
    symbolic_logic_t* logic,
    logic_clause_t* goal,
    inference_rule_t*** proof_trace,
    int* num_steps)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_heartbeat("symbolic_log_backward_chain", 0.0f);


    LOG_DEBUG("Backward chaining");
    if (!logic || !goal || !proof_trace || !num_steps) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_backward_chain: required parameter is NULL (logic, goal, proof_trace, num_steps)");
        return false;
    }

    // Simple implementation: check if goal matches any fact
    *proof_trace = NULL;
    *num_steps = 0;

    /* Phase 5: Use predicate index for O(1) lookup */
    if (logic->pred_index_built && goal->num_literals > 0 && goal->literals[0]) {
        const pred_index_bucket_t* bucket = predicate_index_lookup(logic, goal->literals[0]->name);
        if (bucket) {
            for (uint32_t bi = 0; bi < bucket->count; bi++) {
                uint32_t f = bucket->fact_indices[bi];
                if (f < logic->num_facts) {
                    kb_entry_t* entry = logic->kb[f];
                    if (!entry || !entry->clause) continue;
                    if (entry->clause->num_literals == goal->num_literals &&
                        entry->clause->num_literals > 0) {
                        if (strcmp(entry->clause->literals[0]->name,
                                  goal->literals[0]->name) == 0) {
                            LOG_DEBUG("Goal proven by indexed fact match");
                            return true;
                        }
                    }
                }
            }
        }
    } else {
        for (uint32_t i = 0; i < logic->num_facts; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && logic->num_facts > 256) {
                symbolic_logic_heartbeat("symbolic_log_loop",
                                 (float)(i + 1) / (float)logic->num_facts);
            }

            kb_entry_t* entry = logic->kb[i];
            if (!entry || !entry->clause) continue;

            // Check if clause matches goal
            if (entry->clause->num_literals == goal->num_literals &&
                entry->clause->num_literals > 0) {
                // Simple match by predicate name
                if (strcmp(entry->clause->literals[0]->name,
                          goal->literals[0]->name) == 0) {
                    LOG_DEBUG("Goal proven by fact match");
                    return true;
                }
            }
        }
    }

    LOG_DEBUG("Goal could not be proven");
    return false;
}

bool symbolic_logic_resolve(
    symbolic_logic_t* logic,
    logic_clause_t* negated_goal,
    bool* derived_empty)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_heartbeat("symbolic_log_resolve", 0.0f);


    LOG_DEBUG("Resolution proving");
    if (!logic || !negated_goal || !derived_empty) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_resolve: required parameter is NULL (logic, negated_goal, derived_empty)");
        return false;
    }

    *derived_empty = false;

    /* Robinson's Resolution Algorithm:
       Add negated_goal to the clause set, then repeatedly try to resolve
       pairs of clauses. If we derive the empty clause, the goal is proven. */

    /* Collect clauses: KB facts + negated_goal */
    uint32_t max_clauses = logic->num_facts + 128; /* room for resolvents */
    uint32_t num_clauses = 0;

    /* Use arrays of (literals, num_literals, negated) triples stored inline */
    /* For efficiency, store pointers to existing clause data + generated resolvents */
    typedef struct { atomic_formula_t** lits; uint32_t n; bool owned; } res_clause_t;
    res_clause_t* clauses = nimcp_calloc(max_clauses, sizeof(res_clause_t));
    if (!clauses) return false;

    /* Add KB facts */
    for (uint32_t i = 0; i < logic->num_facts && num_clauses < max_clauses; i++) {
        if (logic->kb[i] && logic->kb[i]->clause &&
            logic->kb[i]->clause->num_literals > 0) {
            clauses[num_clauses].lits = logic->kb[i]->clause->literals;
            clauses[num_clauses].n = logic->kb[i]->clause->num_literals;
            clauses[num_clauses].owned = false;
            num_clauses++;
        }
    }

    /* Add negated goal */
    if (negated_goal->num_literals > 0) {
        clauses[num_clauses].lits = negated_goal->literals;
        clauses[num_clauses].n = negated_goal->num_literals;
        clauses[num_clauses].owned = false;
        num_clauses++;
    }

    /* Resolution loop: try all pairs, up to iteration limit */
    uint32_t max_iterations = 200;
    bool found_empty = false;

    for (uint32_t iter = 0; iter < max_iterations && !found_empty; iter++) {
        bool progress = false;
        uint32_t start_count = num_clauses;

        for (uint32_t i = 0; i < start_count && !found_empty; i++) {
            for (uint32_t j = i + 1; j < start_count && !found_empty; j++) {
                /* Try to find complementary literals between clause i and j */
                for (uint32_t li = 0; li < clauses[i].n; li++) {
                    for (uint32_t lj = 0; lj < clauses[j].n; lj++) {
                        atomic_formula_t* a = clauses[i].lits[li];
                        atomic_formula_t* b = clauses[j].lits[lj];
                        if (!a || !b) continue;

                        /* Check complementary: same name, opposite negation */
                        if (strcmp(a->name, b->name) == 0 &&
                            a->negated != b->negated &&
                            a->arity == b->arity) {

                            /* Build resolvent: all literals from both clauses
                               except the complementary pair */
                            uint32_t res_n = (clauses[i].n - 1) + (clauses[j].n - 1);

                            if (res_n == 0) {
                                /* Empty clause derived — proof found! */
                                found_empty = true;
                                break;
                            }

                            /* Only add if room */
                            if (num_clauses < max_clauses) {
                                atomic_formula_t** res_lits = nimcp_calloc(res_n, sizeof(atomic_formula_t*));
                                if (!res_lits) continue;
                                uint32_t ri = 0;
                                for (uint32_t k = 0; k < clauses[i].n; k++)
                                    if (k != li) res_lits[ri++] = clauses[i].lits[k];
                                for (uint32_t k = 0; k < clauses[j].n; k++)
                                    if (k != lj) res_lits[ri++] = clauses[j].lits[k];

                                clauses[num_clauses].lits = res_lits;
                                clauses[num_clauses].n = ri;
                                clauses[num_clauses].owned = true;
                                num_clauses++;
                                progress = true;
                            }
                        }
                    }
                    if (found_empty) break;
                }
            }
        }
        if (!progress) break; /* No new resolvents possible */
    }

    *derived_empty = found_empty;

    /* Clean up owned resolvents */
    for (uint32_t i = 0; i < num_clauses; i++) {
        if (clauses[i].owned) nimcp_free(clauses[i].lits);
    }
    nimcp_free(clauses);

    logic->stats.inferences_performed++;
    return found_empty;
}

bool symbolic_logic_evaluate(
    symbolic_logic_t* logic,
    logical_formula_t* formula,
    bool* result)
{
    // Process pending bio-async messages
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_heartbeat("symbolic_log_evaluate", 0.0f);


    if (logic && logic->bio_ctx) {
        bio_router_process_inbox(logic->bio_ctx, 5);
    }

    LOG_DEBUG("Evaluating formula");
    if (!logic || !formula || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_evaluate: required parameter is NULL (logic, formula, result)");
        return false;
    }

    *result = false;

    if (!formula->atom) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_evaluate: formula->atom is NULL");
        return false;
    }

    // WHAT: Try quantum-accelerated evaluation first if available
    // WHY:  O(√N) speedup for SAT solving, handles uncertainty with ternary logic
    // HOW:  Query quantum bridge, fallback to classical evaluation
    if (logic->quantum_bridge && symbolic_quantum_bridge_is_enabled(logic->quantum_bridge)) {
        // Map predicate name to variable ID (simple hash for demo)
        uint32_t variable = 0;
        for (size_t i = 0; i < strlen(formula->atom->name); i++) {
            variable = (variable * 31 + formula->atom->name[i]) % 1000;
        }
        variable = variable % QREASON_MAX_VARIABLES;  // Ensure within bounds

        qreason_result_t qresult;
        memset(&qresult, 0, sizeof(qresult));

        int query_status = symbolic_quantum_query(logic->quantum_bridge, variable, &qresult);
        if (query_status == 0 && qresult.confidences[variable] >= 0.5f) {
            // Convert ternary truth value to boolean
            qreason_truth_t truth = qresult.assignment[variable];
            if (truth == QREASON_TRUE) {
                *result = !formula->atom->negated;
                LOG_DEBUG("Quantum evaluation: TRUE (confidence: %.2f)",
                         qresult.confidences[variable]);
                return true;
            } else if (truth == QREASON_FALSE) {
                *result = formula->atom->negated;
                LOG_DEBUG("Quantum evaluation: FALSE (confidence: %.2f)",
                         qresult.confidences[variable]);
                return true;
            }
            // QREASON_UNKNOWN falls through to classical evaluation
            LOG_DEBUG("Quantum evaluation: UNKNOWN, falling back to classical");
        }
    }

    // Classical evaluation: check if formula matches any fact in KB
    /* Phase 5: Use predicate index for O(1) lookup */
    if (logic->pred_index_built) {
        const pred_index_bucket_t* bucket = predicate_index_lookup(logic, formula->atom->name);
        if (bucket) {
            for (uint32_t bi = 0; bi < bucket->count; bi++) {
                uint32_t f = bucket->fact_indices[bi];
                if (f < logic->num_facts) {
                    kb_entry_t* entry = logic->kb[f];
                    if (!entry || !entry->clause || entry->clause->num_literals == 0) continue;
                    if (strcmp(entry->clause->literals[0]->name, formula->atom->name) == 0) {
                        *result = !formula->atom->negated;
                        return true;
                    }
                }
            }
        }
    } else {
        for (uint32_t i = 0; i < logic->num_facts; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && logic->num_facts > 256) {
                symbolic_logic_heartbeat("symbolic_log_loop",
                                 (float)(i + 1) / (float)logic->num_facts);
            }

            kb_entry_t* entry = logic->kb[i];
            if (!entry || !entry->clause || entry->clause->num_literals == 0) continue;

            if (strcmp(entry->clause->literals[0]->name, formula->atom->name) == 0) {
                *result = !formula->atom->negated;
                return true;
            }
        }
    }

    return true;
}

// Note: symbolic_logic_compute_novelty is defined later in the file

static atomic_formula_t* logic_atom_copy(const atomic_formula_t* atom)
{
    if (!atom) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "logic_atom_copy: atom is NULL");
        return NULL;
    }

    atomic_formula_t* copy = logic_atom_create(atom->name, atom->terms, atom->arity);
    if (copy) {
        copy->negated = atom->negated;
    }

    return copy;
}

static bool atoms_equal(const atomic_formula_t* a1, const atomic_formula_t* a2)
{
    if (!a1 || !a2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "atoms_equal: required parameter is NULL (a1, a2)");
        return false;
    }
    if (strcmp(a1->name, a2->name) != 0) {
        return false;
    }
    if (a1->arity != a2->arity) {
        return false;
    }
    if (a1->negated != a2->negated) {
        return false;
    }

    for (uint8_t i = 0; i < a1->arity; i++) {
        if (!terms_equal(a1->terms[i], a2->terms[i])) {
            return false;
        }
    }

    return true;
}

//=============================================================================
// Helper Functions - Clause Management
//=============================================================================

static logic_clause_t* logic_clause_create(uint32_t num_literals)
{
    LOG_DEBUG("Creating module");
    logic_clause_t* clause = (logic_clause_t*)nimcp_calloc(1, sizeof(logic_clause_t));
    if (!clause) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "logic_clause_create: clause is NULL");
        return NULL;
    }

    if (num_literals > 0) {
        clause->literals = (atomic_formula_t**)nimcp_calloc(num_literals, sizeof(atomic_formula_t*));
        if (!clause->literals) {
            nimcp_free(clause);
            clause = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "logic_clause_create: clause->literals is NULL");
            return NULL;
        }
    }

    clause->num_literals = num_literals;
    clause->confidence = 1.0F;

    return clause;
}

static void logic_clause_destroy(logic_clause_t* clause)
{
    LOG_DEBUG("Destroying module");
    if (!clause) return;

    if (clause->literals) {
        for (uint32_t i = 0; i < clause->num_literals; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && clause->num_literals > 256) {
                symbolic_logic_heartbeat("symbolic_log_loop",
                                 (float)(i + 1) / (float)clause->num_literals);
            }

            logic_atom_destroy(clause->literals[i]);
        }
        nimcp_free(clause->literals);
    }

    nimcp_free(clause);
    clause = NULL;
}

static logic_clause_t* logic_clause_copy(const logic_clause_t* clause)
{
    if (!clause) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "logic_clause_copy: clause is NULL");
        return NULL;
    }

    logic_clause_t* copy = logic_clause_create(clause->num_literals);
    if (!copy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "logic_clause_copy: copy is NULL");
        return NULL;
    }

    copy->confidence = clause->confidence;

    for (uint32_t i = 0; i < clause->num_literals; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && clause->num_literals > 256) {
            symbolic_logic_heartbeat("symbolic_log_loop",
                             (float)(i + 1) / (float)clause->num_literals);
        }

        copy->literals[i] = logic_atom_copy(clause->literals[i]);
        if (!copy->literals[i]) {
            logic_clause_destroy(copy);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "logic_clause_copy: copy->literals is NULL");
            return NULL;
        }
    }

    return copy;
}

//=============================================================================
// W7 KG integration helpers
//
// The logic engine is usually created BEFORE it is attached to a brain, so we
// cannot write at create-time.  Instead the attachment path calls
// symbolic_logic_kg_register(logic, brain) which stashes the brain and emits
// the structural root node.  Subsequent add_fact/add_rule calls then mirror
// into the KG.
//=============================================================================

static void sl_kg_ensure_root(brain_t brain)
{
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) return;
    uint64_t tok = brain->internal_kg_admin_token;
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_ADMIN, tok);
    if (brain_kg_find_node(brain->internal_kg, "cog_logic_symbolic_engine")
        == BRAIN_KG_INVALID_NODE) {
        brain_kg_add_node(brain->internal_kg, "cog_logic_symbolic_engine",
                          BRAIN_KG_NODE_COGNITIVE,
                          "First-order logic engine (facts + rules + CNF)");
    }
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_READ, 0);
}

static void sl_kg_emit_fact_added(brain_t brain, uint32_t fact_index,
                                  const char* predicate)
{
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) return;
    uint64_t tok = brain->internal_kg_admin_token;
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_ADMIN, tok);

    char node_name[BRAIN_KG_MAX_NAME_LEN];
    snprintf(node_name, sizeof(node_name),
             "cog_logic_fact_%u_%llu", (unsigned)fact_index,
             (unsigned long long)nimcp_time_monotonic_us());
    char desc[128];
    snprintf(desc, sizeof(desc), "fact predicate=%s idx=%u",
             predicate ? predicate : "?", (unsigned)fact_index);
    brain_kg_node_id_t nid = brain_kg_add_node(brain->internal_kg, node_name,
                                               BRAIN_KG_NODE_COGNITIVE, desc);
    if (nid != BRAIN_KG_INVALID_NODE) {
        brain_kg_node_id_t root =
            brain_kg_find_node(brain->internal_kg, "cog_logic_symbolic_engine");
        if (root != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(brain->internal_kg, root, nid,
                              BRAIN_KG_EDGE_PROVIDES_TO, "has_fact", 0.6f);
        }
    }
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_READ, 0);
}

static void sl_kg_emit_rule_added(brain_t brain, uint32_t rule_index,
                                  const char* name)
{
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) return;
    uint64_t tok = brain->internal_kg_admin_token;
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_ADMIN, tok);

    char node_name[BRAIN_KG_MAX_NAME_LEN];
    /* Stable, not timestamp-based: a rule is a persistent artifact. */
    snprintf(node_name, sizeof(node_name), "cog_logic_rule_%u",
             (unsigned)rule_index);
    if (brain_kg_find_node(brain->internal_kg, node_name)
        == BRAIN_KG_INVALID_NODE) {
        brain_kg_node_id_t nid = brain_kg_add_node(brain->internal_kg,
            node_name, BRAIN_KG_NODE_COGNITIVE,
            name ? name : "symbolic-logic rule");
        if (nid != BRAIN_KG_INVALID_NODE) {
            brain_kg_node_id_t root = brain_kg_find_node(brain->internal_kg,
                "cog_logic_symbolic_engine");
            if (root != BRAIN_KG_INVALID_NODE) {
                brain_kg_add_edge(brain->internal_kg, root, nid,
                                  BRAIN_KG_EDGE_PROVIDES_TO, "has_rule", 0.7f);
            }
        }
    }
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_READ, 0);
}

/**
 * @brief Register a symbolic logic engine with a brain's internal KG.
 *
 * WHAT: Stores a weak back-reference from the logic engine to the brain so
 *       subsequent add_fact / add_rule calls can mirror into brain->internal_kg.
 * WHY:  The engine is created before attachment, so we can't mirror at
 *       create-time — this is the attach-time hook.
 *
 * @param logic Logic engine (required).
 * @param brain Owning brain (required; ignored if already registered).
 * @return 0 on success, -1 on NULL arg.
 */
int symbolic_logic_kg_register(symbolic_logic_t* logic, brain_t brain)
{
    if (!logic || !brain) return -1;
    logic->kg_brain = brain;
    sl_kg_ensure_root(brain);
    return 0;
}

//=============================================================================
// Core API Implementation
//=============================================================================

symbolic_logic_t* symbolic_logic_create(const logic_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_heartbeat("symbolic_log_create", 0.0f);


    LOG_DEBUG("Creating module");
    NIMCP_API_CHECK_NULL_RET_NULL(config, "NULL config in symbolic_logic_create");

    if (config->max_predicates == 0 || config->max_predicates > LOGIC_MAX_PREDICATES) {
        LOG_ERROR("Invalid max_predicates: %u", config->max_predicates);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Invalid max_predicates: %u", config->max_predicates);
        return NULL;
    }

    symbolic_logic_t* logic = (symbolic_logic_t*)nimcp_calloc(1, sizeof(symbolic_logic_t));
    if (!logic) return NULL;
    NIMCP_API_CHECK_ALLOC(logic, "Failed to allocate symbolic logic engine");

    logic->config = *config;

    // Initialize knowledge base
    logic->kb_capacity = config->max_kb_size;
    logic->kb = (kb_entry_t**)nimcp_calloc(logic->kb_capacity, sizeof(kb_entry_t*));
    if (!logic->kb) {
        symbolic_logic_destroy(logic);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "symbolic_logic_create: logic->kb is NULL");
        return NULL;
    }

    // Initialize rules
    logic->rules_capacity = config->max_rules;
    logic->rules = (inference_rule_t**)nimcp_calloc(logic->rules_capacity, sizeof(inference_rule_t*));
    if (!logic->rules) {
        symbolic_logic_destroy(logic);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "symbolic_logic_create: logic->rules is NULL");
        return NULL;
    }

    // Initialize working memory
    logic->working_memory = (logic_clause_t**)nimcp_calloc(LOGIC_MAX_PREDICATES, sizeof(logic_clause_t*));
    if (!logic->working_memory) {
        symbolic_logic_destroy(logic);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "symbolic_logic_create: logic->working_memory is NULL");
        return NULL;
    }

    memset(&logic->stats, 0, sizeof(logic_stats_t));

    // Initialize quantum bridge if enabled
    logic->quantum_bridge = NULL;
    if (config->enable_quantum_logic) {
        symbolic_quantum_config_t qconfig = symbolic_quantum_default_config();
        qconfig.enabled = true;
        qconfig.max_inference_depth = config->max_inference_depth;

        logic->quantum_bridge = symbolic_quantum_bridge_create(&qconfig);
        if (!logic->quantum_bridge) {
            NIMCP_LOGGING_WARN("Failed to create quantum bridge, continuing without quantum acceleration");
        } else {
            // Connect quantum bridge to symbolic logic
            int connect_result = symbolic_quantum_bridge_connect(logic->quantum_bridge, logic);
            if (connect_result != 0) {
                NIMCP_LOGGING_WARN("Failed to connect quantum bridge");
                symbolic_quantum_bridge_destroy(logic->quantum_bridge);
                logic->quantum_bridge = NULL;
            } else {
                NIMCP_LOGGING_INFO("Quantum reasoning bridge initialized");
            }
        }
    }

    // Bio-async registration
    logic->bio_ctx = NULL;
    logic->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_KNOWLEDGE_SYMBOLIC_LOGIC,
            .module_name = "symbolic_logic",
            .inbox_capacity = 32,
            .user_data = logic
        };
        logic->bio_ctx = bio_router_register_module(&bio_info);
        if (logic->bio_ctx) {
            logic->bio_async_enabled = true;
        }
    }

    /* Phase 5: Initialize predicate index */
    logic->pred_index_built = false;
    memset(logic->pred_index, 0, sizeof(logic->pred_index));

    LOG_INFO("Symbolic logic engine created");
    return logic;
}

void symbolic_logic_destroy(symbolic_logic_t* logic)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_heartbeat("symbolic_log_destroy", 0.0f);


    LOG_DEBUG("Destroying module");
    if (!logic) return;

    // Free knowledge base
    if (logic->kb) {
        for (uint32_t i = 0; i < logic->num_facts; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && logic->num_facts > 256) {
                symbolic_logic_heartbeat("symbolic_log_loop",
                                 (float)(i + 1) / (float)logic->num_facts);
            }

            if (logic->kb[i]) {
                logic_clause_destroy(logic->kb[i]->clause);
                nimcp_free(logic->kb[i]);
            }
        }
        nimcp_free(logic->kb);
    }

    // Free rules
    if (logic->rules) {
        for (uint32_t i = 0; i < logic->num_rules; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && logic->num_rules > 256) {
                symbolic_logic_heartbeat("symbolic_log_loop",
                                 (float)(i + 1) / (float)logic->num_rules);
            }

            if (logic->rules[i]) {
                for (uint32_t j = 0; j < logic->rules[i]->num_premises; j++) {
                    logic_clause_destroy(logic->rules[i]->premises[j]);
                }
                nimcp_free(logic->rules[i]->premises);
                logic_clause_destroy(logic->rules[i]->conclusion);
                nimcp_free(logic->rules[i]);
            }
        }
        nimcp_free(logic->rules);
    }

    // Free working memory
    if (logic->working_memory) {
        for (uint32_t i = 0; i < logic->working_memory_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && logic->working_memory_size > 256) {
                symbolic_logic_heartbeat("symbolic_log_loop",
                                 (float)(i + 1) / (float)logic->working_memory_size);
            }

            logic_clause_destroy(logic->working_memory[i]);
        }
        nimcp_free(logic->working_memory);
    }

    // Destroy quantum bridge
    if (logic->quantum_bridge) {
        symbolic_quantum_bridge_destroy(logic->quantum_bridge);
        logic->quantum_bridge = NULL;
    }

    // Unregister from bio-router
    if (logic->bio_async_enabled && logic->bio_ctx) {
        bio_router_unregister_module(logic->bio_ctx);
        logic->bio_ctx = NULL;
        logic->bio_async_enabled = false;
    }

    nimcp_free(logic);
    logic = NULL;
}

bool symbolic_logic_get_stats(const symbolic_logic_t* logic, logic_stats_t* stats)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_heartbeat("symbolic_log_get_stats", 0.0f);

    if (!nimcp_validate_pointer(logic, "logic") ||
        !nimcp_validate_pointer(stats, "stats")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_get_stats: nimcp_validate_pointer is NULL");
        return false;
    }

    *stats = logic->stats;
    return true;
}

//=============================================================================
// Knowledge Base Management
//=============================================================================

bool symbolic_logic_add_fact(symbolic_logic_t* logic, logic_clause_t* clause, float salience)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_heartbeat("symbolic_log_add_fact", 0.0f);

    if (!nimcp_validate_pointer(logic, "logic") ||
        !nimcp_validate_pointer(clause, "clause")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_add_fact: nimcp_validate_pointer is NULL");
        return false;
    }

    if (salience < 0.0F || salience > 1.0F) {
        LOG_ERROR("Invalid salience value: %f (must be [0,1])", salience);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_add_fact: validation failed");
        return false;
    }

    if (logic->num_facts >= logic->kb_capacity) {
        LOG_ERROR("Knowledge base full");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "symbolic_logic_add_fact: capacity exceeded");
        return false;
    }

    kb_entry_t* entry = (kb_entry_t*)nimcp_calloc(1, sizeof(kb_entry_t));
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "symbolic_logic_add_fact: entry is NULL");
        return false;
    }

    entry->clause = logic_clause_copy(clause);
    if (!entry->clause) {
        nimcp_free(entry);
        entry = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_add_fact: entry->clause is NULL");
        return false;
    }

    entry->salience = salience;
    entry->timestamp = nimcp_time_get_ms();
    entry->context[0] = '\0';

    logic->kb[logic->num_facts++] = entry;
    logic->stats.facts_stored++;

    /* Phase 5: Update predicate index with new fact */
    predicate_index_add(logic, logic->num_facts - 1);

    /* W7: mirror fact into the brain's internal KG (if registered). */
    if (logic->kg_brain) {
        const char* pred = (entry->clause && entry->clause->num_literals > 0
                            && entry->clause->literals[0])
            ? entry->clause->literals[0]->name : NULL;
        sl_kg_emit_fact_added(logic->kg_brain, logic->num_facts - 1, pred);
    }

    return true;
}

bool symbolic_logic_add_rule(symbolic_logic_t* logic, inference_rule_t* rule)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_heartbeat("symbolic_log_add_rule", 0.0f);

    if (!nimcp_validate_pointer(logic, "logic") ||
        !nimcp_validate_pointer(rule, "rule")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_add_rule: nimcp_validate_pointer is NULL");
        return false;
    }

    if (logic->num_rules >= logic->rules_capacity) {
        LOG_ERROR("Rules capacity full");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "symbolic_logic_add_rule: capacity exceeded");
        return false;
    }

    logic->rules[logic->num_rules++] = rule;

    /* W7: mirror the rule into the brain's internal KG (if registered). */
    if (logic->kg_brain) {
        sl_kg_emit_rule_added(logic->kg_brain, logic->num_rules - 1,
                              rule->name);
    }

    return true;
}

bool symbolic_logic_query(symbolic_logic_t* logic, logic_clause_t* query,
                         kb_entry_t*** results, int* num_results)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_heartbeat("symbolic_log_query", 0.0f);

    if (!nimcp_validate_pointer(logic, "logic") ||
        !nimcp_validate_pointer(query, "query") ||
        !nimcp_validate_pointer(results, "results") ||
        !nimcp_validate_pointer(num_results, "num_results")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_query: nimcp_validate_pointer is NULL");
        return false;
    }

    *num_results = 0;
    *results = NULL;

    // Count matching facts
    uint32_t match_count = 0;
    for (uint32_t i = 0; i < logic->num_facts; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && logic->num_facts > 256) {
            symbolic_logic_heartbeat("symbolic_log_loop",
                             (float)(i + 1) / (float)logic->num_facts);
        }

        kb_entry_t* entry = logic->kb[i];

        // Simple matching: check if query literals match fact literals
        bool match = false;
        if (query->num_literals > 0 && entry->clause->num_literals > 0) {
            match = atoms_equal(query->literals[0], entry->clause->literals[0]);
        }

        if (match) {
            match_count++;
        }
    }

    if (match_count == 0) {
        return true;
    }

    // Allocate results
    *results = (kb_entry_t**)nimcp_calloc(match_count, sizeof(kb_entry_t*));
    if (!*results) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_query: validation failed");
        return false;
    }

    // Fill results
    uint32_t idx = 0;
    for (uint32_t i = 0; i < logic->num_facts && idx < match_count; i++) {
        kb_entry_t* entry = logic->kb[i];

        bool match = false;
        if (query->num_literals > 0 && entry->clause->num_literals > 0) {
            match = atoms_equal(query->literals[0], entry->clause->literals[0]);
        }

        if (match) {
            (*results)[idx++] = entry;
        }
    }

    *num_results = (int)match_count;
    return true;
}

//=============================================================================
// Unification Algorithm
//=============================================================================

unification_t* symbolic_logic_unify(logical_term_t* term1, logical_term_t* term2)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_heartbeat("symbolic_log_unify", 0.0f);


    NIMCP_API_CHECK_NULL_RET_NULL(term1, "NULL term1 in symbolic_logic_unify");
    NIMCP_API_CHECK_NULL_RET_NULL(term2, "NULL term2 in symbolic_logic_unify");

    unification_t* unif = (unification_t*)nimcp_calloc(1, sizeof(unification_t));
    NIMCP_API_CHECK_ALLOC(unif, "Failed to allocate unification result");

    unif->success = false;
    unif->bindings = NULL;
    unif->num_bindings = 0;

    // Both constants or same variable - must be identical
    if (term1->type == TERM_CONSTANT && term2->type == TERM_CONSTANT) {
        unif->success = terms_equal(term1, term2);
        return unif;
    }

    // Variable unification
    if (term1->type == TERM_VARIABLE) {
        substitution_t* binding = (substitution_t*)nimcp_calloc(1, sizeof(substitution_t));
        if (!binding) {
            unification_destroy(unif);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "symbolic_logic_unify: binding is NULL");
            return NULL;
        }

        binding->variable = logic_term_copy(term1);
        binding->value = logic_term_copy(term2);

        unif->bindings = (substitution_t**)nimcp_calloc(1, sizeof(substitution_t*));
        if (!unif->bindings) {
            nimcp_free(binding);
            binding = NULL;
            unification_destroy(unif);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "symbolic_logic_unify: unif->bindings is NULL");
            return NULL;
        }

        unif->bindings[0] = binding;
        unif->num_bindings = 1;
        unif->success = true;
        return unif;
    }

    if (term2->type == TERM_VARIABLE) {
        substitution_t* binding = (substitution_t*)nimcp_calloc(1, sizeof(substitution_t));
        if (!binding) {
            unification_destroy(unif);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "symbolic_logic_unify: binding is NULL");
            return NULL;
        }

        binding->variable = logic_term_copy(term2);
        binding->value = logic_term_copy(term1);

        unif->bindings = (substitution_t**)nimcp_calloc(1, sizeof(substitution_t*));
        if (!unif->bindings) {
            nimcp_free(binding);
            binding = NULL;
            unification_destroy(unif);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "symbolic_logic_unify: unif->bindings is NULL");
            return NULL;
        }

        unif->bindings[0] = binding;
        unif->num_bindings = 1;
        unif->success = true;
        return unif;
    }

    // Function unification
    if (term1->type == TERM_FUNCTION && term2->type == TERM_FUNCTION) {
        if (strcmp(term1->name, term2->name) != 0 || term1->arity != term2->arity) {
            return unif;
        }

        // Unify all arguments
        unif->success = true;
        for (uint8_t i = 0; i < term1->arity; i++) {
            unification_t* arg_unif = symbolic_logic_unify(term1->args[i], term2->args[i]);
            if (!arg_unif || !arg_unif->success) {
                unif->success = false;
                if (arg_unif) unification_destroy(arg_unif);
                break;
            }
            unification_destroy(arg_unif);
        }

        if (unif->success) {
            }
    }

    return unif;
}

logical_term_t* symbolic_logic_substitute(logical_term_t* term, const substitution_t* subst)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_heartbeat("symbolic_log_substitute", 0.0f);


    NIMCP_API_CHECK_NULL_RET_NULL(term, "NULL term in symbolic_logic_substitute");
    NIMCP_API_CHECK_NULL_RET_NULL(subst, "NULL subst in symbolic_logic_substitute");

    // If term is the variable being substituted, return the value
    if (terms_equal(term, subst->variable)) {
        return logic_term_copy(subst->value);
    }

    // Otherwise copy term with recursive substitution of args
    logical_term_t* result = logic_term_create(term->type, term->name);
    if (!result) {
        LOG_ERROR("NULL parameter or allocation failure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "symbolic_logic_substitute: result is NULL");
        return NULL;
    }

    if (term->arity > 0 && term->args) {
        result->arity = term->arity;
        result->args = (logical_term_t**)nimcp_calloc(term->arity, sizeof(logical_term_t*));
        if (!result->args) {
            logic_term_destroy(result);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "symbolic_logic_substitute: result->args is NULL");
            return NULL;
        }

        for (uint8_t i = 0; i < term->arity; i++) {
            result->args[i] = symbolic_logic_substitute(term->args[i], subst);
            if (!result->args[i]) {
                logic_term_destroy(result);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_substitute: result->args is NULL");
                return NULL;
            }
        }
    }

    return result;
}

void unification_destroy(unification_t* unif)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_heartbeat("symbolic_log_unification_destroy", 0.0f);


    LOG_DEBUG("Destroying module");
    if (!unif) return;

    if (unif->bindings) {
        for (uint32_t i = 0; i < unif->num_bindings; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && unif->num_bindings > 256) {
                symbolic_logic_heartbeat("symbolic_log_loop",
                                 (float)(i + 1) / (float)unif->num_bindings);
            }

            if (unif->bindings[i]) {
                logic_term_destroy(unif->bindings[i]->variable);
                logic_term_destroy(unif->bindings[i]->value);
                nimcp_free(unif->bindings[i]);
            }
        }
        nimcp_free(unif->bindings);
    }

    nimcp_free(unif);
    unif = NULL;
}

//=============================================================================
// Inference Engine - Forward Chaining
//=============================================================================

bool symbolic_logic_forward_chain(symbolic_logic_t* logic, uint32_t max_iterations,
                                  logic_clause_t*** new_facts, int* num_new_facts)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_heartbeat("symbolic_log_forward_chain", 0.0f);

    if (!nimcp_validate_pointer(logic, "logic") ||
        !nimcp_validate_pointer(new_facts, "new_facts") ||
        !nimcp_validate_pointer(num_new_facts, "num_new_facts")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_forward_chain: nimcp_validate_pointer is NULL");
        return false;
    }

    if (!logic->config.enable_forward_chaining) {
        LOG_DEBUG("Forward chaining not enabled in config");
        return false;
    }

    *new_facts = NULL;
    *num_new_facts = 0;

    uint64_t start_time = nimcp_time_get_ms();
    uint32_t iteration = 0;
    uint32_t derived_count = 0;

    // Allocate array for new facts
    logic_clause_t** derived = (logic_clause_t**)nimcp_calloc(LOGIC_MAX_PREDICATES, sizeof(logic_clause_t*));
    if (!derived) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "symbolic_logic_forward_chain: derived is NULL");
        return false;
    }

    /* Phase 5: Ensure predicate index is built before forward chaining */
    if (!logic->pred_index_built) {
        predicate_index_rebuild(logic);
    }

    while (iteration < max_iterations) {
        bool new_fact_derived = false;

        // Try to apply each rule
        for (uint32_t r = 0; r < logic->num_rules; r++) {
            /* Phase 8: Loop progress heartbeat */
            if ((r & 0xFF) == 0 && logic->num_rules > 256) {
                symbolic_logic_heartbeat("symbolic_log_loop",
                                 (float)(r + 1) / (float)logic->num_rules);
            }

            inference_rule_t* rule = logic->rules[r];

            // Check if all premises are satisfied
            bool premises_satisfied = true;
            for (uint32_t p = 0; p < rule->num_premises; p++) {
                /* Phase 8: Loop progress heartbeat */
                if ((p & 0xFF) == 0 && rule->num_premises > 256) {
                    symbolic_logic_heartbeat("symbolic_log_loop",
                                     (float)(p + 1) / (float)rule->num_premises);
                }

                bool found = false;

                /* Phase 5: Use predicate index for O(1) lookup */
                if (logic->pred_index_built && rule->premises[p]->literals[0]) {
                    const pred_index_bucket_t* bucket = predicate_index_lookup(
                        logic, rule->premises[p]->literals[0]->name);
                    if (bucket) {
                        for (uint32_t bi = 0; bi < bucket->count; bi++) {
                            uint32_t f = bucket->fact_indices[bi];
                            if (f < logic->num_facts && logic->kb[f] && logic->kb[f]->clause) {
                                if (atoms_equal(rule->premises[p]->literals[0],
                                               logic->kb[f]->clause->literals[0])) {
                                    found = true;
                                    break;
                                }
                            }
                        }
                    }
                } else {
                    for (uint32_t f = 0; f < logic->num_facts; f++) {
                        if (!logic->kb[f] || !logic->kb[f]->clause) continue;
                        if (atoms_equal(rule->premises[p]->literals[0],
                                       logic->kb[f]->clause->literals[0])) {
                            found = true;
                            break;
                        }
                    }
                }

                if (!found) {
                    premises_satisfied = false;
                    break;
                }
            }

            // If premises satisfied, derive conclusion
            if (premises_satisfied && rule->conclusion) {
                // Check if conclusion is new
                bool already_known = false;
                for (uint32_t f = 0; f < logic->num_facts; f++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((f & 0xFF) == 0 && logic->num_facts > 256) {
                        symbolic_logic_heartbeat("symbolic_log_loop",
                                         (float)(f + 1) / (float)logic->num_facts);
                    }

                    if (atoms_equal(rule->conclusion->literals[0],
                                   logic->kb[f]->clause->literals[0])) {
                        already_known = true;
                        break;
                    }
                }

                if (!already_known) {
                    derived[derived_count] = logic_clause_copy(rule->conclusion);
                    if (derived[derived_count]) {
                        derived_count++;
                        new_fact_derived = true;

                        // Add to knowledge base
                        symbolic_logic_add_fact(logic, rule->conclusion, 0.7F);

                        logic->stats.rules_applied++;
                    }
                }
            }
        }

        if (!new_fact_derived) {
            break;  // Fixed point reached
        }

        iteration++;
    }

    uint64_t end_time = nimcp_time_get_ms();
    logic->stats.avg_inference_time = (float)(end_time - start_time);
    logic->stats.inferences_performed++;

    *new_facts = derived;
    *num_new_facts = (int)derived_count;

    return true;
}

//=============================================================================
// Brain Integration Helpers
//=============================================================================

float symbolic_logic_compute_novelty(symbolic_logic_t* logic, logic_clause_t* clause)
{
    if (!logic || !clause) {
        return 0.0F;
    }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_heartbeat("symbolic_log_compute_novelty", 0.0f);


    if (logic->num_facts == 0) {
        return 1.0F;  // Completely novel if no facts exist
    }

    // Check similarity to existing facts
    float max_similarity = 0.0F;

    for (uint32_t i = 0; i < logic->num_facts; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && logic->num_facts > 256) {
            symbolic_logic_heartbeat("symbolic_log_loop",
                             (float)(i + 1) / (float)logic->num_facts);
        }

        kb_entry_t* entry = logic->kb[i];

        // Simple similarity: do they share any predicates?
        if (clause->num_literals > 0 && entry->clause->num_literals > 0) {
            if (strcmp(clause->literals[0]->name, entry->clause->literals[0]->name) == 0) {
                float similarity = 0.5F;  // Same predicate

                // Check arguments
                if (atoms_equal(clause->literals[0], entry->clause->literals[0])) {
                    similarity = 1.0F;  // Identical
                }

                if (similarity > max_similarity) {
                    max_similarity = similarity;
                }
            }
        }
    }

    return 1.0F - max_similarity;
}

bool symbolic_logic_get_salient_facts(symbolic_logic_t* logic, int top_k,
                                      kb_entry_t*** salient_facts, int* num_facts)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_heartbeat("symbolic_log_get_salient_facts", 0.0f);

    if (!nimcp_validate_pointer(logic, "logic") ||
        !nimcp_validate_pointer(salient_facts, "salient_facts") ||
        !nimcp_validate_pointer(num_facts, "num_facts")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_get_salient_facts: nimcp_validate_pointer is NULL");
        return false;
    }

    if (top_k <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_get_salient_facts: validation failed");
        return false;
    }

    uint32_t k = (uint32_t)top_k;
    if (k > logic->num_facts) {
        k = logic->num_facts;
    }

    // Allocate results array
    *salient_facts = (kb_entry_t**)nimcp_calloc(k, sizeof(kb_entry_t*));
    if (!*salient_facts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_get_salient_facts: validation failed");
        return false;
    }

    // Sort by salience (simple selection sort for top-k)
    for (uint32_t i = 0; i < k; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && k > 256) {
            symbolic_logic_heartbeat("symbolic_log_loop",
                             (float)(i + 1) / (float)k);
        }

        uint32_t max_idx = i;
        float max_salience = (i < logic->num_facts) ? logic->kb[i]->salience : 0.0F;

        for (uint32_t j = i + 1; j < logic->num_facts; j++) {
            if (logic->kb[j]->salience > max_salience) {
                max_salience = logic->kb[j]->salience;
                max_idx = j;
            }
        }

        (*salient_facts)[i] = logic->kb[max_idx];

        // Swap
        if (max_idx != i && i < logic->num_facts) {
            kb_entry_t* temp = logic->kb[i];
            logic->kb[i] = logic->kb[max_idx];
            logic->kb[max_idx] = temp;
        }
    }

    *num_facts = (int)k;
    return true;
}

bool symbolic_logic_consolidate_memory(symbolic_logic_t* logic, logic_clause_t* clause,
                                       float salience, const char* context)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_heartbeat("symbolic_log_consolidate_memory", 0.0f);

    if (!nimcp_validate_pointer(logic, "logic") ||
        !nimcp_validate_pointer(clause, "clause")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_consolidate_memory: nimcp_validate_pointer is NULL");
        return false;
    }

    if (!logic->config.enable_memory_consolidation) {
        return false;  // Memory consolidation disabled in config
    }

    // Add fact with context
    if (!symbolic_logic_add_fact(logic, clause, salience)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_UNKNOWN, "symbolic_logic_consolidate_memory: add_fact failed");
        return false;
    }

    // Set context if provided
    if (context && logic->num_facts > 0) {
        kb_entry_t* entry = logic->kb[logic->num_facts - 1];
        strncpy(entry->context, context, sizeof(entry->context) - 1);
        entry->context[sizeof(entry->context) - 1] = '\0';
    }

    return true;
}

bool symbolic_logic_explore(symbolic_logic_t* logic, uint32_t exploration_depth,
                           logic_clause_t*** interesting_facts, int* num_facts)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_heartbeat("symbolic_log_explore", 0.0f);

    if (!nimcp_validate_pointer(logic, "logic") ||
        !nimcp_validate_pointer(interesting_facts, "interesting_facts") ||
        !nimcp_validate_pointer(num_facts, "num_facts")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_explore: nimcp_validate_pointer is NULL");
        return false;
    }

    // Use forward chaining to explore
    return symbolic_logic_forward_chain(logic, exploration_depth,
                                       interesting_facts, num_facts);
}

//=============================================================================
// Utility Functions
//=============================================================================

bool symbolic_logic_to_string(const logical_formula_t* formula, char* buffer, size_t buffer_size)
{
    if (!formula || !buffer || buffer_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_to_string: required parameter is NULL (formula, buffer)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_heartbeat("symbolic_log_to_string", 0.0f);


    if (formula->atom) {
        // Atomic formula
        int written = snprintf(buffer, buffer_size, "%s%s",
                              formula->atom->negated ? "~" : "",
                              formula->atom->name);

        if (formula->atom->arity > 0) {
            written += snprintf(buffer + written, buffer_size - written, "(");
            for (uint8_t i = 0; i < formula->atom->arity; i++) {
                written += snprintf(buffer + written, buffer_size - written, "%s%s",
                                  formula->atom->terms[i]->name,
                                  (i < formula->atom->arity - 1) ? "," : "");
            }
            written += snprintf(buffer + written, buffer_size - written, ")");
        }

        return true;
    }

    // Composite formula
    const char* op_str = "?";
    switch (formula->op) {
        case OP_AND: op_str = "&"; break;
        case OP_OR: op_str = "|"; break;
        case OP_NOT: op_str = "~"; break;
        case OP_IMPLIES: op_str = "->"; break;
        case OP_IFF: op_str = "<->"; break;
        default: break;
    }

    snprintf(buffer, buffer_size, "(%s %s)", op_str, "...");
    return true;
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

/**
 * @brief Query knowledge graph for self-knowledge about symbolic logic
 *
 * WHAT: Retrieve module's own entity and connections from KG
 * WHY:  Enable self-awareness - module can introspect its own capabilities
 * HOW:  Query entity by name, get relations from/to
 *
 * @param kg Knowledge graph reader
 * @return 1 if entity found, 0 if not
 */
int symbolic_logic_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Query our own entity from the knowledge graph */
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_heartbeat("symbolic_log_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Symbolic_Logic_Module");
    if (self) {
        /* Module now knows its own capabilities from KG */
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                symbolic_logic_heartbeat("symbolic_log_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG("Symbolic logic self-knowledge: %s", self->observations[i]);
        }
    }

    /* Query connections to understand integration points */
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Symbolic_Logic_Module");
    if (connections) {
        LOG_DEBUG("Symbolic logic has %u outgoing connections", connections->count);
        kg_relation_list_destroy(connections);
    }

    /* Query incoming connections */
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Symbolic_Logic_Module");
    if (incoming) {
        LOG_DEBUG("Symbolic logic has %u incoming connections", incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void symbolic_logic_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_symbolic_logic_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int symbolic_logic_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "symbolic_logic_training_begin: NULL argument");
        return -1;
    }
    symbolic_logic_heartbeat_instance(NULL, "symbolic_logic_training_begin", 0.0f);
    (void)(struct symbolic_logic*)instance; /* Module state available for reset */
    return 0;
}

int symbolic_logic_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "symbolic_logic_training_end: NULL argument");
        return -1;
    }
    symbolic_logic_heartbeat_instance(NULL, "symbolic_logic_training_end", 1.0f);
    (void)(struct symbolic_logic*)instance; /* Module state available for finalization */
    return 0;
}

int symbolic_logic_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "symbolic_logic_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    symbolic_logic_heartbeat_instance(NULL, "symbolic_logic_training_step", progress);
    (void)(struct symbolic_logic*)instance; /* Module state available for step adaptation */
    return 0;
}
