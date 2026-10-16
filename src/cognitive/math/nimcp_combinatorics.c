/**
 * @file nimcp_combinatorics.c
 * @brief Combinatorial mathematics engine implementation
 */

#include "cognitive/math/nimcp_combinatorics.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "COMBINATORICS"

/* ---------- internal helpers ---------- */

static void init_factorial_cache(combinatorics_t* ctx) {
    if (ctx->cache_initialized) return;
    ctx->factorial_cache[0] = 1;
    ctx->factorial_cache[1] = 1;
    for (int i = 2; i < COMB_FACTORIAL_CACHE_SIZE; i++) {
        ctx->factorial_cache[i] = ctx->factorial_cache[i - 1] * (uint64_t)i;
    }
    ctx->cache_initialized = true;
    LOG_INFO(LOG_TAG, "Factorial cache initialized (0! through 20!)");
}

static int popcount32(uint32_t x) {
    x = x - ((x >> 1) & 0x55555555u);
    x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
    return (int)(((x + (x >> 4)) & 0x0F0F0F0Fu) * 0x01010101u >> 24);
}

/* ---------- lifecycle ---------- */

combinatorics_t* combinatorics_create(void) {
    combinatorics_t* ctx = (combinatorics_t*)nimcp_calloc(1, sizeof(combinatorics_t));
    if (!ctx) {
        LOG_ERROR(LOG_TAG, "Failed to allocate combinatorics context");
        return NULL;
    }
    init_factorial_cache(ctx);
    return ctx;
}

void combinatorics_destroy(combinatorics_t* ctx) {
    if (!ctx) return;
    nimcp_free(ctx);
}

/* ---------- factorial ---------- */

uint64_t comb_factorial(combinatorics_t* ctx, int n) {
    if (n < 0) return 0;
    if (!ctx->cache_initialized) init_factorial_cache(ctx);
    if (n < COMB_FACTORIAL_CACHE_SIZE) {
        return ctx->factorial_cache[n];
    }
    /* Beyond cache: compute iteratively (will overflow for n > ~20) */
    uint64_t result = ctx->factorial_cache[COMB_FACTORIAL_CACHE_SIZE - 1];
    for (int i = COMB_FACTORIAL_CACHE_SIZE; i <= n; i++) {
        result *= (uint64_t)i;
    }
    return result;
}

/* ---------- binomial coefficient ---------- */

uint64_t comb_binomial(combinatorics_t* ctx, int n, int k) {
    if (k < 0 || k > n) return 0;
    if (k == 0 || k == n) return 1;

    /* Use symmetry: C(n,k) = C(n, n-k) */
    if (k > n - k) k = n - k;

    /* Overflow-safe multiplicative formula:
     * C(n,k) = n/1 * (n-1)/2 * ... * (n-k+1)/k
     * Each intermediate result is guaranteed to be an integer. */
    uint64_t result = 1;
    for (int i = 0; i < k; i++) {
        result *= (uint64_t)(n - i);
        result /= (uint64_t)(i + 1);
    }
    return result;
}

/* ---------- permutation ---------- */

uint64_t comb_permutation(combinatorics_t* ctx, int n, int k) {
    if (k < 0 || k > n) return 0;
    uint64_t result = 1;
    for (int i = 0; i < k; i++) {
        result *= (uint64_t)(n - i);
    }
    return result;
}

/* ---------- Catalan numbers ---------- */

uint64_t comb_catalan(combinatorics_t* ctx, int n) {
    if (n < 0) return 0;
    /* C_n = C(2n, n) / (n + 1) */
    return comb_binomial(ctx, 2 * n, n) / (uint64_t)(n + 1);
}

/* ---------- Stirling number of the first kind (unsigned) ---------- */

int64_t comb_stirling_first(int n, int k) {
    if (n < 0 || k < 0 || k > n) return 0;
    if (n == 0 && k == 0) return 1;
    if (n == 0 || k == 0) return 0;

    /* Recurrence: |s(n,k)| = (n-1)*|s(n-1,k)| + |s(n-1,k-1)| */
    /* Use two rows of DP to save memory */
    int64_t* prev = (int64_t*)nimcp_calloc((size_t)(k + 1), sizeof(int64_t));
    int64_t* curr = (int64_t*)nimcp_calloc((size_t)(k + 1), sizeof(int64_t));
    if (!prev || !curr) {
        nimcp_free(prev);
        nimcp_free(curr);
        return 0;
    }

    prev[0] = 1; /* |s(0,0)| = 1 */

    for (int i = 1; i <= n; i++) {
        memset(curr, 0, (size_t)(k + 1) * sizeof(int64_t));
        for (int j = 1; j <= k && j <= i; j++) {
            curr[j] = (int64_t)(i - 1) * prev[j] + prev[j - 1];
        }
        int64_t* tmp = prev;
        prev = curr;
        curr = tmp;
    }

    int64_t result = prev[k];
    nimcp_free(prev);
    nimcp_free(curr);
    return result;
}

/* ---------- Stirling number of the second kind ---------- */

int64_t comb_stirling_second(int n, int k) {
    if (n < 0 || k < 0 || k > n) return 0;
    if (n == 0 && k == 0) return 1;
    if (n == 0 || k == 0) return 0;

    /* Recurrence: S(n,k) = k*S(n-1,k) + S(n-1,k-1) */
    int64_t* prev = (int64_t*)nimcp_calloc((size_t)(k + 1), sizeof(int64_t));
    int64_t* curr = (int64_t*)nimcp_calloc((size_t)(k + 1), sizeof(int64_t));
    if (!prev || !curr) {
        nimcp_free(prev);
        nimcp_free(curr);
        return 0;
    }

    prev[0] = 1;

    for (int i = 1; i <= n; i++) {
        memset(curr, 0, (size_t)(k + 1) * sizeof(int64_t));
        for (int j = 1; j <= k && j <= i; j++) {
            curr[j] = (int64_t)j * prev[j] + prev[j - 1];
        }
        int64_t* tmp = prev;
        prev = curr;
        curr = tmp;
    }

    int64_t result = prev[k];
    nimcp_free(prev);
    nimcp_free(curr);
    return result;
}

/* ---------- Bell numbers ---------- */

uint64_t comb_bell(int n) {
    if (n < 0) return 0;
    if (n == 0) return 1;
    if (n > COMB_MAX_BELL_N) {
        LOG_WARN(LOG_TAG, "Bell number B(%d) exceeds safe range", n);
        return 0;
    }

    /* Bell triangle method */
    uint64_t* row = (uint64_t*)nimcp_calloc((size_t)(n + 1), sizeof(uint64_t));
    if (!row) return 0;

    row[0] = 1;
    for (int i = 1; i <= n; i++) {
        /* Shift: new row starts with last element of previous row */
        uint64_t prev = row[i - 1];
        row[0] = prev;
        for (int j = 1; j <= i; j++) {
            uint64_t tmp = row[j];
            row[j] = row[j - 1] + prev;
            prev = tmp;
        }
    }

    uint64_t result = row[0];
    nimcp_free(row);
    return result;
}

/* ---------- partition function ---------- */

uint64_t comb_partition(int n) {
    if (n < 0) return 0;
    if (n == 0) return 1;
    if (n > COMB_MAX_PARTITION_N) {
        LOG_WARN(LOG_TAG, "Partition p(%d) exceeds safe range", n);
        return 0;
    }

    /* Euler's pentagonal number theorem recurrence:
     * p(n) = sum_{k!=0} (-1)^{k+1} * p(n - k(3k-1)/2)
     * where k = 1, -1, 2, -2, 3, -3, ... */
    uint64_t* p = (uint64_t*)nimcp_calloc((size_t)(n + 1), sizeof(uint64_t));
    if (!p) return 0;

    p[0] = 1;
    for (int i = 1; i <= n; i++) {
        int64_t sum = 0;
        for (int k = 1; ; k++) {
            /* Generalized pentagonal numbers: k(3k-1)/2 and k(3k+1)/2 */
            int pent1 = k * (3 * k - 1) / 2;
            int pent2 = k * (3 * k + 1) / 2;
            if (pent1 > i) break;

            int sign = (k % 2 == 1) ? 1 : -1;
            sum += sign * (int64_t)p[i - pent1];
            if (pent2 <= i) {
                sum += sign * (int64_t)p[i - pent2];
            }
        }
        p[i] = (uint64_t)sum;
    }

    uint64_t result = p[n];
    nimcp_free(p);
    return result;
}

/* ---------- Fibonacci ---------- */

uint64_t comb_fibonacci(int n) {
    if (n < 0) return 0;
    if (n <= 1) return (uint64_t)n;

    uint64_t a = 0, b = 1;
    for (int i = 2; i <= n; i++) {
        uint64_t c = a + b;
        a = b;
        b = c;
    }
    return b;
}

/* ---------- multinomial ---------- */

uint64_t comb_multinomial(combinatorics_t* ctx, int n, const int* k, int m) {
    if (!k || m <= 0) return 0;

    int sum = 0;
    for (int i = 0; i < m; i++) {
        if (k[i] < 0) return 0;
        sum += k[i];
    }
    if (sum != n) return 0;

    /* n! / (k1! * k2! * ... * km!) computed incrementally */
    uint64_t result = comb_factorial(ctx, n);
    for (int i = 0; i < m; i++) {
        result /= comb_factorial(ctx, k[i]);
    }
    return result;
}

/* ---------- derangements ---------- */

uint64_t comb_derangement(combinatorics_t* ctx, int n) {
    if (n < 0) return 0;
    if (n == 0) return 1;
    if (n == 1) return 0;

    /* D(n) = (n-1) * (D(n-1) + D(n-2)) */
    uint64_t d_prev2 = 1; /* D(0) */
    uint64_t d_prev1 = 0; /* D(1) */
    uint64_t d = 0;

    for (int i = 2; i <= n; i++) {
        d = (uint64_t)(i - 1) * (d_prev1 + d_prev2);
        d_prev2 = d_prev1;
        d_prev1 = d;
    }
    return d;
}

/* ---------- inclusion-exclusion ---------- */

int64_t comb_inclusion_exclusion(const int64_t* sizes, int m) {
    if (!sizes || m <= 0 || m > 20) return 0;

    uint32_t total_masks = (1u << (uint32_t)m);
    int64_t result = 0;

    for (uint32_t mask = 1; mask < total_masks; mask++) {
        int bits = popcount32(mask);
        int sign = (bits % 2 == 1) ? 1 : -1;
        result += sign * sizes[mask - 1];
    }
    return result;
}

/* ---------- generating functions ---------- */

ogf_t comb_ogf_create(const double* coeffs, int n_terms) {
    ogf_t gf;
    memset(&gf, 0, sizeof(gf));
    if (n_terms > COMB_MAX_GF_TERMS) n_terms = COMB_MAX_GF_TERMS;
    gf.n_terms = n_terms;
    if (coeffs) {
        memcpy(gf.coeffs, coeffs, (size_t)n_terms * sizeof(double));
    }
    return gf;
}

double comb_ogf_evaluate(const ogf_t* gf, double x) {
    if (!gf || gf->n_terms == 0) return 0.0;

    /* Horner's method for polynomial evaluation */
    double result = gf->coeffs[gf->n_terms - 1];
    for (int i = gf->n_terms - 2; i >= 0; i--) {
        result = result * x + gf->coeffs[i];
    }
    return result;
}

ogf_t comb_ogf_multiply(const ogf_t* a, const ogf_t* b) {
    ogf_t result;
    memset(&result, 0, sizeof(result));
    if (!a || !b) return result;

    int n = a->n_terms + b->n_terms - 1;
    if (n > COMB_MAX_GF_TERMS) n = COMB_MAX_GF_TERMS;
    result.n_terms = n;

    /* Convolution */
    for (int i = 0; i < a->n_terms; i++) {
        for (int j = 0; j < b->n_terms; j++) {
            if (i + j < COMB_MAX_GF_TERMS) {
                result.coeffs[i + j] += a->coeffs[i] * b->coeffs[j];
            }
        }
    }

    LOG_DEBUG(LOG_TAG, "OGF multiply: %d x %d terms -> %d terms",
              a->n_terms, b->n_terms, result.n_terms);
    return result;
}
