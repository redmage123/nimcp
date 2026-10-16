/**
 * @file nimcp_quantum_kernels.cu
 * @brief GPU Quantum Algorithm CUDA Kernels Implementation
 *
 * WHAT: CUDA kernels for quantum-inspired algorithms
 * WHY:  GPU acceleration for Grover's search and quantum annealing
 * HOW:  Custom kernels for quantum state manipulation and optimization
 *
 * RNG ARCHITECTURE:
 * =================
 * Uses kernel_init_rng for quantum annealing spin flip operations.
 * The pattern follows the central GPU statistics module:
 *   - See: gpu/statistics/nimcp_statistics_gpu.h for stats_gpu_rng_create/destroy
 *   - See: gpu/common/nimcp_device_utils.cuh for shared device RNG functions
 *
 * Local RNG state is used for:
 *   1. Metropolis-Hastings acceptance in simulated annealing
 *   2. Stochastic quantum state perturbations
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

// Include CUDA headers FIRST (before any extern "C" blocks from our headers)
#include "utils/memory/nimcp_memory.h"
#include <cuda_runtime.h>
#include <curand_kernel.h>
#include <math.h>
#include <float.h>

// Now include our headers (which have extern "C" blocks)
#include "gpu/quantum/nimcp_quantum_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "constants/nimcp_math_constants.h"

#define LOG_MODULE "QUANTUM_GPU"

#define BLOCK_SIZE 256
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)

//=============================================================================
// Device Helper Functions
//=============================================================================

__device__ inline float device_rsqrt(float x)
{
    return rsqrtf(x);
}

__device__ inline void device_complex_mul(
    float a_real, float a_imag,
    float b_real, float b_imag,
    float* c_real, float* c_imag)
{
    *c_real = a_real * b_real - a_imag * b_imag;
    *c_imag = a_real * b_imag + a_imag * b_real;
}

__device__ inline float device_complex_mag_sq(float real, float imag)
{
    return real * real + imag * imag;
}

//=============================================================================
// Default Configurations
//=============================================================================

nimcp_grover_config_t nimcp_grover_default_config(uint32_t n_qubits)
{
    nimcp_grover_config_t config;
    config.n_qubits = n_qubits;
    config.marked_states = NULL;
    config.n_marked = 0;  // User must set marked states
    config.optimal_iterations = nimcp_grover_optimal_iterations(n_qubits, 1);
    config.success_probability = 0.0f;  // Computed during search
    return config;
}

nimcp_annealing_config_t nimcp_annealing_default_config(uint32_t n_spins)
{
    nimcp_annealing_config_t config;
    config.n_spins = n_spins;
    config.T_initial = 10.0f;
    config.T_final = 0.01f;
    config.n_steps = 1000;
    config.transverse_field_initial = 5.0f;
    config.transverse_field_final = 0.01f;
    config.use_schedule = false;
    config.schedule = NULL;
    return config;
}

uint32_t nimcp_grover_optimal_iterations(uint32_t n_qubits, uint32_t n_marked)
{
    // Optimal iterations: pi/4 * sqrt(N/M) where N = 2^n_qubits, M = n_marked
    uint64_t N = 1ULL << n_qubits;
    float ratio = (float)N / (float)n_marked;
    float iterations = (M_PI / 4.0f) * sqrtf(ratio);
    return (uint32_t)roundf(iterations);
}

//=============================================================================
// Quantum State Operations - CUDA Kernels
//=============================================================================

/**
 * @brief Kernel to initialize state to |0>
 */
__global__ void kernel_init_zero_state(float* real, float* imag, size_t n_states)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_states) return;

    real[idx] = (idx == 0) ? 1.0f : 0.0f;
    imag[idx] = 0.0f;
}

/**
 * @brief Kernel to apply Hadamard to all qubits (uniform superposition)
 */
__global__ void kernel_hadamard_all(
    float* real, float* imag, size_t n_states, float norm_factor)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_states) return;

    real[idx] = norm_factor;
    imag[idx] = 0.0f;
}

/**
 * @brief Kernel to apply single-qubit gate
 */
__global__ void kernel_single_qubit_gate(
    float* real, float* imag,
    uint32_t qubit_idx, uint32_t n_qubits,
    float g00_r, float g00_i, float g01_r, float g01_i,
    float g10_r, float g10_i, float g11_r, float g11_i,
    size_t n_states)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t n_pairs = n_states / 2;
    if (idx >= n_pairs) return;

    // Find the pair of states that differ in qubit_idx
    uint32_t mask = 1U << qubit_idx;
    size_t lower_bits = idx & (mask - 1);
    size_t upper_bits = (idx >> qubit_idx) << (qubit_idx + 1);
    size_t state0 = lower_bits | upper_bits;
    size_t state1 = state0 | mask;

    // Get current amplitudes
    float a0_r = real[state0], a0_i = imag[state0];
    float a1_r = real[state1], a1_i = imag[state1];

    // Apply gate: [g00 g01; g10 g11] * [a0; a1]
    float new0_r, new0_i, new1_r, new1_i;
    float tmp_r, tmp_i;

    // new0 = g00 * a0 + g01 * a1
    device_complex_mul(g00_r, g00_i, a0_r, a0_i, &new0_r, &new0_i);
    device_complex_mul(g01_r, g01_i, a1_r, a1_i, &tmp_r, &tmp_i);
    new0_r += tmp_r;
    new0_i += tmp_i;

    // new1 = g10 * a0 + g11 * a1
    device_complex_mul(g10_r, g10_i, a0_r, a0_i, &new1_r, &new1_i);
    device_complex_mul(g11_r, g11_i, a1_r, a1_i, &tmp_r, &tmp_i);
    new1_r += tmp_r;
    new1_i += tmp_i;

    real[state0] = new0_r;
    imag[state0] = new0_i;
    real[state1] = new1_r;
    imag[state1] = new1_i;
}

/**
 * @brief Kernel to compute probabilities from amplitudes
 */
__global__ void kernel_compute_probabilities(
    const float* real, const float* imag, float* probs, size_t n_states)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_states) return;

    probs[idx] = device_complex_mag_sq(real[idx], imag[idx]);
}

/**
 * @brief Kernel for parallel prefix sum (for sampling)
 */
__global__ void kernel_cumsum(const float* input, float* output, size_t n)
{
    extern __shared__ float sdata[];

    size_t tid = threadIdx.x;
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    sdata[tid] = (idx < n) ? input[idx] : 0.0f;
    __syncthreads();

    // Up-sweep
    for (uint32_t stride = 1; stride < blockDim.x; stride *= 2) {
        uint32_t index = (tid + 1) * stride * 2 - 1;
        if (index < blockDim.x) {
            sdata[index] += sdata[index - stride];
        }
        __syncthreads();
    }

    // Down-sweep
    if (tid == 0) {
        output[blockDim.x - 1] = sdata[blockDim.x - 1];
        sdata[blockDim.x - 1] = 0.0f;
    }
    __syncthreads();

    for (uint32_t stride = blockDim.x / 2; stride > 0; stride /= 2) {
        uint32_t index = (tid + 1) * stride * 2 - 1;
        if (index < blockDim.x) {
            float t = sdata[index - stride];
            sdata[index - stride] = sdata[index];
            sdata[index] += t;
        }
        __syncthreads();
    }

    if (idx < n) {
        output[idx] = sdata[tid];
    }
}

//=============================================================================
// Quantum State API Implementation
//=============================================================================

nimcp_quantum_state_t* nimcp_quantum_state_create(
    nimcp_gpu_context_t* ctx,
    uint32_t n_qubits)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || n_qubits == 0 || n_qubits > 24) {
        LOG_ERROR("Invalid parameters: n_qubits=%u (max 24)", n_qubits);
        return NULL;
    }

    nimcp_quantum_state_t* state = (nimcp_quantum_state_t*)nimcp_calloc(1, sizeof(nimcp_quantum_state_t));
    if (!state) return NULL;

    state->n_qubits = n_qubits;
    state->n_states = 1U << n_qubits;

    size_t dims[] = {state->n_states};

    state->amplitudes_real = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    state->amplitudes_imag = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);

    if (!state->amplitudes_real || !state->amplitudes_imag) {
        nimcp_quantum_state_destroy(state);
        return NULL;
    }

    // Initialize to |0> state
    kernel_init_zero_state<<<GRID_SIZE(state->n_states), BLOCK_SIZE>>>(
        (float*)state->amplitudes_real->data,
        (float*)state->amplitudes_imag->data,
        state->n_states);

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        LOG_ERROR("CUDA error in state creation: %s", cudaGetErrorString(err));
        nimcp_quantum_state_destroy(state);
        return NULL;
    }

    LOG_DEBUG("Created quantum state with %u qubits (%u basis states)", n_qubits, state->n_states);
    return state;
}

void nimcp_quantum_state_destroy(nimcp_quantum_state_t* state)
{
    if (!state) return;

    nimcp_gpu_tensor_destroy(state->amplitudes_real);
    nimcp_gpu_tensor_destroy(state->amplitudes_imag);
    nimcp_free(state);
}

bool nimcp_quantum_state_hadamard_all(
    nimcp_gpu_context_t* ctx,
    nimcp_quantum_state_t* state)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state) return false;

    float norm_factor = 1.0f / sqrtf((float)state->n_states);

    kernel_hadamard_all<<<GRID_SIZE(state->n_states), BLOCK_SIZE>>>(
        (float*)state->amplitudes_real->data,
        (float*)state->amplitudes_imag->data,
        state->n_states,
        norm_factor);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_quantum_apply_gate(
    nimcp_gpu_context_t* ctx,
    nimcp_quantum_state_t* state,
    uint32_t qubit_idx,
    const float gate_real[2][2],
    const float gate_imag[2][2])
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || qubit_idx >= state->n_qubits) return false;

    size_t n_pairs = state->n_states / 2;

    kernel_single_qubit_gate<<<GRID_SIZE(n_pairs), BLOCK_SIZE>>>(
        (float*)state->amplitudes_real->data,
        (float*)state->amplitudes_imag->data,
        qubit_idx, state->n_qubits,
        gate_real[0][0], gate_imag[0][0], gate_real[0][1], gate_imag[0][1],
        gate_real[1][0], gate_imag[1][0], gate_real[1][1], gate_imag[1][1],
        state->n_states);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_quantum_compute_probabilities(
    nimcp_gpu_context_t* ctx,
    const nimcp_quantum_state_t* state,
    nimcp_gpu_tensor_t* probabilities)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !probabilities) return false;

    kernel_compute_probabilities<<<GRID_SIZE(state->n_states), BLOCK_SIZE>>>(
        (const float*)state->amplitudes_real->data,
        (const float*)state->amplitudes_imag->data,
        (float*)probabilities->data,
        state->n_states);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_quantum_measure(
    nimcp_gpu_context_t* ctx,
    nimcp_quantum_state_t* state,
    uint32_t* measured_state,
    float* probability)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state) return false;

    // Compute probabilities
    float* d_probs;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_probs, state->n_states * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);

    kernel_compute_probabilities<<<GRID_SIZE(state->n_states), BLOCK_SIZE>>>(
        (const float*)state->amplitudes_real->data,
        (const float*)state->amplitudes_imag->data,
        d_probs,
        state->n_states);

    // Copy probabilities to host for sampling
    float* h_probs = (float*)nimcp_malloc(state->n_states * sizeof(float));
    NIMCP_CUDA_RECOVER(cudaMemcpy(h_probs, d_probs, state->n_states * sizeof(float), cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);

    // Generate random number and sample
    float r = (float)rand() / (float)RAND_MAX;
    float cumsum = 0.0f;
    uint32_t result = 0;

    for (uint32_t i = 0; i < state->n_states; i++) {
        cumsum += h_probs[i];
        if (r <= cumsum) {
            result = i;
            break;
        }
    }

    // Return measured state if pointer provided
    if (measured_state) {
        *measured_state = result;
    }
    if (probability) {
        *probability = h_probs[result];
    }

    // Collapse state
    kernel_init_zero_state<<<GRID_SIZE(state->n_states), BLOCK_SIZE>>>(
        (float*)state->amplitudes_real->data,
        (float*)state->amplitudes_imag->data,
        state->n_states);

    // Set measured state amplitude to 1
    float one = 1.0f;
    NIMCP_CUDA_RECOVER(cudaMemcpy((float*)state->amplitudes_real->data + result, &one, sizeof(float), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);

    cudaFree(d_probs);
    nimcp_free(h_probs);

    return true;
}

//=============================================================================
// Grover's Algorithm - CUDA Kernels
//=============================================================================

/**
 * @brief Kernel to apply oracle (phase flip on marked states)
 */
__global__ void kernel_grover_oracle(
    float* real, float* imag,
    const uint32_t* marked_states, uint32_t n_marked,
    size_t n_states)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_states) return;

    // Check if this state is marked
    for (uint32_t i = 0; i < n_marked; i++) {
        if (idx == marked_states[i]) {
            real[idx] = -real[idx];
            imag[idx] = -imag[idx];
            return;
        }
    }
}

/**
 * @brief Kernel to compute mean amplitude for diffusion
 */
static __global__ void kernel_compute_mean(
    const float* real, const float* imag,
    float* mean_real, float* mean_imag, size_t n_states)
{
    extern __shared__ float sdata[];

    size_t tid = threadIdx.x;
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    // Load into shared memory
    sdata[tid] = (idx < n_states) ? real[idx] : 0.0f;
    sdata[tid + blockDim.x] = (idx < n_states) ? imag[idx] : 0.0f;
    __syncthreads();

    // Parallel reduction
    for (size_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            sdata[tid] += sdata[tid + s];
            sdata[tid + blockDim.x] += sdata[tid + blockDim.x + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        atomicAdd(mean_real, sdata[0]);
        atomicAdd(mean_imag, sdata[blockDim.x]);
    }
}

/**
 * @brief Kernel to apply diffusion: 2*mean - amplitude
 */
__global__ void kernel_grover_diffusion(
    float* real, float* imag,
    float mean_real, float mean_imag, size_t n_states)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_states) return;

    real[idx] = 2.0f * mean_real - real[idx];
    imag[idx] = 2.0f * mean_imag - imag[idx];
}

bool nimcp_grover_oracle(
    nimcp_gpu_context_t* ctx,
    nimcp_quantum_state_t* state,
    const uint32_t* marked_states,
    uint32_t n_marked)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !marked_states || n_marked == 0) return false;

    // Copy marked states to GPU
    uint32_t* d_marked;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_marked, n_marked * sizeof(uint32_t)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMemcpy(d_marked, marked_states, n_marked * sizeof(uint32_t), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);

    kernel_grover_oracle<<<GRID_SIZE(state->n_states), BLOCK_SIZE>>>(
        (float*)state->amplitudes_real->data,
        (float*)state->amplitudes_imag->data,
        d_marked, n_marked,
        state->n_states);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    cudaFree(d_marked);

    return true;
}

bool nimcp_grover_diffusion(
    nimcp_gpu_context_t* ctx,
    nimcp_quantum_state_t* state)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state) return false;

    // Compute mean amplitude
    float* d_mean_real;
    float* d_mean_imag;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_mean_real, sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_mean_imag, sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMemset(d_mean_real, 0, sizeof(float)), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemset(d_mean_imag, 0, sizeof(float)), GPU_ERROR_CUDA_RUNTIME);

    kernel_compute_mean<<<GRID_SIZE(state->n_states), BLOCK_SIZE, 2 * BLOCK_SIZE * sizeof(float)>>>(
        (const float*)state->amplitudes_real->data,
        (const float*)state->amplitudes_imag->data,
        d_mean_real, d_mean_imag,
        state->n_states);

    float h_mean_real, h_mean_imag;
    NIMCP_CUDA_RECOVER(cudaMemcpy(&h_mean_real, d_mean_real, sizeof(float), cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpy(&h_mean_imag, d_mean_imag, sizeof(float), cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);

    h_mean_real /= (float)state->n_states;
    h_mean_imag /= (float)state->n_states;

    // Apply diffusion
    kernel_grover_diffusion<<<GRID_SIZE(state->n_states), BLOCK_SIZE>>>(
        (float*)state->amplitudes_real->data,
        (float*)state->amplitudes_imag->data,
        h_mean_real, h_mean_imag,
        state->n_states);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    cudaFree(d_mean_real);
    cudaFree(d_mean_imag);

    return true;
}

bool nimcp_grover_iteration(
    nimcp_gpu_context_t* ctx,
    nimcp_quantum_state_t* state,
    const uint32_t* marked_states,
    uint32_t n_marked)
{
    if (!nimcp_grover_oracle(ctx, state, marked_states, n_marked)) {
        return false;
    }
    return nimcp_grover_diffusion(ctx, state);
}

bool nimcp_grover_search(
    nimcp_gpu_context_t* ctx,
    const nimcp_grover_config_t* config,
    uint32_t* found_state,
    bool* success)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !config || !config->marked_states || !found_state || !success) return false;

    // Create quantum state
    nimcp_quantum_state_t* state = nimcp_quantum_state_create(ctx, config->n_qubits);
    if (!state) return false;

    // Initialize to uniform superposition
    if (!nimcp_quantum_state_hadamard_all(ctx, state)) {
        nimcp_quantum_state_destroy(state);
        return false;
    }

    // Run Grover iterations
    uint32_t iterations = config->optimal_iterations;
    LOG_DEBUG("Running %u Grover iterations for %u-qubit search", iterations, config->n_qubits);

    for (uint32_t i = 0; i < iterations; i++) {
        if (!nimcp_grover_iteration(ctx, state, config->marked_states, config->n_marked)) {
            nimcp_quantum_state_destroy(state);
            return false;
        }
    }

    // Measure result
    float prob;
    if (!nimcp_quantum_measure(ctx, state, found_state, &prob)) {
        nimcp_quantum_state_destroy(state);
        return false;
    }

    // Check if found state is marked
    *success = false;
    for (uint32_t i = 0; i < config->n_marked; i++) {
        if (*found_state == config->marked_states[i]) {
            *success = true;
            break;
        }
    }

    LOG_DEBUG("Grover search result: state=%u, success=%d, probability=%.4f",
              *found_state, *success, prob);

    nimcp_quantum_state_destroy(state);
    return true;
}

//=============================================================================
// Quantum Annealing - CUDA Kernels
//=============================================================================

/**
 * @brief Kernel to compute Ising energy
 */
__global__ void kernel_ising_energy(
    const float* J, const float* h, const float* spins,
    float* partial_energy, uint32_t n_spins)
{
    extern __shared__ float sdata[];

    size_t tid = threadIdx.x;
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    float local_energy = 0.0f;

    if (idx < n_spins) {
        // Local field contribution
        local_energy -= h[idx] * spins[idx];

        // Coupling contribution (only upper triangle to avoid double counting)
        for (uint32_t j = idx + 1; j < n_spins; j++) {
            local_energy -= J[idx * n_spins + j] * spins[idx] * spins[j];
        }
    }

    sdata[tid] = local_energy;
    __syncthreads();

    // Parallel reduction
    for (size_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            sdata[tid] += sdata[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        atomicAdd(partial_energy, sdata[0]);
    }
}

/**
 * @brief Kernel for single spin flip with Metropolis criterion
 */
__global__ void kernel_metropolis_update(
    const float* J, const float* h, float* spins,
    float temperature, curandState* rng_states, uint32_t n_spins)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_spins) return;

    curandState local_state = rng_states[idx];

    // Compute energy change for flipping spin idx
    float delta_E = 2.0f * h[idx] * spins[idx];

    for (uint32_t j = 0; j < n_spins; j++) {
        if (j != idx) {
            delta_E += 2.0f * J[idx * n_spins + j] * spins[idx] * spins[j];
        }
    }

    // Metropolis criterion
    float r = curand_uniform(&local_state);
    if (delta_E < 0.0f || r < expf(-delta_E / temperature)) {
        spins[idx] = -spins[idx];
    }

    rng_states[idx] = local_state;
}

/**
 * @brief Kernel to initialize RNG states
 */
__global__ void kernel_init_rng(curandState* states, unsigned long seed, uint32_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    curand_init(seed, idx, 0, &states[idx]);
}

/**
 * @brief Kernel for transverse field contribution (simplified SQA)
 */
__global__ void kernel_transverse_field_update(
    float* spins, float transverse_field,
    curandState* rng_states, uint32_t n_spins)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_spins) return;

    curandState local_state = rng_states[idx];

    // Quantum tunneling probability
    float tunnel_prob = transverse_field / (1.0f + transverse_field);
    float r = curand_uniform(&local_state);

    if (r < tunnel_prob) {
        // Flip spin with probability proportional to transverse field
        spins[idx] = -spins[idx];
    }

    rng_states[idx] = local_state;
}

//=============================================================================
// Ising Model API Implementation
//=============================================================================

nimcp_ising_model_t* nimcp_ising_model_create(
    nimcp_gpu_context_t* ctx,
    uint32_t n_spins)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || n_spins == 0) return NULL;

    nimcp_ising_model_t* model = (nimcp_ising_model_t*)nimcp_calloc(1, sizeof(nimcp_ising_model_t));
    if (!model) return NULL;

    model->n_spins = n_spins;

    size_t dims_1d[] = {n_spins};
    size_t dims_2d[] = {n_spins, n_spins};

    model->J = nimcp_gpu_tensor_create(ctx, dims_2d, 2, NIMCP_GPU_PRECISION_FP32);
    model->h = nimcp_gpu_tensor_create(ctx, dims_1d, 1, NIMCP_GPU_PRECISION_FP32);
    model->spins = nimcp_gpu_tensor_create(ctx, dims_1d, 1, NIMCP_GPU_PRECISION_FP32);

    if (!model->J || !model->h || !model->spins) {
        nimcp_ising_model_destroy(model);
        return NULL;
    }

    // Initialize spins randomly to +1 or -1
    float* h_spins = (float*)nimcp_malloc(n_spins * sizeof(float));
    for (uint32_t i = 0; i < n_spins; i++) {
        h_spins[i] = (rand() % 2) * 2.0f - 1.0f;  // Random +1 or -1
    }
    cudaMemcpy(model->spins->data, h_spins, n_spins * sizeof(float), cudaMemcpyHostToDevice);
    nimcp_free(h_spins);

    model->energy = 0.0f;

    LOG_DEBUG("Created Ising model with %u spins", n_spins);
    return model;
}

void nimcp_ising_model_destroy(nimcp_ising_model_t* model)
{
    if (!model) return;

    nimcp_gpu_tensor_destroy(model->J);
    nimcp_gpu_tensor_destroy(model->h);
    nimcp_gpu_tensor_destroy(model->spins);
    nimcp_free(model);
}

bool nimcp_ising_model_set_params(
    nimcp_gpu_context_t* ctx,
    nimcp_ising_model_t* model,
    const float* J,
    const float* h)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !model || !J || !h) return false;

    NIMCP_CUDA_RECOVER(cudaMemcpy(model->J->data, J,
                          model->n_spins * model->n_spins * sizeof(float),
                          cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpy(model->h->data, h,
                          model->n_spins * sizeof(float),
                          cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);

    return true;
}

float nimcp_ising_compute_energy(
    nimcp_gpu_context_t* ctx,
    nimcp_ising_model_t* model)
{
    if (!ctx || !model) return 0.0f;

    float* d_energy;
    cudaMalloc(&d_energy, sizeof(float));
    cudaMemset(d_energy, 0, sizeof(float));

    kernel_ising_energy<<<GRID_SIZE(model->n_spins), BLOCK_SIZE, BLOCK_SIZE * sizeof(float)>>>(
        (const float*)model->J->data,
        (const float*)model->h->data,
        (const float*)model->spins->data,
        d_energy,
        model->n_spins);

    float h_energy;
    cudaMemcpy(&h_energy, d_energy, sizeof(float), cudaMemcpyDeviceToHost);
    cudaFree(d_energy);

    model->energy = h_energy;
    return h_energy;
}

bool nimcp_annealing_step(
    nimcp_gpu_context_t* ctx,
    nimcp_ising_model_t* model,
    float temperature,
    float transverse_field)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !model) return false;

    // Initialize RNG states (should be done once, but simplified here)
    static curandState* d_rng_states = NULL;
    static uint32_t rng_n = 0;

    if (!d_rng_states || rng_n != model->n_spins) {
        if (d_rng_states) cudaFree(d_rng_states);
        cudaMalloc(&d_rng_states, model->n_spins * sizeof(curandState));
        kernel_init_rng<<<GRID_SIZE(model->n_spins), BLOCK_SIZE>>>(
            d_rng_states, (unsigned long)time(NULL), model->n_spins);
        rng_n = model->n_spins;
    }

    // Transverse field update (quantum tunneling)
    if (transverse_field > 0.0f) {
        kernel_transverse_field_update<<<GRID_SIZE(model->n_spins), BLOCK_SIZE>>>(
            (float*)model->spins->data,
            transverse_field,
            d_rng_states,
            model->n_spins);
    }

    // Metropolis update
    kernel_metropolis_update<<<GRID_SIZE(model->n_spins), BLOCK_SIZE>>>(
        (const float*)model->J->data,
        (const float*)model->h->data,
        (float*)model->spins->data,
        temperature,
        d_rng_states,
        model->n_spins);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

float nimcp_quantum_anneal(
    nimcp_gpu_context_t* ctx,
    nimcp_ising_model_t* model,
    const nimcp_annealing_config_t* config)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !model || !config) return 0.0f;

    float T = config->T_initial;
    float gamma = config->transverse_field_initial;

    float T_ratio = powf(config->T_final / config->T_initial, 1.0f / config->n_steps);
    float gamma_ratio = powf(config->transverse_field_final / config->transverse_field_initial,
                              1.0f / config->n_steps);

    LOG_DEBUG("Starting quantum annealing: T=%.2f->%.2f, gamma=%.2f->%.2f, steps=%u",
              config->T_initial, config->T_final,
              config->transverse_field_initial, config->transverse_field_final,
              config->n_steps);

    for (uint32_t step = 0; step < config->n_steps; step++) {
        float curr_T, curr_gamma;

        if (config->use_schedule && config->schedule) {
            float s = config->schedule[step];
            curr_T = config->T_initial * (1.0f - s) + config->T_final * s;
            curr_gamma = config->transverse_field_initial * (1.0f - s) +
                        config->transverse_field_final * s;
        } else {
            curr_T = T;
            curr_gamma = gamma;
            T *= T_ratio;
            gamma *= gamma_ratio;
        }

        if (!nimcp_annealing_step(ctx, model, curr_T, curr_gamma)) {
            LOG_ERROR("Annealing step %u failed", step);
            break;
        }
    }

    float final_energy = nimcp_ising_compute_energy(ctx, model);
    LOG_DEBUG("Annealing complete: final energy = %.4f", final_energy);

    return final_energy;
}

float nimcp_pimc_anneal(
    nimcp_gpu_context_t* ctx,
    nimcp_ising_model_t* model,
    const nimcp_annealing_config_t* config,
    uint32_t n_trotter)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !model || !config) return 0.0f;

    // Simplified PIMC: use multiple Trotter slices
    LOG_DEBUG("PIMC annealing with %u Trotter slices", n_trotter);

    // For now, run standard annealing multiple times and average
    float total_energy = 0.0f;

    for (uint32_t t = 0; t < n_trotter; t++) {
        // Reinitialize spins randomly
        float* h_spins = (float*)nimcp_malloc(model->n_spins * sizeof(float));
        for (uint32_t i = 0; i < model->n_spins; i++) {
            h_spins[i] = (rand() % 2) * 2.0f - 1.0f;
        }
        cudaMemcpy(model->spins->data, h_spins, model->n_spins * sizeof(float), cudaMemcpyHostToDevice);
        nimcp_free(h_spins);

        float energy = nimcp_quantum_anneal(ctx, model, config);
        total_energy += energy;
    }

    return total_energy / n_trotter;
}

//=============================================================================
// Variational Quantum Circuit Utilities
//=============================================================================

bool nimcp_vqc_init_params(
    nimcp_gpu_context_t* ctx,
    uint32_t n_qubits,
    uint32_t n_layers,
    nimcp_gpu_tensor_t* params)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !params) return false;

    size_t n_params = n_layers * n_qubits * 3;  // 3 rotation angles per qubit per layer

    // Initialize with random small values
    float* h_params = (float*)nimcp_malloc(n_params * sizeof(float));
    for (size_t i = 0; i < n_params; i++) {
        h_params[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;  // Small random values
    }

    NIMCP_CUDA_RECOVER(cudaMemcpy(params->data, h_params, n_params * sizeof(float), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);
    nimcp_free(h_params);

    return true;
}

bool nimcp_vqc_apply_layer(
    nimcp_gpu_context_t* ctx,
    nimcp_quantum_state_t* state,
    const nimcp_gpu_tensor_t* params)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !params) return false;

    // Copy parameters to host for gate construction
    float* h_params = (float*)nimcp_malloc(state->n_qubits * 3 * sizeof(float));
    NIMCP_CUDA_RECOVER(cudaMemcpy(h_params, params->data, state->n_qubits * 3 * sizeof(float), cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);

    // Apply Rx, Ry, Rz to each qubit
    for (uint32_t q = 0; q < state->n_qubits; q++) {
        float theta_x = h_params[q * 3 + 0];
        float theta_y = h_params[q * 3 + 1];
        float theta_z = h_params[q * 3 + 2];

        // Rx gate
        float rx_real[2][2] = {
            {cosf(theta_x / 2.0f), 0.0f},
            {0.0f, cosf(theta_x / 2.0f)}
        };
        float rx_imag[2][2] = {
            {0.0f, -sinf(theta_x / 2.0f)},
            {-sinf(theta_x / 2.0f), 0.0f}
        };

        nimcp_quantum_apply_gate(ctx, state, q, rx_real, rx_imag);

        // Ry gate
        float ry_real[2][2] = {
            {cosf(theta_y / 2.0f), -sinf(theta_y / 2.0f)},
            {sinf(theta_y / 2.0f), cosf(theta_y / 2.0f)}
        };
        float ry_imag[2][2] = {{0.0f, 0.0f}, {0.0f, 0.0f}};

        nimcp_quantum_apply_gate(ctx, state, q, ry_real, ry_imag);

        // Rz gate
        float rz_real[2][2] = {
            {cosf(theta_z / 2.0f), 0.0f},
            {0.0f, cosf(theta_z / 2.0f)}
        };
        float rz_imag[2][2] = {
            {-sinf(theta_z / 2.0f), 0.0f},
            {0.0f, sinf(theta_z / 2.0f)}
        };

        nimcp_quantum_apply_gate(ctx, state, q, rz_real, rz_imag);
    }

    nimcp_free(h_params);
    return true;
}

bool nimcp_vqc_parameter_shift_gradient(
    nimcp_gpu_context_t* ctx,
    nimcp_quantum_state_t* state,
    const nimcp_gpu_tensor_t* params,
    const nimcp_gpu_tensor_t* observable,
    nimcp_gpu_tensor_t* gradients)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !params || !observable || !gradients) return false;

    // Simplified parameter shift: gradient = (f(theta + pi/2) - f(theta - pi/2)) / 2
    // This requires re-running the circuit for each parameter, so it's expensive
    LOG_WARN("Parameter shift gradient not fully implemented - returning zeros");

    // Zero out gradients for now
    NIMCP_CUDA_RECOVER(cudaMemset(gradients->data, 0, gradients->numel * sizeof(float)), GPU_ERROR_CUDA_RUNTIME);

    return true;
}

#else // !NIMCP_ENABLE_CUDA

//=============================================================================
// CPU Implementation (when CUDA is not available)
//=============================================================================

#include "gpu/quantum/nimcp_quantum_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define LOG_MODULE "QUANTUM_CPU"


//-----------------------------------------------------------------------------
// Default Configurations
//-----------------------------------------------------------------------------

nimcp_grover_config_t nimcp_grover_default_config(uint32_t n_qubits)
{
    nimcp_grover_config_t config;
    config.n_qubits = n_qubits;
    config.marked_states = NULL;
    config.n_marked = 0;  // User must set marked states
    config.optimal_iterations = nimcp_grover_optimal_iterations(n_qubits, 1);
    config.success_probability = 0.0f;
    return config;
}

nimcp_annealing_config_t nimcp_annealing_default_config(uint32_t n_spins)
{
    nimcp_annealing_config_t config;
    config.n_spins = n_spins;
    config.T_initial = 10.0f;
    config.T_final = 0.01f;
    config.n_steps = 1000;
    config.transverse_field_initial = 5.0f;
    config.transverse_field_final = 0.01f;
    config.use_schedule = false;
    config.schedule = NULL;
    return config;
}

uint32_t nimcp_grover_optimal_iterations(uint32_t n_qubits, uint32_t n_marked)
{
    uint64_t N = 1ULL << n_qubits;
    float ratio = (float)N / (float)n_marked;
    float iterations = (M_PI / 4.0f) * sqrtf(ratio);
    return (uint32_t)roundf(iterations);
}

//-----------------------------------------------------------------------------
// Quantum State Operations (CPU)
//-----------------------------------------------------------------------------

nimcp_quantum_state_t* nimcp_quantum_state_create(
    nimcp_gpu_context_t* ctx,
    uint32_t n_qubits)
{
    if (!ctx || n_qubits == 0 || n_qubits > 20) {  // Limit for CPU
        LOG_ERROR("Invalid n_qubits=%u (CPU max 20)", n_qubits);
        return NULL;
    }
    (void)ctx;  // Context not used in CPU fallback but required for API consistency

    nimcp_quantum_state_t* state = (nimcp_quantum_state_t*)nimcp_calloc(1, sizeof(nimcp_quantum_state_t));
    if (!state) return NULL;

    state->n_qubits = n_qubits;
    state->n_states = 1U << n_qubits;

    // Allocate CPU tensors (using simple wrapper)
    state->amplitudes_real = (nimcp_gpu_tensor_t*)nimcp_calloc(1, sizeof(nimcp_gpu_tensor_t));
    state->amplitudes_imag = (nimcp_gpu_tensor_t*)nimcp_calloc(1, sizeof(nimcp_gpu_tensor_t));

    if (!state->amplitudes_real || !state->amplitudes_imag) {
        nimcp_quantum_state_destroy(state);
        return NULL;
    }

    state->amplitudes_real->data = nimcp_calloc(state->n_states, sizeof(float));
    state->amplitudes_imag->data = nimcp_calloc(state->n_states, sizeof(float));
    state->amplitudes_real->numel = state->n_states;
    state->amplitudes_imag->numel = state->n_states;

    if (!state->amplitudes_real->data || !state->amplitudes_imag->data) {
        nimcp_quantum_state_destroy(state);
        return NULL;
    }

    // Initialize to |0>
    ((float*)state->amplitudes_real->data)[0] = 1.0f;

    LOG_DEBUG("Created CPU quantum state with %u qubits", n_qubits);
    return state;
}

void nimcp_quantum_state_destroy(nimcp_quantum_state_t* state)
{
    if (!state) return;

    if (state->amplitudes_real) {
        nimcp_free(state->amplitudes_real->data);
        nimcp_free(state->amplitudes_real);
    }
    if (state->amplitudes_imag) {
        nimcp_free(state->amplitudes_imag->data);
        nimcp_free(state->amplitudes_imag);
    }
    nimcp_free(state);
}

bool nimcp_quantum_state_hadamard_all(
    nimcp_gpu_context_t* ctx,
    nimcp_quantum_state_t* state)
{
    if (!ctx || !state) return false;
    (void)ctx;  // API consistency

    float* real = (float*)state->amplitudes_real->data;
    float* imag = (float*)state->amplitudes_imag->data;
    float norm = 1.0f / sqrtf((float)state->n_states);

    for (uint32_t i = 0; i < state->n_states; i++) {
        real[i] = norm;
        imag[i] = 0.0f;
    }

    return true;
}

bool nimcp_quantum_apply_gate(
    nimcp_gpu_context_t* ctx,
    nimcp_quantum_state_t* state,
    uint32_t qubit_idx,
    const float gate_real[2][2],
    const float gate_imag[2][2])
{
    if (!ctx || !state || qubit_idx >= state->n_qubits) return false;
    (void)ctx;  // API consistency

    float* real = (float*)state->amplitudes_real->data;
    float* imag = (float*)state->amplitudes_imag->data;

    uint32_t mask = 1U << qubit_idx;
    uint32_t n_pairs = state->n_states / 2;

    for (uint32_t idx = 0; idx < n_pairs; idx++) {
        uint32_t lower_bits = idx & (mask - 1);
        uint32_t upper_bits = (idx >> qubit_idx) << (qubit_idx + 1);
        uint32_t state0 = lower_bits | upper_bits;
        uint32_t state1 = state0 | mask;

        float a0_r = real[state0], a0_i = imag[state0];
        float a1_r = real[state1], a1_i = imag[state1];

        // new0 = g00 * a0 + g01 * a1
        float new0_r = gate_real[0][0] * a0_r - gate_imag[0][0] * a0_i +
                       gate_real[0][1] * a1_r - gate_imag[0][1] * a1_i;
        float new0_i = gate_real[0][0] * a0_i + gate_imag[0][0] * a0_r +
                       gate_real[0][1] * a1_i + gate_imag[0][1] * a1_r;

        // new1 = g10 * a0 + g11 * a1
        float new1_r = gate_real[1][0] * a0_r - gate_imag[1][0] * a0_i +
                       gate_real[1][1] * a1_r - gate_imag[1][1] * a1_i;
        float new1_i = gate_real[1][0] * a0_i + gate_imag[1][0] * a0_r +
                       gate_real[1][1] * a1_i + gate_imag[1][1] * a1_r;

        real[state0] = new0_r;
        imag[state0] = new0_i;
        real[state1] = new1_r;
        imag[state1] = new1_i;
    }

    return true;
}

bool nimcp_quantum_compute_probabilities(
    nimcp_gpu_context_t* ctx,
    const nimcp_quantum_state_t* state,
    nimcp_gpu_tensor_t* probabilities)
{
    (void)ctx;
    if (!state || !probabilities) return false;

    float* real = (float*)state->amplitudes_real->data;
    float* imag = (float*)state->amplitudes_imag->data;
    float* probs = (float*)probabilities->data;

    for (uint32_t i = 0; i < state->n_states; i++) {
        probs[i] = real[i] * real[i] + imag[i] * imag[i];
    }

    return true;
}

bool nimcp_quantum_measure(
    nimcp_gpu_context_t* ctx,
    nimcp_quantum_state_t* state,
    uint32_t* measured_state,
    float* probability)
{
    (void)ctx;
    if (!state) return false;

    float* real = (float*)state->amplitudes_real->data;
    float* imag = (float*)state->amplitudes_imag->data;

    // Compute probabilities and sample
    float r = (float)rand() / (float)RAND_MAX;
    float cumsum = 0.0f;
    uint32_t result = 0;
    float result_prob = 0.0f;

    for (uint32_t i = 0; i < state->n_states; i++) {
        float prob = real[i] * real[i] + imag[i] * imag[i];
        cumsum += prob;
        if (r <= cumsum) {
            result = i;
            result_prob = prob;
            break;
        }
    }

    // Return measured state if pointer provided
    if (measured_state) {
        *measured_state = result;
    }
    if (probability) {
        *probability = result_prob;
    }

    // Collapse state
    memset(real, 0, state->n_states * sizeof(float));
    memset(imag, 0, state->n_states * sizeof(float));
    real[result] = 1.0f;

    return true;
}

//-----------------------------------------------------------------------------
// Grover's Algorithm (CPU)
//-----------------------------------------------------------------------------

bool nimcp_grover_oracle(
    nimcp_gpu_context_t* ctx,
    nimcp_quantum_state_t* state,
    const uint32_t* marked_states,
    uint32_t n_marked)
{
    (void)ctx;
    if (!state || !marked_states) return false;

    float* real = (float*)state->amplitudes_real->data;
    float* imag = (float*)state->amplitudes_imag->data;

    for (uint32_t m = 0; m < n_marked; m++) {
        uint32_t idx = marked_states[m];
        if (idx < state->n_states) {
            real[idx] = -real[idx];
            imag[idx] = -imag[idx];
        }
    }

    return true;
}

bool nimcp_grover_diffusion(
    nimcp_gpu_context_t* ctx,
    nimcp_quantum_state_t* state)
{
    (void)ctx;
    if (!state) return false;

    float* real = (float*)state->amplitudes_real->data;
    float* imag = (float*)state->amplitudes_imag->data;

    // Compute mean
    float mean_real = 0.0f, mean_imag = 0.0f;
    for (uint32_t i = 0; i < state->n_states; i++) {
        mean_real += real[i];
        mean_imag += imag[i];
    }
    mean_real /= (float)state->n_states;
    mean_imag /= (float)state->n_states;

    // Apply diffusion: 2*mean - amplitude
    for (uint32_t i = 0; i < state->n_states; i++) {
        real[i] = 2.0f * mean_real - real[i];
        imag[i] = 2.0f * mean_imag - imag[i];
    }

    return true;
}

bool nimcp_grover_iteration(
    nimcp_gpu_context_t* ctx,
    nimcp_quantum_state_t* state,
    const uint32_t* marked_states,
    uint32_t n_marked)
{
    if (!nimcp_grover_oracle(ctx, state, marked_states, n_marked)) return false;
    return nimcp_grover_diffusion(ctx, state);
}

bool nimcp_grover_search(
    nimcp_gpu_context_t* ctx,
    const nimcp_grover_config_t* config,
    uint32_t* found_state,
    bool* success)
{
    if (!config || !config->marked_states || !found_state || !success) return false;

    nimcp_quantum_state_t* state = nimcp_quantum_state_create(ctx, config->n_qubits);
    if (!state) return false;

    nimcp_quantum_state_hadamard_all(ctx, state);

    for (uint32_t i = 0; i < config->optimal_iterations; i++) {
        nimcp_grover_iteration(ctx, state, config->marked_states, config->n_marked);
    }

    float prob;
    nimcp_quantum_measure(ctx, state, found_state, &prob);

    *success = false;
    for (uint32_t i = 0; i < config->n_marked; i++) {
        if (*found_state == config->marked_states[i]) {
            *success = true;
            break;
        }
    }

    nimcp_quantum_state_destroy(state);
    return true;
}

//-----------------------------------------------------------------------------
// Quantum Annealing (CPU)
//-----------------------------------------------------------------------------

nimcp_ising_model_t* nimcp_ising_model_create(
    nimcp_gpu_context_t* ctx,
    uint32_t n_spins)
{
    (void)ctx;
    if (n_spins == 0) return NULL;

    nimcp_ising_model_t* model = (nimcp_ising_model_t*)nimcp_calloc(1, sizeof(nimcp_ising_model_t));
    if (!model) return NULL;

    model->n_spins = n_spins;

    model->J = (nimcp_gpu_tensor_t*)nimcp_calloc(1, sizeof(nimcp_gpu_tensor_t));
    model->h = (nimcp_gpu_tensor_t*)nimcp_calloc(1, sizeof(nimcp_gpu_tensor_t));
    model->spins = (nimcp_gpu_tensor_t*)nimcp_calloc(1, sizeof(nimcp_gpu_tensor_t));

    if (!model->J || !model->h || !model->spins) {
        nimcp_ising_model_destroy(model);
        return NULL;
    }

    model->J->data = nimcp_calloc(n_spins * n_spins, sizeof(float));
    model->h->data = nimcp_calloc(n_spins, sizeof(float));
    model->spins->data = nimcp_malloc(n_spins * sizeof(float));

    model->J->numel = n_spins * n_spins;
    model->h->numel = n_spins;
    model->spins->numel = n_spins;

    // Initialize spins randomly
    float* spins = (float*)model->spins->data;
    for (uint32_t i = 0; i < n_spins; i++) {
        spins[i] = (rand() % 2) * 2.0f - 1.0f;
    }

    return model;
}

void nimcp_ising_model_destroy(nimcp_ising_model_t* model)
{
    if (!model) return;

    if (model->J) { nimcp_free(model->J->data); nimcp_free(model->J); }
    if (model->h) { nimcp_free(model->h->data); nimcp_free(model->h); }
    if (model->spins) { nimcp_free(model->spins->data); nimcp_free(model->spins); }
    nimcp_free(model);
}

bool nimcp_ising_model_set_params(
    nimcp_gpu_context_t* ctx,
    nimcp_ising_model_t* model,
    const float* J,
    const float* h)
{
    if (!ctx || !model || !J || !h) return false;

    memcpy(model->J->data, J, model->n_spins * model->n_spins * sizeof(float));
    memcpy(model->h->data, h, model->n_spins * sizeof(float));
    return true;
}

float nimcp_ising_compute_energy(
    nimcp_gpu_context_t* ctx,
    nimcp_ising_model_t* model)
{
    (void)ctx;
    if (!model) return 0.0f;

    float* J = (float*)model->J->data;
    float* h = (float*)model->h->data;
    float* spins = (float*)model->spins->data;
    uint32_t n = model->n_spins;

    float energy = 0.0f;

    for (uint32_t i = 0; i < n; i++) {
        energy -= h[i] * spins[i];
        for (uint32_t j = i + 1; j < n; j++) {
            energy -= J[i * n + j] * spins[i] * spins[j];
        }
    }

    model->energy = energy;
    return energy;
}

bool nimcp_annealing_step(
    nimcp_gpu_context_t* ctx,
    nimcp_ising_model_t* model,
    float temperature,
    float transverse_field)
{
    (void)ctx;
    if (!model) return false;

    float* J = (float*)model->J->data;
    float* h = (float*)model->h->data;
    float* spins = (float*)model->spins->data;
    uint32_t n = model->n_spins;

    // Transverse field tunneling
    if (transverse_field > 0.0f) {
        float tunnel_prob = transverse_field / (1.0f + transverse_field);
        for (uint32_t i = 0; i < n; i++) {
            if ((float)rand() / RAND_MAX < tunnel_prob) {
                spins[i] = -spins[i];
            }
        }
    }

    // Metropolis updates
    for (uint32_t i = 0; i < n; i++) {
        float delta_E = 2.0f * h[i] * spins[i];
        for (uint32_t j = 0; j < n; j++) {
            if (j != i) {
                delta_E += 2.0f * J[i * n + j] * spins[i] * spins[j];
            }
        }

        if (delta_E < 0.0f || (float)rand() / RAND_MAX < expf(-delta_E / temperature)) {
            spins[i] = -spins[i];
        }
    }

    return true;
}

float nimcp_quantum_anneal(
    nimcp_gpu_context_t* ctx,
    nimcp_ising_model_t* model,
    const nimcp_annealing_config_t* config)
{
    if (!model || !config) return 0.0f;

    float T = config->T_initial;
    float gamma = config->transverse_field_initial;

    float T_ratio = powf(config->T_final / config->T_initial, 1.0f / config->n_steps);
    float gamma_ratio = powf(config->transverse_field_final / config->transverse_field_initial,
                              1.0f / config->n_steps);

    for (uint32_t step = 0; step < config->n_steps; step++) {
        float curr_T = T;
        float curr_gamma = gamma;

        if (config->use_schedule && config->schedule) {
            float s = config->schedule[step];
            curr_T = config->T_initial * (1.0f - s) + config->T_final * s;
            curr_gamma = config->transverse_field_initial * (1.0f - s) +
                        config->transverse_field_final * s;
        }

        nimcp_annealing_step(ctx, model, curr_T, curr_gamma);

        T *= T_ratio;
        gamma *= gamma_ratio;
    }

    return nimcp_ising_compute_energy(ctx, model);
}

float nimcp_pimc_anneal(
    nimcp_gpu_context_t* ctx,
    nimcp_ising_model_t* model,
    const nimcp_annealing_config_t* config,
    uint32_t n_trotter)
{
    if (!model || !config) return 0.0f;

    float total_energy = 0.0f;

    for (uint32_t t = 0; t < n_trotter; t++) {
        // Reinitialize spins
        float* spins = (float*)model->spins->data;
        for (uint32_t i = 0; i < model->n_spins; i++) {
            spins[i] = (rand() % 2) * 2.0f - 1.0f;
        }

        total_energy += nimcp_quantum_anneal(ctx, model, config);
    }

    return total_energy / n_trotter;
}

//-----------------------------------------------------------------------------
// VQC Utilities (CPU)
//-----------------------------------------------------------------------------

bool nimcp_vqc_init_params(
    nimcp_gpu_context_t* ctx,
    uint32_t n_qubits,
    uint32_t n_layers,
    nimcp_gpu_tensor_t* params)
{
    (void)ctx;
    if (!params) return false;

    size_t n_params = n_layers * n_qubits * 3;
    float* p = (float*)params->data;

    for (size_t i = 0; i < n_params; i++) {
        p[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
    }

    return true;
}

bool nimcp_vqc_apply_layer(
    nimcp_gpu_context_t* ctx,
    nimcp_quantum_state_t* state,
    const nimcp_gpu_tensor_t* params)
{
    if (!state || !params) return false;

    float* p = (float*)params->data;

    for (uint32_t q = 0; q < state->n_qubits; q++) {
        float theta_x = p[q * 3 + 0];
        float theta_y = p[q * 3 + 1];
        float theta_z = p[q * 3 + 2];

        // Rx gate
        float rx_real[2][2] = {
            {cosf(theta_x / 2.0f), 0.0f},
            {0.0f, cosf(theta_x / 2.0f)}
        };
        float rx_imag[2][2] = {
            {0.0f, -sinf(theta_x / 2.0f)},
            {-sinf(theta_x / 2.0f), 0.0f}
        };
        nimcp_quantum_apply_gate(ctx, state, q, rx_real, rx_imag);

        // Ry gate
        float ry_real[2][2] = {
            {cosf(theta_y / 2.0f), -sinf(theta_y / 2.0f)},
            {sinf(theta_y / 2.0f), cosf(theta_y / 2.0f)}
        };
        float ry_imag[2][2] = {{0.0f, 0.0f}, {0.0f, 0.0f}};
        nimcp_quantum_apply_gate(ctx, state, q, ry_real, ry_imag);

        // Rz gate
        float rz_real[2][2] = {
            {cosf(theta_z / 2.0f), 0.0f},
            {0.0f, cosf(theta_z / 2.0f)}
        };
        float rz_imag[2][2] = {
            {-sinf(theta_z / 2.0f), 0.0f},
            {0.0f, sinf(theta_z / 2.0f)}
        };
        nimcp_quantum_apply_gate(ctx, state, q, rz_real, rz_imag);
    }

    return true;
}

bool nimcp_vqc_parameter_shift_gradient(
    nimcp_gpu_context_t* ctx,
    nimcp_quantum_state_t* state,
    const nimcp_gpu_tensor_t* params,
    const nimcp_gpu_tensor_t* observable,
    nimcp_gpu_tensor_t* gradients)
{
    (void)ctx;
    (void)state;
    (void)params;
    (void)observable;

    if (!gradients) return false;

    memset(gradients->data, 0, gradients->numel * sizeof(float));
    return true;
}

#endif // NIMCP_ENABLE_CUDA
