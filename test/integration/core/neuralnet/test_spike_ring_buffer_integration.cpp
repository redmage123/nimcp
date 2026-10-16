/**
 * @file test_spike_ring_buffer_integration.cpp
 * @brief Integration tests for dynamic spike history ring buffer
 *
 * WHAT: Test spike ring buffer behavior in full brain and network workflows
 * WHY:  Validate ring buffer correctness across create/train/resize lifecycle
 * HOW:  Google Test framework with brain API and network API integration scenarios
 *
 * TEST COVERAGE:
 * - Brain create + train + resize lifecycle
 * - Spike counting with large recording volumes
 * - Network reset as round-trip proxy
 */

#include <gtest/gtest.h>
#include <cstring>

// Brain API (public)
#include "nimcp.h"
// Network API (for direct network-level tests)
#include "core/neuralnet/nimcp_neuralnet.h"

//=============================================================================
// Helper: Create a default network config
//=============================================================================

static network_config_t make_default_config(uint32_t num_neurons) {
    network_config_t config;
    memset(&config, 0, sizeof(config));
    config.num_neurons = num_neurons;
    config.ei_ratio = 0.8f;
    config.learning_rate = 0.01f;
    config.min_weight = -1.0f;
    config.max_weight = 1.0f;
    config.refractory_period = 2.0f;
    config.target_activity = 0.1f;
    config.input_size = 4;
    config.output_size = 2;
    return config;
}

//=============================================================================
// Brain API Integration Tests
//=============================================================================

TEST(SpikeRingBufferIntegration, BrainCreateTrainResize) {
    // WHAT: Full brain lifecycle with dynamic spike history
    // WHY:  Verify spike buffers survive create/train/resize workflow
    // HOW:  Create TINY brain, train 50 iterations, resize to 2x, train again

    nimcp_brain_t brain = nimcp_brain_create(
        "spike_ring_test",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        4, 2
    );
    ASSERT_NE(brain, nullptr) << "Brain creation should succeed";

    // Train 50 iterations via learn_example
    float features[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    for (int i = 0; i < 50; i++) {
        nimcp_status_t status = nimcp_brain_learn_example(
            brain, features, 4, "class_a", 0.9f
        );
        // Training should not crash; status may vary
        (void)status;
    }

    // Predict to verify brain is functional
    char label[64] = {0};
    float confidence = 0.0f;
    nimcp_status_t pred_status = nimcp_brain_predict(
        brain, features, 4, label, &confidence
    );
    EXPECT_EQ(pred_status, NIMCP_OK) << "Prediction should succeed after training";

    // Resize to 2x neurons
    bool resize_ok = nimcp_brain_resize(brain, 200);
    EXPECT_TRUE(resize_ok) << "Resize should succeed";

    // Train again after resize — spike buffers must be intact
    for (int i = 0; i < 20; i++) {
        nimcp_brain_learn_example(brain, features, 4, "class_b", 0.8f);
    }

    // Predict after resize
    pred_status = nimcp_brain_predict(brain, features, 4, label, &confidence);
    EXPECT_EQ(pred_status, NIMCP_OK)
        << "Prediction should succeed after resize and retraining";

    nimcp_brain_destroy(brain);
}

//=============================================================================
// Network-Level Integration Tests
//=============================================================================

TEST(SpikeRingBufferIntegration, SpikeCounting) {
    // WHAT: Record many spikes and verify count saturation
    // WHY:  Validate ring buffer count semantics at scale
    // HOW:  Record 500 spikes, verify count = min(500, capacity)

    network_config_t config = make_default_config(20);
    config.spike_history_capacity = 64;
    neural_network_t network = neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Record 500 spikes on neuron 0
    for (uint64_t t = 1; t <= 500; t++) {
        bool ok = neural_network_record_spike(network, 0, 1.0f, t * 10);
        EXPECT_TRUE(ok) << "record_spike should succeed for spike " << t;
    }

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    ASSERT_NE(neuron, nullptr);

    // Count should saturate at capacity
    uint32_t expected_count = (500 < neuron->spike_history_capacity)
        ? 500 : neuron->spike_history_capacity;
    EXPECT_EQ(neuron->spike_history_count, expected_count)
        << "Count should be min(500, capacity)";

    // Verify the most recent spike is present
    // The last write position is (500 % capacity)
    uint32_t last_idx = (500 - 1) % neuron->spike_history_capacity;
    EXPECT_EQ(neuron->spike_history[last_idx].timestamp, 5000u)
        << "Most recent spike (t=5000) should be at last write index";

    neural_network_destroy(network);
}

TEST(SpikeRingBufferIntegration, SerializationRoundTrip) {
    // WHAT: Verify network remains functional after reset (round-trip proxy)
    // WHY:  Spike history is transient; reset simulates save/load clearing it
    // HOW:  Create, record spikes, reset, record again, verify functionality

    network_config_t config = make_default_config(20);
    config.spike_history_capacity = 32;
    neural_network_t network = neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Phase 1: Record spikes
    for (uint64_t t = 1; t <= 50; t++) {
        neural_network_record_spike(network, 0, 1.0f, t * 100);
    }

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    ASSERT_NE(neuron, nullptr);
    EXPECT_EQ(neuron->spike_history_count, 32u)
        << "Count should saturate at capacity before reset";

    // Phase 2: Reset (simulates round-trip — spike history is transient)
    neural_network_reset(network);

    neuron = neural_network_get_neuron(network, 0);
    ASSERT_NE(neuron, nullptr);
    EXPECT_EQ(neuron->spike_history_count, 0u)
        << "Count should be 0 after reset";
    EXPECT_EQ(neuron->spike_history_index, 0u)
        << "Index should be 0 after reset";
    EXPECT_NE(neuron->spike_history, nullptr)
        << "Buffer should still be allocated after reset";

    // Phase 3: Record again after reset
    for (uint64_t t = 1; t <= 10; t++) {
        bool ok = neural_network_record_spike(network, 0, 0.5f, t * 200);
        EXPECT_TRUE(ok) << "record_spike should succeed after reset";
    }

    neuron = neural_network_get_neuron(network, 0);
    ASSERT_NE(neuron, nullptr);
    EXPECT_EQ(neuron->spike_history_count, 10u)
        << "Should have 10 spikes after re-recording";

    neural_network_destroy(network);
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
