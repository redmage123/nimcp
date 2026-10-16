/**
 * @file nimcp_complexity_theory.c
 * @brief Computational Complexity Theory — classification, reductions, TM simulation
 *
 * WHAT: Empirical complexity fitting, known problem database, NP-completeness,
 *       Turing machine simulation, Master Theorem, reduction chains
 * WHY:  Reasoning about tractability of problems the brain encounters
 * HOW:  Log-log regression for empirical fitting, graph search for reduction
 *       chains, step-by-step TM execution
 */

#include "cognitive/math/nimcp_complexity_theory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "COMPLEXITY"

/* ============================================================================
 * Class Names
 * ============================================================================ */

static const char* class_names[] = {
    [CPLX_CLASS_O1]       = "O(1)",
    [CPLX_CLASS_OLOGN]    = "O(log n)",
    [CPLX_CLASS_ON]       = "O(n)",
    [CPLX_CLASS_ONLOGN]   = "O(n log n)",
    [CPLX_CLASS_ON2]      = "O(n^2)",
    [CPLX_CLASS_ON3]      = "O(n^3)",
    [CPLX_CLASS_ONK]      = "O(n^k)",
    [CPLX_CLASS_O2N]      = "O(2^n)",
    [CPLX_CLASS_ONF]      = "O(n!)",
    [CPLX_CLASS_P]        = "P",
    [CPLX_CLASS_NP]       = "NP",
    [CPLX_CLASS_NP_COMPLETE] = "NP-complete",
    [CPLX_CLASS_NP_HARD]  = "NP-hard",
    [CPLX_CLASS_PSPACE]   = "PSPACE",
    [CPLX_CLASS_EXPTIME]  = "EXPTIME",
    [CPLX_CLASS_UNDECIDABLE] = "Undecidable",
    [CPLX_CLASS_LOGSPACE] = "LOGSPACE",
    [CPLX_CLASS_FPT]      = "FPT",
};

const char* complexity_theory_class_name(cplx_class_t cls) {
    if (cls < sizeof(class_names)/sizeof(class_names[0]) && class_names[cls])
        return class_names[cls];
    return "Unknown";
}

/* ============================================================================
 * Public API
 * ============================================================================ */

cplx_config_t complexity_theory_default_config(void) {
    return (cplx_config_t){
        .max_tm_steps = 100000,
        .enable_empirical = true,
        .enable_reductions = true,
    };
}

complexity_theory_sim_t* complexity_theory_create(const cplx_config_t* config) {
    cplx_config_t cfg = config ? *config : complexity_theory_default_config();
    complexity_theory_sim_t* sim = nimcp_calloc(1, sizeof(*sim));
    if (!sim) return NULL;
    sim->config = cfg;
    sim->initialized = true;
    LOG_INFO(LOG_TAG, "Complexity theory engine created");
    return sim;
}

void complexity_theory_destroy(complexity_theory_sim_t* sim) {
    if (!sim) return;
    nimcp_free(sim);
}

/* ============================================================================
 * Known Problems Database
 * ============================================================================ */

void complexity_theory_load_known_problems(complexity_theory_sim_t* sim) {
    if (!sim) return;

    struct { const char* name; cplx_known_problem_t id; cplx_class_t time;
             cplx_class_t space; float degree; bool np_c; bool undec; float approx; } known[] = {
        {"Sorting",         CPLX_PROB_SORTING,      CPLX_CLASS_ONLOGN, CPLX_CLASS_ON,    1.0f, false, false, 1.0f},
        {"Binary Search",   CPLX_PROB_SEARCHING,    CPLX_CLASS_OLOGN,  CPLX_CLASS_O1,    0.0f, false, false, 1.0f},
        {"Shortest Path",   CPLX_PROB_SHORTEST_PATH,CPLX_CLASS_ON2,    CPLX_CLASS_ON,    2.0f, false, false, 1.0f},
        {"MST",             CPLX_PROB_MST,          CPLX_CLASS_ONLOGN, CPLX_CLASS_ON,    1.0f, false, false, 1.0f},
        {"Max Flow",        CPLX_PROB_MAX_FLOW,     CPLX_CLASS_ON3,    CPLX_CLASS_ON2,   3.0f, false, false, 1.0f},
        {"Primality",       CPLX_PROB_PRIMALITY,    CPLX_CLASS_ONK,    CPLX_CLASS_OLOGN, 6.0f, false, false, 1.0f},
        {"SAT",             CPLX_PROB_SAT,          CPLX_CLASS_NP_COMPLETE, CPLX_CLASS_ONK, 0, true, false, 0},
        {"3-SAT",           CPLX_PROB_3SAT,         CPLX_CLASS_NP_COMPLETE, CPLX_CLASS_ON, 0, true, false, 0.875f},
        {"Clique",          CPLX_PROB_CLIQUE,       CPLX_CLASS_NP_COMPLETE, CPLX_CLASS_ON, 0, true, false, 0},
        {"Vertex Cover",    CPLX_PROB_VERTEX_COVER, CPLX_CLASS_NP_COMPLETE, CPLX_CLASS_ON, 0, true, false, 2.0f},
        {"Hamiltonian Cycle",CPLX_PROB_HAM_CYCLE,   CPLX_CLASS_NP_COMPLETE, CPLX_CLASS_ON, 0, true, false, 0},
        {"TSP",             CPLX_PROB_TSP,          CPLX_CLASS_NP_HARD,    CPLX_CLASS_ON, 0, false, false, 1.5f},
        {"Knapsack",        CPLX_PROB_KNAPSACK,     CPLX_CLASS_NP_COMPLETE, CPLX_CLASS_ON, 0, true, false, 0},
        {"Graph Coloring",  CPLX_PROB_GRAPH_COLOR,  CPLX_CLASS_NP_COMPLETE, CPLX_CLASS_ON, 0, true, false, 0},
        {"Subset Sum",      CPLX_PROB_SUBSET_SUM,   CPLX_CLASS_NP_COMPLETE, CPLX_CLASS_ON, 0, true, false, 0},
        {"Set Cover",       CPLX_PROB_SET_COVER,    CPLX_CLASS_NP_COMPLETE, CPLX_CLASS_ON, 0, true, false, 0},
        {"Factoring",       CPLX_PROB_FACTORING,    CPLX_CLASS_NP,     CPLX_CLASS_ON,    0, false, false, 1.0f},
        {"Graph Isomorphism",CPLX_PROB_GRAPH_ISO,   CPLX_CLASS_NP,     CPLX_CLASS_ON,    0, false, false, 1.0f},
        {"QBF",             CPLX_PROB_QBF,          CPLX_CLASS_PSPACE, CPLX_CLASS_PSPACE, 0, false, false, 0},
        {"Halting Problem", CPLX_PROB_HALTING,       CPLX_CLASS_UNDECIDABLE, CPLX_CLASS_UNDECIDABLE, 0, false, true, 0},
        {"Rice's Theorem",  CPLX_PROB_RICE,          CPLX_CLASS_UNDECIDABLE, CPLX_CLASS_UNDECIDABLE, 0, false, true, 0},
        {"Post Correspondence",CPLX_PROB_PCP,        CPLX_CLASS_UNDECIDABLE, CPLX_CLASS_UNDECIDABLE, 0, false, true, 0},
    };

    for (uint32_t i = 0; i < sizeof(known)/sizeof(known[0]) && sim->num_problems < CPLX_MAX_PROBLEMS; i++) {
        cplx_problem_t p = {0};
        p.id = sim->num_problems;
        strncpy(p.name, known[i].name, CPLX_MAX_NAME - 1);
        p.time_class = known[i].time;
        p.space_class = known[i].space;
        p.polynomial_degree = known[i].degree;
        p.best_known_exponent = known[i].degree;
        p.is_decision = true;
        p.has_poly_verifier = (known[i].time == CPLX_CLASS_NP ||
                                known[i].time == CPLX_CLASS_NP_COMPLETE);
        p.is_np_complete = known[i].np_c;
        p.is_undecidable = known[i].undec;
        p.best_approx_ratio = known[i].approx;
        p.active = true;
        sim->problems[sim->num_problems++] = p;
    }

    /* Standard reductions: 3SAT ≤ₚ Clique ≤ₚ Vertex Cover, SAT ≤ₚ 3SAT, etc. */
    struct { cplx_known_problem_t from, to; } reds[] = {
        {CPLX_PROB_SAT,     CPLX_PROB_3SAT},
        {CPLX_PROB_3SAT,    CPLX_PROB_CLIQUE},
        {CPLX_PROB_3SAT,    CPLX_PROB_VERTEX_COVER},
        {CPLX_PROB_3SAT,    CPLX_PROB_HAM_CYCLE},
        {CPLX_PROB_HAM_CYCLE, CPLX_PROB_TSP},
        {CPLX_PROB_3SAT,    CPLX_PROB_GRAPH_COLOR},
        {CPLX_PROB_3SAT,    CPLX_PROB_SUBSET_SUM},
        {CPLX_PROB_VERTEX_COVER, CPLX_PROB_SET_COVER},
        {CPLX_PROB_SUBSET_SUM, CPLX_PROB_KNAPSACK},
    };
    for (uint32_t i = 0; i < sizeof(reds)/sizeof(reds[0]) && sim->num_reductions < CPLX_MAX_REDUCTIONS; i++) {
        sim->reductions[sim->num_reductions++] = (cplx_reduction_t){
            .from_problem = reds[i].from, .to_problem = reds[i].to,
            .reduction_type = CPLX_CLASS_P, .active = true
        };
    }

    sim->stats.problems_classified = sim->num_problems;
    LOG_INFO(LOG_TAG, "Loaded %u known problems, %u reductions",
             sim->num_problems, sim->num_reductions);
}

/* ============================================================================
 * Empirical Complexity Analysis
 * ============================================================================ */

double complexity_theory_fit_polynomial(const cplx_measurement_t* points,
                                         uint32_t num_points,
                                         double* coefficient, double* exponent) {
    /* Fit T(n) = c·n^k via log-log linear regression:
     * log(T) = log(c) + k·log(n) */
    if (!points || num_points < 2 || !coefficient || !exponent) return 0;

    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;
    uint32_t valid = 0;

    for (uint32_t i = 0; i < num_points; i++) {
        if (points[i].input_size < 2 || points[i].time_us <= 0) continue;
        double x = log((double)points[i].input_size);
        double y = log(points[i].time_us);
        sum_x += x; sum_y += y; sum_xy += x*y; sum_x2 += x*x;
        valid++;
    }
    if (valid < 2) { *coefficient = 1; *exponent = 1; return 0; }

    double n = (double)valid;
    double denom = n * sum_x2 - sum_x * sum_x;
    if (fabs(denom) < 1e-20) { *coefficient = 1; *exponent = 1; return 0; }

    double k = (n * sum_xy - sum_x * sum_y) / denom;
    double log_c = (sum_y - k * sum_x) / n;

    *exponent = k;
    *coefficient = exp(log_c);

    /* R² goodness of fit */
    double mean_y = sum_y / n;
    double ss_tot = 0, ss_res = 0;
    for (uint32_t i = 0; i < num_points; i++) {
        if (points[i].input_size < 2 || points[i].time_us <= 0) continue;
        double x = log((double)points[i].input_size);
        double y = log(points[i].time_us);
        double y_pred = log_c + k * x;
        ss_tot += (y - mean_y) * (y - mean_y);
        ss_res += (y - y_pred) * (y - y_pred);
    }
    return (ss_tot > 1e-20) ? 1.0 - ss_res / ss_tot : 0;
}

cplx_class_t complexity_theory_classify_empirical(complexity_theory_sim_t* sim,
                                                    const cplx_measurement_t* points,
                                                    uint32_t num_points) {
    if (!sim || !points || num_points < 3) return CPLX_CLASS_ONK;

    double c, k;
    double r2 = complexity_theory_fit_polynomial(points, num_points, &c, &k);

    sim->empirical.num_points = (num_points > CPLX_MAX_MEASUREMENTS) ? CPLX_MAX_MEASUREMENTS : num_points;
    memcpy(sim->empirical.points, points, sim->empirical.num_points * sizeof(cplx_measurement_t));
    sim->empirical.fitted_coefficient = c;
    sim->empirical.fitted_exponent = k;
    sim->empirical.r_squared = r2;
    sim->stats.empirical_fits++;

    /* Classify based on fitted exponent */
    cplx_class_t cls;
    if (r2 < 0.8) {
        /* Poor polynomial fit — might be exponential */
        /* Try fitting T(n) = c·2^(an) */
        cls = CPLX_CLASS_O2N;
    } else if (k < 0.2)       cls = CPLX_CLASS_O1;
    else if (k < 0.7)         cls = CPLX_CLASS_OLOGN;
    else if (k < 1.3)         cls = CPLX_CLASS_ON;
    else if (k < 1.7)         cls = CPLX_CLASS_ONLOGN;
    else if (k < 2.3)         cls = CPLX_CLASS_ON2;
    else if (k < 3.3)         cls = CPLX_CLASS_ON3;
    else                       cls = CPLX_CLASS_ONK;

    sim->empirical.inferred_class = cls;
    return cls;
}

/* ============================================================================
 * Problem Queries
 * ============================================================================ */

cplx_class_t complexity_theory_get_class(const complexity_theory_sim_t* sim,
                                           cplx_known_problem_t problem) {
    if (!sim) return CPLX_CLASS_ONK;
    for (uint32_t i = 0; i < sim->num_problems; i++) {
        if (sim->problems[i].active && i == (uint32_t)problem)
            return sim->problems[i].time_class;
    }
    return CPLX_CLASS_ONK;
}

bool complexity_theory_is_in_P(const complexity_theory_sim_t* sim, uint32_t problem_id) {
    if (!sim || problem_id >= sim->num_problems) return false;
    cplx_class_t c = sim->problems[problem_id].time_class;
    return c <= CPLX_CLASS_ONK || c == CPLX_CLASS_P;
}

bool complexity_theory_is_np_complete(const complexity_theory_sim_t* sim, uint32_t problem_id) {
    if (!sim || problem_id >= sim->num_problems) return false;
    return sim->problems[problem_id].is_np_complete;
}

bool complexity_theory_is_undecidable(const complexity_theory_sim_t* sim, uint32_t problem_id) {
    if (!sim || problem_id >= sim->num_problems) return false;
    return sim->problems[problem_id].is_undecidable;
}

float complexity_theory_approx_ratio(const complexity_theory_sim_t* sim, uint32_t problem_id) {
    if (!sim || problem_id >= sim->num_problems) return 0;
    return sim->problems[problem_id].best_approx_ratio;
}

bool complexity_theory_reduces_to(const complexity_theory_sim_t* sim,
                                    uint32_t problem_a, uint32_t problem_b) {
    if (!sim) return false;
    /* BFS through reduction graph */
    bool visited[CPLX_MAX_PROBLEMS] = {0};
    uint32_t queue[CPLX_MAX_PROBLEMS];
    uint32_t qh = 0, qt = 0;
    queue[qt++] = problem_a;
    visited[problem_a] = true;

    while (qh < qt) {
        uint32_t curr = queue[qh++];
        if (curr == problem_b) return true;
        for (uint32_t r = 0; r < sim->num_reductions; r++) {
            if (sim->reductions[r].active && sim->reductions[r].from_problem == curr) {
                uint32_t next = sim->reductions[r].to_problem;
                if (next < CPLX_MAX_PROBLEMS && !visited[next]) {
                    visited[next] = true;
                    if (qt < CPLX_MAX_PROBLEMS) queue[qt++] = next;
                }
            }
        }
    }
    return false;
}

uint32_t complexity_theory_add_reduction(complexity_theory_sim_t* sim,
                                          uint32_t from, uint32_t to) {
    if (!sim || sim->num_reductions >= CPLX_MAX_REDUCTIONS) return UINT32_MAX;
    uint32_t id = sim->num_reductions;
    sim->reductions[id] = (cplx_reduction_t){
        .from_problem = from, .to_problem = to,
        .reduction_type = CPLX_CLASS_P, .active = true
    };
    sim->num_reductions = id + 1;
    sim->stats.reductions_verified++;
    return id;
}

/* ============================================================================
 * Turing Machine
 * ============================================================================ */

void complexity_theory_tm_init(complexity_theory_sim_t* sim,
                                 uint32_t num_states, uint8_t num_symbols) {
    if (!sim) return;
    memset(&sim->tm, 0, sizeof(sim->tm));
    sim->tm.num_states = num_states < CPLX_MAX_STATES ? num_states : CPLX_MAX_STATES;
    sim->tm.num_symbols = num_symbols < CPLX_MAX_SYMBOLS ? num_symbols : CPLX_MAX_SYMBOLS;
    sim->tm.accept_state = num_states - 2;
    sim->tm.reject_state = num_states - 1;
    sim->tm.head_position = CPLX_MAX_TAPE / 2;  /* start in middle */
    memset(sim->tm.tape, 0, CPLX_MAX_TAPE);
}

void complexity_theory_tm_add_transition(complexity_theory_sim_t* sim,
                                           uint32_t from, uint8_t read,
                                           uint32_t to, uint8_t write, int8_t move) {
    if (!sim || sim->tm.num_transitions >= CPLX_MAX_STATES * CPLX_MAX_SYMBOLS) return;
    uint32_t idx = sim->tm.num_transitions++;
    sim->tm.transitions[idx] = (cplx_tm_transition_t){
        .from_state = from, .read_symbol = read,
        .to_state = to, .write_symbol = write, .move = move
    };
}

void complexity_theory_tm_load_input(complexity_theory_sim_t* sim,
                                       const uint8_t* input, uint32_t length) {
    if (!sim || !input) return;
    uint32_t start = CPLX_MAX_TAPE / 2;
    uint32_t copy_len = length < (CPLX_MAX_TAPE - start) ? length : (CPLX_MAX_TAPE - start);
    memcpy(&sim->tm.tape[start], input, copy_len);
    sim->tm.head_position = (int32_t)start;
    sim->tm.current_state = sim->tm.start_state;
    sim->tm.halted = false;
    sim->tm.accepted = false;
    sim->tm.steps_executed = 0;
}

bool complexity_theory_tm_step(complexity_theory_sim_t* sim) {
    if (!sim || sim->tm.halted) return true;

    uint32_t state = sim->tm.current_state;
    if (state == sim->tm.accept_state) { sim->tm.halted = true; sim->tm.accepted = true; return true; }
    if (state == sim->tm.reject_state) { sim->tm.halted = true; sim->tm.accepted = false; return true; }

    int32_t pos = sim->tm.head_position;
    if (pos < 0 || pos >= CPLX_MAX_TAPE) { sim->tm.halted = true; return true; }
    uint8_t read = sim->tm.tape[pos];

    /* Find matching transition */
    for (uint32_t t = 0; t < sim->tm.num_transitions; t++) {
        cplx_tm_transition_t* tr = &sim->tm.transitions[t];
        if (tr->from_state == state && tr->read_symbol == read) {
            sim->tm.tape[pos] = tr->write_symbol;
            sim->tm.current_state = tr->to_state;
            sim->tm.head_position += tr->move;
            sim->tm.steps_executed++;
            sim->stats.tm_steps_total++;
            return false;  /* not halted */
        }
    }

    /* No matching transition → halt (reject) */
    sim->tm.halted = true;
    sim->tm.accepted = false;
    return true;
}

bool complexity_theory_tm_run(complexity_theory_sim_t* sim, uint64_t max_steps) {
    if (!sim) return true;
    if (max_steps == 0) max_steps = sim->config.max_tm_steps;
    for (uint64_t s = 0; s < max_steps; s++) {
        if (complexity_theory_tm_step(sim)) return true;
    }
    return false;  /* didn't halt within max_steps */
}

bool complexity_theory_tm_accepted(const complexity_theory_sim_t* sim) {
    return sim ? sim->tm.accepted : false;
}

/* ============================================================================
 * Master Theorem
 * ============================================================================ */

cplx_class_t complexity_theory_master_theorem(uint32_t a, uint32_t b,
                                                float f_exponent) {
    /* T(n) = aT(n/b) + Θ(n^f_exponent)
     * c_crit = log_b(a)
     * Case 1: f_exp < c_crit → T = Θ(n^c_crit)
     * Case 2: f_exp = c_crit → T = Θ(n^c_crit · log n)
     * Case 3: f_exp > c_crit → T = Θ(n^f_exp) */
    if (b <= 1 || a == 0) return CPLX_CLASS_ON;
    double c_crit = log((double)a) / log((double)b);
    double eps = 0.1;

    if ((double)f_exponent < c_crit - eps) {
        /* Case 1 */
        if (c_crit < 1.3) return CPLX_CLASS_ON;
        if (c_crit < 2.3) return CPLX_CLASS_ON2;
        if (c_crit < 3.3) return CPLX_CLASS_ON3;
        return CPLX_CLASS_ONK;
    } else if ((double)f_exponent > c_crit + eps) {
        /* Case 3 */
        if (f_exponent < 1.3f) return CPLX_CLASS_ON;
        if (f_exponent < 2.3f) return CPLX_CLASS_ON2;
        return CPLX_CLASS_ONK;
    } else {
        /* Case 2 */
        if (c_crit < 0.7) return CPLX_CLASS_OLOGN;
        if (c_crit < 1.3) return CPLX_CLASS_ONLOGN;
        return CPLX_CLASS_ONK;
    }
}

double complexity_theory_amortized(const double* op_costs, uint32_t num_ops) {
    if (!op_costs || num_ops == 0) return 0;
    double total = 0;
    for (uint32_t i = 0; i < num_ops; i++) total += op_costs[i];
    return total / (double)num_ops;
}

cplx_stats_t complexity_theory_get_stats(const complexity_theory_sim_t* sim) {
    if (!sim) return (cplx_stats_t){0};
    return sim->stats;
}
