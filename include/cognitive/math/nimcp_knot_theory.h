/**
 * @file nimcp_knot_theory.h
 * @brief Knot Theory — invariants, polynomials, braids, links, Reidemeister moves
 *
 * WHAT: Represents knots as planar diagrams (crossing sequences), computes
 *       invariants (writhe, linking number, Jones polynomial, Alexander
 *       polynomial, HOMFLY-PT), classifies knots by crossing number,
 *       detects unknots, Reidemeister move simplification, braid groups.
 * WHY:  Knot theory connects to quantum field theory (Witten's Chern-Simons),
 *       DNA topology (supercoiling, recombination), statistical mechanics
 *       (Yang-Baxter equation), and quantum computing (topological QC).
 *       Also connects to the Jones polynomial ↔ Potts model ↔ stat mech.
 * HOW:  Gauss codes for knot diagrams, skein relations for polynomial
 *       computation, Seifert matrix for Alexander polynomial, braid word
 *       representation, Markov equivalence.
 *
 * THEORETICAL FOUNDATION:
 *   - Reidemeister moves: R1 (twist), R2 (poke), R3 (slide) — equivalence
 *   - Writhe: w(K) = Σ sign(crossing) — NOT a knot invariant alone
 *   - Linking number: lk(L₁,L₂) = (1/2)Σ sign(crossing between components)
 *   - Alexander polynomial: det(tV - t⁻¹Vᵀ) from Seifert matrix V
 *   - Jones polynomial: skein relation q⁻¹V(L₊) - qV(L₋) = (q^½-q^-½)V(L₀)
 *   - HOMFLY-PT: αP(L₊) + α⁻¹P(L₋) = zP(L₀) (two-variable generalization)
 *   - Bracket polynomial: <K> via Kauffman states (A/B smoothings)
 *   - Genus: g(K) = (1 - χ(Seifert surface))/2
 *   - Bridge number, tunnel number, unknotting number
 */

#ifndef NIMCP_KNOT_THEORY_H
#define NIMCP_KNOT_THEORY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define KNOT_MAX_CROSSINGS      64
#define KNOT_MAX_ARCS           128
#define KNOT_MAX_COMPONENTS     8       /* for links */
#define KNOT_MAX_POLY_DEGREE    32      /* max polynomial degree */
#define KNOT_MAX_BRAID_LEN      128
#define KNOT_MAX_BRAID_STRANDS  16
#define KNOT_MAX_NAME           32

/* ============================================================================
 * Crossing
 * ============================================================================ */

typedef enum {
    KNOT_CROSSING_POSITIVE = +1,    /* right-handed: over strand goes left-to-right */
    KNOT_CROSSING_NEGATIVE = -1,    /* left-handed: over strand goes right-to-left */
} knot_crossing_sign_t;

typedef struct {
    uint32_t    id;
    int8_t      sign;               /* +1 or -1 */
    uint32_t    over_arc_in;        /* arc entering on top */
    uint32_t    over_arc_out;       /* arc leaving on top */
    uint32_t    under_arc_in;       /* arc entering below */
    uint32_t    under_arc_out;      /* arc leaving below */
    uint32_t    component;          /* which link component (0 for knots) */
} knot_crossing_t;

/* ============================================================================
 * Knot/Link Diagram (planar projection)
 * ============================================================================ */

typedef struct {
    uint32_t            id;
    char                name[KNOT_MAX_NAME];    /* "trefoil", "figure-8", "5_1" */
    knot_crossing_t     crossings[KNOT_MAX_CROSSINGS];
    uint32_t            num_crossings;
    uint32_t            num_components;         /* 1=knot, >1=link */
    /* Gauss code: sequence of crossing encounters along the knot */
    int32_t             gauss_code[KNOT_MAX_CROSSINGS * 2];
    uint32_t            gauss_code_len;
    /* Dowker-Thistlethwaite notation */
    int32_t             dt_code[KNOT_MAX_CROSSINGS];
    uint32_t            dt_code_len;
    bool                oriented;
    bool                active;
} knot_diagram_t;

/* ============================================================================
 * Polynomial (Laurent polynomial in one or two variables)
 * ============================================================================ */

typedef struct {
    /* Coefficients for q^min_exp, q^(min_exp+1), ..., q^(min_exp+num_terms-1) */
    double      coeffs[KNOT_MAX_POLY_DEGREE * 2 + 1];
    int32_t     min_exp;            /* minimum exponent */
    uint32_t    num_terms;
} knot_polynomial_t;

/* ============================================================================
 * Braid Word
 * ============================================================================ */

typedef struct {
    int32_t     generators[KNOT_MAX_BRAID_LEN]; /* σᵢ = +i, σᵢ⁻¹ = -i */
    uint32_t    length;
    uint32_t    num_strands;
} knot_braid_t;

/* ============================================================================
 * Seifert Matrix
 * ============================================================================ */

typedef struct {
    double      matrix[KNOT_MAX_CROSSINGS * KNOT_MAX_CROSSINGS];
    uint32_t    dim;                /* genus × 2 */
} knot_seifert_matrix_t;

/* ============================================================================
 * Knot Invariants
 * ============================================================================ */

typedef struct {
    int32_t     writhe;             /* sum of crossing signs */
    int32_t     linking_number;     /* for links: (1/2)Σsign of inter-component crossings */
    uint32_t    crossing_number;    /* minimum crossings in any diagram */
    uint32_t    unknotting_number;  /* min crossing changes to unknot */
    uint32_t    bridge_number;      /* min bridges in any diagram */
    uint32_t    seifert_genus;      /* genus of Seifert surface */
    uint32_t    num_seifert_circles;/* from Seifert algorithm */
    bool        is_alternating;     /* crossings alternate over/under */
    bool        is_torus_knot;
    bool        is_satellite;
    bool        is_unknot;          /* trivially knotted? */
    knot_polynomial_t alexander;    /* Alexander polynomial Δ(t) */
    knot_polynomial_t jones;        /* Jones polynomial V(q) */
    knot_polynomial_t bracket;      /* Kauffman bracket <K>(A) */
} knot_invariants_t;

/* ============================================================================
 * Known Knot Table
 * ============================================================================ */

typedef enum {
    KNOT_UNKNOT     = 0,    /* 0_1: trivial knot */
    KNOT_TREFOIL    = 1,    /* 3_1: simplest nontrivial */
    KNOT_FIGURE_8   = 2,    /* 4_1: first amphicheiral knot */
    KNOT_CINQUEFOIL = 3,    /* 5_1: torus knot T(5,2) */
    KNOT_5_2        = 4,    /* 5_2 */
    KNOT_GRANNY     = 5,    /* 3_1 # 3_1: connected sum */
    KNOT_SQUARE     = 6,    /* 3_1 # 3_1*: connected sum with mirror */
    KNOT_HOPF_LINK  = 7,    /* simplest nontrivial link (2 components) */
    KNOT_TREFOIL_LINK = 8,  /* trefoil as 2-component link */
    KNOT_BORROMEAN  = 9,    /* Borromean rings (3 components, pairwise unlinked) */
    KNOT_TABLE_SIZE
} knot_table_id_t;

/* ============================================================================
 * Config & Stats
 * ============================================================================ */

typedef struct {
    uint32_t    max_simplification_steps;   /* Reidemeister move budget */
    bool        compute_jones;              /* Jones polynomial (expensive) */
    bool        compute_alexander;
    bool        compute_bracket;
} knot_config_t;

typedef struct {
    uint64_t    knots_analyzed;
    uint64_t    polynomials_computed;
    uint64_t    reidemeister_moves;
    uint64_t    unknots_detected;
} knot_stats_t;

/* ============================================================================
 * Engine
 * ============================================================================ */

typedef struct knot_theory_engine {
    knot_diagram_t      diagrams[16];       /* working diagrams */
    uint32_t            num_diagrams;
    knot_config_t       config;
    knot_stats_t        stats;
    bool                initialized;
} knot_theory_engine_t;

/* ============================================================================
 * API
 * ============================================================================ */

knot_theory_engine_t* knot_theory_create(const knot_config_t* config);
void knot_theory_destroy(knot_theory_engine_t* engine);

/* === Diagram Construction === */

/** Create knot from Gauss code */
uint32_t knot_from_gauss_code(knot_theory_engine_t* engine,
                                const int32_t* gauss_code, uint32_t length,
                                const char* name);

/** Create knot from DT code */
uint32_t knot_from_dt_code(knot_theory_engine_t* engine,
                             const int32_t* dt_code, uint32_t num_crossings,
                             const char* name);

/** Create knot from braid closure */
uint32_t knot_from_braid(knot_theory_engine_t* engine,
                           const knot_braid_t* braid, const char* name);

/** Load a known knot from the table */
uint32_t knot_load_known(knot_theory_engine_t* engine, knot_table_id_t id);

/* === Invariant Computation === */

/** Compute all invariants for a knot diagram */
knot_invariants_t knot_compute_invariants(knot_theory_engine_t* engine,
                                            uint32_t diagram_id);

/** Compute writhe: Σ sign(crossing) */
int32_t knot_writhe(const knot_diagram_t* diagram);

/** Compute linking number (for links with 2+ components) */
int32_t knot_linking_number(const knot_diagram_t* diagram);

/** Count Seifert circles (from Seifert's algorithm) */
uint32_t knot_seifert_circles(const knot_diagram_t* diagram);

/** Compute Seifert genus: g = (c - s + 1)/2 where c=crossings, s=Seifert circles */
uint32_t knot_seifert_genus(const knot_diagram_t* diagram);

/** Compute Seifert matrix */
knot_seifert_matrix_t knot_seifert_matrix(const knot_diagram_t* diagram);

/** Compute Alexander polynomial from Seifert matrix: Δ(t) = det(tV - t⁻¹Vᵀ) */
knot_polynomial_t knot_alexander_polynomial(knot_theory_engine_t* engine,
                                              uint32_t diagram_id);

/** Compute Jones polynomial via Kauffman bracket + writhe normalization */
knot_polynomial_t knot_jones_polynomial(knot_theory_engine_t* engine,
                                          uint32_t diagram_id);

/** Compute Kauffman bracket polynomial <K>(A) via state sum */
knot_polynomial_t knot_bracket_polynomial(knot_theory_engine_t* engine,
                                            uint32_t diagram_id);

/* === Detection & Classification === */

/** Is this the unknot? (uses invariant checks) */
bool knot_is_unknot(knot_theory_engine_t* engine, uint32_t diagram_id);

/** Is this knot alternating? */
bool knot_is_alternating(const knot_diagram_t* diagram);

/** Are two knots equivalent? (compare invariants — necessary but not sufficient) */
bool knot_are_equivalent(knot_theory_engine_t* engine,
                           uint32_t diagram_a, uint32_t diagram_b);

/* === Simplification === */

/** Apply Reidemeister moves to simplify the diagram */
uint32_t knot_simplify(knot_theory_engine_t* engine, uint32_t diagram_id,
                         uint32_t max_moves);

/** Try to reduce crossing number */
uint32_t knot_reduce_crossings(knot_theory_engine_t* engine, uint32_t diagram_id);

/* === Braid Operations === */

/** Compose two braids (concatenate words) */
knot_braid_t knot_braid_compose(const knot_braid_t* a, const knot_braid_t* b);

/** Inverse of a braid word */
knot_braid_t knot_braid_inverse(const knot_braid_t* braid);

/** Braid word length (number of generators) */
uint32_t knot_braid_length(const knot_braid_t* braid);

/* === Polynomial Operations === */

/** Evaluate polynomial at a point */
double knot_poly_eval(const knot_polynomial_t* p, double x);

/** Multiply two polynomials */
knot_polynomial_t knot_poly_multiply(const knot_polynomial_t* a,
                                       const knot_polynomial_t* b);

/** Add two polynomials */
knot_polynomial_t knot_poly_add(const knot_polynomial_t* a,
                                  const knot_polynomial_t* b);

/** Print polynomial as string (for debugging) */
void knot_poly_print(const knot_polynomial_t* p, char* buf, uint32_t buf_size);

/* === Connected Sum === */

/** Compute connected sum K₁ # K₂ */
uint32_t knot_connected_sum(knot_theory_engine_t* engine,
                              uint32_t diagram_a, uint32_t diagram_b,
                              const char* name);

/* === Utility === */

knot_config_t knot_theory_default_config(void);
knot_stats_t knot_theory_get_stats(const knot_theory_engine_t* engine);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KNOT_THEORY_H */
