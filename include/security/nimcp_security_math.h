/**
 * @file nimcp_security_math.h
 * @brief Mathematical Security Enhancements for NIMCP
 *
 * Phase SC-3: Mathematical Security Framework
 *
 * Implements three rigorous mathematical security mechanisms:
 *
 * 1. SHANNON ENTROPY ANOMALY DETECTION
 *    - Information-theoretic analysis of memory/behavior patterns
 *    - Detects tampering via entropy deviation from baseline
 *    - Based on H(X) = -sum(p(x) * log2(p(x)))
 *
 * 2. BAYESIAN TRUST PROPAGATION
 *    - Probabilistic trust model with evidence-based updates
 *    - P(Trust|Evidence) = P(Evidence|Trust) * P(Trust) / P(Evidence)
 *    - Web of trust with transitive confidence
 *
 * 3. DIFFERENTIAL PRIVACY FOR AUDIT LOGS
 *    - (epsilon, delta)-differential privacy guarantees
 *    - Laplacian noise mechanism for numeric queries
 *    - Privacy budget tracking and enforcement
 *
 * @version 1.0.0
 * @author NIMCP Security Team
 */

#ifndef NIMCP_SECURITY_MATH_H
#define NIMCP_SECURITY_MATH_H

#include "utils/validation/nimcp_common.h"
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define NIMCP_ENTROPY_HISTOGRAM_BINS 256
#define NIMCP_ENTROPY_WINDOW_SIZE 4096
#define NIMCP_TRUST_MAX_ENTITIES 1024
#define NIMCP_TRUST_MAX_VOUCHERS 16
#define NIMCP_DP_DEFAULT_EPSILON 1.0
#define NIMCP_DP_DEFAULT_DELTA 1e-6

//=============================================================================
// Part 1: Shannon Entropy Anomaly Detection
//=============================================================================

/**
 * @brief Entropy analysis result
 *
 * MATHEMATICAL BASIS:
 * Shannon Entropy: H(X) = -sum_{i=1}^{n} p(x_i) * log2(p(x_i))
 *
 * Properties:
 * - H(X) in [0, log2(n)] where n = alphabet size
 * - Maximum entropy = perfectly uniform distribution
 * - Zero entropy = single symbol (no information)
 *
 * SECURITY APPLICATION:
 * - Encrypted/compressed data: H ~ 8.0 bits/byte
 * - Plain text: H ~ 4.0-5.0 bits/byte
 * - Executable code: H ~ 5.5-6.5 bits/byte
 * - Tampered data: entropy deviation from baseline
 */
typedef struct {
    double entropy;                  /**< Current Shannon entropy (bits) */
    double baseline_entropy;         /**< Established baseline entropy */
    double deviation;                /**< Deviation from baseline (sigma) */
    double min_entropy;              /**< Minimum entropy (Renyi H_inf) */
    double conditional_entropy;      /**< H(X|Y) if context provided */

    uint64_t byte_histogram[NIMCP_ENTROPY_HISTOGRAM_BINS]; /**< Byte frequency */
    uint64_t total_bytes;           /**< Total bytes analyzed */

    bool is_anomaly;                /**< Anomaly detected */
    double anomaly_score;           /**< Anomaly score [0, 1] */
    char analysis[256];             /**< Human-readable analysis */
} nimcp_entropy_result_t;

/**
 * @brief Entropy analyzer configuration
 */
typedef struct {
    double deviation_threshold;     /**< Sigma threshold for anomaly (default: 3.0) */
    double min_entropy_threshold;   /**< Min entropy for crypto detection */
    uint32_t window_size;           /**< Sliding window size */
    bool track_baseline;            /**< Automatically track baseline */
    uint32_t baseline_samples;      /**< Samples for baseline estimation */
} nimcp_entropy_config_t;

/**
 * @brief Entropy analyzer context
 */
typedef struct nimcp_entropy_analyzer nimcp_entropy_analyzer_t;

/**
 * @brief Create entropy analyzer
 * @return Analyzer context or NULL on failure
 */
nimcp_entropy_analyzer_t* nimcp_entropy_create(void);

/**
 * @brief Initialize with configuration
 * @param analyzer Analyzer context
 * @param config Configuration (NULL for defaults)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_entropy_init(
    nimcp_entropy_analyzer_t* analyzer,
    const nimcp_entropy_config_t* config
);

/**
 * @brief Destroy entropy analyzer
 * @param analyzer Analyzer context
 */
void nimcp_entropy_destroy(nimcp_entropy_analyzer_t* analyzer);

/**
 * @brief Analyze data entropy
 *
 * ALGORITHM:
 * 1. Build byte histogram from data
 * 2. Calculate Shannon entropy: H = -sum(p_i * log2(p_i))
 * 3. Calculate min-entropy: H_inf = -log2(max(p_i))
 * 4. Compare to baseline, compute deviation
 * 5. Flag anomaly if deviation > threshold
 *
 * @param analyzer Analyzer context
 * @param data Data to analyze
 * @param size Data size
 * @param result Output: analysis result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_entropy_analyze(
    nimcp_entropy_analyzer_t* analyzer,
    const void* data,
    size_t size,
    nimcp_entropy_result_t* result
);

/**
 * @brief Set baseline entropy for a memory region
 *
 * @param analyzer Analyzer context
 * @param region_id Region identifier
 * @param data Data to baseline
 * @param size Data size
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_entropy_set_baseline(
    nimcp_entropy_analyzer_t* analyzer,
    uint32_t region_id,
    const void* data,
    size_t size
);

/**
 * @brief Check region against its baseline
 *
 * @param analyzer Analyzer context
 * @param region_id Region identifier
 * @param data Current data
 * @param size Data size
 * @param result Output: analysis result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_entropy_check_baseline(
    nimcp_entropy_analyzer_t* analyzer,
    uint32_t region_id,
    const void* data,
    size_t size,
    nimcp_entropy_result_t* result
);

/**
 * @brief Calculate pure Shannon entropy (stateless)
 *
 * H(X) = -sum_{i=0}^{255} p(x_i) * log2(p(x_i))
 *
 * @param data Data buffer
 * @param size Data size
 * @return Entropy in bits per byte [0, 8]
 */
double nimcp_entropy_calculate(const void* data, size_t size);

/**
 * @brief Calculate joint entropy H(X,Y)
 *
 * H(X,Y) = -sum_{x,y} p(x,y) * log2(p(x,y))
 *
 * @param data1 First data buffer
 * @param data2 Second data buffer
 * @param size Size of both buffers
 * @return Joint entropy
 */
double nimcp_entropy_joint(const void* data1, const void* data2, size_t size);

/**
 * @brief Calculate mutual information I(X;Y)
 *
 * I(X;Y) = H(X) + H(Y) - H(X,Y)
 *
 * Used for detecting information leakage between regions.
 *
 * @param data1 First data buffer
 * @param data2 Second data buffer
 * @param size Size of both buffers
 * @return Mutual information in bits
 */
double nimcp_entropy_mutual_information(
    const void* data1,
    const void* data2,
    size_t size
);

/**
 * @brief Get default entropy configuration
 * @return Default configuration
 */
nimcp_entropy_config_t nimcp_entropy_default_config(void);

//=============================================================================
// Part 2: Bayesian Trust Propagation
//=============================================================================

/**
 * @brief Trust level for an entity
 *
 * MATHEMATICAL MODEL:
 * Trust is modeled as a Beta distribution Beta(alpha, beta):
 * - alpha = successful interactions + prior successes
 * - beta = failed interactions + prior failures
 * - Expected trust = alpha / (alpha + beta)
 * - Variance = (alpha * beta) / ((alpha + beta)^2 * (alpha + beta + 1))
 *
 * BAYESIAN UPDATE:
 * After observing outcome O:
 * - Success: alpha' = alpha + 1
 * - Failure: beta' = beta + 1
 *
 * This naturally handles:
 * - Uncertainty (wide distribution = uncertain)
 * - Evidence accumulation (distribution narrows with evidence)
 * - Prior knowledge (initial alpha, beta values)
 */
typedef struct {
    double alpha;                    /**< Beta distribution alpha (successes + prior) */
    double beta;                     /**< Beta distribution beta (failures + prior) */
    double expected_trust;           /**< E[Trust] = alpha / (alpha + beta) */
    double variance;                 /**< Var[Trust] for uncertainty */
    double confidence;               /**< Confidence = 1 - variance */
    uint64_t observations;           /**< Total observations */
} nimcp_trust_score_t;

/**
 * @brief Entity in trust network
 */
typedef struct {
    uint32_t entity_id;             /**< Unique entity identifier */
    char name[64];                  /**< Entity name */
    nimcp_trust_score_t direct_trust;   /**< Direct trust score */
    nimcp_trust_score_t derived_trust;  /**< Trust derived from vouchers */
    nimcp_trust_score_t combined_trust; /**< Combined final trust */

    uint32_t vouchers[NIMCP_TRUST_MAX_VOUCHERS]; /**< Entities vouching for this one */
    uint32_t voucher_count;         /**< Number of vouchers */

    uint64_t last_interaction;      /**< Timestamp of last interaction */
    bool active;                    /**< Entity is active */
} nimcp_trust_entity_t;

/**
 * @brief Trust propagation configuration
 */
typedef struct {
    double prior_alpha;             /**< Prior alpha (default: 1.0) */
    double prior_beta;              /**< Prior beta (default: 1.0) */
    double vouch_weight;            /**< Weight for voucher trust (default: 0.5) */
    double decay_rate;              /**< Trust decay per day (default: 0.01) */
    double propagation_damping;     /**< Damping for transitive trust (default: 0.8) */
    uint32_t max_propagation_depth; /**< Max depth for trust propagation */
    double min_trust_threshold;     /**< Minimum trust to propagate */
} nimcp_trust_config_t;

/**
 * @brief Trust network context
 */
typedef struct nimcp_trust_network nimcp_trust_network_t;

/**
 * @brief Create trust network
 * @return Network context or NULL on failure
 */
nimcp_trust_network_t* nimcp_trust_create(void);

/**
 * @brief Initialize trust network
 * @param network Network context
 * @param config Configuration (NULL for defaults)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_trust_init(
    nimcp_trust_network_t* network,
    const nimcp_trust_config_t* config
);

/**
 * @brief Destroy trust network
 * @param network Network context
 */
void nimcp_trust_destroy(nimcp_trust_network_t* network);

/**
 * @brief Register entity in trust network
 *
 * @param network Network context
 * @param entity_id Entity identifier
 * @param name Entity name
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_trust_register_entity(
    nimcp_trust_network_t* network,
    uint32_t entity_id,
    const char* name
);

/**
 * @brief Record interaction outcome (Bayesian update)
 *
 * BAYESIAN UPDATE FORMULA:
 * P(Trust|Evidence) = P(Evidence|Trust) * P(Trust) / P(Evidence)
 *
 * For Beta-Bernoulli model:
 * - Success: alpha' = alpha + 1
 * - Failure: beta' = beta + 1
 *
 * @param network Network context
 * @param entity_id Entity identifier
 * @param success Was interaction successful?
 * @param weight Interaction weight (default: 1.0)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_trust_record_interaction(
    nimcp_trust_network_t* network,
    uint32_t entity_id,
    bool success,
    double weight
);

/**
 * @brief Add voucher relationship
 *
 * Entity A vouches for Entity B, meaning:
 * - A's trust in B propagates to others
 * - Trust_derived(B) incorporates A's trust
 *
 * @param network Network context
 * @param voucher_id Entity doing the vouching
 * @param entity_id Entity being vouched for
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_trust_add_voucher(
    nimcp_trust_network_t* network,
    uint32_t voucher_id,
    uint32_t entity_id
);

/**
 * @brief Propagate trust through network
 *
 * ALGORITHM (iterative relaxation):
 * 1. Initialize derived_trust = direct_trust for all entities
 * 2. For each entity E with vouchers V1, V2, ...:
 *    derived_trust(E) = direct_trust(E) * (1 - vouch_weight) +
 *                       vouch_weight * weighted_avg(trust(Vi) * damping^depth)
 * 3. Repeat until convergence or max iterations
 *
 * @param network Network context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_trust_propagate(nimcp_trust_network_t* network);

/**
 * @brief Get trust score for entity
 *
 * @param network Network context
 * @param entity_id Entity identifier
 * @param score Output: trust score
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_trust_get_score(
    nimcp_trust_network_t* network,
    uint32_t entity_id,
    nimcp_trust_score_t* score
);

/**
 * @brief Check if entity is trusted
 *
 * @param network Network context
 * @param entity_id Entity identifier
 * @param threshold Minimum trust threshold
 * @return true if trusted, false otherwise
 */
bool nimcp_trust_is_trusted(
    nimcp_trust_network_t* network,
    uint32_t entity_id,
    double threshold
);

/**
 * @brief Calculate probability entity is trustworthy
 *
 * Uses Beta CDF: P(Trust >= threshold) = 1 - I_threshold(alpha, beta)
 * where I is the regularized incomplete beta function.
 *
 * @param score Trust score
 * @param threshold Trust threshold
 * @return Probability trust >= threshold
 */
double nimcp_trust_probability_above(
    const nimcp_trust_score_t* score,
    double threshold
);

/**
 * @brief Get default trust configuration
 * @return Default configuration
 */
nimcp_trust_config_t nimcp_trust_default_config(void);

//=============================================================================
// Part 3: Differential Privacy for Audit Logs
//=============================================================================

/**
 * @brief Differential privacy mechanism type
 *
 * MATHEMATICAL DEFINITION:
 * A mechanism M is (epsilon, delta)-differentially private if for all
 * neighboring datasets D1, D2 (differing in one record) and all outputs S:
 *
 * P(M(D1) in S) <= e^epsilon * P(M(D2) in S) + delta
 *
 * MECHANISMS:
 * - Laplace: Add Lap(sensitivity/epsilon) noise
 * - Gaussian: Add N(0, sensitivity^2 * 2*ln(1.25/delta) / epsilon^2) noise
 * - Exponential: Select output with probability prop. to e^(epsilon*score/2*sensitivity)
 */
typedef enum {
    NIMCP_DP_LAPLACE = 0,           /**< Laplacian noise (pure DP) */
    NIMCP_DP_GAUSSIAN,              /**< Gaussian noise (approximate DP) */
    NIMCP_DP_EXPONENTIAL            /**< Exponential mechanism (categorical) */
} nimcp_dp_mechanism_t;

/**
 * @brief Differential privacy configuration
 */
typedef struct {
    double epsilon;                 /**< Privacy parameter epsilon (smaller = more private) */
    double delta;                   /**< Privacy parameter delta (for approximate DP) */
    nimcp_dp_mechanism_t mechanism; /**< Noise mechanism */
    double total_budget;            /**< Total privacy budget */
    bool enforce_budget;            /**< Enforce budget limits */
} nimcp_dp_config_t;

/**
 * @brief Differential privacy query result
 */
typedef struct {
    double true_value;              /**< True query result (internal) */
    double noisy_value;             /**< Privatized result */
    double noise_added;             /**< Amount of noise added */
    double epsilon_spent;           /**< Privacy budget spent */
    double remaining_budget;        /**< Remaining privacy budget */
    double accuracy_bound;          /**< Probabilistic accuracy bound */
} nimcp_dp_result_t;

/**
 * @brief Differential privacy context
 */
typedef struct nimcp_dp_context nimcp_dp_context_t;

/**
 * @brief Create differential privacy context
 * @return Context or NULL on failure
 */
nimcp_dp_context_t* nimcp_dp_create(void);

/**
 * @brief Initialize DP context
 * @param ctx DP context
 * @param config Configuration (NULL for defaults)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_dp_init(
    nimcp_dp_context_t* ctx,
    const nimcp_dp_config_t* config
);

/**
 * @brief Destroy DP context
 * @param ctx DP context
 */
void nimcp_dp_destroy(nimcp_dp_context_t* ctx);

/**
 * @brief Add Laplacian noise to numeric value
 *
 * ALGORITHM:
 * 1. Calculate sensitivity: delta_f = max |f(D1) - f(D2)| for neighboring D1, D2
 * 2. Draw noise from Lap(sensitivity / epsilon)
 * 3. Return value + noise
 *
 * Laplace distribution: p(x) = (epsilon / 2*sensitivity) * e^(-epsilon * |x| / sensitivity)
 *
 * @param ctx DP context
 * @param value True value
 * @param sensitivity Query sensitivity
 * @param result Output: privatized result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_dp_add_laplace_noise(
    nimcp_dp_context_t* ctx,
    double value,
    double sensitivity,
    nimcp_dp_result_t* result
);

/**
 * @brief Add Gaussian noise for (epsilon, delta)-DP
 *
 * ALGORITHM:
 * sigma = sensitivity * sqrt(2 * ln(1.25 / delta)) / epsilon
 * noise ~ N(0, sigma^2)
 *
 * @param ctx DP context
 * @param value True value
 * @param sensitivity Query sensitivity
 * @param result Output: privatized result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_dp_add_gaussian_noise(
    nimcp_dp_context_t* ctx,
    double value,
    double sensitivity,
    nimcp_dp_result_t* result
);

/**
 * @brief Privatize a count query
 *
 * For COUNT queries, sensitivity = 1 (one person changes count by 1)
 *
 * @param ctx DP context
 * @param count True count
 * @param result Output: privatized result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_dp_count(
    nimcp_dp_context_t* ctx,
    uint64_t count,
    nimcp_dp_result_t* result
);

/**
 * @brief Privatize a sum query
 *
 * @param ctx DP context
 * @param sum True sum
 * @param max_contribution Maximum individual contribution
 * @param result Output: privatized result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_dp_sum(
    nimcp_dp_context_t* ctx,
    double sum,
    double max_contribution,
    nimcp_dp_result_t* result
);

/**
 * @brief Privatize a mean query
 *
 * Uses sensitivity = max_value / n for bounded means
 *
 * @param ctx DP context
 * @param mean True mean
 * @param count Number of records
 * @param max_value Maximum value
 * @param result Output: privatized result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_dp_mean(
    nimcp_dp_context_t* ctx,
    double mean,
    uint64_t count,
    double max_value,
    nimcp_dp_result_t* result
);

/**
 * @brief Privatize a histogram
 *
 * Adds noise to each bin independently. Sensitivity = 1 per bin.
 *
 * @param ctx DP context
 * @param histogram True histogram
 * @param num_bins Number of bins
 * @param noisy_histogram Output: privatized histogram
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_dp_histogram(
    nimcp_dp_context_t* ctx,
    const uint64_t* histogram,
    uint32_t num_bins,
    double* noisy_histogram
);

/**
 * @brief Get remaining privacy budget
 *
 * @param ctx DP context
 * @return Remaining epsilon budget
 */
double nimcp_dp_remaining_budget(const nimcp_dp_context_t* ctx);

/**
 * @brief Reset privacy budget
 *
 * @param ctx DP context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_dp_reset_budget(nimcp_dp_context_t* ctx);

/**
 * @brief Get default DP configuration
 * @return Default configuration
 */
nimcp_dp_config_t nimcp_dp_default_config(void);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Generate Laplacian random variable
 *
 * Uses inverse CDF method:
 * X = mu - b * sign(U - 0.5) * ln(1 - 2|U - 0.5|)
 * where U ~ Uniform(0, 1)
 *
 * @param scale Scale parameter b (= sensitivity / epsilon)
 * @return Laplacian random variable
 */
double nimcp_random_laplace(double scale);

/**
 * @brief Generate Gaussian random variable
 *
 * Uses Box-Muller transform:
 * Z = sqrt(-2 * ln(U1)) * cos(2 * pi * U2)
 *
 * @param mean Mean
 * @param stddev Standard deviation
 * @return Gaussian random variable
 */
double nimcp_random_gaussian(double mean, double stddev);

/**
 * @brief Calculate regularized incomplete beta function
 *
 * I_x(a, b) = B(x; a, b) / B(a, b)
 *
 * Used for Beta distribution CDF.
 *
 * @param x Value [0, 1]
 * @param a Alpha parameter
 * @param b Beta parameter
 * @return I_x(a, b)
 */
double nimcp_beta_incomplete(double x, double a, double b);

/**
 * @brief Calculate log2 safely (returns 0 for x <= 0)
 *
 * @param x Input value
 * @return log2(x) or 0 if x <= 0
 */
double nimcp_safe_log2(double x);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_MATH_H */
