/**
 * @file nimcp_combinatorics.h
 * @brief Combinatorial mathematics engine
 *
 * Factorial, binomial coefficients, permutations, Catalan numbers,
 * Stirling numbers, Bell numbers, partitions, Fibonacci, multinomial
 * coefficients, derangements, inclusion-exclusion, generating functions.
 */

#ifndef NIMCP_COMBINATORICS_H
#define NIMCP_COMBINATORICS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- constants ---------- */
#define COMB_FACTORIAL_CACHE_SIZE  21   /* 0! through 20! */
#define COMB_MAX_BELL_N           30
#define COMB_MAX_PARTITION_N      200
#define COMB_MAX_GF_TERMS         64

/* ---------- types ---------- */

/** Main combinatorics engine handle */
typedef struct combinatorics_s {
    uint64_t factorial_cache[COMB_FACTORIAL_CACHE_SIZE]; /* 0!..20! */
    bool     cache_initialized;
} combinatorics_t;

/** Ordinary generating function (polynomial) */
typedef struct ogf_s {
    double  coeffs[COMB_MAX_GF_TERMS];
    int     n_terms;
} ogf_t;

/* ---------- lifecycle ---------- */
combinatorics_t* combinatorics_create(void);
void             combinatorics_destroy(combinatorics_t* ctx);

/* ---------- basic counts ---------- */
uint64_t comb_factorial(combinatorics_t* ctx, int n);
uint64_t comb_binomial(combinatorics_t* ctx, int n, int k);
uint64_t comb_permutation(combinatorics_t* ctx, int n, int k);
uint64_t comb_catalan(combinatorics_t* ctx, int n);

/* ---------- Stirling numbers ---------- */
/** Unsigned Stirling number of the first kind |s(n,k)| */
int64_t  comb_stirling_first(int n, int k);
/** Stirling number of the second kind S(n,k) */
int64_t  comb_stirling_second(int n, int k);

/* ---------- Bell numbers ---------- */
uint64_t comb_bell(int n);

/* ---------- partition function ---------- */
/** p(n) = number of integer partitions of n */
uint64_t comb_partition(int n);

/* ---------- Fibonacci ---------- */
uint64_t comb_fibonacci(int n);

/* ---------- multinomial ---------- */
/** Multinomial coefficient n! / (k1!*k2!*...*km!) */
uint64_t comb_multinomial(combinatorics_t* ctx, int n, const int* k, int m);

/* ---------- derangements ---------- */
/** D(n) = n! * sum_{i=0}^{n} (-1)^i / i! */
uint64_t comb_derangement(combinatorics_t* ctx, int n);

/* ---------- inclusion-exclusion ---------- */
/**
 * Given |A_i| for each of m sets, and all pairwise |A_i ∩ A_j|, etc.,
 * compute |A_1 ∪ A_2 ∪ ... ∪ A_m| via inclusion-exclusion.
 * @param sizes   Array of 2^m - 1 entries: sizes[mask-1] = |intersection of sets in mask|
 * @param m       Number of sets (max 20)
 * @return        Size of the union
 */
int64_t comb_inclusion_exclusion(const int64_t* sizes, int m);

/* ---------- generating functions ---------- */
ogf_t   comb_ogf_create(const double* coeffs, int n_terms);
double  comb_ogf_evaluate(const ogf_t* gf, double x);
ogf_t   comb_ogf_multiply(const ogf_t* a, const ogf_t* b);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COMBINATORICS_H */
