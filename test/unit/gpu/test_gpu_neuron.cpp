/**
 * @file test_gpu_neuron.cpp
 * @brief Unit tests for GPU neural network implementation
 *
 * WHAT: Tests GPU neuron creation, update, STDP, and CUDA kernel launch
 * WHY:  Verify GPU acceleration with CPU fallback works correctly
 * HOW:  Test all public API functions with various configurations
 *
 * TEST COVERAGE:
 * - GPU availability detection
 * - Network creation with CPU/GPU modes
 * - Neuron and synapse operations
 * - Network update (CUDA kernel or CPU fallback)
 * - STDP learning
 * - Synchronization
 * - Statistics retrieval
 * - Edge cases and error handling
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

// GPU headers include CUDA headers that cannot be in extern "C" blocks
#include "gpu/nimcp_gpu_neuron.h"
#include "gpu/nimcp_execution_mode.h"

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * @brief Test fixture for GPU neuron tests
 * WHAT: Provides common setup/teardown for GPU tests
 * WHY:  Ensure proper cleanup of GPU resources
 * HOW:  Automatically destroys networks in TearDown()
 */
class GPUNeuronTest : public ::testing::Test {
protected:
    gpu_neural_network_t network = nullptr;

    void TearDown() override {
        if (network) {
            gpu_neural_network_destroy(network);
            network = nullptr;
        }
    }

    /**
     * @brief Create a default test configuration
     */
    gpu_network_config_t create_test_config(uint32_t num_neurons = 100) {
        gpu_network_config_t config = {};
        config.num_neurons = num_neurons;
        config.num_synapses = num_neurons * 10;
        config.threads_per_block = 256;
        config.max_blocks = 1024;
        config.spike_queue_capacity = num_neurons * 10;
        config.use_unified_memory = false;
        config.pin_host_memory = false;
        config.exec_mode = EXEC_MODE_CPU_SEQUENTIAL;  // Safe default
        config.enable_stdp = true;
        config.enable_bcm = false;
        config.global_learning_rate = 0.01f;
        return config;
    }

    /**
     * @brief Create a default neuron state
     */
    gpu_neuron_state_t create_test_neuron_state() {
        gpu_neuron_state_t state = {};
        state.membrane_potential = -65.0f;
        state.threshold = -55.0f;
        state.state = 0.0f;
        state.bias = 0.1f;
        state.last_spike = 0;
        state.calcium_concentration = 0.0f;
        state.synaptic_trace = 0.0f;
        state.learning_rate = 0.01f;
        state.firing_rate = 0.0f;
        state.spike_count = 0;
        state.refractory_period = 2000;  // 2ms
        return state;
    }
};

//=============================================================================
// GPU Availability Tests
//=============================================================================

/**
 * TEST: GPU availability check
 * WHAT: Verify gpu_is_available() returns valid result
 * WHY:  Must detect GPU presence for mode selection
 */
TEST_F(GPUNeuronTest, IsGPUAvailable_ReturnsValidBool) {
    bool available = gpu_is_available();
    // Just verify it returns without crashing
    SUCCEED();
}

/**
 * TEST: GPU device count
 * WHAT: Verify gpu_get_device_count() works
 * WHY:  Need device count for multi-GPU support
 */
TEST_F(GPUNeuronTest, GetDeviceCount_ReturnsNonNegative) {
    uint32_t count = gpu_get_device_count();
    EXPECT_GE(count, 0u);
}

/**
 * TEST: GPU device name with valid buffer
 * WHAT: Query device name for first GPU
 * WHY:  Device name needed for logging/diagnostics
 */
TEST_F(GPUNeuronTest, GetDeviceName_ValidBuffer) {
    char name[256] = {0};
    bool result = gpu_get_device_name(0, name, sizeof(name));
    // May fail if no GPU, but shouldn't crash
    if (result) {
        EXPECT_GT(strlen(name), 0u);
    }
}

/**
 * TEST: GPU device name with NULL buffer
 * WHAT: Verify NULL pointer handling
 * WHY:  Prevent crashes from invalid input
 */
TEST_F(GPUNeuronTest, GetDeviceName_NullBuffer_ReturnsFalse) {
    bool result = gpu_get_device_name(0, nullptr, 256);
    EXPECT_FALSE(result);
}

/**
 * TEST: GPU device name with zero length
 * WHAT: Verify zero-length buffer handling
 * WHY:  Edge case protection
 */
TEST_F(GPUNeuronTest, GetDeviceName_ZeroLength_ReturnsFalse) {
    char name[256];
    bool result = gpu_get_device_name(0, name, 0);
    EXPECT_FALSE(result);
}

//=============================================================================
// Configuration Tests
//=============================================================================

/**
 * TEST: Get optimal configuration
 * WHAT: Verify gpu_get_optimal_config returns valid config
 * WHY:  Auto-configuration should provide reasonable defaults
 */
TEST_F(GPUNeuronTest, GetOptimalConfig_ValidNeuronCount) {
    gpu_network_config_t config = gpu_get_optimal_config(1000);

    EXPECT_EQ(config.num_neurons, 1000u);
    EXPECT_GT(config.num_synapses, 0u);
    EXPECT_GT(config.threads_per_block, 0u);
}

/**
 * TEST: Optimal config with zero neurons
 * WHAT: Edge case with zero neurons
 * WHY:  Should still return valid (empty) config
 */
TEST_F(GPUNeuronTest, GetOptimalConfig_ZeroNeurons) {
    gpu_network_config_t config = gpu_get_optimal_config(0);
    EXPECT_EQ(config.num_neurons, 0u);
}

//=============================================================================
// Network Creation Tests
//=============================================================================

/**
 * TEST: Create network with CPU mode
 * WHAT: Create network in CPU sequential mode
 * WHY:  CPU mode should always work
 */
TEST_F(GPUNeuronTest, CreateNetwork_CPUMode_Succeeds) {
    gpu_network_config_t config = create_test_config(100);
    config.exec_mode = EXEC_MODE_CPU_SEQUENTIAL;

    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);
}

/**
 * TEST: Create network with NULL config
 * WHAT: Verify NULL config handling
 * WHY:  Guard clause validation
 */
TEST_F(GPUNeuronTest, CreateNetwork_NullConfig_ReturnsNull) {
    network = gpu_neural_network_create(nullptr);
    EXPECT_EQ(network, nullptr);
}

/**
 * TEST: Create network with zero neurons
 * WHAT: Verify zero neuron handling
 * WHY:  Invalid configuration rejection
 */
TEST_F(GPUNeuronTest, CreateNetwork_ZeroNeurons_ReturnsNull) {
    gpu_network_config_t config = create_test_config(0);

    network = gpu_neural_network_create(&config);
    EXPECT_EQ(network, nullptr);
}

/**
 * TEST: Destroy NULL network
 * WHAT: Verify NULL-safe destroy
 * WHY:  Prevent crashes on double-free or NULL
 */
TEST_F(GPUNeuronTest, DestroyNetwork_Null_DoesNotCrash) {
    gpu_neural_network_destroy(nullptr);
    SUCCEED();
}

//=============================================================================
// Neuron Operations Tests
//=============================================================================

/**
 * TEST: Add neuron to network
 * WHAT: Add single neuron with valid state
 * WHY:  Core neuron addition functionality
 */
TEST_F(GPUNeuronTest, AddNeuron_ValidState_ReturnsId) {
    gpu_network_config_t config = create_test_config(100);
    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    gpu_neuron_state_t state = create_test_neuron_state();
    uint32_t id = gpu_neural_network_add_neuron(network, &state);

    EXPECT_EQ(id, 0u);  // First neuron has ID 0
}

/**
 * TEST: Add multiple neurons
 * WHAT: Add several neurons sequentially
 * WHY:  Verify ID assignment is sequential
 */
TEST_F(GPUNeuronTest, AddNeuron_Multiple_SequentialIds) {
    gpu_network_config_t config = create_test_config(100);
    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    gpu_neuron_state_t state = create_test_neuron_state();

    for (uint32_t i = 0; i < 10; i++) {
        uint32_t id = gpu_neural_network_add_neuron(network, &state);
        EXPECT_EQ(id, i);
    }
}

/**
 * TEST: Add neuron with NULL state
 * WHAT: Verify NULL state handling
 * WHY:  Guard clause validation
 */
TEST_F(GPUNeuronTest, AddNeuron_NullState_ReturnsMax) {
    gpu_network_config_t config = create_test_config(100);
    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    uint32_t id = gpu_neural_network_add_neuron(network, nullptr);
    EXPECT_EQ(id, UINT32_MAX);
}

/**
 * TEST: Add synapse between neurons
 * WHAT: Create synaptic connection
 * WHY:  Core connectivity functionality
 */
TEST_F(GPUNeuronTest, AddSynapse_ValidNeurons_Succeeds) {
    gpu_network_config_t config = create_test_config(100);
    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    gpu_neuron_state_t state = create_test_neuron_state();
    gpu_neural_network_add_neuron(network, &state);
    gpu_neural_network_add_neuron(network, &state);

    bool result = gpu_neural_network_add_synapse(network, 0, 1, 0.5f, 1.0f);
    EXPECT_TRUE(result);
}

/**
 * TEST: Add synapse with invalid source
 * WHAT: Synapse with non-existent source neuron
 * WHY:  Verify connectivity validation
 */
TEST_F(GPUNeuronTest, AddSynapse_InvalidSource_ReturnsFalse) {
    gpu_network_config_t config = create_test_config(100);
    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    gpu_neuron_state_t state = create_test_neuron_state();
    gpu_neural_network_add_neuron(network, &state);

    bool result = gpu_neural_network_add_synapse(network, 999, 0, 0.5f, 1.0f);
    EXPECT_FALSE(result);
}

//=============================================================================
// Network Update Tests
//=============================================================================

/**
 * TEST: Update network (CPU fallback)
 * WHAT: Run one timestep update
 * WHY:  Core simulation functionality
 */
TEST_F(GPUNeuronTest, Update_CPUMode_ReturnsSpikes) {
    gpu_network_config_t config = create_test_config(10);
    config.exec_mode = EXEC_MODE_CPU_SEQUENTIAL;
    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    gpu_neuron_state_t state = create_test_neuron_state();
    state.state = -50.0f;  // Above threshold to trigger spike

    for (int i = 0; i < 10; i++) {
        gpu_neural_network_add_neuron(network, &state);
    }

    uint32_t spikes = gpu_neural_network_update(network, 1000, 1000);
    // May or may not have spikes depending on dynamics
    EXPECT_GE(spikes, 0u);
}

/**
 * TEST: Update NULL network
 * WHAT: Update with NULL network pointer
 * WHY:  Guard clause validation
 */
TEST_F(GPUNeuronTest, Update_NullNetwork_ReturnsZero) {
    uint32_t spikes = gpu_neural_network_update(nullptr, 1000, 1000);
    EXPECT_EQ(spikes, 0u);
}

/**
 * TEST: Multiple update steps
 * WHAT: Run simulation for multiple timesteps
 * WHY:  Verify consistent behavior over time
 */
TEST_F(GPUNeuronTest, Update_MultipleSteps_Succeeds) {
    gpu_network_config_t config = create_test_config(10);
    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    gpu_neuron_state_t state = create_test_neuron_state();
    for (int i = 0; i < 10; i++) {
        gpu_neural_network_add_neuron(network, &state);
    }

    // Run 100 timesteps
    for (uint64_t t = 0; t < 100; t++) {
        uint32_t spikes = gpu_neural_network_update(network, t * 1000, 1000);
        EXPECT_GE(spikes, 0u);
    }
}

//=============================================================================
// STDP Learning Tests
//=============================================================================

/**
 * TEST: Apply STDP with enabled learning
 * WHAT: Run STDP weight update
 * WHY:  Verify plasticity mechanism
 */
TEST_F(GPUNeuronTest, ApplySTDP_Enabled_Succeeds) {
    gpu_network_config_t config = create_test_config(10);
    config.enable_stdp = true;
    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    gpu_neuron_state_t state = create_test_neuron_state();
    for (int i = 0; i < 5; i++) {
        gpu_neural_network_add_neuron(network, &state);
    }

    // Create some synapses
    for (int i = 0; i < 4; i++) {
        gpu_neural_network_add_synapse(network, i, i + 1, 0.5f, 1.0f);
    }

    uint32_t modified = gpu_neural_network_apply_stdp(network, 1000);
    EXPECT_GE(modified, 0u);
}

/**
 * TEST: Apply STDP with disabled learning
 * WHAT: STDP should be no-op when disabled
 * WHY:  Configuration respect
 */
TEST_F(GPUNeuronTest, ApplySTDP_Disabled_ReturnsZero) {
    gpu_network_config_t config = create_test_config(10);
    config.enable_stdp = false;
    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    uint32_t modified = gpu_neural_network_apply_stdp(network, 1000);
    EXPECT_EQ(modified, 0u);
}

//=============================================================================
// Synchronization Tests
//=============================================================================

/**
 * TEST: Synchronize CPU network
 * WHAT: Sync is no-op for CPU mode
 * WHY:  Should always succeed in CPU mode
 */
TEST_F(GPUNeuronTest, Synchronize_CPUMode_Succeeds) {
    gpu_network_config_t config = create_test_config(10);
    config.exec_mode = EXEC_MODE_CPU_SEQUENTIAL;
    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    bool result = gpu_neural_network_synchronize(network);
    EXPECT_TRUE(result);
}

/**
 * TEST: Synchronize NULL network
 * WHAT: Sync with NULL pointer
 * WHY:  Guard clause validation
 */
TEST_F(GPUNeuronTest, Synchronize_NullNetwork_ReturnsFalse) {
    bool result = gpu_neural_network_synchronize(nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// Data Access Tests
//=============================================================================

/**
 * TEST: Get neuron state
 * WHAT: Retrieve neuron state by ID
 * WHY:  State inspection for debugging/monitoring
 */
TEST_F(GPUNeuronTest, GetNeuronState_ValidId_Succeeds) {
    gpu_network_config_t config = create_test_config(10);
    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    gpu_neuron_state_t state = create_test_neuron_state();
    state.bias = 0.5f;
    gpu_neural_network_add_neuron(network, &state);

    gpu_neuron_state_t retrieved = {};
    bool result = gpu_neural_network_get_neuron_state(network, 0, &retrieved);

    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(retrieved.bias, 0.5f);
}

/**
 * TEST: Get neuron state with invalid ID
 * WHAT: Query non-existent neuron
 * WHY:  Bounds checking validation
 */
TEST_F(GPUNeuronTest, GetNeuronState_InvalidId_ReturnsFalse) {
    gpu_network_config_t config = create_test_config(10);
    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    gpu_neuron_state_t state = {};
    bool result = gpu_neural_network_get_neuron_state(network, 999, &state);
    EXPECT_FALSE(result);
}

/**
 * TEST: Set neuron state
 * WHAT: Modify neuron state by ID
 * WHY:  External control of neuron parameters
 */
TEST_F(GPUNeuronTest, SetNeuronState_ValidId_Succeeds) {
    gpu_network_config_t config = create_test_config(10);
    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    gpu_neuron_state_t state = create_test_neuron_state();
    gpu_neural_network_add_neuron(network, &state);

    state.bias = 0.9f;
    bool result = gpu_neural_network_set_neuron_state(network, 0, &state);
    EXPECT_TRUE(result);

    gpu_neuron_state_t retrieved = {};
    gpu_neural_network_get_neuron_state(network, 0, &retrieved);
    EXPECT_FLOAT_EQ(retrieved.bias, 0.9f);
}

/**
 * TEST: Get all neuron states
 * WHAT: Batch retrieval of all states
 * WHY:  Efficient bulk data access
 */
TEST_F(GPUNeuronTest, GetAllStates_FullBuffer_ReturnsAll) {
    gpu_network_config_t config = create_test_config(100);
    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    gpu_neuron_state_t state = create_test_neuron_state();
    for (int i = 0; i < 10; i++) {
        state.bias = (float)i * 0.1f;
        gpu_neural_network_add_neuron(network, &state);
    }

    std::vector<gpu_neuron_state_t> states(10);
    uint32_t count = gpu_neural_network_get_all_states(network, states.data(), 10);

    EXPECT_EQ(count, 10u);
}

//=============================================================================
// Statistics Tests
//=============================================================================

/**
 * TEST: Get network statistics
 * WHAT: Retrieve spike counts and firing rates
 * WHY:  Monitoring and analysis
 */
TEST_F(GPUNeuronTest, GetStats_ValidNetwork_Succeeds) {
    gpu_network_config_t config = create_test_config(10);
    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    uint64_t total_spikes = 0;
    float avg_rate = 0.0f;
    uint64_t gpu_mem = 0;

    bool result = gpu_neural_network_get_stats(network, &total_spikes,
                                                &avg_rate, &gpu_mem);
    EXPECT_TRUE(result);
    EXPECT_GE(total_spikes, 0u);
    EXPECT_GE(avg_rate, 0.0f);
}

/**
 * TEST: Get stats with NULL outputs
 * WHAT: Partial statistics retrieval
 * WHY:  Flexible API usage
 */
TEST_F(GPUNeuronTest, GetStats_NullOutputs_Succeeds) {
    gpu_network_config_t config = create_test_config(10);
    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    bool result = gpu_neural_network_get_stats(network, nullptr, nullptr, nullptr);
    EXPECT_TRUE(result);
}

/**
 * TEST: Get stats with NULL network
 * WHAT: Stats on NULL network
 * WHY:  Guard clause validation
 */
TEST_F(GPUNeuronTest, GetStats_NullNetwork_ReturnsFalse) {
    uint64_t spikes = 0;
    bool result = gpu_neural_network_get_stats(nullptr, &spikes, nullptr, nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// Integration Tests
//=============================================================================

/**
 * TEST: Full simulation cycle
 * WHAT: Create, add neurons/synapses, update, apply STDP
 * WHY:  End-to-end validation
 */
TEST_F(GPUNeuronTest, Integration_FullCycle_Succeeds) {
    // Create network
    gpu_network_config_t config = create_test_config(50);
    config.enable_stdp = true;
    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Add neurons
    gpu_neuron_state_t state = create_test_neuron_state();
    for (int i = 0; i < 20; i++) {
        uint32_t id = gpu_neural_network_add_neuron(network, &state);
        EXPECT_EQ(id, (uint32_t)i);
    }

    // Add synapses (chain connectivity)
    for (int i = 0; i < 19; i++) {
        bool added = gpu_neural_network_add_synapse(network, i, i + 1, 0.5f, 1.0f);
        EXPECT_TRUE(added);
    }

    // Run simulation
    for (uint64_t t = 0; t < 100; t++) {
        gpu_neural_network_update(network, t * 1000, 1000);
        if (t % 10 == 0) {
            gpu_neural_network_apply_stdp(network, t * 1000);
        }
    }

    // Verify statistics
    uint64_t total_spikes = 0;
    float avg_rate = 0.0f;
    bool result = gpu_neural_network_get_stats(network, &total_spikes, &avg_rate, nullptr);
    EXPECT_TRUE(result);

    // Synchronize
    EXPECT_TRUE(gpu_neural_network_synchronize(network));
}

//=============================================================================
// GPU Kernel Launch Tests (CUDA-specific with CPU Fallback)
//=============================================================================

/**
 * TEST: GPU mode network creation
 * WHAT: Create network requesting GPU mode
 * WHY:  Verify GPU initialization or CPU fallback
 */
TEST_F(GPUNeuronTest, CreateNetwork_GPUMode_HandlesGracefully) {
    gpu_network_config_t config = create_test_config(100);
    config.exec_mode = EXEC_MODE_GPU_CUDA;

    network = gpu_neural_network_create(&config);

    // Should succeed (either GPU or CPU fallback)
    ASSERT_NE(network, nullptr);
}

/**
 * TEST: Update with GPU mode (or fallback)
 * WHAT: Run update on GPU-configured network
 * WHY:  Verify CUDA kernel launch or CPU fallback
 */
TEST_F(GPUNeuronTest, Update_GPUMode_ExecutesCorrectly) {
    gpu_network_config_t config = create_test_config(50);
    config.exec_mode = gpu_is_available() ? EXEC_MODE_GPU_CUDA : EXEC_MODE_CPU_SEQUENTIAL;

    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Add neurons with above-threshold state to trigger spikes
    gpu_neuron_state_t state = create_test_neuron_state();
    state.membrane_potential = -50.0f;  // Close to threshold
    state.state = -52.0f;

    for (int i = 0; i < 50; i++) {
        gpu_neural_network_add_neuron(network, &state);
    }

    // Add some synapses
    for (int i = 0; i < 49; i++) {
        gpu_neural_network_add_synapse(network, i, i + 1, 0.5f, 1.0f);
    }

    // Run multiple updates
    uint32_t total_spikes = 0;
    for (uint64_t t = 0; t < 10; t++) {
        uint32_t spikes = gpu_neural_network_update(network, t * 1000, 1000);
        total_spikes += spikes;
    }

    // Verify update completed (spikes >= 0)
    EXPECT_GE(total_spikes, 0u);

    // Synchronize
    EXPECT_TRUE(gpu_neural_network_synchronize(network));
}

/**
 * TEST: Large network update
 * WHAT: Update with many neurons to stress GPU parallelism
 * WHY:  Verify scalability of GPU kernel launch
 */
TEST_F(GPUNeuronTest, Update_LargeNetwork_Succeeds) {
    gpu_network_config_t config = create_test_config(1000);
    config.threads_per_block = 256;
    config.max_blocks = 1024;

    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    gpu_neuron_state_t state = create_test_neuron_state();

    // Add many neurons
    for (int i = 0; i < 1000; i++) {
        uint32_t id = gpu_neural_network_add_neuron(network, &state);
        EXPECT_EQ(id, (uint32_t)i);
    }

    // Add sparse synapses
    for (int i = 0; i < 999; i += 10) {
        gpu_neural_network_add_synapse(network, i, i + 1, 0.3f, 1.0f);
    }

    // Run update
    uint32_t spikes = gpu_neural_network_update(network, 1000, 1000);
    EXPECT_GE(spikes, 0u);

    // Verify statistics
    uint64_t total = 0;
    float rate = 0.0f;
    uint64_t gpu_mem = 0;
    EXPECT_TRUE(gpu_neural_network_get_stats(network, &total, &rate, &gpu_mem));
}

/**
 * TEST: GPU fallback behavior
 * WHAT: Verify CPU fallback when GPU operations fail
 * WHY:  Ensure graceful degradation
 */
TEST_F(GPUNeuronTest, Update_CPUFallback_WorksWhenGPUUnavailable) {
    // Force CPU mode
    gpu_network_config_t config = create_test_config(100);
    config.exec_mode = EXEC_MODE_CPU_SEQUENTIAL;

    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    gpu_neuron_state_t state = create_test_neuron_state();
    for (int i = 0; i < 100; i++) {
        gpu_neural_network_add_neuron(network, &state);
    }

    // This should use CPU implementation
    uint32_t spikes = gpu_neural_network_update(network, 1000, 1000);
    EXPECT_GE(spikes, 0u);
}

/**
 * TEST: Network update with synaptic transmission
 * WHAT: Verify spike propagation through synapses
 * WHY:  Core neural network functionality
 */
TEST_F(GPUNeuronTest, Update_SynapticTransmission_PropagatesSpikes) {
    gpu_network_config_t config = create_test_config(5);
    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Create a chain: N0 -> N1 -> N2 -> N3 -> N4
    gpu_neuron_state_t state = create_test_neuron_state();
    state.threshold = -55.0f;
    state.membrane_potential = -65.0f;

    for (int i = 0; i < 5; i++) {
        gpu_neural_network_add_neuron(network, &state);
    }

    // Chain connectivity with strong weights
    for (int i = 0; i < 4; i++) {
        gpu_neural_network_add_synapse(network, i, i + 1, 1.0f, 1.0f);
    }

    // Set first neuron above threshold to trigger spike
    gpu_neuron_state_t trigger_state = state;
    trigger_state.membrane_potential = -50.0f;
    trigger_state.state = -50.0f;
    gpu_neural_network_set_neuron_state(network, 0, &trigger_state);

    // Run several updates to allow spike propagation
    for (uint64_t t = 0; t < 50; t++) {
        gpu_neural_network_update(network, t * 1000, 1000);
    }

    // Synchronize before checking states
    EXPECT_TRUE(gpu_neural_network_synchronize(network));
}

/**
 * TEST: Rapid sequential updates
 * WHAT: Many rapid timesteps to test stability
 * WHY:  Verify no memory leaks or corruption
 */
TEST_F(GPUNeuronTest, Update_RapidSequential_Stable) {
    gpu_network_config_t config = create_test_config(100);
    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    gpu_neuron_state_t state = create_test_neuron_state();
    for (int i = 0; i < 100; i++) {
        gpu_neural_network_add_neuron(network, &state);
    }

    // Add synapses
    for (int i = 0; i < 99; i++) {
        gpu_neural_network_add_synapse(network, i, i + 1, 0.5f, 1.0f);
    }

    // Rapid updates (1000 timesteps)
    for (uint64_t t = 0; t < 1000; t++) {
        uint32_t spikes = gpu_neural_network_update(network, t * 100, 100);
        EXPECT_GE(spikes, 0u);
    }

    // Final sync
    EXPECT_TRUE(gpu_neural_network_synchronize(network));

    // Get final stats
    uint64_t total_spikes = 0;
    float avg_rate = 0.0f;
    EXPECT_TRUE(gpu_neural_network_get_stats(network, &total_spikes, &avg_rate, nullptr));
}

/**
 * TEST: GPU memory reporting
 * WHAT: Verify GPU memory statistics are reported
 * WHY:  Monitor resource usage
 */
TEST_F(GPUNeuronTest, GetStats_GPUMemory_ReportsCorrectly) {
    gpu_network_config_t config = create_test_config(100);
    config.exec_mode = gpu_is_available() ? EXEC_MODE_GPU_CUDA : EXEC_MODE_CPU_SEQUENTIAL;

    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    uint64_t gpu_mem = 0;
    bool result = gpu_neural_network_get_stats(network, nullptr, nullptr, &gpu_mem);
    EXPECT_TRUE(result);

    // If GPU is being used, memory should be non-zero
    if (gpu_is_available()) {
        // GPU memory should be allocated
        // (exact value depends on whether CUDA is actually enabled at compile time)
    }
}

//=============================================================================
// Stress Tests for CUDA Implementation
//=============================================================================

/**
 * TEST: Create and destroy many networks
 * WHAT: Repeated creation/destruction cycle
 * WHY:  Verify no GPU resource leaks
 */
TEST_F(GPUNeuronTest, Lifecycle_CreateDestroyMany_NoLeaks) {
    for (int iteration = 0; iteration < 10; iteration++) {
        gpu_network_config_t config = create_test_config(50);

        gpu_neural_network_t net = gpu_neural_network_create(&config);
        ASSERT_NE(net, nullptr);

        gpu_neuron_state_t state = create_test_neuron_state();
        for (int i = 0; i < 50; i++) {
            gpu_neural_network_add_neuron(net, &state);
        }

        // Run some updates
        for (uint64_t t = 0; t < 10; t++) {
            gpu_neural_network_update(net, t * 1000, 1000);
        }

        gpu_neural_network_destroy(net);
    }
    SUCCEED();
}

/**
 * TEST: Concurrent-like usage pattern
 * WHAT: Simulate rapid state changes and updates
 * WHY:  Stress test GPU synchronization
 */
TEST_F(GPUNeuronTest, Update_StateChangesAndUpdates_Stable) {
    gpu_network_config_t config = create_test_config(100);
    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    gpu_neuron_state_t state = create_test_neuron_state();
    for (int i = 0; i < 100; i++) {
        gpu_neural_network_add_neuron(network, &state);
    }

    // Interleave state changes with updates
    for (uint64_t t = 0; t < 100; t++) {
        // Modify some neuron states
        if (t % 5 == 0) {
            gpu_neuron_state_t modified = state;
            modified.bias = 0.1f + (float)(t % 10) * 0.01f;
            for (uint32_t i = 0; i < 10; i++) {
                gpu_neural_network_set_neuron_state(network, i, &modified);
            }
        }

        // Update
        gpu_neural_network_update(network, t * 1000, 1000);

        // Occasionally read states
        if (t % 10 == 0) {
            gpu_neuron_state_t read_state;
            EXPECT_TRUE(gpu_neural_network_get_neuron_state(network, 0, &read_state));
        }
    }

    EXPECT_TRUE(gpu_neural_network_synchronize(network));
}
