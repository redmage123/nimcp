//=============================================================================
// test_statistics_shannon_integration.cpp - Integration Tests for Stats-Shannon
//=============================================================================
/**
 * @file test_statistics_shannon_integration.cpp
 * @brief Integration tests verifying statistics and Shannon modules work together
 *
 * WHAT: Cross-module integration tests
 * WHY:  Ensure consistent behavior between statistics and Shannon implementations
 * HOW:  Test equivalent functions produce identical results
 *
 * @date 2026-01-30
 */

#include <gtest/gtest.h>
#include "utils/statistics/nimcp_statistics.h"
#include "information/nimcp_shannon.h"
#include <cmath>
#include <vector>
#include <random>

// Test tolerances
#define TOLERANCE 1e-5f
#define LOOSE_TOLERANCE 1e-3f

//=============================================================================
// Test Fixture
//=============================================================================

class StatsShannonIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        stats_config = nimcp_stats_default_config();
        nimcp_stats_init(&stats_config);
        shannon_config = shannon_default_config();
    }

    void TearDown() override {
        nimcp_stats_shutdown();
    }

    nimcp_stats_config_t stats_config;
    shannon_config_t shannon_config;

    // Helper to create Shannon distribution from array
    shannon_distribution_t* create_shannon_dist(const float* probs, uint32_t n) {
        return shannon_distribution_create(n, probs);
    }
};

//=============================================================================
// Entropy Consistency Tests
//=============================================================================

TEST_F(StatsShannonIntegrationTest, Entropy_BitsConsistency) {
    // Both modules should compute identical entropy in bits
    float probs[] = {0.25f, 0.25f, 0.25f, 0.25f};

    // Statistics module
    float stats_entropy = nimcp_stats_entropy(probs, 4);

    // Shannon module
    float shannon_entropy = shannon_entropy_array(probs, 4);

    // Should be exactly equal (same formula)
    EXPECT_NEAR(stats_entropy, shannon_entropy, TOLERANCE);
    EXPECT_NEAR(stats_entropy, 2.0f, TOLERANCE);  // log2(4) = 2 bits
}

TEST_F(StatsShannonIntegrationTest, Entropy_NatsConsistency) {
    // Nats versions should be consistent: H_nats = H_bits * ln(2)
    float probs[] = {0.5f, 0.5f};

    float stats_nats = nimcp_stats_entropy_nats(probs, 2);
    float shannon_nats = shannon_entropy_nats_array(probs, 2);

    EXPECT_NEAR(stats_nats, shannon_nats, TOLERANCE);

    // Verify relationship: H_nats = H_bits * ln(2)
    float stats_bits = nimcp_stats_entropy(probs, 2);
    EXPECT_NEAR(stats_nats, stats_bits * std::log(2.0f), TOLERANCE);
}

TEST_F(StatsShannonIntegrationTest, Entropy_NonUniformDistribution) {
    float probs[] = {0.7f, 0.2f, 0.1f};

    float stats_entropy = nimcp_stats_entropy(probs, 3);
    float shannon_entropy = shannon_entropy_array(probs, 3);

    EXPECT_NEAR(stats_entropy, shannon_entropy, TOLERANCE);
    EXPECT_LT(stats_entropy, std::log2(3.0f));  // Less than max entropy
    EXPECT_GT(stats_entropy, 0.0f);
}

//=============================================================================
// KL Divergence Consistency Tests
//=============================================================================

TEST_F(StatsShannonIntegrationTest, KLDivergence_Consistency) {
    float p[] = {0.6f, 0.4f};
    float q[] = {0.5f, 0.5f};

    // Statistics module
    float stats_kl = nimcp_stats_kl_divergence(p, q, 2);

    // Shannon module
    shannon_distribution_t* p_dist = create_shannon_dist(p, 2);
    shannon_distribution_t* q_dist = create_shannon_dist(q, 2);

    float shannon_kl = shannon_kl_divergence(p_dist, q_dist);

    shannon_distribution_free(p_dist);
    shannon_distribution_free(q_dist);

    EXPECT_NEAR(stats_kl, shannon_kl, TOLERANCE);
}

TEST_F(StatsShannonIntegrationTest, KLDivergence_Asymmetric) {
    float p[] = {0.9f, 0.1f};
    float q[] = {0.5f, 0.5f};

    float kl_pq = nimcp_stats_kl_divergence(p, q, 2);
    float kl_qp = nimcp_stats_kl_divergence(q, p, 2);

    // KL divergence is asymmetric
    EXPECT_NE(kl_pq, kl_qp);
    EXPECT_GT(kl_pq, 0.0f);
    EXPECT_GT(kl_qp, 0.0f);
}

//=============================================================================
// JS Divergence Consistency Tests
//=============================================================================

TEST_F(StatsShannonIntegrationTest, JSDivergence_Consistency) {
    float p[] = {0.6f, 0.4f};
    float q[] = {0.4f, 0.6f};

    // Statistics module
    float stats_js = nimcp_stats_js_divergence(p, q, 2);

    // Shannon module
    shannon_distribution_t* p_dist = create_shannon_dist(p, 2);
    shannon_distribution_t* q_dist = create_shannon_dist(q, 2);

    float shannon_js = shannon_js_divergence(p_dist, q_dist);

    shannon_distribution_free(p_dist);
    shannon_distribution_free(q_dist);

    EXPECT_NEAR(stats_js, shannon_js, TOLERANCE);
}

TEST_F(StatsShannonIntegrationTest, JSDivergence_Symmetric) {
    float p[] = {0.7f, 0.3f};
    float q[] = {0.3f, 0.7f};

    float js_pq = nimcp_stats_js_divergence(p, q, 2);
    float js_qp = nimcp_stats_js_divergence(q, p, 2);

    // JS divergence is symmetric
    EXPECT_NEAR(js_pq, js_qp, TOLERANCE);
}

TEST_F(StatsShannonIntegrationTest, JSDivergence_Bounded) {
    float p[] = {1.0f, 0.0f};
    float q[] = {0.0f, 1.0f};

    float js = nimcp_stats_js_divergence(p, q, 2);

    // JS divergence is bounded by 1 bit (log2(2))
    EXPECT_LE(js, 1.0f + TOLERANCE);
    EXPECT_GE(js, 0.0f);
}

//=============================================================================
// Cross Entropy Consistency Tests
//=============================================================================

TEST_F(StatsShannonIntegrationTest, CrossEntropy_Consistency) {
    float p[] = {0.6f, 0.4f};
    float q[] = {0.5f, 0.5f};

    // Statistics module
    float stats_ce = nimcp_stats_cross_entropy(p, q, 2);

    // Shannon module
    shannon_distribution_t* p_dist = create_shannon_dist(p, 2);
    shannon_distribution_t* q_dist = create_shannon_dist(q, 2);

    float shannon_ce = shannon_cross_entropy(p_dist, q_dist);

    shannon_distribution_free(p_dist);
    shannon_distribution_free(q_dist);

    EXPECT_NEAR(stats_ce, shannon_ce, TOLERANCE);
}

TEST_F(StatsShannonIntegrationTest, CrossEntropy_RelationToKL) {
    float p[] = {0.6f, 0.4f};
    float q[] = {0.5f, 0.5f};

    float ce = nimcp_stats_cross_entropy(p, q, 2);
    float h_p = nimcp_stats_entropy(p, 2);
    float kl = nimcp_stats_kl_divergence(p, q, 2);

    // H(P,Q) = H(P) + D_KL(P||Q)
    EXPECT_NEAR(ce, h_p + kl, TOLERANCE);
}

//=============================================================================
// Mutual Information Consistency Tests
//=============================================================================

TEST_F(StatsShannonIntegrationTest, MutualInformation_Consistency) {
    // Joint distribution P(X,Y)
    float joint[4] = {0.4f, 0.1f, 0.1f, 0.4f};

    // Statistics module
    float stats_mi = nimcp_stats_mutual_information(joint, 2, 2);

    // Shannon module
    shannon_joint_distribution_t shannon_joint;
    shannon_joint.joint_probabilities = joint;
    shannon_joint.num_x_states = 2;
    shannon_joint.num_y_states = 2;
    shannon_joint.total_probability = 1.0f;

    float shannon_mi = shannon_mutual_information(&shannon_joint);

    EXPECT_NEAR(stats_mi, shannon_mi, TOLERANCE);
}

TEST_F(StatsShannonIntegrationTest, MutualInformation_Independent) {
    // Independent X and Y
    float joint[4] = {0.25f, 0.25f, 0.25f, 0.25f};

    float mi = nimcp_stats_mutual_information(joint, 2, 2);

    // MI = 0 for independent variables
    EXPECT_NEAR(mi, 0.0f, TOLERANCE);
}

TEST_F(StatsShannonIntegrationTest, MutualInformation_PerfectCorrelation) {
    // Perfectly correlated: P(X=Y) = 1
    float joint[4] = {0.5f, 0.0f, 0.0f, 0.5f};

    float mi = nimcp_stats_mutual_information(joint, 2, 2);

    // MI = H(X) = H(Y) = 1 bit for uniform marginals
    EXPECT_NEAR(mi, 1.0f, TOLERANCE);
}

//=============================================================================
// Channel Capacity Tests
//=============================================================================

TEST_F(StatsShannonIntegrationTest, ChannelCapacity_Consistency) {
    float bandwidth = 100.0f;
    float snr = 10.0f;

    // Statistics module
    float stats_capacity = nimcp_stats_channel_capacity(bandwidth, snr);

    // Shannon module
    float shannon_capacity = shannon_channel_capacity(bandwidth, snr);

    EXPECT_NEAR(stats_capacity, shannon_capacity, TOLERANCE);
}

TEST_F(StatsShannonIntegrationTest, ChannelCapacity_DoublingBandwidth) {
    float snr = 10.0f;

    float c1 = nimcp_stats_channel_capacity(100.0f, snr);
    float c2 = nimcp_stats_channel_capacity(200.0f, snr);

    // Doubling bandwidth doubles capacity
    EXPECT_NEAR(c2, 2.0f * c1, TOLERANCE);
}

TEST_F(StatsShannonIntegrationTest, ChannelCapacity_SNRIncrease) {
    float bandwidth = 100.0f;

    float c1 = nimcp_stats_channel_capacity(bandwidth, 10.0f);
    float c2 = nimcp_stats_channel_capacity(bandwidth, 100.0f);

    // Higher SNR increases capacity (logarithmically)
    EXPECT_GT(c2, c1);
    EXPECT_LT(c2, 2.0f * c1);  // Not linear
}

//=============================================================================
// Conditional Entropy Consistency Tests
//=============================================================================

TEST_F(StatsShannonIntegrationTest, ConditionalEntropy_Consistency) {
    float joint[4] = {0.4f, 0.1f, 0.1f, 0.4f};

    // Statistics module
    float stats_cond = nimcp_stats_conditional_entropy(joint, 2, 2);

    // Shannon module
    shannon_joint_distribution_t shannon_joint;
    shannon_joint.joint_probabilities = joint;
    shannon_joint.num_x_states = 2;
    shannon_joint.num_y_states = 2;
    shannon_joint.total_probability = 1.0f;

    float shannon_cond = shannon_conditional_entropy(&shannon_joint);

    EXPECT_NEAR(stats_cond, shannon_cond, TOLERANCE);
}

TEST_F(StatsShannonIntegrationTest, ConditionalEntropy_Relation) {
    float joint[4] = {0.3f, 0.2f, 0.2f, 0.3f};

    float h_xy = nimcp_stats_joint_entropy(joint, 2, 2);
    float h_y_given_x = nimcp_stats_conditional_entropy(joint, 2, 2);
    float mi = nimcp_stats_mutual_information(joint, 2, 2);

    // Compute H(X) from marginal
    float p_x[2] = {joint[0] + joint[1], joint[2] + joint[3]};
    float h_x = nimcp_stats_entropy(p_x, 2);

    // H(Y|X) = H(X,Y) - H(X)
    EXPECT_NEAR(h_y_given_x, h_xy - h_x, TOLERANCE);

    // I(X;Y) = H(Y) - H(Y|X)
    float p_y[2] = {joint[0] + joint[2], joint[1] + joint[3]};
    float h_y = nimcp_stats_entropy(p_y, 2);
    EXPECT_NEAR(mi, h_y - h_y_given_x, TOLERANCE);
}

//=============================================================================
// End-to-End Workflow Tests
//=============================================================================

TEST_F(StatsShannonIntegrationTest, NeuralNetworkWorkflow) {
    // Simulate a neural network classification scenario

    // True labels (one-hot encoded, 3 classes)
    float true_dist[] = {0.0f, 1.0f, 0.0f};  // Class 1

    // Model predictions
    float pred_dist[] = {0.1f, 0.7f, 0.2f};

    // Compute cross-entropy loss
    float ce_loss = nimcp_stats_cross_entropy(true_dist, pred_dist, 3);

    // Cross-entropy should be -log2(0.7) ≈ 0.515 bits
    EXPECT_NEAR(ce_loss, -std::log2(0.7f), TOLERANCE);

    // KL divergence for the same
    float kl = nimcp_stats_kl_divergence(true_dist, pred_dist, 3);
    float h_true = nimcp_stats_entropy(true_dist, 3);

    // CE = H + KL, but H=0 for one-hot
    EXPECT_NEAR(ce_loss, h_true + kl, TOLERANCE);
}

TEST_F(StatsShannonIntegrationTest, InformationFlowAnalysis) {
    // Analyze information flow in a simple network

    // Channel capacity with given SNR
    float snr_db = 20.0f;  // 20 dB
    float snr_linear = nimcp_stats_snr_from_db(snr_db);
    float capacity = nimcp_stats_channel_capacity(1000.0f, snr_linear);

    // Verify SNR conversion
    EXPECT_NEAR(snr_linear, 100.0f, TOLERANCE);

    // Capacity should be significant
    EXPECT_GT(capacity, 1000.0f);  // At least 1 kbit/s

    // Joint distribution for input-output analysis
    float joint[4] = {0.35f, 0.15f, 0.15f, 0.35f};

    float mi = nimcp_stats_mutual_information(joint, 2, 2);
    float uniform_input[2] = {0.5f, 0.5f};
    float h_input = nimcp_stats_entropy(uniform_input, 2);

    // Efficiency = MI / H(input)
    float efficiency = mi / h_input;
    EXPECT_GT(efficiency, 0.0f);
    EXPECT_LE(efficiency, 1.0f);
}

TEST_F(StatsShannonIntegrationTest, BayesianUpdateWithInformationGain) {
    // Prior distribution
    float prior[] = {0.5f, 0.5f};

    // Likelihood (evidence strongly favors hypothesis 0)
    float likelihood[] = {0.9f, 0.1f};

    // Compute KL divergence from prior to posterior
    // (simplified - actual Bayesian update would normalize)

    float kl_gain = nimcp_stats_kl_divergence(likelihood, prior, 2);

    // Information gain should be positive
    EXPECT_GT(kl_gain, 0.0f);

    // JS divergence gives symmetric measure
    float js = nimcp_stats_js_divergence(likelihood, prior, 2);
    EXPECT_GT(js, 0.0f);
    EXPECT_LE(js, 1.0f);
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(StatsShannonIntegrationTest, LargeDistribution) {
    // Test with larger distribution
    const uint32_t n = 1000;
    std::vector<float> probs(n);

    // Create normalized distribution
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        probs[i] = (float)(i + 1);
        sum += probs[i];
    }
    for (uint32_t i = 0; i < n; i++) {
        probs[i] /= sum;
    }

    float stats_entropy = nimcp_stats_entropy(probs.data(), n);
    float shannon_entropy_val = shannon_entropy_array(probs.data(), n);

    EXPECT_NEAR(stats_entropy, shannon_entropy_val, TOLERANCE);
    EXPECT_GT(stats_entropy, 0.0f);
    EXPECT_LT(stats_entropy, std::log2((float)n));  // Less than max entropy
}

TEST_F(StatsShannonIntegrationTest, NearZeroProbabilities) {
    // Test numerical stability with very small probabilities
    float probs[] = {0.999f, 0.0005f, 0.0005f};

    float stats_entropy = nimcp_stats_entropy(probs, 3);
    float shannon_entropy_val = shannon_entropy_array(probs, 3);

    EXPECT_NEAR(stats_entropy, shannon_entropy_val, TOLERANCE);
    EXPECT_GT(stats_entropy, 0.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
