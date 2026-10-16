/**
 * @file nimcp_logic.h
 * @brief Propositional and predicate logic engine
 *
 * Formula AST, truth tables, DPLL satisfiability, CNF conversion,
 * Quine-McCluskey simplification, proof verification with inference rules.
 */

#ifndef NIMCP_LOGIC_H
#define NIMCP_LOGIC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- constants ---------- */
#define LOGIC_MAX_VARIABLES   32
#define LOGIC_MAX_DEPTH       16
#define LOGIC_MAX_CHILDREN    64
#define LOGIC_MAX_CLAUSES     256
#define LOGIC_MAX_LITERALS    512
#define LOGIC_MAX_PREMISES    16
#define LOGIC_MAX_MINTERMS    256
#define LOGIC_MAX_DOMAIN_SIZE 64

/* ---------- enums ---------- */

typedef enum logic_op_e {
    LOGIC_OP_VAR,       /* leaf: variable */
    LOGIC_OP_CONST,     /* leaf: true/false */
    LOGIC_OP_NOT,       /* unary */
    LOGIC_OP_AND,       /* binary */
    LOGIC_OP_OR,        /* binary */
    LOGIC_OP_IMPLIES,   /* binary */
    LOGIC_OP_IFF,       /* binary */
    LOGIC_OP_XOR        /* binary */
} logic_op_t;

typedef enum inference_rule_e {
    RULE_MODUS_PONENS,          /* P, P->Q  |- Q  */
    RULE_MODUS_TOLLENS,         /* ~Q, P->Q |- ~P */
    RULE_HYPOTHETICAL_SYLLOGISM,/* P->Q, Q->R |- P->R */
    RULE_DISJUNCTIVE_SYLLOGISM  /* P|Q, ~P |- Q  */
} inference_rule_t;

typedef enum quantifier_e {
    QUANTIFIER_FORALL,
    QUANTIFIER_EXISTS
} quantifier_t;

/* ---------- formula AST ---------- */

typedef struct logic_formula_s {
    logic_op_t op;
    int        var_index;       /* for LOGIC_OP_VAR: 0..31 */
    bool       const_value;     /* for LOGIC_OP_CONST */
    struct logic_formula_s* left;
    struct logic_formula_s* right;  /* NULL for unary ops */
    int        depth;
} logic_formula_t;

/* ---------- CNF ---------- */

/** A literal is a signed variable: positive = var, negative = ~var */
typedef struct cnf_clause_s {
    int  literals[LOGIC_MAX_VARIABLES];
    int  n_literals;
} cnf_clause_t;

typedef struct cnf_formula_s {
    cnf_clause_t clauses[LOGIC_MAX_CLAUSES];
    int          n_clauses;
    int          n_variables;
} cnf_formula_t;

/* ---------- truth table ---------- */

typedef struct truth_table_s {
    int      n_variables;
    int      n_rows;            /* 2^n_variables */
    bool*    results;           /* n_rows booleans */
} truth_table_t;

/* ---------- Quine-McCluskey ---------- */

typedef struct qm_result_s {
    uint32_t prime_implicants[LOGIC_MAX_MINTERMS];
    uint32_t dont_care_mask[LOGIC_MAX_MINTERMS];
    int      n_implicants;
    int      n_variables;
} qm_result_t;

/* ---------- proof step ---------- */

typedef struct proof_step_s {
    inference_rule_t  rule;
    int               premise_indices[2]; /* indices into premises array */
    logic_formula_t*  conclusion;
} proof_step_t;

/* ---------- main context ---------- */

typedef struct logic_engine_s {
    char     var_names[LOGIC_MAX_VARIABLES][32];
    int      n_variables;
    bool     assignment[LOGIC_MAX_VARIABLES]; /* current variable assignment */
} logic_engine_t;

/* ---------- lifecycle ---------- */
logic_engine_t*  logic_engine_create(void);
void             logic_engine_destroy(logic_engine_t* eng);

/* ---------- formula construction ---------- */
logic_formula_t* logic_var(int var_index);
logic_formula_t* logic_const(bool value);
logic_formula_t* logic_not(logic_formula_t* child);
logic_formula_t* logic_binary(logic_op_t op, logic_formula_t* left, logic_formula_t* right);
void             logic_formula_free(logic_formula_t* f);
logic_formula_t* logic_formula_clone(const logic_formula_t* f);

/* ---------- evaluation ---------- */
bool logic_evaluate(const logic_formula_t* f, const bool* assignment);

/* ---------- truth table ---------- */
truth_table_t* logic_truth_table(const logic_formula_t* f, int n_vars);
void           logic_truth_table_free(truth_table_t* tt);
bool           logic_is_tautology(const logic_formula_t* f, int n_vars);
bool           logic_is_satisfiable(const logic_formula_t* f, int n_vars);
bool           logic_are_equivalent(const logic_formula_t* a, const logic_formula_t* b, int n_vars);

/* ---------- CNF ---------- */
cnf_formula_t* logic_to_cnf(const logic_formula_t* f, int n_vars);
void           logic_cnf_free(cnf_formula_t* cnf);

/* ---------- DPLL satisfiability ---------- */
bool logic_dpll_sat(const cnf_formula_t* cnf, bool* model_out);

/* ---------- quantifiers (finite domain) ---------- */
/**
 * Evaluate quantified formula over a finite domain.
 * @param q          FORALL or EXISTS
 * @param var_index  which variable is quantified
 * @param domain     array of truth values to try for var_index
 * @param domain_size number of domain elements
 * @param body       formula body
 * @param base_assignment current assignment for other variables
 */
bool logic_quantifier_eval(quantifier_t q, int var_index,
                           const bool* domain, int domain_size,
                           const logic_formula_t* body,
                           bool* base_assignment);

/* ---------- Quine-McCluskey ---------- */
qm_result_t logic_quine_mccluskey(const uint32_t* minterms, int n_minterms,
                                  const uint32_t* dont_cares, int n_dont_cares,
                                  int n_variables);

/* ---------- proof verification ---------- */
/**
 * Verify that conclusion follows from premises via the given inference rule.
 */
bool logic_verify_inference(const logic_formula_t** premises, int n_premises,
                            const logic_formula_t* conclusion,
                            inference_rule_t rule, int n_vars);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LOGIC_H */
