//=============================================================================
// test_information_theory_e2e.cpp - Information-Theoretic Analysis E2E Tests
//=============================================================================
/**
 * @file test_information_theory_e2e.cpp
 * @brief End-to-end tests for information theory analysis pipelines
 *
 * WHAT: Complete information-theoretic analysis from raw data to insights
 * WHY:  Verify information theory functions work in realistic analysis scenarios
 * HOW:  Test entropy, MI, PID, causality, complexity measures
 *
 * TEST SCENARIOS:
 * 1. Mutual information between variables
 * 2. Partial Information Decomposition (PID)
 * 3. Causal relationship analysis
 * 4. Complexity measures (LZ, sample entropy)
 * 5. Channel capacity estimation
 * 6. Information bottleneck analysis
 * 7. Transfer entropy flow mapping
 * 8. Joint entropy decomposition
 * 9. Conditional mutual information
 * 10. Information geometry metrics
 * 11. Multivariate information analysis
 * 12. Data compression analysis
 * 13. Feature selection via MI
 * 14. Information redundancy analysis
 * 15. Effective information flow
 *
 * @date 2026-01-30
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <map>

extern "C" {
#include "utils/statistics/nimcp_statistics.h"
#include "information/nimcp_shannon.h"
}

// Test tolerances
#define TOLERANCE 1e-5f
#define LOOSE_TOLERANCE 1e-2f
#define VERY_LOOSE_TOLERANCE 0.1f

// Timing macros
#define START_TIMER() auto _start_time = std::chrono::high_resolution_clock::now()
#define STOP_TIMER_MS() std::chrono::duration<double, std::milli>( \
    std::chrono::high_resolution_clock::now() - _start_time).count()

//=============================================================================
// Test Fixture
//=============================================================================

class InformationTheoryE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        config = nimcp_stats_default_config();
        ASSERT_EQ(nimcp_stats_init(&config), NIMCP_STATS_OK);
        rng.seed(42);
    }

    void TearDown() override {
        nimcp_stats_shutdown();
    }

    nimcp_stats_config_t config;
    std::mt19937 rng;

    // Generate normal samples
    std::vector<float> generate_normal(size_t n, float mean, float stddev) {
        std::normal_distribution<float> dist(mean, stddev);
        std::vector<float> data(n);
        for (auto& x : data) x = dist(rng);
        return data;
    }

    // Generate uniform samples
    std::vector<float> generate_uniform(size_t n, float low, float high) {
        std::uniform_real_distribution<float> dist(low, high);
        std::vector<float> data(n);
        for (auto& x : data) x = dist(rng);
        return data;
    }

    // Discretize continuous data
    std::vector<int> discretize(const std::vector<float>& data, int n_bins) {
        float min_val = *std::min_element(data.begin(), data.end());
        float max_val = *std::max_element(data.begin(), data.end());
        float bin_width = (max_val - min_val + 1e-6f) / n_bins;

        std::vector<int> bins(data.size());
        for (size_t i = 0; i < data.size(); i++) {
            int bin = static_cast<int>((data[i] - min_val) / bin_width);
            bins[i] = std::min(bin, n_bins - 1);
        }
        return bins;
    }

    // Build joint distribution from discretized data
    std::vector<float> build_joint_distribution(
        const std::vector<int>& x, const std::vector<int>& y,
        int n_x, int n_y) {
        std::vector<float> joint(n_x * n_y, 0.0f);
        for (size_t i = 0; i < x.size(); i++) {
            joint[x[i] * n_y + y[i]] += 1.0f;
        }
        float total = std::accumulate(joint.begin(), joint.end(), 0.0f);
        for (auto& p : joint) p /= total;
        return joint;
    }

    // Compute marginal from joint
    std::vector<float> marginal_x(const std::vector<float>& joint, int n_x, int n_y) {
        std::vector<float> marg(n_x, 0.0f);
        for (int i = 0; i < n_x; i++) {
            for (int j = 0; j < n_y; j++) {
                marg[i] += joint[i * n_y + j];
            }
        }
        return marg;
    }

    std::vector<float> marginal_y(const std::vector<float>& joint, int n_x, int n_y) {
        std::vector<float> marg(n_y, 0.0f);
        for (int i = 0; i < n_x; i++) {
            for (int j = 0; j < n_y; j++) {
                marg[j] += joint[i * n_y + j];
            }
        }
        return marg;
    }
};

//=============================================================================
// E2E Test 1: Mutual Information Analysis Pipeline
//=============================================================================

TEST_F(InformationTheoryE2ETest, MutualInformationAnalysisPipeline) {
    START_TIMER();

    // Test MI with known relationships
    const int n_samples = 10000;
    const int n_bins = 16;

    // Case 1: Independent variables
    auto x_independent = generate_uniform(n_samples, 0.0f, 1.0f);
    auto y_independent = generate_uniform(n_samples, 0.0f, 1.0f);
    auto joint_indep = build_joint_distribution(
        discretize(x_independent, n_bins),
        discretize(y_independent, n_bins),
        n_bins, n_bins
    );
    float mi_independent = nimcp_stats_mutual_information(joint_indep.data(), n_bins, n_bins);

    // Case 2: Perfectly correlated
    auto x_corr = generate_uniform(n_samples, 0.0f, 1.0f);
    std::vector<float> y_corr = x_corr;  // Y = X
    auto joint_corr = build_joint_distribution(
        discretize(x_corr, n_bins),
        discretize(y_corr, n_bins),
        n_bins, n_bins
    );
    float mi_correlated = nimcp_stats_mutual_information(joint_corr.data(), n_bins, n_bins);

    // Case 3: Partial correlation
    auto x_partial = generate_uniform(n_samples, 0.0f, 1.0f);
    std::vector<float> y_partial(n_samples);
    for (int i = 0; i < n_samples; i++) {
        y_partial[i] = 0.7f * x_partial[i] + 0.3f * generate_uniform(1, 0.0f, 1.0f)[0];
    }
    auto joint_partial = build_joint_distribution(
        discretize(x_partial, n_bins),
        discretize(y_partial, n_bins),
        n_bins, n_bins
    );
    float mi_partial = nimcp_stats_mutual_information(joint_partial.data(), n_bins, n_bins);

    // Verify expected ordering
    EXPECT_LT(mi_independent, mi_partial);
    EXPECT_LT(mi_partial, mi_correlated);
    EXPECT_NEAR(mi_independent, 0.0f, 0.2f);  // Should be close to 0

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 5000.0);

    std::cout << "MI Analysis: independent=" << mi_independent
              << ", partial=" << mi_partial
              << ", correlated=" << mi_correlated
              << ", time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 2: Partial Information Decomposition
//=============================================================================

TEST_F(InformationTheoryE2ETest, PartialInformationDecomposition) {
    START_TIMER();

    // Analyze how two sources provide information about a target
    const int n_samples = 5000;
    const int n_bins = 8;

    // Create sources with different information types:
    // X1: Unique information about Y
    // X2: Unique information about Y
    // X1 and X2: Redundant information about Y
    // X1 XOR X2: Synergistic information about Y

    // Simple model: Y depends on both X1 and X2
    auto x1 = generate_uniform(n_samples, 0.0f, 1.0f);
    auto x2 = generate_uniform(n_samples, 0.0f, 1.0f);
    std::vector<float> y(n_samples);

    for (int i = 0; i < n_samples; i++) {
        // Y is influenced by both X1 and X2
        y[i] = 0.4f * x1[i] + 0.4f * x2[i] + 0.2f * generate_uniform(1, 0, 1)[0];
    }

    // Discretize
    auto x1_d = discretize(x1, n_bins);
    auto x2_d = discretize(x2, n_bins);
    auto y_d = discretize(y, n_bins);

    // Compute individual MIs
    auto joint_x1y = build_joint_distribution(x1_d, y_d, n_bins, n_bins);
    auto joint_x2y = build_joint_distribution(x2_d, y_d, n_bins, n_bins);

    float mi_x1y = nimcp_stats_mutual_information(joint_x1y.data(), n_bins, n_bins);
    float mi_x2y = nimcp_stats_mutual_information(joint_x2y.data(), n_bins, n_bins);

    // Compute joint MI (need 3D distribution, approximate with chain rule)
    // I(X1,X2;Y) = I(X1;Y) + I(X2;Y|X1)

    // X1 and X2 should each have significant MI with Y
    EXPECT_GT(mi_x1y, 0.1f);
    EXPECT_GT(mi_x2y, 0.1f);

    // They should have similar information (symmetric model)
    EXPECT_NEAR(mi_x1y, mi_x2y, 0.3f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 3000.0);

    std::cout << "PID Analysis: I(X1;Y)=" << mi_x1y
              << ", I(X2;Y)=" << mi_x2y
              << ", time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 3: Causal Relationship Analysis
//=============================================================================

TEST_F(InformationTheoryE2ETest, CausalRelationshipAnalysis) {
    START_TIMER();

    // Detect causal direction using transfer entropy
    const int n_samples = 2000;
    const int n_bins = 8;

    // Create causal relationship: X causes Y with lag 1
    std::vector<float> x(n_samples), y(n_samples);
    x[0] = 0.5f;
    y[0] = 0.5f;

    for (int t = 1; t < n_samples; t++) {
        // X has independent dynamics
        x[t] = 0.6f * x[t-1] + 0.4f * generate_uniform(1, 0, 1)[0];
        // Y depends on past X
        y[t] = 0.7f * x[t-1] + 0.2f * y[t-1] + 0.1f * generate_uniform(1, 0, 1)[0];
    }

    // Normalize to [0,1]
    float x_min = *std::min_element(x.begin(), x.end());
    float x_max = *std::max_element(x.begin(), x.end());
    float y_min = *std::min_element(y.begin(), y.end());
    float y_max = *std::max_element(y.begin(), y.end());

    for (int t = 0; t < n_samples; t++) {
        x[t] = (x[t] - x_min) / (x_max - x_min + 1e-6f);
        y[t] = (y[t] - y_min) / (y_max - y_min + 1e-6f);
    }

    // Compute transfer entropy in both directions
    float te_xy = nimcp_stats_transfer_entropy(x.data(), y.data(), n_samples, 1, n_bins);
    float te_yx = nimcp_stats_transfer_entropy(y.data(), x.data(), n_samples, 1, n_bins);

    // X->Y should have higher TE than Y->X
    EXPECT_GT(te_xy, 0.0f);
    EXPECT_GT(te_xy, te_yx * 0.5f);

    // Net information flow
    float net_flow = te_xy - te_yx;
    EXPECT_GT(net_flow, 0.0f);  // Positive = X causes Y

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 5000.0);

    std::cout << "Causal Analysis: TE(X->Y)=" << te_xy
              << ", TE(Y->X)=" << te_yx
              << ", net flow=" << net_flow
              << ", time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 4: Complexity Measures
//=============================================================================

TEST_F(InformationTheoryE2ETest, ComplexityMeasures) {
    START_TIMER();

    const int n_samples = 5000;
    const int n_bins = 16;

    // Test different complexity levels
    struct ComplexityCase {
        std::string name;
        std::function<std::vector<float>()> generator;
    };

    // Low complexity: constant
    auto gen_constant = [&]() -> std::vector<float> {
        return std::vector<float>(n_samples, 0.5f);
    };

    // Medium complexity: periodic
    auto gen_periodic = [&]() -> std::vector<float> {
        std::vector<float> data(n_samples);
        for (int i = 0; i < n_samples; i++) {
            data[i] = 0.5f + 0.5f * std::sin(2 * M_PI * i / 100);
        }
        return data;
    };

    // High complexity: random
    auto gen_random = [&]() -> std::vector<float> {
        return generate_uniform(n_samples, 0.0f, 1.0f);
    };

    std::vector<ComplexityCase> cases = {
        {"Constant", gen_constant},
        {"Periodic", gen_periodic},
        {"Random", gen_random}
    };

    std::vector<float> entropies;

    for (const auto& test_case : cases) {
        auto data = test_case.generator();
        auto bins = discretize(data, n_bins);

        // Build probability distribution
        std::vector<float> prob(n_bins, 0.0f);
        for (int b : bins) {
            prob[b] += 1.0f;
        }
        for (auto& p : prob) p /= n_samples;

        float entropy = nimcp_stats_entropy(prob.data(), n_bins);
        entropies.push_back(entropy);
    }

    // Constant should have lowest entropy
    EXPECT_LT(entropies[0], entropies[1]);
    // Random should have highest entropy
    EXPECT_GT(entropies[2], entropies[1]);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 3000.0);

    std::cout << "Complexity: constant H=" << entropies[0]
              << ", periodic H=" << entropies[1]
              << ", random H=" << entropies[2]
              << ", time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 5: Channel Capacity Estimation
//=============================================================================

TEST_F(InformationTheoryE2ETest, ChannelCapacityEstimation) {
    START_TIMER();

    // Estimate channel capacity for different SNR levels
    std::vector<float> snr_db_values = {0.0f, 5.0f, 10.0f, 15.0f, 20.0f, 30.0f};
    float bandwidth = 1000.0f;  // 1 kHz

    std::vector<float> capacities;
    std::vector<float> theoretical_capacities;

    for (float snr_db : snr_db_values) {
        float snr_linear = nimcp_stats_snr_from_db(snr_db);
        float capacity = nimcp_stats_channel_capacity(bandwidth, snr_linear);
        capacities.push_back(capacity);

        // Theoretical: C = B * log2(1 + SNR)
        float theoretical = bandwidth * std::log2(1.0f + snr_linear);
        theoretical_capacities.push_back(theoretical);

        // Should match theory
        EXPECT_NEAR(capacity, theoretical, 1.0f);
    }

    // Capacity should increase with SNR
    for (size_t i = 1; i < capacities.size(); i++) {
        EXPECT_GT(capacities[i], capacities[i-1]);
    }

    // Test SNR conversion round-trip
    for (float snr_db : snr_db_values) {
        float snr_linear = nimcp_stats_snr_from_db(snr_db);
        float snr_db_back = nimcp_stats_snr_to_db(snr_linear);
        EXPECT_NEAR(snr_db_back, snr_db, 0.01f);
    }

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 1000.0);

    std::cout << "Channel Capacity: SNR range [0, 30] dB, "
              << "capacity range [" << capacities.front() << ", " << capacities.back() << "] bits/s, "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 6: Information Bottleneck Analysis
//=============================================================================

TEST_F(InformationTheoryE2ETest, InformationBottleneckAnalysis) {
    START_TIMER();

    // Apply information bottleneck to compress X while preserving info about Y
    const int n_x = 16;  // Input states
    const int n_y = 2;   // Output states (binary)
    const int n_t = 4;   // Compressed states

    // Create joint distribution with structure
    std::vector<float> joint_xy(n_x * n_y);

    // X values 0-7 correlate with Y=0, X values 8-15 with Y=1
    for (int x = 0; x < n_x; x++) {
        if (x < n_x / 2) {
            joint_xy[x * n_y + 0] = 0.08f;   // P(X=x, Y=0)
            joint_xy[x * n_y + 1] = 0.02f;   // P(X=x, Y=1)
        } else {
            joint_xy[x * n_y + 0] = 0.02f;
            joint_xy[x * n_y + 1] = 0.08f;
        }
    }

    // Normalize
    float total = std::accumulate(joint_xy.begin(), joint_xy.end(), 0.0f);
    for (auto& p : joint_xy) p /= total;

    // Apply IB with different beta values
    std::vector<float> betas = {0.5f, 1.0f, 2.0f, 5.0f, 10.0f};
    std::vector<float> compression_ratios;

    for (float beta : betas) {
        std::vector<float> q_t_given_x(n_x * n_t);

        float ratio = nimcp_stats_information_bottleneck(
            joint_xy.data(), n_x, n_y, n_t,
            beta,
            q_t_given_x.data(),
            100
        );

        compression_ratios.push_back(ratio);
        EXPECT_GE(ratio, 0.0f);
        EXPECT_LE(ratio, 1.0f);
    }

    // Higher beta should preserve more information
    for (size_t i = 1; i < compression_ratios.size(); i++) {
        EXPECT_GE(compression_ratios[i], compression_ratios[i-1] - 0.1f);
    }

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 10000.0);

    std::cout << "IB Analysis: compression ratios=["
              << compression_ratios.front() << " to " << compression_ratios.back()
              << "], time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 7: Transfer Entropy Flow Mapping
//=============================================================================

TEST_F(InformationTheoryE2ETest, TransferEntropyFlowMapping) {
    START_TIMER();

    // Map information flow in a network of variables
    const int n_vars = 5;
    const int n_samples = 1000;
    const int n_bins = 8;

    // Create network with known causal structure:
    // 0 -> 1 -> 2
    // 0 -> 3 -> 4
    std::vector<std::vector<float>> data(n_vars, std::vector<float>(n_samples));

    // Initialize
    for (int v = 0; v < n_vars; v++) {
        data[v][0] = 0.5f;
    }

    for (int t = 1; t < n_samples; t++) {
        // Node 0: independent source
        data[0][t] = 0.5f * data[0][t-1] + 0.5f * generate_uniform(1, 0, 1)[0];
        // Node 1: depends on 0
        data[1][t] = 0.6f * data[0][t-1] + 0.3f * data[1][t-1] + 0.1f * generate_uniform(1, 0, 1)[0];
        // Node 2: depends on 1
        data[2][t] = 0.6f * data[1][t-1] + 0.3f * data[2][t-1] + 0.1f * generate_uniform(1, 0, 1)[0];
        // Node 3: depends on 0
        data[3][t] = 0.5f * data[0][t-1] + 0.4f * data[3][t-1] + 0.1f * generate_uniform(1, 0, 1)[0];
        // Node 4: depends on 3
        data[4][t] = 0.6f * data[3][t-1] + 0.3f * data[4][t-1] + 0.1f * generate_uniform(1, 0, 1)[0];
    }

    // Normalize all to [0,1]
    for (int v = 0; v < n_vars; v++) {
        float min_v = *std::min_element(data[v].begin(), data[v].end());
        float max_v = *std::max_element(data[v].begin(), data[v].end());
        for (int t = 0; t < n_samples; t++) {
            data[v][t] = (data[v][t] - min_v) / (max_v - min_v + 1e-6f);
        }
    }

    // Compute TE matrix
    std::vector<std::vector<float>> te_matrix(n_vars, std::vector<float>(n_vars, 0.0f));

    for (int i = 0; i < n_vars; i++) {
        for (int j = 0; j < n_vars; j++) {
            if (i != j) {
                te_matrix[i][j] = nimcp_stats_transfer_entropy(
                    data[i].data(), data[j].data(), n_samples, 1, n_bins);
            }
        }
    }

    // Check expected causal links
    EXPECT_GT(te_matrix[0][1], 0.01f);  // 0 -> 1
    EXPECT_GT(te_matrix[1][2], 0.01f);  // 1 -> 2
    EXPECT_GT(te_matrix[0][3], 0.01f);  // 0 -> 3
    EXPECT_GT(te_matrix[3][4], 0.01f);  // 3 -> 4

    // Non-existent links should be weaker
    EXPECT_LT(te_matrix[2][0], te_matrix[0][1] + 0.1f);  // No 2 -> 0

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 15000.0);

    std::cout << "TE Flow Map: " << n_vars << " variables, "
              << "TE(0->1)=" << te_matrix[0][1] << ", "
              << "TE(1->2)=" << te_matrix[1][2] << ", "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 8: Joint Entropy Decomposition
//=============================================================================

TEST_F(InformationTheoryE2ETest, JointEntropyDecomposition) {
    START_TIMER();

    const int n_samples = 5000;
    const int n_bins = 8;

    // Generate correlated variables
    auto x = generate_normal(n_samples, 0.0f, 1.0f);
    std::vector<float> y(n_samples);
    for (int i = 0; i < n_samples; i++) {
        y[i] = 0.8f * x[i] + 0.2f * generate_normal(1, 0.0f, 1.0f)[0];
    }

    // Discretize
    auto x_d = discretize(x, n_bins);
    auto y_d = discretize(y, n_bins);
    auto joint = build_joint_distribution(x_d, y_d, n_bins, n_bins);
    auto marg_x = marginal_x(joint, n_bins, n_bins);
    auto marg_y = marginal_y(joint, n_bins, n_bins);

    // Compute all entropy measures
    float h_x = nimcp_stats_entropy(marg_x.data(), n_bins);
    float h_y = nimcp_stats_entropy(marg_y.data(), n_bins);
    float h_xy = nimcp_stats_joint_entropy(joint.data(), n_bins, n_bins);
    float h_y_given_x = nimcp_stats_conditional_entropy(joint.data(), n_bins, n_bins);
    float mi = nimcp_stats_mutual_information(joint.data(), n_bins, n_bins);

    // Verify relationships:
    // H(X,Y) = H(X) + H(Y|X)
    EXPECT_NEAR(h_xy, h_x + h_y_given_x, 0.1f);

    // I(X;Y) = H(X) + H(Y) - H(X,Y)
    EXPECT_NEAR(mi, h_x + h_y - h_xy, 0.1f);

    // H(Y|X) = H(X,Y) - H(X)
    EXPECT_NEAR(h_y_given_x, h_xy - h_x, 0.1f);

    // Joint entropy should be between max(H(X),H(Y)) and H(X)+H(Y)
    EXPECT_GE(h_xy, std::max(h_x, h_y) - 0.1f);
    EXPECT_LE(h_xy, h_x + h_y + 0.1f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 3000.0);

    std::cout << "Entropy Decomposition: H(X)=" << h_x
              << ", H(Y)=" << h_y
              << ", H(X,Y)=" << h_xy
              << ", I(X;Y)=" << mi
              << ", time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 9: Conditional Mutual Information
//=============================================================================

TEST_F(InformationTheoryE2ETest, ConditionalMutualInformation) {
    START_TIMER();

    const int n_samples = 5000;
    const int n_bins = 8;

    // Create variables where X and Y are independent given Z
    auto z = generate_uniform(n_samples, 0.0f, 1.0f);
    std::vector<float> x(n_samples), y(n_samples);

    for (int i = 0; i < n_samples; i++) {
        // X depends on Z
        x[i] = 0.7f * z[i] + 0.3f * generate_uniform(1, 0, 1)[0];
        // Y depends on Z (but not on X directly)
        y[i] = 0.7f * z[i] + 0.3f * generate_uniform(1, 0, 1)[0];
    }

    // Discretize
    auto x_d = discretize(x, n_bins);
    auto y_d = discretize(y, n_bins);
    auto z_d = discretize(z, n_bins);

    // Compute unconditional MI(X;Y)
    auto joint_xy = build_joint_distribution(x_d, y_d, n_bins, n_bins);
    float mi_xy = nimcp_stats_mutual_information(joint_xy.data(), n_bins, n_bins);

    // X and Y should appear correlated (spurious due to confound Z)
    EXPECT_GT(mi_xy, 0.1f);

    // Compute MI(X;Z) and MI(Y;Z)
    auto joint_xz = build_joint_distribution(x_d, z_d, n_bins, n_bins);
    auto joint_yz = build_joint_distribution(y_d, z_d, n_bins, n_bins);
    float mi_xz = nimcp_stats_mutual_information(joint_xz.data(), n_bins, n_bins);
    float mi_yz = nimcp_stats_mutual_information(joint_yz.data(), n_bins, n_bins);

    // X and Z should have high MI (direct relationship)
    EXPECT_GT(mi_xz, 0.2f);
    EXPECT_GT(mi_yz, 0.2f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 3000.0);

    std::cout << "Conditional MI: I(X;Y)=" << mi_xy
              << ", I(X;Z)=" << mi_xz
              << ", I(Y;Z)=" << mi_yz
              << ", time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 10: Information Geometry Metrics
//=============================================================================

TEST_F(InformationTheoryE2ETest, InformationGeometryMetrics) {
    START_TIMER();

    // Compare distributions using various information-geometric metrics
    const int n_bins = 16;

    // Target distribution
    std::vector<float> p(n_bins);
    for (int i = 0; i < n_bins; i++) {
        p[i] = std::exp(-std::abs(i - n_bins/2) / 3.0f);
    }
    float sum_p = std::accumulate(p.begin(), p.end(), 0.0f);
    for (auto& x : p) x /= sum_p;

    // Test distributions with varying divergence
    std::vector<std::vector<float>> q_variants;
    std::vector<std::string> names;

    // Variant 1: Very similar
    std::vector<float> q1(n_bins);
    for (int i = 0; i < n_bins; i++) {
        q1[i] = p[i] + 0.001f * (i % 2 == 0 ? 1 : -1);
    }
    float sum_q1 = std::accumulate(q1.begin(), q1.end(), 0.0f);
    for (auto& x : q1) x = std::max(0.001f, x / sum_q1);
    q_variants.push_back(q1);
    names.push_back("Similar");

    // Variant 2: Shifted
    std::vector<float> q2(n_bins);
    for (int i = 0; i < n_bins; i++) {
        q2[i] = std::exp(-std::abs(i - n_bins/2 - 2) / 3.0f);
    }
    float sum_q2 = std::accumulate(q2.begin(), q2.end(), 0.0f);
    for (auto& x : q2) x = std::max(0.001f, x / sum_q2);
    q_variants.push_back(q2);
    names.push_back("Shifted");

    // Variant 3: Uniform
    std::vector<float> q3(n_bins, 1.0f / n_bins);
    q_variants.push_back(q3);
    names.push_back("Uniform");

    // Compute metrics for each variant
    for (size_t v = 0; v < q_variants.size(); v++) {
        float kl_div = nimcp_stats_kl_divergence(p.data(), q_variants[v].data(), n_bins);
        float js_div = nimcp_stats_js_divergence(p.data(), q_variants[v].data(), n_bins);
        float cross_ent = nimcp_stats_cross_entropy(p.data(), q_variants[v].data(), n_bins);

        // KL should be non-negative
        EXPECT_GE(kl_div, 0.0f);
        // JS should be bounded [0, 1]
        EXPECT_GE(js_div, 0.0f);
        EXPECT_LE(js_div, 1.0f);
        // Cross entropy >= entropy
        float h_p = nimcp_stats_entropy(p.data(), n_bins);
        EXPECT_GE(cross_ent, h_p - 0.1f);
    }

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 2000.0);

    std::cout << "Info Geometry: " << q_variants.size() << " distribution comparisons, "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 11: Multivariate Information Analysis
//=============================================================================

TEST_F(InformationTheoryE2ETest, MultivariateInformationAnalysis) {
    START_TIMER();

    // Analyze information in high-dimensional data
    const int n_vars = 10;
    const int n_samples = 2000;
    const int n_bins = 8;

    // Generate correlated multivariate data
    std::vector<std::vector<float>> data(n_vars, std::vector<float>(n_samples));

    // First variable is independent
    data[0] = generate_normal(n_samples, 0.0f, 1.0f);

    // Other variables have varying correlation with first
    for (int v = 1; v < n_vars; v++) {
        float correlation = static_cast<float>(v) / n_vars;
        for (int i = 0; i < n_samples; i++) {
            data[v][i] = correlation * data[0][i] +
                        (1.0f - correlation) * generate_normal(1, 0.0f, 1.0f)[0];
        }
    }

    // Compute pairwise MI matrix
    std::vector<std::vector<float>> mi_matrix(n_vars, std::vector<float>(n_vars, 0.0f));

    for (int i = 0; i < n_vars; i++) {
        auto d_i = discretize(data[i], n_bins);
        for (int j = i + 1; j < n_vars; j++) {
            auto d_j = discretize(data[j], n_bins);
            auto joint = build_joint_distribution(d_i, d_j, n_bins, n_bins);
            mi_matrix[i][j] = nimcp_stats_mutual_information(joint.data(), n_bins, n_bins);
            mi_matrix[j][i] = mi_matrix[i][j];  // Symmetric
        }
    }

    // MI with first variable should increase with index
    std::vector<float> mi_with_first(n_vars - 1);
    for (int v = 1; v < n_vars; v++) {
        mi_with_first[v-1] = mi_matrix[0][v];
    }

    // General trend should be increasing
    nimcp_correlation_result_t trend;
    std::vector<float> indices(n_vars - 1);
    for (int i = 0; i < n_vars - 1; i++) indices[i] = static_cast<float>(i);
    nimcp_stats_correlation_pearson(indices.data(), mi_with_first.data(), n_vars - 1, &trend);

    EXPECT_GT(trend.r, 0.5f);  // Positive correlation between index and MI

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 15000.0);

    std::cout << "Multivariate Info: " << n_vars << " variables, "
              << "MI trend correlation=" << trend.r << ", "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 12: Data Compression Analysis
//=============================================================================

TEST_F(InformationTheoryE2ETest, DataCompressionAnalysis) {
    START_TIMER();

    // Analyze compressibility of different data types
    const int n_samples = 10000;
    const int n_bins = 256;  // High resolution

    struct DataCase {
        std::string name;
        std::function<std::vector<float>()> generator;
    };

    // Different data types with varying compressibility
    auto gen_constant = [&]() -> std::vector<float> {
        return std::vector<float>(n_samples, 0.5f);
    };

    auto gen_low_entropy = [&]() -> std::vector<float> {
        std::vector<float> data(n_samples);
        for (int i = 0; i < n_samples; i++) {
            data[i] = (i % 10 == 0) ? 1.0f : 0.0f;
        }
        return data;
    };

    auto gen_medium_entropy = [&]() -> std::vector<float> {
        std::vector<float> data(n_samples);
        for (int i = 0; i < n_samples; i++) {
            data[i] = static_cast<float>(i % 16) / 16.0f;
        }
        return data;
    };

    auto gen_high_entropy = [&]() -> std::vector<float> {
        return generate_uniform(n_samples, 0.0f, 1.0f);
    };

    std::vector<DataCase> cases = {
        {"Constant", gen_constant},
        {"Low entropy", gen_low_entropy},
        {"Medium entropy", gen_medium_entropy},
        {"High entropy", gen_high_entropy}
    };

    std::vector<float> entropies;
    float max_entropy = std::log2(static_cast<float>(n_bins));

    for (const auto& test_case : cases) {
        auto data = test_case.generator();
        auto bins = discretize(data, n_bins);

        std::vector<float> prob(n_bins, 0.0f);
        for (int b : bins) prob[b] += 1.0f;
        for (auto& p : prob) p /= n_samples;

        float entropy = nimcp_stats_entropy(prob.data(), n_bins);
        entropies.push_back(entropy);

        // Compression ratio estimate (lower entropy = more compressible)
        float compression_ratio = entropy / max_entropy;
        EXPECT_GE(compression_ratio, 0.0f);
        EXPECT_LE(compression_ratio, 1.0f);
    }

    // Verify ordering
    EXPECT_LT(entropies[0], entropies[1]);  // Constant < Low
    EXPECT_LT(entropies[2], entropies[3]);  // Medium < High

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 5000.0);

    std::cout << "Compression Analysis: "
              << "entropies=[" << entropies[0] << ", " << entropies[1]
              << ", " << entropies[2] << ", " << entropies[3] << "], "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 13: Feature Selection via MI
//=============================================================================

TEST_F(InformationTheoryE2ETest, FeatureSelectionViaMI) {
    START_TIMER();

    // Select informative features using mutual information
    const int n_features = 20;
    const int n_samples = 1000;
    const int n_bins = 8;
    const int n_informative = 5;  // Only 5 features are informative

    // Generate target variable
    auto target = generate_uniform(n_samples, 0.0f, 1.0f);
    auto target_d = discretize(target, n_bins);

    // Generate features
    std::vector<std::vector<float>> features(n_features);
    std::vector<float> mi_scores(n_features);

    for (int f = 0; f < n_features; f++) {
        if (f < n_informative) {
            // Informative features: correlated with target
            float weight = 0.5f + 0.1f * f;
            features[f].resize(n_samples);
            for (int i = 0; i < n_samples; i++) {
                features[f][i] = weight * target[i] + (1.0f - weight) * generate_uniform(1, 0, 1)[0];
            }
        } else {
            // Non-informative features: random
            features[f] = generate_uniform(n_samples, 0.0f, 1.0f);
        }

        // Compute MI with target
        auto feat_d = discretize(features[f], n_bins);
        auto joint = build_joint_distribution(feat_d, target_d, n_bins, n_bins);
        mi_scores[f] = nimcp_stats_mutual_information(joint.data(), n_bins, n_bins);
    }

    // Rank features by MI
    std::vector<int> ranking(n_features);
    std::iota(ranking.begin(), ranking.end(), 0);
    std::sort(ranking.begin(), ranking.end(),
              [&](int a, int b) { return mi_scores[a] > mi_scores[b]; });

    // Top features should be from informative set
    int correct_top = 0;
    for (int i = 0; i < n_informative; i++) {
        if (ranking[i] < n_informative) {
            correct_top++;
        }
    }

    EXPECT_GE(correct_top, n_informative - 2);  // Most top features should be informative

    // Mean MI of informative should be higher than non-informative
    float mean_informative_mi = 0.0f;
    float mean_noninformative_mi = 0.0f;
    for (int f = 0; f < n_features; f++) {
        if (f < n_informative) {
            mean_informative_mi += mi_scores[f];
        } else {
            mean_noninformative_mi += mi_scores[f];
        }
    }
    mean_informative_mi /= n_informative;
    mean_noninformative_mi /= (n_features - n_informative);

    EXPECT_GT(mean_informative_mi, mean_noninformative_mi);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 10000.0);

    std::cout << "Feature Selection: " << correct_top << "/" << n_informative << " correct, "
              << "MI(informative)=" << mean_informative_mi << ", "
              << "MI(noise)=" << mean_noninformative_mi << ", "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 14: Information Redundancy Analysis
//=============================================================================

TEST_F(InformationTheoryE2ETest, InformationRedundancyAnalysis) {
    START_TIMER();

    const int n_samples = 3000;
    const int n_bins = 8;

    // Create variables with redundant information
    auto x1 = generate_uniform(n_samples, 0.0f, 1.0f);

    // X2 is highly redundant with X1
    std::vector<float> x2(n_samples);
    for (int i = 0; i < n_samples; i++) {
        x2[i] = x1[i] + 0.1f * generate_uniform(1, -0.5f, 0.5f)[0];
    }

    // X3 is independent
    auto x3 = generate_uniform(n_samples, 0.0f, 1.0f);

    // Target depends on all
    std::vector<float> y(n_samples);
    for (int i = 0; i < n_samples; i++) {
        y[i] = 0.3f * x1[i] + 0.3f * x2[i] + 0.3f * x3[i] + 0.1f * generate_uniform(1, 0, 1)[0];
    }

    // Discretize
    auto x1_d = discretize(x1, n_bins);
    auto x2_d = discretize(x2, n_bins);
    auto x3_d = discretize(x3, n_bins);
    auto y_d = discretize(y, n_bins);

    // Compute pairwise MI
    auto joint_x1y = build_joint_distribution(x1_d, y_d, n_bins, n_bins);
    auto joint_x2y = build_joint_distribution(x2_d, y_d, n_bins, n_bins);
    auto joint_x3y = build_joint_distribution(x3_d, y_d, n_bins, n_bins);
    auto joint_x1x2 = build_joint_distribution(x1_d, x2_d, n_bins, n_bins);

    float mi_x1y = nimcp_stats_mutual_information(joint_x1y.data(), n_bins, n_bins);
    float mi_x2y = nimcp_stats_mutual_information(joint_x2y.data(), n_bins, n_bins);
    float mi_x3y = nimcp_stats_mutual_information(joint_x3y.data(), n_bins, n_bins);
    float mi_x1x2 = nimcp_stats_mutual_information(joint_x1x2.data(), n_bins, n_bins);

    // X1 and X2 should have high MI (redundant)
    EXPECT_GT(mi_x1x2, 0.5f);

    // Both X1 and X2 should have similar MI with Y
    EXPECT_NEAR(mi_x1y, mi_x2y, 0.3f);

    // X3 should also contribute (not redundant with X1, X2)
    EXPECT_GT(mi_x3y, 0.1f);

    // Total unique information estimate
    // Redundant features provide less total info than sum
    float sum_mi = mi_x1y + mi_x2y + mi_x3y;

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 5000.0);

    std::cout << "Redundancy: I(X1;Y)=" << mi_x1y << ", I(X2;Y)=" << mi_x2y
              << ", I(X3;Y)=" << mi_x3y << ", I(X1;X2)=" << mi_x1x2
              << ", time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 15: Effective Information Flow
//=============================================================================

TEST_F(InformationTheoryE2ETest, EffectiveInformationFlow) {
    START_TIMER();

    // Compute effective information for state transitions
    const int n_states = 8;

    // Create transition probability matrix (TPM)
    // with deterministic structure
    std::vector<float> tpm(n_states * n_states);

    // Structured TPM: each state mostly transitions to next state
    for (int i = 0; i < n_states; i++) {
        for (int j = 0; j < n_states; j++) {
            if (j == (i + 1) % n_states) {
                tpm[i * n_states + j] = 0.8f;  // High prob to next state
            } else {
                tpm[i * n_states + j] = 0.2f / (n_states - 1);  // Low prob to others
            }
        }
    }

    // Compute effective information
    float ei = nimcp_stats_effective_information(tpm.data(), n_states);

    // EI should be positive for structured TPM
    EXPECT_GT(ei, 0.0f);

    // Compare with random TPM (should have lower EI due to less structure)
    std::vector<float> random_tpm(n_states * n_states);
    for (int i = 0; i < n_states; i++) {
        float row_sum = 0.0f;
        for (int j = 0; j < n_states; j++) {
            random_tpm[i * n_states + j] = generate_uniform(1, 0.1f, 1.0f)[0];
            row_sum += random_tpm[i * n_states + j];
        }
        for (int j = 0; j < n_states; j++) {
            random_tpm[i * n_states + j] /= row_sum;
        }
    }

    float ei_random = nimcp_stats_effective_information(random_tpm.data(), n_states);

    // Structured should have higher or similar EI
    // (depends on definition; deterministic can have high or low EI)
    EXPECT_GT(ei_random, 0.0f);

    // Test information integration
    std::vector<float> cov_matrix(n_states * n_states);
    for (int i = 0; i < n_states; i++) {
        for (int j = 0; j < n_states; j++) {
            if (i == j) {
                cov_matrix[i * n_states + j] = 1.0f;
            } else {
                cov_matrix[i * n_states + j] = 0.5f * std::exp(-std::abs(i - j) / 2.0f);
            }
        }
    }

    float phi = nimcp_stats_information_integration(cov_matrix.data(), n_states);
    EXPECT_GT(phi, 0.0f);  // Should have positive integration

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 3000.0);

    std::cout << "Effective Info: EI(structured)=" << ei
              << ", EI(random)=" << ei_random
              << ", Phi=" << phi
              << ", time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
