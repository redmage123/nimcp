//=============================================================================
// nimcp_fuzzy_operators.h - Fuzzy Logic Operators
//=============================================================================
/**
 * @file nimcp_fuzzy_operators.h
 * @brief T-norms, t-conorms, complements, implications, aggregation, and
 *        fuzzy relation operations for NIMCP fuzzy logic system
 *
 * WHAT: 28 operators (7 t-norms, 7 t-conorms, 3 complements, 6 implications,
 *       5 aggregations) plus weighted variants and relation composition
 * WHY:  Provide the full algebraic toolkit for combining fuzzy membership
 *       degrees in inference rules, set operations, and decision pipelines
 * HOW:  Each operator family implements multiple variants from fuzzy logic
 *       literature (Zadeh, Lukasiewicz, Einstein, Hamacher, etc.)
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#ifndef NIMCP_FUZZY_OPERATORS_H
#define NIMCP_FUZZY_OPERATORS_H

#include "utils/fuzzy/nimcp_fuzzy_types.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define BIO_MODULE_FUZZY_OPERATORS      0x0281

//=============================================================================
// T-Norm Types (Fuzzy AND / Conjunction)
//=============================================================================

typedef enum {
    FUZZY_TNORM_MIN,                /**< Godel: min(a, b) -- standard */
    FUZZY_TNORM_ALGEBRAIC_PRODUCT,  /**< Probabilistic: a * b */
    FUZZY_TNORM_LUKASIEWICZ,        /**< Lukasiewicz: max(0, a + b - 1) */
    FUZZY_TNORM_DRASTIC,            /**< Drastic: min(a,b) if max=1, else 0 */
    FUZZY_TNORM_EINSTEIN,           /**< Einstein: (a*b)/(2-(a+b-a*b)) */
    FUZZY_TNORM_HAMACHER,           /**< Hamacher: (a*b)/(a+b-a*b) */
    FUZZY_TNORM_NILPOTENT_MIN,      /**< Nilpotent: min(a,b) if a+b>1, else 0 */
    FUZZY_TNORM_TYPE_COUNT
} fuzzy_tnorm_type_t;

//=============================================================================
// T-Conorm Types (Fuzzy OR / Disjunction)
//=============================================================================

typedef enum {
    FUZZY_TCONORM_MAX,              /**< Godel: max(a, b) -- standard */
    FUZZY_TCONORM_ALGEBRAIC_SUM,    /**< Probabilistic: a + b - a*b */
    FUZZY_TCONORM_LUKASIEWICZ,      /**< Lukasiewicz: min(1, a + b) */
    FUZZY_TCONORM_DRASTIC,          /**< Drastic: max(a,b) if min=0, else 1 */
    FUZZY_TCONORM_EINSTEIN,         /**< Einstein: (a+b)/(1+a*b) */
    FUZZY_TCONORM_HAMACHER,         /**< Hamacher: (a+b-2*a*b)/(1-a*b) */
    FUZZY_TCONORM_NILPOTENT_MAX,    /**< Nilpotent: max(a,b) if a+b<1, else 1 */
    FUZZY_TCONORM_TYPE_COUNT
} fuzzy_tconorm_type_t;

//=============================================================================
// Complement Types (Fuzzy NOT)
//=============================================================================

typedef enum {
    FUZZY_COMPLEMENT_STANDARD,      /**< 1 - a */
    FUZZY_COMPLEMENT_SUGENO,        /**< (1 - a) / (1 + lambda*a), lambda > -1 */
    FUZZY_COMPLEMENT_YAGER,         /**< (1 - a^w)^(1/w), w > 0 */
    FUZZY_COMPLEMENT_TYPE_COUNT
} fuzzy_complement_type_t;

//=============================================================================
// Implication Types
//=============================================================================

typedef enum {
    FUZZY_IMPL_MAMDANI,             /**< min(a, b) -- Mamdani minimum */
    FUZZY_IMPL_LARSEN,              /**< a * b -- Larsen product */
    FUZZY_IMPL_LUKASIEWICZ,         /**< min(1, 1-a+b) */
    FUZZY_IMPL_GODEL,               /**< b if a <= b, else 1 */
    FUZZY_IMPL_KLEENE_DIENES,       /**< max(1-a, b) */
    FUZZY_IMPL_ZADEH,               /**< max(min(a,b), 1-a) */
    FUZZY_IMPL_TYPE_COUNT
} fuzzy_implication_type_t;

//=============================================================================
// Aggregation Types
//=============================================================================

typedef enum {
    FUZZY_AGG_MAX,                  /**< Maximum */
    FUZZY_AGG_ALGEBRAIC_SUM,        /**< Probabilistic OR */
    FUZZY_AGG_BOUNDED_SUM,          /**< min(1, a+b) */
    FUZZY_AGG_NORMALIZED_SUM,       /**< (a+b) / max_sum, normalized */
    FUZZY_AGG_EINSTEIN_SUM,         /**< (a+b)/(1+a*b) */
    FUZZY_AGG_TYPE_COUNT
} fuzzy_aggregation_type_t;

//=============================================================================
// Configuration & Statistics
//=============================================================================

typedef struct {
    fuzzy_tnorm_type_t default_tnorm;
    fuzzy_tconorm_type_t default_tconorm;
    fuzzy_complement_type_t default_complement;
    fuzzy_implication_type_t default_implication;
    fuzzy_aggregation_type_t default_aggregation;
    float sugeno_lambda;            /**< For Sugeno complement, lambda > -1 */
    float yager_w;                  /**< For Yager complement, w > 0 */
    float hamacher_gamma;           /**< Parameterized Hamacher, gamma >= 0 */
} fuzzy_operator_config_t;

typedef struct {
    uint64_t tnorm_evaluations;
    uint64_t tconorm_evaluations;
    uint64_t complement_evaluations;
    uint64_t implication_evaluations;
    uint64_t aggregation_evaluations;
    uint64_t relation_compositions;
    float avg_evaluation_time_us;
} fuzzy_operator_stats_t;

//=============================================================================
// T-Norm (Fuzzy AND)
//=============================================================================

float fuzzy_tnorm(float a, float b, fuzzy_tnorm_type_t type);
float fuzzy_tnorm_array(const float* values, uint32_t count, fuzzy_tnorm_type_t type);

//=============================================================================
// T-Conorm (Fuzzy OR)
//=============================================================================

float fuzzy_tconorm(float a, float b, fuzzy_tconorm_type_t type);
float fuzzy_tconorm_array(const float* values, uint32_t count, fuzzy_tconorm_type_t type);

//=============================================================================
// Complement (Fuzzy NOT)
//=============================================================================

float fuzzy_complement(float a, fuzzy_complement_type_t type, float param);

//=============================================================================
// Implication
//=============================================================================

float fuzzy_implication(float antecedent, float consequent,
                         fuzzy_implication_type_t type);

//=============================================================================
// Aggregation
//=============================================================================

float fuzzy_aggregate(float a, float b, fuzzy_aggregation_type_t type);
float fuzzy_aggregate_array(const float* values, uint32_t count,
                             fuzzy_aggregation_type_t type);

//=============================================================================
// Weighted Operations
//=============================================================================

float fuzzy_weighted_tnorm(const float* values, const float* weights, uint32_t count,
                            fuzzy_tnorm_type_t type);
float fuzzy_weighted_tconorm(const float* values, const float* weights, uint32_t count,
                              fuzzy_tconorm_type_t type);
float fuzzy_weighted_average(const float* values, const float* weights, uint32_t count);

//=============================================================================
// Fuzzy Relations
//=============================================================================

/**
 * @brief Max-min (or general) composition of two fuzzy relations
 * @param rel_a   First relation matrix (rows_a x cols_a)
 * @param rows_a  Rows of first relation
 * @param cols_a  Columns of first relation (must equal rows_b)
 * @param rel_b   Second relation matrix (rows_b x cols_b)
 * @param rows_b  Rows of second relation
 * @param cols_b  Columns of second relation
 * @param out_rel Output matrix (rows_a x cols_b)
 * @param tnorm   T-norm for composition inner operation
 * @param tconorm T-conorm for composition outer operation
 * @return 0 on success
 */
int fuzzy_relation_compose(const float* rel_a, uint32_t rows_a, uint32_t cols_a,
                            const float* rel_b, uint32_t rows_b, uint32_t cols_b,
                            float* out_rel, fuzzy_tnorm_type_t tnorm,
                            fuzzy_tconorm_type_t tconorm);

//=============================================================================
// Set-Level Operations (over fuzzy_value_t)
//=============================================================================

int fuzzy_value_and(const fuzzy_value_t* a, const fuzzy_value_t* b,
                     fuzzy_tnorm_type_t tnorm, fuzzy_value_t* out);
int fuzzy_value_or(const fuzzy_value_t* a, const fuzzy_value_t* b,
                    fuzzy_tconorm_type_t tconorm, fuzzy_value_t* out);
int fuzzy_value_not(const fuzzy_value_t* a, fuzzy_complement_type_t comp,
                     float param, fuzzy_value_t* out);

//=============================================================================
// Similarity & Distance
//=============================================================================

float fuzzy_set_similarity(const float* set_a, const float* set_b, uint32_t count);
float fuzzy_set_distance(const float* set_a, const float* set_b, uint32_t count);
float fuzzy_set_inclusion(const float* subset, const float* superset, uint32_t count);

//=============================================================================
// Statistics
//=============================================================================

int fuzzy_operator_get_stats(fuzzy_operator_stats_t* stats);
void fuzzy_operator_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FUZZY_OPERATORS_H */
