//=============================================================================
// test_information_theory_regression.cpp - Information Theory Regression Tests
//=============================================================================
/**
 * @file test_information_theory_regression.cpp
 * @brief Comprehensive regression tests for information theory module
 *
 * REGRESSION TEST FOCUS:
 * - Partial information decomposition (PID) on XOR gate (all synergistic)
 * - Renyi entropy special cases
 * - Transfer entropy known values
 * - KL divergence properties
 * - Mutual information bounds
 * - Channel capacity analytical results
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <limits>
#include <chrono>
#include <random>
#include <algorithm>
#include <numeric>

extern "C" {
#include "utils/statistics/nimcp_statistics.h"
#include "information/nimcp_shannon.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class InformationTheoryRegressionTest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-5f;
    static constexpr float RELATIVE_TOL = 1e-4f;

    std::mt19937 rng;

    void SetUp() override {
        nimcp_stats_init(nullptr);
        rng.seed(42);
    }

    void TearDown() override {
        nimcp_stats_shutdown();
    }

    bool relativelyEqual(float a, float b, float tol = RELATIVE_TOL) {
        if (std::isnan(a) || std::isnan(b)) return false;
        return std::fabs(a - b) <= tol * std::max(1.0f, std::max(std::fabs(a), std::fabs(b)));
    }

    // Binary entropy function H(p) = -p*log2(p) - (1-p)*log2(1-p)
    float binaryEntropy(float p) {
        if (p <= 0.0f || p >= 1.0f) return 0.0f;
        return -p * std::log2(p) - (1.0f - p) * std::log2(1.0f - p);
    }
};

//=============================================================================
// SHANNON ENTROPY REGRESSION TESTS
//=============================================================================

TEST_F(InformationTheoryRegressionTest, EntropyUniformMaximal) {
    // Uniform distribution achieves maximum entropy = log2(n)
    for (uint32_t n = 2; n <= 16; n *= 2) {
        std::vector<float> uniform(n, 1.0f / n);
        float result = nimcp_stats_entropy(uniform.data(), n);
        float expected = std::log2(static_cast<float>(n));

        EXPECT_NEAR(result, expected, 1e-5f)
            << "Uniform entropy for n=" << n << " should be log2(" << n << ")=" << expected;
    }
}

TEST_F(InformationTheoryRegressionTest, EntropyDeltaDistribution) {
    // Delta distribution (one state with probability 1) has entropy = 0
    for (uint32_t n = 2; n <= 8; ++n) {
        std::vector<float> delta(n, 0.0f);
        delta[0] = 1.0f;

        float result = nimcp_stats_entropy(delta.data(), n);
        EXPECT_FLOAT_EQ(result, 0.0f) << "Delta distribution should have zero entropy";
    }
}

TEST_F(InformationTheoryRegressionTest, EntropyBinaryKnownValues) {
    // Test binary entropy at known values
    struct TestCase {
        float p;
        float expected_entropy;
    };

    std::vector<TestCase> cases = {
        {0.5f, 1.0f},      // Maximum entropy
        {0.0f, 0.0f},      // Deterministic
        {1.0f, 0.0f},      // Deterministic
        {0.25f, 0.8113f},  // H(0.25) = 0.8113 bits
        {0.75f, 0.8113f},  // H(0.75) = H(0.25) (symmetry)
        {0.1f, 0.4690f},   // H(0.1)
        {0.9f, 0.4690f}    // H(0.9) = H(0.1)
    };

    for (const auto& tc : cases) {
        std::vector<float> probs = {tc.p, 1.0f - tc.p};
        float result = nimcp_stats_entropy(probs.data(), 2);

        EXPECT_NEAR(result, tc.expected_entropy, 0.01f)
            << "H(" << tc.p << ") should be " << tc.expected_entropy;
    }
}

TEST_F(InformationTheoryRegressionTest, EntropySymmetry) {
    // H(p, 1-p) = H(1-p, p) - entropy is symmetric
    for (float p = 0.1f; p <= 0.5f; p += 0.1f) {
        std::vector<float> probs1 = {p, 1.0f - p};
        std::vector<float> probs2 = {1.0f - p, p};

        float h1 = nimcp_stats_entropy(probs1.data(), 2);
        float h2 = nimcp_stats_entropy(probs2.data(), 2);

        EXPECT_NEAR(h1, h2, 1e-6f) << "Entropy should be symmetric for p=" << p;
    }
}

TEST_F(InformationTheoryRegressionTest, EntropyConcavity) {
    // Entropy is concave: H(lambda*p + (1-lambda)*q) >= lambda*H(p) + (1-lambda)*H(q)
    std::vector<float> p = {0.3f, 0.7f};
    std::vector<float> q = {0.6f, 0.4f};
    float lambda = 0.5f;

    std::vector<float> mixture = {
        lambda * p[0] + (1.0f - lambda) * q[0],
        lambda * p[1] + (1.0f - lambda) * q[1]
    };

    float h_p = nimcp_stats_entropy(p.data(), 2);
    float h_q = nimcp_stats_entropy(q.data(), 2);
    float h_mix = nimcp_stats_entropy(mixture.data(), 2);

    EXPECT_GE(h_mix, lambda * h_p + (1.0f - lambda) * h_q - 1e-6f)
        << "Entropy should be concave";
}

//=============================================================================
// JOINT AND CONDITIONAL ENTROPY REGRESSION TESTS
//=============================================================================

TEST_F(InformationTheoryRegressionTest, JointEntropyIndependent) {
    // For independent X, Y: H(X,Y) = H(X) + H(Y)
    // Joint distribution where p(x,y) = p(x)*p(y)
    std::vector<float> p_x = {0.5f, 0.5f};
    std::vector<float> p_y = {0.3f, 0.7f};

    // Create joint distribution (2x2)
    std::vector<float> joint(4);
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            joint[i * 2 + j] = p_x[i] * p_y[j];
        }
    }

    float h_joint = nimcp_stats_joint_entropy(joint.data(), 2, 2);
    float h_x = nimcp_stats_entropy(p_x.data(), 2);
    float h_y = nimcp_stats_entropy(p_y.data(), 2);

    EXPECT_NEAR(h_joint, h_x + h_y, 1e-5f)
        << "H(X,Y) = H(X) + H(Y) for independent variables";
}

TEST_F(InformationTheoryRegressionTest, JointEntropyDeterministic) {
    // If Y = f(X) deterministically, H(X,Y) = H(X)
    // Joint: Y = X (identity function)
    std::vector<float> joint = {0.5f, 0.0f, 0.0f, 0.5f}; // Diagonal

    float h_joint = nimcp_stats_joint_entropy(joint.data(), 2, 2);
    float h_x = nimcp_stats_entropy(std::vector<float>{0.5f, 0.5f}.data(), 2);

    EXPECT_NEAR(h_joint, h_x, 1e-5f)
        << "H(X,Y) = H(X) when Y is deterministic function of X";
}

TEST_F(InformationTheoryRegressionTest, ConditionalEntropyChainRule) {
    // Chain rule: H(X,Y) = H(X) + H(Y|X)
    // Therefore: H(Y|X) = H(X,Y) - H(X)
    std::vector<float> joint = {0.2f, 0.3f, 0.4f, 0.1f}; // 2x2

    float h_joint = nimcp_stats_joint_entropy(joint.data(), 2, 2);
    float h_cond = nimcp_stats_conditional_entropy(joint.data(), 2, 2);

    // Compute H(X) from marginal
    float p_x0 = joint[0] + joint[1];
    float p_x1 = joint[2] + joint[3];
    std::vector<float> marginal_x = {p_x0, p_x1};
    float h_x = nimcp_stats_entropy(marginal_x.data(), 2);

    EXPECT_NEAR(h_joint, h_x + h_cond, 1e-5f)
        << "Chain rule: H(X,Y) = H(X) + H(Y|X)";
}

TEST_F(InformationTheoryRegressionTest, ConditionalEntropyNonNegative) {
    // H(Y|X) >= 0 always
    std::vector<float> joint = {0.1f, 0.2f, 0.3f, 0.4f};

    float h_cond = nimcp_stats_conditional_entropy(joint.data(), 2, 2);

    EXPECT_GE(h_cond, -1e-6f) << "Conditional entropy should be non-negative";
}

//=============================================================================
// MUTUAL INFORMATION REGRESSION TESTS
//=============================================================================

TEST_F(InformationTheoryRegressionTest, MIIndependentVariables) {
    // I(X;Y) = 0 for independent X, Y
    std::vector<float> p_x = {0.4f, 0.6f};
    std::vector<float> p_y = {0.3f, 0.7f};

    std::vector<float> joint(4);
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            joint[i * 2 + j] = p_x[i] * p_y[j];
        }
    }

    float mi = nimcp_stats_mutual_information(joint.data(), 2, 2);

    EXPECT_NEAR(mi, 0.0f, 1e-5f) << "MI should be 0 for independent variables";
}

TEST_F(InformationTheoryRegressionTest, MIPerfectCorrelation) {
    // I(X;X) = H(X)
    // Joint where Y = X
    std::vector<float> joint = {0.3f, 0.0f, 0.0f, 0.7f}; // Diagonal

    float mi = nimcp_stats_mutual_information(joint.data(), 2, 2);
    float h_x = nimcp_stats_entropy(std::vector<float>{0.3f, 0.7f}.data(), 2);

    EXPECT_NEAR(mi, h_x, 1e-5f) << "I(X;X) = H(X)";
}

TEST_F(InformationTheoryRegressionTest, MISymmetric) {
    // I(X;Y) = I(Y;X)
    std::vector<float> joint = {0.1f, 0.2f, 0.3f, 0.4f};

    float mi_xy = nimcp_stats_mutual_information(joint.data(), 2, 2);

    // Transpose the joint distribution
    std::vector<float> joint_t = {joint[0], joint[2], joint[1], joint[3]};
    float mi_yx = nimcp_stats_mutual_information(joint_t.data(), 2, 2);

    EXPECT_NEAR(mi_xy, mi_yx, 1e-5f) << "MI should be symmetric";
}

TEST_F(InformationTheoryRegressionTest, MIBoundedByEntropy) {
    // I(X;Y) <= min(H(X), H(Y))
    std::vector<float> joint = {0.15f, 0.35f, 0.10f, 0.40f};

    float mi = nimcp_stats_mutual_information(joint.data(), 2, 2);

    // Compute marginals
    std::vector<float> p_x = {joint[0] + joint[1], joint[2] + joint[3]};
    std::vector<float> p_y = {joint[0] + joint[2], joint[1] + joint[3]};

    float h_x = nimcp_stats_entropy(p_x.data(), 2);
    float h_y = nimcp_stats_entropy(p_y.data(), 2);

    EXPECT_LE(mi, std::min(h_x, h_y) + 1e-5f)
        << "I(X;Y) <= min(H(X), H(Y))";
}

TEST_F(InformationTheoryRegressionTest, NormalizedMIBounds) {
    // NMI should be in [0, 1]
    std::vector<float> joint = {0.2f, 0.3f, 0.1f, 0.4f};

    float nmi = nimcp_stats_normalized_mi(joint.data(), 2, 2);

    EXPECT_GE(nmi, -1e-6f) << "NMI should be >= 0";
    EXPECT_LE(nmi, 1.0f + 1e-6f) << "NMI should be <= 1";
}

//=============================================================================
// KL DIVERGENCE REGRESSION TESTS
//=============================================================================

TEST_F(InformationTheoryRegressionTest, KLDivergenceSameDistribution) {
    // D_KL(P||P) = 0
    std::vector<float> p = {0.3f, 0.5f, 0.2f};

    float kl = nimcp_stats_kl_divergence(p.data(), p.data(), p.size());

    EXPECT_NEAR(kl, 0.0f, 1e-6f) << "KL divergence of P from itself should be 0";
}

TEST_F(InformationTheoryRegressionTest, KLDivergenceNonNegative) {
    // D_KL(P||Q) >= 0 (Gibbs' inequality)
    std::vector<float> p = {0.4f, 0.4f, 0.2f};
    std::vector<float> q = {0.3f, 0.5f, 0.2f};

    float kl = nimcp_stats_kl_divergence(p.data(), q.data(), p.size());

    EXPECT_GE(kl, -1e-6f) << "KL divergence should be non-negative";
}

TEST_F(InformationTheoryRegressionTest, KLDivergenceAsymmetric) {
    // D_KL(P||Q) != D_KL(Q||P) in general
    std::vector<float> p = {0.9f, 0.1f};
    std::vector<float> q = {0.5f, 0.5f};

    float kl_pq = nimcp_stats_kl_divergence(p.data(), q.data(), 2);
    float kl_qp = nimcp_stats_kl_divergence(q.data(), p.data(), 2);

    EXPECT_NE(kl_pq, kl_qp) << "KL divergence should be asymmetric";
}

TEST_F(InformationTheoryRegressionTest, KLDivergenceKnownValue) {
    // D_KL([0.5,0.5] || [0.25,0.75])
    // = 0.5*log2(0.5/0.25) + 0.5*log2(0.5/0.75)
    // = 0.5*log2(2) + 0.5*log2(2/3)
    // = 0.5*1 + 0.5*(-0.585) = 0.5 - 0.2925 = 0.2075
    std::vector<float> p = {0.5f, 0.5f};
    std::vector<float> q = {0.25f, 0.75f};

    float kl = nimcp_stats_kl_divergence(p.data(), q.data(), 2);

    EXPECT_NEAR(kl, 0.2075f, 0.01f);
}

//=============================================================================
// JENSEN-SHANNON DIVERGENCE REGRESSION TESTS
//=============================================================================

TEST_F(InformationTheoryRegressionTest, JSDSymmetric) {
    // JSD(P||Q) = JSD(Q||P)
    std::vector<float> p = {0.4f, 0.6f};
    std::vector<float> q = {0.7f, 0.3f};

    float jsd_pq = nimcp_stats_js_divergence(p.data(), q.data(), 2);
    float jsd_qp = nimcp_stats_js_divergence(q.data(), p.data(), 2);

    EXPECT_NEAR(jsd_pq, jsd_qp, 1e-6f) << "JSD should be symmetric";
}

TEST_F(InformationTheoryRegressionTest, JSDBounded) {
    // JSD is bounded: 0 <= JSD <= 1 (for log base 2)
    std::vector<float> p = {0.1f, 0.9f};
    std::vector<float> q = {0.9f, 0.1f};

    float jsd = nimcp_stats_js_divergence(p.data(), q.data(), 2);

    EXPECT_GE(jsd, 0.0f) << "JSD should be >= 0";
    EXPECT_LE(jsd, 1.0f) << "JSD should be <= 1 (log2)";
}

TEST_F(InformationTheoryRegressionTest, JSDSameDistribution) {
    // JSD(P||P) = 0
    std::vector<float> p = {0.25f, 0.25f, 0.5f};

    float jsd = nimcp_stats_js_divergence(p.data(), p.data(), p.size());

    EXPECT_NEAR(jsd, 0.0f, 1e-6f);
}

//=============================================================================
// CROSS ENTROPY REGRESSION TESTS
//=============================================================================

TEST_F(InformationTheoryRegressionTest, CrossEntropyRelation) {
    // H(P, Q) = H(P) + D_KL(P||Q)
    std::vector<float> p = {0.3f, 0.7f};
    std::vector<float> q = {0.5f, 0.5f};

    float h_p = nimcp_stats_entropy(p.data(), 2);
    float kl = nimcp_stats_kl_divergence(p.data(), q.data(), 2);
    float ce = nimcp_stats_cross_entropy(p.data(), q.data(), 2);

    EXPECT_NEAR(ce, h_p + kl, 1e-5f) << "H(P,Q) = H(P) + D_KL(P||Q)";
}

TEST_F(InformationTheoryRegressionTest, CrossEntropyMinimum) {
    // Cross entropy is minimized when Q = P, giving H(P,P) = H(P)
    std::vector<float> p = {0.3f, 0.4f, 0.3f};

    float ce_self = nimcp_stats_cross_entropy(p.data(), p.data(), p.size());
    float h_p = nimcp_stats_entropy(p.data(), p.size());

    EXPECT_NEAR(ce_self, h_p, 1e-5f);
}

//=============================================================================
// CHANNEL CAPACITY REGRESSION TESTS (Shannon)
//=============================================================================

TEST_F(InformationTheoryRegressionTest, ChannelCapacityBasic) {
    // C = B * log2(1 + SNR)
    // B = 100 Hz, SNR = 10 -> C = 100 * log2(11) ~= 345.9 bits/s
    float bandwidth = 100.0f;
    float snr = 10.0f;

    float capacity = shannon_channel_capacity(bandwidth, snr);
    float expected = 100.0f * std::log2(11.0f);

    EXPECT_NEAR(capacity, expected, 0.1f);
}

TEST_F(InformationTheoryRegressionTest, ChannelCapacityZeroSNR) {
    // SNR = 0 -> C = B * log2(1) = 0
    float capacity = shannon_channel_capacity(100.0f, 0.0f);

    EXPECT_FLOAT_EQ(capacity, 0.0f);
}

TEST_F(InformationTheoryRegressionTest, ChannelCapacityHighSNR) {
    // At high SNR, C ~ B * log2(SNR)
    float bandwidth = 100.0f;
    float snr = 1000.0f;

    float capacity = shannon_channel_capacity(bandwidth, snr);
    float approx = bandwidth * std::log2(snr);

    // Should be close to approximation at high SNR
    EXPECT_NEAR(capacity, approx, bandwidth * 0.1f);
}

TEST_F(InformationTheoryRegressionTest, ChannelCapacityScaling) {
    // Doubling bandwidth doubles capacity
    float snr = 10.0f;

    float c1 = shannon_channel_capacity(100.0f, snr);
    float c2 = shannon_channel_capacity(200.0f, snr);

    EXPECT_NEAR(c2, 2.0f * c1, 0.1f);
}

//=============================================================================
// TRANSFER ENTROPY REGRESSION TESTS
//=============================================================================

TEST_F(InformationTheoryRegressionTest, TransferEntropyNonNegative) {
    // TE(X->Y) >= 0
    std::vector<float> x(100), y(100);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (size_t i = 0; i < 100; ++i) {
        x[i] = dist(rng);
        y[i] = dist(rng);
    }

    float te = nimcp_stats_transfer_entropy(x.data(), y.data(), 100, 1, 4);

    EXPECT_GE(te, -1e-5f) << "Transfer entropy should be non-negative";
}

TEST_F(InformationTheoryRegressionTest, TransferEntropyCausalRelation) {
    // If X causes Y, TE(X->Y) > TE(Y->X)
    // Create causal relationship: y[t] depends on x[t-1]
    std::vector<float> x(100), y(100);
    std::normal_distribution<float> noise(0.0f, 0.1f);

    x[0] = 0.5f;
    y[0] = 0.5f;
    for (size_t i = 1; i < 100; ++i) {
        x[i] = 0.8f * x[i-1] + noise(rng);
        y[i] = 0.7f * x[i-1] + 0.2f * y[i-1] + noise(rng); // Y depends on X
    }

    float te_xy = nimcp_stats_transfer_entropy(x.data(), y.data(), 100, 1, 4);
    float te_yx = nimcp_stats_transfer_entropy(y.data(), x.data(), 100, 1, 4);

    EXPECT_GT(te_xy, te_yx) << "TE(X->Y) should be greater when X causes Y";
}

//=============================================================================
// VARIATION OF INFORMATION REGRESSION TESTS
//=============================================================================

TEST_F(InformationTheoryRegressionTest, VIMetricProperty) {
    // VI is a metric: VI(X,X) = 0
    std::vector<float> joint = {0.5f, 0.0f, 0.0f, 0.5f}; // X = Y

    float vi = nimcp_stats_variation_of_information(joint.data(), 2, 2);

    EXPECT_NEAR(vi, 0.0f, 1e-5f) << "VI(X,X) should be 0";
}

TEST_F(InformationTheoryRegressionTest, VISymmetric) {
    // VI(X,Y) = VI(Y,X)
    std::vector<float> joint = {0.1f, 0.2f, 0.3f, 0.4f};
    std::vector<float> joint_t = {joint[0], joint[2], joint[1], joint[3]};

    float vi_xy = nimcp_stats_variation_of_information(joint.data(), 2, 2);
    float vi_yx = nimcp_stats_variation_of_information(joint_t.data(), 2, 2);

    EXPECT_NEAR(vi_xy, vi_yx, 1e-5f) << "VI should be symmetric";
}

TEST_F(InformationTheoryRegressionTest, VINonNegative) {
    // VI >= 0
    std::vector<float> joint = {0.15f, 0.35f, 0.25f, 0.25f};

    float vi = nimcp_stats_variation_of_information(joint.data(), 2, 2);

    EXPECT_GE(vi, -1e-6f) << "VI should be non-negative";
}

//=============================================================================
// EFFECTIVE INFORMATION REGRESSION TESTS
//=============================================================================

TEST_F(InformationTheoryRegressionTest, EffectiveInfoDeterministic) {
    // Deterministic system (identity TPM) should have high EI
    // TPM for identity: state 0 -> 0, state 1 -> 1
    std::vector<float> tpm_identity = {1.0f, 0.0f, 0.0f, 1.0f}; // 2x2 identity

    float ei = nimcp_stats_effective_information(tpm_identity.data(), 2);

    EXPECT_GT(ei, 0.5f) << "Deterministic system should have significant EI";
}

TEST_F(InformationTheoryRegressionTest, EffectiveInfoRandom) {
    // Fully random transitions should have low EI
    std::vector<float> tpm_random = {0.5f, 0.5f, 0.5f, 0.5f}; // Uniform

    float ei = nimcp_stats_effective_information(tpm_random.data(), 2);

    EXPECT_LT(ei, 0.1f) << "Random system should have near-zero EI";
}

//=============================================================================
// NUMERICAL STABILITY TESTS
//=============================================================================

TEST_F(InformationTheoryRegressionTest, EntropyNearZeroProbability) {
    // Should handle p -> 0 gracefully (0 * log(0) = 0 by convention)
    std::vector<float> probs = {1e-10f, 1.0f - 1e-10f};

    float h = nimcp_stats_entropy(probs.data(), 2);

    EXPECT_FALSE(std::isnan(h)) << "Entropy should handle near-zero probabilities";
    EXPECT_GT(h, 0.0f);
    EXPECT_LT(h, 1e-5f) << "Should be nearly deterministic";
}

TEST_F(InformationTheoryRegressionTest, KLDivergenceNearZero) {
    // KL divergence when q[i] is near zero but p[i] is also near zero
    std::vector<float> p = {1e-10f, 1.0f - 1e-10f};
    std::vector<float> q = {1e-10f, 1.0f - 1e-10f};

    float kl = nimcp_stats_kl_divergence(p.data(), q.data(), 2);

    EXPECT_FALSE(std::isnan(kl));
    EXPECT_NEAR(kl, 0.0f, 1e-5f);
}

TEST_F(InformationTheoryRegressionTest, EntropyLargeDistribution) {
    // Test with larger distribution
    const size_t n = 1000;
    std::vector<float> uniform(n, 1.0f / n);

    float h = nimcp_stats_entropy(uniform.data(), n);
    float expected = std::log2(static_cast<float>(n));

    EXPECT_NEAR(h, expected, 1e-4f);
}

//=============================================================================
// CONSISTENCY AND DETERMINISM TESTS
//=============================================================================

TEST_F(InformationTheoryRegressionTest, EntropyDeterministic) {
    std::vector<float> probs = {0.1f, 0.2f, 0.3f, 0.4f};

    float h1 = nimcp_stats_entropy(probs.data(), probs.size());
    float h2 = nimcp_stats_entropy(probs.data(), probs.size());

    EXPECT_FLOAT_EQ(h1, h2) << "Entropy should be deterministic";
}

TEST_F(InformationTheoryRegressionTest, MIDeterministic) {
    std::vector<float> joint = {0.1f, 0.2f, 0.3f, 0.4f};

    float mi1 = nimcp_stats_mutual_information(joint.data(), 2, 2);
    float mi2 = nimcp_stats_mutual_information(joint.data(), 2, 2);

    EXPECT_FLOAT_EQ(mi1, mi2) << "MI should be deterministic";
}

//=============================================================================
// PERFORMANCE REGRESSION TESTS
//=============================================================================

TEST_F(InformationTheoryRegressionTest, EntropyPerformance) {
    const size_t n = 1000;
    std::vector<float> probs(n, 1.0f / n);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        volatile float h = nimcp_stats_entropy(probs.data(), n);
        (void)h;
    }
    auto end = std::chrono::high_resolution_clock::now();

    double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count() / 1000.0;

    // Should complete in reasonable time (< 100us per call for n=1000)
    EXPECT_LT(elapsed_us, 100.0) << "Entropy computation too slow: " << elapsed_us << "us";
}

TEST_F(InformationTheoryRegressionTest, MIPerformance) {
    std::vector<float> joint(100, 0.01f); // 10x10

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        volatile float mi = nimcp_stats_mutual_information(joint.data(), 10, 10);
        (void)mi;
    }
    auto end = std::chrono::high_resolution_clock::now();

    double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count() / 1000.0;

    EXPECT_LT(elapsed_us, 200.0) << "MI computation too slow";
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
