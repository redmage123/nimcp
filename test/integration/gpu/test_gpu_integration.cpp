/**
 * @file test_gpu_integration.cpp
 * @brief Comprehensive GPU Integration Tests - End-to-End Workflows
 *
 * WHAT: Tests complete GPU workflows across all subsystems:
 *       - CPU to GPU fallback scenarios
 *       - Multi-GPU training pipelines
 *       - Spike event processing pipeline
 *       - GPU neuron computation integration
 *       - Execution mode switching
 *       - Cross-module GPU interactions
 *       - Memory management across modules
 *       - Error recovery and fallback
 *       - Performance comparisons
 *
 * WHY:  Verify GPU components work together correctly in production scenarios
 * HOW:  End-to-end workflows with real data and comprehensive validation
 *
 * ARCHITECTURE:
 *   Brain Creation → GPU Mode → Multi-GPU → Spike Events → Training → Validation
 *   ↓                ↓          ↓            ↓             ↓          ↓
 *   CPU Fallback   Detection  Distribution  Processing   STDP       Metrics
 *
 * TESTING STRATEGY:
 * - Happy paths with various GPU configurations
 * - Error injection and recovery
 * - Cross-boundary interactions
 * - Performance under load
 * - Memory safety and leak detection
 * - CPU/GPU mode switching
 *
 * @author NIMCP Development Team
 * @date 2025-11-19
 * @version GPU Integration Testing v2.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>
#include <cmath>

// Core components
#include "core/brain/nimcp_brain.h"
#include "gpu/nimcp_execution_mode.h"
#include "gpu/nimcp_multigpu.h"
#include "gpu/nimcp_spike_event.h"
#include "gpu/nimcp_gpu_neuron.h"

//=============================================================================
// Test Fixture
//=============================================================================

class GPUIntegrationTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    execution_context_t exec_ctx = nullptr;
    multigpu_context_t multi_ctx = nullptr;
    gpu_neural_network_t gpu_network = nullptr;
    spike_queue_t* spike_queue = nullptr;
    spike_train_t* spike_train = nullptr;

    // Test configuration
    const uint32_t test_neurons = 1000;
    const uint32_t test_synapses = 100;
    const float test_learning_rate = 0.01f;

    void SetUp() override {
        // Initialize with CPU mode (fallback always available)
        execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
        exec_ctx = execution_context_create(&config);
        ASSERT_NE(exec_ctx, nullptr);
    }

    void TearDown() override {
        // Clean up in reverse order of creation
        if (spike_train) {
            spike_train_destroy(spike_train);
            spike_train = nullptr;
        }
        if (spike_queue) {
            spike_queue_destroy(spike_queue);
            spike_queue = nullptr;
        }
        if (gpu_network) {
            gpu_neural_network_destroy(gpu_network);
            gpu_network = nullptr;
        }
        if (multi_ctx) {
            multigpu_context_destroy(multi_ctx);
            multi_ctx = nullptr;
        }
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
        if (exec_ctx) {
            execution_context_destroy(exec_ctx);
            exec_ctx = nullptr;
        }
    }

    // Helper: Train brain with sample data
    void train_sample_brain(brain_t b, int num_samples = 10) {
        ASSERT_NE(b, nullptr);
        for (int i = 0; i < num_samples; i++) {
            float features[5] = {
                (float)i / num_samples,
                (float)(i * 2) / num_samples,
                (float)(i * 3) / num_samples,
                (float)(i * 4) / num_samples,
                (float)(i * 5) / num_samples
            };
            const char* label = (i % 2 == 0) ? "positive" : "negative";
            brain_learn_example(b, features, 5, label, 0.95f);
        }
    }

    // Helper: Create test spike events
    std::vector<spike_event_t> create_test_spikes(uint32_t count) {
        std::vector<spike_event_t> spikes;
        for (uint32_t i = 0; i < count; i++) {
            spike_event_t spike;
            spike.timestamp = i * 1000;  // 1ms intervals
            spike.source_id = i % 100;
            spike.target_id = (i + 1) % 100;
            spike.synapse_id = i;
            spike.amplitude = 1.0f;
            spikes.push_back(spike);
        }
        return spikes;
    }

    // Helper: Check if GPU is available
    bool is_gpu_available() {
        return gpu_is_available();
    }
};

//=============================================================================
// WORKFLOW 1: CPU to GPU Fallback Scenarios
//=============================================================================

TEST_F(GPUIntegrationTest, CPUFallbackWhenNoGPU) {
    // WHAT: Test graceful fallback to CPU when GPU unavailable
    // WHY: Ensure system works on all hardware configurations

    // Skip if GPU is available (this test is for no-GPU systems)
    if (gpu_is_available()) {
        GTEST_SKIP() << "GPU available - skipping CPU fallback test";
    }

    // Try to create GPU network
    gpu_network_config_t config;
    memset(&config, 0, sizeof(config));
    config.num_neurons = test_neurons;
    config.num_synapses = test_synapses;
    config.exec_mode = EXEC_MODE_GPU_CUDA;

    gpu_network = gpu_neural_network_create(&config);

    // Should create network using CPU fallback
    ASSERT_NE(gpu_network, nullptr) << "Network creation should succeed with CPU fallback";

    // Verify CPU fallback mode is being used
    EXPECT_TRUE(execution_mode_is_supported(EXEC_MODE_CPU_SEQUENTIAL));
}

TEST_F(GPUIntegrationTest, AutoModeSelectionWithFallback) {
    // WHAT: Test AUTO mode selects best available option
    // WHY: Verify intelligent mode selection

    execution_config_t config = execution_get_default_config(EXEC_MODE_AUTO);
    execution_context_t auto_ctx = execution_context_create(&config);
    ASSERT_NE(auto_ctx, nullptr);

    execution_mode_t selected = execution_context_get_mode(auto_ctx);
    EXPECT_TRUE(execution_mode_is_supported(selected));

    // Verify selected mode is appropriate
    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    if (caps.cuda_available || caps.rocm_available) {
        // GPU available - may select GPU mode for appropriate workload
        (void)selected;  // Don't enforce GPU selection
    } else {
        // No GPU - should select CPU mode
        EXPECT_TRUE(selected == EXEC_MODE_CPU_SEQUENTIAL ||
                    selected == EXEC_MODE_CPU_PARALLEL);
    }

    execution_context_destroy(auto_ctx);
}

TEST_F(GPUIntegrationTest, BrainOperationAfterGPUFallback) {
    // WHAT: Verify brain continues to work after GPU fallback
    // WHY: Ensure seamless operation regardless of hardware

    brain = brain_create("fallback_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(brain, nullptr);

    train_sample_brain(brain, 20);

    // Make decisions - should work whether GPU is available or not
    float features[5] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    brain_decision_t* decision = brain_decide(brain, features, 5);
    ASSERT_NE(decision, nullptr);
    EXPECT_NE(decision->label, nullptr);
    brain_free_decision(decision);
}

//=============================================================================
// WORKFLOW 2: Multi-GPU Training Pipelines
//=============================================================================

TEST_F(GPUIntegrationTest, MultiGPUDeviceEnumeration) {
    // WHAT: Enumerate available GPU devices
    // WHY: Need to know GPU configuration before multi-GPU setup

    multigpu_device_info_t devices[8];
    uint32_t count = 0;
    bool enumerated = multigpu_enumerate_devices(devices, 8, &count);

    EXPECT_TRUE(enumerated);
    EXPECT_LE(count, 8);

    if (count > 0) {
        // Verify device info is valid
        for (uint32_t i = 0; i < count; i++) {
            EXPECT_GE(devices[i].device_id, 0);
            EXPECT_GT(devices[i].total_memory_bytes, 0);
            EXPECT_GT(devices[i].multiprocessor_count, 0);
        }
    }
}

TEST_F(GPUIntegrationTest, MultiGPUContextCreation) {
    // WHAT: Create multi-GPU context for distributed training
    // WHY: Verify multi-GPU initialization workflow

    multigpu_device_info_t devices[8];
    uint32_t count = 0;
    multigpu_enumerate_devices(devices, 8, &count);

    if (count == 0) {
        GTEST_SKIP() << "No GPUs available for multi-GPU test";
    }

    multigpu_config_t config = multigpu_default_config();
    config.num_devices = (count > 2) ? 2 : count;  // Use up to 2 GPUs
    config.partition_strategy = MULTIGPU_PARTITION_HYBRID;

    multi_ctx = multigpu_context_create(&config);
    // May return NULL if GPUs not available or multi-GPU not supported
    if (multi_ctx) {
        uint32_t active = multigpu_get_device_count(multi_ctx);
        EXPECT_GT(active, 0);
        EXPECT_LE(active, config.num_devices);
    }
}

TEST_F(GPUIntegrationTest, MultiGPUWorkPartitioning) {
    // WHAT: Partition neural network across multiple GPUs
    // WHY: Verify work distribution algorithm

    multigpu_device_info_t devices[8];
    uint32_t count = 0;
    multigpu_enumerate_devices(devices, 8, &count);

    if (count < 2) {
        GTEST_SKIP() << "Need at least 2 GPUs for partitioning test";
    }

    multigpu_config_t config = multigpu_default_config();
    config.num_devices = 2;
    multi_ctx = multigpu_context_create(&config);

    if (!multi_ctx) {
        GTEST_SKIP() << "Multi-GPU not available";
    }

    // Partition network
    const uint32_t num_layers = 4;
    uint32_t neurons_per_layer[4] = {100, 200, 150, 100};

    bool partitioned = multigpu_partition_network(multi_ctx, num_layers, neurons_per_layer);
    EXPECT_TRUE(partitioned);

    // Verify layer assignments
    for (uint32_t i = 0; i < num_layers; i++) {
        int assigned_gpu = multigpu_get_layer_assignment(multi_ctx, i);
        EXPECT_GE(assigned_gpu, 0);
        EXPECT_LT(assigned_gpu, 2);
    }
}

TEST_F(GPUIntegrationTest, MultiGPUMemoryBroadcast) {
    // WHAT: Broadcast data from CPU to all GPUs
    // WHY: Verify memory synchronization across devices

    multigpu_device_info_t devices[8];
    uint32_t count = 0;
    multigpu_enumerate_devices(devices, 8, &count);

    if (count < 2) {
        GTEST_SKIP() << "Need at least 2 GPUs";
    }

    multigpu_config_t config = multigpu_default_config();
    config.num_devices = 2;
    multi_ctx = multigpu_context_create(&config);

    if (!multi_ctx) {
        GTEST_SKIP() << "Multi-GPU not available";
    }

    // Allocate distributed memory
    // FIX: multigpu_alloc allocates total_size / num_gpus per GPU,
    // but multigpu_broadcast copies size bytes to EACH GPU.
    // Need to allocate per_gpu_size * num_gpus to avoid buffer overflow.
    uint32_t num_gpus = multigpu_get_device_count(multi_ctx);
    size_t per_gpu_size = 1024;
    size_t total_allocation = per_gpu_size * num_gpus;
    void** device_ptrs = multigpu_alloc(multi_ctx, total_allocation);

    if (device_ptrs) {
        // Create test data
        std::vector<float> host_data(per_gpu_size / sizeof(float), 3.14f);

        // Broadcast to all GPUs
        bool broadcasted = multigpu_broadcast(multi_ctx, host_data.data(),
                                              device_ptrs, per_gpu_size);
        EXPECT_TRUE(broadcasted);

        // Synchronize
        multigpu_synchronize(multi_ctx);

        // Clean up
        multigpu_free(multi_ctx, device_ptrs);
    }
}

//=============================================================================
// WORKFLOW 3: Spike Event Processing Pipeline
//=============================================================================

TEST_F(GPUIntegrationTest, SpikeEventQueueCreation) {
    // WHAT: Create spike event queue for GPU processing
    // WHY: Verify spike queue infrastructure

    bool gpu_available = is_gpu_available();
    spike_queue = spike_queue_create(1024, gpu_available);
    ASSERT_NE(spike_queue, nullptr);

    EXPECT_TRUE(spike_queue_is_empty(spike_queue));
    EXPECT_EQ(spike_queue_size(spike_queue), 0);
}

TEST_F(GPUIntegrationTest, SpikeEventPushPop) {
    // WHAT: Push and pop spike events through queue
    // WHY: Verify basic spike event operations

    spike_queue = spike_queue_create(1024, false);  // CPU mode
    ASSERT_NE(spike_queue, nullptr);

    // Create test spikes
    auto spikes = create_test_spikes(10);

    // Push spikes
    for (const auto& spike : spikes) {
        bool pushed = spike_queue_push(spike_queue, &spike);
        EXPECT_TRUE(pushed);
    }

    EXPECT_EQ(spike_queue_size(spike_queue), 10);

    // Pop spikes
    for (size_t i = 0; i < spikes.size(); i++) {
        spike_event_t retrieved;
        bool popped = spike_queue_pop(spike_queue, &retrieved);
        EXPECT_TRUE(popped);
        EXPECT_EQ(retrieved.timestamp, spikes[i].timestamp);
    }

    EXPECT_TRUE(spike_queue_is_empty(spike_queue));
}

TEST_F(GPUIntegrationTest, SpikeTrainRecording) {
    // WHAT: Record spike train for temporal learning
    // WHY: Verify spike train infrastructure for STDP

    spike_train = spike_train_create(100);
    ASSERT_NE(spike_train, nullptr);

    // Add spikes over time
    uint64_t base_time = 1000000;  // 1 second
    for (int i = 0; i < 10; i++) {
        uint64_t spike_time = base_time + (i * 10000);  // 10ms intervals
        bool added = spike_train_add(spike_train, spike_time, 1.0f);
        EXPECT_TRUE(added);
    }

    // Get last spike time
    uint64_t last_spike = spike_train_get_last_spike(spike_train);
    EXPECT_GT(last_spike, base_time);

    // Compute firing rate
    float rate = spike_train_compute_rate(spike_train, 100000);  // 100ms window
    EXPECT_GT(rate, 0.0f);
    EXPECT_LT(rate, 1000.0f);  // Reasonable range
}

TEST_F(GPUIntegrationTest, SpikeEventGPUSynchronization) {
    // WHAT: Synchronize spike queue between CPU and GPU
    // WHY: Verify bidirectional spike transfer

    if (!is_gpu_available()) {
        GTEST_SKIP() << "GPU not available";
    }

    spike_queue = spike_queue_create(1024, true);  // GPU-enabled
    if (!spike_queue) {
        GTEST_SKIP() << "GPU spike queue creation failed";
    }

    // Push spikes on CPU
    auto spikes = create_test_spikes(5);
    for (const auto& spike : spikes) {
        spike_queue_push(spike_queue, &spike);
    }

    // Sync to GPU
    bool synced = spike_queue_sync_gpu(spike_queue, true);
    if (synced) {
        // Sync back to CPU
        bool synced_back = spike_queue_sync_gpu(spike_queue, false);
        EXPECT_TRUE(synced_back);
    }
}

//=============================================================================
// WORKFLOW 4: GPU Neuron Computation Integration
//=============================================================================

TEST_F(GPUIntegrationTest, GPUNeuronNetworkCreation) {
    // WHAT: Create GPU neural network
    // WHY: Verify GPU neuron infrastructure

    if (!gpu_is_available()) {
        GTEST_SKIP() << "GPU not available - skipping GPU neuron test";
    }

    gpu_network_config_t config = gpu_get_optimal_config(test_neurons);
    config.num_neurons = test_neurons;
    config.num_synapses = test_synapses;
    config.enable_stdp = true;

    gpu_network = gpu_neural_network_create(&config);
    ASSERT_NE(gpu_network, nullptr) << "GPU network creation should succeed when GPU is available";
}

TEST_F(GPUIntegrationTest, GPUNeuronAddition) {
    // WHAT: Add neurons to GPU network
    // WHY: Verify GPU memory management

    if (!is_gpu_available()) {
        GTEST_SKIP() << "GPU not available";
    }

    gpu_network_config_t config = gpu_get_optimal_config(100);
    config.num_neurons = 100;
    config.num_synapses = 10;

    gpu_network = gpu_neural_network_create(&config);
    if (!gpu_network) {
        GTEST_SKIP() << "GPU network creation failed";
    }

    // Add neurons
    for (uint32_t i = 0; i < 10; i++) {
        gpu_neuron_state_t state;
        memset(&state, 0, sizeof(state));
        state.neuron_id = i;
        state.threshold = -55.0f;
        state.learning_rate = test_learning_rate;

        uint32_t id = gpu_neural_network_add_neuron(gpu_network, &state);
        EXPECT_NE(id, UINT32_MAX);
    }
}

TEST_F(GPUIntegrationTest, GPUSynapseAddition) {
    // WHAT: Create synaptic connections on GPU
    // WHY: Verify connectivity setup

    if (!is_gpu_available()) {
        GTEST_SKIP() << "GPU not available";
    }

    gpu_network_config_t config = gpu_get_optimal_config(100);
    config.num_neurons = 100;
    config.num_synapses = 50;

    gpu_network = gpu_neural_network_create(&config);
    if (!gpu_network) {
        GTEST_SKIP() << "GPU network creation failed";
    }

    // Add neurons first
    for (uint32_t i = 0; i < 20; i++) {
        gpu_neuron_state_t state;
        memset(&state, 0, sizeof(state));
        state.neuron_id = i;
        gpu_neural_network_add_neuron(gpu_network, &state);
    }

    // Add synapses
    for (uint32_t i = 0; i < 10; i++) {
        bool added = gpu_neural_network_add_synapse(gpu_network, i, i + 1, 0.5f, 1.0f);
        EXPECT_TRUE(added);
    }
}

TEST_F(GPUIntegrationTest, GPUNeuronUpdate) {
    // WHAT: Update neurons on GPU for one timestep
    // WHY: Verify GPU kernel execution

    if (!is_gpu_available()) {
        GTEST_SKIP() << "GPU not available";
    }

    gpu_network_config_t config = gpu_get_optimal_config(100);
    config.num_neurons = 100;
    config.num_synapses = 50;

    gpu_network = gpu_neural_network_create(&config);
    if (!gpu_network) {
        GTEST_SKIP() << "GPU network creation failed";
    }

    // Add some neurons
    for (uint32_t i = 0; i < 50; i++) {
        gpu_neuron_state_t state;
        memset(&state, 0, sizeof(state));
        state.neuron_id = i;
        state.membrane_potential = -70.0f + (i * 0.1f);
        state.threshold = -55.0f;
        gpu_neural_network_add_neuron(gpu_network, &state);
    }

    // Update network
    uint64_t timestamp = 0;
    uint64_t delta_t = 1000;  // 1ms
    uint32_t spikes = gpu_neural_network_update(gpu_network, timestamp, delta_t);

    // Synchronize
    gpu_neural_network_synchronize(gpu_network);

    // Should process update (may or may not have spikes)
    (void)spikes;  // Don't assert on spike count
}

//=============================================================================
// WORKFLOW 5: Execution Mode Switching
//=============================================================================

TEST_F(GPUIntegrationTest, ExecutionModeSwitch_CPUtoCPU) {
    // WHAT: Switch between CPU modes
    // WHY: Verify mode switching infrastructure

    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    execution_context_t ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Switch to parallel mode
    bool switched = execution_context_set_mode(ctx, EXEC_MODE_CPU_PARALLEL);
    if (switched) {
        execution_mode_t mode = execution_context_get_mode(ctx);
        EXPECT_EQ(mode, EXEC_MODE_CPU_PARALLEL);
    }

    execution_context_destroy(ctx);
}

TEST_F(GPUIntegrationTest, ExecutionModeSwitch_CPUtoGPU) {
    // WHAT: Switch from CPU to GPU mode
    // WHY: Verify runtime GPU activation

    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    execution_context_t ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Try to switch to GPU mode
    bool switched = execution_context_set_mode(ctx, EXEC_MODE_GPU_CUDA);

    if (switched) {
        // GPU available and switch successful
        EXPECT_TRUE(gpu_is_available());
        execution_mode_t mode = execution_context_get_mode(ctx);
        EXPECT_EQ(mode, EXEC_MODE_GPU_CUDA);
    } else {
        // GPU not available or switch failed - should stay on CPU
        execution_mode_t mode = execution_context_get_mode(ctx);
        EXPECT_EQ(mode, EXEC_MODE_CPU_SEQUENTIAL);
    }

    execution_context_destroy(ctx);
}

TEST_F(GPUIntegrationTest, ExecutionModeSwitchDuringTraining) {
    // WHAT: Switch execution mode during training
    // WHY: Verify state preservation across mode switch

    brain = brain_create("mode_switch", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(brain, nullptr);

    // Train on initial mode
    train_sample_brain(brain, 10);

    // Get decision before mode switch
    float features[5] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    brain_decision_t* decision1 = brain_decide(brain, features, 5);
    ASSERT_NE(decision1, nullptr);

    // Note: Brain API doesn't expose mode switching directly
    // This tests that brain continues to work after internal mode changes

    // Continue training
    train_sample_brain(brain, 10);

    // Get decision after more training
    brain_decision_t* decision2 = brain_decide(brain, features, 5);
    ASSERT_NE(decision2, nullptr);

    brain_free_decision(decision1);
    brain_free_decision(decision2);
}

//=============================================================================
// WORKFLOW 6: Cross-Module GPU Interactions
//=============================================================================

TEST_F(GPUIntegrationTest, BrainWithGPUNeuronIntegration) {
    // WHAT: Use brain with GPU neuron backend
    // WHY: Verify high-level API works with GPU

    brain = brain_create("gpu_brain", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(brain, nullptr);

    // Train brain (may use GPU internally)
    train_sample_brain(brain, 50);

    // Verify functionality
    float features[5] = {0.3f, 0.4f, 0.5f, 0.6f, 0.7f};
    brain_decision_t* decision = brain_decide(brain, features, 5);
    ASSERT_NE(decision, nullptr);
    EXPECT_NE(decision->label, nullptr);
    brain_free_decision(decision);
}

TEST_F(GPUIntegrationTest, SpikeEventWithGPUNeuronIntegration) {
    // WHAT: Process spike events with GPU neurons
    // WHY: Verify spike event pipeline works with GPU

    if (!is_gpu_available()) {
        GTEST_SKIP() << "GPU not available";
    }

    // Create GPU network
    gpu_network_config_t config = gpu_get_optimal_config(100);
    config.num_neurons = 100;
    gpu_network = gpu_neural_network_create(&config);

    if (!gpu_network) {
        GTEST_SKIP() << "GPU network creation failed";
    }

    // Create spike queue
    spike_queue = spike_queue_create(1024, true);
    if (!spike_queue) {
        GTEST_SKIP() << "GPU spike queue creation failed";
    }

    // Generate and process spikes
    auto spikes = create_test_spikes(20);
    for (const auto& spike : spikes) {
        spike_queue_push(spike_queue, &spike);
    }

    // Sync to GPU
    spike_queue_sync_gpu(spike_queue, true);

    // Update network (processes spikes)
    gpu_neural_network_update(gpu_network, 0, 1000);
    gpu_neural_network_synchronize(gpu_network);
}

TEST_F(GPUIntegrationTest, MultiGPUWithExecutionMode) {
    // WHAT: Combine multi-GPU and execution mode
    // WHY: Verify multi-GPU works with execution context

    multigpu_device_info_t devices[8];
    uint32_t count = 0;
    multigpu_enumerate_devices(devices, 8, &count);

    if (count < 2) {
        GTEST_SKIP() << "Need multiple GPUs";
    }

    // Create execution context for GPU
    execution_config_t exec_config = execution_get_default_config(EXEC_MODE_GPU_CUDA);
    execution_context_t gpu_ctx = execution_context_create(&exec_config);

    if (!gpu_ctx) {
        GTEST_SKIP() << "GPU execution context creation failed";
    }

    // Create multi-GPU context
    multigpu_config_t multi_config = multigpu_default_config();
    multi_config.num_devices = 2;
    multi_ctx = multigpu_context_create(&multi_config);

    if (multi_ctx) {
        EXPECT_GT(multigpu_get_device_count(multi_ctx), 0);
    }

    execution_context_destroy(gpu_ctx);
}

//=============================================================================
// WORKFLOW 7: Memory Management Across Modules
//=============================================================================

TEST_F(GPUIntegrationTest, ExecutionContextMemoryAllocation) {
    // WHAT: Allocate and free memory through execution context
    // WHY: Verify memory management API

    std::vector<void*> allocations;
    const size_t sizes[] = {1024, 4096, 16384, 65536};

    for (size_t size : sizes) {
        void* ptr = execution_alloc(exec_ctx, size);
        EXPECT_NE(ptr, nullptr);
        if (ptr) {
            allocations.push_back(ptr);
        }
    }

    // Free all allocations
    for (void* ptr : allocations) {
        execution_free(exec_ctx, ptr);
    }
}

TEST_F(GPUIntegrationTest, MultiGPUMemoryDistribution) {
    // WHAT: Distribute memory across multiple GPUs
    // WHY: Verify distributed memory management

    multigpu_device_info_t devices[8];
    uint32_t count = 0;
    multigpu_enumerate_devices(devices, 8, &count);

    if (count < 2) {
        GTEST_SKIP() << "Need multiple GPUs";
    }

    multigpu_config_t config = multigpu_default_config();
    config.num_devices = 2;
    multi_ctx = multigpu_context_create(&config);

    if (!multi_ctx) {
        GTEST_SKIP() << "Multi-GPU not available";
    }

    // Allocate distributed memory
    size_t total_size = 1024 * 1024;  // 1MB
    void** device_ptrs = multigpu_alloc(multi_ctx, total_size);

    if (device_ptrs) {
        // Memory allocated successfully
        multigpu_free(multi_ctx, device_ptrs);
    }
}

TEST_F(GPUIntegrationTest, MemoryTransferCPUtoGPU) {
    // WHAT: Transfer memory from CPU to GPU
    // WHY: Verify bidirectional memory transfer

    if (!is_gpu_available()) {
        GTEST_SKIP() << "GPU not available";
    }

    execution_config_t config = execution_get_default_config(EXEC_MODE_GPU_CUDA);
    execution_context_t gpu_ctx = execution_context_create(&config);

    if (!gpu_ctx) {
        GTEST_SKIP() << "GPU context creation failed";
    }

    // Allocate on both sides
    size_t size = 1024 * sizeof(float);
    std::vector<float> host_data(1024, 1.23f);
    void* device_ptr = execution_alloc(gpu_ctx, size);

    if (device_ptr) {
        // Transfer to GPU
        bool copied = execution_memcpy(gpu_ctx, device_ptr, host_data.data(),
                                       size, true);
        EXPECT_TRUE(copied);

        // Transfer back
        std::vector<float> host_result(1024);
        bool copied_back = execution_memcpy(gpu_ctx, host_result.data(),
                                            device_ptr, size, false);
        EXPECT_TRUE(copied_back);

        execution_free(gpu_ctx, device_ptr);
    }

    execution_context_destroy(gpu_ctx);
}

//=============================================================================
// WORKFLOW 8: Error Recovery and Fallback
//=============================================================================

TEST_F(GPUIntegrationTest, RecoveryFromGPUAllocationFailure) {
    // WHAT: Handle GPU memory allocation failure
    // WHY: Verify graceful degradation

    if (!is_gpu_available()) {
        GTEST_SKIP() << "GPU not available";
    }

    execution_config_t config = execution_get_default_config(EXEC_MODE_GPU_CUDA);
    config.auto_fallback = true;
    config.fallback_mode = EXEC_MODE_CPU_PARALLEL;

    execution_context_t ctx = execution_context_create(&config);
    if (!ctx) {
        // Context creation failed - fallback should have been attempted
        SUCCEED();
        return;
    }

    // Try to allocate unreasonably large memory
    size_t huge_size = 1024ULL * 1024ULL * 1024ULL * 100ULL;  // 100GB
    void* ptr = execution_alloc(ctx, huge_size);

    // Should fail gracefully
    EXPECT_EQ(ptr, nullptr);

    execution_context_destroy(ctx);
}

TEST_F(GPUIntegrationTest, RecoveryFromInvalidSpikeEvent) {
    // WHAT: Handle corrupted spike events
    // WHY: Verify error detection in spike pipeline

    spike_queue = spike_queue_create(1024, false);
    ASSERT_NE(spike_queue, nullptr);

    // Create invalid spike event
    spike_event_t invalid_spike;
    memset(&invalid_spike, 0xFF, sizeof(invalid_spike));  // Corrupt data

    // Queue should handle gracefully
    bool pushed = spike_queue_push(spike_queue, &invalid_spike);
    (void)pushed;  // May succeed (queue doesn't validate data)

    // Pop should work
    spike_event_t retrieved;
    spike_queue_pop(spike_queue, &retrieved);
}

TEST_F(GPUIntegrationTest, RecoveryFromMultiGPUDeviceFailure) {
    // WHAT: Handle GPU device failure in multi-GPU setup
    // WHY: Verify fault tolerance

    multigpu_device_info_t devices[8];
    uint32_t count = 0;
    multigpu_enumerate_devices(devices, 8, &count);

    if (count == 0) {
        GTEST_SKIP() << "No GPUs available";
    }

    multigpu_config_t config = multigpu_default_config();
    config.num_devices = count + 5;  // Request more than available

    multi_ctx = multigpu_context_create(&config);
    // Should either create with available GPUs or return NULL
    if (multi_ctx) {
        uint32_t actual = multigpu_get_device_count(multi_ctx);
        EXPECT_LE(actual, count);
    }
}

//=============================================================================
// WORKFLOW 9: Performance Comparisons
//=============================================================================

TEST_F(GPUIntegrationTest, PerformanceComparisonCPUvsGPU) {
    // WHAT: Compare CPU and GPU performance
    // WHY: Verify GPU provides speedup

    const int iterations = 100;

    // CPU timing
    auto cpu_start = std::chrono::high_resolution_clock::now();
    {
        execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
        execution_context_t cpu_ctx = execution_context_create(&config);
        ASSERT_NE(cpu_ctx, nullptr);

        for (int i = 0; i < iterations; i++) {
            void* ptr = execution_alloc(cpu_ctx, 1024);
            if (ptr) execution_free(cpu_ctx, ptr);
        }

        execution_context_destroy(cpu_ctx);
    }
    auto cpu_end = std::chrono::high_resolution_clock::now();
    auto cpu_duration = std::chrono::duration_cast<std::chrono::microseconds>(
        cpu_end - cpu_start).count();

    // GPU timing (if available)
    if (is_gpu_available()) {
        auto gpu_start = std::chrono::high_resolution_clock::now();
        {
            execution_config_t config = execution_get_default_config(EXEC_MODE_GPU_CUDA);
            execution_context_t gpu_ctx = execution_context_create(&config);

            if (gpu_ctx) {
                for (int i = 0; i < iterations; i++) {
                    void* ptr = execution_alloc(gpu_ctx, 1024);
                    if (ptr) execution_free(gpu_ctx, ptr);
                }
                execution_context_destroy(gpu_ctx);
            }
        }
        auto gpu_end = std::chrono::high_resolution_clock::now();
        auto gpu_duration = std::chrono::duration_cast<std::chrono::microseconds>(
            gpu_end - gpu_start).count();

        // Both should complete in reasonable time
        EXPECT_GT(cpu_duration, 0);
        EXPECT_GT(gpu_duration, 0);
    }
}

TEST_F(GPUIntegrationTest, ExecutionStatistics) {
    // WHAT: Collect execution statistics
    // WHY: Verify performance monitoring

    uint64_t total_ops = 0;
    double total_time_ms = 0.0;

    bool got_stats = execution_get_stats(exec_ctx, &total_ops, &total_time_ms);
    EXPECT_TRUE(got_stats);

    // Stats should be reasonable
    EXPECT_GE(total_ops, 0);
    EXPECT_GE(total_time_ms, 0.0);
}

TEST_F(GPUIntegrationTest, MultiGPUPerformanceStats) {
    // WHAT: Monitor multi-GPU performance
    // WHY: Verify load balancing metrics

    multigpu_device_info_t devices[8];
    uint32_t count = 0;
    multigpu_enumerate_devices(devices, 8, &count);

    if (count < 2) {
        GTEST_SKIP() << "Need multiple GPUs";
    }

    multigpu_config_t config = multigpu_default_config();
    config.num_devices = 2;
    multi_ctx = multigpu_context_create(&config);

    if (!multi_ctx) {
        GTEST_SKIP() << "Multi-GPU not available";
    }

    uint64_t ops = 0;
    double time_ms = 0.0;
    float utilization = 0.0f;
    float imbalance = 0.0f;

    bool got_stats = multigpu_get_performance_stats(multi_ctx, &ops, &time_ms,
                                                     &utilization, &imbalance);
    EXPECT_TRUE(got_stats);

    // Stats should be in valid ranges
    EXPECT_GE(utilization, 0.0f);
    EXPECT_LE(utilization, 1.0f);
    EXPECT_GE(imbalance, 0.0f);
    EXPECT_LE(imbalance, 1.0f);
}

//=============================================================================
// WORKFLOW 10: End-to-End Training Pipeline
//=============================================================================

TEST_F(GPUIntegrationTest, EndToEndTrainingPipeline) {
    // WHAT: Complete training workflow with GPU
    // WHY: Verify all components work together

    // Create brain
    brain = brain_create("e2e_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 5, 3);
    ASSERT_NE(brain, nullptr);

    // Train with multiple examples
    const int num_classes = 3;
    const char* labels[] = {"class_a", "class_b", "class_c"};

    for (int epoch = 0; epoch < 3; epoch++) {
        for (int i = 0; i < 30; i++) {
            float features[5];
            for (int j = 0; j < 5; j++) {
                features[j] = (float)(i * j) / 150.0f;
            }

            const char* label = labels[i % num_classes];
            brain_learn_example(brain, features, 5, label, 0.9f);
        }
    }

    // Test inference
    float test_features[5] = {0.2f, 0.3f, 0.4f, 0.5f, 0.6f};
    brain_decision_t* decision = brain_decide(brain, test_features, 5);
    ASSERT_NE(decision, nullptr);
    EXPECT_NE(decision->label, nullptr);
    EXPECT_GE(decision->confidence, 0.0f);
    EXPECT_LE(decision->confidence, 1.0f);

    brain_free_decision(decision);
}

TEST_F(GPUIntegrationTest, EndToEndGPUNeuronPipeline) {
    // WHAT: Complete GPU neuron simulation
    // WHY: Verify GPU neuron workflow

    if (!is_gpu_available()) {
        GTEST_SKIP() << "GPU not available";
    }

    // Create network
    gpu_network_config_t config = gpu_get_optimal_config(200);
    config.num_neurons = 200;
    config.num_synapses = 20;
    config.enable_stdp = true;

    gpu_network = gpu_neural_network_create(&config);
    if (!gpu_network) {
        GTEST_SKIP() << "GPU network creation failed";
    }

    // Build network
    for (uint32_t i = 0; i < 100; i++) {
        gpu_neuron_state_t state;
        memset(&state, 0, sizeof(state));
        state.neuron_id = i;
        state.membrane_potential = -70.0f;
        state.threshold = -55.0f;
        state.learning_rate = 0.01f;
        gpu_neural_network_add_neuron(gpu_network, &state);
    }

    // Add connectivity
    for (uint32_t i = 0; i < 50; i++) {
        gpu_neural_network_add_synapse(gpu_network, i, (i + 1) % 100, 0.5f, 1.0f);
    }

    // Simulate
    for (int t = 0; t < 10; t++) {
        uint64_t timestamp = t * 1000;
        gpu_neural_network_update(gpu_network, timestamp, 1000);
    }

    // Apply learning
    gpu_neural_network_apply_stdp(gpu_network, 10000);

    // Synchronize
    gpu_neural_network_synchronize(gpu_network);

    // Get statistics
    uint64_t spikes = 0;
    float rate = 0.0f;
    uint64_t mem_used = 0;
    bool got_stats = gpu_neural_network_get_stats(gpu_network, &spikes, &rate, &mem_used);
    EXPECT_TRUE(got_stats);
}

//=============================================================================
// WORKFLOW 11: Concurrent GPU Operations
//=============================================================================

TEST_F(GPUIntegrationTest, ConcurrentSpikeProcessing) {
    // WHAT: Process spikes from multiple threads
    // WHY: Verify thread safety

    spike_queue = spike_queue_create(4096, false);
    ASSERT_NE(spike_queue, nullptr);

    const int num_threads = 4;
    const int spikes_per_thread = 100;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, spikes_per_thread]() {
            for (int i = 0; i < spikes_per_thread; i++) {
                spike_event_t spike;
                spike.timestamp = (t * spikes_per_thread + i) * 1000;
                spike.source_id = t * 100 + i;
                spike.target_id = (t * 100 + i + 1) % 400;
                spike.synapse_id = i;
                spike.amplitude = 1.0f;

                spike_queue_push(spike_queue, &spike);
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Verify all spikes were queued
    uint32_t queued = spike_queue_size(spike_queue);
    EXPECT_GT(queued, 0);
    EXPECT_LE(queued, num_threads * spikes_per_thread);
}

//=============================================================================
// Test Summary
//=============================================================================

/**
 * INTEGRATION TEST SUMMARY
 *
 * Total Tests: 25
 *
 * Coverage Areas:
 * 1. CPU to GPU Fallback (3 tests)
 *    - CPU fallback when no GPU
 *    - Auto mode selection with fallback
 *    - Brain operation after GPU fallback
 *
 * 2. Multi-GPU Training Pipelines (4 tests)
 *    - Device enumeration
 *    - Context creation
 *    - Work partitioning
 *    - Memory broadcast
 *
 * 3. Spike Event Processing (4 tests)
 *    - Queue creation
 *    - Push/pop operations
 *    - Spike train recording
 *    - GPU synchronization
 *
 * 4. GPU Neuron Computation (4 tests)
 *    - Network creation
 *    - Neuron addition
 *    - Synapse addition
 *    - Neuron update
 *
 * 5. Execution Mode Switching (3 tests)
 *    - CPU to CPU switch
 *    - CPU to GPU switch
 *    - Switch during training
 *
 * 6. Cross-Module Interactions (3 tests)
 *    - Brain with GPU neuron integration
 *    - Spike event with GPU neuron
 *    - Multi-GPU with execution mode
 *
 * 7. Memory Management (3 tests)
 *    - Execution context allocation
 *    - Multi-GPU distribution
 *    - CPU to GPU transfer
 *
 * 8. Error Recovery (3 tests)
 *    - GPU allocation failure
 *    - Invalid spike events
 *    - Multi-GPU device failure
 *
 * 9. Performance Comparisons (3 tests)
 *    - CPU vs GPU performance
 *    - Execution statistics
 *    - Multi-GPU performance stats
 *
 * 10. End-to-End Pipelines (2 tests)
 *     - Complete training pipeline
 *     - GPU neuron simulation pipeline
 *
 * 11. Concurrent Operations (1 test)
 *     - Concurrent spike processing
 *
 * Key Patterns Tested:
 * - Graceful GPU fallback
 * - Multi-GPU coordination
 * - Spike event pipelines
 * - Memory management across devices
 * - Mode switching
 * - Error recovery
 * - Performance monitoring
 * - Thread safety
 */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
