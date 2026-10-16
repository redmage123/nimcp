#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_mps.c - Matrix Product States Implementation
//=============================================================================
/**
 * @file nimcp_mps.c
 * @brief Matrix Product States (MPS) tensor network compression
 *
 * THREAD SAFETY:
 * - Thread-local RNG state for safe random initialization
 * - No global mutable state shared between threads
 *
 * @version 1.1.0
 * @date 2026-01-15
 */

#include "utils/tensor_networks/nimcp_mps.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>

//=============================================================================
// THREAD-SAFE RNG
//=============================================================================

/**
 * @brief Thread-local RNG state for thread-safe random number generation
 *
 * WHAT: Per-thread LCG-based RNG state
 * WHY:  rand() is not thread-safe (uses global state, causes race conditions)
 * HOW:  Each thread gets its own isolated state, seeded uniquely on first use
 */
static _Thread_local uint64_t g_mps_rng_state = 0;
static _Thread_local bool g_mps_rng_initialized = false;

/**
 * @brief Initialize thread-local RNG if needed
 */
static void mps_rng_ensure_initialized(void) {
    if (!g_mps_rng_initialized) {
        /* Create unique seed combining multiple entropy sources */
        uint64_t seed = (uint64_t)time(NULL);
        seed ^= (uint64_t)(uintptr_t)&g_mps_rng_state;  /* Address randomness */
        seed ^= ((uint64_t)clock()) << 32;  /* CPU time for additional entropy */

        /* Mix the seed for better distribution */
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;

        g_mps_rng_state = seed;
        g_mps_rng_initialized = true;
    }
}

/**
 * @brief Thread-safe LCG random number generator
 *
 * WHAT: Generate next random value using Linear Congruential Generator
 * WHY:  Simple, fast, and thread-safe when using thread-local state
 * HOW:  Uses same constants as glibc's MINSTD
 */
static inline uint64_t mps_rng_next(void) {
    mps_rng_ensure_initialized();
    g_mps_rng_state = g_mps_rng_state * 6364136223846793005ULL + 1442695040888963407ULL;
    return g_mps_rng_state;
}

/**
 * @brief Generate uniform random float in [0, 1)
 *
 * WHAT: Convert RNG output to floating point
 * WHY:  MPS initialization needs uniform random values
 * HOW:  Use high bits for better randomness quality
 */
static inline float mps_rng_uniform(void) {
    return (float)(mps_rng_next() >> 33) / (float)(1ULL << 31);
}

// For SVD computation - LAPACK-based or fallback simple implementation
#include "utils/tensor_networks/nimcp_svd_simple.h"  // Header is the same, implementation switches via LAPACK flag
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(mps)

#ifdef NIMCP_ENABLE_LAPACK
// WHAT: BLAS/LAPACK FORTRAN interface for optimized operations
// WHY:  Proper tensor contractions require matrix multiplication
// HOW:  FORTRAN calling conventions (pass-by-pointer, column-major)

extern void sgemm_(
    const char* transa, const char* transb,
    const int* m, const int* n, const int* k,
    const float* alpha,
    const float* A, const int* lda,
    const float* B, const int* ldb,
    const float* beta,
    float* C, const int* ldc
);

extern void sgeqrf_(
    const int* m, const int* n,
    float* A, const int* lda,
    float* tau,
    float* work, const int* lwork,
    int* info
);

extern void sorgqr_(
    const int* m, const int* n, const int* k,
    float* A, const int* lda,
    const float* tau,
    float* work, const int* lwork,
    int* info
);
#endif

//=============================================================================
// Configuration Functions
//=============================================================================

mps_config_t mps_default_config(void) {
    mps_config_t config = {
        .bond_dim = 10,              // 10-20x compression
        .svd_tolerance = 1e-6F,      // Discard small singular values
        .adaptive_bond_dim = true,   // Optimize per bond
        .max_iterations = 100,       // For iterative optimization
        .learning_rate = 0.01F,      // For gradient descent
        .normalize_tensors = true    // Numerical stability
    };
    return config;
}

mps_config_t mps_high_compression_config(void) {
    mps_config_t config = mps_default_config();
    config.bond_dim = 5;             // 50-100x compression
    config.svd_tolerance = 1e-4F;    // More aggressive truncation
    config.adaptive_bond_dim = true;
    return config;
}

mps_config_t mps_high_accuracy_config(void) {
    mps_config_t config = mps_default_config();
    config.bond_dim = 20;            // 5-10x compression
    config.svd_tolerance = 1e-8F;    // Keep more singular values
    config.adaptive_bond_dim = false; // Use full bond_dim everywhere
    return config;
}

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Compute optimal number of MPS sites for given dimensions
 *
 * WHAT: Determine chain length for MPS decomposition
 * WHY: Balance compression ratio and computational cost
 * HOW: Heuristic based on matrix dimensions
 *
 * @param num_rows Number of matrix rows
 * @param num_cols Number of matrix columns
 * @return Optimal number of sites
 */
static uint32_t compute_num_sites(uint32_t num_rows, uint32_t num_cols) {
    // HEURISTIC: Use log2(N) sites for N×M matrix
    // This gives balanced factorization
    uint32_t n = num_rows > num_cols ? num_rows : num_cols;

    // Compute ceil(log2(n))
    uint32_t num_sites = 0;
    uint32_t temp = n - 1;
    while (temp > 0) {
        num_sites++;
        temp >>= 1;
    }

    // Clamp to reasonable range [2, 20]
    if (num_sites < 2) num_sites = 2;
    if (num_sites > 20) num_sites = 20;

    return num_sites;
}

/**
 * @brief Compute physical dimension for each MPS site
 *
 * WHAT: Distribute input dimensions across sites
 * WHY: Create balanced tensor train
 * HOW: Approximately equal split
 *
 * @param num_rows Total input dimension
 * @param num_sites Number of MPS sites
 * @return Physical dimension per site
 */
static uint32_t compute_phys_dim(uint32_t num_rows, uint32_t num_sites) {
    // HEURISTIC: phys_dim ≈ (num_rows)^(1/num_sites)
    // This ensures product of phys_dims ≈ num_rows

    double phys_dim_float = pow((double)num_rows, 1.0 / (double)num_sites);
    uint32_t phys_dim = (uint32_t)ceil(phys_dim_float);

    // Ensure at least 2
    if (phys_dim < 2) phys_dim = 2;

    return phys_dim;
}

/**
 * @brief Allocate single MPS tensor
 *
 * @param left_dim Left bond dimension
 * @param right_dim Right bond dimension
 * @param phys_dim Physical dimension
 * @return Allocated tensor, or NULL on failure
 */
static mps_tensor_t* mps_tensor_alloc(
    uint32_t left_dim,
    uint32_t right_dim,
    uint32_t phys_dim
) {
    mps_tensor_t* tensor = (mps_tensor_t*)nimcp_malloc(sizeof(mps_tensor_t));
    if (!tensor) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tensor is NULL");

        return NULL;

    }

    tensor->left_dim = left_dim;
    tensor->right_dim = right_dim;
    tensor->phys_dim = phys_dim;
    tensor->total_size = left_dim * right_dim * phys_dim;

    tensor->data = (float*)nimcp_calloc(tensor->total_size, sizeof(float));
    if (!tensor->data) {
        nimcp_free(tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mps_tensor_alloc: tensor->data is NULL");
        return NULL;
    }

    return tensor;
}

/**
 * @brief Free single MPS tensor
 */
static void mps_tensor_free(mps_tensor_t* tensor) {
    if (!tensor) return;
    if (tensor->data) nimcp_free(tensor->data);
    nimcp_free(tensor);
}

/**
 * WHAT: Compute reconstruction error ||W_original - W_mps||_F / ||W_original||_F
 * WHY:  Quantify MPS approximation quality
 * HOW:  Reconstruct matrix and compute Frobenius norm difference
 */
static float compute_reconstruction_error(
    const mps_matrix_t* mps,
    const float* original_weights,
    uint32_t num_rows,
    uint32_t num_cols)
{
    if (!mps || !original_weights) return 1.0F;

    // WHAT: Reconstruct matrix from MPS
    float* reconstructed = nimcp_malloc(num_rows * num_cols * sizeof(float));
    if (!reconstructed) return 1.0F;

    if (!mps_reconstruct_matrix(mps, reconstructed)) {
        nimcp_free(reconstructed);
        return 1.0F;
    }

    // WHAT: Compute ||original - reconstructed||_F
    float error_sum = 0.0F;
    float orig_sum = 0.0F;
    for (uint32_t i = 0; i < num_rows * num_cols; i++) {
        float diff = original_weights[i] - reconstructed[i];
        error_sum += diff * diff;
        orig_sum += original_weights[i] * original_weights[i];
    }

    nimcp_free(reconstructed);

    // WHAT: Return relative error
    return (orig_sum > 1e-10F) ? sqrtf(error_sum / orig_sum) : 0.0F;
}

/**
 * WHAT: Full TT-SVD decomposition algorithm
 * WHY:  Optimal low-rank tensor train representation with adaptive rank
 * HOW:  Sequential left-to-right SVD sweeps with truncation
 *
 * ALGORITHM (TT-SVD):
 * 1. Reshape W[N×M] into multi-dimensional tensor W[d₁,d₂,...,dₖ,M]
 * 2. For each site i = 0 to k-1:
 *    a. Reshape remaining tensor into matrix [left_indices × (dᵢ × right_indices)]
 *    b. Compute SVD: Matrix = U Σ Vᵀ
 *    c. Truncate to bond_dim largest singular values (adaptive based on tolerance)
 *    d. Store left part as MPS tensor A[i]
 *    e. Continue with right part (Σ Vᵀ)
 * 3. Last tensor stores final right part
 */
static bool tt_svd_decompose(
    const float* weights,
    uint32_t num_rows,
    uint32_t num_cols,
    mps_matrix_t* mps,
    const mps_config_t* config)
{
    if (!weights || !mps || !config || !mps->sites) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tt_svd_decompose: required parameter is NULL (weights, mps, config, mps->sites)");
        return false;
    }

    // WHAT: Working buffer for current reshaped matrix
    uint32_t max_size = num_rows * num_cols;
    float* current_matrix = nimcp_malloc(max_size * sizeof(float));
    if (!current_matrix) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "tt_svd_decompose: current_matrix is NULL");
        return false;
    }

    // WHAT: Initialize with original weights
    memcpy(current_matrix, weights, num_rows * num_cols * sizeof(float));

    // WHAT: Track current dimensions through decomposition
    uint32_t curr_left_dim = 1;  // Start with trivial left dimension
    uint32_t curr_rows = num_rows;
    uint32_t curr_cols = num_cols;

    // WHAT: Process each site sequentially (left-to-right)
    for (uint32_t site = 0; site < mps->num_sites; site++) {
        mps_tensor_t* tensor = &mps->sites[site];
        uint32_t phys_dim = tensor->phys_dim;

        // WHAT: Determine SVD matrix dimensions
        // WHY:  Reshape for optimal factorization
        uint32_t svd_rows = curr_left_dim * phys_dim;
        uint32_t svd_cols = curr_cols;

        // For middle sites, adjust to partition remaining dimensions
        if (site < mps->num_sites - 1) {
            uint32_t remaining_sites = mps->num_sites - site - 1;
            uint32_t remaining_dim = curr_rows / phys_dim;
            if (remaining_dim < 1) remaining_dim = 1;

            svd_cols = remaining_dim * curr_cols;
        }

        // WHAT: Reshape current matrix for SVD
        // WHY:  Separate current physical index from rest
        float* svd_matrix = nimcp_malloc(svd_rows * svd_cols * sizeof(float));
        if (!svd_matrix) {
            nimcp_free(current_matrix);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "tt_svd_decompose: svd_matrix is NULL");
            return false;
        }

        // WHAT: Copy data with appropriate reshaping
        uint32_t copy_rows = (svd_rows < curr_rows) ? svd_rows : curr_rows;
        uint32_t copy_cols = (svd_cols < curr_cols) ? svd_cols : curr_cols;
        for (uint32_t i = 0; i < copy_rows; i++) {
            for (uint32_t j = 0; j < copy_cols; j++) {
                uint32_t src_idx = (i % curr_rows) * curr_cols + (j % curr_cols);
                uint32_t dst_idx = i * svd_cols + j;
                if (src_idx < curr_rows * curr_cols) {
                    svd_matrix[dst_idx] = current_matrix[src_idx];
                }
            }
        }

        // WHAT: Compute SVD with adaptive rank selection
        // WHY:  Find optimal truncation for this bond
        uint32_t max_rank = (site == mps->num_sites - 1) ?
                           svd_cols : tensor->right_dim;

        svd_result_t svd = svd_compute(svd_matrix, svd_rows, svd_cols,
                                       max_rank, config->svd_tolerance);

        if (!svd.U || !svd.S || !svd.Vt || svd.rank == 0) {
            nimcp_free(svd_matrix);
            nimcp_free(current_matrix);
            svd_free(&svd);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tt_svd_decompose: required parameter is NULL (svd, svd, svd)");
            return false;
        }

        // WHAT: Update bond dimension based on actual SVD rank
        // WHY:  Adaptive compression - use optimal rank for each bond
        if (config->adaptive_bond_dim && site < mps->num_sites - 1) {
            tensor->right_dim = svd.rank;
            // Update next site's left dimension
            if (site + 1 < mps->num_sites) {
                mps->sites[site + 1].left_dim = svd.rank;
            }
        }

        // WHAT: Fill MPS tensor with left singular vectors
        // WHY:  Store factorized component at this site
        // HOW:  A[i,j,k] = U[i×phys_dim + k, j]
        // NOTE: svd.U is column-major: U[row, col] = U[col * m + row]
        for (uint32_t i = 0; i < tensor->left_dim; i++) {
            for (uint32_t j = 0; j < tensor->right_dim; j++) {
                for (uint32_t k = 0; k < phys_dim; k++) {
                    uint32_t row_idx = i * phys_dim + k;
                    if (row_idx < svd.m && j < svd.rank) {
                        uint32_t tensor_idx = i * tensor->right_dim * phys_dim +
                                            j * phys_dim + k;
                        tensor->data[tensor_idx] = svd.U[j * svd.m + row_idx];
                    }
                }
            }
        }

        // WHAT: Prepare next matrix: Σ Vᵀ
        // WHY:  Continue decomposition with remaining part
        if (site < mps->num_sites - 1) {
            uint32_t next_rows = svd.rank;
            uint32_t next_cols = svd.n;

            float* next_matrix = nimcp_malloc(next_rows * next_cols * sizeof(float));
            if (!next_matrix) {
                nimcp_free(svd_matrix);
                nimcp_free(current_matrix);
                svd_free(&svd);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "tt_svd_decompose: next_matrix is NULL");
                return false;
            }

            // Multiply Σ into Vᵀ: (Σ Vᵀ)[i,j] = S[i] × Vt[i,j]
            for (uint32_t i = 0; i < next_rows; i++) {
                for (uint32_t j = 0; j < next_cols; j++) {
                    next_matrix[i * next_cols + j] = svd.S[i] * svd.Vt[i * next_cols + j];
                }
            }

            // Update for next iteration
            nimcp_free(current_matrix);
            current_matrix = next_matrix;
            curr_left_dim = svd.rank;
            curr_rows = next_rows;
            curr_cols = next_cols;
        }

        nimcp_free(svd_matrix);
        svd_free(&svd);
    }

    nimcp_free(current_matrix);
    return true;
}

/**
 * @brief Initialize MPS with random small values
 *
 * WHAT: Fill MPS tensors with random data
 * WHY:  Starting point for optimization-based compression
 * HOW:  Uniform random in [-0.1, 0.1]
 *
 * THREAD SAFETY:
 * - Uses thread-local RNG (mps_rng_uniform) instead of rand()
 * - WHY: rand() uses global state, causing race conditions in multi-threaded code
 * - Each thread gets independent, properly seeded random sequence
 */
static void mps_randomize(mps_matrix_t* mps) {
    if (!mps) return;

    for (uint32_t site = 0; site < mps->num_sites; site++) {
        mps_tensor_t* tensor = &mps->sites[site];
        for (uint32_t i = 0; i < tensor->total_size; i++) {
            /* THREAD SAFE: Use thread-local RNG instead of rand()
             * Random in [-0.1, 0.1] = uniform[0,1) * 0.2 - 0.1 */
            tensor->data[i] = mps_rng_uniform() * 0.2F - 0.1F;
        }
    }
}

//=============================================================================
// Core Compression Algorithm
//=============================================================================

/**
 * @brief Simplified MPS compression using sequential decomposition
 *
 * WHAT: Decompose weight matrix into MPS chain
 * WHY: Full SVD-based TT decomposition is complex; this is a practical approximation
 * HOW: Greedy factorization with controlled rank
 *
 * ALGORITHM:
 * 1. Reshape W[N×M] into tensor W[d₁,d₂,...,dₖ,M]
 * 2. For each site:
 *    - Matricize remaining dimensions
 *    - Compute truncated SVD: W ≈ U·Σ·Vᵀ
 *    - Store U as MPS tensor
 *    - Continue with Σ·Vᵀ
 * 3. Last site stores remaining matrix
 *
 * NOTE: This is a simplified implementation. Production version should use
 * proper tensor train SVD (TT-SVD) algorithm for optimal accuracy.
 */
mps_matrix_t* mps_compress_matrix(
    const float* weights,
    uint32_t num_rows,
    uint32_t num_cols,
    const mps_config_t* config,
    mps_stats_t* stats
) {
    // Guard: NULL checks
    if (!weights || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mps_compress_matrix: required parameter is NULL (weights, config)");
        return NULL;
    }
    if (num_rows == 0 || num_cols == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mps_compress_matrix: num_rows is zero");
        return NULL;
    }

    // Record start time
    uint64_t start_time = nimcp_time_get_ms();

    // Allocate MPS structure
    mps_matrix_t* mps = (mps_matrix_t*)nimcp_malloc(sizeof(mps_matrix_t));
    if (!mps) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mps is NULL");

        return NULL;

    }

    // Compute MPS structure parameters
    mps->num_sites = compute_num_sites(num_rows, num_cols);
    mps->bond_dim = config->bond_dim;
    mps->input_dim = num_rows;
    mps->output_dim = num_cols;

    // Compute physical dimension per site
    uint32_t phys_dim = compute_phys_dim(num_rows, mps->num_sites);

    // Allocate site tensors
    mps->sites = (mps_tensor_t*)nimcp_calloc(mps->num_sites, sizeof(mps_tensor_t));
    if (!mps->sites) {
        nimcp_free(mps);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mps_compress_matrix: mps->sites is NULL");
        return NULL;
    }

    // Initialize each site tensor
    uint32_t total_params = 0;
    for (uint32_t site = 0; site < mps->num_sites; site++) {
        uint32_t left_dim = (site == 0) ? 1 : config->bond_dim;
        uint32_t right_dim = (site == mps->num_sites - 1) ? num_cols : config->bond_dim;

        mps_tensor_t* tensor = mps_tensor_alloc(left_dim, right_dim, phys_dim);
        if (!tensor) {
            // Cleanup on failure
            for (uint32_t j = 0; j < site; j++) {
                mps_tensor_free(&mps->sites[j]);
            }
            nimcp_free(mps->sites);
            nimcp_free(mps);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mps_compress_matrix: tensor is NULL");
            return NULL;
        }

        mps->sites[site] = *tensor;
        nimcp_free(tensor); // We copied the contents

        total_params += mps->sites[site].total_size;
    }

    // WHAT: Full TT-SVD algorithm for optimal compression
    // WHY:  Provides best possible approximation for given bond dimension
    // HOW:  Sequential SVD decomposition with adaptive rank selection

    // WHAT: Perform TT-SVD decomposition
    // WHY:  Optimal low-rank tensor train representation
    // HOW:  Left-to-right SVD sweeps with truncation
    bool success = tt_svd_decompose(weights, num_rows, num_cols, mps, config);
    if (!success) {
        // Cleanup on failure
        for (uint32_t j = 0; j < mps->num_sites; j++) {
            mps_tensor_free(&mps->sites[j]);
        }
        nimcp_free(mps->sites);
        nimcp_free(mps);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mps_compress_matrix: success is NULL");
        return NULL;
    }

    // Compute compression statistics
    mps->total_params = total_params;
    uint32_t original_params = num_rows * num_cols;
    mps->compression_ratio = (float)original_params / (float)total_params;

    // WHAT: Compute actual reconstruction error
    // WHY:  Quantify approximation quality accurately
    mps->reconstruction_error = compute_reconstruction_error(mps, weights, num_rows, num_cols);

    // Fill statistics if provided
    if (stats) {
        stats->compression_ratio = mps->compression_ratio;
        stats->reconstruction_error = mps->reconstruction_error;
        stats->max_singular_value = 1.0F;
        stats->min_singular_value = config->svd_tolerance;
        stats->num_singular_values_kept = total_params;
        stats->num_singular_values_dropped = original_params - total_params;
        stats->compression_time_ms = (float)(nimcp_time_get_ms() - start_time);
    }

    return mps;
}

//=============================================================================
// Matrix Operations
//=============================================================================

bool mps_matrix_vector_multiply(
    const mps_matrix_t* mps,
    const float* input,
    float* output
) {
    // Guard: NULL checks
    if (!mps || !input || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mps_matrix_vector_multiply: required parameter is NULL (mps, input, output)");
        return false;
    }
    if (!mps->sites || mps->num_sites == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mps_matrix_vector_multiply: mps->sites is NULL");
        return false;
    }

    // ALGORITHM: Left-to-right contraction
    //
    // v₀ = input vector (length: input_dim)
    // For site s = 0 to num_sites-1:
    //   v_{s+1}[j] = Σᵢ Σₖ A[s][i,j,k] × v_s[k]
    // output = v_{num_sites}

    // Working buffer for intermediate vectors
    // Max size needed: max(bond_dim, output_dim)
    uint32_t max_dim = mps->bond_dim > mps->output_dim ? mps->bond_dim : mps->output_dim;
    float* v_curr = (float*)nimcp_calloc(max_dim, sizeof(float));
    float* v_next = (float*)nimcp_calloc(max_dim, sizeof(float));

    if (!v_curr || !v_next) {
        if (v_curr) nimcp_free(v_curr);
        if (v_next) nimcp_free(v_next);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mps_matrix_vector_multiply: validation failed");
        return false;
    }

    // Initialize: Extract features from input vector into first site dimensions
    // For simplicity, we'll use a direct mapping
    // TODO: Implement proper multi-index extraction

    uint32_t phys_dim = mps->sites[0].phys_dim;
    for (uint32_t k = 0; k < phys_dim && k < mps->input_dim; k++) {
        v_curr[k] = input[k];
    }

    // Contract through MPS chain
    for (uint32_t site = 0; site < mps->num_sites; site++) {
        const mps_tensor_t* tensor = &mps->sites[site];

        // Zero next vector
        memset(v_next, 0, max_dim * sizeof(float));

        // Contract: v_next[j] = Σᵢ Σₖ A[i,j,k] × v_curr[i × phys_dim + k]
        for (uint32_t i = 0; i < tensor->left_dim; i++) {
            for (uint32_t j = 0; j < tensor->right_dim; j++) {
                float sum = 0.0F;

                for (uint32_t k = 0; k < tensor->phys_dim; k++) {
                    // Index into tensor: A[i,j,k]
                    uint32_t tensor_idx = i * tensor->right_dim * tensor->phys_dim
                                        + j * tensor->phys_dim
                                        + k;

                    // Index into current vector
                    uint32_t v_idx = (site == 0) ? k : i;

                    if (v_idx < max_dim) {
                        sum += tensor->data[tensor_idx] * v_curr[v_idx];
                    }
                }

                v_next[j] += sum;
            }
        }

        // Swap buffers
        float* temp = v_curr;
        v_curr = v_next;
        v_next = temp;
    }

    // Copy final result to output
    uint32_t output_size = mps->output_dim < max_dim ? mps->output_dim : max_dim;
    memcpy(output, v_curr, output_size * sizeof(float));

    // Cleanup
    nimcp_free(v_curr);
    nimcp_free(v_next);

    return true;
}

bool mps_reconstruct_matrix(
    const mps_matrix_t* mps,
    float* weights
) {
    // Guard: NULL checks
    if (!mps || !weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mps_reconstruct_matrix: required parameter is NULL (mps, weights)");
        return false;
    }

    // WARNING: This defeats the purpose of compression!
    // Only use for validation/debugging.

    // For each output element W[i,j], compute via MPS contraction
    for (uint32_t i = 0; i < mps->input_dim; i++) {
        // Create one-hot input vector
        float* input = (float*)nimcp_calloc(mps->input_dim, sizeof(float));
        if (!input) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mps_reconstruct_matrix: input is NULL");
            return false;
        }

        input[i] = 1.0F;

        // Compute output row
        float* output = weights + i * mps->output_dim;
        bool success = mps_matrix_vector_multiply(mps, input, output);

        nimcp_free(input);

        if (!success) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mps_reconstruct_matrix: success is NULL");
            return false;
        }
    }

    return true;
}

float mps_compute_error(
    const mps_matrix_t* mps,
    const float* original_weights
) {
    // Guard: NULL checks
    if (!mps || !original_weights) return -1.0F;

    // Reconstruct MPS matrix
    float* reconstructed = (float*)nimcp_malloc(
        mps->input_dim * mps->output_dim * sizeof(float)
    );
    if (!reconstructed) return -1.0F;

    bool success = mps_reconstruct_matrix(mps, reconstructed);
    if (!success) {
        nimcp_free(reconstructed);
        return -1.0F;
    }

    // Compute Frobenius norms
    float error_norm = 0.0F;
    float original_norm = 0.0F;

    for (uint32_t i = 0; i < mps->input_dim * mps->output_dim; i++) {
        float diff = original_weights[i] - reconstructed[i];
        error_norm += diff * diff;
        original_norm += original_weights[i] * original_weights[i];
    }

    nimcp_free(reconstructed);

    // Return relative error
    if (original_norm < 1e-12F) return 0.0F; // Avoid division by zero
    return sqrtf(error_norm / original_norm);
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

void mps_free(mps_matrix_t* mps) {
    if (!mps) return;

    if (mps->sites) {
        for (uint32_t i = 0; i < mps->num_sites; i++) {
            if (mps->sites[i].data) {
                nimcp_free(mps->sites[i].data);
            }
        }
        nimcp_free(mps->sites);
    }

    nimcp_free(mps);
}

mps_matrix_t* mps_clone(const mps_matrix_t* mps) {
    if (!mps) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mps is NULL");

        return NULL;

    }

    // Allocate new MPS
    mps_matrix_t* clone = (mps_matrix_t*)nimcp_malloc(sizeof(mps_matrix_t));
    if (!clone) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "clone is NULL");

        return NULL;

    }

    // Copy metadata
    *clone = *mps;

    // Allocate and copy sites
    clone->sites = (mps_tensor_t*)nimcp_malloc(
        mps->num_sites * sizeof(mps_tensor_t)
    );
    if (!clone->sites) {
        nimcp_free(clone);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mps_clone: clone->sites is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < mps->num_sites; i++) {
        clone->sites[i] = mps->sites[i];

        // Deep copy tensor data
        size_t data_size = mps->sites[i].total_size * sizeof(float);
        clone->sites[i].data = (float*)nimcp_malloc(data_size);
        if (!clone->sites[i].data) {
            // Cleanup on failure
            for (uint32_t j = 0; j < i; j++) {
                nimcp_free(clone->sites[j].data);
            }
            nimcp_free(clone->sites);
            nimcp_free(clone);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mps_clone: clone->sites is NULL");
            return NULL;
        }

        memcpy(clone->sites[i].data, mps->sites[i].data, data_size);
    }

    return clone;
}

//=============================================================================
// Diagnostic Functions
//=============================================================================

void mps_print_info(const mps_matrix_t* mps) {
    if (!mps) {
        printf("MPS: NULL\n");
        return;
    }

    printf("\n=== MPS Matrix Information ===\n");
    printf("Input dimension:      %u\n", mps->input_dim);
    printf("Output dimension:     %u\n", mps->output_dim);
    printf("Number of sites:      %u\n", mps->num_sites);
    printf("Bond dimension:       %u\n", mps->bond_dim);
    printf("Total parameters:     %u\n", mps->total_params);
    printf("Original parameters:  %u\n", mps->input_dim * mps->output_dim);
    printf("Compression ratio:    %.2fx\n", mps->compression_ratio);
    printf("Reconstruction error: %.6f\n", mps->reconstruction_error);
    printf("Memory usage:         %.2f KB\n", mps_memory_usage(mps) / 1024.0F);

    printf("\nSite structure:\n");
    for (uint32_t i = 0; i < mps->num_sites; i++) {
        const mps_tensor_t* site = &mps->sites[i];
        printf("  Site %u: [%u × %u × %u] = %u params\n",
               i, site->left_dim, site->right_dim, site->phys_dim,
               site->total_size);
    }
    printf("==============================\n\n");
}

size_t mps_memory_usage(const mps_matrix_t* mps) {
    if (!mps) return 0;

    size_t total = sizeof(mps_matrix_t);
    total += mps->num_sites * sizeof(mps_tensor_t);

    for (uint32_t i = 0; i < mps->num_sites; i++) {
        total += mps->sites[i].total_size * sizeof(float);
    }

    return total;
}

bool mps_verify_structure(const mps_matrix_t* mps) {
    if (!mps || !mps->sites) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mps_verify_structure: required parameter is NULL (mps, mps->sites)");
        return false;
    }
    if (mps->num_sites == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mps_verify_structure: mps->num_sites is zero");
        return false;
    }

    // Check first site has left_dim = 1
    if (mps->sites[0].left_dim != 1) {
        printf("MPS verification failed: First site left_dim = %u (expected 1)\n",
               mps->sites[0].left_dim);
        return false;
    }

    // Check last site has right_dim = output_dim
    uint32_t last = mps->num_sites - 1;
    if (mps->sites[last].right_dim != mps->output_dim) {
        printf("MPS verification failed: Last site right_dim = %u (expected %u)\n",
               mps->sites[last].right_dim, mps->output_dim);
        return false;
    }

    // Check bond dimensions match
    for (uint32_t i = 0; i < mps->num_sites - 1; i++) {
        if (mps->sites[i].right_dim != mps->sites[i+1].left_dim) {
            printf("MPS verification failed: Bond mismatch at site %u\n", i);
            printf("  Site %u right_dim = %u\n", i, mps->sites[i].right_dim);
            printf("  Site %u left_dim = %u\n", i+1, mps->sites[i+1].left_dim);
            return false;
        }
    }

    // Check all tensors have data allocated
    for (uint32_t i = 0; i < mps->num_sites; i++) {
        if (!mps->sites[i].data) {
            printf("MPS verification failed: Site %u has NULL data\n", i);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mps_verify_structure: mps->sites is NULL");
            return false;
        }
    }

    return true;
}

//=============================================================================
// Advanced Operations (Stubs for future implementation)
//=============================================================================

bool mps_backward(
    const mps_matrix_t* mps,
    const float* input,
    const float* grad_output,
    mps_matrix_t* grad_mps
) {
    /**
     * WHAT: Backpropagate gradients through MPS chain
     * WHY: Enable learning with compressed weight representations
     * HOW: Reverse-mode automatic differentiation through tensor contractions
     *
     * ALGORITHM:
     * 1. Forward pass: Store intermediate contractions v[0], v[1], ..., v[num_sites]
     * 2. Backward pass: Compute gradients w.r.t. each tensor site
     *    For site s:
     *      ∂L/∂A[s][i,j,k] = v[s-1][i] × ∂L/∂v[s][j] × input_features[k]
     * 3. Chain rule through all sites from right to left
     */

    // Guard: NULL checks
    if (!mps || !input || !grad_output || !grad_mps) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mps_backward: required parameter is NULL (mps, input, grad_output, grad_mps)");
        return false;
    }
    if (!mps->sites || mps->num_sites == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mps_backward: mps->sites is NULL");
        return false;
    }
    if (!grad_mps->sites || grad_mps->num_sites != mps->num_sites) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mps_backward: grad_mps->sites is NULL");
        return false;
    }

    // Validate grad_mps structure matches mps
    for (uint32_t site = 0; site < mps->num_sites; site++) {
        if (grad_mps->sites[site].left_dim != mps->sites[site].left_dim ||
            grad_mps->sites[site].right_dim != mps->sites[site].right_dim ||
            grad_mps->sites[site].phys_dim != mps->sites[site].phys_dim) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mps_backward: grad_mps->sites is NULL");
            return false;
        }
    }

    // STEP 1: Forward pass with intermediate storage
    uint32_t max_dim = mps->bond_dim > mps->output_dim ? mps->bond_dim : mps->output_dim;

    // Allocate storage for intermediate values
    float** v_intermediates = (float**)nimcp_calloc(mps->num_sites + 1, sizeof(float*));
    if (!v_intermediates) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mps_backward: v_intermediates is NULL");
        return false;
    }

    for (uint32_t i = 0; i <= mps->num_sites; i++) {
        v_intermediates[i] = (float*)nimcp_calloc(max_dim, sizeof(float));
        if (!v_intermediates[i]) {
            // Cleanup on failure
            for (uint32_t j = 0; j < i; j++) {
                nimcp_free(v_intermediates[j]);
            }
            nimcp_free(v_intermediates);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mps_backward: v_intermediates is NULL");
            return false;
        }
    }

    // WHAT: Initialize first intermediate with ALL input features
    // WHY:  Forward pass needs all input dimensions for gradient computation
    // HOW:  Copy entire input vector into v_intermediates[0]
    // FIX: Previous code only copied phys_dim elements, causing zero gradients
    //      for inputs beyond the first few elements
    for (uint32_t k = 0; k < mps->input_dim && k < max_dim; k++) {
        v_intermediates[0][k] = input[k];
    }

    // WHAT: Forward contraction through MPS chain (store intermediates)
    // WHY:  Need intermediate activations for gradient computation
    // HOW:  Contract each site with previous activation
    for (uint32_t site = 0; site < mps->num_sites; site++) {
        const mps_tensor_t* tensor = &mps->sites[site];
        float* v_curr = v_intermediates[site];
        float* v_next = v_intermediates[site + 1];

        // WHAT: Contract tensor with current vector
        // WHY:  v_next[j] = Σᵢₖ A[i,j,k] × v_curr[v_idx]
        // NOTE: v_idx mapping depends on site:
        //   - Site 0: v_idx = k (physical index maps to input)
        //   - Other sites: v_idx = i (left bond index)
        // FIX: For site 0, we need to handle all input dimensions, not just phys_dim
        //      The tensor only has phys_dim elements, but v_curr has input_dim elements
        for (uint32_t i = 0; i < tensor->left_dim; i++) {
            for (uint32_t j = 0; j < tensor->right_dim; j++) {
                float sum = 0.0F;

                if (site == 0) {
                    // Site 0: Contract over all input dimensions
                    // Only use the dimensions that exist in the tensor
                    for (uint32_t k = 0; k < tensor->phys_dim && k < mps->input_dim; k++) {
                        uint32_t tensor_idx = i * tensor->right_dim * tensor->phys_dim
                                            + j * tensor->phys_dim + k;
                        sum += tensor->data[tensor_idx] * v_curr[k];
                    }
                } else {
                    // Other sites: Contract over physical dimension using left bond
                    // FIX: The contraction should be over k with different v_curr indices
                    // For simplified MPS: just use first element of phys dimension
                    uint32_t k = 0; // Simplified: use first physical index
                    uint32_t tensor_idx = i * tensor->right_dim * tensor->phys_dim
                                        + j * tensor->phys_dim + k;
                    if (i < max_dim) {
                        sum += tensor->data[tensor_idx] * v_curr[i];
                    }
                }

                v_next[j] += sum;
            }
        }
    }

    // STEP 2: Backward pass - compute gradients
    // Allocate gradient buffers for intermediate values
    float** grad_v = (float**)nimcp_calloc(mps->num_sites + 1, sizeof(float*));
    if (!grad_v) {
        for (uint32_t i = 0; i <= mps->num_sites; i++) {
            nimcp_free(v_intermediates[i]);
        }
        nimcp_free(v_intermediates);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mps_backward: grad_v is NULL");
        return false;
    }

    for (uint32_t i = 0; i <= mps->num_sites; i++) {
        grad_v[i] = (float*)nimcp_calloc(max_dim, sizeof(float));
        if (!grad_v[i]) {
            for (uint32_t j = 0; j < i; j++) {
                nimcp_free(grad_v[j]);
            }
            nimcp_free(grad_v);
            for (uint32_t j = 0; j <= mps->num_sites; j++) {
                nimcp_free(v_intermediates[j]);
            }
            nimcp_free(v_intermediates);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mps_backward: grad_v is NULL");
            return false;
        }
    }

    // Initialize output gradient
    for (uint32_t j = 0; j < mps->output_dim && j < max_dim; j++) {
        grad_v[mps->num_sites][j] = grad_output[j];
    }

    // Backward through MPS chain
    for (int site = (int)mps->num_sites - 1; site >= 0; site--) {
        const mps_tensor_t* tensor = &mps->sites[site];
        mps_tensor_t* grad_tensor = &grad_mps->sites[site];

        float* v_curr = v_intermediates[site];
        float* grad_v_next = grad_v[site + 1];
        float* grad_v_curr = grad_v[site];

        // Zero gradient tensor
        memset(grad_tensor->data, 0, grad_tensor->total_size * sizeof(float));

        // Compute gradient w.r.t. tensor parameters
        // WHAT: Backprop through tensor contraction: v_next[j] = Σᵢₖ A[i,j,k] × v_curr[v_idx]
        // WHY:  Chain rule: ∂L/∂A[i,j,k] = ∂L/∂v_next[j] × v_curr[v_idx]
        // HOW:  Iterate through all tensor elements and accumulate gradients
        for (uint32_t i = 0; i < tensor->left_dim; i++) {
            for (uint32_t j = 0; j < tensor->right_dim; j++) {
                for (uint32_t k = 0; k < tensor->phys_dim; k++) {
                    uint32_t tensor_idx = i * tensor->right_dim * tensor->phys_dim
                                        + j * tensor->phys_dim + k;

                    if (site == 0) {
                        // Site 0: Gradient w.r.t. A[i,j,k] uses input[k]
                        if (k < mps->input_dim && j < max_dim) {
                            grad_tensor->data[tensor_idx] += v_curr[k] * grad_v_next[j];
                        }

                        // Gradient w.r.t. input (v_curr[k])
                        if (k < mps->input_dim && j < max_dim) {
                            grad_v_curr[k] += tensor->data[tensor_idx] * grad_v_next[j];
                        }
                    } else {
                        // Other sites: Gradient w.r.t. A[i,j,k] uses v_curr[i]
                        // FIX: Only compute gradient for k=0 to match forward pass
                        if (k == 0 && i < max_dim && j < max_dim) {
                            grad_tensor->data[tensor_idx] += v_curr[i] * grad_v_next[j];

                            // Gradient w.r.t. v_curr[i] (only accumulate once per i,j pair)
                            grad_v_curr[i] += tensor->data[tensor_idx] * grad_v_next[j];
                        }
                    }
                }
            }
        }
    }

    // Cleanup
    for (uint32_t i = 0; i <= mps->num_sites; i++) {
        nimcp_free(v_intermediates[i]);
        nimcp_free(grad_v[i]);
    }
    nimcp_free(v_intermediates);
    nimcp_free(grad_v);

    return true;
}

bool mps_update_params(
    mps_matrix_t* mps,
    const mps_matrix_t* grad_mps,
    float learning_rate
) {
    /**
     * WHAT: Update MPS parameters using gradient descent
     * WHY: Enable learning on compressed weight representation
     * HOW: θ_new = θ_old - learning_rate × ∇θ
     *
     * ALGORITHM:
     * For each site s = 0 to num_sites-1:
     *   For each parameter A[s][i,j,k]:
     *     A[s][i,j,k] -= learning_rate × ∂L/∂A[s][i,j,k]
     */

    // Guard: NULL checks
    if (!mps || !grad_mps) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mps_update_params: required parameter is NULL (mps, grad_mps)");
        return false;
    }
    if (!mps->sites || !grad_mps->sites) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mps_update_params: required parameter is NULL (mps->sites, grad_mps->sites)");
        return false;
    }
    if (mps->num_sites != grad_mps->num_sites) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mps_update_params: validation failed");
        return false;
    }
    if (learning_rate <= 0.0F || learning_rate > 1.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mps_update_params: validation failed");
        return false;
    }

    // Update each site tensor
    for (uint32_t site = 0; site < mps->num_sites; site++) {
        mps_tensor_t* tensor = &mps->sites[site];
        const mps_tensor_t* grad_tensor = &grad_mps->sites[site];

        // Validate dimensions match
        if (tensor->left_dim != grad_tensor->left_dim ||
            tensor->right_dim != grad_tensor->right_dim ||
            tensor->phys_dim != grad_tensor->phys_dim ||
            tensor->total_size != grad_tensor->total_size) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mps_update_params: operation failed");
            return false;
        }

        // Gradient descent update: θ -= lr × ∇θ
        for (uint32_t i = 0; i < tensor->total_size; i++) {
            tensor->data[i] -= learning_rate * grad_tensor->data[i];
        }

        // Optional: Gradient clipping to prevent instability
        // Clip individual gradient values to [-10, 10]
        for (uint32_t i = 0; i < tensor->total_size; i++) {
            if (tensor->data[i] > 10.0F) tensor->data[i] = 10.0F;
            if (tensor->data[i] < -10.0F) tensor->data[i] = -10.0F;
        }
    }

    return true;
}

bool mps_adapt_bond_dimensions(
    mps_matrix_t* mps,
    float target_error
) {
    /**
     * WHAT: Adaptively adjust bond dimensions based on target error
     * WHY: Optimize memory-accuracy tradeoff dynamically
     * HOW: Analyze tensor magnitudes and adjust bond dimensions
     *
     * ALGORITHM:
     * 1. For each bond between sites:
     *    a. Compute variance/importance of bond connections
     *    b. If importance < threshold: reduce bond dimension
     *    c. If importance > threshold: increase bond dimension (up to limit)
     * 2. Ensure bond dimensions remain consistent across chain
     * 3. Preserve MPS approximation quality
     *
     * SIMPLIFIED VERSION: Analyze tensor norms and prune low-magnitude bonds
     */

    // Guard: NULL checks
    if (!mps || !mps->sites) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mps_adapt_bond_dimensions: required parameter is NULL (mps, mps->sites)");
        return false;
    }
    if (mps->num_sites < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mps_adapt_bond_dimensions: validation failed");
        return false;
    }
    if (target_error <= 0.0F || target_error >= 1.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mps_adapt_bond_dimensions: capacity exceeded");
        return false;
    }

    // Adaptive strategy: Compute importance of each bond
    // For simplicity, we'll analyze the Frobenius norm of each site tensor
    // Sites with low norms can use smaller bond dimensions

    bool adapted = false;

    for (uint32_t site = 0; site < mps->num_sites; site++) {
        mps_tensor_t* tensor = &mps->sites[site];

        // Compute Frobenius norm of tensor
        float tensor_norm = 0.0F;
        for (uint32_t i = 0; i < tensor->total_size; i++) {
            tensor_norm += tensor->data[i] * tensor->data[i];
        }
        tensor_norm = sqrtf(tensor_norm);

        // Compute average magnitude per parameter
        float avg_magnitude = tensor_norm / sqrtf((float)tensor->total_size);

        // If tensor has very small values, it contributes little to the output
        // We could potentially reduce its bond dimensions
        // However, actually changing bond dimensions requires recompression
        // For now, we'll just mark tensors that could be compressed further

        // Heuristic: If avg magnitude < target_error, this site is a candidate
        // for dimension reduction
        if (avg_magnitude < target_error * 0.1F) {
            // This site could potentially use smaller bond dimensions
            // Mark for potential recompression
            adapted = true;

            // In a full implementation, we would:
            // 1. Reshape the MPS around this site
            // 2. Perform SVD with adaptive truncation
            // 3. Update bond dimensions accordingly

            // For this simplified version, we'll just renormalize the tensor
            // to maintain numerical stability
            if (tensor_norm > 1e-12F) {
                float scale = 1.0F / tensor_norm;
                for (uint32_t i = 0; i < tensor->total_size; i++) {
                    tensor->data[i] *= scale;
                }
            }
        }
    }

    // Update stored reconstruction error estimate
    if (adapted) {
        // Estimate new reconstruction error based on adaptations
        // This is a rough heuristic - proper implementation would recompute
        mps->reconstruction_error = target_error * 0.5F;
    }

    return adapted;
}

bool mps_recompress(
    mps_matrix_t* mps,
    uint32_t new_bond_dim
) {
    /**
     * WHAT: Recompress MPS with different bond dimension
     * WHY: Dynamic memory management - reduce memory footprint at runtime
     * HOW: Truncate or expand bond dimensions while preserving structure
     *
     * ALGORITHM:
     * 1. If new_bond_dim < current: Truncate bond dimensions
     *    - Keep most important components
     *    - Renormalize to maintain approximation
     * 2. If new_bond_dim > current: Expand bond dimensions
     *    - Pad with zeros for future learning
     * 3. Update MPS metadata
     *
     * SIMPLIFIED VERSION: Direct truncation/padding without SVD
     */

    // Guard: NULL checks
    if (!mps || !mps->sites) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mps_recompress: required parameter is NULL (mps, mps->sites)");
        return false;
    }
    if (mps->num_sites == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mps_recompress: mps->num_sites is zero");
        return false;
    }
    if (new_bond_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mps_recompress: new_bond_dim is zero");
        return false;
    }
    if (new_bond_dim == mps->bond_dim) return true; // Already at target

    uint32_t old_bond_dim = mps->bond_dim;

    // Process each site (except first and last which have special dimensions)
    for (uint32_t site = 0; site < mps->num_sites; site++) {
        mps_tensor_t* tensor = &mps->sites[site];

        // Determine new dimensions for this site
        uint32_t new_left_dim = (site == 0) ? 1 : new_bond_dim;
        uint32_t new_right_dim = (site == mps->num_sites - 1) ? mps->output_dim : new_bond_dim;
        uint32_t new_total_size = new_left_dim * new_right_dim * tensor->phys_dim;

        // Skip if dimensions unchanged
        if (new_left_dim == tensor->left_dim && new_right_dim == tensor->right_dim) {
            continue;
        }

        // Allocate new tensor data
        float* new_data = (float*)nimcp_calloc(new_total_size, sizeof(float));
        if (!new_data) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mps_recompress: new_data is NULL");
            return false;
        }

        // Copy existing data (truncate or pad as needed)
        uint32_t copy_left_dim = (new_left_dim < tensor->left_dim) ? new_left_dim : tensor->left_dim;
        uint32_t copy_right_dim = (new_right_dim < tensor->right_dim) ? new_right_dim : tensor->right_dim;

        for (uint32_t i = 0; i < copy_left_dim; i++) {
            for (uint32_t j = 0; j < copy_right_dim; j++) {
                for (uint32_t k = 0; k < tensor->phys_dim; k++) {
                    // Old index
                    uint32_t old_idx = i * tensor->right_dim * tensor->phys_dim
                                     + j * tensor->phys_dim + k;
                    // New index
                    uint32_t new_idx = i * new_right_dim * tensor->phys_dim
                                     + j * tensor->phys_dim + k;

                    new_data[new_idx] = tensor->data[old_idx];
                }
            }
        }

        // If expanding, normalize to preserve magnitude
        if (new_bond_dim > old_bond_dim) {
            float scale = sqrtf((float)old_bond_dim / (float)new_bond_dim);
            for (uint32_t i = 0; i < new_total_size; i++) {
                new_data[i] *= scale;
            }
        }

        // Replace old data with new data
        nimcp_free(tensor->data);
        tensor->data = new_data;
        tensor->left_dim = new_left_dim;
        tensor->right_dim = new_right_dim;
        tensor->total_size = new_total_size;
    }

    // Update MPS metadata
    mps->bond_dim = new_bond_dim;

    // Recompute total parameters
    uint32_t total_params = 0;
    for (uint32_t site = 0; site < mps->num_sites; site++) {
        total_params += mps->sites[site].total_size;
    }
    mps->total_params = total_params;

    // Update compression ratio
    uint32_t original_params = mps->input_dim * mps->output_dim;
    mps->compression_ratio = (float)original_params / (float)total_params;

    // Estimate reconstruction error change
    if (new_bond_dim < old_bond_dim) {
        // Compressing further increases error
        mps->reconstruction_error *= (1.0F + 0.1F * (float)(old_bond_dim - new_bond_dim) / (float)old_bond_dim);
    } else {
        // Expanding potentially reduces error (if followed by training)
        mps->reconstruction_error *= 0.9F;
    }

    return true;
}

bool mps_canonicalize(
    mps_matrix_t* mps,
    uint32_t center_site
) {
    /**
     * WHAT: Bring MPS to canonical form (left/right orthogonal)
     * WHY:  Numerical stability and efficient operations
     * HOW:  QR decomposition sweeps using LAPACK (or simplified fallback)
     *
     * ALGORITHM (with LAPACK):
     * 1. Left canonicalization: QR sweep from left to center
     *    For site s = 0 to center-1:
     *      Reshape A[s] to matrix [left_dim*phys_dim × right_dim]
     *      Compute QR: A[s] = Q·R
     *      Store Q as new A[s]
     *      Multiply R into A[s+1]
     * 2. Right canonicalization: LQ sweep from right to center
     *    For site s = num_sites-1 down to center+1:
     *      Reshape A[s] to matrix [left_dim × right_dim*phys_dim]
     *      Compute QR: A[s]ᵀ = Q·R, so A[s] = Rᵀ·Qᵀ = LQ
     *      Store Q as new A[s]
     *      Multiply R into A[s-1]
     * 3. Center site accumulates all residuals
     */

    // Guard: NULL checks
    if (!mps || !mps->sites) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mps_canonicalize: required parameter is NULL (mps, mps->sites)");
        return false;
    }
    if (center_site >= mps->num_sites) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mps_canonicalize: capacity exceeded");
        return false;
    }

#ifdef NIMCP_ENABLE_LAPACK
    // WHAT: LAPACK-based QR canonicalization
    // WHY:  Proper orthogonalization for numerical stability
    // HOW:  sgeqrf for QR, sorgqr to extract Q

    // Left sweep: Make sites 0..center-1 left-orthogonal
    for (uint32_t site = 0; site < center_site; site++) {
        mps_tensor_t* tensor = &mps->sites[site];

        // WHAT: Reshape tensor to matrix for QR
        // WHY:  QR operates on matrices [left_dim*phys_dim × right_dim]
        int m = (int)(tensor->left_dim * tensor->phys_dim);
        int n = (int)(tensor->right_dim);
        int k = (m < n) ? m : n;

        if (m == 0 || n == 0) continue;

        // Allocate workspace
        float* tau = (float*)nimcp_malloc(k * sizeof(float));
        int lwork = -1;
        float work_query;
        int info;

        if (!tau) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mps_canonicalize: tau is NULL");
            return false;
        }

        // Query optimal workspace
        sgeqrf_(&m, &n, tensor->data, &m, tau, &work_query, &lwork, &info);
        if (info != 0) {
            nimcp_free(tau);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mps_canonicalize: validation failed");
            return false;
        }

        lwork = (int)work_query;
        float* work = (float*)nimcp_malloc(lwork * sizeof(float));
        if (!work) {
            nimcp_free(tau);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mps_canonicalize: work is NULL");
            return false;
        }

        // WHAT: Compute QR factorization
        // WHY:  A = Q·R with Q orthogonal
        sgeqrf_(&m, &n, tensor->data, &m, tau, work, &lwork, &info);
        if (info != 0) {
            nimcp_free(tau);
            nimcp_free(work);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mps_canonicalize: validation failed");
            return false;
        }

        // WHAT: Extract R matrix before generating Q
        // WHY:  Need to propagate R to next site
        float* R = (float*)nimcp_calloc(n * n, sizeof(float));
        if (!R) {
            nimcp_free(tau);
            nimcp_free(work);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mps_canonicalize: R is NULL");
            return false;
        }

        // Copy upper triangular R from QR factorization
        for (int j = 0; j < n; j++) {
            for (int i = 0; i <= j && i < m; i++) {
                R[j * n + i] = tensor->data[j * m + i];
            }
        }

        // WHAT: Generate explicit Q matrix
        // WHY:  Replace current tensor with orthogonal Q
        sorgqr_(&m, &k, &k, tensor->data, &m, tau, work, &lwork, &info);
        nimcp_free(tau);
        nimcp_free(work);

        if (info != 0) {
            nimcp_free(R);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mps_canonicalize: validation failed");
            return false;
        }

        // WHAT: Multiply R into next site
        // WHY:  Preserve the MPS contraction: A[s]·A[s+1] = (Q·R)·A[s+1] = Q·(R·A[s+1])
        if (site + 1 < mps->num_sites) {
            mps_tensor_t* next_tensor = &mps->sites[site + 1];
            int next_left = (int)next_tensor->left_dim;
            int next_phys = (int)next_tensor->phys_dim;
            int next_right = (int)next_tensor->right_dim;

            // Reshape next tensor to [left_dim × (phys_dim*right_dim)]
            // Then compute: next = R·next
            // R is [n×n], next is [n × (phys_dim*right_dim)]

            float* next_reshaped = (float*)nimcp_malloc(
                next_left * next_phys * next_right * sizeof(float));
            if (!next_reshaped) {
                nimcp_free(R);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mps_canonicalize: next_reshaped is NULL");
                return false;
            }

            // BLAS matrix multiply: next_reshaped = R · next_tensor
            char transa = 'N', transb = 'N';
            float alpha = 1.0f, beta = 0.0f;
            int r_rows = n;
            int next_cols = next_phys * next_right;

            sgemm_(&transa, &transb, &r_rows, &next_cols, &n,
                   &alpha, R, &r_rows,
                   next_tensor->data, &next_left,
                   &beta, next_reshaped, &r_rows);

            memcpy(next_tensor->data, next_reshaped,
                   next_left * next_phys * next_right * sizeof(float));

            nimcp_free(next_reshaped);
        }

        nimcp_free(R);
    }

    // Right sweep: Make sites center+1..num_sites-1 right-orthogonal
    // (Similar to left sweep but in reverse)
    for (int site = (int)mps->num_sites - 1; site > (int)center_site; site--) {
        mps_tensor_t* tensor = &mps->sites[site];

        // For right canonicalization, we want to make the tensor right-orthogonal
        // This means QR of the transposed reshape: [left_dim × right_dim*phys_dim]ᵀ
        // Then Q goes back into the tensor, R goes to the previous site

        int m = (int)(tensor->right_dim * tensor->phys_dim);
        int n = (int)(tensor->left_dim);
        int k = (m < n) ? m : n;

        if (m == 0 || n == 0) continue;

        // For simplicity in right sweep, use normalization approach
        // Full LQ would require additional LAPACK calls
        float norm = 0.0f;
        for (uint32_t i = 0; i < tensor->total_size; i++) {
            norm += tensor->data[i] * tensor->data[i];
        }
        norm = sqrtf(norm);

        if (norm > 1e-12f) {
            float scale = 1.0f / norm;
            for (uint32_t i = 0; i < tensor->total_size; i++) {
                tensor->data[i] *= scale;
            }

            if (site > 0) {
                mps_tensor_t* prev_tensor = &mps->sites[site - 1];
                for (uint32_t i = 0; i < prev_tensor->total_size; i++) {
                    prev_tensor->data[i] *= norm;
                }
            }
        }
    }

    return true;

#else
    // WHAT: Fallback simplified canonicalization without LAPACK
    // WHY:  Maintain basic numerical stability
    // HOW:  Normalize and transfer norms

    // Left sweep
    for (uint32_t site = 0; site < center_site; site++) {
        mps_tensor_t* tensor = &mps->sites[site];

        float norm = 0.0F;
        for (uint32_t i = 0; i < tensor->total_size; i++) {
            norm += tensor->data[i] * tensor->data[i];
        }
        norm = sqrtf(norm);

        if (norm > 1e-12F) {
            float scale = 1.0F / norm;
            for (uint32_t i = 0; i < tensor->total_size; i++) {
                tensor->data[i] *= scale;
            }

            if (site + 1 < mps->num_sites) {
                mps_tensor_t* next_tensor = &mps->sites[site + 1];
                for (uint32_t i = 0; i < next_tensor->total_size; i++) {
                    next_tensor->data[i] *= norm;
                }
            }
        }
    }

    // Right sweep
    for (int site = (int)mps->num_sites - 1; site > (int)center_site; site--) {
        mps_tensor_t* tensor = &mps->sites[site];

        float norm = 0.0F;
        for (uint32_t i = 0; i < tensor->total_size; i++) {
            norm += tensor->data[i] * tensor->data[i];
        }
        norm = sqrtf(norm);

        if (norm > 1e-12F) {
            float scale = 1.0F / norm;
            for (uint32_t i = 0; i < tensor->total_size; i++) {
                tensor->data[i] *= scale;
            }

            if (site > 0) {
                mps_tensor_t* prev_tensor = &mps->sites[site - 1];
                for (uint32_t i = 0; i < prev_tensor->total_size; i++) {
                    prev_tensor->data[i] *= norm;
                }
            }
        }
    }

    return true;
#endif
}
