/**
 * @file test_weights_stats.cpp
 * @brief Unit tests for weights/stats separation (v2.7.0 Enhancement 1)
 */

#include <gtest/gtest.h>

extern "C" {
#include "plasticity/adaptive/nimcp_weights.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/memory/nimcp_memory.h"
}

class WeightsStatsTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
    }

    void TearDown() override {
        // Cleanup
    }
};

//=============================================================================
// Network Statistics Tests
//=============================================================================

TEST_F(WeightsStatsTest, StatsCreation) {
    nimcp_network_stats_t* stats = network_stats_create();
    ASSERT_NE(stats, nullptr);

    // Check initial values
    EXPECT_EQ(stats->total_inferences, 0);
    EXPECT_EQ(stats->total_learning_steps, 0);
    EXPECT_FLOAT_EQ(stats->running_sparsity, 0.0f);
    EXPECT_FLOAT_EQ(stats->current_loss, 0.0f);

    network_stats_destroy(stats);
}

TEST_F(WeightsStatsTest, StatsUpdateSparsity) {
    nimcp_network_stats_t* stats = network_stats_create();
    ASSERT_NE(stats, nullptr);

    // Update with 20 active out of 100 neurons (80% sparsity)
    network_stats_update_sparsity(stats, 20, 100);

    EXPECT_FLOAT_EQ(stats->running_sparsity, 0.8f);
    EXPECT_EQ(stats->active_neuron_count, 20);

    network_stats_destroy(stats);
}

TEST_F(WeightsStatsTest, StatsRecordInference) {
    nimcp_network_stats_t* stats = network_stats_create();
    ASSERT_NE(stats, nullptr);

    // Record inference timing
    network_stats_record_inference(stats, 100.0f);  // 100 microseconds

    EXPECT_EQ(stats->total_inferences, 1);
    EXPECT_FLOAT_EQ(stats->avg_inference_time_us, 100.0f);

    // Record another inference - should update average
    network_stats_record_inference(stats, 200.0f);

    EXPECT_EQ(stats->total_inferences, 2);
    EXPECT_GT(stats->avg_inference_time_us, 100.0f);
    EXPECT_LT(stats->avg_inference_time_us, 200.0f);

    network_stats_destroy(stats);
}

TEST_F(WeightsStatsTest, StatsRecordLearning) {
    nimcp_network_stats_t* stats = network_stats_create();
    ASSERT_NE(stats, nullptr);

    // Record learning with loss
    network_stats_record_learning(stats, 50.0f, 0.5f);  // 50us, 0.5 loss

    EXPECT_EQ(stats->total_learning_steps, 1);
    EXPECT_FLOAT_EQ(stats->avg_learning_time_us, 50.0f);
    EXPECT_FLOAT_EQ(stats->current_loss, 0.5f);

    network_stats_destroy(stats);
}

TEST_F(WeightsStatsTest, StatsReset) {
    nimcp_network_stats_t* stats = network_stats_create();
    ASSERT_NE(stats, nullptr);

    // Add some data
    network_stats_record_inference(stats, 100.0f);
    network_stats_record_learning(stats, 50.0f, 0.5f);

    EXPECT_GT(stats->total_inferences, 0);
    EXPECT_GT(stats->total_learning_steps, 0);

    // Reset
    network_stats_reset(stats);

    EXPECT_EQ(stats->total_inferences, 0);
    EXPECT_EQ(stats->total_learning_steps, 0);
    EXPECT_FLOAT_EQ(stats->running_sparsity, 0.0f);

    network_stats_destroy(stats);
}

//=============================================================================
// Network Weights Tests
//=============================================================================

TEST_F(WeightsStatsTest, WeightsCreationWithNullNetwork) {
    adaptive_spike_params_t params = {0};
    params.min_threshold = 0.1f;
    params.max_threshold = 1.0f;

    network_weights_t* weights = network_weights_create(nullptr, &params);

    EXPECT_EQ(weights, nullptr);
}

TEST_F(WeightsStatsTest, WeightsCreationWithNullParams) {
    // Create a simple neural network
    network_config_t config = {0};
    config.input_size = 10;
    config.output_size = 5;
    neural_network_t network = neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    network_weights_t* weights = network_weights_create(network, nullptr);

    EXPECT_EQ(weights, nullptr);

    neural_network_destroy(network);
}

TEST_F(WeightsStatsTest, WeightsReferenceCountingRetain) {
    // Create network and weights
    network_config_t config = {0};
    config.input_size = 10;
    config.output_size = 5;
    neural_network_t network = neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    adaptive_spike_params_t params;
    params.min_threshold = 0.1f; params.max_threshold = 1.0f;

    network_weights_t* weights = network_weights_create(network, &params);
    ASSERT_NE(weights, nullptr);

    // Initial refcount should be 1
    EXPECT_EQ(network_weights_get_refcount(weights), 1);

    // Retain - refcount should increase
    uint32_t new_count = network_weights_retain(weights);
    EXPECT_EQ(new_count, 2);
    EXPECT_EQ(network_weights_get_refcount(weights), 2);

    // Release back to 1
    new_count = network_weights_release(weights);
    EXPECT_EQ(new_count, 1);

    // Final release (destroys weights)
    new_count = network_weights_release(weights);
    EXPECT_EQ(new_count, 0);
}

TEST_F(WeightsStatsTest, WeightsReferenceCountingMultipleRetains) {
    network_config_t config = {0};
    config.input_size = 10;
    config.output_size = 5;
    neural_network_t network = neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    adaptive_spike_params_t params;
    params.min_threshold = 0.1f; params.max_threshold = 1.0f;

    network_weights_t* weights = network_weights_create(network, &params);
    ASSERT_NE(weights, nullptr);

    // Simulate 5 brains sharing the weights
    for (int i = 0; i < 4; i++) {
        network_weights_retain(weights);
    }

    EXPECT_EQ(network_weights_get_refcount(weights), 5);

    // Release all references
    for (int i = 0; i < 5; i++) {
        uint32_t remaining = network_weights_release(weights);
        if (i < 4) {
            EXPECT_EQ(remaining, 4 - i);
        } else {
            EXPECT_EQ(remaining, 0);  // Last release destroys
        }
    }
}

TEST_F(WeightsStatsTest, WeightsCloneReturnsNull) {
    // Clone is not yet implemented
    network_config_t config = {0};
    config.input_size = 10;
    config.output_size = 5;
    neural_network_t network = neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    adaptive_spike_params_t params;
    params.min_threshold = 0.1f; params.max_threshold = 1.0f;

    network_weights_t* weights = network_weights_create(network, &params);
    ASSERT_NE(weights, nullptr);

    network_weights_t* cloned = network_weights_clone(weights);

    // Should return NULL since clone not implemented
    EXPECT_EQ(cloned, nullptr);

    network_weights_release(weights);
}

//=============================================================================
// Memory Safety Tests
//=============================================================================

TEST_F(WeightsStatsTest, StatsNullPointerSafety) {
    // All functions should handle NULL gracefully
    network_stats_destroy(nullptr);
    network_stats_reset(nullptr);
    network_stats_update_sparsity(nullptr, 10, 100);
    network_stats_record_inference(nullptr, 100.0f);
    network_stats_record_learning(nullptr, 50.0f, 0.5f);

    // If we get here without crashing, test passes
    SUCCEED();
}

TEST_F(WeightsStatsTest, WeightsNullPointerSafety) {
    EXPECT_EQ(network_weights_get_refcount(nullptr), 0);
    EXPECT_EQ(network_weights_retain(nullptr), 0);
    EXPECT_EQ(network_weights_release(nullptr), 0);
    EXPECT_EQ(network_weights_clone(nullptr), nullptr);

    SUCCEED();
}

//=============================================================================
// Integration Tests (Weights + Stats)
//=============================================================================

TEST_F(WeightsStatsTest, SeparationAllowsIndependentStats) {
    // Create shared weights
    network_config_t config = {0};
    config.input_size = 10;
    config.output_size = 5;
    neural_network_t network = neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    adaptive_spike_params_t params;
    params.min_threshold = 0.1f; params.max_threshold = 1.0f;

    network_weights_t* shared_weights = network_weights_create(network, &params);
    ASSERT_NE(shared_weights, nullptr);

    // Simulate 2 brains sharing weights
    network_weights_retain(shared_weights);

    // Each brain has its own statistics
    nimcp_network_stats_t* brain1_stats = network_stats_create();
    nimcp_network_stats_t* brain2_stats = network_stats_create();

    ASSERT_NE(brain1_stats, nullptr);
    ASSERT_NE(brain2_stats, nullptr);

    // Brain 1 does inferences
    network_stats_record_inference(brain1_stats, 100.0f);
    network_stats_record_inference(brain1_stats, 110.0f);

    // Brain 2 does learning
    network_stats_record_learning(brain2_stats, 200.0f, 0.8f);

    // Statistics should be independent
    EXPECT_EQ(brain1_stats->total_inferences, 2);
    EXPECT_EQ(brain1_stats->total_learning_steps, 0);

    EXPECT_EQ(brain2_stats->total_inferences, 0);
    EXPECT_EQ(brain2_stats->total_learning_steps, 1);

    // Weights should still be shared
    EXPECT_EQ(network_weights_get_refcount(shared_weights), 2);

    // Cleanup
    network_stats_destroy(brain1_stats);
    network_stats_destroy(brain2_stats);
    network_weights_release(shared_weights);
    network_weights_release(shared_weights);
}
