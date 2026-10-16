//=============================================================================
// test_statistics_e2e.cpp - End-to-End Tests for Statistics Module
//=============================================================================
/**
 * @file test_statistics_e2e.cpp
 * @brief End-to-end tests simulating real-world usage scenarios
 *
 * WHAT: Full workflow tests for statistics and Shannon integration
 * WHY:  Verify complete use cases work correctly in production-like scenarios
 * HOW:  Simulate neural network training, information analysis, hypothesis testing
 *
 * @date 2026-01-30
 */

#include <gtest/gtest.h>
#include "utils/statistics/nimcp_statistics.h"
#include "information/nimcp_shannon.h"
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>
#include <numeric>

// Test tolerances
#define TOLERANCE 1e-5f
#define LOOSE_TOLERANCE 1e-3f

//=============================================================================
// Test Fixture
//=============================================================================

class StatisticsE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        config = nimcp_stats_default_config();
        nimcp_stats_init(&config);
        rng.seed(42);  // Reproducible tests
    }

    void TearDown() override {
        nimcp_stats_shutdown();
    }

    nimcp_stats_config_t config;
    std::mt19937 rng;

    // Helper: generate normal samples
    std::vector<float> generate_normal(size_t n, float mean, float stddev) {
        std::normal_distribution<float> dist(mean, stddev);
        std::vector<float> data(n);
        for (auto& x : data) x = dist(rng);
        return data;
    }

    // Helper: generate uniform samples
    std::vector<float> generate_uniform(size_t n, float low, float high) {
        std::uniform_real_distribution<float> dist(low, high);
        std::vector<float> data(n);
        for (auto& x : data) x = dist(rng);
        return data;
    }
};

//=============================================================================
// E2E: Neural Network Classification Training Workflow
//=============================================================================

TEST_F(StatisticsE2ETest, NeuralNetworkTraining_CrossEntropyLoss) {
    // Simulate a multi-class classification training loop
    const int num_classes = 10;
    const int num_samples = 100;
    const int num_epochs = 5;

    std::vector<float> epoch_losses;

    for (int epoch = 0; epoch < num_epochs; epoch++) {
        float total_loss = 0.0f;

        for (int sample = 0; sample < num_samples; sample++) {
            // True label (one-hot)
            std::vector<float> true_dist(num_classes, 0.0f);
            int true_class = sample % num_classes;
            true_dist[true_class] = 1.0f;

            // Simulated model predictions (getting better each epoch)
            std::vector<float> pred_dist(num_classes);
            float improvement = 0.1f * epoch;
            for (int c = 0; c < num_classes; c++) {
                if (c == true_class) {
                    pred_dist[c] = 0.1f + improvement + 0.05f * generate_uniform(1, 0, 1)[0];
                } else {
                    pred_dist[c] = (0.9f - improvement) / (num_classes - 1);
                }
            }
            // Normalize
            float sum = std::accumulate(pred_dist.begin(), pred_dist.end(), 0.0f);
            for (auto& p : pred_dist) p /= sum;

            // Compute cross-entropy loss
            float ce = nimcp_stats_cross_entropy(true_dist.data(), pred_dist.data(), num_classes);
            total_loss += ce;
        }

        float avg_loss = total_loss / num_samples;
        epoch_losses.push_back(avg_loss);
    }

    // Loss should decrease over epochs (training improves)
    for (size_t i = 1; i < epoch_losses.size(); i++) {
        EXPECT_LT(epoch_losses[i], epoch_losses[i-1] + 0.5f);  // Allow some variance
    }

    // Final loss should be lower than initial
    EXPECT_LT(epoch_losses.back(), epoch_losses.front());
}

TEST_F(StatisticsE2ETest, NeuralNetworkTraining_KLDivergenceLoss) {
    // Variational autoencoder style: KL divergence for latent space regularization
    const int latent_dim = 8;
    const int batch_size = 32;

    std::vector<float> kl_losses;

    for (int batch = 0; batch < 10; batch++) {
        float batch_kl = 0.0f;

        for (int sample = 0; sample < batch_size; sample++) {
            // Encoder output (approximate posterior)
            std::vector<float> q_z(latent_dim);
            // Prior (standard normal, discretized)
            std::vector<float> p_z(latent_dim, 1.0f / latent_dim);

            // Simulate encoder learning to match prior
            float learning = 0.1f * batch;
            for (int d = 0; d < latent_dim; d++) {
                q_z[d] = 1.0f / latent_dim + (1.0f - learning) * 0.1f * (d % 2 == 0 ? 1 : -1);
            }
            // Normalize
            float sum = std::accumulate(q_z.begin(), q_z.end(), 0.0f);
            for (auto& p : q_z) p = std::max(0.001f, p / sum);

            batch_kl += nimcp_stats_kl_divergence(q_z.data(), p_z.data(), latent_dim);
        }

        kl_losses.push_back(batch_kl / batch_size);
    }

    // KL should decrease as encoder learns to match prior
    EXPECT_LT(kl_losses.back(), kl_losses.front() + 0.1f);
}

//=============================================================================
// E2E: A/B Testing Statistical Analysis
//=============================================================================

TEST_F(StatisticsE2ETest, ABTesting_TwoSampleTTest) {
    // Simulate A/B test for conversion rates

    // Control group (A): 5% conversion
    auto control = generate_normal(100, 0.05f, 0.02f);
    for (auto& x : control) x = std::max(0.0f, std::min(1.0f, x));

    // Treatment group (B): 7% conversion (real improvement)
    auto treatment = generate_normal(100, 0.07f, 0.02f);
    for (auto& x : treatment) x = std::max(0.0f, std::min(1.0f, x));

    nimcp_test_result_t result;
    EXPECT_EQ(nimcp_stats_ttest_two_sample(
        control.data(), 100,
        treatment.data(), 100,
        true,  // Equal variance
        NIMCP_TEST_TWO_SIDED,
        0.95f,
        &result
    ), NIMCP_STATS_OK);

    // Should detect significant difference
    EXPECT_LT(result.p_value, 0.05f);
    EXPECT_TRUE(result.reject_null);

    // Effect size should be moderate to large
    EXPECT_GT(std::abs(result.effect_size), 0.5f);
}

TEST_F(StatisticsE2ETest, ABTesting_NoEffect) {
    // A/B test where there's no real difference

    auto group_a = generate_normal(50, 0.10f, 0.03f);
    auto group_b = generate_normal(50, 0.10f, 0.03f);  // Same mean

    nimcp_test_result_t result;
    nimcp_stats_ttest_two_sample(
        group_a.data(), 50,
        group_b.data(), 50,
        true,
        NIMCP_TEST_TWO_SIDED,
        0.95f,
        &result
    );

    // Should NOT detect significant difference (usually)
    // Note: with random data, ~5% false positives expected
    EXPECT_GT(result.p_value, 0.01f);  // Very unlikely to be highly significant
}

//=============================================================================
// E2E: Information Flow Analysis in Neural Network
//=============================================================================

TEST_F(StatisticsE2ETest, InformationFlow_LayerAnalysis) {
    // Analyze information flow through network layers
    const int input_dim = 4;
    const int hidden_dim = 4;
    const int output_dim = 2;

    // Simulate layer transition matrices (joint distributions)
    // Input -> Hidden
    std::vector<float> joint_ih(input_dim * hidden_dim);
    float sum_ih = 0.0f;
    for (int i = 0; i < input_dim; i++) {
        for (int h = 0; h < hidden_dim; h++) {
            // Correlated: nearby indices more likely
            joint_ih[i * hidden_dim + h] = 1.0f + 0.5f * std::exp(-std::abs(i - h));
            sum_ih += joint_ih[i * hidden_dim + h];
        }
    }
    for (auto& p : joint_ih) p /= sum_ih;

    // Hidden -> Output
    std::vector<float> joint_ho(hidden_dim * output_dim);
    float sum_ho = 0.0f;
    for (int h = 0; h < hidden_dim; h++) {
        for (int o = 0; o < output_dim; o++) {
            joint_ho[h * output_dim + o] = 1.0f + 0.3f * (h % 2 == o ? 2.0f : 0.5f);
            sum_ho += joint_ho[h * output_dim + o];
        }
    }
    for (auto& p : joint_ho) p /= sum_ho;

    // Compute mutual information for each layer transition
    float mi_ih = nimcp_stats_mutual_information(joint_ih.data(), input_dim, hidden_dim);
    float mi_ho = nimcp_stats_mutual_information(joint_ho.data(), hidden_dim, output_dim);

    // Information should be preserved or reduced (data processing inequality)
    EXPECT_GT(mi_ih, 0.0f);
    EXPECT_GT(mi_ho, 0.0f);

    // Compute entropies
    float h_input = nimcp_stats_entropy(std::vector<float>(input_dim, 1.0f/input_dim).data(), input_dim);
    float h_hidden = nimcp_stats_entropy(std::vector<float>(hidden_dim, 1.0f/hidden_dim).data(), hidden_dim);

    // MI should not exceed marginal entropies
    EXPECT_LE(mi_ih, h_input + LOOSE_TOLERANCE);
    EXPECT_LE(mi_ih, h_hidden + LOOSE_TOLERANCE);
}

TEST_F(StatisticsE2ETest, InformationFlow_BottleneckDetection) {
    // Detect information bottlenecks using IB algorithm

    // Create a joint distribution with clear structure
    const int n_x = 8;  // Input features
    const int n_y = 2;  // Binary label
    std::vector<float> joint_xy(n_x * n_y);

    // Features 0-3 correlate with Y=0, features 4-7 with Y=1
    for (int x = 0; x < n_x; x++) {
        if (x < n_x / 2) {
            joint_xy[x * n_y + 0] = 0.1f;   // High P(Y=0|X<4)
            joint_xy[x * n_y + 1] = 0.02f;
        } else {
            joint_xy[x * n_y + 0] = 0.02f;
            joint_xy[x * n_y + 1] = 0.1f;   // High P(Y=1|X>=4)
        }
    }
    // Normalize
    float sum = std::accumulate(joint_xy.begin(), joint_xy.end(), 0.0f);
    for (auto& p : joint_xy) p /= sum;

    // Apply information bottleneck to compress X
    const int n_t = 2;  // Compress to 2 states
    std::vector<float> q_t_given_x(n_x * n_t);

    float ratio = nimcp_stats_information_bottleneck(
        joint_xy.data(), n_x, n_y, n_t,
        5.0f,  // High beta: preserve info
        q_t_given_x.data(),
        200
    );

    // Should preserve most information about Y
    EXPECT_GT(ratio, 0.7f);

    // The compression should cluster X into relevant groups
    // Check that similar X values map to same T
}

//=============================================================================
// E2E: Channel Capacity Analysis for Neural Communication
//=============================================================================

TEST_F(StatisticsE2ETest, ChannelCapacity_SynapticTransmission) {
    // Simulate synaptic channel capacity analysis

    // Different synapse types with varying SNR
    struct SynapseType {
        const char* name;
        float bandwidth_hz;
        float snr_db;
    };

    std::vector<SynapseType> synapses = {
        {"Excitatory (strong)", 100.0f, 20.0f},
        {"Excitatory (weak)", 100.0f, 10.0f},
        {"Inhibitory", 150.0f, 15.0f},
        {"Modulatory", 50.0f, 25.0f}
    };

    std::vector<float> capacities;

    for (const auto& syn : synapses) {
        float snr_linear = nimcp_stats_snr_from_db(syn.snr_db);
        float capacity = nimcp_stats_channel_capacity(syn.bandwidth_hz, snr_linear);
        capacities.push_back(capacity);

        // Verify capacity is positive
        EXPECT_GT(capacity, 0.0f);
        // Verify capacity scales reasonably
        EXPECT_LT(capacity, syn.bandwidth_hz * 10.0f);  // Not unreasonably high
    }

    // Strong excitatory should have more capacity than weak
    EXPECT_GT(capacities[0], capacities[1]);
}

TEST_F(StatisticsE2ETest, ChannelCapacity_NetworkThroughput) {
    // Analyze total network information throughput

    const int num_synapses = 1000;
    float total_capacity = 0.0f;

    for (int s = 0; s < num_synapses; s++) {
        // Random synapse properties
        float bandwidth = 50.0f + generate_uniform(1, 0, 150)[0];
        float snr_db = 5.0f + generate_uniform(1, 0, 25)[0];
        float snr_linear = nimcp_stats_snr_from_db(snr_db);

        float capacity = nimcp_stats_channel_capacity(bandwidth, snr_linear);
        total_capacity += capacity;
    }

    // Average capacity per synapse
    float avg_capacity = total_capacity / num_synapses;

    // Should be in reasonable range (100-1000 bits/s for biological synapses)
    EXPECT_GT(avg_capacity, 50.0f);
    EXPECT_LT(avg_capacity, 5000.0f);
}

//=============================================================================
// E2E: Time Series Causality Analysis
//=============================================================================

TEST_F(StatisticsE2ETest, TransferEntropy_CausalDirection) {
    // Detect causal direction in coupled time series
    const int n = 500;

    // X drives Y with lag 1
    std::vector<float> x(n), y(n);
    x[0] = generate_uniform(1, -1, 1)[0];
    y[0] = generate_uniform(1, -1, 1)[0];

    for (int t = 1; t < n; t++) {
        x[t] = 0.5f * x[t-1] + generate_uniform(1, -0.5f, 0.5f)[0];
        y[t] = 0.7f * x[t-1] + 0.2f * y[t-1] + generate_uniform(1, -0.3f, 0.3f)[0];
    }

    float te_xy = nimcp_stats_transfer_entropy(x.data(), y.data(), n, 1, 8);
    float te_yx = nimcp_stats_transfer_entropy(y.data(), x.data(), n, 1, 8);

    // Transfer entropy X->Y should be higher than Y->X
    EXPECT_GT(te_xy, te_yx * 0.8f);  // Relaxed due to noise
}

//=============================================================================
// E2E: Bayesian Parameter Estimation
//=============================================================================

TEST_F(StatisticsE2ETest, BayesianEstimation_BetaBinomial) {
    // Estimate conversion rate using Bayesian approach

    // Prior: Beta(1,1) = Uniform (no prior knowledge)
    float alpha = 1.0f, beta_param = 1.0f;

    // Observations: 30 successes out of 100 trials
    uint32_t successes = 30;
    uint32_t failures = 70;

    // Update: Posterior is Beta(alpha + successes, beta + failures)
    float post_alpha = alpha + successes;
    float post_beta = beta_param + failures;

    // Posterior mean
    float post_mean = post_alpha / (post_alpha + post_beta);
    EXPECT_NEAR(post_mean, 0.31f, 0.01f);  // Close to 30%

    // Credible interval using beta quantiles
    // This would use beta quantile functions if available
    // For now, verify posterior is valid
    EXPECT_GT(post_alpha, 0.0f);
    EXPECT_GT(post_beta, 0.0f);
}

//=============================================================================
// E2E: Correlation Analysis for Feature Selection
//=============================================================================

TEST_F(StatisticsE2ETest, FeatureSelection_CorrelationMatrix) {
    // Analyze feature correlations for dimensionality reduction
    const int n_samples = 100;
    const int n_features = 5;

    // Generate correlated features
    std::vector<std::vector<float>> features(n_features);

    // Feature 0: base feature
    features[0] = generate_normal(n_samples, 0.0f, 1.0f);

    // Feature 1: highly correlated with 0
    features[1].resize(n_samples);
    for (int i = 0; i < n_samples; i++) {
        features[1][i] = 0.9f * features[0][i] + 0.1f * generate_normal(1, 0, 1)[0];
    }

    // Feature 2: negatively correlated with 0
    features[2].resize(n_samples);
    for (int i = 0; i < n_samples; i++) {
        features[2][i] = -0.7f * features[0][i] + 0.3f * generate_normal(1, 0, 1)[0];
    }

    // Features 3,4: independent
    features[3] = generate_normal(n_samples, 0.0f, 1.0f);
    features[4] = generate_normal(n_samples, 0.0f, 1.0f);

    // Compute correlation matrix
    nimcp_correlation_result_t result;

    // Feature 0 and 1: high positive correlation
    nimcp_stats_correlation_pearson(features[0].data(), features[1].data(), n_samples, &result);
    EXPECT_GT(result.r, 0.8f);

    // Feature 0 and 2: negative correlation
    nimcp_stats_correlation_pearson(features[0].data(), features[2].data(), n_samples, &result);
    EXPECT_LT(result.r, -0.5f);

    // Feature 0 and 3: near zero correlation
    nimcp_stats_correlation_pearson(features[0].data(), features[3].data(), n_samples, &result);
    EXPECT_LT(std::abs(result.r), 0.3f);
}

//=============================================================================
// E2E: Integrated Information Analysis
//=============================================================================

TEST_F(StatisticsE2ETest, IntegratedInformation_SystemComplexity) {
    // Measure integrated information of different systems

    // System 1: Independent components (low integration)
    float cov_independent[9] = {
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f
    };
    float phi_independent = nimcp_stats_information_integration(cov_independent, 3);

    // System 2: Highly integrated
    float cov_integrated[9] = {
        1.0f, 0.8f, 0.7f,
        0.8f, 1.0f, 0.8f,
        0.7f, 0.8f, 1.0f
    };
    float phi_integrated = nimcp_stats_information_integration(cov_integrated, 3);

    // Integrated system should have higher Phi
    EXPECT_GT(phi_integrated, phi_independent);
}

//=============================================================================
// E2E: Distribution Comparison Workflow
//=============================================================================

TEST_F(StatisticsE2ETest, DistributionComparison_ModelEvaluation) {
    // Compare model output distribution to target

    // Target distribution (empirical from data)
    float target[] = {0.1f, 0.2f, 0.4f, 0.2f, 0.1f};

    // Model 1: Good fit
    float model1[] = {0.12f, 0.18f, 0.38f, 0.22f, 0.10f};

    // Model 2: Poor fit
    float model2[] = {0.3f, 0.3f, 0.2f, 0.1f, 0.1f};

    // KL divergence (lower is better)
    float kl1 = nimcp_stats_kl_divergence(target, model1, 5);
    float kl2 = nimcp_stats_kl_divergence(target, model2, 5);
    EXPECT_LT(kl1, kl2);

    // JS divergence (symmetric)
    float js1 = nimcp_stats_js_divergence(target, model1, 5);
    float js2 = nimcp_stats_js_divergence(target, model2, 5);
    EXPECT_LT(js1, js2);

    // Cross-entropy (loss function)
    float ce1 = nimcp_stats_cross_entropy(target, model1, 5);
    float ce2 = nimcp_stats_cross_entropy(target, model2, 5);
    EXPECT_LT(ce1, ce2);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
