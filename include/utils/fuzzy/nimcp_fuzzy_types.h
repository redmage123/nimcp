//=============================================================================
// nimcp_fuzzy_types.h - Core Fuzzy Logic Type Definitions
//=============================================================================
/**
 * @file nimcp_fuzzy_types.h
 * @brief Core fuzzy set types, membership functions, linguistic variables,
 *        and hedges for NIMCP fuzzy logic system
 *
 * WHAT: Fundamental fuzzy logic types for graded membership reasoning
 * WHY:  Replace hard thresholds with continuous membership degrees across
 *       risk assessment, market classification, ethical evaluation, and
 *       neural integration throughout NIMCP
 * HOW:  Defines 14 membership function types, 8 linguistic hedges,
 *       fuzzy sets, linguistic variables, fuzzification, and discrete
 *       set operations for defuzzification pipelines
 *
 * BIOLOGICAL BASIS:
 * - Neural firing rates encode graded activation (not binary on/off)
 * - Sensory perception operates on continuous scales with overlapping
 *   receptive fields — fuzzy sets model this overlap naturally
 * - Decision-making under uncertainty benefits from graded evidence
 *   accumulation rather than crisp threshold crossings
 *
 * MATHEMATICAL FOUNDATIONS:
 * - Zadeh fuzzy sets (1965): μ_A(x) ∈ [0,1]
 * - Linguistic hedges: concentration (very), dilation (somewhat)
 * - Alpha-cuts: {x | μ_A(x) >= α}
 * - Shannon entropy over membership distributions
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#ifndef NIMCP_FUZZY_TYPES_H
#define NIMCP_FUZZY_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Bio-async module ID for fuzzy types */
#define BIO_MODULE_FUZZY_TYPES          0x0280

/** Maximum number of fuzzy terms per linguistic variable */
#define FUZZY_MAX_TERMS                 32

/** Maximum parameters for a membership function */
#define FUZZY_MAX_PARAMS                8

/** Maximum name length for fuzzy sets and variables */
#define FUZZY_MAX_NAME_LEN              64

/** Maximum fuzzy sets in a registry */
#define FUZZY_MAX_SETS                  128

/** Maximum linguistic variables in a registry */
#define FUZZY_MAX_VARIABLES             64

/** Default discretization resolution for defuzzification */
#define FUZZY_RESOLUTION                256

/** Numerical precision for fuzzy comparisons */
#define FUZZY_PRECISION                 1e-8f

//=============================================================================
// Error Codes
//=============================================================================

/** Fuzzy types error code base */
#define FUZZY_TYPES_ERROR_BASE          28000

#define FUZZY_ERR_OK                    0
#define FUZZY_ERR_NULL                  (FUZZY_TYPES_ERROR_BASE + 1)
#define FUZZY_ERR_INVALID_MF_TYPE       (FUZZY_TYPES_ERROR_BASE + 2)
#define FUZZY_ERR_INVALID_PARAMS        (FUZZY_TYPES_ERROR_BASE + 3)
#define FUZZY_ERR_PARAM_COUNT           (FUZZY_TYPES_ERROR_BASE + 4)
#define FUZZY_ERR_INVALID_HEDGE         (FUZZY_TYPES_ERROR_BASE + 5)
#define FUZZY_ERR_MAX_TERMS             (FUZZY_TYPES_ERROR_BASE + 6)
#define FUZZY_ERR_UNIVERSE_RANGE        (FUZZY_TYPES_ERROR_BASE + 7)
#define FUZZY_ERR_ALLOC                 (FUZZY_TYPES_ERROR_BASE + 8)
#define FUZZY_ERR_RESOLUTION            (FUZZY_TYPES_ERROR_BASE + 9)
#define FUZZY_ERR_DIMENSION_MISMATCH    (FUZZY_TYPES_ERROR_BASE + 10)
#define FUZZY_ERR_EMPTY_SET             (FUZZY_TYPES_ERROR_BASE + 11)
#define FUZZY_ERR_INVALID_NAME          (FUZZY_TYPES_ERROR_BASE + 12)
#define FUZZY_ERR_CUSTOM_FN_NULL        (FUZZY_TYPES_ERROR_BASE + 13)

//=============================================================================
// Membership Function Types
//=============================================================================

/**
 * @brief Types of membership functions supported
 *
 * Each type maps a crisp input x to a membership degree μ(x) ∈ [0,1].
 * Parameters are stored in the params[] array of fuzzy_mf_t.
 */
typedef enum {
    FUZZY_MF_TRIANGULAR,            /**< Params: [a, b, c] — left foot, peak, right foot */
    FUZZY_MF_TRAPEZOIDAL,           /**< Params: [a, b, c, d] — left foot, left shoulder, right shoulder, right foot */
    FUZZY_MF_GAUSSIAN,              /**< Params: [mean, sigma] */
    FUZZY_MF_GAUSSIAN_DOUBLE,       /**< Params: [mean1, sigma1, mean2, sigma2] — asymmetric Gaussian */
    FUZZY_MF_BELL,                  /**< Params: [a, b, c] — generalized bell (width, slope, center) */
    FUZZY_MF_SIGMOID,               /**< Params: [a, c] — slope, center */
    FUZZY_MF_SIGMOID_DIFF,          /**< Params: [a1, c1, a2, c2] — difference of two sigmoids */
    FUZZY_MF_SIGMOID_PROD,          /**< Params: [a1, c1, a2, c2] — product of two sigmoids */
    FUZZY_MF_PI_SHAPED,             /**< Params: [a, b, c, d] — pi-shaped (combination of S and Z) */
    FUZZY_MF_S_SHAPED,              /**< Params: [a, b] — S-curve (foot, shoulder) */
    FUZZY_MF_Z_SHAPED,              /**< Params: [a, b] — Z-curve (shoulder, foot) */
    FUZZY_MF_SINGLETON,             /**< Params: [x0] — crisp value */
    FUZZY_MF_PIECEWISE_LINEAR,      /**< Params: [x0,y0, x1,y1, ...] pairs */
    FUZZY_MF_CUSTOM,                /**< User-supplied callback function */
    FUZZY_MF_TYPE_COUNT
} fuzzy_mf_type_t;

//=============================================================================
// Linguistic Hedges
//=============================================================================

/**
 * @brief Linguistic modifiers that transform membership functions
 *
 * Hedges modify the shape of a membership function to express
 * linguistic qualifiers like "very", "somewhat", "extremely".
 */
typedef enum {
    FUZZY_HEDGE_NONE,               /**< No modification: μ(x) */
    FUZZY_HEDGE_VERY,               /**< Concentration: μ²(x) */
    FUZZY_HEDGE_SOMEWHAT,           /**< Dilation: √μ(x) */
    FUZZY_HEDGE_EXTREMELY,          /**< Strong concentration: μ³(x) */
    FUZZY_HEDGE_SLIGHTLY,           /**< Intensification around 0.5 */
    FUZZY_HEDGE_NOT,                /**< Complement: 1 - μ(x) */
    FUZZY_HEDGE_MORE_OR_LESS,       /**< Mild dilation: μ^0.75(x) */
    FUZZY_HEDGE_INDEED,             /**< Intensification: shifts toward 0 or 1 */
    FUZZY_HEDGE_TYPE_COUNT
} fuzzy_hedge_t;

//=============================================================================
// Core Data Structures
//=============================================================================

/**
 * @brief Custom membership function callback
 *
 * @param x         Input crisp value
 * @param params    Parameter array
 * @param num_params Number of parameters
 * @param user_data User-supplied context
 * @return Membership degree in [0,1]
 */
typedef float (*fuzzy_mf_callback_t)(float x, const float* params,
                                      uint32_t num_params, void* user_data);

/**
 * @brief Membership function definition
 *
 * Encapsulates the type, parameters, and optional custom callback
 * for evaluating membership degree at any crisp input value.
 */
typedef struct {
    fuzzy_mf_type_t type;                   /**< Membership function type */
    float params[FUZZY_MAX_PARAMS];         /**< Parameters (interpretation depends on type) */
    uint32_t num_params;                    /**< Number of active parameters */
    fuzzy_mf_callback_t custom_fn;          /**< Only for FUZZY_MF_CUSTOM */
    void* custom_data;                      /**< User data for custom fn */
} fuzzy_mf_t;

/**
 * @brief Fuzzy set: a named membership function over a universe of discourse
 */
typedef struct {
    char name[FUZZY_MAX_NAME_LEN];          /**< Human-readable term name */
    fuzzy_mf_t mf;                          /**< Membership function */
    fuzzy_hedge_t hedge;                    /**< Applied linguistic hedge */
    float alpha_cut;                        /**< Minimum activation threshold (default 0.0) */
} fuzzy_set_t;

/**
 * @brief Linguistic variable: a collection of fuzzy sets over the same domain
 *
 * Example: "temperature" with terms {cold, warm, hot} over [0, 100]°C
 */
typedef struct {
    char name[FUZZY_MAX_NAME_LEN];          /**< Variable name */
    float universe_min;                     /**< Minimum of universe of discourse */
    float universe_max;                     /**< Maximum of universe of discourse */
    fuzzy_set_t terms[FUZZY_MAX_TERMS];     /**< Fuzzy set terms */
    uint32_t num_terms;                     /**< Number of active terms */
} fuzzy_variable_t;

/**
 * @brief Fuzzy value: degree of membership for each term of a variable
 *
 * Result of fuzzifying a crisp input against a linguistic variable.
 */
typedef struct {
    float memberships[FUZZY_MAX_TERMS];     /**< Membership degree per term */
    uint32_t num_terms;                     /**< Number of terms */
    uint32_t dominant_term;                 /**< Index of highest membership */
    float dominant_degree;                  /**< Highest membership degree */
    float entropy;                          /**< Shannon entropy of distribution */
} fuzzy_value_t;

/**
 * @brief Discretized membership function for defuzzification
 *
 * Samples a membership function at uniform intervals across
 * the universe of discourse for numerical integration.
 */
typedef struct {
    float* values;                          /**< Membership degrees at each point */
    uint32_t resolution;                    /**< Number of discretization points */
    float x_min;                            /**< Start of universe */
    float x_max;                            /**< End of universe */
} fuzzy_discrete_set_t;

//=============================================================================
// Configuration & Statistics
//=============================================================================

/**
 * @brief Configuration for the fuzzy types engine
 */
typedef struct {
    uint32_t default_resolution;            /**< Default discretization resolution (256) */
    float alpha_cut_default;                /**< Default alpha-cut threshold (0.0) */
    bool enable_caching;                    /**< Cache membership evaluations */
    uint32_t cache_size;                    /**< Number of cached evaluations */
    float inflammation_sensitivity;         /**< Immune inflammation modulation factor */
    float fatigue_sensitivity;              /**< Fatigue modulation factor */
} fuzzy_types_config_t;

/**
 * @brief Runtime statistics for the fuzzy types engine
 */
typedef struct {
    uint64_t mf_evaluations;                /**< Total membership function evaluations */
    uint64_t fuzzifications;                /**< Total fuzzification operations */
    uint64_t hedge_applications;            /**< Total hedge applications */
    uint64_t sets_created;                  /**< Total sets created */
    uint64_t variables_created;             /**< Total variables created */
    float avg_evaluation_time_us;           /**< Average MF evaluation time */
} fuzzy_types_stats_t;

//=============================================================================
// Opaque Handle
//=============================================================================

/** Opaque fuzzy types engine handle */
typedef struct fuzzy_types_engine fuzzy_types_engine_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create a fuzzy types engine with default configuration
 * @return Engine handle, or NULL on failure
 */
fuzzy_types_engine_t* fuzzy_types_create(void);

/**
 * @brief Create a fuzzy types engine with custom configuration
 * @param config Configuration (NULL uses defaults)
 * @return Engine handle, or NULL on failure
 */
fuzzy_types_engine_t* fuzzy_types_create_custom(const fuzzy_types_config_t* config);

/**
 * @brief Destroy a fuzzy types engine (NULL-safe)
 */
void fuzzy_types_destroy(fuzzy_types_engine_t* engine);

/**
 * @brief Get default configuration values
 */
fuzzy_types_config_t fuzzy_types_default_config(void);

//=============================================================================
// Membership Function Evaluation
//=============================================================================

/**
 * @brief Evaluate membership degree at a crisp input
 * @param mf  Membership function definition
 * @param x   Crisp input value
 * @return Membership degree in [0,1], or 0.0 on error
 */
float fuzzy_mf_evaluate(const fuzzy_mf_t* mf, float x);

/**
 * @brief Evaluate membership degree with a linguistic hedge applied
 * @param mf    Membership function definition
 * @param x     Crisp input value
 * @param hedge Hedge to apply
 * @return Modified membership degree in [0,1]
 */
float fuzzy_mf_evaluate_hedged(const fuzzy_mf_t* mf, float x, fuzzy_hedge_t hedge);

/**
 * @brief Discretize a membership function over a range
 * @param mf         Membership function
 * @param x_min      Start of range
 * @param x_max      End of range
 * @param resolution Number of sample points
 * @param out        Output discrete set (must be pre-allocated or will be allocated)
 * @return 0 on success, error code on failure
 */
int fuzzy_mf_discretize(const fuzzy_mf_t* mf, float x_min, float x_max,
                         uint32_t resolution, fuzzy_discrete_set_t* out);

//=============================================================================
// Membership Function Construction Helpers
//=============================================================================

/** Create triangular MF: peak at b, feet at a and c */
fuzzy_mf_t fuzzy_mf_triangular(float a, float b, float c);

/** Create trapezoidal MF: shoulders at b,c, feet at a,d */
fuzzy_mf_t fuzzy_mf_trapezoidal(float a, float b, float c, float d);

/** Create Gaussian MF: centered at mean with given sigma */
fuzzy_mf_t fuzzy_mf_gaussian(float mean, float sigma);

/** Create generalized bell MF: width a, slope b, center c */
fuzzy_mf_t fuzzy_mf_bell(float width, float slope, float center);

/** Create sigmoid MF: slope a, center c */
fuzzy_mf_t fuzzy_mf_sigmoid(float slope, float center);

/** Create S-shaped MF: foot a, shoulder b */
fuzzy_mf_t fuzzy_mf_s_shaped(float foot, float shoulder);

/** Create Z-shaped MF: shoulder a, foot b */
fuzzy_mf_t fuzzy_mf_z_shaped(float shoulder, float foot);

/** Create singleton MF: crisp value at x0 */
fuzzy_mf_t fuzzy_mf_singleton(float x0);

/** Create custom MF with user callback */
fuzzy_mf_t fuzzy_mf_custom(fuzzy_mf_callback_t fn, const float* params,
                             uint32_t num_params, void* user_data);

//=============================================================================
// Fuzzy Set Operations
//=============================================================================

/**
 * @brief Create a named fuzzy set from a membership function
 * @param set   Output set struct
 * @param name  Human-readable name (e.g., "cold", "warm", "hot")
 * @param mf    Membership function definition
 * @param hedge Linguistic hedge to apply
 * @return 0 on success, error code on failure
 */
int fuzzy_set_create(fuzzy_set_t* set, const char* name,
                      const fuzzy_mf_t* mf, fuzzy_hedge_t hedge);

/**
 * @brief Evaluate a fuzzy set at a crisp input (applies hedge and alpha-cut)
 * @param set Fuzzy set
 * @param x   Crisp input
 * @return Membership degree in [0,1]
 */
float fuzzy_set_evaluate(const fuzzy_set_t* set, float x);

//=============================================================================
// Linguistic Variable Operations
//=============================================================================

/**
 * @brief Create a linguistic variable over a universe of discourse
 * @param var          Output variable struct
 * @param name         Variable name
 * @param universe_min Minimum value of universe
 * @param universe_max Maximum value of universe
 * @return 0 on success, error code on failure
 */
int fuzzy_variable_create(fuzzy_variable_t* var, const char* name,
                           float universe_min, float universe_max);

/**
 * @brief Add a fuzzy set term to a linguistic variable
 * @param var  Linguistic variable
 * @param term Fuzzy set to add as a term
 * @return 0 on success, error code on failure
 */
int fuzzy_variable_add_term(fuzzy_variable_t* var, const fuzzy_set_t* term);

/**
 * @brief Fuzzify a crisp input against all terms of a variable
 * @param var        Linguistic variable
 * @param crisp_input Crisp input value
 * @param out_value  Output fuzzy value with membership per term
 * @return 0 on success, error code on failure
 */
int fuzzy_variable_fuzzify(const fuzzy_variable_t* var, float crisp_input,
                            fuzzy_value_t* out_value);

/**
 * @brief Compute centroid of a fuzzy value relative to its variable
 * @param var   Linguistic variable (provides universe bounds and term MFs)
 * @param value Fuzzy value (membership degrees per term)
 * @return Centroid crisp value
 */
float fuzzy_variable_centroid(const fuzzy_variable_t* var, const fuzzy_value_t* value);

//=============================================================================
// Hedge Application
//=============================================================================

/**
 * @brief Apply a linguistic hedge to a membership degree
 * @param membership Input membership in [0,1]
 * @param hedge      Hedge to apply
 * @return Modified membership in [0,1]
 */
float fuzzy_apply_hedge(float membership, fuzzy_hedge_t hedge);

//=============================================================================
// Discrete Set Operations
//=============================================================================

/**
 * @brief Create (allocate) a discrete fuzzy set
 * @param set        Output set
 * @param resolution Number of points
 * @param x_min      Start of universe
 * @param x_max      End of universe
 * @return 0 on success, error code on failure
 */
int fuzzy_discrete_set_create(fuzzy_discrete_set_t* set, uint32_t resolution,
                               float x_min, float x_max);

/**
 * @brief Free memory for a discrete set (NULL-safe)
 */
void fuzzy_discrete_set_free(fuzzy_discrete_set_t* set);

/**
 * @brief Union of two discrete sets (pointwise max)
 */
int fuzzy_discrete_set_union(const fuzzy_discrete_set_t* a, const fuzzy_discrete_set_t* b,
                              fuzzy_discrete_set_t* out);

/**
 * @brief Intersection of two discrete sets (pointwise min)
 */
int fuzzy_discrete_set_intersection(const fuzzy_discrete_set_t* a, const fuzzy_discrete_set_t* b,
                                     fuzzy_discrete_set_t* out);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Compute Shannon entropy of a membership distribution
 * @param memberships Array of membership degrees
 * @param count       Number of elements
 * @return Entropy value >= 0 (higher = more uncertain)
 */
float fuzzy_entropy(const float* memberships, uint32_t count);

/**
 * @brief Compute cardinality (sum of memberships) of a fuzzy set
 * @param memberships Array of membership degrees
 * @param count       Number of elements
 * @return Sum of all membership degrees
 */
float fuzzy_cardinality(const float* memberships, uint32_t count);

//=============================================================================
// Modulation (Immune/Fatigue Integration)
//=============================================================================

/**
 * @brief Set inflammation level for modulating fuzzy precision
 * @param engine Fuzzy types engine
 * @param level  Inflammation level [0,1] — higher reduces precision
 * @return 0 on success
 */
int fuzzy_types_set_inflammation(fuzzy_types_engine_t* engine, float level);

/**
 * @brief Set fatigue level for modulating fuzzy processing
 * @param engine Fuzzy types engine
 * @param level  Fatigue level [0,1] — higher increases alpha-cut thresholds
 * @return 0 on success
 */
int fuzzy_types_set_fatigue(fuzzy_types_engine_t* engine, float level);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get runtime statistics
 */
int fuzzy_types_get_stats(const fuzzy_types_engine_t* engine, fuzzy_types_stats_t* stats);

/**
 * @brief Reset all statistics counters
 */
void fuzzy_types_reset_stats(fuzzy_types_engine_t* engine);

/**
 * @brief Get the last error message (thread-local)
 */
const char* fuzzy_types_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FUZZY_TYPES_H */
