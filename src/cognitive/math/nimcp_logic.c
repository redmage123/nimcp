/**
 * @file nimcp_logic.c
 * @brief Propositional and predicate logic engine implementation
 */

#include "cognitive/math/nimcp_logic.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "LOGIC"

/* ================================================================
 * LIFECYCLE
 * ================================================================ */

logic_engine_t* logic_engine_create(void) {
    logic_engine_t* eng = (logic_engine_t*)nimcp_calloc(1, sizeof(logic_engine_t));
    if (!eng) {
        LOG_ERROR(LOG_TAG, "Failed to allocate logic engine");
        return NULL;
    }
    LOG_INFO(LOG_TAG, "Logic engine created (max %d variables, depth %d)",
             LOGIC_MAX_VARIABLES, LOGIC_MAX_DEPTH);
    return eng;
}

void logic_engine_destroy(logic_engine_t* eng) {
    if (!eng) return;
    nimcp_free(eng);
}

/* ================================================================
 * FORMULA CONSTRUCTION
 * ================================================================ */

static logic_formula_t* formula_alloc(void) {
    logic_formula_t* f = (logic_formula_t*)nimcp_calloc(1, sizeof(logic_formula_t));
    return f;
}

logic_formula_t* logic_var(int var_index) {
    if (var_index < 0 || var_index >= LOGIC_MAX_VARIABLES) return NULL;
    logic_formula_t* f = formula_alloc();
    if (!f) return NULL;
    f->op = LOGIC_OP_VAR;
    f->var_index = var_index;
    f->depth = 0;
    return f;
}

logic_formula_t* logic_const(bool value) {
    logic_formula_t* f = formula_alloc();
    if (!f) return NULL;
    f->op = LOGIC_OP_CONST;
    f->const_value = value;
    f->depth = 0;
    return f;
}

logic_formula_t* logic_not(logic_formula_t* child) {
    if (!child) return NULL;
    if (child->depth + 1 > LOGIC_MAX_DEPTH) {
        LOG_WARN(LOG_TAG, "Formula depth exceeded maximum %d", LOGIC_MAX_DEPTH);
        return NULL;
    }
    logic_formula_t* f = formula_alloc();
    if (!f) return NULL;
    f->op = LOGIC_OP_NOT;
    f->left = child;
    f->depth = child->depth + 1;
    return f;
}

logic_formula_t* logic_binary(logic_op_t op, logic_formula_t* left, logic_formula_t* right) {
    if (!left || !right) return NULL;
    int max_d = (left->depth > right->depth) ? left->depth : right->depth;
    if (max_d + 1 > LOGIC_MAX_DEPTH) {
        LOG_WARN(LOG_TAG, "Formula depth exceeded maximum %d", LOGIC_MAX_DEPTH);
        return NULL;
    }
    logic_formula_t* f = formula_alloc();
    if (!f) return NULL;
    f->op = op;
    f->left = left;
    f->right = right;
    f->depth = max_d + 1;
    return f;
}

void logic_formula_free(logic_formula_t* f) {
    if (!f) return;
    logic_formula_free(f->left);
    logic_formula_free(f->right);
    nimcp_free(f);
}

logic_formula_t* logic_formula_clone(const logic_formula_t* f) {
    if (!f) return NULL;
    logic_formula_t* c = formula_alloc();
    if (!c) return NULL;
    c->op = f->op;
    c->var_index = f->var_index;
    c->const_value = f->const_value;
    c->depth = f->depth;
    c->left = logic_formula_clone(f->left);
    c->right = logic_formula_clone(f->right);
    return c;
}

/* ================================================================
 * EVALUATION
 * ================================================================ */

bool logic_evaluate(const logic_formula_t* f, const bool* assignment) {
    if (!f) return false;
    switch (f->op) {
        case LOGIC_OP_VAR:
            return assignment[f->var_index];
        case LOGIC_OP_CONST:
            return f->const_value;
        case LOGIC_OP_NOT:
            return !logic_evaluate(f->left, assignment);
        case LOGIC_OP_AND:
            return logic_evaluate(f->left, assignment) && logic_evaluate(f->right, assignment);
        case LOGIC_OP_OR:
            return logic_evaluate(f->left, assignment) || logic_evaluate(f->right, assignment);
        case LOGIC_OP_IMPLIES: {
            bool p = logic_evaluate(f->left, assignment);
            bool q = logic_evaluate(f->right, assignment);
            return !p || q;
        }
        case LOGIC_OP_IFF: {
            bool p = logic_evaluate(f->left, assignment);
            bool q = logic_evaluate(f->right, assignment);
            return p == q;
        }
        case LOGIC_OP_XOR: {
            bool p = logic_evaluate(f->left, assignment);
            bool q = logic_evaluate(f->right, assignment);
            return p != q;
        }
    }
    return false;
}

/* ================================================================
 * TRUTH TABLE
 * ================================================================ */

truth_table_t* logic_truth_table(const logic_formula_t* f, int n_vars) {
    if (!f || n_vars <= 0 || n_vars > LOGIC_MAX_VARIABLES) return NULL;
    if (n_vars > 20) {
        LOG_WARN(LOG_TAG, "Truth table with %d variables would be too large", n_vars);
        return NULL;
    }

    int n_rows = 1 << n_vars;
    truth_table_t* tt = (truth_table_t*)nimcp_calloc(1, sizeof(truth_table_t));
    if (!tt) return NULL;

    tt->results = (bool*)nimcp_calloc((size_t)n_rows, sizeof(bool));
    if (!tt->results) {
        nimcp_free(tt);
        return NULL;
    }
    tt->n_variables = n_vars;
    tt->n_rows = n_rows;

    bool assignment[LOGIC_MAX_VARIABLES];
    memset(assignment, 0, sizeof(assignment));

    for (int row = 0; row < n_rows; row++) {
        for (int v = 0; v < n_vars; v++) {
            assignment[v] = (row >> (n_vars - 1 - v)) & 1;
        }
        tt->results[row] = logic_evaluate(f, assignment);
    }

    LOG_DEBUG(LOG_TAG, "Generated truth table: %d vars, %d rows", n_vars, n_rows);
    return tt;
}

void logic_truth_table_free(truth_table_t* tt) {
    if (!tt) return;
    nimcp_free(tt->results);
    nimcp_free(tt);
}

bool logic_is_tautology(const logic_formula_t* f, int n_vars) {
    truth_table_t* tt = logic_truth_table(f, n_vars);
    if (!tt) return false;
    bool result = true;
    for (int i = 0; i < tt->n_rows; i++) {
        if (!tt->results[i]) { result = false; break; }
    }
    logic_truth_table_free(tt);
    return result;
}

bool logic_is_satisfiable(const logic_formula_t* f, int n_vars) {
    truth_table_t* tt = logic_truth_table(f, n_vars);
    if (!tt) return false;
    bool result = false;
    for (int i = 0; i < tt->n_rows; i++) {
        if (tt->results[i]) { result = true; break; }
    }
    logic_truth_table_free(tt);
    return result;
}

bool logic_are_equivalent(const logic_formula_t* a, const logic_formula_t* b, int n_vars) {
    /* a ≡ b iff (a IFF b) is a tautology */
    logic_formula_t* a_clone = logic_formula_clone(a);
    logic_formula_t* b_clone = logic_formula_clone(b);
    logic_formula_t* iff = logic_binary(LOGIC_OP_IFF, a_clone, b_clone);
    bool result = logic_is_tautology(iff, n_vars);
    logic_formula_free(iff);
    return result;
}

/* ================================================================
 * CNF CONVERSION (via truth table -- negation of false rows)
 * ================================================================ */

cnf_formula_t* logic_to_cnf(const logic_formula_t* f, int n_vars) {
    if (!f || n_vars <= 0 || n_vars > 20) return NULL;

    truth_table_t* tt = logic_truth_table(f, n_vars);
    if (!tt) return NULL;

    cnf_formula_t* cnf = (cnf_formula_t*)nimcp_calloc(1, sizeof(cnf_formula_t));
    if (!cnf) { logic_truth_table_free(tt); return NULL; }
    cnf->n_variables = n_vars;

    /* Each false row produces a maxterm (clause) */
    for (int row = 0; row < tt->n_rows; row++) {
        if (tt->results[row]) continue;
        if (cnf->n_clauses >= LOGIC_MAX_CLAUSES) break;

        cnf_clause_t* cl = &cnf->clauses[cnf->n_clauses];
        cl->n_literals = 0;
        for (int v = 0; v < n_vars; v++) {
            bool val = (row >> (n_vars - 1 - v)) & 1;
            /* Maxterm: negate the variable if it was true in the false row */
            cl->literals[cl->n_literals++] = val ? -(v + 1) : (v + 1);
        }
        cnf->n_clauses++;
    }

    logic_truth_table_free(tt);
    LOG_DEBUG(LOG_TAG, "CNF: %d clauses, %d variables", cnf->n_clauses, cnf->n_variables);
    return cnf;
}

void logic_cnf_free(cnf_formula_t* cnf) {
    if (!cnf) return;
    nimcp_free(cnf);
}

/* ================================================================
 * DPLL SAT SOLVER
 * ================================================================ */

typedef struct dpll_state_s {
    int  assignment[LOGIC_MAX_VARIABLES + 1]; /* 0=unset, 1=true, -1=false */
    int  n_vars;
} dpll_state_t;

static int dpll_clause_status(const cnf_clause_t* cl, const dpll_state_t* s) {
    /* Returns: 1=satisfied, 0=undetermined, -1=unsatisfied */
    bool has_unset = false;
    for (int i = 0; i < cl->n_literals; i++) {
        int lit = cl->literals[i];
        int var = (lit > 0) ? lit : -lit;
        int asgn = s->assignment[var];
        if (asgn == 0) { has_unset = true; continue; }
        bool val = (asgn == 1);
        if (lit > 0 && val) return 1;
        if (lit < 0 && !val) return 1;
    }
    return has_unset ? 0 : -1;
}

static bool dpll_unit_propagate(const cnf_formula_t* cnf, dpll_state_t* s) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (int c = 0; c < cnf->n_clauses; c++) {
            int status = dpll_clause_status(&cnf->clauses[c], s);
            if (status == -1) return false; /* conflict */
            if (status == 1) continue;      /* satisfied */

            /* Find unit clause: exactly one unset literal */
            int unset_lit = 0;
            int unset_count = 0;
            for (int i = 0; i < cnf->clauses[c].n_literals; i++) {
                int lit = cnf->clauses[c].literals[i];
                int var = (lit > 0) ? lit : -lit;
                if (s->assignment[var] == 0) {
                    unset_lit = lit;
                    unset_count++;
                }
            }
            if (unset_count == 1) {
                int var = (unset_lit > 0) ? unset_lit : -unset_lit;
                s->assignment[var] = (unset_lit > 0) ? 1 : -1;
                changed = true;
            }
        }
    }
    return true;
}

static bool dpll_solve(const cnf_formula_t* cnf, dpll_state_t* s) {
    /* Unit propagation */
    dpll_state_t saved;
    memcpy(&saved, s, sizeof(dpll_state_t));

    if (!dpll_unit_propagate(cnf, s)) {
        memcpy(s, &saved, sizeof(dpll_state_t));
        return false;
    }

    /* Check all clauses satisfied */
    bool all_sat = true;
    for (int c = 0; c < cnf->n_clauses; c++) {
        int status = dpll_clause_status(&cnf->clauses[c], s);
        if (status == -1) { memcpy(s, &saved, sizeof(dpll_state_t)); return false; }
        if (status == 0) all_sat = false;
    }
    if (all_sat) return true;

    /* Pick unassigned variable */
    int pick = 0;
    for (int v = 1; v <= s->n_vars; v++) {
        if (s->assignment[v] == 0) { pick = v; break; }
    }
    if (pick == 0) return false;

    /* Try true */
    dpll_state_t branch;
    memcpy(&branch, s, sizeof(dpll_state_t));
    branch.assignment[pick] = 1;
    if (dpll_solve(cnf, &branch)) { memcpy(s, &branch, sizeof(dpll_state_t)); return true; }

    /* Try false */
    memcpy(&branch, s, sizeof(dpll_state_t));
    branch.assignment[pick] = -1;
    if (dpll_solve(cnf, &branch)) { memcpy(s, &branch, sizeof(dpll_state_t)); return true; }

    return false;
}

bool logic_dpll_sat(const cnf_formula_t* cnf, bool* model_out) {
    if (!cnf) return false;

    dpll_state_t state;
    memset(&state, 0, sizeof(state));
    state.n_vars = cnf->n_variables;

    bool result = dpll_solve(cnf, &state);
    if (result && model_out) {
        for (int v = 0; v < cnf->n_variables; v++) {
            model_out[v] = (state.assignment[v + 1] == 1);
        }
    }
    LOG_DEBUG(LOG_TAG, "DPLL: %s (%d variables, %d clauses)",
              result ? "SAT" : "UNSAT", cnf->n_variables, cnf->n_clauses);
    return result;
}

/* ================================================================
 * QUANTIFIER EVALUATION
 * ================================================================ */

bool logic_quantifier_eval(quantifier_t q, int var_index,
                           const bool* domain, int domain_size,
                           const logic_formula_t* body,
                           bool* base_assignment) {
    if (!domain || !body || !base_assignment) return false;
    if (var_index < 0 || var_index >= LOGIC_MAX_VARIABLES) return false;

    for (int i = 0; i < domain_size; i++) {
        base_assignment[var_index] = domain[i];
        bool val = logic_evaluate(body, base_assignment);
        if (q == QUANTIFIER_FORALL && !val) return false;
        if (q == QUANTIFIER_EXISTS && val) return true;
    }
    return (q == QUANTIFIER_FORALL);
}

/* ================================================================
 * QUINE-McCLUSKEY
 * ================================================================ */

qm_result_t logic_quine_mccluskey(const uint32_t* minterms, int n_minterms,
                                  const uint32_t* dont_cares, int n_dont_cares,
                                  int n_variables) {
    qm_result_t result;
    memset(&result, 0, sizeof(result));
    result.n_variables = n_variables;

    if (!minterms || n_minterms <= 0 || n_variables <= 0 || n_variables > 16) return result;

    /* Combine minterms and don't-cares */
    int total = n_minterms + n_dont_cares;
    if (total > LOGIC_MAX_MINTERMS) total = LOGIC_MAX_MINTERMS;

    uint32_t terms[LOGIC_MAX_MINTERMS];
    uint32_t masks[LOGIC_MAX_MINTERMS]; /* bits that are "don't care" in implicant */
    bool used[LOGIC_MAX_MINTERMS];
    int n_terms = 0;

    for (int i = 0; i < n_minterms && n_terms < LOGIC_MAX_MINTERMS; i++) {
        terms[n_terms] = minterms[i];
        masks[n_terms] = 0;
        n_terms++;
    }
    for (int i = 0; i < n_dont_cares && n_terms < LOGIC_MAX_MINTERMS; i++) {
        terms[n_terms] = dont_cares[i];
        masks[n_terms] = 0;
        n_terms++;
    }

    /* Iteratively combine terms differing in exactly one bit */
    bool did_combine = true;
    while (did_combine) {
        did_combine = false;
        memset(used, 0, (size_t)n_terms * sizeof(bool));

        uint32_t new_terms[LOGIC_MAX_MINTERMS];
        uint32_t new_masks[LOGIC_MAX_MINTERMS];
        int n_new = 0;

        for (int i = 0; i < n_terms; i++) {
            for (int j = i + 1; j < n_terms; j++) {
                if (masks[i] != masks[j]) continue;
                uint32_t diff = terms[i] ^ terms[j];
                /* Check diff is exactly one bit (and not in mask) */
                if (diff != 0 && (diff & (diff - 1)) == 0 && (diff & masks[i]) == 0) {
                    if (n_new < LOGIC_MAX_MINTERMS) {
                        new_terms[n_new] = terms[i] & terms[j];
                        new_masks[n_new] = masks[i] | diff;
                        /* Check for duplicate */
                        bool dup = false;
                        for (int k = 0; k < n_new; k++) {
                            if (new_terms[k] == new_terms[n_new] && new_masks[k] == new_masks[n_new]) {
                                dup = true; break;
                            }
                        }
                        if (!dup) n_new++;
                    }
                    used[i] = true;
                    used[j] = true;
                    did_combine = true;
                }
            }
        }

        /* Collect unused terms as prime implicants */
        for (int i = 0; i < n_terms; i++) {
            if (!used[i] && result.n_implicants < LOGIC_MAX_MINTERMS) {
                result.prime_implicants[result.n_implicants] = terms[i];
                result.dont_care_mask[result.n_implicants] = masks[i];
                result.n_implicants++;
            }
        }

        memcpy(terms, new_terms, (size_t)n_new * sizeof(uint32_t));
        memcpy(masks, new_masks, (size_t)n_new * sizeof(uint32_t));
        n_terms = n_new;
    }

    /* Remaining terms are also prime implicants */
    for (int i = 0; i < n_terms; i++) {
        if (result.n_implicants < LOGIC_MAX_MINTERMS) {
            result.prime_implicants[result.n_implicants] = terms[i];
            result.dont_care_mask[result.n_implicants] = masks[i];
            result.n_implicants++;
        }
    }

    LOG_INFO(LOG_TAG, "Quine-McCluskey: %d prime implicants from %d minterms",
             result.n_implicants, n_minterms);
    return result;
}

/* ================================================================
 * PROOF VERIFICATION
 * ================================================================ */

static bool formulas_equivalent(const logic_formula_t* a, const logic_formula_t* b, int n_vars) {
    return logic_are_equivalent(a, b, n_vars);
}

bool logic_verify_inference(const logic_formula_t** premises, int n_premises,
                            const logic_formula_t* conclusion,
                            inference_rule_t rule, int n_vars) {
    if (!premises || !conclusion || n_premises < 1) return false;

    switch (rule) {
        case RULE_MODUS_PONENS: {
            /* P, P->Q |- Q: find P and P->Q among premises */
            if (n_premises < 2) return false;
            for (int i = 0; i < n_premises; i++) {
                for (int j = 0; j < n_premises; j++) {
                    if (i == j) continue;
                    const logic_formula_t* impl = premises[j];
                    if (impl->op != LOGIC_OP_IMPLIES) continue;
                    /* Check: premises[i] ≡ impl->left, conclusion ≡ impl->right */
                    if (formulas_equivalent(premises[i], impl->left, n_vars) &&
                        formulas_equivalent(conclusion, impl->right, n_vars)) {
                        LOG_DEBUG(LOG_TAG, "Modus ponens verified");
                        return true;
                    }
                }
            }
            return false;
        }
        case RULE_MODUS_TOLLENS: {
            /* ~Q, P->Q |- ~P */
            if (n_premises < 2) return false;
            for (int i = 0; i < n_premises; i++) {
                for (int j = 0; j < n_premises; j++) {
                    if (i == j) continue;
                    const logic_formula_t* impl = premises[j];
                    if (impl->op != LOGIC_OP_IMPLIES) continue;
                    const logic_formula_t* neg_q = premises[i];
                    if (neg_q->op != LOGIC_OP_NOT) continue;
                    /* Check: neg_q->left ≡ impl->right, conclusion ≡ NOT(impl->left) */
                    if (formulas_equivalent(neg_q->left, impl->right, n_vars) &&
                        conclusion->op == LOGIC_OP_NOT &&
                        formulas_equivalent(conclusion->left, impl->left, n_vars)) {
                        LOG_DEBUG(LOG_TAG, "Modus tollens verified");
                        return true;
                    }
                }
            }
            return false;
        }
        case RULE_HYPOTHETICAL_SYLLOGISM: {
            /* P->Q, Q->R |- P->R */
            if (n_premises < 2) return false;
            if (conclusion->op != LOGIC_OP_IMPLIES) return false;
            for (int i = 0; i < n_premises; i++) {
                if (premises[i]->op != LOGIC_OP_IMPLIES) continue;
                for (int j = 0; j < n_premises; j++) {
                    if (i == j) continue;
                    if (premises[j]->op != LOGIC_OP_IMPLIES) continue;
                    /* Check: P_i->Q_i, Q_j matches, and conclusion = P_i->R_j */
                    if (formulas_equivalent(premises[i]->right, premises[j]->left, n_vars) &&
                        formulas_equivalent(conclusion->left, premises[i]->left, n_vars) &&
                        formulas_equivalent(conclusion->right, premises[j]->right, n_vars)) {
                        LOG_DEBUG(LOG_TAG, "Hypothetical syllogism verified");
                        return true;
                    }
                }
            }
            return false;
        }
        case RULE_DISJUNCTIVE_SYLLOGISM: {
            /* P|Q, ~P |- Q */
            if (n_premises < 2) return false;
            for (int i = 0; i < n_premises; i++) {
                if (premises[i]->op != LOGIC_OP_OR) continue;
                for (int j = 0; j < n_premises; j++) {
                    if (i == j) continue;
                    if (premises[j]->op != LOGIC_OP_NOT) continue;
                    /* ~P negates left disjunct, conclusion ≡ right disjunct */
                    if (formulas_equivalent(premises[j]->left, premises[i]->left, n_vars) &&
                        formulas_equivalent(conclusion, premises[i]->right, n_vars)) {
                        LOG_DEBUG(LOG_TAG, "Disjunctive syllogism verified");
                        return true;
                    }
                    /* ~P negates right disjunct, conclusion ≡ left disjunct */
                    if (formulas_equivalent(premises[j]->left, premises[i]->right, n_vars) &&
                        formulas_equivalent(conclusion, premises[i]->left, n_vars)) {
                        return true;
                    }
                }
            }
            return false;
        }
    }
    return false;
}
