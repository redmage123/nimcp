/**
 * @file test_spike_ring_buffer_regression.cpp
 * @brief Regression tests for dynamic spike history ring buffer
 *
 * WHAT: Regression suite validating memory footprint, large-scale creation,
 *       and backward-compatible brain workflow with dynamic spike buffers
 * WHY:  Prevent regressions in neuron size, large network support, and brain lifecycle
 * HOW:  Google Test framework with size assertions, 200K neuron scaling,
 *       and full brain create/train/decide/resize workflow
 *
 * TEST COVERAGE:
 * - Memory footprint (neuron_t < 5000 bytes)
 * - Large network scale test (200K neurons)
 * - Backward-compatible brain workflow
 */

#include <gtest/gtest.h>
#include <cstring>

// Brain API (public)
#include "nimcp.h"
// Network API (for direct network-level tests)
#include "core/neuralnet/nimcp_neuralnet.h"
// Internal constants (NIMCP_MAX_NEURONS)
#include "common/nimcp_internal.h"

//=============================================================================
// Memory Footprint Regression
//=============================================================================

TEST(SpikeRingBufferRegression, MemoryFootprint) {
    // WHAT: Verify neuron_t struct size is under 5000 bytes
    // WHY:  Dynamic spike history replaced fixed spike_record_t[1000] array,
    //       reducing per-neuron cost from ~19KB to ~3-4KB
    // HOW:  Static size check
    EXPECT_LT(sizeof(neuron_t), 4600u)
        << "neuron_t should be < 4600 bytes; dynamic spike + activity history "
           "replace the old fixed-size arrays";

    // Log actual size for documentation
    printf("  [INFO] sizeof(neuron_t) = %zu bytes\n", sizeof(neuron_t));
    printf("  [INFO] MAX_NEURONS = %u\n", MAX_NEURONS);
    printf("  [INFO] NIMCP_MAX_NEURONS = %u\n", NIMCP_MAX_NEURONS);
    printf("  [INFO] SPIKE_HISTORY_DEFAULT_CAPACITY = %u\n",
           SPIKE_HISTORY_DEFAULT_CAPACITY);
    printf("  [INFO] ACTIVITY_HISTORY_DEFAULT_CAPACITY = %u\n",
           ACTIVITY_HISTORY_DEFAULT_CAPACITY);
}

//=============================================================================
// Large Network Scale Test
//=============================================================================

TEST(SpikeRingBufferRegression, LargeNetworkScaleTest) {
    // WHAT: Create 200K neuron network and run forward passes
    // WHY:  Validate spike history optimization enables 2M-scale networks
    //       without OOM (200K * 4KB = ~800MB vs old 200K * 19KB = ~3.8GB)
    // HOW:  Create 200K neurons with spike_capacity=16, run 10 forward passes

    network_config_t config;
    memset(&config, 0, sizeof(config));
    config.num_neurons = 200000;
    config.ei_ratio = 0.8f;
    config.learning_rate = 0.01f;
    config.min_weight = -1.0f;
    config.max_weight = 1.0f;
    config.refractory_period = 2.0f;
    config.target_activity = 0.1f;
    config.input_size = 10;
    config.output_size = 10;
    config.spike_history_capacity = 16;  // Minimal capacity for scale test

    neural_network_t network = neural_network_create(&config);
    ASSERT_NE(network, nullptr)
        << "200K neuron network creation should succeed";

    uint32_t num = neural_network_get_num_neurons(network);
    EXPECT_GE(num, 200000u)
        << "Network should have at least 200K neurons";

    // Spot-check a few neurons across the range
    for (uint32_t id : {0u, 1000u, 50000u, 100000u, 199999u}) {
        neuron_t* neuron = neural_network_get_neuron(network, id);
        ASSERT_NE(neuron, nullptr) << "Neuron " << id << " should exist";
        EXPECT_NE(neuron->spike_history, nullptr)
            << "Neuron " << id << " should have spike buffer";
        EXPECT_EQ(neuron->spike_history_capacity, 16u)
            << "Neuron " << id << " should have capacity=16";
    }

    // Run 10 forward passes (update a subset of neurons)
    for (int pass = 0; pass < 10; pass++) {
        // Update first 100 neurons to exercise spike recording path
        for (uint32_t i = 0; i < 100; i++) {
            neural_network_update_neuron(network, i, 0.5f,
                                          static_cast<uint64_t>(pass * 100 + i));
        }
    }

    neural_network_destroy(network);
    // Success if no crash, OOM, or ASAN error
}

//=============================================================================
// Backward-Compatible Brain Workflow
//=============================================================================

TEST(SpikeRingBufferRegression, BackwardCompatWorkflow) {
    // WHAT: Full brain create -> train -> decide -> resize workflow
    // WHY:  Existing brain API must work identically with dynamic spike history
    // HOW:  Exercise entire lifecycle and verify correct behavior

    // Phase 1: Create brain
    nimcp_brain_t brain = nimcp_brain_create(
        "compat_test",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        4, 3
    );
    ASSERT_NE(brain, nullptr) << "Brain creation must succeed";

    // Phase 2: Train with several examples
    float features_a[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float features_b[4] = {0.0f, 1.0f, 0.0f, 0.0f};
    float features_c[4] = {0.0f, 0.0f, 1.0f, 0.0f};

    for (int epoch = 0; epoch < 30; epoch++) {
        nimcp_brain_learn_example(brain, features_a, 4, "class_a", 1.0f);
        nimcp_brain_learn_example(brain, features_b, 4, "class_b", 1.0f);
        nimcp_brain_learn_example(brain, features_c, 4, "class_c", 1.0f);
    }

    // Phase 3: Decide (predict)
    char label[64] = {0};
    float confidence = 0.0f;
    nimcp_status_t status = nimcp_brain_predict(
        brain, features_a, 4, label, &confidence
    );
    EXPECT_EQ(status, NIMCP_OK) << "Prediction should succeed";
    EXPECT_GT(confidence, 0.0f) << "Confidence should be positive";

    // Phase 4: Resize to 2x
    bool resize_ok = nimcp_brain_resize(brain, 200);
    EXPECT_TRUE(resize_ok) << "Resize should succeed";

    // Phase 5: Train after resize
    for (int i = 0; i < 10; i++) {
        nimcp_brain_learn_example(brain, features_b, 4, "class_b", 0.9f);
    }

    // Phase 6: Decide after resize
    status = nimcp_brain_predict(brain, features_b, 4, label, &confidence);
    EXPECT_EQ(status, NIMCP_OK)
        << "Prediction should succeed after resize";

    // Phase 7: Cleanup
    nimcp_brain_destroy(brain);
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
