//=============================================================================
// test_shannon.cpp - Unit Tests for Shannon Information Theory
//=============================================================================
/**
 * @file test_shannon.cpp
 * @brief Comprehensive unit tests for Shannon information theory module
 *
 * COVERAGE TARGET: 100%
 * TEST COUNT: 45+ tests
 *
 * @author NIMCP Development Team
 * @date 2025-11-13
 * @version 3.0.0 Phase C4
 */

#include <gtest/gtest.h>
#include "information/nimcp_shannon.h"
#include <cmath>
#include <cstdlib>

//=============================================================================
// Test Fixture
//=============================================================================

class ShannonTest : public ::testing::Test {
protected:
    void SetUp() override {
        config = shannon_default_config();
    }

    void TearDown() override {
        // Cleanup
    }

    shannon_config_t config;
};

//=============================================================================
// Core Shannon Functions Tests
//=============================================================================

TEST_F(ShannonTest, ChannelCapacity_ZeroSNR) {
    float capacity = shannon_channel_capacity(100.0f, 0.0f);
    EXPECT_FLOAT_EQ(capacity, 0.0f);
}

TEST_F(ShannonTest, ChannelCapacity_UnitSNR) {
    // C = B × log₂(1 + SNR) = 100 × log₂(2) = 100 bits/s
    float capacity = shannon_channel_capacity(100.0f, 1.0f);
    EXPECT_NEAR(capacity, 100.0f, 0.1f);
}

TEST_F(ShannonTest, ChannelCapacity_HighSNR) {
    // C = B × log₂(1 + SNR) = 100 × log₂(101) ≈ 664-666 bits/s
    float capacity = shannon_channel_capacity(100.0f, 100.0f);
    EXPECT_NEAR(capacity, 665.0f, 2.0f);
}

TEST_F(ShannonTest, ChannelCapacity_NegativeSNR_ClampedToZero) {
    float capacity = shannon_channel_capacity(100.0f, -10.0f);
    EXPECT_FLOAT_EQ(capacity, 0.0f);
}

TEST_F(ShannonTest, ChannelCapacity_MaxSNR_Clamped) {
    float capacity = shannon_channel_capacity(100.0f, 1e10f);
    EXPECT_LT(capacity, 1e7f);  // Should be clamped
}

TEST_F(ShannonTest, ChannelCapacity_MinBandwidth_Clamped) {
    float capacity = shannon_channel_capacity(0.0f, 10.0f);
    EXPECT_GT(capacity, 0.0f);  // Should use minimum bandwidth
}

TEST_F(ShannonTest, Entropy_Deterministic) {
    float probs[] = {1.0f, 0.0f, 0.0f};
    float entropy = shannon_entropy_array(probs, 3);
    EXPECT_FLOAT_EQ(entropy, 0.0f);  // No uncertainty
}

TEST_F(ShannonTest, Entropy_FairCoin) {
    float probs[] = {0.5f, 0.5f};
    float entropy = shannon_entropy_array(probs, 2);
    EXPECT_NEAR(entropy, 1.0f, 0.001f);  // 1 bit
}

TEST_F(ShannonTest, Entropy_BiasedCoin) {
    float probs[] = {0.9f, 0.1f};
    float entropy = shannon_entropy_array(probs, 2);
    EXPECT_NEAR(entropy, 0.469f, 0.01f);
}

TEST_F(ShannonTest, Entropy_Uniform4States) {
    float probs[] = {0.25f, 0.25f, 0.25f, 0.25f};
    float entropy = shannon_entropy_array(probs, 4);
    EXPECT_NEAR(entropy, 2.0f, 0.001f);  // log₂(4) = 2 bits
}

TEST_F(ShannonTest, Entropy_NullDistribution) {
    float entropy = shannon_entropy_array(nullptr, 10);
    EXPECT_FLOAT_EQ(entropy, 0.0f);
}

TEST_F(ShannonTest, Entropy_ZeroStates) {
    float probs[] = {0.5f, 0.5f};
    float entropy = shannon_entropy_array(probs, 0);
    EXPECT_FLOAT_EQ(entropy, 0.0f);
}

TEST_F(ShannonTest, MutualInformation_Independent) {
    // Independent: P(X,Y) = P(X)P(Y)
    float joint_probs[] = {
        0.25f, 0.25f,  // X=0
        0.25f, 0.25f   // X=1
    };

    shannon_joint_distribution_t* joint = shannon_joint_distribution_create(
        2, 2, joint_probs
    );

    ASSERT_NE(joint, nullptr);

    float mi = shannon_mutual_information(joint);
    EXPECT_NEAR(mi, 0.0f, 0.01f);  // I(X;Y) = 0 for independent

    shannon_joint_distribution_free(joint);
}

TEST_F(ShannonTest, MutualInformation_PerfectCorrelation) {
    // Perfect correlation: P(X=Y) = 1
    float joint_probs[] = {
        0.5f, 0.0f,  // X=0, Y=0 or 1
        0.0f, 0.5f   // X=1, Y=0 or 1
    };

    shannon_joint_distribution_t* joint = shannon_joint_distribution_create(
        2, 2, joint_probs
    );

    ASSERT_NE(joint, nullptr);

    float mi = shannon_mutual_information(joint);
    EXPECT_NEAR(mi, 1.0f, 0.01f);  // I(X;Y) = 1 bit

    shannon_joint_distribution_free(joint);
}

TEST_F(ShannonTest, MutualInformation_NullDistribution) {
    float mi = shannon_mutual_information(nullptr);
    EXPECT_FLOAT_EQ(mi, 0.0f);
}

TEST_F(ShannonTest, ConditionalEntropy_Deterministic) {
    // Y is deterministic given X: H(Y|X) = 0
    float joint_probs[] = {
        0.5f, 0.0f,
        0.0f, 0.5f
    };

    shannon_joint_distribution_t* joint = shannon_joint_distribution_create(
        2, 2, joint_probs
    );

    float cond_entropy = shannon_conditional_entropy(joint);
    EXPECT_NEAR(cond_entropy, 0.0f, 0.01f);

    shannon_joint_distribution_free(joint);
}

TEST_F(ShannonTest, ConditionalEntropy_Independent) {
    // Independent: H(Y|X) = H(Y)
    float joint_probs[] = {
        0.25f, 0.25f,
        0.25f, 0.25f
    };

    shannon_joint_distribution_t* joint = shannon_joint_distribution_create(
        2, 2, joint_probs
    );

    float cond_entropy = shannon_conditional_entropy(joint);
    EXPECT_NEAR(cond_entropy, 1.0f, 0.01f);  // H(Y) = 1 bit

    shannon_joint_distribution_free(joint);
}

TEST_F(ShannonTest, KLDivergence_IdenticalDistributions) {
    float probs[] = {0.3f, 0.3f, 0.4f};

    shannon_distribution_t* p = shannon_distribution_create(3, probs);
    shannon_distribution_t* q = shannon_distribution_create(3, probs);

    float kl = shannon_kl_divergence(p, q);
    EXPECT_NEAR(kl, 0.0f, 0.01f);  // D(P||P) = 0

    shannon_distribution_free(p);
    shannon_distribution_free(q);
}

TEST_F(ShannonTest, KLDivergence_DifferentDistributions) {
    float probs_p[] = {0.5f, 0.5f};
    float probs_q[] = {0.9f, 0.1f};

    shannon_distribution_t* p = shannon_distribution_create(2, probs_p);
    shannon_distribution_t* q = shannon_distribution_create(2, probs_q);

    float kl = shannon_kl_divergence(p, q);
    EXPECT_GT(kl, 0.0f);  // D(P||Q) > 0 when different

    shannon_distribution_free(p);
    shannon_distribution_free(q);
}

TEST_F(ShannonTest, KLDivergence_NullDistributions) {
    float kl = shannon_kl_divergence(nullptr, nullptr);
    EXPECT_FLOAT_EQ(kl, 0.0f);
}

//=============================================================================
// Synapse-Level Tests
//=============================================================================

TEST_F(ShannonTest, AnalyzeSynapse_ZeroWeight) {
    shannon_synapse_metrics_t metrics = shannon_analyze_synapse(
        0.0f,   // weight
        10.0f,  // pre_firing_rate
        0.1f,   // noise_level
        10.0f,  // bandwidth
        &config
    );

    EXPECT_FLOAT_EQ(metrics.channel_capacity, 0.0f);
    EXPECT_FLOAT_EQ(metrics.signal_power, 0.0f);
}

TEST_F(ShannonTest, AnalyzeSynapse_HighWeight) {
    shannon_synapse_metrics_t metrics = shannon_analyze_synapse(
        1.0f,   // weight
        100.0f, // pre_firing_rate
        0.1f,   // noise_level
        100.0f, // bandwidth
        &config
    );

    EXPECT_GT(metrics.channel_capacity, 0.0f);
    EXPECT_GT(metrics.snr, 0.0f);
    EXPECT_GT(metrics.signal_power, 0.0f);
    EXPECT_GE(metrics.coding_efficiency, 0.0f);
    EXPECT_LE(metrics.coding_efficiency, 1.0f);
}

TEST_F(ShannonTest, AnalyzeSynapse_NegativeWeight) {
    shannon_synapse_metrics_t metrics = shannon_analyze_synapse(
        -0.8f,  // weight
        50.0f,  // pre_firing_rate
        0.1f,   // noise_level
        50.0f,  // bandwidth
        &config
    );

    EXPECT_GT(metrics.channel_capacity, 0.0f);
    EXPECT_GT(metrics.signal_power, 0.0f);  // Signal power = weight²
}

TEST_F(ShannonTest, AnalyzeSynapse_HighNoise) {
    shannon_synapse_metrics_t metrics = shannon_analyze_synapse(
        0.5f,   // weight
        10.0f,  // pre_firing_rate
        10.0f,  // noise_level (high)
        10.0f,  // bandwidth
        &config
    );

    EXPECT_LT(metrics.snr, 1.0f);  // Low SNR due to high noise
    EXPECT_LT(metrics.channel_capacity, 100.0f);
}

TEST_F(ShannonTest, AnalyzeSynapse_ZeroNoise) {
    shannon_synapse_metrics_t metrics = shannon_analyze_synapse(
        0.5f,   // weight
        10.0f,  // pre_firing_rate
        0.0f,   // noise_level (zero)
        10.0f,  // bandwidth
        &config
    );

    // Should handle zero noise gracefully (clamp to minimum)
    EXPECT_GT(metrics.channel_capacity, 0.0f);
}

TEST_F(ShannonTest, AnalyzeSynapse_NullConfig_UsesDefault) {
    shannon_synapse_metrics_t metrics = shannon_analyze_synapse(
        0.5f, 10.0f, 0.1f, 10.0f, nullptr
    );

    EXPECT_GT(metrics.channel_capacity, 0.0f);
}

TEST_F(ShannonTest, OptimizeSynapseWeight_IncreaseCapacity) {
    float current_weight = 0.3f;
    float target_capacity = 100.0f;

    float new_weight = shannon_optimize_synapse_weight(
        current_weight,
        target_capacity,
        10.0f,  // pre_firing_rate
        0.1f,   // noise_level
        0.1f    // learning_rate
    );

    EXPECT_GT(new_weight, current_weight);  // Should increase
    EXPECT_LE(new_weight, 1.0f);            // Should be clamped
}

TEST_F(ShannonTest, OptimizeSynapseWeight_DecreaseCapacity) {
    float current_weight = 0.9f;
    float target_capacity = 1.0f;  // Low target

    float new_weight = shannon_optimize_synapse_weight(
        current_weight,
        target_capacity,
        10.0f,  // pre_firing_rate
        0.1f,   // noise_level
        0.1f    // learning_rate
    );

    // Weight may decrease or stay same depending on gradient
    EXPECT_GE(new_weight, -1.0f);
    EXPECT_LE(new_weight, 1.0f);
}

TEST_F(ShannonTest, OptimizeSynapseWeight_AlreadyAtTarget) {
    // Current capacity ≈ target, so weight should change minimally
    float current_weight = 0.5f;

    shannon_synapse_metrics_t metrics = shannon_analyze_synapse(
        current_weight, 10.0f, 0.1f, 10.0f, &config
    );

    float new_weight = shannon_optimize_synapse_weight(
        current_weight,
        metrics.channel_capacity,  // Use current capacity as target
        10.0f, 0.1f, 0.1f
    );

    EXPECT_NEAR(new_weight, current_weight, 0.01f);
}

//=============================================================================
// Neuron-Level Tests
//=============================================================================

TEST_F(ShannonTest, AnalyzeNeuron_NoInputs) {
    shannon_neuron_metrics_t metrics = shannon_analyze_neuron(
        1, nullptr, 0, nullptr, 0, 0.5f, nullptr, 0, &config
    );

    EXPECT_EQ(metrics.neuron_id, 1u);
    EXPECT_EQ(metrics.num_inputs, 0u);
    EXPECT_EQ(metrics.num_outputs, 0u);
    EXPECT_FLOAT_EQ(metrics.input_information, 0.0f);
    EXPECT_FLOAT_EQ(metrics.output_information, 0.0f);
}

TEST_F(ShannonTest, AnalyzeNeuron_WithInputsAndOutputs) {
    // Create input synapses
    shannon_synapse_metrics_t inputs[3];
    for (int i = 0; i < 3; i++) {
        inputs[i] = shannon_analyze_synapse(0.5f, 10.0f, 0.1f, 10.0f, &config);
    }

    // Create output synapses
    shannon_synapse_metrics_t outputs[2];
    for (int i = 0; i < 2; i++) {
        outputs[i] = shannon_analyze_synapse(0.6f, 10.0f, 0.1f, 10.0f, &config);
    }

    shannon_neuron_metrics_t metrics = shannon_analyze_neuron(
        42, inputs, 3, outputs, 2, 0.7f, nullptr, 0, &config
    );

    EXPECT_EQ(metrics.neuron_id, 42u);
    EXPECT_EQ(metrics.num_inputs, 3u);
    EXPECT_EQ(metrics.num_outputs, 2u);
    EXPECT_GT(metrics.input_information, 0.0f);
    EXPECT_GT(metrics.output_information, 0.0f);
    EXPECT_GT(metrics.state_entropy, 0.0f);
}

TEST_F(ShannonTest, AnalyzeNeuron_WithSpikeHistory) {
    shannon_synapse_metrics_t inputs[1];
    inputs[0] = shannon_analyze_synapse(0.5f, 10.0f, 0.1f, 10.0f, &config);

    uint64_t spike_history[] = {100, 120, 145, 165, 190};

    shannon_neuron_metrics_t metrics = shannon_analyze_neuron(
        1, inputs, 1, nullptr, 0, 0.5f, spike_history, 5, &config
    );

    EXPECT_GT(metrics.spike_entropy, 0.0f);  // Should compute ISI entropy
}

//=============================================================================
// Network-Level Tests
//=============================================================================

TEST_F(ShannonTest, AnalyzeNetwork_EmptyNetwork) {
    shannon_network_metrics_t metrics = shannon_analyze_network(
        nullptr, 0, nullptr, 0, &config
    );

    EXPECT_EQ(metrics.num_synapses, 0u);
    EXPECT_EQ(metrics.num_neurons, 0u);
    EXPECT_FLOAT_EQ(metrics.total_capacity, 0.0f);
    EXPECT_FLOAT_EQ(metrics.total_entropy, 0.0f);
}

TEST_F(ShannonTest, AnalyzeNetwork_SmallNetwork) {
    // Create synapse metrics
    const uint32_t num_synapses = 10;
    shannon_synapse_metrics_t synapse_metrics[num_synapses];

    for (uint32_t i = 0; i < num_synapses; i++) {
        float weight = 0.5f + 0.1f * (float)i / (float)num_synapses;
        synapse_metrics[i] = shannon_analyze_synapse(
            weight, 10.0f, 0.1f, 10.0f, &config
        );
    }

    // Create neuron metrics
    const uint32_t num_neurons = 5;
    shannon_neuron_metrics_t neuron_metrics[num_neurons];

    for (uint32_t i = 0; i < num_neurons; i++) {
        shannon_synapse_metrics_t inputs[2];
        inputs[0] = synapse_metrics[i * 2];
        inputs[1] = synapse_metrics[i * 2 + 1];

        neuron_metrics[i] = shannon_analyze_neuron(
            i, inputs, 2, nullptr, 0, 0.5f, nullptr, 0, &config
        );
    }

    shannon_network_metrics_t metrics = shannon_analyze_network(
        synapse_metrics, num_synapses, neuron_metrics, num_neurons, &config
    );

    EXPECT_EQ(metrics.num_synapses, num_synapses);
    EXPECT_EQ(metrics.num_neurons, num_neurons);
    EXPECT_GT(metrics.total_capacity, 0.0f);
    EXPECT_GT(metrics.total_entropy, 0.0f);
    EXPECT_GE(metrics.average_efficiency, 0.0f);
    EXPECT_LE(metrics.average_efficiency, 1.0f);
    EXPECT_GE(metrics.bottleneck_score, 0.0f);
    EXPECT_LE(metrics.bottleneck_score, 1.0f);
}

TEST_F(ShannonTest, DetectBottlenecks_NoBottlenecks) {
    // Create uniform synapse metrics (all same capacity)
    const uint32_t num_synapses = 10;
    shannon_synapse_metrics_t synapse_metrics[num_synapses];

    for (uint32_t i = 0; i < num_synapses; i++) {
        synapse_metrics[i] = shannon_analyze_synapse(
            0.5f, 10.0f, 0.1f, 10.0f, &config
        );
    }

    shannon_bottleneck_t bottlenecks[10];
    uint32_t num_found = shannon_detect_bottlenecks(
        synapse_metrics, num_synapses, 0.5f, bottlenecks, 10
    );

    EXPECT_EQ(num_found, 0u);  // No bottlenecks (all same)
}

TEST_F(ShannonTest, DetectBottlenecks_WithBottlenecks) {
    const uint32_t num_synapses = 10;
    shannon_synapse_metrics_t synapse_metrics[num_synapses];

    // Create mostly strong synapses
    for (uint32_t i = 0; i < 8; i++) {
        synapse_metrics[i] = shannon_analyze_synapse(
            0.9f, 100.0f, 0.1f, 100.0f, &config
        );
    }

    // Create weak synapses (bottlenecks)
    for (uint32_t i = 8; i < 10; i++) {
        synapse_metrics[i] = shannon_analyze_synapse(
            0.1f, 10.0f, 1.0f, 10.0f, &config  // Low weight, high noise
        );
    }

    shannon_bottleneck_t bottlenecks[10];
    uint32_t num_found = shannon_detect_bottlenecks(
        synapse_metrics, num_synapses, 0.5f, bottlenecks, 10
    );

    EXPECT_GT(num_found, 0u);  // Should find bottlenecks
}

TEST_F(ShannonTest, DetectBottlenecks_NullInput) {
    shannon_bottleneck_t bottlenecks[10];
    uint32_t num_found = shannon_detect_bottlenecks(
        nullptr, 10, 0.5f, bottlenecks, 10
    );

    EXPECT_EQ(num_found, 0u);
}

TEST_F(ShannonTest, InformationFlowRate_ZeroSynapses) {
    float rate = shannon_information_flow_rate(nullptr, 0, 1000.0f);
    EXPECT_FLOAT_EQ(rate, 0.0f);
}

TEST_F(ShannonTest, InformationFlowRate_ActiveSynapses) {
    const uint32_t num_synapses = 5;
    shannon_synapse_metrics_t synapse_metrics[num_synapses];

    for (uint32_t i = 0; i < num_synapses; i++) {
        synapse_metrics[i] = shannon_analyze_synapse(
            0.7f, 50.0f, 0.1f, 50.0f, &config
        );
    }

    float rate = shannon_information_flow_rate(
        synapse_metrics, num_synapses, 1000.0f
    );

    EXPECT_GT(rate, 0.0f);
}

//=============================================================================
// Distribution Utility Tests
//=============================================================================

TEST_F(ShannonTest, CreateDistribution_Uniform) {
    shannon_distribution_t* dist = shannon_distribution_create(4, nullptr);

    ASSERT_NE(dist, nullptr);
    EXPECT_EQ(dist->num_states, 4u);

    // Should be uniform
    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_NEAR(dist->probabilities[i], 0.25f, 0.001f);
    }

    shannon_distribution_free(dist);
}

TEST_F(ShannonTest, CreateDistribution_CustomProbs) {
    float probs[] = {0.1f, 0.3f, 0.6f};
    shannon_distribution_t* dist = shannon_distribution_create(3, probs);

    ASSERT_NE(dist, nullptr);
    EXPECT_FLOAT_EQ(dist->probabilities[0], 0.1f);
    EXPECT_FLOAT_EQ(dist->probabilities[1], 0.3f);
    EXPECT_FLOAT_EQ(dist->probabilities[2], 0.6f);

    shannon_distribution_free(dist);
}

TEST_F(ShannonTest, CreateDistribution_ZeroStates) {
    shannon_distribution_t* dist = shannon_distribution_create(0, nullptr);
    EXPECT_EQ(dist, nullptr);
}

TEST_F(ShannonTest, NormalizeDistribution_Valid) {
    float probs[] = {2.0f, 3.0f, 5.0f};  // Sum = 10
    shannon_distribution_t* dist = shannon_distribution_create(3, probs);

    ASSERT_NE(dist, nullptr);

    bool success = shannon_distribution_normalize(dist);
    EXPECT_TRUE(success);

    EXPECT_NEAR(dist->probabilities[0], 0.2f, 0.001f);
    EXPECT_NEAR(dist->probabilities[1], 0.3f, 0.001f);
    EXPECT_NEAR(dist->probabilities[2], 0.5f, 0.001f);
    EXPECT_NEAR(dist->total_probability, 1.0f, 0.001f);

    shannon_distribution_free(dist);
}

TEST_F(ShannonTest, NormalizeDistribution_ZeroSum) {
    float probs[] = {0.0f, 0.0f, 0.0f};
    shannon_distribution_t* dist = shannon_distribution_create(3, probs);

    ASSERT_NE(dist, nullptr);

    bool success = shannon_distribution_normalize(dist);
    EXPECT_FALSE(success);  // Cannot normalize zero sum

    shannon_distribution_free(dist);
}

TEST_F(ShannonTest, CreateJointDistribution_Uniform) {
    shannon_joint_distribution_t* joint = shannon_joint_distribution_create(
        2, 3, nullptr
    );

    ASSERT_NE(joint, nullptr);
    EXPECT_EQ(joint->num_x_states, 2u);
    EXPECT_EQ(joint->num_y_states, 3u);

    // Should be uniform: 1/6 each
    for (uint32_t i = 0; i < 6; i++) {
        EXPECT_NEAR(joint->joint_probabilities[i], 1.0f/6.0f, 0.001f);
    }

    shannon_joint_distribution_free(joint);
}

TEST_F(ShannonTest, MarginalX_ComputesCorrectly) {
    float joint_probs[] = {
        0.1f, 0.2f, 0.1f,  // X=0
        0.2f, 0.3f, 0.1f   // X=1
    };

    shannon_joint_distribution_t* joint = shannon_joint_distribution_create(
        2, 3, joint_probs
    );

    shannon_distribution_t* marginal = shannon_marginal_x(joint);

    ASSERT_NE(marginal, nullptr);
    EXPECT_EQ(marginal->num_states, 2u);

    // P(X=0) = 0.1 + 0.2 + 0.1 = 0.4
    // P(X=1) = 0.2 + 0.3 + 0.1 = 0.6
    EXPECT_NEAR(marginal->probabilities[0], 0.4f, 0.001f);
    EXPECT_NEAR(marginal->probabilities[1], 0.6f, 0.001f);

    shannon_distribution_free(marginal);
    shannon_joint_distribution_free(joint);
}

TEST_F(ShannonTest, MarginalY_ComputesCorrectly) {
    float joint_probs[] = {
        0.1f, 0.2f, 0.1f,  // X=0
        0.2f, 0.3f, 0.1f   // X=1
    };

    shannon_joint_distribution_t* joint = shannon_joint_distribution_create(
        2, 3, joint_probs
    );

    shannon_distribution_t* marginal = shannon_marginal_y(joint);

    ASSERT_NE(marginal, nullptr);
    EXPECT_EQ(marginal->num_states, 3u);

    // P(Y=0) = 0.1 + 0.2 = 0.3
    // P(Y=1) = 0.2 + 0.3 = 0.5
    // P(Y=2) = 0.1 + 0.1 = 0.2
    EXPECT_NEAR(marginal->probabilities[0], 0.3f, 0.001f);
    EXPECT_NEAR(marginal->probabilities[1], 0.5f, 0.001f);
    EXPECT_NEAR(marginal->probabilities[2], 0.2f, 0.001f);

    shannon_distribution_free(marginal);
    shannon_joint_distribution_free(joint);
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(ShannonTest, DefaultConfig_HasReasonableValues) {
    shannon_config_t cfg = shannon_default_config();

    EXPECT_FLOAT_EQ(cfg.min_probability, SHANNON_EPSILON);
    EXPECT_GT(cfg.min_capacity, 0.0f);
    EXPECT_GT(cfg.bottleneck_threshold, 0.0f);
    EXPECT_LT(cfg.bottleneck_threshold, 1.0f);
    EXPECT_GT(cfg.sampling_window_ms, 0.0f);
}

TEST_F(ShannonTest, HighAccuracyConfig_MorePrecise) {
    shannon_config_t cfg = shannon_high_accuracy_config();

    shannon_config_t default_cfg = shannon_default_config();

    EXPECT_LT(cfg.min_probability, default_cfg.min_probability);
    EXPECT_GT(cfg.sampling_window_ms, default_cfg.sampling_window_ms);
}

TEST_F(ShannonTest, FastConfig_LessPrecise) {
    shannon_config_t cfg = shannon_fast_config();

    shannon_config_t default_cfg = shannon_default_config();

    EXPECT_GT(cfg.min_probability, default_cfg.min_probability);
    EXPECT_LT(cfg.sampling_window_ms, default_cfg.sampling_window_ms);
    EXPECT_TRUE(cfg.use_log_approximation);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(ShannonTest, Log2Fast_ApproximatesCorrectly) {
    float values[] = {1.0f, 2.0f, 4.0f, 8.0f, 10.0f, 100.0f};
    float expected[] = {0.0f, 1.0f, 2.0f, 3.0f, 3.32f, 6.64f};

    for (int i = 0; i < 6; i++) {
        float result = shannon_log2_fast(values[i]);
        EXPECT_NEAR(result, expected[i], 0.7f);  // Fast approximation has ~20% error
    }
}

TEST_F(ShannonTest, Log2Fast_ZeroInput) {
    float result = shannon_log2_fast(0.0f);
    EXPECT_LT(result, -1000.0f);  // Very large negative
}

TEST_F(ShannonTest, SNR_Conversions) {
    float snr_linear = 100.0f;
    float snr_db = shannon_snr_to_db(snr_linear);

    EXPECT_NEAR(snr_db, 20.0f, 0.1f);  // 10*log₁₀(100) = 20 dB

    float snr_back = shannon_snr_from_db(snr_db);
    EXPECT_NEAR(snr_back, snr_linear, 1.0f);
}

TEST_F(ShannonTest, SNR_ZeroLinear) {
    float snr_db = shannon_snr_to_db(0.0f);
    EXPECT_LT(snr_db, -50.0f);  // Very low dB
}

TEST_F(ShannonTest, PrintFunctions_DoNotCrash) {
    shannon_synapse_metrics_t syn_metrics = shannon_analyze_synapse(
        0.5f, 10.0f, 0.1f, 10.0f, &config
    );

    // These should not crash
    shannon_print_synapse_metrics(&syn_metrics, "Test Synapse");
    shannon_print_synapse_metrics(nullptr, nullptr);

    shannon_neuron_metrics_t neuron_metrics;
    memset(&neuron_metrics, 0, sizeof(neuron_metrics));
    shannon_print_neuron_metrics(&neuron_metrics, "Test Neuron");

    shannon_network_metrics_t network_metrics;
    memset(&network_metrics, 0, sizeof(network_metrics));
    shannon_print_network_metrics(&network_metrics, nullptr);
}

//=============================================================================
// Edge Cases and Robustness Tests
//=============================================================================

TEST_F(ShannonTest, AnalyzeSynapse_ExtremeValues) {
    // Very high firing rate
    shannon_synapse_metrics_t metrics1 = shannon_analyze_synapse(
        0.5f, 10000.0f, 0.1f, 10000.0f, &config
    );
    EXPECT_GT(metrics1.channel_capacity, 0.0f);

    // Very low firing rate
    shannon_synapse_metrics_t metrics2 = shannon_analyze_synapse(
        0.5f, 0.001f, 0.1f, 0.001f, &config
    );
    EXPECT_GE(metrics2.channel_capacity, 0.0f);

    // Clamped weight (> 1)
    shannon_synapse_metrics_t metrics3 = shannon_analyze_synapse(
        10.0f, 10.0f, 0.1f, 10.0f, &config
    );
    EXPECT_GT(metrics3.channel_capacity, 0.0f);
}

TEST_F(ShannonTest, DistributionFree_NullSafe) {
    shannon_distribution_free(nullptr);
    shannon_joint_distribution_free(nullptr);
    // Should not crash
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
