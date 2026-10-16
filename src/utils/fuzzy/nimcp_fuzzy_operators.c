//=============================================================================
// nimcp_fuzzy_operators.c - Fuzzy Logic Operator Implementations
//=============================================================================
/**
 * @file nimcp_fuzzy_operators.c
 * @brief Implementation of t-norms, t-conorms, complements, implications,
 *        aggregation, weighted operations, relations, similarity/distance
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include "utils/fuzzy/nimcp_fuzzy_operators.h"
#include "constants/nimcp_buffer_constants.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdarg.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(fuzzy_operators)

//=============================================================================
// Exception Handling
//=============================================================================
#include "utils/exception/nimcp_exception_macros.h"

//=============================================================================
// Thread-Local Error Handling
//=============================================================================

static _Thread_local char fuzzy_op_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fuzzy_op_last_error, sizeof(fuzzy_op_last_error), fmt, args);
    va_end(args);
}

const char* fuzzy_operator_get_last_error(void) {
    return fuzzy_op_last_error;
}

//=============================================================================
// Global Statistics
//=============================================================================

static fuzzy_operator_stats_t g_operator_stats = {0};

static inline float minf(float a, float b) { return (a < b) ? a : b; }
static inline float maxf(float a, float b) { return (a > b) ? a : b; }

//=============================================================================
// T-Norm (Fuzzy AND)
//=============================================================================

float fuzzy_tnorm(float a, float b, fuzzy_tnorm_type_t type) {
    a = nimcp_clampf(a, 0.0f, 1.0f);
    b = nimcp_clampf(b, 0.0f, 1.0f);

    switch (type) {
        case FUZZY_TNORM_MIN:
            return minf(a, b);
        case FUZZY_TNORM_ALGEBRAIC_PRODUCT:
            return a * b;
        case FUZZY_TNORM_LUKASIEWICZ:
            return maxf(0.0f, a + b - 1.0f);
        case FUZZY_TNORM_DRASTIC:
            if (a >= 1.0f - FUZZY_PRECISION) return b;
            if (b >= 1.0f - FUZZY_PRECISION) return a;
            return 0.0f;
        case FUZZY_TNORM_EINSTEIN: {
            float denom = 2.0f - (a + b - a * b);
            if (fabsf(denom) < FUZZY_PRECISION) return 0.0f;
            return (a * b) / denom;
        }
        case FUZZY_TNORM_HAMACHER: {
            float denom = a + b - a * b;
            if (fabsf(denom) < FUZZY_PRECISION) return 0.0f;
            return (a * b) / denom;
        }
        case FUZZY_TNORM_NILPOTENT_MIN:
            return (a + b > 1.0f) ? minf(a, b) : 0.0f;
        default:
            return minf(a, b);
    }
}

float fuzzy_tnorm_array(const float* values, uint32_t count, fuzzy_tnorm_type_t type) {
    if (!values || count == 0) return 0.0f;
    float result = values[0];
    for (uint32_t i = 1; i < count; i++) {
        result = fuzzy_tnorm(result, values[i], type);
    }
    return result;
}

//=============================================================================
// T-Conorm (Fuzzy OR)
//=============================================================================

float fuzzy_tconorm(float a, float b, fuzzy_tconorm_type_t type) {
    a = nimcp_clampf(a, 0.0f, 1.0f);
    b = nimcp_clampf(b, 0.0f, 1.0f);

    switch (type) {
        case FUZZY_TCONORM_MAX:
            return maxf(a, b);
        case FUZZY_TCONORM_ALGEBRAIC_SUM:
            return a + b - a * b;
        case FUZZY_TCONORM_LUKASIEWICZ:
            return minf(1.0f, a + b);
        case FUZZY_TCONORM_DRASTIC:
            if (a <= FUZZY_PRECISION) return b;
            if (b <= FUZZY_PRECISION) return a;
            return 1.0f;
        case FUZZY_TCONORM_EINSTEIN: {
            float denom = 1.0f + a * b;
            if (fabsf(denom) < FUZZY_PRECISION) return 1.0f;
            return (a + b) / denom;
        }
        case FUZZY_TCONORM_HAMACHER: {
            float denom = 1.0f - a * b;
            if (fabsf(denom) < FUZZY_PRECISION) return 1.0f;
            return (a + b - 2.0f * a * b) / denom;
        }
        case FUZZY_TCONORM_NILPOTENT_MAX:
            return (a + b < 1.0f) ? maxf(a, b) : 1.0f;
        default:
            return maxf(a, b);
    }
}

float fuzzy_tconorm_array(const float* values, uint32_t count, fuzzy_tconorm_type_t type) {
    if (!values || count == 0) return 0.0f;
    float result = values[0];
    for (uint32_t i = 1; i < count; i++) {
        result = fuzzy_tconorm(result, values[i], type);
    }
    return result;
}

//=============================================================================
// Complement (Fuzzy NOT)
//=============================================================================

float fuzzy_complement(float a, fuzzy_complement_type_t type, float param) {
    a = nimcp_clampf(a, 0.0f, 1.0f);

    switch (type) {
        case FUZZY_COMPLEMENT_STANDARD:
            return 1.0f - a;
        case FUZZY_COMPLEMENT_SUGENO: {
            /* (1 - a) / (1 + lambda*a), lambda > -1 */
            float lambda = param;
            float denom = 1.0f + lambda * a;
            if (fabsf(denom) < FUZZY_PRECISION) return 0.0f;
            return nimcp_clampf((1.0f - a) / denom, 0.0f, 1.0f);
        }
        case FUZZY_COMPLEMENT_YAGER: {
            /* (1 - a^w)^(1/w), w > 0 */
            float w = param;
            if (w <= FUZZY_PRECISION) return 1.0f - a;
            float aw = powf(a, w);
            return nimcp_clampf(powf(1.0f - aw, 1.0f / w), 0.0f, 1.0f);
        }
        default:
            return 1.0f - a;
    }
}

//=============================================================================
// Implication
//=============================================================================

float fuzzy_implication(float antecedent, float consequent,
                         fuzzy_implication_type_t type) {
    float a = nimcp_clampf(antecedent, 0.0f, 1.0f);
    float b = nimcp_clampf(consequent, 0.0f, 1.0f);

    switch (type) {
        case FUZZY_IMPL_MAMDANI:
            return minf(a, b);
        case FUZZY_IMPL_LARSEN:
            return a * b;
        case FUZZY_IMPL_LUKASIEWICZ:
            return minf(1.0f, 1.0f - a + b);
        case FUZZY_IMPL_GODEL:
            return (a <= b) ? 1.0f : b;
        case FUZZY_IMPL_KLEENE_DIENES:
            return maxf(1.0f - a, b);
        case FUZZY_IMPL_ZADEH:
            return maxf(minf(a, b), 1.0f - a);
        default:
            return minf(a, b);
    }
}

//=============================================================================
// Aggregation
//=============================================================================

float fuzzy_aggregate(float a, float b, fuzzy_aggregation_type_t type) {
    a = nimcp_clampf(a, 0.0f, 1.0f);
    b = nimcp_clampf(b, 0.0f, 1.0f);

    switch (type) {
        case FUZZY_AGG_MAX:
            return maxf(a, b);
        case FUZZY_AGG_ALGEBRAIC_SUM:
            return a + b - a * b;
        case FUZZY_AGG_BOUNDED_SUM:
            return minf(1.0f, a + b);
        case FUZZY_AGG_NORMALIZED_SUM: {
            float sum = a + b;
            return (sum > FUZZY_PRECISION) ? minf(1.0f, sum / 2.0f) : 0.0f;
        }
        case FUZZY_AGG_EINSTEIN_SUM: {
            float denom = 1.0f + a * b;
            if (fabsf(denom) < FUZZY_PRECISION) return 1.0f;
            return (a + b) / denom;
        }
        default:
            return maxf(a, b);
    }
}

float fuzzy_aggregate_array(const float* values, uint32_t count,
                             fuzzy_aggregation_type_t type) {
    if (!values || count == 0) return 0.0f;
    float result = values[0];
    for (uint32_t i = 1; i < count; i++) {
        result = fuzzy_aggregate(result, values[i], type);
    }
    return result;
}

//=============================================================================
// Weighted Operations
//=============================================================================

float fuzzy_weighted_tnorm(const float* values, const float* weights, uint32_t count,
                            fuzzy_tnorm_type_t type) {
    if (!values || !weights || count == 0) return 0.0f;

    /* Normalize weights */
    float wsum = 0.0f;
    for (uint32_t i = 0; i < count; i++) wsum += weights[i];
    if (wsum < FUZZY_PRECISION) return 0.0f;

    /* Weighted t-norm: T(w_i * v_i + (1-w_i)) */
    float result = 1.0f;
    for (uint32_t i = 0; i < count; i++) {
        float w = weights[i] / wsum;
        float adjusted = w * values[i] + (1.0f - w);
        result = fuzzy_tnorm(result, nimcp_clampf(adjusted, 0.0f, 1.0f), type);
    }
    return result;
}

float fuzzy_weighted_tconorm(const float* values, const float* weights, uint32_t count,
                              fuzzy_tconorm_type_t type) {
    if (!values || !weights || count == 0) return 0.0f;

    float wsum = 0.0f;
    for (uint32_t i = 0; i < count; i++) wsum += weights[i];
    if (wsum < FUZZY_PRECISION) return 0.0f;

    /* Weighted t-conorm: S(w_i * v_i) */
    float result = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        float w = weights[i] / wsum;
        float adjusted = w * values[i];
        result = fuzzy_tconorm(result, nimcp_clampf(adjusted, 0.0f, 1.0f), type);
    }
    return result;
}

float fuzzy_weighted_average(const float* values, const float* weights, uint32_t count) {
    if (!values || !weights || count == 0) return 0.0f;

    float num = 0.0f, den = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        num += weights[i] * values[i];
        den += weights[i];
    }
    if (den < FUZZY_PRECISION) return 0.0f;
    return num / den;
}

//=============================================================================
// Fuzzy Relations
//=============================================================================

int fuzzy_relation_compose(const float* rel_a, uint32_t rows_a, uint32_t cols_a,
                            const float* rel_b, uint32_t rows_b, uint32_t cols_b,
                            float* out_rel, fuzzy_tnorm_type_t tnorm,
                            fuzzy_tconorm_type_t tconorm) {
    if (!rel_a || !rel_b || !out_rel) {
        set_error("fuzzy_relation_compose: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_relation_compose: NULL argument");
        return FUZZY_ERR_NULL;
    }
    if (cols_a != rows_b) {
        set_error("fuzzy_relation_compose: dimension mismatch cols_a=%u rows_b=%u", cols_a, rows_b);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "fuzzy_relation_compose: dimension mismatch cols_a=%u rows_b=%u", cols_a, rows_b);
        return FUZZY_ERR_DIMENSION_MISMATCH;
    }

    fuzzy_operators_heartbeat("fuzzy_relation_compose", 0.0f);

    for (uint32_t i = 0; i < rows_a; i++) {
        for (uint32_t j = 0; j < cols_b; j++) {
            /* Composition: S_k T(R1(i,k), R2(k,j)) */
            float agg = 0.0f;
            bool first = true;
            for (uint32_t k = 0; k < cols_a; k++) {
                float t = fuzzy_tnorm(rel_a[i * cols_a + k],
                                       rel_b[k * cols_b + j], tnorm);
                if (first) {
                    agg = t;
                    first = false;
                } else {
                    agg = fuzzy_tconorm(agg, t, tconorm);
                }
            }
            out_rel[i * cols_b + j] = agg;
        }
    }

    fuzzy_operators_heartbeat("fuzzy_relation_compose", 1.0f);
    return FUZZY_ERR_OK;
}

//=============================================================================
// Set-Level Operations (over fuzzy_value_t)
//=============================================================================

int fuzzy_value_and(const fuzzy_value_t* a, const fuzzy_value_t* b,
                     fuzzy_tnorm_type_t tnorm, fuzzy_value_t* out) {
    if (!a || !b || !out) {
        set_error("fuzzy_value_and: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_value_and: NULL argument");
        return FUZZY_ERR_NULL;
    }
    if (a->num_terms != b->num_terms) {
        set_error("fuzzy_value_and: dimension mismatch a=%u b=%u", a->num_terms, b->num_terms);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "fuzzy_value_and: dimension mismatch a=%u b=%u", a->num_terms, b->num_terms);
        return FUZZY_ERR_DIMENSION_MISMATCH;
    }

    memset(out, 0, sizeof(fuzzy_value_t));
    out->num_terms = a->num_terms;
    out->dominant_degree = 0.0f;

    for (uint32_t i = 0; i < a->num_terms; i++) {
        out->memberships[i] = fuzzy_tnorm(a->memberships[i], b->memberships[i], tnorm);
        if (out->memberships[i] > out->dominant_degree) {
            out->dominant_degree = out->memberships[i];
            out->dominant_term = i;
        }
    }
    out->entropy = fuzzy_entropy(out->memberships, out->num_terms);
    return FUZZY_ERR_OK;
}

int fuzzy_value_or(const fuzzy_value_t* a, const fuzzy_value_t* b,
                    fuzzy_tconorm_type_t tconorm, fuzzy_value_t* out) {
    if (!a || !b || !out) {
        set_error("fuzzy_value_or: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_value_or: NULL argument");
        return FUZZY_ERR_NULL;
    }
    if (a->num_terms != b->num_terms) {
        set_error("fuzzy_value_or: dimension mismatch a=%u b=%u", a->num_terms, b->num_terms);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "fuzzy_value_or: dimension mismatch a=%u b=%u", a->num_terms, b->num_terms);
        return FUZZY_ERR_DIMENSION_MISMATCH;
    }

    memset(out, 0, sizeof(fuzzy_value_t));
    out->num_terms = a->num_terms;
    out->dominant_degree = 0.0f;

    for (uint32_t i = 0; i < a->num_terms; i++) {
        out->memberships[i] = fuzzy_tconorm(a->memberships[i], b->memberships[i], tconorm);
        if (out->memberships[i] > out->dominant_degree) {
            out->dominant_degree = out->memberships[i];
            out->dominant_term = i;
        }
    }
    out->entropy = fuzzy_entropy(out->memberships, out->num_terms);
    return FUZZY_ERR_OK;
}

int fuzzy_value_not(const fuzzy_value_t* a, fuzzy_complement_type_t comp,
                     float param, fuzzy_value_t* out) {
    if (!a || !out) {
        set_error("fuzzy_value_not: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_value_not: NULL argument");
        return FUZZY_ERR_NULL;
    }

    memset(out, 0, sizeof(fuzzy_value_t));
    out->num_terms = a->num_terms;
    out->dominant_degree = 0.0f;

    for (uint32_t i = 0; i < a->num_terms; i++) {
        out->memberships[i] = fuzzy_complement(a->memberships[i], comp, param);
        if (out->memberships[i] > out->dominant_degree) {
            out->dominant_degree = out->memberships[i];
            out->dominant_term = i;
        }
    }
    out->entropy = fuzzy_entropy(out->memberships, out->num_terms);
    return FUZZY_ERR_OK;
}

//=============================================================================
// Similarity & Distance
//=============================================================================

float fuzzy_set_similarity(const float* set_a, const float* set_b, uint32_t count) {
    if (!set_a || !set_b || count == 0) return 0.0f;

    /* Jaccard-style: |A ∩ B| / |A ∪ B| */
    float intersection = 0.0f, union_val = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        intersection += minf(set_a[i], set_b[i]);
        union_val += maxf(set_a[i], set_b[i]);
    }
    if (union_val < FUZZY_PRECISION) return 1.0f; /* Both empty */
    return intersection / union_val;
}

float fuzzy_set_distance(const float* set_a, const float* set_b, uint32_t count) {
    if (!set_a || !set_b || count == 0) return 0.0f;

    /* Hamming-style distance: normalized sum of absolute differences */
    float dist = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        float d = set_a[i] - set_b[i];
        dist += (d < 0.0f) ? -d : d;
    }
    return dist / (float)count;
}

float fuzzy_set_inclusion(const float* subset, const float* superset, uint32_t count) {
    if (!subset || !superset || count == 0) return 0.0f;

    /* Degree of inclusion via Lukasiewicz implication: min_i(min(1, 1-A(i)+B(i)))
     * Satisfies reflexivity: A ⊆ A = 1.0 for all fuzzy sets A */
    float result = 1.0f;
    for (uint32_t i = 0; i < count; i++) {
        float imp = minf(1.0f, 1.0f - subset[i] + superset[i]);
        if (imp < result) result = imp;
    }
    return result;
}

//=============================================================================
// Statistics
//=============================================================================

int fuzzy_operator_get_stats(fuzzy_operator_stats_t* stats) {
    if (!stats) {
        set_error("fuzzy_operator_get_stats: NULL stats pointer");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_operator_get_stats: NULL stats pointer");
        return FUZZY_ERR_NULL;
    }
    *stats = g_operator_stats;
    return FUZZY_ERR_OK;
}

void fuzzy_operator_reset_stats(void) {
    memset(&g_operator_stats, 0, sizeof(g_operator_stats));
}
