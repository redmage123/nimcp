//=============================================================================
// nimcp_quantum_statistics.h - Quantum Statistics and Probability Module
//=============================================================================
/**
 * @file nimcp_quantum_statistics.h
 * @brief Quantum-enhanced statistics and probability functions
 *
 * WHAT: Quantum variants of all statistical and probability functions
 * WHY:  Enable quantum speedup and quantum information measures for neural computation
 * HOW:  Integrate quantum amplitudes, density matrices, and quantum algorithms
 *
 * QUANTUM ENHANCEMENTS:
 * - Von Neumann entropy and quantum mutual information
 * - Quantum relative entropy (quantum KL divergence)
 * - Quantum fidelity and trace distance
 * - Quantum Fisher information for parameter estimation
 * - Quantum amplitude-based probability distributions
 * - Integration with quantum walk, annealing, and Monte Carlo
 *
 * MATHEMATICAL FOUNDATION:
 *
 * Quantum State Representation:
 * - Pure state: |ψ⟩ = Σᵢ αᵢ|i⟩ where Σ|αᵢ|² = 1
 * - Density matrix: ρ = Σᵢ pᵢ|ψᵢ⟩⟨ψᵢ| (mixed states)
 * - Probability: P(i) = |⟨i|ψ⟩|² = |αᵢ|² (Born rule)
 *
 * Quantum Entropy:
 * - Von Neumann: S(ρ) = -Tr(ρ log ρ) = -Σᵢ λᵢ log λᵢ
 * - Quantum relative: S(ρ||σ) = Tr(ρ log ρ) - Tr(ρ log σ)
 * - Quantum mutual: I(A:B) = S(ρ_A) + S(ρ_B) - S(ρ_AB)
 *
 * Quantum Correlations:
 * - Fidelity: F(ρ,σ) = (Tr√(√ρ σ √ρ))²
 * - Trace distance: D(ρ,σ) = ½ Tr|ρ-σ|
 * - Quantum discord: D(A|B) = I(A:B) - J(A|B)
 *
 * INTEGRATION:
 * - nimcp_quantum_walk.h: Probability from quantum amplitudes
 * - nimcp_quantum_monte_carlo.h: MC sampling for large state spaces
 * - nimcp_quantum_annealing.h: Optimization with quantum tunneling
 * - nimcp_quantum_shannon.h: Channel capacity and information flow
 *
 * @author NIMCP Development Team
 * @date 2026-01-30
 * @version 1.0.0
 */

#ifndef NIMCP_QUANTUM_STATISTICS_H
#define NIMCP_QUANTUM_STATISTICS_H

#include "utils/statistics/nimcp_statistics.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Default numerical tolerance for quantum operations */
#define QSTATS_EPSILON              1e-10f

/** Maximum qubits for direct computation (2^20 = 1M states) */
#define QSTATS_MAX_DIRECT_QUBITS    20

/** Default Monte Carlo samples for large systems */
#define QSTATS_DEFAULT_MC_SAMPLES   10000

/** Minimum eigenvalue for log computation */
#define QSTATS_MIN_EIGENVALUE       1e-15f

//=============================================================================
// Return Codes
//=============================================================================

typedef enum qstats_result {
    QSTATS_OK               =  0,   /**< Success */
    QSTATS_ERROR_NULL       = -1,   /**< NULL parameter */
    QSTATS_ERROR_MEMORY     = -2,   /**< Memory allocation failed */
    QSTATS_ERROR_INVALID    = -3,   /**< Invalid parameter */
    QSTATS_ERROR_SIZE       = -4,   /**< Size mismatch */
    QSTATS_ERROR_NOT_NORMALIZED = -5, /**< State not normalized */
    QSTATS_ERROR_NOT_POSITIVE   = -6, /**< Matrix not positive semidefinite */
    QSTATS_ERROR_CONVERGENCE    = -7  /**< Algorithm did not converge */
} qstats_result_t;

//=============================================================================
// Quantum State Structures
//=============================================================================

/**
 * @brief Complex amplitude for quantum states
 */
typedef struct qstats_complex {
    float real;
    float imag;
} qstats_complex_t;

/**
 * @brief Pure quantum state (state vector)
 *
 * |ψ⟩ = Σᵢ αᵢ|i⟩ where αᵢ are complex amplitudes
 */
typedef struct qstats_pure_state {
    qstats_complex_t* amplitudes;   /**< Complex amplitudes [dim] */
    uint32_t dim;                   /**< Hilbert space dimension */
    bool normalized;                /**< Whether Σ|αᵢ|² = 1 */
} qstats_pure_state_t;

/**
 * @brief Density matrix for mixed quantum states
 *
 * ρ = Σᵢ pᵢ|ψᵢ⟩⟨ψᵢ| stored as Hermitian matrix
 */
typedef struct qstats_density_matrix {
    qstats_complex_t* elements;     /**< Matrix elements [dim × dim] row-major */
    uint32_t dim;                   /**< Matrix dimension */
    float* eigenvalues;             /**< Cached eigenvalues [dim] (NULL if not computed) */
    bool eigenvalues_valid;         /**< Whether eigenvalues are up-to-date */
} qstats_density_matrix_t;

/**
 * @brief Quantum measurement result
 */
typedef struct qstats_measurement {
    uint32_t outcome;               /**< Measured basis state index */
    float probability;              /**< Probability of outcome */
    qstats_complex_t amplitude;     /**< Amplitude of outcome */
} qstats_measurement_t;

/**
 * @brief Quantum entropy result structure
 */
typedef struct qstats_entropy_result {
    float von_neumann;              /**< Von Neumann entropy S(ρ) in bits */
    float von_neumann_nats;         /**< Von Neumann entropy in nats */
    float purity;                   /**< Tr(ρ²) - 1 for pure, <1 for mixed */
    float linear_entropy;           /**< S_L = 1 - Tr(ρ²) */
    float min_entropy;              /**< H_min = -log(λ_max) */
    float max_entropy;              /**< log(dim) */
    float renyi_2;                  /**< Rényi-2 entropy: -log(Tr(ρ²)) */
} qstats_entropy_result_t;

/**
 * @brief Quantum correlation result structure
 */
typedef struct qstats_correlation_result {
    float fidelity;                 /**< F(ρ,σ) ∈ [0,1] */
    float trace_distance;           /**< D(ρ,σ) ∈ [0,1] */
    float relative_entropy;         /**< S(ρ||σ) ≥ 0 */
    float bures_distance;           /**< d_B = √(2(1-√F)) */
    float hellinger_distance;       /**< Quantum Hellinger distance */
} qstats_correlation_result_t;

/**
 * @brief Quantum Fisher information result
 */
typedef struct qstats_fisher_result {
    float* fisher_matrix;           /**< Fisher information matrix [n_params × n_params] */
    float* cramer_rao_bounds;       /**< Lower bounds on variance [n_params] */
    uint32_t n_params;              /**< Number of parameters */
    float total_fisher_info;        /**< Tr(F) */
} qstats_fisher_result_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Quantum statistics configuration
 */
typedef struct qstats_config {
    uint32_t mc_samples;            /**< Monte Carlo samples for large systems */
    float tolerance;                /**< Numerical tolerance */
    bool use_gpu;                   /**< Use GPU acceleration if available */
    bool cache_eigenvalues;         /**< Cache eigenvalue decomposition */
    uint32_t seed;                  /**< RNG seed for MC methods */
} qstats_config_t;

/**
 * @brief Get default quantum statistics configuration
 */
NIMCP_EXPORT qstats_config_t qstats_default_config(void);

//=============================================================================
// State Creation and Management
//=============================================================================

/**
 * @brief Create pure quantum state
 *
 * @param dim Hilbert space dimension
 * @return Allocated state (caller must free with qstats_pure_state_destroy)
 */
NIMCP_EXPORT qstats_pure_state_t* qstats_pure_state_create(uint32_t dim);

/**
 * @brief Destroy pure quantum state
 */
NIMCP_EXPORT void qstats_pure_state_destroy(qstats_pure_state_t* state);

/**
 * @brief Create density matrix
 *
 * @param dim Matrix dimension
 * @return Allocated matrix (caller must free with qstats_density_matrix_destroy)
 */
NIMCP_EXPORT qstats_density_matrix_t* qstats_density_matrix_create(uint32_t dim);

/**
 * @brief Destroy density matrix
 */
NIMCP_EXPORT void qstats_density_matrix_destroy(qstats_density_matrix_t* dm);

/**
 * @brief Create density matrix from pure state
 *
 * ρ = |ψ⟩⟨ψ|
 */
NIMCP_EXPORT qstats_density_matrix_t* qstats_density_matrix_from_pure(
    const qstats_pure_state_t* state
);

/**
 * @brief Create density matrix from ensemble of pure states
 *
 * ρ = Σᵢ pᵢ|ψᵢ⟩⟨ψᵢ|
 */
NIMCP_EXPORT qstats_density_matrix_t* qstats_density_matrix_from_ensemble(
    const qstats_pure_state_t** states,
    const float* probabilities,
    uint32_t num_states
);

/**
 * @brief Create maximally mixed state
 *
 * ρ = I/d (uniform distribution over basis states)
 */
NIMCP_EXPORT qstats_density_matrix_t* qstats_density_matrix_maximally_mixed(uint32_t dim);

/**
 * @brief Create thermal state (Gibbs state)
 *
 * ρ = exp(-βH) / Z where Z = Tr(exp(-βH))
 *
 * @param hamiltonian Hamiltonian matrix [dim × dim]
 * @param dim Dimension
 * @param beta Inverse temperature (1/kT)
 */
NIMCP_EXPORT qstats_density_matrix_t* qstats_density_matrix_thermal(
    const float* hamiltonian,
    uint32_t dim,
    float beta
);

/**
 * @brief Normalize pure state
 *
 * Ensures Σ|αᵢ|² = 1
 */
NIMCP_EXPORT qstats_result_t qstats_pure_state_normalize(qstats_pure_state_t* state);

/**
 * @brief Check if density matrix is valid (positive semidefinite, Tr=1)
 */
NIMCP_EXPORT bool qstats_density_matrix_is_valid(const qstats_density_matrix_t* dm);

//=============================================================================
// Quantum Probability Distributions
//=============================================================================

/**
 * @brief Extract probability distribution from pure state (Born rule)
 *
 * P(i) = |αᵢ|²
 *
 * @param state Pure quantum state
 * @param probabilities Output probability array [dim] (caller allocated)
 */
NIMCP_EXPORT qstats_result_t qstats_born_probabilities(
    const qstats_pure_state_t* state,
    float* probabilities
);

/**
 * @brief Extract diagonal of density matrix (measurement probabilities)
 *
 * P(i) = ρᵢᵢ = ⟨i|ρ|i⟩
 */
NIMCP_EXPORT qstats_result_t qstats_diagonal_probabilities(
    const qstats_density_matrix_t* dm,
    float* probabilities
);

/**
 * @brief Create quantum state from classical probability distribution
 *
 * |ψ⟩ = Σᵢ √pᵢ|i⟩ (amplitude encoding)
 */
NIMCP_EXPORT qstats_pure_state_t* qstats_amplitude_encode(
    const float* probabilities,
    uint32_t dim
);

/**
 * @brief Simulate quantum measurement
 *
 * @param state Pure state (NOT modified - use qstats_measure_collapse for collapse)
 * @param result Output measurement result
 * @param seed RNG seed pointer
 */
NIMCP_EXPORT qstats_result_t qstats_measure(
    const qstats_pure_state_t* state,
    qstats_measurement_t* result,
    uint32_t* seed
);

/**
 * @brief Measure and collapse quantum state
 *
 * @param state Pure state (MODIFIED to post-measurement state)
 * @param result Output measurement result
 * @param seed RNG seed pointer
 */
NIMCP_EXPORT qstats_result_t qstats_measure_collapse(
    qstats_pure_state_t* state,
    qstats_measurement_t* result,
    uint32_t* seed
);

/**
 * @brief Finite-shot quantum measurement simulation
 *
 * @param state Pure state
 * @param num_shots Number of measurement shots
 * @param counts Output: measurement counts [dim] (caller allocated)
 * @param seed RNG seed pointer
 */
NIMCP_EXPORT qstats_result_t qstats_measure_finite_shots(
    const qstats_pure_state_t* state,
    uint32_t num_shots,
    uint32_t* counts,
    uint32_t* seed
);

//=============================================================================
// Quantum Entropy Functions
//=============================================================================

/**
 * @brief Compute Von Neumann entropy
 *
 * S(ρ) = -Tr(ρ log₂ ρ) = -Σᵢ λᵢ log₂ λᵢ
 *
 * @param dm Density matrix
 * @return Entropy in bits (0 for pure states, log(d) for maximally mixed)
 */
NIMCP_EXPORT float qstats_von_neumann_entropy(const qstats_density_matrix_t* dm);

/**
 * @brief Compute Von Neumann entropy in natural units (nats)
 *
 * S(ρ) = -Tr(ρ ln ρ)
 */
NIMCP_EXPORT float qstats_von_neumann_entropy_nats(const qstats_density_matrix_t* dm);

/**
 * @brief Compute all quantum entropy measures
 *
 * Returns Von Neumann, purity, linear entropy, min-entropy, Rényi-2
 */
NIMCP_EXPORT qstats_result_t qstats_entropy_all(
    const qstats_density_matrix_t* dm,
    qstats_entropy_result_t* result
);

/**
 * @brief Compute quantum relative entropy (quantum KL divergence)
 *
 * S(ρ||σ) = Tr(ρ log ρ) - Tr(ρ log σ)
 *
 * NOTE: Returns INFINITY if support(ρ) ⊄ support(σ)
 */
NIMCP_EXPORT float qstats_quantum_relative_entropy(
    const qstats_density_matrix_t* rho,
    const qstats_density_matrix_t* sigma
);

/**
 * @brief Compute quantum mutual information
 *
 * I(A:B) = S(ρ_A) + S(ρ_B) - S(ρ_AB)
 *
 * @param rho_ab Joint density matrix [dim_a × dim_b]
 * @param dim_a Dimension of subsystem A
 * @param dim_b Dimension of subsystem B
 */
NIMCP_EXPORT float qstats_quantum_mutual_information(
    const qstats_density_matrix_t* rho_ab,
    uint32_t dim_a,
    uint32_t dim_b
);

/**
 * @brief Compute quantum conditional entropy
 *
 * S(A|B) = S(ρ_AB) - S(ρ_B)
 *
 * NOTE: Can be negative (unlike classical conditional entropy)!
 */
NIMCP_EXPORT float qstats_quantum_conditional_entropy(
    const qstats_density_matrix_t* rho_ab,
    uint32_t dim_a,
    uint32_t dim_b
);

/**
 * @brief Compute purity of quantum state
 *
 * γ = Tr(ρ²) ∈ [1/d, 1]
 * 1 for pure states, 1/d for maximally mixed
 */
NIMCP_EXPORT float qstats_purity(const qstats_density_matrix_t* dm);

/**
 * @brief Compute linear entropy
 *
 * S_L = (d/(d-1))(1 - Tr(ρ²))
 *
 * Normalized to [0,1]
 */
NIMCP_EXPORT float qstats_linear_entropy(const qstats_density_matrix_t* dm);

/**
 * @brief Compute Rényi entropy of order α
 *
 * S_α(ρ) = (1/(1-α)) log(Tr(ρ^α))
 *
 * @param dm Density matrix
 * @param alpha Rényi order (α > 0, α ≠ 1)
 */
NIMCP_EXPORT float qstats_renyi_entropy(
    const qstats_density_matrix_t* dm,
    float alpha
);

/**
 * @brief Compute min-entropy
 *
 * H_min(ρ) = -log(λ_max) where λ_max is largest eigenvalue
 */
NIMCP_EXPORT float qstats_min_entropy(const qstats_density_matrix_t* dm);

/**
 * @brief Compute max-entropy (for dimension)
 *
 * H_max = log(d)
 */
NIMCP_EXPORT float qstats_max_entropy(uint32_t dim);

//=============================================================================
// Quantum Correlation and Distance Measures
//=============================================================================

/**
 * @brief Compute quantum fidelity
 *
 * F(ρ,σ) = (Tr√(√ρ σ √ρ))²
 *
 * For pure states: F(|ψ⟩,|φ⟩) = |⟨ψ|φ⟩|²
 *
 * @return Fidelity ∈ [0,1] (1 = identical states)
 */
NIMCP_EXPORT float qstats_fidelity(
    const qstats_density_matrix_t* rho,
    const qstats_density_matrix_t* sigma
);

/**
 * @brief Compute fidelity between pure states
 *
 * F = |⟨ψ|φ⟩|² = |Σᵢ αᵢ* βᵢ|²
 */
NIMCP_EXPORT float qstats_fidelity_pure(
    const qstats_pure_state_t* psi,
    const qstats_pure_state_t* phi
);

/**
 * @brief Compute trace distance
 *
 * D(ρ,σ) = ½ Tr|ρ-σ| = ½ Σᵢ |λᵢ|
 *
 * @return Distance ∈ [0,1] (0 = identical, 1 = orthogonal)
 */
NIMCP_EXPORT float qstats_trace_distance(
    const qstats_density_matrix_t* rho,
    const qstats_density_matrix_t* sigma
);

/**
 * @brief Compute Bures distance
 *
 * d_B(ρ,σ) = √(2(1-√F(ρ,σ)))
 */
NIMCP_EXPORT float qstats_bures_distance(
    const qstats_density_matrix_t* rho,
    const qstats_density_matrix_t* sigma
);

/**
 * @brief Compute quantum Hellinger distance
 *
 * d_H(ρ,σ) = √(2(1-Tr(√ρ√σ)))
 */
NIMCP_EXPORT float qstats_hellinger_distance(
    const qstats_density_matrix_t* rho,
    const qstats_density_matrix_t* sigma
);

/**
 * @brief Compute all quantum correlation measures
 */
NIMCP_EXPORT qstats_result_t qstats_correlation_all(
    const qstats_density_matrix_t* rho,
    const qstats_density_matrix_t* sigma,
    qstats_correlation_result_t* result
);

//=============================================================================
// Quantum Entanglement Measures
//=============================================================================

/**
 * @brief Compute entanglement entropy (bipartite)
 *
 * E(ρ_AB) = S(ρ_A) where ρ_A = Tr_B(ρ_AB)
 *
 * @param rho_ab Bipartite density matrix
 * @param dim_a Dimension of subsystem A
 * @param dim_b Dimension of subsystem B
 */
NIMCP_EXPORT float qstats_entanglement_entropy(
    const qstats_density_matrix_t* rho_ab,
    uint32_t dim_a,
    uint32_t dim_b
);

/**
 * @brief Compute concurrence (2-qubit entanglement)
 *
 * C(ρ) = max(0, λ₁-λ₂-λ₃-λ₄)
 * where λᵢ are sqrt of eigenvalues of ρ(σy⊗σy)ρ*(σy⊗σy) in decreasing order
 *
 * @param rho_ab 4×4 two-qubit density matrix
 * @return Concurrence ∈ [0,1] (0 = separable, 1 = maximally entangled)
 */
NIMCP_EXPORT float qstats_concurrence(const qstats_density_matrix_t* rho_ab);

/**
 * @brief Compute entanglement of formation (2-qubit)
 *
 * E_F(ρ) = h((1+√(1-C²))/2)
 * where h(x) = -x log x - (1-x) log(1-x) is binary entropy
 */
NIMCP_EXPORT float qstats_entanglement_of_formation(const qstats_density_matrix_t* rho_ab);

/**
 * @brief Compute negativity (bipartite entanglement)
 *
 * N(ρ) = (||ρ^(T_A)||₁ - 1) / 2
 * where ρ^(T_A) is partial transpose
 */
NIMCP_EXPORT float qstats_negativity(
    const qstats_density_matrix_t* rho_ab,
    uint32_t dim_a,
    uint32_t dim_b
);

/**
 * @brief Compute logarithmic negativity
 *
 * E_N(ρ) = log₂(||ρ^(T_A)||₁)
 */
NIMCP_EXPORT float qstats_log_negativity(
    const qstats_density_matrix_t* rho_ab,
    uint32_t dim_a,
    uint32_t dim_b
);

//=============================================================================
// Quantum Fisher Information
//=============================================================================

/**
 * @brief Compute quantum Fisher information for single parameter
 *
 * F_Q(ρ,H) = 2 Σᵢⱼ ((λᵢ-λⱼ)²/(λᵢ+λⱼ)) |⟨i|H|j⟩|²
 *
 * Bounds parameter estimation via quantum Cramér-Rao: Var(θ) ≥ 1/(n×F_Q)
 *
 * @param rho Quantum state
 * @param generator Hermitian generator H (∂ρ/∂θ = -i[H,ρ])
 */
NIMCP_EXPORT float qstats_quantum_fisher_information(
    const qstats_density_matrix_t* rho,
    const qstats_complex_t* generator,
    uint32_t dim
);

/**
 * @brief Compute quantum Fisher information matrix for multiple parameters
 *
 * F_ij = Re(Tr(ρ {L_i, L_j})) where L_i are SLD operators
 *
 * @param rho Quantum state
 * @param generators Array of Hermitian generators [n_params × dim × dim]
 * @param n_params Number of parameters
 * @param result Output Fisher information result
 */
NIMCP_EXPORT qstats_result_t qstats_quantum_fisher_matrix(
    const qstats_density_matrix_t* rho,
    const qstats_complex_t** generators,
    uint32_t n_params,
    qstats_fisher_result_t* result
);

/**
 * @brief Free Fisher information result
 */
NIMCP_EXPORT void qstats_fisher_result_free(qstats_fisher_result_t* result);

/**
 * @brief Compute symmetric logarithmic derivative (SLD)
 *
 * Solves: ∂ρ/∂θ = (L ρ + ρ L) / 2
 *
 * @param rho Density matrix
 * @param drho_dtheta Derivative of density matrix w.r.t. parameter
 * @param sld Output: SLD operator [dim × dim]
 */
NIMCP_EXPORT qstats_result_t qstats_symmetric_log_derivative(
    const qstats_density_matrix_t* rho,
    const qstats_complex_t* drho_dtheta,
    qstats_complex_t* sld,
    uint32_t dim
);

//=============================================================================
// Quantum Hypothesis Testing
//=============================================================================

/**
 * @brief Quantum state discrimination (minimum error)
 *
 * Finds optimal POVM for distinguishing ρ₀ vs ρ₁ with minimum error
 *
 * @param rho0 State 0 (null hypothesis)
 * @param rho1 State 1 (alternative)
 * @param prior0 Prior probability of state 0
 * @return Minimum error probability
 */
NIMCP_EXPORT float qstats_quantum_discrimination_error(
    const qstats_density_matrix_t* rho0,
    const qstats_density_matrix_t* rho1,
    float prior0
);

/**
 * @brief Quantum Chernoff bound
 *
 * Error exponent for distinguishing ρ vs σ with many copies:
 * P_err ≤ exp(-n × ξ_QCB)
 *
 * ξ_QCB = -log(min_s Tr(ρ^s σ^(1-s))) for s ∈ [0,1]
 */
NIMCP_EXPORT float qstats_quantum_chernoff_bound(
    const qstats_density_matrix_t* rho,
    const qstats_density_matrix_t* sigma
);

/**
 * @brief Quantum Hoeffding bound exponent
 *
 * For testing ρ vs σ with false positive rate ≤ exp(-nR):
 * β_n ≤ exp(-n × H_R(ρ||σ))
 *
 * @param rho Null hypothesis state
 * @param sigma Alternative state
 * @param R Exponent constraint on false positive rate
 */
NIMCP_EXPORT float qstats_quantum_hoeffding_exponent(
    const qstats_density_matrix_t* rho,
    const qstats_density_matrix_t* sigma,
    float R
);

//=============================================================================
// Integration with Quantum Walk
//=============================================================================

/**
 * @brief Convert quantum walk amplitudes to pure state
 *
 * @param amplitudes_real Real parts of amplitudes [num_nodes]
 * @param amplitudes_imag Imaginary parts of amplitudes [num_nodes]
 * @param num_nodes Number of nodes (dimension)
 */
NIMCP_EXPORT qstats_pure_state_t* qstats_from_quantum_walk(
    const float* amplitudes_real,
    const float* amplitudes_imag,
    uint32_t num_nodes
);

/**
 * @brief Compute entropy of quantum walk probability distribution
 *
 * H = -Σᵢ |αᵢ|² log |αᵢ|²
 *
 * @param amplitudes_real Real parts [num_nodes]
 * @param amplitudes_imag Imaginary parts [num_nodes]
 * @param num_nodes Number of nodes
 */
NIMCP_EXPORT float qstats_quantum_walk_entropy(
    const float* amplitudes_real,
    const float* amplitudes_imag,
    uint32_t num_nodes
);

/**
 * @brief Compute localization of quantum walk (inverse participation ratio)
 *
 * IPR = Σᵢ |αᵢ|⁴ (higher = more localized)
 * Returns 1/IPR = effective number of sites occupied
 */
NIMCP_EXPORT float qstats_quantum_walk_localization(
    const float* amplitudes_real,
    const float* amplitudes_imag,
    uint32_t num_nodes
);

/**
 * @brief Compute quantum coherence of walk state
 *
 * C = Σᵢ≠ⱼ |⟨i|ρ|j⟩| (l1-norm of coherence)
 */
NIMCP_EXPORT float qstats_quantum_walk_coherence(
    const float* amplitudes_real,
    const float* amplitudes_imag,
    uint32_t num_nodes
);

//=============================================================================
// Integration with Quantum Annealing
//=============================================================================

/**
 * @brief Compute Boltzmann distribution from annealing energy landscape
 *
 * P(i) = exp(-E_i/T) / Z where Z = Σⱼ exp(-E_j/T)
 *
 * @param energies Energy values [num_states]
 * @param num_states Number of states
 * @param temperature Temperature T
 * @param probabilities Output probabilities [num_states]
 */
NIMCP_EXPORT qstats_result_t qstats_boltzmann_distribution(
    const float* energies,
    uint32_t num_states,
    float temperature,
    float* probabilities
);

/**
 * @brief Compute free energy from energy distribution
 *
 * F = -T log Z = -T log(Σᵢ exp(-Eᵢ/T))
 */
NIMCP_EXPORT float qstats_free_energy(
    const float* energies,
    uint32_t num_states,
    float temperature
);

/**
 * @brief Compute thermodynamic entropy from Boltzmann distribution
 *
 * S = (⟨E⟩ - F) / T
 */
NIMCP_EXPORT float qstats_thermodynamic_entropy(
    const float* energies,
    uint32_t num_states,
    float temperature
);

/**
 * @brief Compute partition function
 *
 * Z = Σᵢ exp(-Eᵢ/T)
 */
NIMCP_EXPORT float qstats_partition_function(
    const float* energies,
    uint32_t num_states,
    float temperature
);

/**
 * @brief Estimate partition function via Monte Carlo
 *
 * More efficient for large state spaces
 */
NIMCP_EXPORT float qstats_partition_function_mc(
    const float* energies,
    uint32_t num_states,
    float temperature,
    uint32_t num_samples,
    float* variance_out
);

//=============================================================================
// Monte Carlo Methods for Quantum Statistics
//=============================================================================

/**
 * @brief Estimate Von Neumann entropy via Monte Carlo
 *
 * For large systems where eigenvalue decomposition is expensive
 */
NIMCP_EXPORT float qstats_von_neumann_entropy_mc(
    const qstats_density_matrix_t* dm,
    uint32_t num_samples,
    float* variance_out
);

/**
 * @brief Estimate quantum relative entropy via Monte Carlo
 */
NIMCP_EXPORT float qstats_quantum_relative_entropy_mc(
    const qstats_density_matrix_t* rho,
    const qstats_density_matrix_t* sigma,
    uint32_t num_samples,
    float* variance_out
);

/**
 * @brief Estimate fidelity via Monte Carlo
 */
NIMCP_EXPORT float qstats_fidelity_mc(
    const qstats_density_matrix_t* rho,
    const qstats_density_matrix_t* sigma,
    uint32_t num_samples,
    float* variance_out
);

//=============================================================================
// Quantum Channel Statistics
//=============================================================================

/**
 * @brief Compute quantum channel capacity (classical capacity)
 *
 * C(N) = max_ρ χ(N,{pᵢ,ρᵢ})
 *
 * Simplified: uses Holevo information χ = S(Σᵢ pᵢ N(ρᵢ)) - Σᵢ pᵢ S(N(ρᵢ))
 *
 * @param channel_kraus Kraus operators [num_kraus × dim × dim]
 * @param num_kraus Number of Kraus operators
 * @param dim Channel dimension
 */
NIMCP_EXPORT float qstats_quantum_channel_capacity(
    const qstats_complex_t** channel_kraus,
    uint32_t num_kraus,
    uint32_t dim
);

/**
 * @brief Compute coherent information for quantum channel
 *
 * I_c(ρ,N) = S(N(ρ)) - S(N^c(ρ))
 *
 * where N^c is complementary channel
 */
NIMCP_EXPORT float qstats_coherent_information(
    const qstats_density_matrix_t* rho,
    const qstats_complex_t** channel_kraus,
    uint32_t num_kraus,
    uint32_t dim
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Compute inner product of two pure states
 *
 * ⟨ψ|φ⟩ = Σᵢ αᵢ* βᵢ
 */
NIMCP_EXPORT qstats_complex_t qstats_inner_product(
    const qstats_pure_state_t* psi,
    const qstats_pure_state_t* phi
);

/**
 * @brief Compute partial trace over subsystem B
 *
 * ρ_A = Tr_B(ρ_AB)
 */
NIMCP_EXPORT qstats_density_matrix_t* qstats_partial_trace_b(
    const qstats_density_matrix_t* rho_ab,
    uint32_t dim_a,
    uint32_t dim_b
);

/**
 * @brief Compute partial trace over subsystem A
 */
NIMCP_EXPORT qstats_density_matrix_t* qstats_partial_trace_a(
    const qstats_density_matrix_t* rho_ab,
    uint32_t dim_a,
    uint32_t dim_b
);

/**
 * @brief Compute partial transpose over subsystem A
 *
 * Used for Peres-Horodecki criterion (entanglement detection)
 */
NIMCP_EXPORT qstats_density_matrix_t* qstats_partial_transpose_a(
    const qstats_density_matrix_t* rho_ab,
    uint32_t dim_a,
    uint32_t dim_b
);

/**
 * @brief Compute eigenvalues of density matrix
 *
 * @param dm Density matrix
 * @param eigenvalues Output eigenvalues [dim] (caller allocated)
 */
NIMCP_EXPORT qstats_result_t qstats_eigenvalues(
    const qstats_density_matrix_t* dm,
    float* eigenvalues
);

/**
 * @brief Compute matrix trace
 */
NIMCP_EXPORT float qstats_trace(const qstats_density_matrix_t* dm);

/**
 * @brief Compute Tr(ρ²)
 */
NIMCP_EXPORT float qstats_trace_squared(const qstats_density_matrix_t* dm);

/**
 * @brief Complex number operations
 */
NIMCP_EXPORT qstats_complex_t qstats_complex_add(qstats_complex_t a, qstats_complex_t b);
NIMCP_EXPORT qstats_complex_t qstats_complex_sub(qstats_complex_t a, qstats_complex_t b);
NIMCP_EXPORT qstats_complex_t qstats_complex_mul(qstats_complex_t a, qstats_complex_t b);
NIMCP_EXPORT qstats_complex_t qstats_complex_conj(qstats_complex_t a);
NIMCP_EXPORT float qstats_complex_abs(qstats_complex_t a);
NIMCP_EXPORT float qstats_complex_abs_squared(qstats_complex_t a);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_QUANTUM_STATISTICS_H */
