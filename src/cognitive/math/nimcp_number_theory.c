/**
 * @file nimcp_number_theory.c
 * @brief Number theory engine implementation
 */

#include "cognitive/math/nimcp_number_theory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "NUM_THEORY"

/* =========================================================================
 * Helpers
 * ========================================================================= */

/** Modular multiplication avoiding overflow via __uint128_t. */
static uint64_t mulmod(uint64_t a, uint64_t b, uint64_t m) {
    return (uint64_t)(((unsigned __int128)a * b) % m);
}

/* =========================================================================
 * Lifecycle
 * ========================================================================= */

number_theory_t *nt_create(void) {
    number_theory_t *nt = (number_theory_t *)nimcp_calloc(1, sizeof(number_theory_t));
    if (!nt) {
        LOG_ERROR("Failed to allocate number_theory_t");
        return NULL;
    }
    nt->sieve = NULL;
    nt->sieve_limit = 0;
    LOG_INFO("Number theory engine created");
    return nt;
}

void nt_destroy(number_theory_t *nt) {
    if (!nt) return;
    if (nt->sieve) {
        if (nt->sieve->is_prime) nimcp_free(nt->sieve->is_prime);
        if (nt->sieve->prime_list) nimcp_free(nt->sieve->prime_list);
        nimcp_free(nt->sieve);
    }
    nimcp_free(nt);
    LOG_INFO("Number theory engine destroyed");
}

/* =========================================================================
 * Sieve of Eratosthenes
 * ========================================================================= */

bool nt_build_sieve(number_theory_t *nt, uint32_t limit) {
    if (!nt) return false;
    if (limit > NT_MAX_SIEVE_SIZE) limit = NT_MAX_SIEVE_SIZE;
    if (limit < 2) limit = 2;

    /* Free existing sieve */
    if (nt->sieve) {
        if (nt->sieve->is_prime) nimcp_free(nt->sieve->is_prime);
        if (nt->sieve->prime_list) nimcp_free(nt->sieve->prime_list);
        nimcp_free(nt->sieve);
        nt->sieve = NULL;
    }

    nt->sieve = (nt_sieve_cache_t *)nimcp_calloc(1, sizeof(nt_sieve_cache_t));
    if (!nt->sieve) return false;

    nt->sieve->limit = limit;
    nt->sieve->is_prime = (bool *)nimcp_calloc(limit + 1, sizeof(bool));
    if (!nt->sieve->is_prime) {
        nimcp_free(nt->sieve);
        nt->sieve = NULL;
        return false;
    }

    /* Initialize all as prime, then sieve */
    for (uint32_t i = 2; i <= limit; i++) {
        nt->sieve->is_prime[i] = true;
    }

    uint32_t sq = (uint32_t)sqrt((double)limit);
    for (uint32_t i = 2; i <= sq; i++) {
        if (nt->sieve->is_prime[i]) {
            for (uint32_t j = i * i; j <= limit; j += i) {
                nt->sieve->is_prime[j] = false;
            }
        }
    }

    /* Count primes and build list */
    uint32_t count = 0;
    for (uint32_t i = 2; i <= limit; i++) {
        if (nt->sieve->is_prime[i]) count++;
    }

    nt->sieve->prime_count = count;
    nt->sieve->prime_list = (uint64_t *)nimcp_calloc(count, sizeof(uint64_t));
    if (!nt->sieve->prime_list) {
        nimcp_free(nt->sieve->is_prime);
        nimcp_free(nt->sieve);
        nt->sieve = NULL;
        return false;
    }

    uint32_t idx = 0;
    for (uint32_t i = 2; i <= limit; i++) {
        if (nt->sieve->is_prime[i]) {
            nt->sieve->prime_list[idx++] = i;
        }
    }

    nt->sieve_limit = limit;
    LOG_INFO("Sieve built: %u primes up to %u", count, limit);
    return true;
}

bool nt_is_prime_sieve(number_theory_t *nt, uint32_t n) {
    if (!nt || n < 2) return false;
    if (!nt->sieve || n > nt->sieve->limit) {
        uint32_t new_limit = (n > NT_MAX_SIEVE_SIZE) ? NT_MAX_SIEVE_SIZE : n;
        if (!nt_build_sieve(nt, new_limit)) return false;
        if (n > nt->sieve->limit) return false;
    }
    return nt->sieve->is_prime[n];
}

uint32_t nt_prime_count(number_theory_t *nt, uint32_t n) {
    if (!nt) return 0;
    if (!nt->sieve || n > nt->sieve->limit) {
        if (!nt_build_sieve(nt, n)) return 0;
    }
    uint32_t count = 0;
    for (uint32_t i = 0; i < nt->sieve->prime_count; i++) {
        if (nt->sieve->prime_list[i] <= n) count++;
        else break;
    }
    return count;
}

uint32_t nt_get_primes(number_theory_t *nt, uint32_t n,
                       uint64_t *out, uint32_t max_out) {
    if (!nt || !out || max_out == 0) return 0;
    if (!nt->sieve || n > nt->sieve->limit) {
        if (!nt_build_sieve(nt, n)) return 0;
    }
    uint32_t copied = 0;
    for (uint32_t i = 0; i < nt->sieve->prime_count && copied < max_out; i++) {
        if (nt->sieve->prime_list[i] <= n) {
            out[copied++] = nt->sieve->prime_list[i];
        } else break;
    }
    return copied;
}

/* =========================================================================
 * Primality testing
 * ========================================================================= */

bool nt_is_prime_trial(uint64_t n) {
    if (n < 2) return false;
    if (n < 4) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    uint64_t sq = (uint64_t)sqrt((double)n);
    for (uint64_t i = 5; i <= sq; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) return false;
    }
    return true;
}

bool nt_is_prime_miller_rabin(uint64_t n, uint32_t k) {
    if (n < 2) return false;
    if (n == 2 || n == 3) return true;
    if (n % 2 == 0) return false;

    /* Write n-1 = 2^r * d with d odd */
    uint64_t d = n - 1;
    uint32_t r = 0;
    while ((d & 1) == 0) {
        d >>= 1;
        r++;
    }

    /* Deterministic witnesses for n < 3,317,044,064,679,887,385,961,981 */
    static const uint64_t witnesses[] = {
        2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37
    };
    uint32_t num_witnesses = sizeof(witnesses) / sizeof(witnesses[0]);
    if (k < num_witnesses) num_witnesses = k;

    for (uint32_t i = 0; i < num_witnesses; i++) {
        uint64_t a = witnesses[i];
        if (a >= n) continue;

        uint64_t x = nt_mod_pow(a, d, n);
        if (x == 1 || x == n - 1) continue;

        bool composite = true;
        for (uint32_t j = 0; j < r - 1; j++) {
            x = mulmod(x, x, n);
            if (x == n - 1) {
                composite = false;
                break;
            }
        }
        if (composite) return false;
    }
    return true;
}

bool nt_is_prime(number_theory_t *nt, uint64_t n) {
    if (n <= NT_MAX_SIEVE_SIZE && nt) {
        return nt_is_prime_sieve(nt, (uint32_t)n);
    }
    return nt_is_prime_miller_rabin(n, NT_MILLER_RABIN_ROUNDS);
}

/* =========================================================================
 * Factorization
 * ========================================================================= */

factorization_t nt_factorize(number_theory_t *nt, uint64_t n) {
    factorization_t result;
    memset(&result, 0, sizeof(result));
    if (n < 2) return result;

    /* Ensure sieve is built for trial division base */
    if (nt && (!nt->sieve || nt->sieve_limit < 1000)) {
        nt_build_sieve(nt, 100000);
    }

    uint64_t remaining = n;

    /* Trial division with small primes */
    uint32_t trial_limit = (nt && nt->sieve) ? nt->sieve->prime_count : 0;
    for (uint32_t i = 0; i < trial_limit && remaining > 1; i++) {
        uint64_t p = nt->sieve->prime_list[i];
        if (p * p > remaining) break;
        if (remaining % p == 0) {
            uint32_t exp = 0;
            while (remaining % p == 0) {
                remaining /= p;
                exp++;
            }
            if (result.count < NT_MAX_FACTORS) {
                result.factors[result.count].prime = p;
                result.factors[result.count].exponent = exp;
                result.count++;
            }
        }
    }

    /* Fallback trial division if no sieve */
    if (!nt || !nt->sieve) {
        uint64_t p = 2;
        while (p * p <= remaining && remaining > 1) {
            if (remaining % p == 0) {
                uint32_t exp = 0;
                while (remaining % p == 0) {
                    remaining /= p;
                    exp++;
                }
                if (result.count < NT_MAX_FACTORS) {
                    result.factors[result.count].prime = p;
                    result.factors[result.count].exponent = exp;
                    result.count++;
                }
            }
            p += (p == 2) ? 1 : 2;
        }
    }

    /* Remaining factor > sqrt(n) */
    if (remaining > 1 && result.count < NT_MAX_FACTORS) {
        result.factors[result.count].prime = remaining;
        result.factors[result.count].exponent = 1;
        result.count++;
    }

    return result;
}

/* =========================================================================
 * GCD / LCM / Extended Euclidean
 * ========================================================================= */

uint64_t nt_gcd(uint64_t a, uint64_t b) {
    while (b != 0) {
        uint64_t t = b;
        b = a % b;
        a = t;
    }
    return a;
}

uint64_t nt_lcm(uint64_t a, uint64_t b) {
    if (a == 0 || b == 0) return 0;
    return (a / nt_gcd(a, b)) * b;
}

ext_gcd_result_t nt_extended_gcd(int64_t a, int64_t b) {
    ext_gcd_result_t result;
    if (b == 0) {
        result.gcd = a;
        result.x = 1;
        result.y = 0;
        return result;
    }
    int64_t old_r = a, r = b;
    int64_t old_s = 1, s = 0;
    int64_t old_t = 0, t = 1;

    while (r != 0) {
        int64_t q = old_r / r;
        int64_t tmp;

        tmp = r; r = old_r - q * r; old_r = tmp;
        tmp = s; s = old_s - q * s; old_s = tmp;
        tmp = t; t = old_t - q * t; old_t = tmp;
    }

    result.gcd = old_r;
    result.x = old_s;
    result.y = old_t;
    if (result.gcd < 0) {
        result.gcd = -result.gcd;
        result.x = -result.x;
        result.y = -result.y;
    }
    return result;
}

/* =========================================================================
 * Modular arithmetic
 * ========================================================================= */

uint64_t nt_mod_pow(uint64_t base, uint64_t exp, uint64_t m) {
    if (m == 0) return 0;
    if (m == 1) return 0;
    uint64_t result = 1;
    base %= m;
    while (exp > 0) {
        if (exp & 1) {
            result = mulmod(result, base, m);
        }
        exp >>= 1;
        base = mulmod(base, base, m);
    }
    return result;
}

uint64_t nt_mod_inverse(uint64_t a, uint64_t m) {
    ext_gcd_result_t eg = nt_extended_gcd((int64_t)a, (int64_t)m);
    if (eg.gcd != 1) return 0; /* no inverse */
    int64_t x = eg.x % (int64_t)m;
    if (x < 0) x += (int64_t)m;
    return (uint64_t)x;
}

/* =========================================================================
 * Multiplicative functions
 * ========================================================================= */

uint64_t nt_euler_totient(number_theory_t *nt, uint64_t n) {
    if (n == 0) return 0;
    if (n == 1) return 1;

    factorization_t f = nt_factorize(nt, n);
    uint64_t result = n;
    for (uint32_t i = 0; i < f.count; i++) {
        uint64_t p = f.factors[i].prime;
        result = result / p * (p - 1);
    }
    return result;
}

int nt_moebius(number_theory_t *nt, uint64_t n) {
    if (n == 0) return 0;
    if (n == 1) return 1;

    factorization_t f = nt_factorize(nt, n);
    for (uint32_t i = 0; i < f.count; i++) {
        if (f.factors[i].exponent > 1) return 0; /* square factor */
    }
    return (f.count % 2 == 0) ? 1 : -1;
}

/* =========================================================================
 * Chinese Remainder Theorem
 * ========================================================================= */

bool nt_crt_solve(crt_system_t *crt) {
    if (!crt || crt->count == 0) return false;

    crt->valid = false;

    /* Compute product of all moduli */
    uint64_t N = 1;
    for (uint32_t i = 0; i < crt->count; i++) {
        if (crt->moduli[i] == 0) return false;
        N *= crt->moduli[i];
    }
    crt->combined_modulus = N;

    /* Check pairwise coprimality */
    for (uint32_t i = 0; i < crt->count; i++) {
        for (uint32_t j = i + 1; j < crt->count; j++) {
            if (nt_gcd(crt->moduli[i], crt->moduli[j]) != 1) {
                LOG_WARN("CRT: moduli[%u]=%lu and moduli[%u]=%lu not coprime",
                         i, (unsigned long)crt->moduli[i],
                         j, (unsigned long)crt->moduli[j]);
                return false;
            }
        }
    }

    /* Garner's algorithm / direct CRT construction */
    uint64_t solution = 0;
    for (uint32_t i = 0; i < crt->count; i++) {
        uint64_t Ni = N / crt->moduli[i];
        uint64_t yi = nt_mod_inverse(Ni % crt->moduli[i], crt->moduli[i]);
        if (yi == 0) return false;

        /* solution += a_i * N_i * y_i (mod N) */
        uint64_t term = mulmod(crt->remainders[i], mulmod(Ni, yi, N), N);
        solution = (solution + term) % N;
    }

    crt->solution = solution;
    crt->valid = true;
    return true;
}

/* =========================================================================
 * Quadratic residues / Legendre symbol
 * ========================================================================= */

int nt_legendre_symbol(int64_t a, uint64_t p) {
    if (p < 3) return 0; /* p must be odd prime */
    int64_t aa = a % (int64_t)p;
    if (aa < 0) aa += (int64_t)p;
    if (aa == 0) return 0;

    /* Euler's criterion: (a/p) = a^((p-1)/2) mod p */
    uint64_t result = nt_mod_pow((uint64_t)aa, (p - 1) / 2, p);
    if (result == 1) return 1;
    if (result == p - 1) return -1;
    return 0;
}

bool nt_is_quadratic_residue(uint64_t a, uint64_t p) {
    return nt_legendre_symbol((int64_t)(a % p), p) == 1;
}

uint32_t nt_count_quadratic_residues(uint64_t p) {
    if (p < 3) return 0;
    /* For odd prime p, there are exactly (p-1)/2 quadratic residues mod p */
    /* Verify by counting */
    uint32_t count = 0;
    for (uint64_t a = 1; a < p; a++) {
        uint64_t sq = mulmod(a, a, p);
        /* Check if this is the first time we see this residue */
        bool found = false;
        for (uint64_t b = 1; b < a; b++) {
            if (mulmod(b, b, p) == sq) {
                found = true;
                break;
            }
        }
        if (!found) count++;
    }
    return count;
}
