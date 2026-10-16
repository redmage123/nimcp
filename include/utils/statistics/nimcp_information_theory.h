//=============================================================================
// nimcp_information_theory.h - Advanced Information Theory Extensions
//=============================================================================
/**
 * @file nimcp_information_theory.h
 * @brief Advanced information-theoretic measures for neural computation
 *
 * WHAT: Comprehensive information theory toolkit extending beyond basic Shannon
 *       measures to include Partial Information Decomposition (PID), Renyi
 *       entropies, quantum correlations, causal information, and complexity measures.
 *
 * WHY:  Modern neural computation requires understanding information flow at
 *       multiple levels - from synergistic coding in neural populations to
 *       causal inference in cognitive hierarchies. These advanced measures
 *       provide the mathematical foundation for:
 *       - Understanding how information is distributed across neural ensembles
 *       - Detecting causal vs correlational relationships
 *       - Measuring integrated information (consciousness theories)
 *       - Analyzing quantum-classical boundaries in neural computation
 *
 * HOW:  C99 implementation with GPU acceleration for computationally intensive
 *       operations, numerical stability guarantees, and full exception handling
 *       with immune system integration.
 *
 * THEORETICAL FOUNDATION:
 *
 *   Information Decomposition Hierarchy:
 *   +---------------------------------------------------------------------------+
 *   |                                                                           |
 *   |  Classical Shannon:        H(X), I(X;Y), D_KL(P||Q)                       |
 *   |         |                                                                 |
 *   |         v                                                                 |
 *   |  Partial Information:      Unique, Redundant, Synergistic components     |
 *   |  Decomposition (PID)       I(S1,S2 -> T) = U1 + U2 + R + S               |
 *   |         |                                                                 |
 *   |         v                                                                 |
 *   |  Renyi Generalization:     H_alpha, I_alpha - parametric family          |
 *   |         |                                                                 |
 *   |         v                                                                 |
 *   |  Quantum Extensions:       Von Neumann, Discord, Accessible Information  |
 *   |         |                                                                 |
 *   |         v                                                                 |
 *   |  Causal Information:       Directed I(X->Y), Transfer Entropy            |
 *   |         |                                                                 |
 *   |         v                                                                 |
 *   |  Complexity Measures:      Phi, Statistical Complexity, Excess Entropy   |
 *   +---------------------------------------------------------------------------+
 *
 * NEUROSCIENCE APPLICATIONS:
 *
 *   PID in Neural Coding:
 *   - Redundancy: Multiple neurons encoding same stimulus feature
 *   - Synergy: Information only available from joint activity
 *   - Unique: Information carried by individual neurons alone
 *
 *   Integrated Information (Phi):
 *   - Measures irreducible causal structure of a system
 *   - Theoretical basis for consciousness measures (IIT)
 *   - Quantifies "integrated-ness" of neural circuits
 *
 *   Causal Information Flow:
 *   - Granger causality generalization
 *   - Effective connectivity analysis
 *   - Information-theoretic control
 *
 * PERFORMANCE:
 * - PID computation: O(n^3) for n sources, GPU accelerated for n > 8
 * - Renyi entropy: O(n log n) with optimized binning
 * - Quantum discord: O(d^6) for d-dimensional systems
 * - Phi computation: Exponential in system size, approximations for large systems
 *
 * @author NIMCP Development Team
 * @date 2026-01-30
 * @version 1.0.0
 */

#ifndef NIMCP_INFORMATION_THEORY_H
#define NIMCP_INFORMATION_THEORY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <float.h>

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of sources for PID computation */
#define INFO_THEORY_MAX_PID_SOURCES         16

/** Maximum dimension for quantum correlation computations */
#define INFO_THEORY_MAX_QUANTUM_DIM         64

/** Default number of bins for continuous data discretization */
#define INFO_THEORY_DEFAULT_BINS            32

/** Numerical tolerance for information measures */
#define INFO_THEORY_EPSILON                 1e-12

/** Maximum iterations for optimization algorithms */
#define INFO_THEORY_MAX_ITERATIONS          1000

/** Convergence threshold for iterative algorithms */
#define INFO_THEORY_CONVERGENCE_THRESHOLD   1e-8

/** Default history length for causal measures */
#define INFO_THEORY_DEFAULT_HISTORY         3

//=============================================================================
// Error Codes
//=============================================================================

/**
 * @brief Result codes for information theory operations
 */
typedef enum info_theory_result {
    INFO_THEORY_OK = 0,                  /**< Success */
    INFO_THEORY_ERROR_NULL = -1,         /**< NULL pointer argument */
    INFO_THEORY_ERROR_SIZE = -2,         /**< Invalid size or dimension */
    INFO_THEORY_ERROR_MEMORY = -3,       /**< Memory allocation failed */
    INFO_THEORY_ERROR_PARAMS = -4,       /**< Invalid parameters */
    INFO_THEORY_ERROR_CONVERGE = -5,     /**< Algorithm did not converge */
    INFO_THEORY_ERROR_SINGULAR = -6,     /**< Singular matrix encountered */
    INFO_THEORY_ERROR_RANGE = -7,        /**< Value out of valid range */
    INFO_THEORY_ERROR_NOT_INIT = -8,     /**< Module not initialized */
    INFO_THEORY_ERROR_GPU = -9,          /**< GPU computation failed */
    INFO_THEORY_ERROR_NOT_POSITIVE = -10 /**< Matrix not positive definite */
} info_theory_result_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Configuration for information theory module
 */
typedef struct info_theory_config {
    bool enable_gpu;                     /**< Use GPU acceleration if available */
    uint32_t gpu_threshold;              /**< Size threshold for GPU offload */
    uint32_t default_bins;               /**< Default bins for discretization */
    uint32_t max_iterations;             /**< Max iterations for optimization */
    float convergence_threshold;         /**< Convergence criterion */
    uint32_t random_seed;                /**< Seed for stochastic methods */
    bool enable_logging;                 /**< Enable diagnostic logging */
} info_theory_config_t;

//=============================================================================
// Partial Information Decomposition (PID) Types
//=============================================================================

/**
 * @brief PID computation method
 *
 * Different methods give different decompositions - choice depends on
 * application and philosophical stance on redundancy/synergy.
 */
typedef enum pid_method {
    PID_METHOD_BROJA,                    /**< BROJA (Bivariate Redundancy Only) */
    PID_METHOD_CCS,                      /**< Common Change in Surprisal */
    PID_METHOD_MMI,                      /**< Minimum Mutual Information */
    PID_METHOD_DEP,                      /**< Dependency decomposition */
    PID_METHOD_SXY,                      /**< Shared exclusions */
    PID_METHOD_IBROJA                    /**< Iterative BROJA */
} pid_method_t;

/**
 * @brief Two-source PID result
 *
 * For sources S1, S2 predicting target T:
 * I(S1,S2 ; T) = Unique_1 + Unique_2 + Redundancy + Synergy
 */
typedef struct pid_bivariate_result {
    float total_mi;                      /**< Total mutual information I(S1,S2;T) */
    float unique_1;                      /**< Unique information from S1 */
    float unique_2;                      /**< Unique information from S2 */
    float redundancy;                    /**< Redundant/shared information */
    float synergy;                       /**< Synergistic information */
    float mi_s1_t;                       /**< I(S1;T) */
    float mi_s2_t;                       /**< I(S2;T) */
    pid_method_t method;                 /**< Method used */
    bool converged;                      /**< Whether optimization converged */
    uint32_t iterations;                 /**< Iterations used */
} pid_bivariate_result_t;

/**
 * @brief PID atom for n-source decomposition
 *
 * Each atom represents information shared by a specific subset of sources.
 * For n sources, there are 2^(2^n - 1) - 1 atoms (partial order lattice).
 */
typedef struct pid_atom {
    uint32_t sources_mask;               /**< Bitmask of contributing sources */
    float value;                         /**< Information value in bits */
    char description[64];                /**< Human-readable description */
} pid_atom_t;

/**
 * @brief Full n-source PID result
 */
typedef struct pid_full_result {
    uint32_t n_sources;                  /**< Number of sources */
    float total_mi;                      /**< Total mutual information */
    pid_atom_t* atoms;                   /**< Array of PID atoms */
    uint32_t n_atoms;                    /**< Number of atoms */
    float* unique;                       /**< Unique info per source [n_sources] */
    float total_redundancy;              /**< Sum of redundant terms */
    float total_synergy;                 /**< Sum of synergistic terms */
    pid_method_t method;                 /**< Method used */
    bool converged;                      /**< Whether optimization converged */
} pid_full_result_t;

//=============================================================================
// Renyi Information Measure Types
//=============================================================================

/**
 * @brief Renyi divergence variant
 */
typedef enum renyi_variant {
    RENYI_STANDARD,                      /**< Standard Renyi divergence */
    RENYI_SANDWICHED,                    /**< Sandwiched Renyi divergence */
    RENYI_PETZ                           /**< Petz Renyi divergence */
} renyi_variant_t;

/**
 * @brief Renyi entropy result with multiple orders
 */
typedef struct renyi_result {
    float order;                         /**< Renyi order alpha */
    float entropy;                       /**< H_alpha in bits */
    float entropy_nats;                  /**< H_alpha in nats */
    float min_entropy;                   /**< H_infinity (alpha -> inf) */
    float collision_entropy;             /**< H_2 (Renyi-2) */
    float hartley_entropy;               /**< H_0 (log of support size) */
    float shannon_limit;                 /**< Limit as alpha -> 1 (Shannon) */
} renyi_result_t;

//=============================================================================
// Quantum Correlation Types
//=============================================================================

/**
 * @brief Quantum discord computation method
 */
typedef enum discord_method {
    DISCORD_EXACT,                       /**< Exact optimization (slow, accurate) */
    DISCORD_NUMERICAL,                   /**< Numerical optimization */
    DISCORD_GEOMETRIC                    /**< Geometric discord approximation */
} discord_method_t;

/**
 * @brief Quantum correlation result
 */
typedef struct quantum_correlation_result {
    float quantum_discord;               /**< Quantum discord D(A|B) */
    float classical_correlation;         /**< Classical correlation J(A|B) */
    float quantum_mutual_info;           /**< Quantum I(A:B) */
    float entanglement_measure;          /**< Entanglement (if applicable) */
    float accessible_information;        /**< Holevo bound */
    float coherent_information;          /**< I_coherent */
    discord_method_t method;             /**< Method used */
    bool converged;                      /**< Whether optimization converged */
} quantum_correlation_result_t;

//=============================================================================
// Causal Information Types
//=============================================================================

/**
 * @brief Directed information result
 */
typedef struct directed_info_result {
    float directed_info;                 /**< I(X^n -> Y^n) in bits */
    float reverse_directed;              /**< I(Y^n -> X^n) in bits */
    float net_flow;                      /**< directed - reverse */
    float normalized_flow;               /**< Net flow / max possible */
    uint32_t history_length;             /**< History length used */
    float* per_step;                     /**< Per-timestep values [n] */
    uint32_t n_steps;                    /**< Number of timesteps */
} directed_info_result_t;

/**
 * @brief Causally conditioned entropy result
 */
typedef struct causal_entropy_result {
    float causal_entropy;                /**< H(Y || X) causally conditioned */
    float standard_conditional;          /**< H(Y | X) standard conditional */
    float causal_gain;                   /**< Difference (information benefit) */
} causal_entropy_result_t;

//=============================================================================
// Complexity Measure Types
//=============================================================================

/**
 * @brief Phi (integrated information) computation method
 */
typedef enum phi_method {
    PHI_IIT_3_0,                         /**< IIT 3.0 (Tononi) */
    PHI_STAR,                            /**< Phi* approximation */
    PHI_EMPIRICAL,                       /**< Empirical approximation */
    PHI_ATOMIC                           /**< Atomic information integration */
} phi_method_t;

/**
 * @brief Integrated information result
 */
typedef struct integration_result {
    float phi;                           /**< Phi value (integrated information) */
    float phi_normalized;                /**< Normalized phi [0,1] */
    uint32_t* partition;                 /**< Minimum information partition */
    uint32_t n_partitions;               /**< Number of partition elements */
    float* phi_atoms;                    /**< Per-element phi values */
    phi_method_t method;                 /**< Method used */
    bool is_approximation;               /**< Whether result is approximated */
} integration_result_t;

/**
 * @brief Statistical complexity result
 */
typedef struct complexity_result {
    float statistical_complexity;        /**< C_mu (epsilon-machine complexity) */
    float excess_entropy;                /**< E (mutual info between past/future) */
    float predictive_information;        /**< I_pred */
    float entropy_rate;                  /**< h_mu (per-symbol entropy rate) */
    float metric_entropy;                /**< Kolmogorov-Sinai entropy */
    float effective_measure_complexity;  /**< EMC */
    uint32_t history_used;               /**< History length used */
} complexity_result_t;

//=============================================================================
// Module Initialization
//=============================================================================

/**
 * @brief Get default configuration
 * @return Default configuration
 */
NIMCP_EXPORT info_theory_config_t info_theory_default_config(void);

/**
 * @brief Initialize the information theory module
 * @param config Configuration (NULL for defaults)
 * @return INFO_THEORY_OK on success
 *
 * WHAT: Initialize information theory subsystem
 * WHY:  Sets up GPU integration, RNG, logging
 * HOW:  One-time setup, thread-safe initialization
 */
NIMCP_EXPORT info_theory_result_t info_theory_init(const info_theory_config_t* config);

/**
 * @brief Shutdown the information theory module
 *
 * WHAT: Clean up information theory subsystem
 * WHY:  Release GPU resources, cached computations
 * HOW:  Free internal allocations
 */
NIMCP_EXPORT void info_theory_shutdown(void);

/**
 * @brief Check if module is initialized
 * @return true if initialized
 */
NIMCP_EXPORT bool info_theory_is_initialized(void);

//=============================================================================
// Partial Information Decomposition (PID)
//=============================================================================

/**
 * @brief Compute full PID decomposition for two sources
 *
 * @param joint_prob  Joint probability P(S1, S2, T) [n_s1 x n_s2 x n_t]
 * @param n_s1        Number of states for source 1
 * @param n_s2        Number of states for source 2
 * @param n_t         Number of states for target
 * @param method      PID computation method
 * @param result      Output PID result
 * @return INFO_THEORY_OK on success
 *
 * WHAT: Decompose mutual information into unique, redundant, and synergistic parts
 * WHY:  Understand how multiple sources contribute to target information
 * HOW:  Optimization over joint distributions matching marginals
 *
 * The decomposition satisfies:
 *   I(S1;T) = U1 + R
 *   I(S2;T) = U2 + R
 *   I(S1,S2;T) = U1 + U2 + R + S
 *
 * GPU: Accelerated for n_s1 * n_s2 * n_t > 1000
 */
NIMCP_EXPORT info_theory_result_t nimcp_info_pid_compute(
    const float* joint_prob,
    uint32_t n_s1,
    uint32_t n_s2,
    uint32_t n_t,
    pid_method_t method,
    pid_bivariate_result_t* result
);

/**
 * @brief Compute unique information from a single source
 *
 * @param joint_prob  Joint probability P(S, T) [n_s x n_t]
 * @param n_s         Number of source states
 * @param n_t         Number of target states
 * @param other_prob  Marginal of other source P(S_other) [n_other]
 * @param n_other     Number of other source states
 * @param method      PID method
 * @param unique_info Output unique information value
 * @return INFO_THEORY_OK on success
 *
 * WHAT: Compute information uniquely provided by one source
 * WHY:  Identify exclusive information contributions
 * HOW:  Minimize over joint distributions with fixed marginals
 */
NIMCP_EXPORT info_theory_result_t nimcp_info_unique_information(
    const float* joint_prob,
    uint32_t n_s,
    uint32_t n_t,
    const float* other_prob,
    uint32_t n_other,
    pid_method_t method,
    float* unique_info
);

/**
 * @brief Compute redundant (shared) information
 *
 * @param joint_prob  Joint probability P(S1, S2, T) [n_s1 x n_s2 x n_t]
 * @param n_s1        Number of states for source 1
 * @param n_s2        Number of states for source 2
 * @param n_t         Number of states for target
 * @param method      PID method
 * @param redundancy  Output redundancy value
 * @return INFO_THEORY_OK on success
 *
 * WHAT: Compute information shared by all sources
 * WHY:  Identify overlapping information contributions
 * HOW:  Find common information accessible from any source alone
 */
NIMCP_EXPORT info_theory_result_t nimcp_info_redundant_information(
    const float* joint_prob,
    uint32_t n_s1,
    uint32_t n_s2,
    uint32_t n_t,
    pid_method_t method,
    float* redundancy
);

/**
 * @brief Compute synergistic information
 *
 * @param joint_prob  Joint probability P(S1, S2, T) [n_s1 x n_s2 x n_t]
 * @param n_s1        Number of states for source 1
 * @param n_s2        Number of states for source 2
 * @param n_t         Number of states for target
 * @param method      PID method
 * @param synergy     Output synergy value
 * @return INFO_THEORY_OK on success
 *
 * WHAT: Compute information only available from joint consideration
 * WHY:  Identify emergent information from source combination
 * HOW:  synergy = I(S1,S2;T) - U1 - U2 - R
 */
NIMCP_EXPORT info_theory_result_t nimcp_info_synergistic_information(
    const float* joint_prob,
    uint32_t n_s1,
    uint32_t n_s2,
    uint32_t n_t,
    pid_method_t method,
    float* synergy
);

/**
 * @brief Compute all PID atoms for n sources
 *
 * @param joint_prob  Joint probability P(S1,...,Sn,T) [prod(n_si) x n_t]
 * @param n_sources   Array of state counts per source
 * @param num_sources Number of sources (max 8 for exact computation)
 * @param n_t         Number of target states
 * @param method      PID method
 * @param result      Output full PID result (caller must free with pid_result_free)
 * @return INFO_THEORY_OK on success
 *
 * WHAT: Complete partial information decomposition
 * WHY:  Full understanding of multi-source information structure
 * HOW:  Solve constrained optimization over information lattice
 *
 * NOTE: Complexity is exponential in num_sources. For num_sources > 4,
 *       approximation methods are automatically used.
 */
NIMCP_EXPORT info_theory_result_t nimcp_info_pid_atoms(
    const float* joint_prob,
    const uint32_t* n_sources,
    uint32_t num_sources,
    uint32_t n_t,
    pid_method_t method,
    pid_full_result_t* result
);

/**
 * @brief Free PID result memory
 * @param result PID result to free
 */
NIMCP_EXPORT void nimcp_info_pid_result_free(pid_full_result_t* result);

//=============================================================================
// Renyi Information Measures
//=============================================================================

/**
 * @brief Compute Renyi entropy of order alpha
 *
 * @param probabilities Probability distribution [n]
 * @param n             Number of outcomes
 * @param alpha         Renyi order (alpha > 0, alpha != 1)
 * @return Renyi entropy in bits, or NAN on error
 *
 * WHAT: Generalized entropy measure with parameter alpha
 * WHY:  Different alpha values emphasize different distribution aspects
 * HOW:  H_alpha(X) = (1/(1-alpha)) * log(sum_i p_i^alpha)
 *
 * Special cases:
 * - alpha -> 0: Hartley entropy (log of support size)
 * - alpha -> 1: Shannon entropy (as limit)
 * - alpha = 2: Collision entropy (-log of collision probability)
 * - alpha -> inf: Min-entropy (-log of max probability)
 */
NIMCP_EXPORT float nimcp_info_renyi_entropy(
    const float* probabilities,
    uint32_t n,
    float alpha
);

/**
 * @brief Compute Renyi divergence
 *
 * @param p      First distribution [n]
 * @param q      Second distribution [n]
 * @param n      Number of outcomes
 * @param alpha  Renyi order (alpha > 0, alpha != 1)
 * @param variant Divergence variant (standard, sandwiched, Petz)
 * @return Renyi divergence in bits, or NAN/INF on error
 *
 * WHAT: Generalized divergence between distributions
 * WHY:  Tighter bounds in hypothesis testing than KL
 * HOW:  D_alpha(P||Q) = (1/(alpha-1)) * log(sum_i p_i^alpha * q_i^(1-alpha))
 */
NIMCP_EXPORT float nimcp_info_renyi_divergence(
    const float* p,
    const float* q,
    uint32_t n,
    float alpha,
    renyi_variant_t variant
);

/**
 * @brief Compute Renyi mutual information
 *
 * @param joint_prob Joint probability [n_x x n_y]
 * @param n_x        Number of X outcomes
 * @param n_y        Number of Y outcomes
 * @param alpha      Renyi order
 * @return Renyi mutual information in bits
 *
 * WHAT: Generalized mutual information with Renyi entropies
 * WHY:  Different operational meanings for different alpha
 * HOW:  I_alpha(X;Y) = H_alpha(X) + H_alpha(Y) - H_alpha(X,Y)
 */
NIMCP_EXPORT float nimcp_info_renyi_mutual_info(
    const float* joint_prob,
    uint32_t n_x,
    uint32_t n_y,
    float alpha
);

/**
 * @brief Compute Tsallis entropy
 *
 * @param probabilities Probability distribution [n]
 * @param n             Number of outcomes
 * @param q             Tsallis parameter (q != 1)
 * @return Tsallis entropy (non-negative), or NAN on error
 *
 * WHAT: Non-extensive entropy generalization
 * WHY:  Models systems with long-range correlations
 * HOW:  S_q(X) = (1 - sum_i p_i^q) / (q - 1)
 *
 * Properties:
 * - Non-additive: S_q(A,B) = S_q(A) + S_q(B) + (1-q)*S_q(A)*S_q(B)
 * - Reduces to Shannon as q -> 1
 */
NIMCP_EXPORT float nimcp_info_tsallis_entropy(
    const float* probabilities,
    uint32_t n,
    float q
);

/**
 * @brief Compute all Renyi measures for a distribution
 *
 * @param probabilities Probability distribution [n]
 * @param n             Number of outcomes
 * @param alpha         Primary Renyi order
 * @param result        Output Renyi result structure
 * @return INFO_THEORY_OK on success
 */
NIMCP_EXPORT info_theory_result_t nimcp_info_renyi_all(
    const float* probabilities,
    uint32_t n,
    float alpha,
    renyi_result_t* result
);

//=============================================================================
// Quantum Correlations
//=============================================================================

/**
 * @brief Compute quantum discord
 *
 * @param rho_ab       Joint density matrix [dim_a*dim_b x dim_a*dim_b]
 * @param dim_a        Dimension of subsystem A
 * @param dim_b        Dimension of subsystem B
 * @param method       Computation method
 * @param discord      Output discord value
 * @return INFO_THEORY_OK on success
 *
 * WHAT: Measure of quantum correlations beyond entanglement
 * WHY:  Captures non-classical correlations in separable states
 * HOW:  D(A|B) = I(A:B) - J(A|B) where J is classical correlation
 *
 * Discord measures the information about A that cannot be extracted
 * by measuring B alone. Non-zero even for some separable states.
 *
 * GPU: Accelerated for dim_a * dim_b > 16
 */
NIMCP_EXPORT info_theory_result_t nimcp_info_quantum_discord(
    const float* rho_ab,
    uint32_t dim_a,
    uint32_t dim_b,
    discord_method_t method,
    float* discord
);

/**
 * @brief Compute classical correlation
 *
 * @param rho_ab       Joint density matrix [dim_a*dim_b x dim_a*dim_b]
 * @param dim_a        Dimension of subsystem A
 * @param dim_b        Dimension of subsystem B
 * @param classical    Output classical correlation value
 * @return INFO_THEORY_OK on success
 *
 * WHAT: Classical part of quantum mutual information
 * WHY:  Separate classical from quantum contributions
 * HOW:  J(A|B) = max_{Pi_B} I(A : Pi_B) over local measurements
 */
NIMCP_EXPORT info_theory_result_t nimcp_info_classical_correlation(
    const float* rho_ab,
    uint32_t dim_a,
    uint32_t dim_b,
    float* classical
);

/**
 * @brief Compute quantum mutual information
 *
 * @param rho_ab       Joint density matrix
 * @param dim_a        Dimension of subsystem A
 * @param dim_b        Dimension of subsystem B
 * @param qmi          Output quantum mutual information
 * @return INFO_THEORY_OK on success
 *
 * WHAT: Total correlations in quantum state
 * WHY:  Bounds on communication and information processing
 * HOW:  I(A:B) = S(rho_A) + S(rho_B) - S(rho_AB)
 */
NIMCP_EXPORT info_theory_result_t nimcp_info_quantum_mutual_info(
    const float* rho_ab,
    uint32_t dim_a,
    uint32_t dim_b,
    float* qmi
);

/**
 * @brief Compute accessible information (Holevo bound)
 *
 * @param ensemble_states  Ensemble of quantum states [n_states x dim x dim]
 * @param probabilities    Ensemble probabilities [n_states]
 * @param n_states         Number of states in ensemble
 * @param dim              State dimension
 * @param accessible       Output accessible information
 * @return INFO_THEORY_OK on success
 *
 * WHAT: Maximum classical information extractable from quantum ensemble
 * WHY:  Fundamental limit on quantum-to-classical information transfer
 * HOW:  chi = S(sum_i p_i rho_i) - sum_i p_i S(rho_i) (Holevo quantity)
 *
 * The accessible information is bounded by chi (equality for commuting states).
 */
NIMCP_EXPORT info_theory_result_t nimcp_info_accessible_information(
    const float* ensemble_states,
    const float* probabilities,
    uint32_t n_states,
    uint32_t dim,
    float* accessible
);

/**
 * @brief Compute all quantum correlation measures
 *
 * @param rho_ab       Joint density matrix
 * @param dim_a        Dimension of subsystem A
 * @param dim_b        Dimension of subsystem B
 * @param method       Discord computation method
 * @param result       Output correlation result
 * @return INFO_THEORY_OK on success
 */
NIMCP_EXPORT info_theory_result_t nimcp_info_quantum_correlations_all(
    const float* rho_ab,
    uint32_t dim_a,
    uint32_t dim_b,
    discord_method_t method,
    quantum_correlation_result_t* result
);

//=============================================================================
// Causal Information
//=============================================================================

/**
 * @brief Compute directed information I(X -> Y)
 *
 * @param x            Source time series [n]
 * @param y            Target time series [n]
 * @param n            Length of time series
 * @param history      History length (embedding dimension)
 * @param n_bins       Number of bins for discretization
 * @param result       Output directed information result
 * @return INFO_THEORY_OK on success
 *
 * WHAT: Causal information flow from X to Y over time
 * WHY:  Measures information X "sends" to Y (not just shares)
 * HOW:  I(X^n -> Y^n) = sum_t I(X^t ; Y_t | Y^{t-1})
 *
 * Directed information captures the asymmetric, causal component
 * of information transfer. Unlike mutual information, it can
 * distinguish X causing Y from Y causing X.
 *
 * GPU: Accelerated for n > 10000
 */
NIMCP_EXPORT info_theory_result_t nimcp_info_directed_information(
    const float* x,
    const float* y,
    uint32_t n,
    uint32_t history,
    uint32_t n_bins,
    directed_info_result_t* result
);

/**
 * @brief Compute causally conditioned entropy
 *
 * @param y            Target time series [n]
 * @param x            Conditioning time series [n]
 * @param n            Length of time series
 * @param history      History length
 * @param n_bins       Number of bins for discretization
 * @param result       Output causal entropy result
 * @return INFO_THEORY_OK on success
 *
 * WHAT: Entropy of Y given causal history of X
 * WHY:  Foundation for directed information computation
 * HOW:  H(Y^n || X^n) = sum_t H(Y_t | Y^{t-1}, X^t)
 *
 * Uses causal conditioning: Y_t depends only on X^t (past and present),
 * not future X values.
 */
NIMCP_EXPORT info_theory_result_t nimcp_info_causally_conditioned(
    const float* y,
    const float* x,
    uint32_t n,
    uint32_t history,
    uint32_t n_bins,
    causal_entropy_result_t* result
);

/**
 * @brief Compute net information flow between variables
 *
 * @param x            First time series [n]
 * @param y            Second time series [n]
 * @param n            Length of time series
 * @param history      History length
 * @param n_bins       Number of bins
 * @param flow_x_to_y  Output: net flow X -> Y (positive = X causes Y)
 * @param flow_y_to_x  Output: net flow Y -> X
 * @return INFO_THEORY_OK on success
 *
 * WHAT: Bidirectional causal information analysis
 * WHY:  Determine dominant causal direction
 * HOW:  Compute I(X->Y) and I(Y->X), return difference
 */
NIMCP_EXPORT info_theory_result_t nimcp_info_information_flow(
    const float* x,
    const float* y,
    uint32_t n,
    uint32_t history,
    uint32_t n_bins,
    float* flow_x_to_y,
    float* flow_y_to_x
);

/**
 * @brief Free directed information result memory
 * @param result Result to free
 */
NIMCP_EXPORT void nimcp_info_directed_result_free(directed_info_result_t* result);

//=============================================================================
// Complexity Measures
//=============================================================================

/**
 * @brief Compute integrated information (Phi)
 *
 * @param tpm          Transition probability matrix [n_states x n_states]
 * @param n_states     Number of system states
 * @param method       Phi computation method
 * @param result       Output integration result
 * @return INFO_THEORY_OK on success
 *
 * WHAT: Measure of irreducible causal structure
 * WHY:  Core measure in Integrated Information Theory (IIT)
 * HOW:  Phi = information generated by system above its parts
 *
 * Phi quantifies how much a system is more than the sum of its parts
 * in terms of information integration. Systems with high Phi have
 * strong causal interactions that cannot be reduced to simpler components.
 *
 * NOTE: Exact computation is exponential in n_states. For n_states > 12,
 *       approximation methods are automatically used.
 *
 * GPU: Accelerated for n_states > 8
 */
NIMCP_EXPORT info_theory_result_t nimcp_info_integration(
    const float* tpm,
    uint32_t n_states,
    phi_method_t method,
    integration_result_t* result
);

/**
 * @brief Compute statistical complexity
 *
 * @param data         Time series data [n]
 * @param n            Length of time series
 * @param history      Maximum history length
 * @param n_bins       Number of bins for discretization
 * @param result       Output complexity result
 * @return INFO_THEORY_OK on success
 *
 * WHAT: Complexity of optimal predictor (epsilon-machine)
 * WHY:  Measures intrinsic computational structure
 * HOW:  C_mu = H(causal states) - minimum memory for optimal prediction
 *
 * Statistical complexity captures the memory required to optimally
 * predict a process - a measure of its computational irreducibility.
 */
NIMCP_EXPORT info_theory_result_t nimcp_info_complexity(
    const float* data,
    uint32_t n,
    uint32_t history,
    uint32_t n_bins,
    complexity_result_t* result
);

/**
 * @brief Compute excess entropy
 *
 * @param data         Time series data [n]
 * @param n            Length of time series
 * @param max_history  Maximum history length to consider
 * @param n_bins       Number of bins for discretization
 * @param excess       Output excess entropy value
 * @return INFO_THEORY_OK on success
 *
 * WHAT: Mutual information between past and future
 * WHY:  Measures long-range temporal correlations
 * HOW:  E = lim_{L->inf} [H(X_{-L:0}) + H(X_{0:L}) - H(X_{-L:L})]
 *
 * Also known as effective measure complexity. High excess entropy
 * indicates strong dependencies between past and future - the
 * system has "memory" that affects its future evolution.
 */
NIMCP_EXPORT info_theory_result_t nimcp_info_excess_entropy(
    const float* data,
    uint32_t n,
    uint32_t max_history,
    uint32_t n_bins,
    float* excess
);

/**
 * @brief Compute predictive information
 *
 * @param data         Time series data [n]
 * @param n            Length of time series
 * @param history      History length for prediction
 * @param future       Future length to predict
 * @param n_bins       Number of bins
 * @param predictive   Output predictive information
 * @return INFO_THEORY_OK on success
 *
 * WHAT: Information about future contained in past
 * WHY:  Measures predictability of a process
 * HOW:  I_pred = I(X_{-h:0} ; X_{0:f})
 *
 * Predictive information quantifies how much knowing the past
 * helps predict the future. For i.i.d. processes, this is zero.
 */
NIMCP_EXPORT info_theory_result_t nimcp_info_predictive_information(
    const float* data,
    uint32_t n,
    uint32_t history,
    uint32_t future,
    uint32_t n_bins,
    float* predictive
);

/**
 * @brief Free integration result memory
 * @param result Result to free
 */
NIMCP_EXPORT void nimcp_info_integration_result_free(integration_result_t* result);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Discretize continuous data into bins
 *
 * @param data         Continuous data [n]
 * @param n            Number of samples
 * @param n_bins       Number of bins
 * @param bins         Output bin assignments [n]
 * @param bin_edges    Output bin edges [n_bins + 1] (can be NULL)
 * @return INFO_THEORY_OK on success
 *
 * Uses equal-width binning. For adaptive binning, use _adaptive variant.
 */
NIMCP_EXPORT info_theory_result_t nimcp_info_discretize(
    const float* data,
    uint32_t n,
    uint32_t n_bins,
    uint32_t* bins,
    float* bin_edges
);

/**
 * @brief Discretize with adaptive binning (equal frequency)
 *
 * @param data         Continuous data [n]
 * @param n            Number of samples
 * @param n_bins       Number of bins
 * @param bins         Output bin assignments [n]
 * @param bin_edges    Output bin edges [n_bins + 1] (can be NULL)
 * @return INFO_THEORY_OK on success
 */
NIMCP_EXPORT info_theory_result_t nimcp_info_discretize_adaptive(
    const float* data,
    uint32_t n,
    uint32_t n_bins,
    uint32_t* bins,
    float* bin_edges
);

/**
 * @brief Estimate joint probability from samples
 *
 * @param x            First variable samples [n]
 * @param y            Second variable samples [n]
 * @param n            Number of samples
 * @param n_bins_x     Bins for x
 * @param n_bins_y     Bins for y
 * @param joint        Output joint probability [n_bins_x x n_bins_y]
 * @return INFO_THEORY_OK on success
 */
NIMCP_EXPORT info_theory_result_t nimcp_info_estimate_joint(
    const float* x,
    const float* y,
    uint32_t n,
    uint32_t n_bins_x,
    uint32_t n_bins_y,
    float* joint
);

/**
 * @brief Apply bias correction to entropy estimate
 *
 * @param raw_entropy  Raw entropy estimate
 * @param n_samples    Number of samples used
 * @param n_bins       Number of bins
 * @return Bias-corrected entropy estimate
 *
 * Uses Miller-Madow correction: H_corrected = H_raw + (k-1)/(2n)
 * where k is number of non-empty bins.
 */
NIMCP_EXPORT float nimcp_info_bias_correction(
    float raw_entropy,
    uint32_t n_samples,
    uint32_t n_bins
);

/**
 * @brief Get error message for result code
 * @param result Result code
 * @return Human-readable error message
 */
NIMCP_EXPORT const char* nimcp_info_error_string(info_theory_result_t result);

//=============================================================================
// GPU Acceleration Interface
//=============================================================================

/**
 * @brief Check if GPU acceleration is available
 * @return true if GPU is available and initialized
 */
NIMCP_EXPORT bool nimcp_info_gpu_available(void);

/**
 * @brief Force GPU computation for next operation
 *
 * Bypasses size threshold check. Reset after one operation.
 */
NIMCP_EXPORT void nimcp_info_force_gpu(void);

/**
 * @brief Force CPU computation for next operation
 *
 * Bypasses GPU availability. Reset after one operation.
 */
NIMCP_EXPORT void nimcp_info_force_cpu(void);

/**
 * @brief Get GPU computation statistics
 *
 * @param gpu_calls    Output: number of GPU-accelerated calls
 * @param cpu_calls    Output: number of CPU calls
 * @param gpu_time_ms  Output: total GPU computation time in ms
 * @param cpu_time_ms  Output: total CPU computation time in ms
 */
NIMCP_EXPORT void nimcp_info_get_gpu_stats(
    uint64_t* gpu_calls,
    uint64_t* cpu_calls,
    double* gpu_time_ms,
    double* cpu_time_ms
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_INFORMATION_THEORY_H
