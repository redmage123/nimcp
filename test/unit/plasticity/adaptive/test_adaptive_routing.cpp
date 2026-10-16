/**
 * @file test_adaptive_routing.cpp
 * @brief Unit tests for Phase C4.4 adaptive neuromodulator routing
 *
 * WHAT: Comprehensive unit tests for adaptive routing functionality
 * WHY:  Ensure 100% code coverage and correctness of Shannon-based routing
 * HOW:  Test all adaptive routing functions with normal and edge cases
 *
 * TEST COVERAGE:
 * - spatial_neuromod_score_neuron()
 * - spatial_neuromod_select_optimal_sources()
 * - spatial_neuromod_release_adaptive()
 * - spatial_neuromod_release_adaptive_batch()
 * - Configuration validation
 * - Edge cases and error handling
 *
 * @author NIMCP Development Team
 * @date 2025-11-14
 */

#include <gtest/gtest.h>
#include "plasticity/neuromodulators/nimcp_spatial_neuromod.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/quantum/nimcp_quantum_shannon.h"
#include <cmath>

//=============================================================================
// Test Fixture
//=============================================================================

class AdaptiveRoutingTest : public ::testing::Test {
protected:
    neural_network_t network;
    spatial_neuromod_field_t* field;
    spatial_neuromod_config_t config;
    uint32_t num_neurons;

    void SetUp() override {
        // Create small network for testing
        num_neurons = 100;

        // Create network config
        network_config_t net_config = {};
        net_config.num_neurons = num_neurons;
        net_config.ei_ratio = 0.8f;
        net_config.learning_rate = 0.01f;
        net_config.stdp_window = 20.0f;
        net_config.refractory_period = 2.0f;
        net_config.min_weight = 0.0f;
        net_config.max_weight = 1.0f;
        net_config.input_size = num_neurons;
        net_config.output_size = num_neurons;

        network = neural_network_create(&net_config);
        ASSERT_NE(network, nullptr);

        // Create config with adaptive routing enabled
        config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
        config.enable_quantum_walk = true;      // Enable quantum-Shannon
        config.enable_adaptive_routing = true;  // Enable adaptive routing
        config.num_adaptive_sources = 5;
        config.min_source_score = 0.1f;

        // Create field
        field = spatial_neuromod_create(num_neurons, &config);
        ASSERT_NE(field, nullptr);

        // Enable quantum-Shannon and create system
        field->use_quantum_shannon = true;
        quantum_shannon_config_t qs_config = quantum_shannon_default_config();
        field->quantum_shannon_diffusion = quantum_shannon_create(
            network, num_neurons / 2, 10.0f, &qs_config);
        ASSERT_NE(field->quantum_shannon_diffusion, nullptr);

        // Set some Shannon metrics for testing (good metrics = positive scores)
        field->last_propagation_efficiency = 0.85f;
        field->last_speedup_vs_classical = 15.0f;
        field->last_num_bottlenecks = 0;  // No bottlenecks for positive scores
        field->last_information_rate = 2.0f;
    }

    void TearDown() override {
        if (field) {
            spatial_neuromod_destroy(field);
        }
        if (network) {
            neural_network_destroy(network);
        }
    }
};

//=============================================================================
// Test: spatial_neuromod_score_neuron()
//=============================================================================

TEST_F(AdaptiveRoutingTest, ScoreNeuron_ValidParameters_ReturnsPositiveScore) {
    // WHAT: Test scoring with valid parameters
    // WHY:  Verify score computation uses Shannon metrics correctly
    // HOW:  Call score function, check result in [0, 1]

    float score = spatial_neuromod_score_neuron(field, 50, network, &config);

    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
    EXPECT_GT(score, 0.0f);  // Should be non-zero with decent Shannon metrics
}

TEST_F(AdaptiveRoutingTest, ScoreNeuron_HighEfficiency_ReturnsHighScore) {
    // WHAT: Test that high efficiency yields high score
    // WHY:  Validate efficiency weighting
    // HOW:  Set high efficiency, compare scores

    field->last_propagation_efficiency = 0.95f;
    field->last_speedup_vs_classical = 15.0f;
    field->last_num_bottlenecks = 0;  // No bottlenecks
    field->last_information_rate = 2.0f;

    float high_score = spatial_neuromod_score_neuron(field, 50, network, &config);

    field->last_propagation_efficiency = 0.2f;
    field->last_speedup_vs_classical = 2.0f;
    field->last_num_bottlenecks = 10;  // Many bottlenecks
    field->last_information_rate = 0.1f;

    float low_score = spatial_neuromod_score_neuron(field, 50, network, &config);

    EXPECT_GT(high_score, low_score);
}

TEST_F(AdaptiveRoutingTest, ScoreNeuron_NullField_ReturnsZero) {
    // WHAT: Test NULL field handling
    // WHY:  Ensure graceful error handling
    // HOW:  Pass NULL field, expect 0.0

    float score = spatial_neuromod_score_neuron(nullptr, 50, network, &config);
    EXPECT_EQ(score, 0.0f);
}

TEST_F(AdaptiveRoutingTest, ScoreNeuron_NullConfig_ReturnsZero) {
    // WHAT: Test NULL config handling
    // WHY:  Ensure graceful error handling
    // HOW:  Pass NULL config, expect 0.0

    float score = spatial_neuromod_score_neuron(field, 50, network, nullptr);
    EXPECT_EQ(score, 0.0f);
}

TEST_F(AdaptiveRoutingTest, ScoreNeuron_InvalidNeuronID_ReturnsZero) {
    // WHAT: Test invalid neuron ID handling
    // WHY:  Ensure bounds checking
    // HOW:  Pass out-of-range neuron ID, expect 0.0

    float score = spatial_neuromod_score_neuron(field, 999999, network, &config);
    EXPECT_EQ(score, 0.0f);
}

TEST_F(AdaptiveRoutingTest, ScoreNeuron_QuantumShannonDisabled_ReturnsZero) {
    // WHAT: Test behavior when quantum-Shannon disabled
    // WHY:  Adaptive routing requires quantum-Shannon
    // HOW:  Disable quantum-Shannon, expect 0.0

    field->use_quantum_shannon = false;
    float score = spatial_neuromod_score_neuron(field, 50, network, &config);
    EXPECT_EQ(score, 0.0f);
}

//=============================================================================
// Test: spatial_neuromod_select_optimal_sources()
//=============================================================================

TEST_F(AdaptiveRoutingTest, SelectOptimalSources_ValidParameters_ReturnsTrue) {
    // WHAT: Test optimal source selection
    // WHY:  Verify selection algorithm works
    // HOW:  Call select function, check success and count

    uint32_t selected_ids[5];
    float selected_scores[5];
    uint32_t num_selected = 0;

    bool success = spatial_neuromod_select_optimal_sources(
        field, network, &config, selected_ids, selected_scores, &num_selected);

    EXPECT_TRUE(success);
    EXPECT_GT(num_selected, 0u);
    EXPECT_LE(num_selected, 5u);  // At most config.num_adaptive_sources

    // Check scores are descending
    for (uint32_t i = 1; i < num_selected; i++) {
        EXPECT_GE(selected_scores[i - 1], selected_scores[i]);
    }

    // Check all scores meet minimum threshold
    for (uint32_t i = 0; i < num_selected; i++) {
        EXPECT_GE(selected_scores[i], config.min_source_score);
    }
}

TEST_F(AdaptiveRoutingTest, SelectOptimalSources_NullField_ReturnsFalse) {
    // WHAT: Test NULL field handling
    // WHY:  Ensure graceful error handling
    // HOW:  Pass NULL field, expect false

    uint32_t selected_ids[5];
    uint32_t num_selected = 0;

    bool success = spatial_neuromod_select_optimal_sources(
        nullptr, network, &config, selected_ids, nullptr, &num_selected);

    EXPECT_FALSE(success);
}

TEST_F(AdaptiveRoutingTest, SelectOptimalSources_NullConfig_ReturnsFalse) {
    // WHAT: Test NULL config handling
    // WHY:  Ensure graceful error handling
    // HOW:  Pass NULL config, expect false

    uint32_t selected_ids[5];
    uint32_t num_selected = 0;

    bool success = spatial_neuromod_select_optimal_sources(
        field, network, nullptr, selected_ids, nullptr, &num_selected);

    EXPECT_FALSE(success);
}

TEST_F(AdaptiveRoutingTest, SelectOptimalSources_NullOutputArrays_ReturnsFalse) {
    // WHAT: Test NULL output array handling
    // WHY:  Ensure graceful error handling
    // HOW:  Pass NULL output arrays, expect false

    uint32_t num_selected = 0;

    bool success = spatial_neuromod_select_optimal_sources(
        field, network, &config, nullptr, nullptr, &num_selected);

    EXPECT_FALSE(success);
}

TEST_F(AdaptiveRoutingTest, SelectOptimalSources_QuantumShannonDisabled_ReturnsFalse) {
    // WHAT: Test behavior when quantum-Shannon disabled
    // WHY:  Selection requires Shannon metrics
    // HOW:  Disable quantum-Shannon, expect false

    field->use_quantum_shannon = false;

    uint32_t selected_ids[5];
    uint32_t num_selected = 0;

    bool success = spatial_neuromod_select_optimal_sources(
        field, network, &config, selected_ids, nullptr, &num_selected);

    EXPECT_FALSE(success);
}

TEST_F(AdaptiveRoutingTest, SelectOptimalSources_HighMinThreshold_ReturnsFewerSources) {
    // WHAT: Test that high threshold filters out low-scoring sources
    // WHY:  Validate threshold mechanism
    // HOW:  Set very high threshold, expect fewer or zero sources

    config.min_source_score = 0.99f;  // Very high threshold

    uint32_t selected_ids[5];
    uint32_t num_selected = 0;

    bool success = spatial_neuromod_select_optimal_sources(
        field, network, &config, selected_ids, nullptr, &num_selected);

    // May succeed with 0 sources or fail, both are valid
    if (success) {
        EXPECT_LE(num_selected, 1u);  // Very few sources should meet threshold
    }
}

//=============================================================================
// Test: spatial_neuromod_release_adaptive()
//=============================================================================

TEST_F(AdaptiveRoutingTest, ReleaseAdaptive_ValidParameters_ReturnsTrue) {
    // WHAT: Test adaptive release
    // WHY:  Verify release function works
    // HOW:  Call release, check success and concentration changes

    float initial_total = 0.0f;
    for (uint32_t i = 0; i < num_neurons; i++) {
        initial_total += field->concentration[i];
    }

    bool success = spatial_neuromod_release_adaptive(field, network, &config, 10.0f);
    EXPECT_TRUE(success);

    // Check that source_rate increased (at optimal neurons)
    float total_source_rate = 0.0f;
    for (uint32_t i = 0; i < num_neurons; i++) {
        total_source_rate += field->source_rate[i];
    }

    EXPECT_GT(total_source_rate, 0.0f);
    EXPECT_FLOAT_EQ(total_source_rate, 10.0f);  // Total amount should equal input
}

TEST_F(AdaptiveRoutingTest, ReleaseAdaptive_NullField_ReturnsFalse) {
    // WHAT: Test NULL field handling
    // WHY:  Ensure graceful error handling
    // HOW:  Pass NULL field, expect false

    bool success = spatial_neuromod_release_adaptive(nullptr, network, &config, 10.0f);
    EXPECT_FALSE(success);
}

TEST_F(AdaptiveRoutingTest, ReleaseAdaptive_NullNetwork_ReturnsFalse) {
    // WHAT: Test NULL network handling
    // WHY:  Ensure graceful error handling
    // HOW:  Pass NULL network, expect false

    bool success = spatial_neuromod_release_adaptive(field, nullptr, &config, 10.0f);
    EXPECT_FALSE(success);
}

TEST_F(AdaptiveRoutingTest, ReleaseAdaptive_NullConfig_ReturnsFalse) {
    // WHAT: Test NULL config handling
    // WHY:  Ensure graceful error handling
    // HOW:  Pass NULL config, expect false

    bool success = spatial_neuromod_release_adaptive(field, network, nullptr, 10.0f);
    EXPECT_FALSE(success);
}

TEST_F(AdaptiveRoutingTest, ReleaseAdaptive_AdaptiveRoutingDisabled_UsesFallback) {
    // WHAT: Test fallback when adaptive routing disabled
    // WHY:  Ensure graceful degradation
    // HOW:  Disable adaptive routing, expect fallback to middle neuron

    config.enable_adaptive_routing = false;

    bool success = spatial_neuromod_release_adaptive(field, network, &config, 10.0f);
    EXPECT_TRUE(success);

    // Check that middle neuron got the release (fallback behavior)
    uint32_t middle = num_neurons / 2;
    EXPECT_GT(field->source_rate[middle], 0.0f);
}

TEST_F(AdaptiveRoutingTest, ReleaseAdaptive_QuantumShannonDisabled_UsesFallback) {
    // WHAT: Test fallback when quantum-Shannon disabled
    // WHY:  Ensure graceful degradation
    // HOW:  Disable quantum-Shannon, expect fallback to middle neuron

    field->use_quantum_shannon = false;

    bool success = spatial_neuromod_release_adaptive(field, network, &config, 10.0f);
    EXPECT_TRUE(success);

    // Check that middle neuron got the release (fallback behavior)
    uint32_t middle = num_neurons / 2;
    EXPECT_GT(field->source_rate[middle], 0.0f);
}

//=============================================================================
// Test: spatial_neuromod_release_adaptive_batch()
//=============================================================================

TEST_F(AdaptiveRoutingTest, ReleaseAdaptiveBatch_ValidParameters_ReturnsTrue) {
    // WHAT: Test batch adaptive release
    // WHY:  Verify batch function works
    // HOW:  Call batch release, check success

    float amounts[] = {5.0f, 10.0f, 15.0f};
    uint32_t count = 3;

    bool success = spatial_neuromod_release_adaptive_batch(
        field, network, &config, amounts, count);

    EXPECT_TRUE(success);

    // Check total source rate
    float total_source_rate = 0.0f;
    for (uint32_t i = 0; i < num_neurons; i++) {
        total_source_rate += field->source_rate[i];
    }

    EXPECT_FLOAT_EQ(total_source_rate, 30.0f);  // Sum of amounts
}

TEST_F(AdaptiveRoutingTest, ReleaseAdaptiveBatch_NullField_ReturnsFalse) {
    // WHAT: Test NULL field handling
    // WHY:  Ensure graceful error handling
    // HOW:  Pass NULL field, expect false

    float amounts[] = {5.0f, 10.0f};

    bool success = spatial_neuromod_release_adaptive_batch(
        nullptr, network, &config, amounts, 2);

    EXPECT_FALSE(success);
}

TEST_F(AdaptiveRoutingTest, ReleaseAdaptiveBatch_NullAmounts_ReturnsFalse) {
    // WHAT: Test NULL amounts handling
    // WHY:  Ensure graceful error handling
    // HOW:  Pass NULL amounts, expect false

    bool success = spatial_neuromod_release_adaptive_batch(
        field, network, &config, nullptr, 2);

    EXPECT_FALSE(success);
}

TEST_F(AdaptiveRoutingTest, ReleaseAdaptiveBatch_ZeroCount_ReturnsTrue) {
    // WHAT: Test zero count handling
    // WHY:  Ensure edge case handling
    // HOW:  Pass count=0, expect true (nothing to do)

    float amounts[] = {5.0f};

    bool success = spatial_neuromod_release_adaptive_batch(
        field, network, &config, amounts, 0);

    EXPECT_TRUE(success);  // No releases, so success
}

//=============================================================================
// Test: Configuration Validation
//=============================================================================

TEST_F(AdaptiveRoutingTest, DefaultConfig_HasAdaptiveRoutingDisabled) {
    // WHAT: Test that adaptive routing is disabled by default
    // WHY:  Ensure backward compatibility
    // HOW:  Check default config

    spatial_neuromod_config_t default_config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
    EXPECT_FALSE(default_config.enable_adaptive_routing);
}

TEST_F(AdaptiveRoutingTest, DefaultConfig_HasValidWeights) {
    // WHAT: Test that default weights are sensible
    // WHY:  Ensure good out-of-box performance
    // HOW:  Check weight values

    spatial_neuromod_config_t default_config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);

    EXPECT_GT(default_config.efficiency_weight, 0.0f);
    EXPECT_GT(default_config.speedup_weight, 0.0f);
    EXPECT_GT(default_config.bottleneck_penalty_weight, 0.0f);
    EXPECT_GT(default_config.info_rate_weight, 0.0f);
    EXPECT_GT(default_config.num_adaptive_sources, 0u);
    EXPECT_GE(default_config.min_source_score, 0.0f);
    EXPECT_LE(default_config.min_source_score, 1.0f);
}

//=============================================================================
// Test: End-to-End Adaptive Routing
//=============================================================================

TEST_F(AdaptiveRoutingTest, EndToEnd_AdaptiveRouting_WorksCorrectly) {
    // WHAT: End-to-end test of adaptive routing
    // WHY:  Verify complete workflow
    // HOW:  Configure, release adaptively, update diffusion, check results

    // Enable adaptive routing and quantum-Shannon
    config.enable_adaptive_routing = true;
    field->use_quantum_shannon = true;

    // Release adaptively
    bool success = spatial_neuromod_release_adaptive(field, network, &config, 20.0f);
    ASSERT_TRUE(success);

    // Update diffusion
    success = spatial_neuromod_update(field, network, 0.01f);
    ASSERT_TRUE(success);

    // Check that concentration increased somewhere
    float max_concentration = 0.0f;
    for (uint32_t i = 0; i < num_neurons; i++) {
        if (field->concentration[i] > max_concentration) {
            max_concentration = field->concentration[i];
        }
    }

    EXPECT_GT(max_concentration, field->baseline);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
