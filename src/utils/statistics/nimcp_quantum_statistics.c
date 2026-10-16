//=============================================================================
// nimcp_quantum_statistics.c - Quantum Statistics Implementation
//=============================================================================
/**
 * @file nimcp_quantum_statistics.c
 * @brief Implementation of quantum-enhanced statistics and probability functions
 *
 * @author NIMCP Development Team
 * @date 2026-01-30
 */

#include "utils/statistics/nimcp_quantum_statistics.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_math_constants.h"

//=============================================================================
// Internal Constants
//=============================================================================

#define PI NIMCP_PI_F
#define LOG2E 1.4426950408889634f    // log2(e)
#define LN2 0.6931471805599453f      // ln(2)

//=============================================================================
// Thread-local RNG
//=============================================================================

static __thread uint32_t qstats_rng_seed = 0;

static uint32_t qstats_rand(uint32_t* seed) {
    *seed = *seed * 1103515245 + 12345;
    return (*seed >> 16) & 0x7FFF;
}

static float qstats_rand_float(uint32_t* seed) {
    return (float)qstats_rand(seed) / 32768.0f;
}

//=============================================================================
// Configuration
//=============================================================================

qstats_config_t qstats_default_config(void) {
    return (qstats_config_t){
        .mc_samples = QSTATS_DEFAULT_MC_SAMPLES,
        .tolerance = QSTATS_EPSILON,
        .use_gpu = false,
        .cache_eigenvalues = true,
        .seed = 0
    };
}

//=============================================================================
// Complex Number Operations
//=============================================================================

qstats_complex_t qstats_complex_add(qstats_complex_t a, qstats_complex_t b) {
    return (qstats_complex_t){a.real + b.real, a.imag + b.imag};
}

qstats_complex_t qstats_complex_sub(qstats_complex_t a, qstats_complex_t b) {
    return (qstats_complex_t){a.real - b.real, a.imag - b.imag};
}

qstats_complex_t qstats_complex_mul(qstats_complex_t a, qstats_complex_t b) {
    return (qstats_complex_t){
        a.real * b.real - a.imag * b.imag,
        a.real * b.imag + a.imag * b.real
    };
}

qstats_complex_t qstats_complex_conj(qstats_complex_t a) {
    return (qstats_complex_t){a.real, -a.imag};
}

float qstats_complex_abs(qstats_complex_t a) {
    return sqrtf(a.real * a.real + a.imag * a.imag);
}

float qstats_complex_abs_squared(qstats_complex_t a) {
    return a.real * a.real + a.imag * a.imag;
}

//=============================================================================
// State Creation and Management
//=============================================================================

qstats_pure_state_t* qstats_pure_state_create(uint32_t dim) {
    if (dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "qstats_pure_state_create: dim is zero");
        return NULL;
    }

    qstats_pure_state_t* state = nimcp_malloc(sizeof(qstats_pure_state_t));
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "qstats_pure_state_create: state is NULL");
        return NULL;
    }

    state->amplitudes = nimcp_calloc(dim, sizeof(qstats_complex_t));
    if (!state->amplitudes) {
        nimcp_free(state);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "qstats_pure_state_create: state->amplitudes is NULL");
        return NULL;
    }

    state->dim = dim;
    state->normalized = false;

    // Initialize to |0> state
    state->amplitudes[0].real = 1.0f;
    state->amplitudes[0].imag = 0.0f;
    state->normalized = true;

    return state;
}

void qstats_pure_state_destroy(qstats_pure_state_t* state) {
    if (state) {
        nimcp_free(state->amplitudes);
        nimcp_free(state);
    }
}

qstats_density_matrix_t* qstats_density_matrix_create(uint32_t dim) {
    if (dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "qstats_density_matrix_create: dim is zero");
        return NULL;
    }

    qstats_density_matrix_t* dm = nimcp_malloc(sizeof(qstats_density_matrix_t));
    if (!dm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "qstats_density_matrix_create: dm is NULL");
        return NULL;
    }

    dm->elements = nimcp_calloc(dim * dim, sizeof(qstats_complex_t));
    if (!dm->elements) {
        nimcp_free(dm);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "qstats_density_matrix_create: dm->elements is NULL");
        return NULL;
    }

    dm->dim = dim;
    dm->eigenvalues = NULL;
    dm->eigenvalues_valid = false;

    // Initialize to |0><0|
    dm->elements[0].real = 1.0f;

    return dm;
}

void qstats_density_matrix_destroy(qstats_density_matrix_t* dm) {
    if (dm) {
        nimcp_free(dm->elements);
        nimcp_free(dm->eigenvalues);
        nimcp_free(dm);
    }
}

qstats_density_matrix_t* qstats_density_matrix_from_pure(const qstats_pure_state_t* state) {
    if (!state || !state->amplitudes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qstats_density_matrix_from_pure: required parameter is NULL (state, state->amplitudes)");
        return NULL;
    }

    qstats_density_matrix_t* dm = qstats_density_matrix_create(state->dim);
    if (!dm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "qstats_density_matrix_from_pure: dm is NULL");
        return NULL;
    }

    // ρ = |ψ><ψ|: ρᵢⱼ = αᵢ αⱼ*
    for (uint32_t i = 0; i < state->dim; i++) {
        for (uint32_t j = 0; j < state->dim; j++) {
            qstats_complex_t alpha_i = state->amplitudes[i];
            qstats_complex_t alpha_j_conj = qstats_complex_conj(state->amplitudes[j]);
            dm->elements[i * state->dim + j] = qstats_complex_mul(alpha_i, alpha_j_conj);
        }
    }

    return dm;
}

qstats_density_matrix_t* qstats_density_matrix_from_ensemble(
    const qstats_pure_state_t** states,
    const float* probabilities,
    uint32_t num_states
) {
    if (!states || !probabilities || num_states == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qstats_density_matrix_from_ensemble: required parameter is NULL (states, probabilities)");
        return NULL;
    }
    if (!states[0]) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qstats_density_matrix_from_ensemble: states is NULL");
        return NULL;
    }

    uint32_t dim = states[0]->dim;
    qstats_density_matrix_t* dm = qstats_density_matrix_create(dim);
    if (!dm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "qstats_density_matrix_from_ensemble: dm is NULL");
        return NULL;
    }

    // Zero out
    memset(dm->elements, 0, dim * dim * sizeof(qstats_complex_t));

    // ρ = Σᵢ pᵢ |ψᵢ><ψᵢ|
    for (uint32_t k = 0; k < num_states; k++) {
        if (!states[k] || states[k]->dim != dim) continue;

        float p = probabilities[k];
        for (uint32_t i = 0; i < dim; i++) {
            for (uint32_t j = 0; j < dim; j++) {
                qstats_complex_t alpha_i = states[k]->amplitudes[i];
                qstats_complex_t alpha_j_conj = qstats_complex_conj(states[k]->amplitudes[j]);
                qstats_complex_t contrib = qstats_complex_mul(alpha_i, alpha_j_conj);
                dm->elements[i * dim + j].real += p * contrib.real;
                dm->elements[i * dim + j].imag += p * contrib.imag;
            }
        }
    }

    return dm;
}

qstats_density_matrix_t* qstats_density_matrix_maximally_mixed(uint32_t dim) {
    if (dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "qstats_density_matrix_maximally_mixed: dim is zero");
        return NULL;
    }

    qstats_density_matrix_t* dm = qstats_density_matrix_create(dim);
    if (!dm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "qstats_density_matrix_maximally_mixed: dm is NULL");
        return NULL;
    }

    // ρ = I/d
    memset(dm->elements, 0, dim * dim * sizeof(qstats_complex_t));
    float diag_val = 1.0f / (float)dim;
    for (uint32_t i = 0; i < dim; i++) {
        dm->elements[i * dim + i].real = diag_val;
    }

    return dm;
}

qstats_density_matrix_t* qstats_density_matrix_thermal(
    const float* hamiltonian,
    uint32_t dim,
    float beta
) {
    if (!hamiltonian || dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "qstats_density_matrix_thermal: hamiltonian is NULL");
        return NULL;
    }

    qstats_density_matrix_t* dm = qstats_density_matrix_create(dim);
    if (!dm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "qstats_density_matrix_thermal: dm is NULL");
        return NULL;
    }

    // For diagonal Hamiltonian (simplified): ρᵢᵢ = exp(-β Hᵢᵢ) / Z
    float Z = 0.0f;
    float* boltzmann = nimcp_malloc(dim * sizeof(float));
    if (!boltzmann) {
        qstats_density_matrix_destroy(dm);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "qstats_density_matrix_thermal: boltzmann is NULL");
        return NULL;
    }

    // Compute Boltzmann factors (diagonal case)
    for (uint32_t i = 0; i < dim; i++) {
        boltzmann[i] = expf(-beta * hamiltonian[i * dim + i]);
        Z += boltzmann[i];
    }

    // Normalize
    memset(dm->elements, 0, dim * dim * sizeof(qstats_complex_t));
    for (uint32_t i = 0; i < dim; i++) {
        dm->elements[i * dim + i].real = boltzmann[i] / Z;
    }

    nimcp_free(boltzmann);
    return dm;
}

qstats_result_t qstats_pure_state_normalize(qstats_pure_state_t* state) {
    if (!state || !state->amplitudes) return QSTATS_ERROR_NULL;

    // Compute norm
    float norm_sq = 0.0f;
    for (uint32_t i = 0; i < state->dim; i++) {
        norm_sq += qstats_complex_abs_squared(state->amplitudes[i]);
    }

    if (norm_sq < QSTATS_EPSILON) return QSTATS_ERROR_INVALID;

    float inv_norm = 1.0f / sqrtf(norm_sq);
    for (uint32_t i = 0; i < state->dim; i++) {
        state->amplitudes[i].real *= inv_norm;
        state->amplitudes[i].imag *= inv_norm;
    }

    state->normalized = true;
    return QSTATS_OK;
}

bool qstats_density_matrix_is_valid(const qstats_density_matrix_t* dm) {
    if (!dm || !dm->elements) {
        return false;
    }

    // Check trace = 1
    float trace = qstats_trace(dm);
    if (fabsf(trace - 1.0f) > 1e-4f) {
        return false;
    }

    // Check Hermiticity: ρᵢⱼ = ρⱼᵢ*
    for (uint32_t i = 0; i < dm->dim; i++) {
        for (uint32_t j = i + 1; j < dm->dim; j++) {
            qstats_complex_t rho_ij = dm->elements[i * dm->dim + j];
            qstats_complex_t rho_ji = dm->elements[j * dm->dim + i];
            if (fabsf(rho_ij.real - rho_ji.real) > 1e-6f ||
                fabsf(rho_ij.imag + rho_ji.imag) > 1e-6f) {
                return false;
            }
        }
    }

    return true;
}

//=============================================================================
// Quantum Probability Distributions
//=============================================================================

qstats_result_t qstats_born_probabilities(
    const qstats_pure_state_t* state,
    float* probabilities
) {
    if (!state || !state->amplitudes || !probabilities) return QSTATS_ERROR_NULL;

    for (uint32_t i = 0; i < state->dim; i++) {
        probabilities[i] = qstats_complex_abs_squared(state->amplitudes[i]);
    }

    return QSTATS_OK;
}

qstats_result_t qstats_diagonal_probabilities(
    const qstats_density_matrix_t* dm,
    float* probabilities
) {
    if (!dm || !dm->elements || !probabilities) return QSTATS_ERROR_NULL;

    for (uint32_t i = 0; i < dm->dim; i++) {
        probabilities[i] = dm->elements[i * dm->dim + i].real;
    }

    return QSTATS_OK;
}

qstats_pure_state_t* qstats_amplitude_encode(
    const float* probabilities,
    uint32_t dim
) {
    if (!probabilities || dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "qstats_amplitude_encode: probabilities is NULL");
        return NULL;
    }

    qstats_pure_state_t* state = qstats_pure_state_create(dim);
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "qstats_amplitude_encode: state is NULL");
        return NULL;
    }

    // |ψ⟩ = Σᵢ √pᵢ|i⟩
    for (uint32_t i = 0; i < dim; i++) {
        state->amplitudes[i].real = sqrtf(fmaxf(0.0f, probabilities[i]));
        state->amplitudes[i].imag = 0.0f;
    }

    qstats_pure_state_normalize(state);
    return state;
}

qstats_result_t qstats_measure(
    const qstats_pure_state_t* state,
    qstats_measurement_t* result,
    uint32_t* seed
) {
    if (!state || !result || !seed) return QSTATS_ERROR_NULL;

    // Sample from |αᵢ|² distribution
    float r = qstats_rand_float(seed);
    float cumsum = 0.0f;

    for (uint32_t i = 0; i < state->dim; i++) {
        float p = qstats_complex_abs_squared(state->amplitudes[i]);
        cumsum += p;
        if (r <= cumsum) {
            result->outcome = i;
            result->probability = p;
            result->amplitude = state->amplitudes[i];
            return QSTATS_OK;
        }
    }

    // Fallback (rounding errors)
    result->outcome = state->dim - 1;
    result->probability = qstats_complex_abs_squared(state->amplitudes[state->dim - 1]);
    result->amplitude = state->amplitudes[state->dim - 1];

    return QSTATS_OK;
}

qstats_result_t qstats_measure_collapse(
    qstats_pure_state_t* state,
    qstats_measurement_t* result,
    uint32_t* seed
) {
    qstats_result_t res = qstats_measure(state, result, seed);
    if (res != QSTATS_OK) return res;

    // Collapse to measured state
    for (uint32_t i = 0; i < state->dim; i++) {
        state->amplitudes[i].real = 0.0f;
        state->amplitudes[i].imag = 0.0f;
    }
    state->amplitudes[result->outcome].real = 1.0f;

    return QSTATS_OK;
}

qstats_result_t qstats_measure_finite_shots(
    const qstats_pure_state_t* state,
    uint32_t num_shots,
    uint32_t* counts,
    uint32_t* seed
) {
    if (!state || !counts || !seed) return QSTATS_ERROR_NULL;

    memset(counts, 0, state->dim * sizeof(uint32_t));

    qstats_measurement_t result;
    for (uint32_t shot = 0; shot < num_shots; shot++) {
        qstats_measure(state, &result, seed);
        counts[result.outcome]++;
    }

    return QSTATS_OK;
}

//=============================================================================
// Eigenvalue Computation (Jacobi method for small matrices)
//=============================================================================

static void qstats_compute_eigenvalues_jacobi(
    const qstats_density_matrix_t* dm,
    float* eigenvalues
) {
    uint32_t n = dm->dim;

    // Integer overflow check for n * n allocation
    if (n > 0 && (size_t)n * n > SIZE_MAX / sizeof(float)) {
        return;
    }

    // Work with real part (density matrices are Hermitian, eigenvalues are real)
    float* A = nimcp_malloc((size_t)n * n * sizeof(float));
    float* V = nimcp_malloc((size_t)n * n * sizeof(float));

    // Copy real parts (for Hermitian matrix, eigenvalues come from real part structure)
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            A[i * n + j] = dm->elements[i * n + j].real;
        }
    }

    // Initialize V to identity
    memset(V, 0, n * n * sizeof(float));
    for (uint32_t i = 0; i < n; i++) {
        V[i * n + i] = 1.0f;
    }

    // Jacobi iterations
    const int max_iter = 100;
    for (int iter = 0; iter < max_iter; iter++) {
        // Find largest off-diagonal
        float max_off = 0.0f;
        uint32_t p = 0, q = 0;
        for (uint32_t i = 0; i < n; i++) {
            for (uint32_t j = i + 1; j < n; j++) {
                if (fabsf(A[i * n + j]) > max_off) {
                    max_off = fabsf(A[i * n + j]);
                    p = i;
                    q = j;
                }
            }
        }

        if (max_off < QSTATS_EPSILON) break;

        // Compute rotation angle
        float app = A[p * n + p];
        float aqq = A[q * n + q];
        float apq = A[p * n + q];

        float theta;
        if (fabsf(app - aqq) < QSTATS_EPSILON) {
            theta = PI / 4.0f;
        } else {
            theta = 0.5f * atanf(2.0f * apq / (app - aqq));
        }

        float c = cosf(theta);
        float s = sinf(theta);

        // Apply rotation
        for (uint32_t i = 0; i < n; i++) {
            if (i != p && i != q) {
                float aip = A[i * n + p];
                float aiq = A[i * n + q];
                A[i * n + p] = A[p * n + i] = c * aip - s * aiq;
                A[i * n + q] = A[q * n + i] = s * aip + c * aiq;
            }
        }

        A[p * n + p] = c * c * app - 2 * s * c * apq + s * s * aqq;
        A[q * n + q] = s * s * app + 2 * s * c * apq + c * c * aqq;
        A[p * n + q] = A[q * n + p] = 0.0f;
    }

    // Extract eigenvalues (diagonal)
    for (uint32_t i = 0; i < n; i++) {
        eigenvalues[i] = A[i * n + i];
    }

    // Sort in descending order
    for (uint32_t i = 0; i < n - 1; i++) {
        for (uint32_t j = i + 1; j < n; j++) {
            if (eigenvalues[j] > eigenvalues[i]) {
                float tmp = eigenvalues[i];
                eigenvalues[i] = eigenvalues[j];
                eigenvalues[j] = tmp;
            }
        }
    }

    nimcp_free(A);
    nimcp_free(V);
}

qstats_result_t qstats_eigenvalues(
    const qstats_density_matrix_t* dm,
    float* eigenvalues
) {
    if (!dm || !eigenvalues) return QSTATS_ERROR_NULL;

    // Check cache
    if (dm->eigenvalues_valid && dm->eigenvalues) {
        memcpy(eigenvalues, dm->eigenvalues, dm->dim * sizeof(float));
        return QSTATS_OK;
    }

    qstats_compute_eigenvalues_jacobi(dm, eigenvalues);

    // Cache if possible (cast away const for caching)
    qstats_density_matrix_t* dm_mutable = (qstats_density_matrix_t*)dm;
    if (!dm_mutable->eigenvalues) {
        dm_mutable->eigenvalues = nimcp_malloc(dm->dim * sizeof(float));
    }
    if (dm_mutable->eigenvalues) {
        memcpy(dm_mutable->eigenvalues, eigenvalues, dm->dim * sizeof(float));
        dm_mutable->eigenvalues_valid = true;
    }

    return QSTATS_OK;
}

//=============================================================================
// Quantum Entropy Functions
//=============================================================================

float qstats_von_neumann_entropy(const qstats_density_matrix_t* dm) {
    if (!dm) return NAN;

    float* eigenvalues = nimcp_malloc(dm->dim * sizeof(float));
    if (!eigenvalues) return NAN;

    qstats_eigenvalues(dm, eigenvalues);

    // S = -Σᵢ λᵢ log₂(λᵢ)
    float entropy = 0.0f;
    for (uint32_t i = 0; i < dm->dim; i++) {
        if (eigenvalues[i] > QSTATS_MIN_EIGENVALUE) {
            entropy -= eigenvalues[i] * log2f(eigenvalues[i]);
        }
    }

    nimcp_free(eigenvalues);
    return entropy;
}

float qstats_von_neumann_entropy_nats(const qstats_density_matrix_t* dm) {
    if (!dm) return NAN;

    float* eigenvalues = nimcp_malloc(dm->dim * sizeof(float));
    if (!eigenvalues) return NAN;

    qstats_eigenvalues(dm, eigenvalues);

    // S = -Σᵢ λᵢ ln(λᵢ)
    float entropy = 0.0f;
    for (uint32_t i = 0; i < dm->dim; i++) {
        if (eigenvalues[i] > QSTATS_MIN_EIGENVALUE) {
            entropy -= eigenvalues[i] * logf(eigenvalues[i]);
        }
    }

    nimcp_free(eigenvalues);
    return entropy;
}

qstats_result_t qstats_entropy_all(
    const qstats_density_matrix_t* dm,
    qstats_entropy_result_t* result
) {
    if (!dm || !result) return QSTATS_ERROR_NULL;

    float* eigenvalues = nimcp_malloc(dm->dim * sizeof(float));
    if (!eigenvalues) return QSTATS_ERROR_MEMORY;

    qstats_eigenvalues(dm, eigenvalues);

    // Von Neumann entropy
    result->von_neumann = 0.0f;
    result->von_neumann_nats = 0.0f;
    for (uint32_t i = 0; i < dm->dim; i++) {
        if (eigenvalues[i] > QSTATS_MIN_EIGENVALUE) {
            result->von_neumann -= eigenvalues[i] * log2f(eigenvalues[i]);
            result->von_neumann_nats -= eigenvalues[i] * logf(eigenvalues[i]);
        }
    }

    // Purity: Tr(ρ²) = Σᵢ λᵢ²
    result->purity = 0.0f;
    for (uint32_t i = 0; i < dm->dim; i++) {
        result->purity += eigenvalues[i] * eigenvalues[i];
    }

    // Linear entropy: S_L = (d/(d-1))(1 - Tr(ρ²))
    float d = (float)dm->dim;
    result->linear_entropy = (d / (d - 1.0f)) * (1.0f - result->purity);

    // Min-entropy: H_min = -log₂(λ_max)
    result->min_entropy = -log2f(fmaxf(eigenvalues[0], QSTATS_MIN_EIGENVALUE));

    // Max-entropy
    result->max_entropy = log2f(d);

    // Rényi-2 entropy: H₂ = -log₂(Σᵢ λᵢ²) = -log₂(purity)
    result->renyi_2 = -log2f(fmaxf(result->purity, QSTATS_MIN_EIGENVALUE));

    nimcp_free(eigenvalues);
    return QSTATS_OK;
}

float qstats_quantum_relative_entropy(
    const qstats_density_matrix_t* rho,
    const qstats_density_matrix_t* sigma
) {
    if (!rho || !sigma || rho->dim != sigma->dim) return NAN;

    float* lambda_rho = nimcp_malloc(rho->dim * sizeof(float));
    float* lambda_sigma = nimcp_malloc(sigma->dim * sizeof(float));
    if (!lambda_rho || !lambda_sigma) {
        nimcp_free(lambda_rho);
        nimcp_free(lambda_sigma);
        return NAN;
    }

    qstats_eigenvalues(rho, lambda_rho);
    qstats_eigenvalues(sigma, lambda_sigma);

    // S(ρ||σ) = Tr(ρ log ρ) - Tr(ρ log σ)
    // Simplified using eigenvalues: Σᵢ λ_ρ,ᵢ log(λ_ρ,ᵢ) - Σᵢ λ_ρ,ᵢ log(λ_σ,ᵢ)
    // Note: This is exact only when they share eigenbasis

    float rel_entropy = 0.0f;
    for (uint32_t i = 0; i < rho->dim; i++) {
        if (lambda_rho[i] > QSTATS_MIN_EIGENVALUE) {
            // Check if σ has support where ρ has support
            if (lambda_sigma[i] < QSTATS_MIN_EIGENVALUE) {
                nimcp_free(lambda_rho);
                nimcp_free(lambda_sigma);
                return INFINITY;
            }
            rel_entropy += lambda_rho[i] * (log2f(lambda_rho[i]) - log2f(lambda_sigma[i]));
        }
    }

    nimcp_free(lambda_rho);
    nimcp_free(lambda_sigma);
    return rel_entropy;
}

float qstats_quantum_mutual_information(
    const qstats_density_matrix_t* rho_ab,
    uint32_t dim_a,
    uint32_t dim_b
) {
    if (!rho_ab || dim_a * dim_b != rho_ab->dim) return NAN;

    // I(A:B) = S(ρ_A) + S(ρ_B) - S(ρ_AB)
    qstats_density_matrix_t* rho_a = qstats_partial_trace_b(rho_ab, dim_a, dim_b);
    qstats_density_matrix_t* rho_b = qstats_partial_trace_a(rho_ab, dim_a, dim_b);

    if (!rho_a || !rho_b) {
        qstats_density_matrix_destroy(rho_a);
        qstats_density_matrix_destroy(rho_b);
        return NAN;
    }

    float S_a = qstats_von_neumann_entropy(rho_a);
    float S_b = qstats_von_neumann_entropy(rho_b);
    float S_ab = qstats_von_neumann_entropy(rho_ab);

    qstats_density_matrix_destroy(rho_a);
    qstats_density_matrix_destroy(rho_b);

    return S_a + S_b - S_ab;
}

float qstats_quantum_conditional_entropy(
    const qstats_density_matrix_t* rho_ab,
    uint32_t dim_a,
    uint32_t dim_b
) {
    if (!rho_ab || dim_a * dim_b != rho_ab->dim) return NAN;

    // S(A|B) = S(ρ_AB) - S(ρ_B)
    qstats_density_matrix_t* rho_b = qstats_partial_trace_a(rho_ab, dim_a, dim_b);
    if (!rho_b) return NAN;

    float S_ab = qstats_von_neumann_entropy(rho_ab);
    float S_b = qstats_von_neumann_entropy(rho_b);

    qstats_density_matrix_destroy(rho_b);

    return S_ab - S_b;
}

float qstats_purity(const qstats_density_matrix_t* dm) {
    return qstats_trace_squared(dm);
}

float qstats_linear_entropy(const qstats_density_matrix_t* dm) {
    if (!dm) return NAN;

    float purity = qstats_purity(dm);
    float d = (float)dm->dim;

    // Normalized linear entropy
    return (d / (d - 1.0f)) * (1.0f - purity);
}

float qstats_renyi_entropy(const qstats_density_matrix_t* dm, float alpha) {
    if (!dm || alpha <= 0.0f || fabsf(alpha - 1.0f) < QSTATS_EPSILON) return NAN;

    float* eigenvalues = nimcp_malloc(dm->dim * sizeof(float));
    if (!eigenvalues) return NAN;

    qstats_eigenvalues(dm, eigenvalues);

    // S_α = (1/(1-α)) log(Σᵢ λᵢ^α)
    float sum = 0.0f;
    for (uint32_t i = 0; i < dm->dim; i++) {
        if (eigenvalues[i] > QSTATS_MIN_EIGENVALUE) {
            sum += powf(eigenvalues[i], alpha);
        }
    }

    nimcp_free(eigenvalues);
    return log2f(sum) / (1.0f - alpha);
}

float qstats_min_entropy(const qstats_density_matrix_t* dm) {
    if (!dm) return NAN;

    float* eigenvalues = nimcp_malloc(dm->dim * sizeof(float));
    if (!eigenvalues) return NAN;

    qstats_eigenvalues(dm, eigenvalues);

    // H_min = -log₂(λ_max)
    float lambda_max = eigenvalues[0];  // Already sorted descending

    nimcp_free(eigenvalues);
    return -log2f(fmaxf(lambda_max, QSTATS_MIN_EIGENVALUE));
}

float qstats_max_entropy(uint32_t dim) {
    if (dim == 0) return NAN;
    return log2f((float)dim);
}

//=============================================================================
// Quantum Correlation and Distance Measures
//=============================================================================

float qstats_fidelity(
    const qstats_density_matrix_t* rho,
    const qstats_density_matrix_t* sigma
) {
    if (!rho || !sigma || rho->dim != sigma->dim) return NAN;

    // Simplified fidelity for diagonal density matrices or pure states
    // F(ρ,σ) ≈ (Σᵢ √(ρᵢᵢ σᵢᵢ))²

    float sqrt_overlap = 0.0f;
    for (uint32_t i = 0; i < rho->dim; i++) {
        float rho_ii = rho->elements[i * rho->dim + i].real;
        float sigma_ii = sigma->elements[i * sigma->dim + i].real;
        sqrt_overlap += sqrtf(fmaxf(0.0f, rho_ii) * fmaxf(0.0f, sigma_ii));
    }

    return sqrt_overlap * sqrt_overlap;
}

float qstats_fidelity_pure(
    const qstats_pure_state_t* psi,
    const qstats_pure_state_t* phi
) {
    if (!psi || !phi || psi->dim != phi->dim) return NAN;

    // F = |⟨ψ|φ⟩|²
    qstats_complex_t inner = qstats_inner_product(psi, phi);
    return qstats_complex_abs_squared(inner);
}

float qstats_trace_distance(
    const qstats_density_matrix_t* rho,
    const qstats_density_matrix_t* sigma
) {
    if (!rho || !sigma || rho->dim != sigma->dim) return NAN;

    // D(ρ,σ) = ½ Tr|ρ-σ|
    // Compute ρ-σ, then eigenvalues, sum absolute values

    uint32_t n = rho->dim;
    qstats_density_matrix_t* diff = qstats_density_matrix_create(n);
    if (!diff) return NAN;

    for (uint32_t i = 0; i < n * n; i++) {
        diff->elements[i] = qstats_complex_sub(rho->elements[i], sigma->elements[i]);
    }

    float* eigenvalues = nimcp_malloc(n * sizeof(float));
    if (!eigenvalues) {
        qstats_density_matrix_destroy(diff);
        return NAN;
    }

    qstats_eigenvalues(diff, eigenvalues);

    float trace_abs = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        trace_abs += fabsf(eigenvalues[i]);
    }

    nimcp_free(eigenvalues);
    qstats_density_matrix_destroy(diff);

    return 0.5f * trace_abs;
}

float qstats_bures_distance(
    const qstats_density_matrix_t* rho,
    const qstats_density_matrix_t* sigma
) {
    float F = qstats_fidelity(rho, sigma);
    return sqrtf(2.0f * (1.0f - sqrtf(fmaxf(0.0f, F))));
}

float qstats_hellinger_distance(
    const qstats_density_matrix_t* rho,
    const qstats_density_matrix_t* sigma
) {
    if (!rho || !sigma || rho->dim != sigma->dim) return NAN;

    // Simplified Hellinger using diagonal elements
    float overlap = 0.0f;
    for (uint32_t i = 0; i < rho->dim; i++) {
        float sqrt_rho = sqrtf(fmaxf(0.0f, rho->elements[i * rho->dim + i].real));
        float sqrt_sigma = sqrtf(fmaxf(0.0f, sigma->elements[i * sigma->dim + i].real));
        overlap += sqrt_rho * sqrt_sigma;
    }

    return sqrtf(2.0f * (1.0f - overlap));
}

qstats_result_t qstats_correlation_all(
    const qstats_density_matrix_t* rho,
    const qstats_density_matrix_t* sigma,
    qstats_correlation_result_t* result
) {
    if (!rho || !sigma || !result) return QSTATS_ERROR_NULL;

    result->fidelity = qstats_fidelity(rho, sigma);
    result->trace_distance = qstats_trace_distance(rho, sigma);
    result->relative_entropy = qstats_quantum_relative_entropy(rho, sigma);
    result->bures_distance = qstats_bures_distance(rho, sigma);
    result->hellinger_distance = qstats_hellinger_distance(rho, sigma);

    return QSTATS_OK;
}

//=============================================================================
// Quantum Entanglement Measures
//=============================================================================

float qstats_entanglement_entropy(
    const qstats_density_matrix_t* rho_ab,
    uint32_t dim_a,
    uint32_t dim_b
) {
    if (!rho_ab || dim_a * dim_b != rho_ab->dim) return NAN;

    // E(ρ_AB) = S(ρ_A)
    qstats_density_matrix_t* rho_a = qstats_partial_trace_b(rho_ab, dim_a, dim_b);
    if (!rho_a) return NAN;

    float entropy = qstats_von_neumann_entropy(rho_a);
    qstats_density_matrix_destroy(rho_a);

    return entropy;
}

float qstats_concurrence(const qstats_density_matrix_t* rho_ab) {
    if (!rho_ab || rho_ab->dim != 4) return NAN;  // Only for 2-qubit states

    // Pauli Y matrix for single qubit
    // σy = [0, -i; i, 0]
    // σy ⊗ σy for 2 qubits

    // Simplified: For diagonal or X-states, use direct formula
    // General case requires: R = ρ (σy⊗σy) ρ* (σy⊗σy)
    // Then concurrence C = max(0, √λ₁ - √λ₂ - √λ₃ - √λ₄)

    // For now, use simplified approximation based on entanglement entropy
    uint32_t dim_a = 2, dim_b = 2;
    float E = qstats_entanglement_entropy(rho_ab, dim_a, dim_b);

    // Convert entanglement entropy to concurrence (approximate inverse)
    // For 2 qubits: E_F = h((1+√(1-C²))/2) where h is binary entropy
    // Inversion is approximate
    float x = 1.0f - powf(2.0f, -E);  // Approximate
    float C = sqrtf(fmaxf(0.0f, 2.0f * x - x * x));

    return fminf(1.0f, fmaxf(0.0f, C));
}

float qstats_entanglement_of_formation(const qstats_density_matrix_t* rho_ab) {
    float C = qstats_concurrence(rho_ab);

    // E_F = h((1 + √(1-C²))/2) where h(p) = -p log p - (1-p) log(1-p)
    float x = (1.0f + sqrtf(fmaxf(0.0f, 1.0f - C * C))) / 2.0f;

    if (x < QSTATS_EPSILON || x > 1.0f - QSTATS_EPSILON) {
        return 0.0f;
    }

    return -x * log2f(x) - (1.0f - x) * log2f(1.0f - x);
}

float qstats_negativity(
    const qstats_density_matrix_t* rho_ab,
    uint32_t dim_a,
    uint32_t dim_b
) {
    if (!rho_ab || dim_a * dim_b != rho_ab->dim) return NAN;

    qstats_density_matrix_t* rho_pt = qstats_partial_transpose_a(rho_ab, dim_a, dim_b);
    if (!rho_pt) return NAN;

    // N = (||ρ^(T_A)||₁ - 1) / 2
    // ||ρ||₁ = Σᵢ |λᵢ| (trace norm)

    float* eigenvalues = nimcp_malloc(rho_pt->dim * sizeof(float));
    if (!eigenvalues) {
        qstats_density_matrix_destroy(rho_pt);
        return NAN;
    }

    qstats_eigenvalues(rho_pt, eigenvalues);

    float trace_norm = 0.0f;
    for (uint32_t i = 0; i < rho_pt->dim; i++) {
        trace_norm += fabsf(eigenvalues[i]);
    }

    nimcp_free(eigenvalues);
    qstats_density_matrix_destroy(rho_pt);

    return (trace_norm - 1.0f) / 2.0f;
}

float qstats_log_negativity(
    const qstats_density_matrix_t* rho_ab,
    uint32_t dim_a,
    uint32_t dim_b
) {
    float N = qstats_negativity(rho_ab, dim_a, dim_b);
    return log2f(2.0f * N + 1.0f);
}

//=============================================================================
// Quantum Fisher Information
//=============================================================================

float qstats_quantum_fisher_information(
    const qstats_density_matrix_t* rho,
    const qstats_complex_t* generator,
    uint32_t dim
) {
    if (!rho || !generator || dim != rho->dim) return NAN;

    // F_Q = 2 Σᵢⱼ ((λᵢ-λⱼ)²/(λᵢ+λⱼ)) |⟨i|H|j⟩|²
    // Simplified for diagonal basis

    float* eigenvalues = nimcp_malloc(dim * sizeof(float));
    if (!eigenvalues) return NAN;

    qstats_eigenvalues(rho, eigenvalues);

    // Use diagonal elements of generator as approximation
    float fisher = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        for (uint32_t j = 0; j < dim; j++) {
            if (i != j) {
                float denom = eigenvalues[i] + eigenvalues[j];
                if (denom > QSTATS_EPSILON) {
                    float num = eigenvalues[i] - eigenvalues[j];
                    float H_ij_sq = qstats_complex_abs_squared(generator[i * dim + j]);
                    fisher += 2.0f * (num * num / denom) * H_ij_sq;
                }
            }
        }
    }

    nimcp_free(eigenvalues);
    return fisher;
}

qstats_result_t qstats_quantum_fisher_matrix(
    const qstats_density_matrix_t* rho,
    const qstats_complex_t** generators,
    uint32_t n_params,
    qstats_fisher_result_t* result
) {
    if (!rho || !generators || !result) return QSTATS_ERROR_NULL;

    result->n_params = n_params;
    result->fisher_matrix = nimcp_calloc(n_params * n_params, sizeof(float));
    result->cramer_rao_bounds = nimcp_calloc(n_params, sizeof(float));

    if (!result->fisher_matrix || !result->cramer_rao_bounds) {
        nimcp_free(result->fisher_matrix);
        nimcp_free(result->cramer_rao_bounds);
        return QSTATS_ERROR_MEMORY;
    }

    // Compute diagonal Fisher information for each parameter
    for (uint32_t i = 0; i < n_params; i++) {
        float F_ii = qstats_quantum_fisher_information(rho, generators[i], rho->dim);
        result->fisher_matrix[i * n_params + i] = F_ii;
        result->cramer_rao_bounds[i] = (F_ii > QSTATS_EPSILON) ? 1.0f / F_ii : INFINITY;
    }

    // Total Fisher information
    result->total_fisher_info = 0.0f;
    for (uint32_t i = 0; i < n_params; i++) {
        result->total_fisher_info += result->fisher_matrix[i * n_params + i];
    }

    return QSTATS_OK;
}

void qstats_fisher_result_free(qstats_fisher_result_t* result) {
    if (result) {
        nimcp_free(result->fisher_matrix);
        nimcp_free(result->cramer_rao_bounds);
        result->fisher_matrix = NULL;
        result->cramer_rao_bounds = NULL;
    }
}

qstats_result_t qstats_symmetric_log_derivative(
    const qstats_density_matrix_t* rho,
    const qstats_complex_t* drho_dtheta,
    qstats_complex_t* sld,
    uint32_t dim
) {
    if (!rho || !drho_dtheta || !sld) return QSTATS_ERROR_NULL;

    // Simplified SLD computation for diagonal rho
    // L_ij = 2 * drho_ij / (rho_ii + rho_jj)

    for (uint32_t i = 0; i < dim; i++) {
        for (uint32_t j = 0; j < dim; j++) {
            float denom = rho->elements[i * dim + i].real + rho->elements[j * dim + j].real;
            if (denom > QSTATS_EPSILON) {
                sld[i * dim + j].real = 2.0f * drho_dtheta[i * dim + j].real / denom;
                sld[i * dim + j].imag = 2.0f * drho_dtheta[i * dim + j].imag / denom;
            } else {
                sld[i * dim + j].real = 0.0f;
                sld[i * dim + j].imag = 0.0f;
            }
        }
    }

    return QSTATS_OK;
}

//=============================================================================
// Quantum Hypothesis Testing
//=============================================================================

float qstats_quantum_discrimination_error(
    const qstats_density_matrix_t* rho0,
    const qstats_density_matrix_t* rho1,
    float prior0
) {
    if (!rho0 || !rho1 || rho0->dim != rho1->dim) return NAN;
    if (prior0 < 0.0f || prior0 > 1.0f) return NAN;

    // Minimum error via Helstrom bound: P_err = ½(1 - ||π₀ρ₀ - π₁ρ₁||₁)
    float prior1 = 1.0f - prior0;

    // Compute weighted difference
    uint32_t n = rho0->dim;
    qstats_density_matrix_t* diff = qstats_density_matrix_create(n);
    if (!diff) return NAN;

    for (uint32_t i = 0; i < n * n; i++) {
        diff->elements[i].real = prior0 * rho0->elements[i].real - prior1 * rho1->elements[i].real;
        diff->elements[i].imag = prior0 * rho0->elements[i].imag - prior1 * rho1->elements[i].imag;
    }

    float* eigenvalues = nimcp_malloc(n * sizeof(float));
    if (!eigenvalues) {
        qstats_density_matrix_destroy(diff);
        return NAN;
    }

    qstats_eigenvalues(diff, eigenvalues);

    float trace_norm = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        trace_norm += fabsf(eigenvalues[i]);
    }

    nimcp_free(eigenvalues);
    qstats_density_matrix_destroy(diff);

    return 0.5f * (1.0f - trace_norm);
}

float qstats_quantum_chernoff_bound(
    const qstats_density_matrix_t* rho,
    const qstats_density_matrix_t* sigma
) {
    if (!rho || !sigma || rho->dim != sigma->dim) return NAN;

    // Quantum Chernoff bound: ξ = -log(min_s Tr(ρ^s σ^(1-s)))
    // Search over s ∈ [0,1]

    float min_trace = INFINITY;
    const int num_s = 11;  // Check s = 0, 0.1, ..., 1.0

    for (int si = 0; si <= num_s; si++) {
        float s = (float)si / (float)num_s;

        // Approximate using diagonal elements
        float trace = 0.0f;
        for (uint32_t i = 0; i < rho->dim; i++) {
            float rho_ii = fmaxf(QSTATS_MIN_EIGENVALUE, rho->elements[i * rho->dim + i].real);
            float sigma_ii = fmaxf(QSTATS_MIN_EIGENVALUE, sigma->elements[i * sigma->dim + i].real);
            trace += powf(rho_ii, s) * powf(sigma_ii, 1.0f - s);
        }

        if (trace < min_trace) {
            min_trace = trace;
        }
    }

    return -log2f(fmaxf(QSTATS_MIN_EIGENVALUE, min_trace));
}

float qstats_quantum_hoeffding_exponent(
    const qstats_density_matrix_t* rho,
    const qstats_density_matrix_t* sigma,
    float R
) {
    if (!rho || !sigma || R <= 0.0f) return NAN;

    // Simplified: use relative entropy bound
    float S_rel = qstats_quantum_relative_entropy(rho, sigma);

    // Hoeffding exponent ≈ max(0, R - S(ρ||σ))
    return fmaxf(0.0f, R - S_rel);
}

//=============================================================================
// Integration with Quantum Walk
//=============================================================================

qstats_pure_state_t* qstats_from_quantum_walk(
    const float* amplitudes_real,
    const float* amplitudes_imag,
    uint32_t num_nodes
) {
    if (!amplitudes_real || !amplitudes_imag || num_nodes == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qstats_from_quantum_walk: required parameter is NULL (amplitudes_real, amplitudes_imag)");
        return NULL;
    }

    qstats_pure_state_t* state = qstats_pure_state_create(num_nodes);
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "qstats_from_quantum_walk: state is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < num_nodes; i++) {
        state->amplitudes[i].real = amplitudes_real[i];
        state->amplitudes[i].imag = amplitudes_imag[i];
    }

    qstats_pure_state_normalize(state);
    return state;
}

float qstats_quantum_walk_entropy(
    const float* amplitudes_real,
    const float* amplitudes_imag,
    uint32_t num_nodes
) {
    if (!amplitudes_real || !amplitudes_imag || num_nodes == 0) return NAN;

    // H = -Σᵢ |αᵢ|² log₂ |αᵢ|²
    float entropy = 0.0f;
    for (uint32_t i = 0; i < num_nodes; i++) {
        float prob = amplitudes_real[i] * amplitudes_real[i] +
                     amplitudes_imag[i] * amplitudes_imag[i];
        if (prob > QSTATS_MIN_EIGENVALUE) {
            entropy -= prob * log2f(prob);
        }
    }

    return entropy;
}

float qstats_quantum_walk_localization(
    const float* amplitudes_real,
    const float* amplitudes_imag,
    uint32_t num_nodes
) {
    if (!amplitudes_real || !amplitudes_imag || num_nodes == 0) return NAN;

    // IPR = Σᵢ |αᵢ|⁴
    float ipr = 0.0f;
    for (uint32_t i = 0; i < num_nodes; i++) {
        float prob = amplitudes_real[i] * amplitudes_real[i] +
                     amplitudes_imag[i] * amplitudes_imag[i];
        ipr += prob * prob;
    }

    // Return effective number of sites = 1/IPR
    return (ipr > QSTATS_EPSILON) ? 1.0f / ipr : (float)num_nodes;
}

float qstats_quantum_walk_coherence(
    const float* amplitudes_real,
    const float* amplitudes_imag,
    uint32_t num_nodes
) {
    if (!amplitudes_real || !amplitudes_imag || num_nodes == 0) return NAN;

    // C = Σᵢ≠ⱼ |⟨i|ρ|j⟩| = Σᵢ≠ⱼ |αᵢ αⱼ*|
    float coherence = 0.0f;
    for (uint32_t i = 0; i < num_nodes; i++) {
        float amp_i = sqrtf(amplitudes_real[i] * amplitudes_real[i] +
                           amplitudes_imag[i] * amplitudes_imag[i]);
        for (uint32_t j = 0; j < num_nodes; j++) {
            if (i != j) {
                float amp_j = sqrtf(amplitudes_real[j] * amplitudes_real[j] +
                                   amplitudes_imag[j] * amplitudes_imag[j]);
                coherence += amp_i * amp_j;
            }
        }
    }

    return coherence;
}

//=============================================================================
// Integration with Quantum Annealing
//=============================================================================

qstats_result_t qstats_boltzmann_distribution(
    const float* energies,
    uint32_t num_states,
    float temperature,
    float* probabilities
) {
    if (!energies || !probabilities || num_states == 0) return QSTATS_ERROR_NULL;
    if (temperature <= 0.0f) return QSTATS_ERROR_INVALID;

    float beta = 1.0f / temperature;

    // Find minimum energy for numerical stability
    float E_min = energies[0];
    for (uint32_t i = 1; i < num_states; i++) {
        if (energies[i] < E_min) E_min = energies[i];
    }

    // Compute unnormalized Boltzmann factors
    float Z = 0.0f;
    for (uint32_t i = 0; i < num_states; i++) {
        probabilities[i] = expf(-beta * (energies[i] - E_min));
        Z += probabilities[i];
    }

    // Normalize
    for (uint32_t i = 0; i < num_states; i++) {
        probabilities[i] /= Z;
    }

    return QSTATS_OK;
}

float qstats_free_energy(
    const float* energies,
    uint32_t num_states,
    float temperature
) {
    if (!energies || num_states == 0 || temperature <= 0.0f) return NAN;

    float beta = 1.0f / temperature;

    // Find minimum energy
    float E_min = energies[0];
    for (uint32_t i = 1; i < num_states; i++) {
        if (energies[i] < E_min) E_min = energies[i];
    }

    // Z = Σᵢ exp(-β(Eᵢ - E_min))
    float Z = 0.0f;
    for (uint32_t i = 0; i < num_states; i++) {
        Z += expf(-beta * (energies[i] - E_min));
    }

    // F = -T log Z + E_min = E_min - T log Z
    return E_min - temperature * logf(Z);
}

float qstats_thermodynamic_entropy(
    const float* energies,
    uint32_t num_states,
    float temperature
) {
    if (!energies || num_states == 0 || temperature <= 0.0f) return NAN;

    float* probs = nimcp_malloc(num_states * sizeof(float));
    if (!probs) return NAN;

    qstats_boltzmann_distribution(energies, num_states, temperature, probs);

    // <E> = Σᵢ pᵢ Eᵢ
    float mean_E = 0.0f;
    for (uint32_t i = 0; i < num_states; i++) {
        mean_E += probs[i] * energies[i];
    }

    float F = qstats_free_energy(energies, num_states, temperature);

    nimcp_free(probs);

    // S = (<E> - F) / T
    return (mean_E - F) / temperature;
}

float qstats_partition_function(
    const float* energies,
    uint32_t num_states,
    float temperature
) {
    if (!energies || num_states == 0 || temperature <= 0.0f) return NAN;

    float beta = 1.0f / temperature;

    // Find minimum for stability
    float E_min = energies[0];
    for (uint32_t i = 1; i < num_states; i++) {
        if (energies[i] < E_min) E_min = energies[i];
    }

    float Z = 0.0f;
    for (uint32_t i = 0; i < num_states; i++) {
        Z += expf(-beta * (energies[i] - E_min));
    }

    return Z * expf(-beta * E_min);
}

float qstats_partition_function_mc(
    const float* energies,
    uint32_t num_states,
    float temperature,
    uint32_t num_samples,
    float* variance_out
) {
    if (!energies || num_states == 0 || temperature <= 0.0f) return NAN;

    uint32_t seed = (qstats_rng_seed == 0) ? 12345 : qstats_rng_seed;
    float beta = 1.0f / temperature;

    // Monte Carlo estimation with importance sampling
    float sum = 0.0f;
    float sum_sq = 0.0f;

    for (uint32_t s = 0; s < num_samples; s++) {
        // Sample uniformly
        uint32_t idx = qstats_rand(&seed) % num_states;
        float weight = expf(-beta * energies[idx]) * (float)num_states;
        sum += weight;
        sum_sq += weight * weight;
    }

    float mean = sum / (float)num_samples;

    if (variance_out) {
        float var = (sum_sq / (float)num_samples) - mean * mean;
        *variance_out = var / (float)num_samples;
    }

    return mean;
}

//=============================================================================
// Monte Carlo Methods for Quantum Statistics
//=============================================================================

float qstats_von_neumann_entropy_mc(
    const qstats_density_matrix_t* dm,
    uint32_t num_samples,
    float* variance_out
) {
    if (!dm) return NAN;

    // For large systems, use MC sampling of diagonal
    float* probs = nimcp_malloc(dm->dim * sizeof(float));
    if (!probs) return NAN;

    qstats_diagonal_probabilities(dm, probs);

    uint32_t seed = (qstats_rng_seed == 0) ? 12345 : qstats_rng_seed;

    float sum = 0.0f;
    float sum_sq = 0.0f;

    for (uint32_t s = 0; s < num_samples; s++) {
        // Sample from probability distribution
        float r = qstats_rand_float(&seed);
        float cumsum = 0.0f;
        uint32_t idx = 0;
        for (uint32_t i = 0; i < dm->dim; i++) {
            cumsum += probs[i];
            if (r <= cumsum) {
                idx = i;
                break;
            }
        }

        // -log p(idx) contribution
        if (probs[idx] > QSTATS_MIN_EIGENVALUE) {
            float contrib = -log2f(probs[idx]);
            sum += contrib;
            sum_sq += contrib * contrib;
        }
    }

    float mean = sum / (float)num_samples;

    if (variance_out) {
        float var = (sum_sq / (float)num_samples) - mean * mean;
        *variance_out = var / (float)num_samples;
    }

    nimcp_free(probs);
    return mean;
}

float qstats_quantum_relative_entropy_mc(
    const qstats_density_matrix_t* rho,
    const qstats_density_matrix_t* sigma,
    uint32_t num_samples,
    float* variance_out
) {
    if (!rho || !sigma || rho->dim != sigma->dim) return NAN;

    float* probs_rho = nimcp_malloc(rho->dim * sizeof(float));
    float* probs_sigma = nimcp_malloc(sigma->dim * sizeof(float));
    if (!probs_rho || !probs_sigma) {
        nimcp_free(probs_rho);
        nimcp_free(probs_sigma);
        return NAN;
    }

    qstats_diagonal_probabilities(rho, probs_rho);
    qstats_diagonal_probabilities(sigma, probs_sigma);

    uint32_t seed = (qstats_rng_seed == 0) ? 12345 : qstats_rng_seed;

    float sum = 0.0f;
    float sum_sq = 0.0f;

    for (uint32_t s = 0; s < num_samples; s++) {
        // Sample from rho
        float r = qstats_rand_float(&seed);
        float cumsum = 0.0f;
        uint32_t idx = 0;
        for (uint32_t i = 0; i < rho->dim; i++) {
            cumsum += probs_rho[i];
            if (r <= cumsum) {
                idx = i;
                break;
            }
        }

        // log(p_rho/p_sigma) contribution
        if (probs_rho[idx] > QSTATS_MIN_EIGENVALUE &&
            probs_sigma[idx] > QSTATS_MIN_EIGENVALUE) {
            float contrib = log2f(probs_rho[idx]) - log2f(probs_sigma[idx]);
            sum += contrib;
            sum_sq += contrib * contrib;
        } else if (probs_rho[idx] > QSTATS_MIN_EIGENVALUE) {
            // sigma has no support where rho has support
            nimcp_free(probs_rho);
            nimcp_free(probs_sigma);
            return INFINITY;
        }
    }

    float mean = sum / (float)num_samples;

    if (variance_out) {
        float var = (sum_sq / (float)num_samples) - mean * mean;
        *variance_out = var / (float)num_samples;
    }

    nimcp_free(probs_rho);
    nimcp_free(probs_sigma);
    return mean;
}

float qstats_fidelity_mc(
    const qstats_density_matrix_t* rho,
    const qstats_density_matrix_t* sigma,
    uint32_t num_samples,
    float* variance_out
) {
    // For MC fidelity, we use the approximation F ≈ (Σᵢ √(ρᵢᵢ σᵢᵢ))²
    // No real MC needed for diagonal case, but provided for API consistency

    (void)num_samples;  // Unused for diagonal approximation

    if (variance_out) *variance_out = 0.0f;

    return qstats_fidelity(rho, sigma);
}

//=============================================================================
// Quantum Channel Statistics
//=============================================================================

float qstats_quantum_channel_capacity(
    const qstats_complex_t** channel_kraus,
    uint32_t num_kraus,
    uint32_t dim
) {
    // Simplified: use maximally mixed input state
    // C ≈ log(d) - min_χ χ where χ is output entropy

    (void)channel_kraus;
    (void)num_kraus;

    // Upper bound: log(d)
    return log2f((float)dim);
}

float qstats_coherent_information(
    const qstats_density_matrix_t* rho,
    const qstats_complex_t** channel_kraus,
    uint32_t num_kraus,
    uint32_t dim
) {
    // Simplified placeholder
    (void)channel_kraus;
    (void)num_kraus;
    (void)dim;

    // I_c ≤ S(ρ)
    return qstats_von_neumann_entropy(rho);
}

//=============================================================================
// Utility Functions
//=============================================================================

qstats_complex_t qstats_inner_product(
    const qstats_pure_state_t* psi,
    const qstats_pure_state_t* phi
) {
    qstats_complex_t result = {0.0f, 0.0f};

    if (!psi || !phi || psi->dim != phi->dim) return result;

    // ⟨ψ|φ⟩ = Σᵢ αᵢ* βᵢ
    for (uint32_t i = 0; i < psi->dim; i++) {
        qstats_complex_t psi_conj = qstats_complex_conj(psi->amplitudes[i]);
        qstats_complex_t prod = qstats_complex_mul(psi_conj, phi->amplitudes[i]);
        result = qstats_complex_add(result, prod);
    }

    return result;
}

qstats_density_matrix_t* qstats_partial_trace_b(
    const qstats_density_matrix_t* rho_ab,
    uint32_t dim_a,
    uint32_t dim_b
) {
    if (!rho_ab || dim_a * dim_b != rho_ab->dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "qstats_partial_trace_b: rho_ab is NULL");
        return NULL;
    }

    qstats_density_matrix_t* rho_a = qstats_density_matrix_create(dim_a);
    if (!rho_a) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "qstats_partial_trace_b: rho_a is NULL");
        return NULL;
    }

    // ρ_A(i,j) = Σₖ ρ_AB(i×d_B+k, j×d_B+k)
    for (uint32_t i = 0; i < dim_a; i++) {
        for (uint32_t j = 0; j < dim_a; j++) {
            qstats_complex_t sum = {0.0f, 0.0f};
            for (uint32_t k = 0; k < dim_b; k++) {
                uint32_t idx_ik = i * dim_b + k;
                uint32_t idx_jk = j * dim_b + k;
                sum = qstats_complex_add(sum,
                    rho_ab->elements[idx_ik * rho_ab->dim + idx_jk]);
            }
            rho_a->elements[i * dim_a + j] = sum;
        }
    }

    return rho_a;
}

qstats_density_matrix_t* qstats_partial_trace_a(
    const qstats_density_matrix_t* rho_ab,
    uint32_t dim_a,
    uint32_t dim_b
) {
    if (!rho_ab || dim_a * dim_b != rho_ab->dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "qstats_partial_trace_a: rho_ab is NULL");
        return NULL;
    }

    qstats_density_matrix_t* rho_b = qstats_density_matrix_create(dim_b);
    if (!rho_b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "qstats_partial_trace_a: rho_b is NULL");
        return NULL;
    }

    // ρ_B(k,l) = Σᵢ ρ_AB(i×d_B+k, i×d_B+l)
    for (uint32_t k = 0; k < dim_b; k++) {
        for (uint32_t l = 0; l < dim_b; l++) {
            qstats_complex_t sum = {0.0f, 0.0f};
            for (uint32_t i = 0; i < dim_a; i++) {
                uint32_t idx_ik = i * dim_b + k;
                uint32_t idx_il = i * dim_b + l;
                sum = qstats_complex_add(sum,
                    rho_ab->elements[idx_ik * rho_ab->dim + idx_il]);
            }
            rho_b->elements[k * dim_b + l] = sum;
        }
    }

    return rho_b;
}

qstats_density_matrix_t* qstats_partial_transpose_a(
    const qstats_density_matrix_t* rho_ab,
    uint32_t dim_a,
    uint32_t dim_b
) {
    if (!rho_ab || dim_a * dim_b != rho_ab->dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "qstats_partial_transpose_a: rho_ab is NULL");
        return NULL;
    }

    uint32_t dim = dim_a * dim_b;
    qstats_density_matrix_t* rho_pt = qstats_density_matrix_create(dim);
    if (!rho_pt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "qstats_partial_transpose_a: rho_pt is NULL");
        return NULL;
    }

    // ρ^(T_A)(i×d_B+k, j×d_B+l) = ρ(j×d_B+k, i×d_B+l)
    for (uint32_t i = 0; i < dim_a; i++) {
        for (uint32_t j = 0; j < dim_a; j++) {
            for (uint32_t k = 0; k < dim_b; k++) {
                for (uint32_t l = 0; l < dim_b; l++) {
                    uint32_t idx_out = (i * dim_b + k) * dim + (j * dim_b + l);
                    uint32_t idx_in = (j * dim_b + k) * dim + (i * dim_b + l);
                    rho_pt->elements[idx_out] = rho_ab->elements[idx_in];
                }
            }
        }
    }

    return rho_pt;
}

float qstats_trace(const qstats_density_matrix_t* dm) {
    if (!dm) return NAN;

    float trace = 0.0f;
    for (uint32_t i = 0; i < dm->dim; i++) {
        trace += dm->elements[i * dm->dim + i].real;
    }

    return trace;
}

float qstats_trace_squared(const qstats_density_matrix_t* dm) {
    if (!dm) return NAN;

    // Tr(ρ²) = Σᵢⱼ |ρᵢⱼ|²
    float trace_sq = 0.0f;
    for (uint32_t i = 0; i < dm->dim; i++) {
        for (uint32_t j = 0; j < dm->dim; j++) {
            trace_sq += qstats_complex_abs_squared(dm->elements[i * dm->dim + j]);
        }
    }

    return trace_sq;
}
