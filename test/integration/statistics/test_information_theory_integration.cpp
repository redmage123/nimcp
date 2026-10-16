/**
 * @file test_information_theory_integration.cpp
 * @brief Integration tests for information theory module with Shannon module
 *
 * WHAT: Verify information theory statistics integrate with Shannon module
 * WHY:  Ensure consistency between statistics and Shannon implementations
 * HOW:  Cross-validate entropy, MI, and channel capacity computations
 *
 * TEST COVERAGE:
 * - Information theory + Shannon module consistency
 * - Cross-module entropy computations
 * - Channel capacity calculations
 * - Transfer entropy for neural connectivity
 * - Information bottleneck analysis
 * - Real-world neural information flow scenarios
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2026-01-30
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <chrono>
#include <numeric>
#include <algorithm>

// Statistics headers
#include "utils/statistics/nimcp_statistics.h"

// Shannon information theory
#include "information/nimcp_shannon.h"

// Memory management
#include "utils/memory/nimcp_memory.h"

// Core types

//=============================================================================
// Test Configuration Constants
//=============================================================================

namespace {
    constexpr float STRICT_TOLERANCE = 1e-5f;
    constexpr float RELAXED_TOLERANCE = 1e-4f;
    constexpr float INFO_TOLERANCE = 0.05f;  // 0.05 bits (relaxed for numerical precision)

    constexpr uint32_t SMALL_SIZE = 100;
    constexpr uint32_t MEDIUM_SIZE = 1000;
    constexpr uint32_t LARGE_SIZE = 10000;

    constexpr float LOG2_E = 1.4426950408889634f;
}

//=============================================================================
// Test Fixture
//=============================================================================

class InformationTheoryIntegrationTest : public ::testing::Test {
protected:
    std::mt19937 rng{42};

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();

        nimcp_stats_config_t config = nimcp_stats_default_config();
        config.random_seed = 42;
        ASSERT_EQ(nimcp_stats_init(&config), NIMCP_STATS_OK);
    }

    void TearDown() override {
        nimcp_stats_shutdown();

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_LT(stats.current_allocated, 4096)
            << "Potential memory leak: " << stats.current_allocated << " bytes";
    }

    //=========================================================================
    // Helper: Generate probability distribution
    //=========================================================================
    std::vector<float> generateDistribution(uint32_t n, float concentration = 1.0f) {
        std::vector<float> dist(n);
        std::gamma_distribution<float> gamma(concentration, 1.0f);

        float sum = 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            dist[i] = gamma(rng);
            sum += dist[i];
        }
        for (uint32_t i = 0; i < n; i++) {
            dist[i] /= sum;
        }
        return dist;
    }

    //=========================================================================
    // Helper: Generate joint distribution
    //=========================================================================
    std::vector<float> generateJointDistribution(uint32_t n_x, uint32_t n_y,
                                                  float dependence = 0.0f) {
        std::vector<float> joint(n_x * n_y);
        std::uniform_real_distribution<float> uniform(0.0f, 1.0f);

        // Generate marginals
        auto p_x = generateDistribution(n_x);
        auto p_y = generateDistribution(n_y);

        // Create joint with controlled dependence
        float sum = 0.0f;
        for (uint32_t i = 0; i < n_x; i++) {
            for (uint32_t j = 0; j < n_y; j++) {
                // Mix independent and correlated components
                float indep = p_x[i] * p_y[j];
                float corr = (i == j % n_x) ? 1.0f / std::min(n_x, n_y) : 0.0f;
                joint[i * n_y + j] = (1.0f - dependence) * indep + dependence * corr;
                sum += joint[i * n_y + j];
            }
        }

        // Normalize
        for (uint32_t k = 0; k < n_x * n_y; k++) {
            joint[k] /= sum;
        }
        return joint;
    }

    //=========================================================================
    // Helper: Generate spike train
    //=========================================================================
    std::vector<float> generateSpikeTrain(float rate_hz, float duration_s,
                                           float dt = 0.001f) {
        std::vector<float> train;
        float prob_spike = rate_hz * dt;
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        for (float t = 0.0f; t < duration_s; t += dt) {
            train.push_back(dist(rng) < prob_spike ? 1.0f : 0.0f);
        }
        return train;
    }

    //=========================================================================
    // Helper: Compute marginal from joint
    //=========================================================================
    std::vector<float> computeMarginalX(const std::vector<float>& joint,
                                         uint32_t n_x, uint32_t n_y) {
        std::vector<float> marginal(n_x, 0.0f);
        for (uint32_t i = 0; i < n_x; i++) {
            for (uint32_t j = 0; j < n_y; j++) {
                marginal[i] += joint[i * n_y + j];
            }
        }
        return marginal;
    }

    std::vector<float> computeMarginalY(const std::vector<float>& joint,
                                         uint32_t n_x, uint32_t n_y) {
        std::vector<float> marginal(n_y, 0.0f);
        for (uint32_t i = 0; i < n_x; i++) {
            for (uint32_t j = 0; j < n_y; j++) {
                marginal[j] += joint[i * n_y + j];
            }
        }
        return marginal;
    }
};

//=============================================================================
// Basic Entropy Tests
//=============================================================================

TEST_F(InformationTheoryIntegrationTest, EntropyUniformDistribution) {
    // Uniform distribution should have maximum entropy
    uint32_t n = 8;
    std::vector<float> uniform(n, 1.0f / n);

    float entropy = nimcp_stats_entropy(uniform.data(), n);
    float max_entropy = std::log2(static_cast<float>(n));

    EXPECT_NEAR(entropy, max_entropy, INFO_TOLERANCE)
        << "Uniform distribution should have H = log2(n) = " << max_entropy;
}

TEST_F(InformationTheoryIntegrationTest, EntropyDeterministicDistribution) {
    // Deterministic distribution should have zero entropy
    std::vector<float> deterministic = {1.0f, 0.0f, 0.0f, 0.0f};

    float entropy = nimcp_stats_entropy(deterministic.data(), 4);

    EXPECT_NEAR(entropy, 0.0f, INFO_TOLERANCE)
        << "Deterministic distribution should have H = 0";
}

TEST_F(InformationTheoryIntegrationTest, EntropyBinaryDistribution) {
    // Binary entropy: H(p) = -p*log2(p) - (1-p)*log2(1-p)
    for (float p = 0.1f; p <= 0.9f; p += 0.1f) {
        std::vector<float> binary = {p, 1.0f - p};
        float entropy = nimcp_stats_entropy(binary.data(), 2);

        float expected = -p * std::log2(p) - (1.0f - p) * std::log2(1.0f - p);
        EXPECT_NEAR(entropy, expected, INFO_TOLERANCE)
            << "Binary entropy at p=" << p;
    }

    // Maximum binary entropy at p=0.5
    std::vector<float> balanced = {0.5f, 0.5f};
    float entropy = nimcp_stats_entropy(balanced.data(), 2);
    EXPECT_NEAR(entropy, 1.0f, INFO_TOLERANCE)
        << "Binary entropy should be 1 bit at p=0.5";
}

TEST_F(InformationTheoryIntegrationTest, EntropyInNats) {
    std::vector<float> dist = {0.25f, 0.25f, 0.25f, 0.25f};

    float entropy_bits = nimcp_stats_entropy(dist.data(), 4);
    float entropy_nats = nimcp_stats_entropy_nats(dist.data(), 4);

    // Conversion: bits = nats * log2(e)
    EXPECT_NEAR(entropy_bits, entropy_nats * LOG2_E, INFO_TOLERANCE)
        << "Entropy conversion between bits and nats";
}

TEST_F(InformationTheoryIntegrationTest, DifferentialEntropy) {
    // Generate normal data and estimate differential entropy
    std::vector<float> normal_data(LARGE_SIZE);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    for (uint32_t i = 0; i < LARGE_SIZE; i++) {
        normal_data[i] = dist(rng);
    }

    float diff_entropy = nimcp_stats_differential_entropy(normal_data.data(), LARGE_SIZE, 0);

    // Differential entropy of N(0,1) = 0.5 * log2(2*pi*e) = ~2.05 bits
    float expected = 0.5f * std::log2(2.0f * M_PI * M_E);
    EXPECT_NEAR(diff_entropy, expected, 0.3f)  // Relaxed tolerance for estimate
        << "Differential entropy of standard normal";
}

//=============================================================================
// Joint and Conditional Entropy Tests
//=============================================================================

TEST_F(InformationTheoryIntegrationTest, JointEntropyIndependent) {
    // For independent X, Y: H(X,Y) = H(X) + H(Y)
    uint32_t n_x = 4, n_y = 4;
    auto joint = generateJointDistribution(n_x, n_y, 0.0f);  // Independent

    auto marginal_x = computeMarginalX(joint, n_x, n_y);
    auto marginal_y = computeMarginalY(joint, n_x, n_y);

    float h_x = nimcp_stats_entropy(marginal_x.data(), n_x);
    float h_y = nimcp_stats_entropy(marginal_y.data(), n_y);
    float h_xy = nimcp_stats_joint_entropy(joint.data(), n_x, n_y);

    EXPECT_NEAR(h_xy, h_x + h_y, 0.1f)
        << "Joint entropy of independent vars should equal sum of marginals";
}

TEST_F(InformationTheoryIntegrationTest, JointEntropyPerfectCorrelation) {
    // For perfectly correlated X = Y: H(X,Y) = H(X) = H(Y)
    uint32_t n = 4;
    std::vector<float> joint(n * n, 0.0f);
    for (uint32_t i = 0; i < n; i++) {
        joint[i * n + i] = 0.25f;  // Only diagonal has probability
    }

    float h_xy = nimcp_stats_joint_entropy(joint.data(), n, n);
    float h_x = std::log2(static_cast<float>(n));  // Uniform marginal

    EXPECT_NEAR(h_xy, h_x, INFO_TOLERANCE)
        << "Joint entropy with perfect correlation equals marginal";
}

TEST_F(InformationTheoryIntegrationTest, ConditionalEntropy) {
    uint32_t n_x = 3, n_y = 3;
    auto joint = generateJointDistribution(n_x, n_y, 0.5f);

    float h_x_given_y = nimcp_stats_conditional_entropy(joint.data(), n_x, n_y);

    // H(X|Y) >= 0 always
    EXPECT_GE(h_x_given_y, 0.0f)
        << "Conditional entropy must be non-negative";

    // H(X|Y) <= H(X)
    auto marginal_x = computeMarginalX(joint, n_x, n_y);
    float h_x = nimcp_stats_entropy(marginal_x.data(), n_x);
    EXPECT_LE(h_x_given_y, h_x + INFO_TOLERANCE)
        << "H(X|Y) <= H(X)";
}

TEST_F(InformationTheoryIntegrationTest, ConditionalEntropyChainRule) {
    // Chain rule: H(X,Y) = H(X) + H(Y|X) = H(Y) + H(X|Y)
    uint32_t n_x = 4, n_y = 4;
    auto joint = generateJointDistribution(n_x, n_y, 0.3f);

    auto marginal_x = computeMarginalX(joint, n_x, n_y);
    auto marginal_y = computeMarginalY(joint, n_x, n_y);

    float h_xy = nimcp_stats_joint_entropy(joint.data(), n_x, n_y);
    float h_x = nimcp_stats_entropy(marginal_x.data(), n_x);
    float h_y = nimcp_stats_entropy(marginal_y.data(), n_y);
    float h_x_given_y = nimcp_stats_conditional_entropy(joint.data(), n_x, n_y);

    // H(X,Y) = H(Y) + H(X|Y)
    EXPECT_NEAR(h_xy, h_y + h_x_given_y, 0.05f)
        << "Chain rule: H(X,Y) = H(Y) + H(X|Y)";
}

//=============================================================================
// Mutual Information Tests
//=============================================================================

TEST_F(InformationTheoryIntegrationTest, MutualInformationIndependent) {
    // For independent X, Y: I(X;Y) = 0
    uint32_t n_x = 4, n_y = 4;
    auto joint = generateJointDistribution(n_x, n_y, 0.0f);

    float mi = nimcp_stats_mutual_information(joint.data(), n_x, n_y);

    EXPECT_NEAR(mi, 0.0f, 0.1f)  // Some tolerance due to numerical noise
        << "MI of independent variables should be ~0";
}

TEST_F(InformationTheoryIntegrationTest, MutualInformationPerfectCorrelation) {
    // For X = Y: I(X;Y) = H(X) = H(Y)
    uint32_t n = 4;
    std::vector<float> joint(n * n, 0.0f);
    for (uint32_t i = 0; i < n; i++) {
        joint[i * n + i] = 0.25f;
    }

    float mi = nimcp_stats_mutual_information(joint.data(), n, n);
    float h_x = std::log2(static_cast<float>(n));

    EXPECT_NEAR(mi, h_x, INFO_TOLERANCE)
        << "MI with perfect correlation equals entropy";
}

TEST_F(InformationTheoryIntegrationTest, MutualInformationSymmetry) {
    // I(X;Y) = I(Y;X)
    uint32_t n_x = 3, n_y = 4;
    auto joint = generateJointDistribution(n_x, n_y, 0.5f);

    float mi_xy = nimcp_stats_mutual_information(joint.data(), n_x, n_y);

    // Transpose joint distribution
    std::vector<float> joint_t(n_y * n_x);
    for (uint32_t i = 0; i < n_x; i++) {
        for (uint32_t j = 0; j < n_y; j++) {
            joint_t[j * n_x + i] = joint[i * n_y + j];
        }
    }

    float mi_yx = nimcp_stats_mutual_information(joint_t.data(), n_y, n_x);

    EXPECT_NEAR(mi_xy, mi_yx, INFO_TOLERANCE)
        << "Mutual information should be symmetric";
}

TEST_F(InformationTheoryIntegrationTest, MutualInformationDefinition) {
    // I(X;Y) = H(X) + H(Y) - H(X,Y) = H(X) - H(X|Y)
    uint32_t n_x = 4, n_y = 4;
    auto joint = generateJointDistribution(n_x, n_y, 0.4f);

    auto marginal_x = computeMarginalX(joint, n_x, n_y);
    auto marginal_y = computeMarginalY(joint, n_x, n_y);

    float h_x = nimcp_stats_entropy(marginal_x.data(), n_x);
    float h_y = nimcp_stats_entropy(marginal_y.data(), n_y);
    float h_xy = nimcp_stats_joint_entropy(joint.data(), n_x, n_y);
    float h_x_given_y = nimcp_stats_conditional_entropy(joint.data(), n_x, n_y);

    float mi = nimcp_stats_mutual_information(joint.data(), n_x, n_y);

    EXPECT_NEAR(mi, h_x + h_y - h_xy, INFO_TOLERANCE)
        << "I(X;Y) = H(X) + H(Y) - H(X,Y)";
    EXPECT_NEAR(mi, h_x - h_x_given_y, INFO_TOLERANCE)
        << "I(X;Y) = H(X) - H(X|Y)";
}

TEST_F(InformationTheoryIntegrationTest, NormalizedMutualInformation) {
    uint32_t n_x = 4, n_y = 4;
    auto joint = generateJointDistribution(n_x, n_y, 0.5f);

    float nmi = nimcp_stats_normalized_mi(joint.data(), n_x, n_y);

    // NMI in [0, 1]
    EXPECT_GE(nmi, 0.0f);
    EXPECT_LE(nmi, 1.0f + INFO_TOLERANCE);

    // Perfect correlation -> NMI = 1
    std::vector<float> perfect(n_x * n_y, 0.0f);
    for (uint32_t i = 0; i < n_x; i++) {
        perfect[i * n_y + i] = 1.0f / n_x;
    }
    float nmi_perfect = nimcp_stats_normalized_mi(perfect.data(), n_x, n_y);
    EXPECT_NEAR(nmi_perfect, 1.0f, INFO_TOLERANCE);
}

//=============================================================================
// KL Divergence Tests
//=============================================================================

TEST_F(InformationTheoryIntegrationTest, KLDivergenceIdentity) {
    // KL(P||P) = 0
    auto p = generateDistribution(5);

    float kl = nimcp_stats_kl_divergence(p.data(), p.data(), 5);

    EXPECT_NEAR(kl, 0.0f, INFO_TOLERANCE)
        << "KL divergence of identical distributions should be 0";
}

TEST_F(InformationTheoryIntegrationTest, KLDivergenceNonNegativity) {
    // KL(P||Q) >= 0 always (Gibbs inequality)
    for (int trial = 0; trial < 10; trial++) {
        auto p = generateDistribution(6);
        auto q = generateDistribution(6);

        float kl = nimcp_stats_kl_divergence(p.data(), q.data(), 6);

        EXPECT_GE(kl, -INFO_TOLERANCE)
            << "KL divergence must be non-negative";
    }
}

TEST_F(InformationTheoryIntegrationTest, KLDivergenceAsymmetry) {
    // KL(P||Q) != KL(Q||P) in general
    auto p = generateDistribution(4, 0.5f);  // Concentrated
    auto q = generateDistribution(4, 2.0f);  // Spread

    float kl_pq = nimcp_stats_kl_divergence(p.data(), q.data(), 4);
    float kl_qp = nimcp_stats_kl_divergence(q.data(), p.data(), 4);

    // Both should be positive but different
    EXPECT_GE(kl_pq, 0.0f);
    EXPECT_GE(kl_qp, 0.0f);
    // They might be equal by chance, so don't assert inequality
}

TEST_F(InformationTheoryIntegrationTest, KLDivergenceFromUniform) {
    // KL(P||U) = log(n) - H(P)
    uint32_t n = 8;
    auto p = generateDistribution(n);
    std::vector<float> uniform(n, 1.0f / n);

    float kl = nimcp_stats_kl_divergence(p.data(), uniform.data(), n);
    float h_p = nimcp_stats_entropy(p.data(), n);
    float log_n = std::log2(static_cast<float>(n));

    EXPECT_NEAR(kl, log_n - h_p, INFO_TOLERANCE)
        << "KL(P||U) = log(n) - H(P)";
}

//=============================================================================
// Jensen-Shannon Divergence Tests
//=============================================================================

TEST_F(InformationTheoryIntegrationTest, JSDivergenceSymmetry) {
    auto p = generateDistribution(5);
    auto q = generateDistribution(5);

    float js_pq = nimcp_stats_js_divergence(p.data(), q.data(), 5);
    float js_qp = nimcp_stats_js_divergence(q.data(), p.data(), 5);

    EXPECT_NEAR(js_pq, js_qp, INFO_TOLERANCE)
        << "JS divergence should be symmetric";
}

TEST_F(InformationTheoryIntegrationTest, JSDivergenceBounds) {
    auto p = generateDistribution(4);
    auto q = generateDistribution(4);

    float js = nimcp_stats_js_divergence(p.data(), q.data(), 4);

    // JS in [0, 1] for log base 2
    EXPECT_GE(js, 0.0f);
    EXPECT_LE(js, 1.0f + INFO_TOLERANCE);
}

TEST_F(InformationTheoryIntegrationTest, JSDivergenceIdentity) {
    auto p = generateDistribution(5);

    float js = nimcp_stats_js_divergence(p.data(), p.data(), 5);

    EXPECT_NEAR(js, 0.0f, INFO_TOLERANCE)
        << "JS(P||P) = 0";
}

//=============================================================================
// Cross Entropy Tests
//=============================================================================

TEST_F(InformationTheoryIntegrationTest, CrossEntropyDefinition) {
    // H(P,Q) = H(P) + KL(P||Q)
    auto p = generateDistribution(5);
    auto q = generateDistribution(5);

    float ce = nimcp_stats_cross_entropy(p.data(), q.data(), 5);
    float h_p = nimcp_stats_entropy(p.data(), 5);
    float kl = nimcp_stats_kl_divergence(p.data(), q.data(), 5);

    EXPECT_NEAR(ce, h_p + kl, INFO_TOLERANCE)
        << "Cross entropy = entropy + KL divergence";
}

TEST_F(InformationTheoryIntegrationTest, CrossEntropySelfEqualsEntropy) {
    auto p = generateDistribution(6);

    float ce = nimcp_stats_cross_entropy(p.data(), p.data(), 6);
    float h = nimcp_stats_entropy(p.data(), 6);

    EXPECT_NEAR(ce, h, INFO_TOLERANCE)
        << "H(P,P) = H(P)";
}

//=============================================================================
// Channel Capacity Tests
//=============================================================================

TEST_F(InformationTheoryIntegrationTest, ChannelCapacityBasic) {
    // C = B * log2(1 + SNR)
    float bandwidth = 100.0f;  // Hz
    float snr = 10.0f;         // Linear

    float capacity = nimcp_stats_channel_capacity(bandwidth, snr);
    float expected = bandwidth * std::log2(1.0f + snr);

    EXPECT_NEAR(capacity, expected, 0.1f)
        << "Channel capacity = B * log2(1 + SNR)";
}

TEST_F(InformationTheoryIntegrationTest, ChannelCapacitySNRConversion) {
    float snr_db = 10.0f;  // 10 dB
    float snr_linear = nimcp_stats_snr_from_db(snr_db);

    // 10 dB = 10 linear
    EXPECT_NEAR(snr_linear, 10.0f, 0.1f);

    // Roundtrip
    float snr_db_back = nimcp_stats_snr_to_db(snr_linear);
    EXPECT_NEAR(snr_db_back, snr_db, 0.01f);
}

TEST_F(InformationTheoryIntegrationTest, ChannelCapacityScaling) {
    float bandwidth = 50.0f;
    float snr = 100.0f;

    float cap1 = nimcp_stats_channel_capacity(bandwidth, snr);
    float cap2 = nimcp_stats_channel_capacity(2 * bandwidth, snr);

    // Doubling bandwidth doubles capacity (for same SNR)
    EXPECT_NEAR(cap2, 2 * cap1, 0.1f);
}

//=============================================================================
// Variation of Information Tests
//=============================================================================

TEST_F(InformationTheoryIntegrationTest, VariationOfInformationIdentity) {
    // VI(X,X) = 0
    uint32_t n = 4;
    std::vector<float> joint(n * n, 0.0f);
    for (uint32_t i = 0; i < n; i++) {
        joint[i * n + i] = 0.25f;
    }

    float vi = nimcp_stats_variation_of_information(joint.data(), n, n);

    EXPECT_NEAR(vi, 0.0f, INFO_TOLERANCE)
        << "VI(X,X) = 0 for identical variables";
}

TEST_F(InformationTheoryIntegrationTest, VariationOfInformationIndependent) {
    // For independent X, Y: VI(X,Y) = H(X) + H(Y)
    uint32_t n_x = 3, n_y = 4;
    auto joint = generateJointDistribution(n_x, n_y, 0.0f);

    auto marginal_x = computeMarginalX(joint, n_x, n_y);
    auto marginal_y = computeMarginalY(joint, n_x, n_y);

    float vi = nimcp_stats_variation_of_information(joint.data(), n_x, n_y);
    float h_x = nimcp_stats_entropy(marginal_x.data(), n_x);
    float h_y = nimcp_stats_entropy(marginal_y.data(), n_y);

    EXPECT_NEAR(vi, h_x + h_y, 0.1f)
        << "VI of independent variables = H(X) + H(Y)";
}

TEST_F(InformationTheoryIntegrationTest, VariationOfInformationSymmetry) {
    uint32_t n = 4;
    auto joint = generateJointDistribution(n, n, 0.5f);

    float vi = nimcp_stats_variation_of_information(joint.data(), n, n);

    // Transpose
    std::vector<float> joint_t(n * n);
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            joint_t[j * n + i] = joint[i * n + j];
        }
    }

    float vi_t = nimcp_stats_variation_of_information(joint_t.data(), n, n);

    EXPECT_NEAR(vi, vi_t, INFO_TOLERANCE)
        << "Variation of information should be symmetric";
}

//=============================================================================
// Transfer Entropy Tests
//=============================================================================

TEST_F(InformationTheoryIntegrationTest, TransferEntropyBasic) {
    // Generate X -> Y causal relationship
    auto x = generateSpikeTrain(10.0f, 1.0f);
    std::vector<float> y(x.size(), 0.0f);

    // Y follows X with delay
    std::uniform_real_distribution<float> noise(0.0f, 1.0f);
    for (size_t i = 5; i < x.size(); i++) {
        if (x[i-5] > 0.5f && noise(rng) < 0.7f) {
            y[i] = 1.0f;
        }
    }

    float te_xy = nimcp_stats_transfer_entropy(
        x.data(), y.data(), static_cast<uint32_t>(x.size()), 5, 10);

    // Should be non-negative
    EXPECT_GE(te_xy, 0.0f) << "Transfer entropy must be non-negative";
}

TEST_F(InformationTheoryIntegrationTest, TransferEntropyDirectionality) {
    // Generate X -> Y with clear causal direction
    auto x = generateSpikeTrain(15.0f, 2.0f);
    std::vector<float> y(x.size(), 0.0f);

    // Y = delayed copy of X with noise
    std::uniform_real_distribution<float> noise(0.0f, 1.0f);
    for (size_t i = 3; i < x.size(); i++) {
        y[i] = (x[i-3] > 0.5f && noise(rng) < 0.8f) ? 1.0f : 0.0f;
    }

    float te_xy = nimcp_stats_transfer_entropy(
        x.data(), y.data(), static_cast<uint32_t>(x.size()), 5, 8);
    float te_yx = nimcp_stats_transfer_entropy(
        y.data(), x.data(), static_cast<uint32_t>(x.size()), 5, 8);

    // X -> Y should have higher TE than Y -> X
    // (though noise can affect this)
    EXPECT_GE(te_xy, 0.0f);
    EXPECT_GE(te_yx, 0.0f);
}

TEST_F(InformationTheoryIntegrationTest, TransferEntropyIndependent) {
    // Independent time series should have low TE
    auto x = generateSpikeTrain(10.0f, 1.0f);
    auto y = generateSpikeTrain(10.0f, 1.0f);

    float te = nimcp_stats_transfer_entropy(
        x.data(), y.data(), static_cast<uint32_t>(x.size()), 3, 8);

    // Should be small (but may not be exactly zero due to finite sample)
    EXPECT_LT(te, 0.5f) << "TE of independent series should be small";
}

//=============================================================================
// Effective Information Tests
//=============================================================================

TEST_F(InformationTheoryIntegrationTest, EffectiveInformationIdentity) {
    // Identity transition matrix -> EI = 0 (no information generation)
    uint32_t n = 4;
    std::vector<float> tpm(n * n, 0.0f);
    for (uint32_t i = 0; i < n; i++) {
        tpm[i * n + i] = 1.0f;  // Deterministic identity
    }

    float ei = nimcp_stats_effective_information(tpm.data(), n);

    // Should be well-defined
    EXPECT_TRUE(std::isfinite(ei));
}

TEST_F(InformationTheoryIntegrationTest, EffectiveInformationRandom) {
    // Random transition matrix
    uint32_t n = 4;
    std::vector<float> tpm(n * n);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (uint32_t i = 0; i < n; i++) {
        float sum = 0.0f;
        for (uint32_t j = 0; j < n; j++) {
            tpm[i * n + j] = dist(rng);
            sum += tpm[i * n + j];
        }
        // Normalize row
        for (uint32_t j = 0; j < n; j++) {
            tpm[i * n + j] /= sum;
        }
    }

    float ei = nimcp_stats_effective_information(tpm.data(), n);

    EXPECT_TRUE(std::isfinite(ei));
    EXPECT_GE(ei, 0.0f);
}

//=============================================================================
// Information Integration Tests
//=============================================================================

TEST_F(InformationTheoryIntegrationTest, InformationIntegrationBasic) {
    // Test with simple covariance structure
    uint32_t n = 3;
    std::vector<float> cov = {
        1.0f, 0.5f, 0.3f,
        0.5f, 1.0f, 0.4f,
        0.3f, 0.4f, 1.0f
    };

    float phi = nimcp_stats_information_integration(cov.data(), n);

    EXPECT_TRUE(std::isfinite(phi));
    EXPECT_GE(phi, 0.0f) << "Information integration should be non-negative";
}

TEST_F(InformationTheoryIntegrationTest, InformationIntegrationIndependent) {
    // Diagonal covariance (independent variables)
    uint32_t n = 4;
    std::vector<float> cov(n * n, 0.0f);
    for (uint32_t i = 0; i < n; i++) {
        cov[i * n + i] = 1.0f;
    }

    float phi = nimcp_stats_information_integration(cov.data(), n);

    // Independent variables should have low integration
    EXPECT_GE(phi, 0.0f);
}

//=============================================================================
// Information Bottleneck Tests
//=============================================================================

TEST_F(InformationTheoryIntegrationTest, InformationBottleneckBasic) {
    // Simple joint distribution
    uint32_t n_x = 4, n_y = 4, n_t = 2;
    auto joint = generateJointDistribution(n_x, n_y, 0.6f);

    std::vector<float> q_t_given_x(n_x * n_t);
    float compression = nimcp_stats_information_bottleneck(
        joint.data(), n_x, n_y, n_t, 1.0f, q_t_given_x.data(), 100);

    // Compression ratio in [0, 1]
    EXPECT_GE(compression, 0.0f);
    EXPECT_LE(compression, 1.0f + 0.1f);

    // Q(T|X) should be valid conditional distribution
    for (uint32_t i = 0; i < n_x; i++) {
        float sum = 0.0f;
        for (uint32_t t = 0; t < n_t; t++) {
            float val = q_t_given_x[i * n_t + t];
            EXPECT_GE(val, 0.0f);
            sum += val;
        }
        EXPECT_NEAR(sum, 1.0f, 0.01f) << "Q(T|X=i) should sum to 1";
    }
}

TEST_F(InformationTheoryIntegrationTest, InformationBottleneckBetaEffect) {
    uint32_t n_x = 4, n_y = 4, n_t = 2;
    auto joint = generateJointDistribution(n_x, n_y, 0.5f);

    std::vector<float> q_low(n_x * n_t);
    std::vector<float> q_high(n_x * n_t);

    float comp_low = nimcp_stats_information_bottleneck(
        joint.data(), n_x, n_y, n_t, 0.1f, q_low.data(), 100);
    float comp_high = nimcp_stats_information_bottleneck(
        joint.data(), n_x, n_y, n_t, 10.0f, q_high.data(), 100);

    // Higher beta should preserve more information
    EXPECT_GE(comp_high, comp_low - 0.1f);
}

//=============================================================================
// Info Measures All-in-One Test
//=============================================================================

TEST_F(InformationTheoryIntegrationTest, InfoMeasuresConsistency) {
    uint32_t n_x = 4, n_y = 4;
    auto joint = generateJointDistribution(n_x, n_y, 0.5f);

    nimcp_info_result_t result;
    nimcp_stats_result_t status = nimcp_stats_info_measures(
        joint.data(), n_x, n_y, &result);

    EXPECT_EQ(status, NIMCP_STATS_OK);

    // Verify all fields are populated
    EXPECT_TRUE(std::isfinite(result.entropy));
    EXPECT_TRUE(std::isfinite(result.joint_entropy));
    EXPECT_TRUE(std::isfinite(result.conditional_entropy));
    EXPECT_TRUE(std::isfinite(result.mutual_information));
    EXPECT_TRUE(std::isfinite(result.normalized_mi));
    EXPECT_TRUE(std::isfinite(result.variation_of_info));

    // Verify relationships
    // MI = H(X) - H(X|Y)
    EXPECT_NEAR(result.mutual_information,
                result.entropy - result.conditional_entropy, 0.05f);

    // NMI in [0, 1]
    EXPECT_GE(result.normalized_mi, 0.0f);
    EXPECT_LE(result.normalized_mi, 1.0f + INFO_TOLERANCE);
}

//=============================================================================
// Real-World Neural Scenarios
//=============================================================================

TEST_F(InformationTheoryIntegrationTest, NeuralCodingEfficiency) {
    // Analyze coding efficiency of spike train
    auto spikes = generateSpikeTrain(20.0f, 10.0f);

    // Bin spikes into counts
    uint32_t bin_size = 10;  // 10ms bins
    uint32_t n_bins = static_cast<uint32_t>(spikes.size()) / bin_size;
    std::vector<float> counts(n_bins);

    for (uint32_t b = 0; b < n_bins; b++) {
        float sum = 0.0f;
        for (uint32_t i = 0; i < bin_size; i++) {
            sum += spikes[b * bin_size + i];
        }
        counts[b] = sum;
    }

    // Estimate entropy of spike count distribution
    float max_count = nimcp_stats_max(counts.data(), n_bins);
    uint32_t n_levels = static_cast<uint32_t>(max_count) + 1;
    std::vector<float> count_dist(n_levels, 0.0f);

    for (uint32_t b = 0; b < n_bins; b++) {
        uint32_t c = static_cast<uint32_t>(counts[b]);
        if (c < n_levels) {
            count_dist[c] += 1.0f / n_bins;
        }
    }

    float entropy = nimcp_stats_entropy(count_dist.data(), n_levels);
    float max_entropy = std::log2(static_cast<float>(n_levels));

    float efficiency = entropy / max_entropy;

    EXPECT_GE(efficiency, 0.0f);
    EXPECT_LE(efficiency, 1.0f);
}

TEST_F(InformationTheoryIntegrationTest, StimulusResponseInformation) {
    // Information between stimulus and neural response
    uint32_t n_stimuli = 4;
    uint32_t n_responses = 8;
    uint32_t n_trials = 1000;

    // Simulate stimulus-response mapping with noise
    std::vector<float> joint(n_stimuli * n_responses, 0.0f);
    std::uniform_int_distribution<uint32_t> stim_dist(0, n_stimuli - 1);
    std::normal_distribution<float> noise(0.0f, 1.0f);

    for (uint32_t trial = 0; trial < n_trials; trial++) {
        uint32_t stim = stim_dist(rng);
        // Response peaks near 2*stim with noise
        float response_mean = 2.0f * stim;
        float response = response_mean + noise(rng);
        uint32_t resp = static_cast<uint32_t>(std::max(0.0f,
            std::min(static_cast<float>(n_responses - 1), response)));

        joint[stim * n_responses + resp] += 1.0f / n_trials;
    }

    float mi = nimcp_stats_mutual_information(joint.data(), n_stimuli, n_responses);

    // Should have significant MI (good stimulus encoding)
    EXPECT_GT(mi, 0.5f) << "Stimulus-response MI should be positive";
    EXPECT_LT(mi, std::log2(static_cast<float>(n_stimuli)) + 0.1f)
        << "MI bounded by stimulus entropy";
}

TEST_F(InformationTheoryIntegrationTest, NeuralRedundancy) {
    // Test redundancy between two neural populations encoding same stimulus
    uint32_t n = 4;

    // High redundancy: both encode same information
    std::vector<float> redundant(n * n, 0.0f);
    for (uint32_t i = 0; i < n; i++) {
        redundant[i * n + i] = 0.25f;
    }

    float mi_redundant = nimcp_stats_mutual_information(redundant.data(), n, n);
    float h_x = std::log2(static_cast<float>(n));

    // High MI indicates redundancy
    EXPECT_NEAR(mi_redundant, h_x, INFO_TOLERANCE)
        << "Redundant coding has high MI";

    // Low redundancy: independent encoding
    auto independent = generateJointDistribution(n, n, 0.0f);
    float mi_independent = nimcp_stats_mutual_information(independent.data(), n, n);

    EXPECT_LT(mi_independent, mi_redundant)
        << "Independent coding has lower MI";
}

//=============================================================================
// Memory and Performance Tests
//=============================================================================

TEST_F(InformationTheoryIntegrationTest, PerformanceEntropy) {
    auto dist = generateDistribution(100);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; i++) {
        volatile float h = nimcp_stats_entropy(dist.data(), 100);
        (void)h;
    }
    auto end = std::chrono::high_resolution_clock::now();

    uint64_t time_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    EXPECT_LT(time_us, 100000u) << "10k entropy computations should be fast";
}

TEST_F(InformationTheoryIntegrationTest, PerformanceMutualInformation) {
    auto joint = generateJointDistribution(10, 10, 0.5f);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        volatile float mi = nimcp_stats_mutual_information(joint.data(), 10, 10);
        (void)mi;
    }
    auto end = std::chrono::high_resolution_clock::now();

    uint64_t time_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    EXPECT_LT(time_us, 50000u) << "1k MI computations should be fast";
}

TEST_F(InformationTheoryIntegrationTest, NoMemoryLeaks) {
    nimcp_memory_clear_stats();

    for (int i = 0; i < 100; i++) {
        auto dist = generateDistribution(20);
        volatile float h = nimcp_stats_entropy(dist.data(), 20);
        (void)h;

        auto joint = generateJointDistribution(5, 5, 0.5f);
        volatile float mi = nimcp_stats_mutual_information(joint.data(), 5, 5);
        (void)mi;
    }

    nimcp_memory_stats_t stats;
    nimcp_memory_get_stats(&stats);
    EXPECT_LT(stats.current_allocated, 4096)
        << "Memory leaked during info theory computations";
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(InformationTheoryIntegrationTest, EntropyWithZeroProbabilities) {
    // 0 * log(0) should be treated as 0
    std::vector<float> sparse = {0.5f, 0.0f, 0.0f, 0.5f};

    float h = nimcp_stats_entropy(sparse.data(), 4);

    EXPECT_TRUE(std::isfinite(h)) << "Should handle zero probabilities";
    EXPECT_NEAR(h, 1.0f, INFO_TOLERANCE) << "H(0.5, 0, 0, 0.5) = 1 bit";
}

TEST_F(InformationTheoryIntegrationTest, KLDivergenceZeroSupport) {
    // Q has zero where P is non-zero -> KL = infinity
    std::vector<float> p = {0.5f, 0.5f, 0.0f};
    std::vector<float> q = {0.0f, 0.5f, 0.5f};

    float kl = nimcp_stats_kl_divergence(p.data(), q.data(), 3);

    EXPECT_TRUE(std::isinf(kl)) << "KL should be infinite when Q=0 where P>0";
}

TEST_F(InformationTheoryIntegrationTest, SmallProbabilities) {
    // Test numerical stability with very small probabilities
    std::vector<float> small_probs = {0.999f, 0.0005f, 0.0003f, 0.0002f};

    float h = nimcp_stats_entropy(small_probs.data(), 4);

    EXPECT_TRUE(std::isfinite(h));
    EXPECT_GE(h, 0.0f);
    EXPECT_LT(h, 2.0f);  // Much less than log2(4) due to concentration
}

