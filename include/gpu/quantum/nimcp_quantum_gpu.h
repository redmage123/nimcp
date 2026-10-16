//=============================================================================
// nimcp_quantum_gpu.h - GPU Quantum Algorithm Kernels
//=============================================================================
/**
 * @file nimcp_quantum_gpu.h
 * @brief GPU-accelerated quantum-inspired algorithm operations
 *
 * WHAT: CUDA kernels for quantum-inspired neural network operations
 * WHY:  Enables GPU acceleration for quantum algorithms (Grover, annealing)
 * HOW:  Custom kernels for quantum state evolution, amplitude manipulation
 *
 * INTEGRATION:
 * - Simulates quantum operations on classical GPU hardware
 * - Used for quantum-inspired optimization in NIMCP
 * - Supports Grover's search and quantum annealing
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_QUANTUM_GPU_H
#define NIMCP_QUANTUM_GPU_H

#include "gpu/context/nimcp_gpu_context.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include <stdbool.h>
#include <stdint.h>
#include <complex.h>

//=============================================================================
// Quantum State Types
//=============================================================================

/**
 * @brief Complex amplitude type (float precision)
 */
typedef struct {
    float real;
    float imag;
} nimcp_complex_t;

/**
 * @brief Quantum state configuration
 */
typedef struct {
    uint32_t n_qubits;              /**< Number of qubits */
    uint32_t n_states;              /**< Number of basis states (2^n_qubits) */
    nimcp_gpu_tensor_t* amplitudes_real; /**< Real parts [n_states] */
    nimcp_gpu_tensor_t* amplitudes_imag; /**< Imaginary parts [n_states] */
} nimcp_quantum_state_t;

/**
 * @brief Grover's algorithm configuration
 */
typedef struct {
    uint32_t n_qubits;              /**< Number of qubits */
    uint32_t* marked_states;        /**< Array of marked state indices */
    uint32_t n_marked;              /**< Number of marked states */
    uint32_t optimal_iterations;    /**< Optimal number of iterations */
    float success_probability;      /**< Expected success probability */
} nimcp_grover_config_t;

/**
 * @brief Quantum annealing configuration
 */
typedef struct {
    uint32_t n_spins;               /**< Number of spins (qubits) */
    float T_initial;                /**< Initial temperature */
    float T_final;                  /**< Final temperature */
    uint32_t n_steps;               /**< Number of annealing steps */
    float transverse_field_initial; /**< Initial transverse field strength */
    float transverse_field_final;   /**< Final transverse field strength */
    bool use_schedule;              /**< Use custom annealing schedule */
    float* schedule;                /**< Custom schedule [n_steps] (NULL for linear) */
} nimcp_annealing_config_t;

/**
 * @brief Ising model for quantum annealing
 */
typedef struct {
    uint32_t n_spins;               /**< Number of spins */
    nimcp_gpu_tensor_t* J;          /**< Coupling matrix [n_spins, n_spins] */
    nimcp_gpu_tensor_t* h;          /**< Local fields [n_spins] */
    nimcp_gpu_tensor_t* spins;      /**< Current spin configuration [n_spins] */
    float energy;                   /**< Current energy */
} nimcp_ising_model_t;

//=============================================================================
// Quantum State Operations
//=============================================================================

/**
 * @brief Create quantum state initialized to |0...0>
 *
 * @param ctx GPU context
 * @param n_qubits Number of qubits
 * @return Quantum state structure, or NULL on failure
 */
NIMCP_EXPORT nimcp_quantum_state_t* nimcp_quantum_state_create(
    nimcp_gpu_context_t* ctx,
    uint32_t n_qubits
);

/**
 * @brief Destroy quantum state
 *
 * @param state State to destroy
 */
NIMCP_EXPORT void nimcp_quantum_state_destroy(nimcp_quantum_state_t* state);

/**
 * @brief Initialize quantum state to uniform superposition
 *
 * |psi> = (1/sqrt(N)) * sum_i |i>
 *
 * @param ctx GPU context
 * @param state Quantum state to initialize
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_quantum_state_hadamard_all(
    nimcp_gpu_context_t* ctx,
    nimcp_quantum_state_t* state
);

/**
 * @brief Apply single-qubit gate
 *
 * @param ctx GPU context
 * @param state Quantum state
 * @param qubit_idx Target qubit index
 * @param gate_real Gate matrix real parts [2][2]
 * @param gate_imag Gate matrix imaginary parts [2][2]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_quantum_apply_gate(
    nimcp_gpu_context_t* ctx,
    nimcp_quantum_state_t* state,
    uint32_t qubit_idx,
    const float gate_real[2][2],
    const float gate_imag[2][2]
);

/**
 * @brief Measure quantum state (collapse to classical)
 *
 * @param ctx GPU context
 * @param state Quantum state
 * @param measured_state Output: measured basis state index
 * @param probability Output: probability of measured state
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_quantum_measure(
    nimcp_gpu_context_t* ctx,
    nimcp_quantum_state_t* state,
    uint32_t* measured_state,
    float* probability
);

/**
 * @brief Compute state probabilities without collapse
 *
 * @param ctx GPU context
 * @param state Quantum state
 * @param probabilities Output: probability distribution [n_states]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_quantum_compute_probabilities(
    nimcp_gpu_context_t* ctx,
    const nimcp_quantum_state_t* state,
    nimcp_gpu_tensor_t* probabilities
);

//=============================================================================
// Grover's Algorithm
//=============================================================================

/**
 * @brief Calculate optimal number of Grover iterations
 *
 * @param n_qubits Number of qubits
 * @param n_marked Number of marked states
 * @return Optimal iteration count
 */
NIMCP_EXPORT uint32_t nimcp_grover_optimal_iterations(
    uint32_t n_qubits,
    uint32_t n_marked
);

/**
 * @brief Apply Grover oracle (phase flip on marked states)
 *
 * Applies: |x> -> (-1)^f(x) |x> where f(x) = 1 for marked states
 *
 * @param ctx GPU context
 * @param state Quantum state
 * @param marked_states Array of marked state indices
 * @param n_marked Number of marked states
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_grover_oracle(
    nimcp_gpu_context_t* ctx,
    nimcp_quantum_state_t* state,
    const uint32_t* marked_states,
    uint32_t n_marked
);

/**
 * @brief Apply Grover diffusion operator
 *
 * Applies: D = 2|s><s| - I where |s> is uniform superposition
 *
 * @param ctx GPU context
 * @param state Quantum state
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_grover_diffusion(
    nimcp_gpu_context_t* ctx,
    nimcp_quantum_state_t* state
);

/**
 * @brief Perform single Grover iteration (oracle + diffusion)
 *
 * @param ctx GPU context
 * @param state Quantum state
 * @param marked_states Array of marked state indices
 * @param n_marked Number of marked states
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_grover_iteration(
    nimcp_gpu_context_t* ctx,
    nimcp_quantum_state_t* state,
    const uint32_t* marked_states,
    uint32_t n_marked
);

/**
 * @brief Run complete Grover's algorithm
 *
 * @param ctx GPU context
 * @param config Grover configuration
 * @param found_state Output: found state index
 * @param success Output: whether search succeeded
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_grover_search(
    nimcp_gpu_context_t* ctx,
    const nimcp_grover_config_t* config,
    uint32_t* found_state,
    bool* success
);

//=============================================================================
// Quantum Annealing
//=============================================================================

/**
 * @brief Create Ising model for annealing
 *
 * @param ctx GPU context
 * @param n_spins Number of spins
 * @return Ising model structure, or NULL on failure
 */
NIMCP_EXPORT nimcp_ising_model_t* nimcp_ising_model_create(
    nimcp_gpu_context_t* ctx,
    uint32_t n_spins
);

/**
 * @brief Destroy Ising model
 *
 * @param model Model to destroy
 */
NIMCP_EXPORT void nimcp_ising_model_destroy(nimcp_ising_model_t* model);

/**
 * @brief Set Ising model parameters
 *
 * @param ctx GPU context
 * @param model Ising model
 * @param J Coupling matrix [n_spins, n_spins]
 * @param h Local fields [n_spins]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_ising_model_set_params(
    nimcp_gpu_context_t* ctx,
    nimcp_ising_model_t* model,
    const float* J,
    const float* h
);

/**
 * @brief Compute Ising model energy
 *
 * E = -sum_{i<j} J_ij * s_i * s_j - sum_i h_i * s_i
 *
 * @param ctx GPU context
 * @param model Ising model
 * @return Current energy
 */
NIMCP_EXPORT float nimcp_ising_compute_energy(
    nimcp_gpu_context_t* ctx,
    nimcp_ising_model_t* model
);

/**
 * @brief Perform single annealing step
 *
 * @param ctx GPU context
 * @param model Ising model
 * @param temperature Current temperature
 * @param transverse_field Transverse field strength
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_annealing_step(
    nimcp_gpu_context_t* ctx,
    nimcp_ising_model_t* model,
    float temperature,
    float transverse_field
);

/**
 * @brief Run complete quantum annealing
 *
 * @param ctx GPU context
 * @param model Ising model
 * @param config Annealing configuration
 * @return Final energy
 */
NIMCP_EXPORT float nimcp_quantum_anneal(
    nimcp_gpu_context_t* ctx,
    nimcp_ising_model_t* model,
    const nimcp_annealing_config_t* config
);

/**
 * @brief Simulated quantum annealing with transverse field
 *
 * Uses path-integral Monte Carlo for quantum effects
 *
 * @param ctx GPU context
 * @param model Ising model
 * @param config Annealing configuration
 * @param n_trotter Number of Trotter slices
 * @return Final energy
 */
NIMCP_EXPORT float nimcp_pimc_anneal(
    nimcp_gpu_context_t* ctx,
    nimcp_ising_model_t* model,
    const nimcp_annealing_config_t* config,
    uint32_t n_trotter
);

//=============================================================================
// Quantum Neural Network Utilities
//=============================================================================

/**
 * @brief Initialize variational quantum circuit parameters
 *
 * @param ctx GPU context
 * @param n_qubits Number of qubits
 * @param n_layers Number of variational layers
 * @param params Output: parameter tensor [n_layers * n_qubits * 3]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_vqc_init_params(
    nimcp_gpu_context_t* ctx,
    uint32_t n_qubits,
    uint32_t n_layers,
    nimcp_gpu_tensor_t* params
);

/**
 * @brief Apply variational quantum circuit layer
 *
 * @param ctx GPU context
 * @param state Quantum state
 * @param params Layer parameters [n_qubits * 3]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_vqc_apply_layer(
    nimcp_gpu_context_t* ctx,
    nimcp_quantum_state_t* state,
    const nimcp_gpu_tensor_t* params
);

/**
 * @brief Compute quantum gradient via parameter shift rule
 *
 * @param ctx GPU context
 * @param state Quantum state
 * @param params Circuit parameters
 * @param observable Observable to measure
 * @param gradients Output: parameter gradients
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_vqc_parameter_shift_gradient(
    nimcp_gpu_context_t* ctx,
    nimcp_quantum_state_t* state,
    const nimcp_gpu_tensor_t* params,
    const nimcp_gpu_tensor_t* observable,
    nimcp_gpu_tensor_t* gradients
);

//=============================================================================
// Default Configurations
//=============================================================================

/**
 * @brief Get default Grover configuration
 *
 * @param n_qubits Number of qubits
 * @return Default configuration
 */
NIMCP_EXPORT nimcp_grover_config_t nimcp_grover_default_config(uint32_t n_qubits);

/**
 * @brief Get default annealing configuration
 *
 * @param n_spins Number of spins
 * @return Default configuration
 */
NIMCP_EXPORT nimcp_annealing_config_t nimcp_annealing_default_config(uint32_t n_spins);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_QUANTUM_GPU_H
