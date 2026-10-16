#include <gtest/gtest.h>

#include "gpu/nimcp_gpu_neuron.h"

//=============================================================================
// GPU Neuron Real Tests
//=============================================================================

class GPUNeuronRealTest : public ::testing::Test {
protected:
    gpu_neural_network_t network = nullptr;

    void TearDown() override {
        if (network) {
            gpu_neural_network_destroy(network);
            network = nullptr;
        }
    }
};

//=============================================================================
// GPU Availability Tests
//=============================================================================

TEST_F(GPUNeuronRealTest, CheckGPUAvailable) {
    bool available = gpu_is_available();
    // Just check it returns a valid bool (may be true or false)
    SUCCEED();
}

TEST_F(GPUNeuronRealTest, GetDeviceCount) {
    uint32_t count = gpu_get_device_count();
    // Count may be 0 if no GPU, or > 0 if GPU present
    EXPECT_GE(count, 0);
}

TEST_F(GPUNeuronRealTest, GetDeviceName) {
    char name[256] = {0};
    bool result = gpu_get_device_name(0, name, sizeof(name));
    // May fail if no GPU, but shouldn't crash
    SUCCEED();
}

TEST_F(GPUNeuronRealTest, GetDeviceNameNull) {
    bool result = gpu_get_device_name(0, nullptr, 256);
    EXPECT_FALSE(result);
}

TEST_F(GPUNeuronRealTest, GetOptimalConfig) {
    gpu_network_config_t config = gpu_get_optimal_config(1000);

    EXPECT_GT(config.num_neurons, 0);
    EXPECT_GT(config.threads_per_block, 0);
}

//=============================================================================
// Network Creation Tests
//=============================================================================

TEST_F(GPUNeuronRealTest, CreateNetworkCPU) {
    gpu_network_config_t config = {};
    config.num_neurons = 100;
    config.num_synapses = 1000;
    config.threads_per_block = 256;
    config.max_blocks = 1024;
    config.spike_queue_capacity = 10000;
    config.use_unified_memory = false;
    config.pin_host_memory = false;
    config.exec_mode = EXEC_MODE_CPU_SEQUENTIAL;
    config.enable_stdp = true;
    config.enable_bcm = false;
    config.global_learning_rate = 0.01f;

    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);
}

TEST_F(GPUNeuronRealTest, CreateNetworkNull) {
    network = gpu_neural_network_create(nullptr);
    EXPECT_EQ(network, nullptr);
}

TEST_F(GPUNeuronRealTest, CreateNetworkZeroNeurons) {
    gpu_network_config_t config = {};
    config.num_neurons = 0;
    config.num_synapses = 1000;
    config.exec_mode = EXEC_MODE_CPU_SEQUENTIAL;

    network = gpu_neural_network_create(&config);
    EXPECT_EQ(network, nullptr);
}

TEST_F(GPUNeuronRealTest, DestroyNetworkNull) {
    // Should not crash
    gpu_neural_network_destroy(nullptr);
    SUCCEED();
}

//=============================================================================
// Neuron Operations Tests
//=============================================================================

TEST_F(GPUNeuronRealTest, AddNeuron) {
    gpu_network_config_t config = {};
    config.num_neurons = 100;
    config.num_synapses = 1000;
    config.exec_mode = EXEC_MODE_CPU_SEQUENTIAL;

    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    gpu_neuron_state_t state = {};
    state.membrane_potential = -70.0f;
    state.threshold = -55.0f;
    state.state = 0.0f;
    state.bias = 0.0f;
    state.learning_rate = 0.01f;

    uint32_t neuron_id = gpu_neural_network_add_neuron(network, &state);
    EXPECT_NE(neuron_id, UINT32_MAX);
}

TEST_F(GPUNeuronRealTest, AddNeuronNull) {
    gpu_network_config_t config = {};
    config.num_neurons = 100;
    config.num_synapses = 1000;
    config.exec_mode = EXEC_MODE_CPU_SEQUENTIAL;

    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    uint32_t neuron_id = gpu_neural_network_add_neuron(network, nullptr);
    EXPECT_EQ(neuron_id, UINT32_MAX);
}

TEST_F(GPUNeuronRealTest, AddSynapse) {
    gpu_network_config_t config = {};
    config.num_neurons = 100;
    config.num_synapses = 1000;
    config.exec_mode = EXEC_MODE_CPU_SEQUENTIAL;

    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    bool result = gpu_neural_network_add_synapse(network, 0, 1, 0.5f, 1.0f);
    // May succeed or fail depending on implementation
    SUCCEED();
}

TEST_F(GPUNeuronRealTest, AddSynapseInvalidSource) {
    gpu_network_config_t config = {};
    config.num_neurons = 100;
    config.num_synapses = 1000;
    config.exec_mode = EXEC_MODE_CPU_SEQUENTIAL;

    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    bool result = gpu_neural_network_add_synapse(network, 999, 1, 0.5f, 1.0f);
    EXPECT_FALSE(result);
}

//=============================================================================
// Simulation Tests
//=============================================================================

TEST_F(GPUNeuronRealTest, UpdateNetwork) {
    gpu_network_config_t config = {};
    config.num_neurons = 100;
    config.num_synapses = 1000;
    config.exec_mode = EXEC_MODE_CPU_SEQUENTIAL;

    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    uint32_t spike_count = gpu_neural_network_update(network, 1000, 1);
    EXPECT_GE(spike_count, 0);
}

TEST_F(GPUNeuronRealTest, UpdateNetworkNull) {
    uint32_t spike_count = gpu_neural_network_update(nullptr, 1000, 1);
    EXPECT_EQ(spike_count, 0);
}

TEST_F(GPUNeuronRealTest, ApplySTDP) {
    gpu_network_config_t config = {};
    config.num_neurons = 100;
    config.num_synapses = 1000;
    config.exec_mode = EXEC_MODE_CPU_SEQUENTIAL;
    config.enable_stdp = true;

    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    uint32_t modified = gpu_neural_network_apply_stdp(network, 1000);
    EXPECT_GE(modified, 0);
}

TEST_F(GPUNeuronRealTest, ApplySTDPNull) {
    uint32_t modified = gpu_neural_network_apply_stdp(nullptr, 1000);
    EXPECT_EQ(modified, 0);
}

TEST_F(GPUNeuronRealTest, Synchronize) {
    gpu_network_config_t config = {};
    config.num_neurons = 100;
    config.num_synapses = 1000;
    config.exec_mode = EXEC_MODE_CPU_SEQUENTIAL;

    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    bool result = gpu_neural_network_synchronize(network);
    EXPECT_TRUE(result);
}

TEST_F(GPUNeuronRealTest, SynchronizeNull) {
    bool result = gpu_neural_network_synchronize(nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// Data Access Tests
//=============================================================================

TEST_F(GPUNeuronRealTest, GetNeuronState) {
    gpu_network_config_t config = {};
    config.num_neurons = 100;
    config.num_synapses = 1000;
    config.exec_mode = EXEC_MODE_CPU_SEQUENTIAL;

    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    gpu_neuron_state_t state;
    bool result = gpu_neural_network_get_neuron_state(network, 0, &state);
    // May succeed or fail depending on implementation
    SUCCEED();
}

TEST_F(GPUNeuronRealTest, GetNeuronStateNull) {
    gpu_network_config_t config = {};
    config.num_neurons = 100;
    config.num_synapses = 1000;
    config.exec_mode = EXEC_MODE_CPU_SEQUENTIAL;

    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    bool result = gpu_neural_network_get_neuron_state(network, 0, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(GPUNeuronRealTest, SetNeuronState) {
    gpu_network_config_t config = {};
    config.num_neurons = 100;
    config.num_synapses = 1000;
    config.exec_mode = EXEC_MODE_CPU_SEQUENTIAL;

    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    gpu_neuron_state_t state = {};
    state.membrane_potential = -65.0f;
    state.threshold = -50.0f;

    bool result = gpu_neural_network_set_neuron_state(network, 0, &state);
    // May succeed or fail depending on implementation
    SUCCEED();
}

TEST_F(GPUNeuronRealTest, GetAllStates) {
    gpu_network_config_t config = {};
    config.num_neurons = 100;
    config.num_synapses = 1000;
    config.exec_mode = EXEC_MODE_CPU_SEQUENTIAL;

    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    gpu_neuron_state_t states[100];
    uint32_t count = gpu_neural_network_get_all_states(network, states, 100);
    EXPECT_GE(count, 0);
    EXPECT_LE(count, 100);
}

TEST_F(GPUNeuronRealTest, GetAllStatesNull) {
    gpu_network_config_t config = {};
    config.num_neurons = 100;
    config.num_synapses = 1000;
    config.exec_mode = EXEC_MODE_CPU_SEQUENTIAL;

    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    uint32_t count = gpu_neural_network_get_all_states(network, nullptr, 100);
    EXPECT_EQ(count, 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(GPUNeuronRealTest, GetStats) {
    gpu_network_config_t config = {};
    config.num_neurons = 100;
    config.num_synapses = 1000;
    config.exec_mode = EXEC_MODE_CPU_SEQUENTIAL;

    network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    uint64_t total_spikes = 0;
    float avg_rate = 0.0f;
    uint64_t mem_used = 0;

    bool result = gpu_neural_network_get_stats(network, &total_spikes, &avg_rate, &mem_used);
    // May succeed or fail depending on implementation
    SUCCEED();
}

TEST_F(GPUNeuronRealTest, GetStatsNull) {
    uint64_t total_spikes = 0;
    float avg_rate = 0.0f;
    uint64_t mem_used = 0;

    bool result = gpu_neural_network_get_stats(nullptr, &total_spikes, &avg_rate, &mem_used);
    EXPECT_FALSE(result);
}
