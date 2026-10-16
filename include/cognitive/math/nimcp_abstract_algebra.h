/**
 * @file nimcp_abstract_algebra.h
 * @brief Abstract algebra engine: groups, rings, fields, permutations, polynomials
 *
 * Groups with Cayley tables, ring/field axiom verification, finite field GF(p)
 * arithmetic, permutation algebra (compose, inverse, cycle decomposition, parity),
 * and polynomials over GF(p).
 */

#ifndef NIMCP_ABSTRACT_ALGEBRA_H
#define NIMCP_ABSTRACT_ALGEBRA_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * Constants
 * -------------------------------------------------------------------------- */

#define AA_MAX_GROUP_ORDER   64
#define AA_MAX_POLY_DEGREE   128
#define AA_MAX_PERM_SIZE     64
#define AA_MAX_CYCLES        64

/* --------------------------------------------------------------------------
 * Group
 * -------------------------------------------------------------------------- */

typedef struct {
    uint32_t order;                             /* number of elements        */
    int32_t  cayley[AA_MAX_GROUP_ORDER][AA_MAX_GROUP_ORDER]; /* a*b table   */
    int32_t  identity;                          /* identity element index    */
    bool     valid;                             /* axioms verified           */
} group_t;

/** Axiom check results */
typedef struct {
    bool closure;
    bool associativity;
    bool has_identity;
    bool has_inverses;
    bool is_group;
    bool is_abelian;
} group_axiom_result_t;

/** Coset result */
typedef struct {
    int32_t  elements[AA_MAX_GROUP_ORDER];
    uint32_t size;
} coset_t;

/* --------------------------------------------------------------------------
 * Ring
 * -------------------------------------------------------------------------- */

typedef struct {
    uint32_t order;
    int32_t  add_table[AA_MAX_GROUP_ORDER][AA_MAX_GROUP_ORDER];
    int32_t  mul_table[AA_MAX_GROUP_ORDER][AA_MAX_GROUP_ORDER];
    int32_t  zero;                              /* additive identity  */
    int32_t  one;                               /* multiplicative id (-1 if none) */
    bool     valid;
} ring_t;

typedef struct {
    bool add_closure;
    bool add_associativity;
    bool add_commutativity;
    bool add_identity;
    bool add_inverses;
    bool mul_closure;
    bool mul_associativity;
    bool distributive_left;
    bool distributive_right;
    bool is_ring;
    bool is_commutative_ring;
    bool has_unity;
} ring_axiom_result_t;

/* --------------------------------------------------------------------------
 * Finite field GF(p)
 * -------------------------------------------------------------------------- */

typedef struct {
    uint64_t p;                                 /* prime modulus */
} gf_field_t;

/* --------------------------------------------------------------------------
 * Permutation
 * -------------------------------------------------------------------------- */

typedef struct {
    uint32_t n;                                 /* degree (permutes 0..n-1) */
    uint32_t image[AA_MAX_PERM_SIZE];           /* sigma(i)                 */
} permutation_t;

typedef struct {
    uint32_t cycle[AA_MAX_PERM_SIZE];           /* elements in this cycle */
    uint32_t length;
} cycle_t;

typedef struct {
    cycle_t  cycles[AA_MAX_CYCLES];
    uint32_t num_cycles;
} cycle_decomposition_t;

/* --------------------------------------------------------------------------
 * Polynomial over GF(p)
 * -------------------------------------------------------------------------- */

typedef struct {
    int64_t  coeffs[AA_MAX_POLY_DEGREE + 1];   /* coeffs[i] = coeff of x^i */
    uint32_t degree;                            /* actual degree             */
    uint64_t p;                                 /* characteristic            */
} gfp_poly_t;

/* --------------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------------- */

/** Group */
group_t *aa_group_create(uint32_t order);
void     aa_group_destroy(group_t *g);

/** Ring */
ring_t *aa_ring_create(uint32_t order);
void    aa_ring_destroy(ring_t *r);

/** GF(p) field */
gf_field_t *aa_gf_create(uint64_t p);
void        aa_gf_destroy(gf_field_t *f);

/* --------------------------------------------------------------------------
 * Group API
 * -------------------------------------------------------------------------- */

group_axiom_result_t aa_group_check_axioms(const group_t *g);
int32_t  aa_group_find_identity(const group_t *g);
int32_t  aa_group_inverse(const group_t *g, int32_t elem);
uint32_t aa_group_element_order(const group_t *g, int32_t elem);
bool     aa_group_is_subgroup(const group_t *g, const int32_t *subset, uint32_t size);
coset_t  aa_group_left_coset(const group_t *g, int32_t elem,
                             const int32_t *subgroup, uint32_t sg_size);
bool     aa_group_verify_lagrange(const group_t *g, const int32_t *subgroup,
                                  uint32_t sg_size);

/* --------------------------------------------------------------------------
 * Ring API
 * -------------------------------------------------------------------------- */

ring_axiom_result_t aa_ring_check_axioms(const ring_t *r);
bool aa_ring_is_ideal(const ring_t *r, const int32_t *subset, uint32_t size);

/* --------------------------------------------------------------------------
 * GF(p) API
 * -------------------------------------------------------------------------- */

uint64_t aa_gf_add(const gf_field_t *f, uint64_t a, uint64_t b);
uint64_t aa_gf_sub(const gf_field_t *f, uint64_t a, uint64_t b);
uint64_t aa_gf_mul(const gf_field_t *f, uint64_t a, uint64_t b);
uint64_t aa_gf_inv(const gf_field_t *f, uint64_t a);
uint64_t aa_gf_div(const gf_field_t *f, uint64_t a, uint64_t b);
uint64_t aa_gf_pow(const gf_field_t *f, uint64_t base, uint64_t exp);

/* --------------------------------------------------------------------------
 * Permutation API
 * -------------------------------------------------------------------------- */

permutation_t aa_perm_identity(uint32_t n);
permutation_t aa_perm_compose(const permutation_t *a, const permutation_t *b);
permutation_t aa_perm_inverse(const permutation_t *p);
cycle_decomposition_t aa_perm_cycle_decomposition(const permutation_t *p);
bool     aa_perm_is_even(const permutation_t *p);
uint32_t aa_perm_order(const permutation_t *p);

/* --------------------------------------------------------------------------
 * Polynomial over GF(p) API
 * -------------------------------------------------------------------------- */

gfp_poly_t aa_poly_create(uint64_t p, uint32_t degree, const int64_t *coeffs);
gfp_poly_t aa_poly_add(const gfp_poly_t *a, const gfp_poly_t *b);
gfp_poly_t aa_poly_mul(const gfp_poly_t *a, const gfp_poly_t *b);
int64_t    aa_poly_eval(const gfp_poly_t *f, int64_t x);
gfp_poly_t aa_poly_gcd(const gfp_poly_t *a, const gfp_poly_t *b);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ABSTRACT_ALGEBRA_H */
