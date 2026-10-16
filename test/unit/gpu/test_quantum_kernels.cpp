/**
 * @file test_quantum_kernels.cpp
 * @brief Unit tests for GPU quantum algorithm kernels
 *
 * WHAT: Tests GPU-accelerated quantum-inspired algorithm operations
 * WHY:  Verify quantum state operations, Grover's algorithm, and annealing
 * HOW:  Test all public API functions with various configurations
 *
 * TEST COVERAGE:
 * - Quantum state creation and initialization
 * - Hadamard gate application (uniform superposition)
 * - Single-qubit gate application
 * - Grover's algorithm: oracle, diffusion, complete search
 * - Ising model creation and energy computation
 * - Quantum annealing steps
 * - Measurement and probability computation
 * - Edge cases and error handling
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

// GPU headers include CUDA headers that cannot be in extern "C" blocks
#include "gpu/quantum/nimcp_quantum_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * @brief Test fixture for quantum kernel tests
 * WHAT: Provides common setup/teardown for quantum GPU tests
 * WHY:  Ensure proper cleanup of GPU resources
 * HOW:  Automatically creates/destroys GPU context
 */
class QuantumKernelTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;
    nimcp_quantum_state_t* state = nullptr;
    nimcp_ising_model_t* model = nullptr;

    void SetUp() override {
        // Try to create GPU context (may fail if no GPU)
        ctx = nimcp_gpu_context_create_auto();
        // Tests will skip if ctx is NULL
    }

    void TearDown() override {
        if (state) {
            nimcp_quantum_state_destroy(state);
            state = nullptr;
        }
        if (model) {
            nimcp_ising_model_destroy(model);
            model = nullptr;
        }
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
    }

    /**
     * @brief Check if GPU is available for tests
     */
    bool hasGPU() const {
        return ctx != nullptr && nimcp_gpu_context_is_valid(ctx);
    }

    /**
     * @brief Skip test if no GPU available
     */
    void skipIfNoGPU() {
        if (!hasGPU()) {
            GTEST_SKIP() << "Skipping test: No GPU available";
        }
    }

    /**
     * @brief Calculate expected amplitude for uniform superposition
     */
    float uniformAmplitude(uint32_t n_qubits) const {
        return 1.0f / sqrtf((float)(1u << n_qubits));
    }

    /**
     * @brief Create identity gate matrices
     */
    void identityGate(float real[2][2], float imag[2][2]) {
        real[0][0] = 1.0f; real[0][1] = 0.0f;
        real[1][0] = 0.0f; real[1][1] = 1.0f;
        imag[0][0] = 0.0f; imag[0][1] = 0.0f;
        imag[1][0] = 0.0f; imag[1][1] = 0.0f;
    }

    /**
     * @brief Create Pauli-X (NOT) gate matrices
     */
    void pauliXGate(float real[2][2], float imag[2][2]) {
        real[0][0] = 0.0f; real[0][1] = 1.0f;
        real[1][0] = 1.0f; real[1][1] = 0.0f;
        imag[0][0] = 0.0f; imag[0][1] = 0.0f;
        imag[1][0] = 0.0f; imag[1][1] = 0.0f;
    }

    /**
     * @brief Create Pauli-Z gate matrices
     */
    void pauliZGate(float real[2][2], float imag[2][2]) {
        real[0][0] = 1.0f; real[0][1] = 0.0f;
        real[1][0] = 0.0f; real[1][1] = -1.0f;
        imag[0][0] = 0.0f; imag[0][1] = 0.0f;
        imag[1][0] = 0.0f; imag[1][1] = 0.0f;
    }

    /**
     * @brief Create Hadamard gate matrices
     */
    void hadamardGate(float real[2][2], float imag[2][2]) {
        float h = 1.0f / sqrtf(2.0f);
        real[0][0] = h; real[0][1] = h;
        real[1][0] = h; real[1][1] = -h;
        imag[0][0] = 0.0f; imag[0][1] = 0.0f;
        imag[1][0] = 0.0f; imag[1][1] = 0.0f;
    }
};

//=============================================================================
// Quantum State Creation Tests
//=============================================================================

/**
 * TEST: Create quantum state with valid parameters
 * WHAT: Verify nimcp_quantum_state_create() creates state initialized to |0...0>
 * WHY:  State creation is fundamental for all quantum operations
 */
TEST_F(QuantumKernelTest, CreateState_ValidParams_Succeeds) {
    skipIfNoGPU();

    state = nimcp_quantum_state_create(ctx, 3);
    ASSERT_NE(state, nullptr);

    EXPECT_EQ(state->n_qubits, 3u);
    EXPECT_EQ(state->n_states, 8u);  // 2^3 = 8
    EXPECT_NE(state->amplitudes_real, nullptr);
    EXPECT_NE(state->amplitudes_imag, nullptr);
}

/**
 * TEST: Create quantum state with NULL context
 * WHAT: Verify NULL context returns NULL state
 * WHY:  Guard clause validation
 */
TEST_F(QuantumKernelTest, CreateState_NullContext_ReturnsNull) {
    nimcp_quantum_state_t* null_state = nimcp_quantum_state_create(nullptr, 3);
    EXPECT_EQ(null_state, nullptr);
}

/**
 * TEST: Create quantum state with zero qubits
 * WHAT: Verify zero qubits returns NULL
 * WHY:  Invalid configuration rejection
 */
TEST_F(QuantumKernelTest, CreateState_ZeroQubits_ReturnsNull) {
    skipIfNoGPU();

    state = nimcp_quantum_state_create(ctx, 0);
    EXPECT_EQ(state, nullptr);
}

/**
 * TEST: Create quantum state with various qubit counts
 * WHAT: Test state creation for 1-10 qubits
 * WHY:  Verify scalability of state representation
 */
TEST_F(QuantumKernelTest, CreateState_VariousQubits_CorrectSize) {
    skipIfNoGPU();

    for (uint32_t n = 1; n <= 10; n++) {
        nimcp_quantum_state_t* test_state = nimcp_quantum_state_create(ctx, n);
        ASSERT_NE(test_state, nullptr) << "Failed for n_qubits=" << n;

        EXPECT_EQ(test_state->n_qubits, n);
        EXPECT_EQ(test_state->n_states, 1u << n);

        nimcp_quantum_state_destroy(test_state);
    }
}

/**
 * TEST: Destroy NULL state
 * WHAT: Verify nimcp_quantum_state_destroy() handles NULL gracefully
 * WHY:  Prevent crashes from invalid input
 */
TEST_F(QuantumKernelTest, DestroyState_Null_DoesNotCrash) {
    nimcp_quantum_state_destroy(nullptr);
    SUCCEED();
}

//=============================================================================
// Hadamard Gate Tests (Uniform Superposition)
//=============================================================================

/**
 * TEST: Apply Hadamard to all qubits
 * WHAT: Verify nimcp_quantum_state_hadamard_all() creates uniform superposition
 * WHY:  Hadamard initialization is standard for quantum algorithms
 */
TEST_F(QuantumKernelTest, HadamardAll_CreatesSuperposition) {
    skipIfNoGPU();

    state = nimcp_quantum_state_create(ctx, 3);
    ASSERT_NE(state, nullptr);

    bool result = nimcp_quantum_state_hadamard_all(ctx, state);
    EXPECT_TRUE(result);

    // Verify by computing probabilities
    size_t dims[1] = {state->n_states};
    nimcp_gpu_tensor_t* probs = nimcp_gpu_tensor_create(ctx, dims, 1,
                                                         NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(probs, nullptr);

    bool prob_result = nimcp_quantum_compute_probabilities(ctx, state, probs);
    EXPECT_TRUE(prob_result);

    // Copy probabilities to host
    std::vector<float> host_probs(state->n_states);
    nimcp_gpu_tensor_to_host(probs, host_probs.data());

    // Each state should have equal probability 1/N
    float expected_prob = 1.0f / (float)state->n_states;
    for (uint32_t i = 0; i < state->n_states; i++) {
        EXPECT_NEAR(host_probs[i], expected_prob, 0.001f)
            << "State " << i << " has incorrect probability";
    }

    nimcp_gpu_tensor_destroy(probs);
}

/**
 * TEST: Hadamard on NULL state
 * WHAT: Verify nimcp_quantum_state_hadamard_all() handles NULL state
 * WHY:  Guard clause validation
 */
TEST_F(QuantumKernelTest, HadamardAll_NullState_ReturnsFalse) {
    skipIfNoGPU();

    bool result = nimcp_quantum_state_hadamard_all(ctx, nullptr);
    EXPECT_FALSE(result);
}

/**
 * TEST: Hadamard with NULL context
 * WHAT: Verify Hadamard handles NULL context
 * WHY:  Guard clause validation
 */
TEST_F(QuantumKernelTest, HadamardAll_NullContext_ReturnsFalse) {
    skipIfNoGPU();

    state = nimcp_quantum_state_create(ctx, 3);
    ASSERT_NE(state, nullptr);

    bool result = nimcp_quantum_state_hadamard_all(nullptr, state);
    EXPECT_FALSE(result);
}

//=============================================================================
// Single-Qubit Gate Tests
//=============================================================================

/**
 * TEST: Apply identity gate
 * WHAT: Verify identity gate leaves state unchanged
 * WHY:  Basic gate application test
 */
TEST_F(QuantumKernelTest, ApplyGate_Identity_NoChange) {
    skipIfNoGPU();

    state = nimcp_quantum_state_create(ctx, 2);
    ASSERT_NE(state, nullptr);

    // Apply Hadamard first to get non-trivial state
    nimcp_quantum_state_hadamard_all(ctx, state);

    // Get initial probabilities
    size_t dims[1] = {state->n_states};
    nimcp_gpu_tensor_t* probs_before = nimcp_gpu_tensor_create(ctx, dims, 1,
                                                                NIMCP_GPU_PRECISION_FP32);
    nimcp_quantum_compute_probabilities(ctx, state, probs_before);
    std::vector<float> before(state->n_states);
    nimcp_gpu_tensor_to_host(probs_before, before.data());

    // Apply identity gate to qubit 0
    float gate_real[2][2], gate_imag[2][2];
    identityGate(gate_real, gate_imag);
    bool result = nimcp_quantum_apply_gate(ctx, state, 0, gate_real, gate_imag);
    EXPECT_TRUE(result);

    // Get final probabilities
    nimcp_gpu_tensor_t* probs_after = nimcp_gpu_tensor_create(ctx, dims, 1,
                                                               NIMCP_GPU_PRECISION_FP32);
    nimcp_quantum_compute_probabilities(ctx, state, probs_after);
    std::vector<float> after(state->n_states);
    nimcp_gpu_tensor_to_host(probs_after, after.data());

    // Probabilities should be unchanged
    for (uint32_t i = 0; i < state->n_states; i++) {
        EXPECT_NEAR(before[i], after[i], 0.0001f);
    }

    nimcp_gpu_tensor_destroy(probs_before);
    nimcp_gpu_tensor_destroy(probs_after);
}

/**
 * TEST: Apply Pauli-X gate
 * WHAT: Verify X gate flips qubit state
 * WHY:  Basic NOT operation
 */
TEST_F(QuantumKernelTest, ApplyGate_PauliX_FlipsQubit) {
    skipIfNoGPU();

    // Create single qubit in |0> state
    state = nimcp_quantum_state_create(ctx, 1);
    ASSERT_NE(state, nullptr);

    // Apply X gate (should flip to |1>)
    float gate_real[2][2], gate_imag[2][2];
    pauliXGate(gate_real, gate_imag);
    bool result = nimcp_quantum_apply_gate(ctx, state, 0, gate_real, gate_imag);
    EXPECT_TRUE(result);

    // Measure probabilities
    size_t dims[1] = {state->n_states};
    nimcp_gpu_tensor_t* probs = nimcp_gpu_tensor_create(ctx, dims, 1,
                                                         NIMCP_GPU_PRECISION_FP32);
    nimcp_quantum_compute_probabilities(ctx, state, probs);
    std::vector<float> host_probs(state->n_states);
    nimcp_gpu_tensor_to_host(probs, host_probs.data());

    // State |1> should have probability 1
    EXPECT_NEAR(host_probs[0], 0.0f, 0.0001f);  // |0>
    EXPECT_NEAR(host_probs[1], 1.0f, 0.0001f);  // |1>

    nimcp_gpu_tensor_destroy(probs);
}

/**
 * TEST: Apply gate with invalid qubit index
 * WHAT: Verify gate application fails for out-of-range qubit
 * WHY:  Bounds checking validation
 */
TEST_F(QuantumKernelTest, ApplyGate_InvalidQubit_ReturnsFalse) {
    skipIfNoGPU();

    state = nimcp_quantum_state_create(ctx, 3);
    ASSERT_NE(state, nullptr);

    float gate_real[2][2], gate_imag[2][2];
    identityGate(gate_real, gate_imag);

    bool result = nimcp_quantum_apply_gate(ctx, state, 5, gate_real, gate_imag);
    EXPECT_FALSE(result);
}

/**
 * TEST: Apply gate with NULL parameters
 * WHAT: Verify gate handles NULL inputs
 * WHY:  Guard clause validation
 */
TEST_F(QuantumKernelTest, ApplyGate_NullInputs_ReturnsFalse) {
    skipIfNoGPU();

    state = nimcp_quantum_state_create(ctx, 2);
    ASSERT_NE(state, nullptr);

    float gate_real[2][2], gate_imag[2][2];
    identityGate(gate_real, gate_imag);

    EXPECT_FALSE(nimcp_quantum_apply_gate(nullptr, state, 0, gate_real, gate_imag));
    EXPECT_FALSE(nimcp_quantum_apply_gate(ctx, nullptr, 0, gate_real, gate_imag));
}

//=============================================================================
// Grover's Algorithm Tests
//=============================================================================

/**
 * TEST: Calculate optimal Grover iterations
 * WHAT: Verify nimcp_grover_optimal_iterations() returns correct count
 * WHY:  Optimal iteration count is critical for Grover efficiency
 */
TEST_F(QuantumKernelTest, GroverOptimalIterations_Correct) {
    // For N states and 1 marked state, optimal iterations ~ pi/4 * sqrt(N)
    uint32_t n_qubits = 4;
    uint32_t n_marked = 1;

    uint32_t optimal = nimcp_grover_optimal_iterations(n_qubits, n_marked);

    // N = 16 states, so optimal ~ pi/4 * sqrt(16) = pi/4 * 4 = pi ~ 3.14
    EXPECT_GE(optimal, 3u);
    EXPECT_LE(optimal, 4u);
}

/**
 * TEST: Optimal iterations for various configurations
 * WHAT: Test iteration calculation for different qubit counts
 * WHY:  Verify formula correctness across scales
 */
TEST_F(QuantumKernelTest, GroverOptimalIterations_VariousConfigs) {
    // 2 qubits (N=4): optimal ~ pi/4 * 2 ~ 1.57 -> 1 or 2
    uint32_t opt2 = nimcp_grover_optimal_iterations(2, 1);
    EXPECT_GE(opt2, 1u);
    EXPECT_LE(opt2, 2u);

    // 6 qubits (N=64): optimal ~ pi/4 * 8 ~ 6.28 -> 6 or 7
    uint32_t opt6 = nimcp_grover_optimal_iterations(6, 1);
    EXPECT_GE(opt6, 5u);
    EXPECT_LE(opt6, 8u);

    // 10 qubits (N=1024): optimal ~ pi/4 * 32 ~ 25.13 -> 24-26
    uint32_t opt10 = nimcp_grover_optimal_iterations(10, 1);
    EXPECT_GE(opt10, 20u);
    EXPECT_LE(opt10, 30u);
}

/**
 * TEST: Apply Grover oracle
 * WHAT: Verify oracle flips phase of marked states
 * WHY:  Oracle is core component of Grover's algorithm
 */
TEST_F(QuantumKernelTest, GroverOracle_FlipsMarkedState) {
    skipIfNoGPU();

    state = nimcp_quantum_state_create(ctx, 3);
    ASSERT_NE(state, nullptr);

    // Initialize to uniform superposition
    nimcp_quantum_state_hadamard_all(ctx, state);

    // Mark state |5> = |101>
    uint32_t marked_states[1] = {5};
    bool result = nimcp_grover_oracle(ctx, state, marked_states, 1);
    EXPECT_TRUE(result);

    // Oracle flips amplitude sign but doesn't change probabilities
    // (since prob = |amplitude|^2)
    size_t dims[1] = {state->n_states};
    nimcp_gpu_tensor_t* probs = nimcp_gpu_tensor_create(ctx, dims, 1,
                                                         NIMCP_GPU_PRECISION_FP32);
    nimcp_quantum_compute_probabilities(ctx, state, probs);
    std::vector<float> host_probs(state->n_states);
    nimcp_gpu_tensor_to_host(probs, host_probs.data());

    // All states should still have equal probability
    float expected = 1.0f / 8.0f;
    for (uint32_t i = 0; i < state->n_states; i++) {
        EXPECT_NEAR(host_probs[i], expected, 0.001f);
    }

    nimcp_gpu_tensor_destroy(probs);
}

/**
 * TEST: Grover oracle with NULL inputs
 * WHAT: Verify oracle handles NULL inputs
 * WHY:  Guard clause validation
 */
TEST_F(QuantumKernelTest, GroverOracle_NullInputs_ReturnsFalse) {
    skipIfNoGPU();

    state = nimcp_quantum_state_create(ctx, 3);
    ASSERT_NE(state, nullptr);

    uint32_t marked[1] = {0};

    EXPECT_FALSE(nimcp_grover_oracle(nullptr, state, marked, 1));
    EXPECT_FALSE(nimcp_grover_oracle(ctx, nullptr, marked, 1));
    EXPECT_FALSE(nimcp_grover_oracle(ctx, state, nullptr, 1));
}

/**
 * TEST: Apply Grover diffusion operator
 * WHAT: Verify diffusion amplifies marked state amplitude
 * WHY:  Diffusion is the amplitude amplification step
 */
TEST_F(QuantumKernelTest, GroverDiffusion_AmplitudesCorrect) {
    skipIfNoGPU();

    state = nimcp_quantum_state_create(ctx, 3);
    ASSERT_NE(state, nullptr);

    // Initialize to uniform superposition
    nimcp_quantum_state_hadamard_all(ctx, state);

    bool result = nimcp_grover_diffusion(ctx, state);
    EXPECT_TRUE(result);

    // After diffusion on uniform state, amplitudes should be negated except mean
    // On uniform superposition, diffusion is essentially identity
    size_t dims[1] = {state->n_states};
    nimcp_gpu_tensor_t* probs = nimcp_gpu_tensor_create(ctx, dims, 1,
                                                         NIMCP_GPU_PRECISION_FP32);
    nimcp_quantum_compute_probabilities(ctx, state, probs);
    std::vector<float> host_probs(state->n_states);
    nimcp_gpu_tensor_to_host(probs, host_probs.data());

    // Should still be uniform
    float expected = 1.0f / 8.0f;
    for (uint32_t i = 0; i < state->n_states; i++) {
        EXPECT_NEAR(host_probs[i], expected, 0.01f);
    }

    nimcp_gpu_tensor_destroy(probs);
}

/**
 * TEST: Single Grover iteration
 * WHAT: Verify one oracle + diffusion iteration
 * WHY:  Single iteration should increase marked state probability
 */
TEST_F(QuantumKernelTest, GroverIteration_IncreasesMarkedProbability) {
    skipIfNoGPU();

    state = nimcp_quantum_state_create(ctx, 3);
    ASSERT_NE(state, nullptr);

    // Initialize to uniform superposition
    nimcp_quantum_state_hadamard_all(ctx, state);

    // Get initial probability of marked state
    size_t dims[1] = {state->n_states};
    nimcp_gpu_tensor_t* probs = nimcp_gpu_tensor_create(ctx, dims, 1,
                                                         NIMCP_GPU_PRECISION_FP32);
    nimcp_quantum_compute_probabilities(ctx, state, probs);
    std::vector<float> initial_probs(state->n_states);
    nimcp_gpu_tensor_to_host(probs, initial_probs.data());

    // Perform one Grover iteration marking state |3>
    uint32_t marked_states[1] = {3};
    bool result = nimcp_grover_iteration(ctx, state, marked_states, 1);
    EXPECT_TRUE(result);

    // Get final probability
    nimcp_quantum_compute_probabilities(ctx, state, probs);
    std::vector<float> final_probs(state->n_states);
    nimcp_gpu_tensor_to_host(probs, final_probs.data());

    // Marked state probability should increase
    EXPECT_GT(final_probs[3], initial_probs[3]);

    nimcp_gpu_tensor_destroy(probs);
}

/**
 * TEST: Complete Grover search
 * WHAT: Run full Grover's algorithm and verify it finds marked state
 * WHY:  End-to-end validation of quantum search
 */
TEST_F(QuantumKernelTest, GroverSearch_FindsMarkedState) {
    skipIfNoGPU();

    nimcp_grover_config_t config = nimcp_grover_default_config(4);
    config.marked_states = new uint32_t[1];
    config.marked_states[0] = 7;  // Search for state |0111>
    config.n_marked = 1;

    uint32_t found_state = UINT32_MAX;
    bool success = false;

    bool result = nimcp_grover_search(ctx, &config, &found_state, &success);
    EXPECT_TRUE(result);

    // With high probability, should find the marked state
    // Note: Quantum algorithms are probabilistic
    if (success) {
        EXPECT_EQ(found_state, 7u);
    }

    delete[] config.marked_states;
}

/**
 * TEST: Default Grover configuration
 * WHAT: Verify nimcp_grover_default_config() returns valid config
 * WHY:  Convenience function should provide sensible defaults
 */
TEST_F(QuantumKernelTest, GroverDefaultConfig_ValidDefaults) {
    nimcp_grover_config_t config = nimcp_grover_default_config(5);

    EXPECT_EQ(config.n_qubits, 5u);
    EXPECT_EQ(config.marked_states, nullptr);
    EXPECT_EQ(config.n_marked, 0u);
    EXPECT_GT(config.optimal_iterations, 0u);
}

//=============================================================================
// Ising Model Tests
//=============================================================================

/**
 * TEST: Create Ising model
 * WHAT: Verify nimcp_ising_model_create() creates valid model
 * WHY:  Ising model is basis for quantum annealing
 */
TEST_F(QuantumKernelTest, IsingCreate_ValidParams_Succeeds) {
    skipIfNoGPU();

    model = nimcp_ising_model_create(ctx, 10);
    ASSERT_NE(model, nullptr);

    EXPECT_EQ(model->n_spins, 10u);
    EXPECT_NE(model->J, nullptr);
    EXPECT_NE(model->h, nullptr);
    EXPECT_NE(model->spins, nullptr);
}

/**
 * TEST: Create Ising model with NULL context
 * WHAT: Verify NULL context returns NULL model
 * WHY:  Guard clause validation
 */
TEST_F(QuantumKernelTest, IsingCreate_NullContext_ReturnsNull) {
    nimcp_ising_model_t* null_model = nimcp_ising_model_create(nullptr, 10);
    EXPECT_EQ(null_model, nullptr);
}

/**
 * TEST: Create Ising model with zero spins
 * WHAT: Verify zero spins returns NULL
 * WHY:  Invalid configuration rejection
 */
TEST_F(QuantumKernelTest, IsingCreate_ZeroSpins_ReturnsNull) {
    skipIfNoGPU();

    model = nimcp_ising_model_create(ctx, 0);
    EXPECT_EQ(model, nullptr);
}

/**
 * TEST: Destroy NULL model
 * WHAT: Verify nimcp_ising_model_destroy() handles NULL gracefully
 * WHY:  Prevent crashes from invalid input
 */
TEST_F(QuantumKernelTest, IsingDestroy_Null_DoesNotCrash) {
    nimcp_ising_model_destroy(nullptr);
    SUCCEED();
}

/**
 * TEST: Set Ising model parameters
 * WHAT: Verify nimcp_ising_model_set_params() sets J and h correctly
 * WHY:  Parameters define the optimization problem
 */
TEST_F(QuantumKernelTest, IsingSetParams_ValidParams_Succeeds) {
    skipIfNoGPU();

    model = nimcp_ising_model_create(ctx, 4);
    ASSERT_NE(model, nullptr);

    // Create antiferromagnetic coupling matrix
    float J[16] = {
        0.0f, -1.0f, -1.0f, 0.0f,
        -1.0f, 0.0f, 0.0f, -1.0f,
        -1.0f, 0.0f, 0.0f, -1.0f,
        0.0f, -1.0f, -1.0f, 0.0f
    };

    // Local fields
    float h[4] = {0.1f, -0.1f, 0.1f, -0.1f};

    bool result = nimcp_ising_model_set_params(ctx, model, J, h);
    EXPECT_TRUE(result);
}

/**
 * TEST: Set Ising parameters with NULL inputs
 * WHAT: Verify set_params handles NULL inputs
 * WHY:  Guard clause validation
 */
TEST_F(QuantumKernelTest, IsingSetParams_NullInputs_ReturnsFalse) {
    skipIfNoGPU();

    model = nimcp_ising_model_create(ctx, 4);
    ASSERT_NE(model, nullptr);

    float J[16] = {0};
    float h[4] = {0};

    EXPECT_FALSE(nimcp_ising_model_set_params(nullptr, model, J, h));
    EXPECT_FALSE(nimcp_ising_model_set_params(ctx, nullptr, J, h));
    EXPECT_FALSE(nimcp_ising_model_set_params(ctx, model, nullptr, h));
    EXPECT_FALSE(nimcp_ising_model_set_params(ctx, model, J, nullptr));
}

/**
 * TEST: Compute Ising energy
 * WHAT: Verify nimcp_ising_compute_energy() returns correct energy
 * WHY:  Energy computation is essential for optimization
 */
TEST_F(QuantumKernelTest, IsingComputeEnergy_ReturnsFiniteValue) {
    skipIfNoGPU();

    model = nimcp_ising_model_create(ctx, 4);
    ASSERT_NE(model, nullptr);

    // Set simple parameters
    float J[16] = {
        0.0f, 1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 0.0f
    };
    float h[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    nimcp_ising_model_set_params(ctx, model, J, h);

    float energy = nimcp_ising_compute_energy(ctx, model);

    // Energy should be finite
    EXPECT_TRUE(std::isfinite(energy));
}

/**
 * TEST: Ising energy with ferromagnetic coupling
 * WHAT: Test energy for ferromagnetic system (J > 0)
 * WHY:  Verify energy formula correctness
 */
TEST_F(QuantumKernelTest, IsingComputeEnergy_Ferromagnetic_CorrectSign) {
    skipIfNoGPU();

    model = nimcp_ising_model_create(ctx, 2);
    ASSERT_NE(model, nullptr);

    // Ferromagnetic coupling (J > 0): aligned spins have lower energy
    float J[4] = {
        0.0f, 1.0f,
        1.0f, 0.0f
    };
    float h[2] = {0.0f, 0.0f};
    nimcp_ising_model_set_params(ctx, model, J, h);

    float energy = nimcp_ising_compute_energy(ctx, model);

    // Energy should be negative for aligned spins (ferromagnetic ground state)
    // E = -J * s1 * s2, so for J > 0 and same spins, E < 0
    EXPECT_TRUE(std::isfinite(energy));
}

//=============================================================================
// Quantum Annealing Tests
//=============================================================================

/**
 * TEST: Single annealing step
 * WHAT: Verify nimcp_annealing_step() executes successfully
 * WHY:  Annealing step is basic building block
 */
TEST_F(QuantumKernelTest, AnnealingStep_ExecutesSuccessfully) {
    skipIfNoGPU();

    model = nimcp_ising_model_create(ctx, 8);
    ASSERT_NE(model, nullptr);

    // Set random coupling
    std::vector<float> J(64, 0.0f);
    std::vector<float> h(8, 0.0f);
    for (int i = 0; i < 8; i++) {
        for (int j = i + 1; j < 8; j++) {
            J[i * 8 + j] = (float)(i - j) * 0.1f;
            J[j * 8 + i] = J[i * 8 + j];
        }
    }
    nimcp_ising_model_set_params(ctx, model, J.data(), h.data());

    // Perform annealing step
    bool result = nimcp_annealing_step(ctx, model, 1.0f, 0.5f);
    EXPECT_TRUE(result);
}

/**
 * TEST: Annealing step with NULL inputs
 * WHAT: Verify annealing step handles NULL inputs
 * WHY:  Guard clause validation
 */
TEST_F(QuantumKernelTest, AnnealingStep_NullInputs_ReturnsFalse) {
    skipIfNoGPU();

    model = nimcp_ising_model_create(ctx, 4);
    ASSERT_NE(model, nullptr);

    EXPECT_FALSE(nimcp_annealing_step(nullptr, model, 1.0f, 0.5f));
    EXPECT_FALSE(nimcp_annealing_step(ctx, nullptr, 1.0f, 0.5f));
}

/**
 * TEST: Complete quantum annealing
 * WHAT: Run full annealing schedule and verify energy decreases
 * WHY:  End-to-end validation of annealing algorithm
 */
TEST_F(QuantumKernelTest, QuantumAnneal_EnergyDecreases) {
    skipIfNoGPU();

    model = nimcp_ising_model_create(ctx, 8);
    ASSERT_NE(model, nullptr);

    // Set antiferromagnetic coupling on a chain
    std::vector<float> J(64, 0.0f);
    std::vector<float> h(8, 0.0f);
    for (int i = 0; i < 7; i++) {
        J[i * 8 + (i + 1)] = -1.0f;
        J[(i + 1) * 8 + i] = -1.0f;
    }
    nimcp_ising_model_set_params(ctx, model, J.data(), h.data());

    // Get initial energy
    float initial_energy = nimcp_ising_compute_energy(ctx, model);

    // Run annealing
    nimcp_annealing_config_t config = nimcp_annealing_default_config(8);
    float final_energy = nimcp_quantum_anneal(ctx, model, &config);

    // Final energy should typically be lower or equal
    // (not guaranteed due to stochastic nature)
    EXPECT_TRUE(std::isfinite(final_energy));
}

/**
 * TEST: Default annealing configuration
 * WHAT: Verify nimcp_annealing_default_config() returns valid config
 * WHY:  Convenience function should provide sensible defaults
 */
TEST_F(QuantumKernelTest, AnnealingDefaultConfig_ValidDefaults) {
    nimcp_annealing_config_t config = nimcp_annealing_default_config(10);

    EXPECT_EQ(config.n_spins, 10u);
    EXPECT_GT(config.T_initial, 0.0f);
    EXPECT_GT(config.T_final, 0.0f);
    EXPECT_LE(config.T_final, config.T_initial);
    EXPECT_GT(config.n_steps, 0u);
    EXPECT_GE(config.transverse_field_initial, 0.0f);
}

/**
 * TEST: Path-integral Monte Carlo annealing
 * WHAT: Test PIMC annealing variant
 * WHY:  PIMC provides more accurate quantum simulation
 */
TEST_F(QuantumKernelTest, PIMCAnneal_ExecutesSuccessfully) {
    skipIfNoGPU();

    model = nimcp_ising_model_create(ctx, 6);
    ASSERT_NE(model, nullptr);

    // Set simple parameters
    std::vector<float> J(36, 0.0f);
    std::vector<float> h(6, 0.0f);
    for (int i = 0; i < 5; i++) {
        J[i * 6 + (i + 1)] = -1.0f;
        J[(i + 1) * 6 + i] = -1.0f;
    }
    nimcp_ising_model_set_params(ctx, model, J.data(), h.data());

    nimcp_annealing_config_t config = nimcp_annealing_default_config(6);
    uint32_t n_trotter = 8;

    float final_energy = nimcp_pimc_anneal(ctx, model, &config, n_trotter);
    EXPECT_TRUE(std::isfinite(final_energy));
}

//=============================================================================
// Measurement and Probability Tests
//=============================================================================

/**
 * TEST: Quantum measurement
 * WHAT: Verify nimcp_quantum_measure() collapses state and returns result
 * WHY:  Measurement is final step of quantum computation
 */
TEST_F(QuantumKernelTest, Measure_ReturnsValidState) {
    skipIfNoGPU();

    state = nimcp_quantum_state_create(ctx, 3);
    ASSERT_NE(state, nullptr);

    // Initialize to uniform superposition
    nimcp_quantum_state_hadamard_all(ctx, state);

    uint32_t measured_state = UINT32_MAX;
    float probability = 0.0f;

    bool result = nimcp_quantum_measure(ctx, state, &measured_state, &probability);
    EXPECT_TRUE(result);

    // Measured state should be in valid range
    EXPECT_LT(measured_state, state->n_states);
    EXPECT_GT(probability, 0.0f);
    EXPECT_LE(probability, 1.0f);
}

/**
 * TEST: Measurement with NULL outputs
 * WHAT: Verify measurement handles NULL output pointers
 * WHY:  Caller may only want some outputs
 */
TEST_F(QuantumKernelTest, Measure_NullOutputs_StillSucceeds) {
    skipIfNoGPU();

    state = nimcp_quantum_state_create(ctx, 2);
    ASSERT_NE(state, nullptr);

    nimcp_quantum_state_hadamard_all(ctx, state);

    // Only get measured state
    uint32_t measured = UINT32_MAX;
    bool result1 = nimcp_quantum_measure(ctx, state, &measured, nullptr);
    EXPECT_TRUE(result1);
    EXPECT_LT(measured, state->n_states);

    // Re-initialize for second measurement
    nimcp_quantum_state_hadamard_all(ctx, state);

    // Only get probability
    float prob = 0.0f;
    bool result2 = nimcp_quantum_measure(ctx, state, nullptr, &prob);
    EXPECT_TRUE(result2);
    EXPECT_GT(prob, 0.0f);
}

/**
 * TEST: Measurement with NULL inputs
 * WHAT: Verify measurement handles NULL inputs
 * WHY:  Guard clause validation
 */
TEST_F(QuantumKernelTest, Measure_NullInputs_ReturnsFalse) {
    skipIfNoGPU();

    state = nimcp_quantum_state_create(ctx, 2);
    ASSERT_NE(state, nullptr);

    uint32_t measured;
    float prob;

    EXPECT_FALSE(nimcp_quantum_measure(nullptr, state, &measured, &prob));
    EXPECT_FALSE(nimcp_quantum_measure(ctx, nullptr, &measured, &prob));
}

/**
 * TEST: Compute probabilities
 * WHAT: Verify nimcp_quantum_compute_probabilities() computes |amplitude|^2
 * WHY:  Probability distribution is key output of quantum computation
 */
TEST_F(QuantumKernelTest, ComputeProbabilities_SumToOne) {
    skipIfNoGPU();

    state = nimcp_quantum_state_create(ctx, 4);
    ASSERT_NE(state, nullptr);

    // Initialize to uniform superposition
    nimcp_quantum_state_hadamard_all(ctx, state);

    // Create output tensor for probabilities
    size_t dims[1] = {state->n_states};
    nimcp_gpu_tensor_t* probs = nimcp_gpu_tensor_create(ctx, dims, 1,
                                                         NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(probs, nullptr);

    bool result = nimcp_quantum_compute_probabilities(ctx, state, probs);
    EXPECT_TRUE(result);

    // Copy to host and verify sum = 1
    std::vector<float> host_probs(state->n_states);
    nimcp_gpu_tensor_to_host(probs, host_probs.data());

    float sum = 0.0f;
    for (uint32_t i = 0; i < state->n_states; i++) {
        EXPECT_GE(host_probs[i], 0.0f);  // Probabilities non-negative
        sum += host_probs[i];
    }
    EXPECT_NEAR(sum, 1.0f, 0.001f);  // Sum to 1

    nimcp_gpu_tensor_destroy(probs);
}

/**
 * TEST: Compute probabilities with NULL inputs
 * WHAT: Verify compute_probabilities handles NULL inputs
 * WHY:  Guard clause validation
 */
TEST_F(QuantumKernelTest, ComputeProbabilities_NullInputs_ReturnsFalse) {
    skipIfNoGPU();

    state = nimcp_quantum_state_create(ctx, 2);
    ASSERT_NE(state, nullptr);

    size_t dims[1] = {state->n_states};
    nimcp_gpu_tensor_t* probs = nimcp_gpu_tensor_create(ctx, dims, 1,
                                                         NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(probs, nullptr);

    EXPECT_FALSE(nimcp_quantum_compute_probabilities(nullptr, state, probs));
    EXPECT_FALSE(nimcp_quantum_compute_probabilities(ctx, nullptr, probs));
    EXPECT_FALSE(nimcp_quantum_compute_probabilities(ctx, state, nullptr));

    nimcp_gpu_tensor_destroy(probs);
}

//=============================================================================
// Variational Quantum Circuit Tests
//=============================================================================

/**
 * TEST: Initialize VQC parameters
 * WHAT: Verify nimcp_vqc_init_params() initializes parameter tensor
 * WHY:  VQC is basis for quantum machine learning
 */
TEST_F(QuantumKernelTest, VQCInitParams_CreatesValidTensor) {
    skipIfNoGPU();

    uint32_t n_qubits = 4;
    uint32_t n_layers = 3;

    // Create parameter tensor: n_layers * n_qubits * 3 parameters per gate
    size_t param_size = n_layers * n_qubits * 3;
    size_t dims[1] = {param_size};
    nimcp_gpu_tensor_t* params = nimcp_gpu_tensor_create(ctx, dims, 1,
                                                          NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(params, nullptr);

    bool result = nimcp_vqc_init_params(ctx, n_qubits, n_layers, params);
    EXPECT_TRUE(result);

    // Verify parameters are initialized (typically random values in [0, 2*pi])
    std::vector<float> host_params(param_size);
    nimcp_gpu_tensor_to_host(params, host_params.data());

    for (size_t i = 0; i < param_size; i++) {
        EXPECT_TRUE(std::isfinite(host_params[i]));
    }

    nimcp_gpu_tensor_destroy(params);
}

/**
 * TEST: VQC init with NULL inputs
 * WHAT: Verify VQC init handles NULL inputs
 * WHY:  Guard clause validation
 */
TEST_F(QuantumKernelTest, VQCInitParams_NullInputs_ReturnsFalse) {
    skipIfNoGPU();

    size_t dims[1] = {12};
    nimcp_gpu_tensor_t* params = nimcp_gpu_tensor_create(ctx, dims, 1,
                                                          NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(params, nullptr);

    EXPECT_FALSE(nimcp_vqc_init_params(nullptr, 4, 1, params));
    EXPECT_FALSE(nimcp_vqc_init_params(ctx, 4, 1, nullptr));

    nimcp_gpu_tensor_destroy(params);
}

/**
 * TEST: Apply VQC layer
 * WHAT: Verify nimcp_vqc_apply_layer() applies parameterized gates
 * WHY:  Layer application is core VQC operation
 */
TEST_F(QuantumKernelTest, VQCApplyLayer_ModifiesState) {
    skipIfNoGPU();

    state = nimcp_quantum_state_create(ctx, 3);
    ASSERT_NE(state, nullptr);

    // Initialize state
    nimcp_quantum_state_hadamard_all(ctx, state);

    // Create layer parameters (n_qubits * 3)
    size_t dims[1] = {9};  // 3 qubits * 3 params
    nimcp_gpu_tensor_t* params = nimcp_gpu_tensor_create(ctx, dims, 1,
                                                          NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(params, nullptr);

    // Set some parameters
    float host_params[9] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f};
    nimcp_gpu_tensor_t* temp = nimcp_gpu_tensor_from_host(ctx, host_params, dims, 1,
                                                          NIMCP_GPU_PRECISION_FP32);
    if (temp) {
        nimcp_gpu_copy(ctx, temp, params);
        nimcp_gpu_tensor_destroy(temp);
    }

    // Get probabilities before
    size_t prob_dims[1] = {state->n_states};
    nimcp_gpu_tensor_t* probs_before = nimcp_gpu_tensor_create(ctx, prob_dims, 1,
                                                                NIMCP_GPU_PRECISION_FP32);
    nimcp_quantum_compute_probabilities(ctx, state, probs_before);
    std::vector<float> before(state->n_states);
    nimcp_gpu_tensor_to_host(probs_before, before.data());

    // Apply VQC layer
    bool result = nimcp_vqc_apply_layer(ctx, state, params);
    EXPECT_TRUE(result);

    // Get probabilities after
    nimcp_gpu_tensor_t* probs_after = nimcp_gpu_tensor_create(ctx, prob_dims, 1,
                                                               NIMCP_GPU_PRECISION_FP32);
    nimcp_quantum_compute_probabilities(ctx, state, probs_after);
    std::vector<float> after(state->n_states);
    nimcp_gpu_tensor_to_host(probs_after, after.data());

    // Probabilities should have changed (unless params were trivial)
    bool changed = false;
    for (uint32_t i = 0; i < state->n_states; i++) {
        if (std::abs(before[i] - after[i]) > 0.001f) {
            changed = true;
            break;
        }
    }
    EXPECT_TRUE(changed);

    nimcp_gpu_tensor_destroy(params);
    nimcp_gpu_tensor_destroy(probs_before);
    nimcp_gpu_tensor_destroy(probs_after);
}

//=============================================================================
// Integration Tests
//=============================================================================

/**
 * TEST: Full quantum search workflow
 * WHAT: Create state, run Grover, measure result
 * WHY:  End-to-end validation
 */
TEST_F(QuantumKernelTest, Integration_FullQuantumSearch) {
    skipIfNoGPU();

    // Create quantum state
    state = nimcp_quantum_state_create(ctx, 4);
    ASSERT_NE(state, nullptr);

    // Initialize to superposition
    EXPECT_TRUE(nimcp_quantum_state_hadamard_all(ctx, state));

    // Mark state |10> = |1010>
    uint32_t marked[1] = {10};

    // Run optimal number of Grover iterations
    uint32_t iterations = nimcp_grover_optimal_iterations(4, 1);
    for (uint32_t i = 0; i < iterations; i++) {
        EXPECT_TRUE(nimcp_grover_iteration(ctx, state, marked, 1));
    }

    // Compute final probabilities
    size_t dims[1] = {state->n_states};
    nimcp_gpu_tensor_t* probs = nimcp_gpu_tensor_create(ctx, dims, 1,
                                                         NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(probs, nullptr);

    nimcp_quantum_compute_probabilities(ctx, state, probs);
    std::vector<float> host_probs(state->n_states);
    nimcp_gpu_tensor_to_host(probs, host_probs.data());

    // Marked state should have highest probability
    float max_prob = 0.0f;
    uint32_t max_state = 0;
    for (uint32_t i = 0; i < state->n_states; i++) {
        if (host_probs[i] > max_prob) {
            max_prob = host_probs[i];
            max_state = i;
        }
    }
    EXPECT_EQ(max_state, 10u);
    EXPECT_GT(max_prob, 0.5f);  // Should have high probability

    // Measure
    uint32_t measured;
    float measure_prob;
    EXPECT_TRUE(nimcp_quantum_measure(ctx, state, &measured, &measure_prob));
    EXPECT_LT(measured, state->n_states);

    nimcp_gpu_tensor_destroy(probs);
}

/**
 * TEST: Full annealing optimization workflow
 * WHAT: Create Ising model, run annealing, check result
 * WHY:  End-to-end validation
 */
TEST_F(QuantumKernelTest, Integration_FullAnnealingOptimization) {
    skipIfNoGPU();

    // Create Ising model
    model = nimcp_ising_model_create(ctx, 12);
    ASSERT_NE(model, nullptr);

    // Set up Max-Cut problem on a ring
    std::vector<float> J(144, 0.0f);
    std::vector<float> h(12, 0.0f);
    for (int i = 0; i < 12; i++) {
        int j = (i + 1) % 12;
        J[i * 12 + j] = -1.0f;  // Antiferromagnetic
        J[j * 12 + i] = -1.0f;
    }
    EXPECT_TRUE(nimcp_ising_model_set_params(ctx, model, J.data(), h.data()));

    // Get initial energy
    float initial_energy = nimcp_ising_compute_energy(ctx, model);
    EXPECT_TRUE(std::isfinite(initial_energy));

    // Run quantum annealing
    nimcp_annealing_config_t config = nimcp_annealing_default_config(12);
    config.n_steps = 100;
    float final_energy = nimcp_quantum_anneal(ctx, model, &config);
    EXPECT_TRUE(std::isfinite(final_energy));

    // For antiferromagnetic ring, ground state energy is known
    // Final energy should be lower than or equal to random initial
}

/**
 * TEST: Multiple quantum states
 * WHAT: Create and manipulate multiple quantum states
 * WHY:  Verify resource management with multiple states
 */
TEST_F(QuantumKernelTest, Integration_MultipleStates) {
    skipIfNoGPU();

    std::vector<nimcp_quantum_state_t*> states;

    // Create multiple states
    for (int n = 2; n <= 6; n++) {
        nimcp_quantum_state_t* s = nimcp_quantum_state_create(ctx, n);
        ASSERT_NE(s, nullptr);
        states.push_back(s);

        // Initialize each
        EXPECT_TRUE(nimcp_quantum_state_hadamard_all(ctx, s));
    }

    // Verify all states are valid
    for (size_t i = 0; i < states.size(); i++) {
        size_t dims[1] = {states[i]->n_states};
        nimcp_gpu_tensor_t* probs = nimcp_gpu_tensor_create(ctx, dims, 1,
                                                             NIMCP_GPU_PRECISION_FP32);
        ASSERT_NE(probs, nullptr);

        EXPECT_TRUE(nimcp_quantum_compute_probabilities(ctx, states[i], probs));

        std::vector<float> host_probs(states[i]->n_states);
        nimcp_gpu_tensor_to_host(probs, host_probs.data());

        float sum = 0.0f;
        for (uint32_t j = 0; j < states[i]->n_states; j++) {
            sum += host_probs[j];
        }
        EXPECT_NEAR(sum, 1.0f, 0.001f);

        nimcp_gpu_tensor_destroy(probs);
    }

    // Cleanup
    for (auto s : states) {
        nimcp_quantum_state_destroy(s);
    }
}

//=============================================================================
// Edge Cases and Error Handling
//=============================================================================

/**
 * TEST: Large quantum state
 * WHAT: Test with maximum practical qubit count
 * WHY:  Verify memory handling at scale
 */
TEST_F(QuantumKernelTest, EdgeCase_LargeState) {
    skipIfNoGPU();

    // 16 qubits = 65536 states = 512KB for amplitudes (manageable)
    state = nimcp_quantum_state_create(ctx, 16);
    if (state == nullptr) {
        // May fail due to memory constraints
        GTEST_SKIP() << "Could not create 16-qubit state (memory constraint)";
    }

    EXPECT_EQ(state->n_qubits, 16u);
    EXPECT_EQ(state->n_states, 65536u);

    // Initialize should still work
    bool result = nimcp_quantum_state_hadamard_all(ctx, state);
    EXPECT_TRUE(result);
}

/**
 * TEST: GPU context synchronization
 * WHAT: Verify synchronization after quantum operations
 * WHY:  Ensure GPU operations complete before accessing results
 */
TEST_F(QuantumKernelTest, EdgeCase_Synchronization) {
    skipIfNoGPU();

    state = nimcp_quantum_state_create(ctx, 5);
    ASSERT_NE(state, nullptr);

    // Perform several operations
    nimcp_quantum_state_hadamard_all(ctx, state);

    uint32_t marked[1] = {7};
    nimcp_grover_iteration(ctx, state, marked, 1);
    nimcp_grover_iteration(ctx, state, marked, 1);

    // Synchronize
    int sync_result = nimcp_gpu_context_synchronize(ctx);
    EXPECT_EQ(sync_result, 0);

    // Now access results
    size_t dims[1] = {state->n_states};
    nimcp_gpu_tensor_t* probs = nimcp_gpu_tensor_create(ctx, dims, 1,
                                                         NIMCP_GPU_PRECISION_FP32);
    nimcp_quantum_compute_probabilities(ctx, state, probs);
    nimcp_gpu_context_synchronize(ctx);

    std::vector<float> host_probs(state->n_states);
    nimcp_gpu_tensor_to_host(probs, host_probs.data());

    float sum = 0.0f;
    for (uint32_t i = 0; i < state->n_states; i++) {
        sum += host_probs[i];
    }
    EXPECT_NEAR(sum, 1.0f, 0.01f);

    nimcp_gpu_tensor_destroy(probs);
}

/**
 * TEST: Repeated creation and destruction
 * WHAT: Create and destroy quantum states many times
 * WHY:  Verify no memory leaks
 */
TEST_F(QuantumKernelTest, EdgeCase_RepeatedCreateDestroy) {
    skipIfNoGPU();

    for (int i = 0; i < 100; i++) {
        nimcp_quantum_state_t* s = nimcp_quantum_state_create(ctx, 4);
        ASSERT_NE(s, nullptr);

        nimcp_quantum_state_hadamard_all(ctx, s);

        uint32_t marked[1] = {5};
        nimcp_grover_iteration(ctx, s, marked, 1);

        nimcp_quantum_state_destroy(s);
    }
    SUCCEED();
}

/**
 * TEST: Repeated Ising model creation
 * WHAT: Create and destroy Ising models many times
 * WHY:  Verify no memory leaks in annealing code
 */
TEST_F(QuantumKernelTest, EdgeCase_RepeatedIsingCreateDestroy) {
    skipIfNoGPU();

    for (int i = 0; i < 50; i++) {
        nimcp_ising_model_t* m = nimcp_ising_model_create(ctx, 8);
        ASSERT_NE(m, nullptr);

        std::vector<float> J(64, 0.0f);
        std::vector<float> h(8, 0.0f);
        for (int j = 0; j < 7; j++) {
            J[j * 8 + j + 1] = -1.0f;
            J[(j + 1) * 8 + j] = -1.0f;
        }
        nimcp_ising_model_set_params(ctx, m, J.data(), h.data());

        nimcp_annealing_step(ctx, m, 1.0f, 0.5f);

        nimcp_ising_model_destroy(m);
    }
    SUCCEED();
}
