/**
 * @file nimcp_number_theory.h
 * @brief Number theory engine for NIMCP cognitive mathematics
 *
 * Provides primality testing (trial division + Miller-Rabin), Sieve of
 * Eratosthenes, prime factorization, GCD/LCM, extended Euclidean algorithm,
 * modular exponentiation, modular inverse, Euler's totient, Moebius function,
 * Chinese Remainder Theorem, Legendre symbol, and quadratic residues.
 */

#ifndef NIMCP_NUMBER_THEORY_H
#define NIMCP_NUMBER_THEORY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * Constants
 * -------------------------------------------------------------------------- */

#define NT_MAX_SIEVE_SIZE       1000000   /* 1M upper bound for sieve       */
#define NT_MAX_FACTORS          64        /* max distinct prime factors      */
#define NT_MILLER_RABIN_ROUNDS  20        /* deterministic for < 3.3e24     */
#define NT_MAX_CRT_MODULI       32        /* max simultaneous CRT congruences */

/* --------------------------------------------------------------------------
 * Structures
 * -------------------------------------------------------------------------- */

/** Single prime-power factor: p^e */
typedef struct {
    uint64_t prime;
    uint32_t exponent;
} prime_factor_t;

/** Result of prime factorization */
typedef struct {
    prime_factor_t factors[NT_MAX_FACTORS];
    uint32_t       count;           /* number of distinct primes */
} factorization_t;

/** Extended GCD result: ax + by = gcd */
typedef struct {
    int64_t gcd;
    int64_t x;
    int64_t y;
} ext_gcd_result_t;

/** Chinese Remainder Theorem input/output */
typedef struct {
    uint64_t remainders[NT_MAX_CRT_MODULI];
    uint64_t moduli[NT_MAX_CRT_MODULI];
    uint32_t count;
    uint64_t solution;              /* filled by nt_crt_solve */
    uint64_t combined_modulus;      /* product of moduli       */
    bool     valid;                 /* false if moduli not coprime */
} crt_system_t;

/** Sieve cache for the number theory engine */
typedef struct {
    bool    *is_prime;              /* boolean sieve array [0..limit] */
    uint32_t limit;                 /* upper bound of current sieve   */

    uint64_t *prime_list;           /* packed list of primes <= limit */
    uint32_t  prime_count;          /* number of primes in list       */
} nt_sieve_cache_t;

/** Top-level number theory engine */
typedef struct {
    nt_sieve_cache_t *sieve;       /* lazily allocated sieve cache */
    uint32_t          sieve_limit; /* requested sieve limit        */
} number_theory_t;

/* --------------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------------- */

number_theory_t *nt_create(void);
void             nt_destroy(number_theory_t *nt);

/* --------------------------------------------------------------------------
 * Sieve
 * -------------------------------------------------------------------------- */

/** Build or rebuild sieve up to limit (clamped to NT_MAX_SIEVE_SIZE). */
bool nt_build_sieve(number_theory_t *nt, uint32_t limit);

/** Query sieve (builds if needed). */
bool nt_is_prime_sieve(number_theory_t *nt, uint32_t n);

/** Return count of primes <= n from sieve. */
uint32_t nt_prime_count(number_theory_t *nt, uint32_t n);

/** Copy up to max_out primes <= n into out[]. Returns count copied. */
uint32_t nt_get_primes(number_theory_t *nt, uint32_t n,
                       uint64_t *out, uint32_t max_out);

/* --------------------------------------------------------------------------
 * Primality
 * -------------------------------------------------------------------------- */

/** Trial division (deterministic, O(sqrt(n))). */
bool nt_is_prime_trial(uint64_t n);

/** Miller-Rabin probabilistic test with k rounds. */
bool nt_is_prime_miller_rabin(uint64_t n, uint32_t k);

/** Combined: sieve for small, Miller-Rabin for large. */
bool nt_is_prime(number_theory_t *nt, uint64_t n);

/* --------------------------------------------------------------------------
 * Factorization
 * -------------------------------------------------------------------------- */

/** Full prime factorization of n. */
factorization_t nt_factorize(number_theory_t *nt, uint64_t n);

/* --------------------------------------------------------------------------
 * GCD / LCM / Extended Euclidean
 * -------------------------------------------------------------------------- */

uint64_t        nt_gcd(uint64_t a, uint64_t b);
uint64_t        nt_lcm(uint64_t a, uint64_t b);
ext_gcd_result_t nt_extended_gcd(int64_t a, int64_t b);

/* --------------------------------------------------------------------------
 * Modular arithmetic
 * -------------------------------------------------------------------------- */

/** Compute (base^exp) mod m using binary exponentiation. */
uint64_t nt_mod_pow(uint64_t base, uint64_t exp, uint64_t m);

/** Modular inverse of a mod m (returns 0 if gcd(a,m) != 1). */
uint64_t nt_mod_inverse(uint64_t a, uint64_t m);

/* --------------------------------------------------------------------------
 * Multiplicative functions
 * -------------------------------------------------------------------------- */

/** Euler's totient phi(n). */
uint64_t nt_euler_totient(number_theory_t *nt, uint64_t n);

/** Moebius function mu(n): 1, -1, or 0. */
int nt_moebius(number_theory_t *nt, uint64_t n);

/* --------------------------------------------------------------------------
 * Chinese Remainder Theorem
 * -------------------------------------------------------------------------- */

/** Solve system of congruences. Fills crt->solution and crt->valid. */
bool nt_crt_solve(crt_system_t *crt);

/* --------------------------------------------------------------------------
 * Quadratic residues
 * -------------------------------------------------------------------------- */

/** Legendre symbol (a/p) for odd prime p. Returns -1, 0, or 1. */
int nt_legendre_symbol(int64_t a, uint64_t p);

/** Check whether a is a quadratic residue mod p (odd prime). */
bool nt_is_quadratic_residue(uint64_t a, uint64_t p);

/** Count quadratic residues mod p (odd prime). */
uint32_t nt_count_quadratic_residues(uint64_t p);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NUMBER_THEORY_H */
