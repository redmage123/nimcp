/**
 * @file nimcp_complexity_theory.h
 * @brief Computational Complexity Theory — complexity classes, reductions, analysis
 *
 * WHAT: Analyzes computational complexity of problems and algorithms: time/space
 *       complexity measurement, complexity class membership, polynomial reductions,
 *       NP-completeness verification, approximation ratios, parameterized complexity.
 * WHY:  Enables reasoning about what's computable, what's tractable, and what
 *       requires approximation. "Can this problem be solved efficiently?" is a
 *       fundamental question for any intelligent system planning actions.
 * HOW:  Empirical complexity measurement (fit O(n^k) to runtime data), known
 *       problem classification, Cook-Levin reduction framework, approximation
 *       algorithm analysis, Rice's theorem implications.
 *
 * THEORETICAL FOUNDATION:
 *   - P: decidable in polynomial time on deterministic TM
 *   - NP: decidable in polynomial time on nondeterministic TM (verifiable in P)
 *   - NP-complete: in NP ∧ NP-hard (every NP problem reduces to it)
 *   - NP-hard: at least as hard as NP-complete (may not be in NP)
 *   - PSPACE: decidable in polynomial space
 *   - EXPTIME: decidable in exponential time
 *   - Undecidable: no TM can decide (halting problem, Rice's theorem)
 *   - Cook-Levin: SAT is NP-complete
 *   - P vs NP: open — greatest unsolved problem in CS
 */

#ifndef NIMCP_COMPLEXITY_THEORY_H
#define NIMCP_COMPLEXITY_THEORY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define CPLX_MAX_PROBLEMS       64
#define CPLX_MAX_REDUCTIONS     128
#define CPLX_MAX_MEASUREMENTS   256
#define CPLX_MAX_NAME           48
#define CPLX_MAX_TAPE           4096    /* Turing machine tape */
#define CPLX_MAX_STATES         64
#define CPLX_MAX_SYMBOLS        16

/* ============================================================================
 * Complexity Classes
 * ============================================================================ */

typedef enum {
    CPLX_CLASS_O1       = 0,    /* O(1) constant */
    CPLX_CLASS_OLOGN    = 1,    /* O(log n) logarithmic */
    CPLX_CLASS_ON       = 2,    /* O(n) linear */
    CPLX_CLASS_ONLOGN   = 3,    /* O(n log n) linearithmic */
    CPLX_CLASS_ON2      = 4,    /* O(n²) quadratic */
    CPLX_CLASS_ON3      = 5,    /* O(n³) cubic */
    CPLX_CLASS_ONK      = 6,    /* O(n^k) polynomial (general) */
    CPLX_CLASS_O2N      = 7,    /* O(2^n) exponential */
    CPLX_CLASS_ONF      = 8,    /* O(n!) factorial */
    /* Formal classes */
    CPLX_CLASS_P        = 10,   /* polynomial time */
    CPLX_CLASS_NP       = 11,   /* nondeterministic polynomial */
    CPLX_CLASS_NP_COMPLETE = 12,
    CPLX_CLASS_NP_HARD  = 13,
    CPLX_CLASS_PSPACE   = 14,
    CPLX_CLASS_EXPTIME  = 15,
    CPLX_CLASS_UNDECIDABLE = 16,
    /* Space classes */
    CPLX_CLASS_LOGSPACE = 20,   /* O(log n) space */
    CPLX_CLASS_PSPACE_COMPLETE = 21,
    /* Parameterized */
    CPLX_CLASS_FPT      = 25,   /* fixed-parameter tractable */
    CPLX_CLASS_W1       = 26,   /* W[1]-hard */
    CPLX_CLASS_COUNT
} cplx_class_t;

/* ============================================================================
 * Known Problems Database
 * ============================================================================ */

typedef enum {
    CPLX_PROB_SORTING       = 0,    /* O(n log n) — P */
    CPLX_PROB_SEARCHING     = 1,    /* O(log n) — P */
    CPLX_PROB_SHORTEST_PATH = 2,    /* O(V² or E log V) — P */
    CPLX_PROB_MST           = 3,    /* O(E log V) — P */
    CPLX_PROB_MAX_FLOW      = 4,    /* O(VE²) — P */
    CPLX_PROB_MATCHING      = 5,    /* O(V³) — P */
    CPLX_PROB_LP            = 6,    /* polynomial — P */
    CPLX_PROB_PRIMALITY     = 7,    /* O(log^6 n) AKS — P */
    CPLX_PROB_SAT           = 10,   /* NP-complete (Cook-Levin) */
    CPLX_PROB_3SAT          = 11,   /* NP-complete */
    CPLX_PROB_CLIQUE        = 12,   /* NP-complete */
    CPLX_PROB_VERTEX_COVER  = 13,   /* NP-complete */
    CPLX_PROB_HAM_CYCLE     = 14,   /* NP-complete */
    CPLX_PROB_TSP           = 15,   /* NP-hard */
    CPLX_PROB_KNAPSACK      = 16,   /* NP-complete (weakly) */
    CPLX_PROB_GRAPH_COLOR   = 17,   /* NP-complete */
    CPLX_PROB_SUBSET_SUM    = 18,   /* NP-complete */
    CPLX_PROB_SET_COVER     = 19,   /* NP-complete */
    CPLX_PROB_ILP           = 20,   /* NP-hard */
    CPLX_PROB_FACTORING     = 21,   /* believed not in P, not known NP-complete */
    CPLX_PROB_GRAPH_ISO     = 22,   /* NP, not known NP-complete */
    CPLX_PROB_QBF           = 25,   /* PSPACE-complete */
    CPLX_PROB_HALTING       = 30,   /* undecidable */
    CPLX_PROB_RICE          = 31,   /* undecidable (any nontrivial property) */
    CPLX_PROB_PCP           = 32,   /* undecidable */
    CPLX_PROB_COUNT
} cplx_known_problem_t;

/* ============================================================================
 * Problem Description
 * ============================================================================ */

typedef struct {
    uint32_t            id;
    char                name[CPLX_MAX_NAME];
    cplx_class_t        time_class;         /* time complexity class */
    cplx_class_t        space_class;        /* space complexity class */
    float               polynomial_degree;  /* k in O(n^k), 0 if not polynomial */
    float               best_known_exponent;/* best known algorithm's exponent */
    bool                is_decision;        /* decision vs optimization */
    bool                has_poly_verifier;  /* certificate verifiable in P */
    bool                is_np_complete;
    bool                is_undecidable;
    float               best_approx_ratio;  /* best known approximation ratio */
    bool                active;
} cplx_problem_t;

/* ============================================================================
 * Polynomial Reduction
 * ============================================================================ */

typedef struct {
    uint32_t    from_problem;       /* reduces FROM this */
    uint32_t    to_problem;         /* reduces TO this */
    cplx_class_t reduction_type;    /* polynomial, log-space, etc. */
    bool        preserves_approx;   /* does reduction preserve approx ratio? */
    bool        active;
} cplx_reduction_t;

/* ============================================================================
 * Empirical Complexity Measurement
 * ============================================================================ */

typedef struct {
    uint32_t    input_size;
    double      time_us;            /* microseconds */
    uint64_t    operations;         /* basic operation count */
    uint64_t    memory_bytes;
} cplx_measurement_t;

typedef struct {
    cplx_measurement_t  points[CPLX_MAX_MEASUREMENTS];
    uint32_t            num_points;
    /* Fitted model: T(n) = c · n^k */
    double              fitted_coefficient;
    double              fitted_exponent;
    double              r_squared;          /* goodness of fit */
    cplx_class_t        inferred_class;
} cplx_empirical_t;

/* ============================================================================
 * Turing Machine (for decidability reasoning)
 * ============================================================================ */

typedef struct {
    uint32_t    from_state;
    uint8_t     read_symbol;
    uint32_t    to_state;
    uint8_t     write_symbol;
    int8_t      move;               /* -1=left, 0=stay, +1=right */
} cplx_tm_transition_t;

typedef struct {
    cplx_tm_transition_t transitions[CPLX_MAX_STATES * CPLX_MAX_SYMBOLS];
    uint32_t    num_transitions;
    uint32_t    num_states;
    uint8_t     num_symbols;
    uint32_t    start_state;
    uint32_t    accept_state;
    uint32_t    reject_state;
    /* Tape */
    uint8_t     tape[CPLX_MAX_TAPE];
    int32_t     head_position;
    uint32_t    current_state;
    uint64_t    steps_executed;
    bool        halted;
    bool        accepted;
} cplx_turing_machine_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    uint32_t    max_tm_steps;       /* halt TM after this many steps */
    bool        enable_empirical;   /* measure runtime empirically */
    bool        enable_reductions;  /* track reduction chains */
} cplx_config_t;

/* ============================================================================
 * Stats
 * ============================================================================ */

typedef struct {
    uint64_t    problems_classified;
    uint64_t    reductions_verified;
    uint64_t    tm_steps_total;
    uint64_t    empirical_fits;
} cplx_stats_t;

/* ============================================================================
 * Engine
 * ============================================================================ */

typedef struct complexity_theory_sim {
    cplx_problem_t      problems[CPLX_MAX_PROBLEMS];
    uint32_t            num_problems;
    cplx_reduction_t    reductions[CPLX_MAX_REDUCTIONS];
    uint32_t            num_reductions;
    cplx_turing_machine_t tm;
    cplx_empirical_t    empirical;
    cplx_config_t       config;
    cplx_stats_t        stats;
    bool                initialized;
} complexity_theory_sim_t;

/* ============================================================================
 * API
 * ============================================================================ */

complexity_theory_sim_t* complexity_theory_create(const cplx_config_t* config);
void complexity_theory_destroy(complexity_theory_sim_t* sim);

/** Load database of known NP-complete and other classified problems */
void complexity_theory_load_known_problems(complexity_theory_sim_t* sim);

/** Classify a problem from empirical measurements */
cplx_class_t complexity_theory_classify_empirical(complexity_theory_sim_t* sim,
                                                    const cplx_measurement_t* points,
                                                    uint32_t num_points);

/** Fit T(n) = c·n^k to measurement data (returns R²) */
double complexity_theory_fit_polynomial(const cplx_measurement_t* points,
                                         uint32_t num_points,
                                         double* coefficient, double* exponent);

/** Check if problem A reduces to problem B (via known reduction chain) */
bool complexity_theory_reduces_to(const complexity_theory_sim_t* sim,
                                    uint32_t problem_a, uint32_t problem_b);

/** Register a polynomial reduction */
uint32_t complexity_theory_add_reduction(complexity_theory_sim_t* sim,
                                          uint32_t from, uint32_t to);

/** Get the complexity class of a known problem */
cplx_class_t complexity_theory_get_class(const complexity_theory_sim_t* sim,
                                           cplx_known_problem_t problem);

/** Check if a problem is in P (has known polynomial algorithm) */
bool complexity_theory_is_in_P(const complexity_theory_sim_t* sim, uint32_t problem_id);

/** Check if a problem is NP-complete */
bool complexity_theory_is_np_complete(const complexity_theory_sim_t* sim, uint32_t problem_id);

/** Check if a problem is undecidable */
bool complexity_theory_is_undecidable(const complexity_theory_sim_t* sim, uint32_t problem_id);

/** Get best known approximation ratio for a problem */
float complexity_theory_approx_ratio(const complexity_theory_sim_t* sim, uint32_t problem_id);

/* === Turing Machine === */

/** Initialize a Turing machine */
void complexity_theory_tm_init(complexity_theory_sim_t* sim,
                                 uint32_t num_states, uint8_t num_symbols);

/** Add a TM transition */
void complexity_theory_tm_add_transition(complexity_theory_sim_t* sim,
                                           uint32_t from, uint8_t read,
                                           uint32_t to, uint8_t write, int8_t move);

/** Load input onto TM tape */
void complexity_theory_tm_load_input(complexity_theory_sim_t* sim,
                                       const uint8_t* input, uint32_t length);

/** Run TM for up to max_steps (returns true if halted) */
bool complexity_theory_tm_run(complexity_theory_sim_t* sim, uint64_t max_steps);

/** Single step the TM */
bool complexity_theory_tm_step(complexity_theory_sim_t* sim);

/** Did the TM accept? */
bool complexity_theory_tm_accepted(const complexity_theory_sim_t* sim);

/* === Complexity Analysis === */

/** Compute Master Theorem result: T(n) = aT(n/b) + f(n) */
cplx_class_t complexity_theory_master_theorem(uint32_t a, uint32_t b,
                                                float f_exponent);

/** Compute amortized complexity from a sequence of operations */
double complexity_theory_amortized(const double* op_costs, uint32_t num_ops);

/** String representation of complexity class */
const char* complexity_theory_class_name(cplx_class_t cls);

/** Default config */
cplx_config_t complexity_theory_default_config(void);

/** Get stats */
cplx_stats_t complexity_theory_get_stats(const complexity_theory_sim_t* sim);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COMPLEXITY_THEORY_H */
