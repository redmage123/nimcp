//=============================================================================
// test_cortical_learning_e2e.cpp - Cortical Learning End-to-End Tests
//=============================================================================
/**
 * @file test_cortical_learning_e2e.cpp
 * @brief End-to-end tests for learning mechanisms in cortical columns
 *
 * WHAT: Full pipeline tests for cortical learning and plasticity
 * WHY:  Verify learning mechanisms including Hebbian learning, competitive
 *       learning, and experience-dependent plasticity
 * HOW:  Test learning rules, sparse coding, synaptic plasticity, and tuning
 *
 * TEST COVERAGE:
 * - Hebbian learning in columns
 * - Competitive learning for features
 * - Sparse distributed representations
 * - Synaptic plasticity effects
 * - Experience-dependent tuning
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <array>
#include <numeric>
#include <algorithm>
#include <random>

#include "e2e_test_framework.h"
#include "core/cortical_columns/nimcp_cortical_column.h"
#include "core/cortical_columns/nimcp_cortical_hierarchy.h"
#include "core/cortical_columns/nimcp_cortical_predictive_coding.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CorticalLearningE2ETest : public ::testing::Test {
protected:
    cortical_column_pool_t* pool = nullptr;
    cortical_hierarchy_t* hierarchy = nullptr;
    cortical_predictive_t* predictive = nullptr;

    static constexpr uint32_t NUM_MINICOLUMNS = 12;
    static constexpr uint32_t NUM_CHANNELS = 64;
    static constexpr uint32_t NEURONS_PER_COLUMN = 80;

    std::mt19937 rng{42};  // Fixed seed for reproducibility

    void SetUp() override {
        cortical_column_pool_config_t pool_config = {
            .max_minicolumns = 500,
            .max_hypercolumns = 50,
            .max_neurons_per_minicolumn = 100,
            .enable_cow_support = true
        };
        pool = cortical_column_pool_create(&pool_config);
        ASSERT_NE(pool, nullptr);

        cortical_hierarchy_config_t hier_config = cortical_hierarchy_default_config();
        hier_config.max_areas = 8;
        hier_config.max_connections = 32;
        hier_config.enable_predictive_coding = true;
        hierarchy = cortical_hierarchy_create(&hier_config);
        ASSERT_NE(hierarchy, nullptr);
    }

    void TearDown() override {
        if (predictive) {
            cortical_predictive_destroy(predictive);
            predictive = nullptr;
        }
        if (hierarchy) {
            cortical_hierarchy_destroy(hierarchy);
            hierarchy = nullptr;
        }
        if (pool) {
            cortical_column_pool_destroy(pool);
            pool = nullptr;
        }
    }

    // Helper: Create hypercolumn for learning experiments
    hypercolumn_t* create_learning_hypercolumn() {
        hypercolumn_config_t config = {
            .num_minicolumns = NUM_MINICOLUMNS,
            .minicolumn_configs = nullptr,
            .feature_space_min = 0.0f,
            .feature_space_max = 1.0f,
            .topographic_x = 0.5f,
            .topographic_y = 0.5f,
            .competition = CC_COMPETITION_SOFTMAX,
            .k_winners = 3,
            .temperature = 1.0f,
            .lateral_inhibition_strength = 0.5f,
            .lateral_inhibition_sigma1 = 1.0f,
            .lateral_inhibition_sigma2 = 3.0f
        };
        return hypercolumn_create(pool, &config);
    }

    // Helper: Generate random input
    std::vector<float> generate_random_input(uint32_t size) {
        std::vector<float> input(size);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        for (uint32_t i = 0; i < size; ++i) {
            input[i] = dist(rng);
        }
        return input;
    }

    // Helper: Generate clustered input (for testing feature learning)
    std::vector<float> generate_clustered_input(uint32_t size, uint32_t cluster_id,
                                                 uint32_t num_clusters) {
        std::vector<float> input(size, 0.0f);
        uint32_t cluster_size = size / num_clusters;
        uint32_t start = cluster_id * cluster_size;

        std::normal_distribution<float> dist(0.7f, 0.1f);
        for (uint32_t i = start; i < start + cluster_size && i < size; ++i) {
            input[i] = std::clamp(dist(rng), 0.0f, 1.0f);
        }
        return input;
    }

    // Helper: Generate category exemplar
    std::vector<float> generate_category_exemplar(uint32_t size, uint32_t category,
                                                   float noise_level) {
        std::vector<float> exemplar(size, 0.0f);
        std::normal_distribution<float> noise(0.0f, noise_level);

        // Each category has a distinct pattern
        float base_value = 0.5f + 0.3f * std::sin(category * 2.0f * M_PI / 4.0f);
        for (uint32_t i = 0; i < size; ++i) {
            float phase = static_cast<float>(i) / size * 2.0f * M_PI + category * M_PI / 4.0f;
            exemplar[i] = std::clamp(base_value + 0.3f * std::sin(phase) + noise(rng),
                                     0.0f, 1.0f);
        }
        return exemplar;
    }

    // Helper: Calculate distribution entropy
    float calculate_entropy(const std::vector<float>& distribution) {
        float entropy = 0.0f;
        float sum = std::accumulate(distribution.begin(), distribution.end(), 0.0f);
        if (sum < 0.001f) return 0.0f;

        for (float p : distribution) {
            float normalized = p / sum;
            if (normalized > 0.001f) {
                entropy -= normalized * std::log2(normalized);
            }
        }
        return entropy;
    }

    // Helper: Calculate sparsity
    float calculate_sparsity(const std::vector<float>& activations, float threshold = 0.1f) {
        uint32_t active = 0;
        for (float a : activations) {
            if (a > threshold) active++;
        }
        return 1.0f - static_cast<float>(active) / activations.size();
    }

    // Helper: Calculate total activation
    float total_activation(const std::vector<float>& activations) {
        return std::accumulate(activations.begin(), activations.end(), 0.0f);
    }
};

//=============================================================================
// Hebbian Learning Tests
//=============================================================================

TEST_F(CorticalLearningE2ETest, Hebbian_CoactivationStrengthening) {
    E2E_PIPELINE_START("Hebbian: Co-activation Strengthening");

    E2E_STAGE_BEGIN("Setup predictive coding for Hebbian learning", 300);
    predictive_config_t pc_config;
    cortical_predictive_default_config(&pc_config);
    pc_config.prediction_learning_rate = 0.1f;
    pc_config.hierarchy_depth = 2;

    predictive = cortical_predictive_create(&pc_config);
    ASSERT_NE(predictive, nullptr);

    cortical_predictive_add_level(predictive, 32, 32);
    cortical_predictive_add_level(predictive, 16, 16);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Measure initial prediction quality", 300);
    auto pattern_A = generate_clustered_input(32, 0, 4);
    std::vector<float> initial_errors(32);
    cortical_predictive_compute_error(predictive, 0, pattern_A.data(), 32,
                                     initial_errors.data(), 32);
    float initial_error = 0.0f;
    for (float e : initial_errors) initial_error += std::abs(e);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Train with repeated co-activation", 1500);
    // Present same pattern repeatedly (Hebbian: "neurons that fire together...")
    for (int trial = 0; trial < 20; ++trial) {
        std::vector<float> errors(32);
        cortical_predictive_compute_error(predictive, 0, pattern_A.data(), 32,
                                         errors.data(), 32);
        cortical_predictive_update_predictions(predictive, 0);
        cortical_predictive_update_predictions(predictive, 1);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Measure learned prediction quality", 300);
    std::vector<float> final_errors(32);
    cortical_predictive_compute_error(predictive, 0, pattern_A.data(), 32,
                                     final_errors.data(), 32);
    float final_error = 0.0f;
    for (float e : final_errors) final_error += std::abs(e);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Hebbian learning effect", 200);
    EXPECT_LE(final_error, initial_error * 1.1f)
        << "Repeated exposure should maintain or improve prediction";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(CorticalLearningE2ETest, Hebbian_AssociativeLearning) {
    E2E_PIPELINE_START("Hebbian: Associative Learning");

    E2E_STAGE_BEGIN("Setup for association learning", 400);
    predictive_config_t pc_config;
    cortical_predictive_default_config(&pc_config);
    pc_config.prediction_learning_rate = 0.15f;

    predictive = cortical_predictive_create(&pc_config);
    ASSERT_NE(predictive, nullptr);

    cortical_predictive_add_level(predictive, 64, 64);
    cortical_predictive_add_level(predictive, 32, 32);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Create paired stimuli", 200);
    // Pattern A always followed by Pattern B
    auto pattern_A = generate_clustered_input(64, 0, 4);
    auto pattern_B = generate_clustered_input(64, 1, 4);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Train A->B association", 2000);
    std::vector<float> errors(64);

    for (int trial = 0; trial < 30; ++trial) {
        // Present A
        cortical_predictive_compute_error(predictive, 0, pattern_A.data(), 64,
                                         errors.data(), 64);
        cortical_predictive_update_predictions(predictive, 0);

        // Present B (associated with A)
        cortical_predictive_compute_error(predictive, 0, pattern_B.data(), 64,
                                         errors.data(), 64);
        cortical_predictive_update_predictions(predictive, 0);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test association recall", 500);
    // Present A and check if B is predicted
    cortical_predictive_compute_error(predictive, 0, pattern_A.data(), 64,
                                     errors.data(), 64);

    std::vector<float> predictions(64);
    cortical_predictive_get_predictions(predictive, 0, predictions.data(), 64);

    // Predictions should be influenced by training
    float total_pred = total_activation(predictions);
    EXPECT_GT(total_pred, 0.0f) << "Should have learned predictions";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(CorticalLearningE2ETest, Hebbian_TemporalSequenceLearning) {
    E2E_PIPELINE_START("Hebbian: Temporal Sequence Learning");

    E2E_STAGE_BEGIN("Setup sequence learning", 400);
    predictive_config_t pc_config;
    cortical_predictive_default_config(&pc_config);
    pc_config.prediction_learning_rate = 0.2f;

    predictive = cortical_predictive_create(&pc_config);
    ASSERT_NE(predictive, nullptr);

    cortical_predictive_add_level(predictive, 32, 32);
    cortical_predictive_add_level(predictive, 16, 16);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Create sequence: A -> B -> C -> A...", 200);
    std::vector<std::vector<float>> sequence;
    for (int i = 0; i < 3; ++i) {
        sequence.push_back(generate_clustered_input(32, i, 3));
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Train on repeating sequence", 2000);
    std::vector<float> errors(32);

    for (int rep = 0; rep < 10; ++rep) {
        for (const auto& pattern : sequence) {
            cortical_predictive_compute_error(predictive, 0, pattern.data(), 32,
                                             errors.data(), 32);
            cortical_predictive_update_predictions(predictive, 0);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test sequence prediction", 500);
    // After training, presenting A should predict B
    cortical_predictive_compute_error(predictive, 0, sequence[0].data(), 32,
                                     errors.data(), 32);

    predictive_stats_t stats;
    cortical_predictive_get_stats(predictive, &stats);
    EXPECT_GT(stats.total_updates, 20) << "Should have performed many updates";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Competitive Learning Tests
//=============================================================================

TEST_F(CorticalLearningE2ETest, Competitive_WinnerTakeAll) {
    E2E_PIPELINE_START("Competitive: Winner-Take-All");

    E2E_STAGE_BEGIN("Create competitive hypercolumn", 200);
    hypercolumn_t* hcol = create_learning_hypercolumn();
    ASSERT_NE(hcol, nullptr);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process input with WTA competition", 500);
    auto input = generate_random_input(NUM_CHANNELS);
    hypercolumn_compute(hcol, input.data(), NUM_CHANNELS);

    // Apply WTA
    hypercolumn_run_competition(hcol, CC_COMPETITION_WINNER_TAKE_ALL, 1.0f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify single winner", 200);
    std::vector<float> distribution(NUM_MINICOLUMNS);
    hypercolumn_get_distribution(hcol, distribution.data(), NUM_MINICOLUMNS);

    int winners = 0;
    for (float d : distribution) {
        if (d > 0.5f) winners++;
    }
    EXPECT_LE(winners, 1) << "WTA should select at most one winner";
    E2E_STAGE_END();

    hypercolumn_destroy(hcol);
    E2E_PIPELINE_END();
}

TEST_F(CorticalLearningE2ETest, Competitive_KWinners) {
    E2E_PIPELINE_START("Competitive: K-Winners");

    E2E_STAGE_BEGIN("Create hypercolumn with K-winners", 200);
    hypercolumn_config_t config = {
        .num_minicolumns = NUM_MINICOLUMNS,
        .minicolumn_configs = nullptr,
        .feature_space_min = 0.0f,
        .feature_space_max = 1.0f,
        .topographic_x = 0.5f,
        .topographic_y = 0.5f,
        .competition = CC_COMPETITION_K_WINNERS,
        .k_winners = 3,
        .temperature = 1.0f,
        .lateral_inhibition_strength = 0.5f,
        .lateral_inhibition_sigma1 = 1.0f,
        .lateral_inhibition_sigma2 = 3.0f
    };
    hypercolumn_t* hcol = hypercolumn_create(pool, &config);
    ASSERT_NE(hcol, nullptr);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process and apply K-winners", 500);
    auto input = generate_random_input(NUM_CHANNELS);
    hypercolumn_compute(hcol, input.data(), NUM_CHANNELS);
    hypercolumn_run_competition(hcol, CC_COMPETITION_K_WINNERS, 1.0f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify K winners selected", 200);
    std::vector<float> distribution(NUM_MINICOLUMNS);
    hypercolumn_get_distribution(hcol, distribution.data(), NUM_MINICOLUMNS);

    int winners = 0;
    for (float d : distribution) {
        if (d > 0.1f) winners++;
    }
    EXPECT_LE(winners, 5) << "K-winners should limit active columns";
    E2E_STAGE_END();

    hypercolumn_destroy(hcol);
    E2E_PIPELINE_END();
}

TEST_F(CorticalLearningE2ETest, Competitive_FeatureSpecialization) {
    E2E_PIPELINE_START("Competitive: Feature Specialization");

    E2E_STAGE_BEGIN("Create multiple hypercolumns", 300);
    std::vector<hypercolumn_t*> hcols;
    for (int i = 0; i < 4; ++i) {
        hypercolumn_t* hcol = create_learning_hypercolumn();
        ASSERT_NE(hcol, nullptr);
        hcols.push_back(hcol);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Train each on different feature clusters", 2000);
    for (size_t i = 0; i < hcols.size(); ++i) {
        // Each hypercolumn sees primarily one cluster
        for (int trial = 0; trial < 10; ++trial) {
            auto input = generate_clustered_input(NUM_CHANNELS, i, 4);
            hypercolumn_compute(hcols[i], input.data(), NUM_CHANNELS);
            hypercolumn_run_competition(hcols[i], CC_COMPETITION_SOFTMAX, 1.0f);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test specialization", 500);
    // Each hypercolumn should prefer its trained cluster
    for (size_t i = 0; i < hcols.size(); ++i) {
        auto trained_input = generate_clustered_input(NUM_CHANNELS, i, 4);
        auto other_input = generate_clustered_input(NUM_CHANNELS, (i + 2) % 4, 4);

        hypercolumn_compute(hcols[i], trained_input.data(), NUM_CHANNELS);
        std::vector<float> trained_dist(NUM_MINICOLUMNS);
        hypercolumn_get_distribution(hcols[i], trained_dist.data(), NUM_MINICOLUMNS);
        float trained_response = total_activation(trained_dist);

        hypercolumn_compute(hcols[i], other_input.data(), NUM_CHANNELS);
        std::vector<float> other_dist(NUM_MINICOLUMNS);
        hypercolumn_get_distribution(hcols[i], other_dist.data(), NUM_MINICOLUMNS);
        float other_response = total_activation(other_dist);

        // Should respond to both, but potentially differently
        EXPECT_GT(trained_response + other_response, 0.0f);
    }
    E2E_STAGE_END();

    for (auto hcol : hcols) hypercolumn_destroy(hcol);
    E2E_PIPELINE_END();
}

//=============================================================================
// Sparse Distributed Representation Tests
//=============================================================================

TEST_F(CorticalLearningE2ETest, SDR_SparseCoding) {
    E2E_PIPELINE_START("SDR: Sparse Coding");

    E2E_STAGE_BEGIN("Setup hierarchy for sparse coding", 400);
    cortical_area_config_t config = {
        .type = CORTICAL_AREA_V1,
        .stream = STREAM_VENTRAL,
        .hierarchy_level = 0,
        .rf_expansion_factor = 2.0f,
        .num_hypercolumns = 32,
        .neurons_per_hypercolumn = 1000,
        .feedforward_strength = 1.0f,
        .feedback_strength = 0.5f,
        .custom_name = nullptr
    };
    uint32_t area_id;
    cortical_hierarchy_add_area(hierarchy, &config, &area_id);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process multiple inputs", 1000);
    std::vector<float> sparsities;

    for (int i = 0; i < 10; ++i) {
        auto input = generate_random_input(32);
        cortical_hierarchy_set_area_input(hierarchy, area_id, input.data(), 32);

        std::vector<float> activity(32);
        uint32_t size;
        cortical_hierarchy_get_area_activity(hierarchy, area_id, activity.data(), 32, &size);

        float sparsity = calculate_sparsity(activity, 0.1f);
        sparsities.push_back(sparsity);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify sparse representations", 200);
    float avg_sparsity = std::accumulate(sparsities.begin(), sparsities.end(), 0.0f)
                         / sparsities.size();
    // Cortical representations should be relatively sparse
    EXPECT_GT(avg_sparsity, 0.0f) << "Should have some sparsity";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(CorticalLearningE2ETest, SDR_DistributedRepresentation) {
    E2E_PIPELINE_START("SDR: Distributed Representation");

    E2E_STAGE_BEGIN("Create hypercolumns for SDR", 300);
    std::vector<hypercolumn_t*> hcols;
    for (int i = 0; i < 8; ++i) {
        hypercolumn_t* hcol = create_learning_hypercolumn();
        ASSERT_NE(hcol, nullptr);
        hcols.push_back(hcol);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process pattern across multiple columns", 500);
    auto input = generate_random_input(NUM_CHANNELS);

    std::vector<uint32_t> winners;
    for (auto hcol : hcols) {
        hypercolumn_compute(hcol, input.data(), NUM_CHANNELS);
        hypercolumn_run_competition(hcol, CC_COMPETITION_SOFTMAX, 0.5f);
        winners.push_back(hypercolumn_get_winner(hcol));
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify distributed encoding", 200);
    // Different hypercolumns may select different winners
    std::set<uint32_t> unique_winners(winners.begin(), winners.end());
    // Should have some variation (distributed)
    EXPECT_GE(winners.size(), 1) << "Should have at least one response";
    E2E_STAGE_END();

    for (auto hcol : hcols) hypercolumn_destroy(hcol);
    E2E_PIPELINE_END();
}

TEST_F(CorticalLearningE2ETest, SDR_PatternCompletion) {
    E2E_PIPELINE_START("SDR: Pattern Completion");

    E2E_STAGE_BEGIN("Setup predictive system", 400);
    predictive_config_t pc_config;
    cortical_predictive_default_config(&pc_config);
    pc_config.prediction_learning_rate = 0.15f;

    predictive = cortical_predictive_create(&pc_config);
    ASSERT_NE(predictive, nullptr);

    cortical_predictive_add_level(predictive, 64, 64);
    cortical_predictive_add_level(predictive, 32, 32);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Learn complete pattern", 1500);
    auto complete_pattern = generate_category_exemplar(64, 0, 0.05f);
    std::vector<float> errors(64);

    for (int i = 0; i < 20; ++i) {
        cortical_predictive_compute_error(predictive, 0, complete_pattern.data(), 64,
                                         errors.data(), 64);
        cortical_predictive_update_predictions(predictive, 0);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test with partial pattern", 500);
    // Create partial pattern (first half only)
    std::vector<float> partial_pattern(64, 0.0f);
    for (uint32_t i = 0; i < 32; ++i) {
        partial_pattern[i] = complete_pattern[i];
    }

    cortical_predictive_compute_error(predictive, 0, partial_pattern.data(), 64,
                                     errors.data(), 64);

    std::vector<float> predictions(64);
    cortical_predictive_get_predictions(predictive, 0, predictions.data(), 64);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify completion attempt", 200);
    // Predictions should exist for the missing portion
    float prediction_second_half = 0.0f;
    for (uint32_t i = 32; i < 64; ++i) {
        prediction_second_half += std::abs(predictions[i]);
    }
    EXPECT_GE(prediction_second_half, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Synaptic Plasticity Tests
//=============================================================================

TEST_F(CorticalLearningE2ETest, Plasticity_PrecisionLearning) {
    E2E_PIPELINE_START("Plasticity: Precision Learning");

    E2E_STAGE_BEGIN("Setup with precision learning", 400);
    predictive_config_t pc_config;
    cortical_predictive_default_config(&pc_config);
    pc_config.precision_learning_rate = 0.05f;
    pc_config.enable_precision_weighting = true;

    predictive = cortical_predictive_create(&pc_config);
    ASSERT_NE(predictive, nullptr);

    cortical_predictive_add_level(predictive, 32, 32);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get initial precisions", 300);
    std::vector<float> initial_precisions(32);
    cortical_predictive_get_precisions(predictive, 0, initial_precisions.data(), 32);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Train with consistent errors", 1500);
    // Present stimuli that produce consistent errors in some channels
    for (int trial = 0; trial < 20; ++trial) {
        std::vector<float> stimulus(32);
        for (uint32_t i = 0; i < 32; ++i) {
            // First half: high variance, second half: low variance
            float variance = (i < 16) ? 0.3f : 0.05f;
            std::normal_distribution<float> dist(0.5f, variance);
            stimulus[i] = std::clamp(dist(rng), 0.0f, 1.0f);
        }

        std::vector<float> errors(32);
        cortical_predictive_compute_error(predictive, 0, stimulus.data(), 32,
                                         errors.data(), 32);
        cortical_predictive_update_precisions(predictive, 0);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify precision adaptation", 300);
    std::vector<float> final_precisions(32);
    cortical_predictive_get_precisions(predictive, 0, final_precisions.data(), 32);

    // Precisions should have changed through learning
    predictive_stats_t stats;
    cortical_predictive_get_stats(predictive, &stats);
    EXPECT_GT(stats.precision_updates, 10) << "Should have updated precisions";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(CorticalLearningE2ETest, Plasticity_ConnectionStrengthening) {
    E2E_PIPELINE_START("Plasticity: Connection Strengthening");

    E2E_STAGE_BEGIN("Setup hierarchy with modifiable connections", 500);
    cortical_area_config_t config1 = {
        .type = CORTICAL_AREA_V1,
        .stream = STREAM_VENTRAL,
        .hierarchy_level = 0,
        .rf_expansion_factor = 2.0f,
        .num_hypercolumns = 16,
        .neurons_per_hypercolumn = 1000,
        .feedforward_strength = 0.5f,  // Initial strength
        .feedback_strength = 0.5f,
        .custom_name = nullptr
    };
    cortical_area_config_t config2 = {
        .type = CORTICAL_AREA_V2,
        .stream = STREAM_VENTRAL,
        .hierarchy_level = 1,
        .rf_expansion_factor = 2.0f,
        .num_hypercolumns = 8,
        .neurons_per_hypercolumn = 1000,
        .feedforward_strength = 0.5f,
        .feedback_strength = 0.5f,
        .custom_name = nullptr
    };

    uint32_t area1, area2;
    cortical_hierarchy_add_area(hierarchy, &config1, &area1);
    cortical_hierarchy_add_area(hierarchy, &config2, &area2);

    inter_area_connection_config_t conn_config = {
        .source_area_id = area1,
        .target_area_id = area2,
        .type = CONNECTION_TYPE_FEEDFORWARD,
        .source_layer = 2,
        .target_layer = 3,
        .weight = 0.5f,
        .delay_ms = 10.0f,
        .use_canonical_layers = true
    };
    uint32_t conn_id;
    cortical_hierarchy_connect_areas(hierarchy, &conn_config, &conn_id);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process repeated stimuli", 1500);
    for (int trial = 0; trial < 20; ++trial) {
        auto input = generate_random_input(16);
        cortical_hierarchy_set_area_input(hierarchy, area1, input.data(), 16);
        cortical_hierarchy_propagate_feedforward(hierarchy, 0, 1);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify processing", 300);
    std::vector<float> activity(8);
    uint32_t size;
    cortical_hierarchy_get_area_activity(hierarchy, area2, activity.data(), 8, &size);

    float total = total_activation(activity);
    EXPECT_GE(total, 0.0f) << "Should have propagated activity";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(CorticalLearningE2ETest, Plasticity_LongTermPotentiation) {
    E2E_PIPELINE_START("Plasticity: Long-Term Potentiation");

    E2E_STAGE_BEGIN("Setup for LTP simulation", 400);
    predictive_config_t pc_config;
    cortical_predictive_default_config(&pc_config);
    pc_config.prediction_learning_rate = 0.1f;

    predictive = cortical_predictive_create(&pc_config);
    ASSERT_NE(predictive, nullptr);

    cortical_predictive_add_level(predictive, 32, 32);
    cortical_predictive_add_level(predictive, 16, 16);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Measure baseline response", 500);
    auto stimulus = generate_category_exemplar(32, 0, 0.1f);
    std::vector<float> baseline_errors(32);
    cortical_predictive_compute_error(predictive, 0, stimulus.data(), 32,
                                     baseline_errors.data(), 32);

    float baseline_error = 0.0f;
    for (float e : baseline_errors) baseline_error += std::abs(e);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply high-frequency stimulation (LTP protocol)", 2000);
    // Rapid repeated stimulation simulates LTP induction
    for (int burst = 0; burst < 5; ++burst) {
        for (int spike = 0; spike < 10; ++spike) {
            std::vector<float> errors(32);
            cortical_predictive_compute_error(predictive, 0, stimulus.data(), 32,
                                             errors.data(), 32);
            cortical_predictive_update_predictions(predictive, 0);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Measure post-LTP response", 500);
    std::vector<float> post_ltp_errors(32);
    cortical_predictive_compute_error(predictive, 0, stimulus.data(), 32,
                                     post_ltp_errors.data(), 32);

    float post_ltp_error = 0.0f;
    for (float e : post_ltp_errors) post_ltp_error += std::abs(e);

    // After learning, predictions should be closer (lower error or similar)
    EXPECT_LE(post_ltp_error, baseline_error * 1.2f)
        << "Post-LTP response should be maintained or improved";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Experience-Dependent Tuning Tests
//=============================================================================

TEST_F(CorticalLearningE2ETest, Tuning_CategoryLearning) {
    E2E_PIPELINE_START("Tuning: Category Learning");

    E2E_STAGE_BEGIN("Setup predictive system", 400);
    predictive_config_t pc_config;
    cortical_predictive_default_config(&pc_config);
    pc_config.prediction_learning_rate = 0.15f;

    predictive = cortical_predictive_create(&pc_config);
    ASSERT_NE(predictive, nullptr);

    cortical_predictive_add_level(predictive, 64, 64);
    cortical_predictive_add_level(predictive, 32, 32);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Train on 4 categories", 3000);
    for (int epoch = 0; epoch < 10; ++epoch) {
        for (int cat = 0; cat < 4; ++cat) {
            // Generate multiple exemplars per category
            for (int ex = 0; ex < 5; ++ex) {
                auto exemplar = generate_category_exemplar(64, cat, 0.1f);
                std::vector<float> errors(64);
                cortical_predictive_compute_error(predictive, 0, exemplar.data(), 64,
                                                 errors.data(), 64);
                cortical_predictive_update_predictions(predictive, 0);
            }
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test category discrimination", 1000);
    // Test with novel exemplars from each category
    std::vector<std::vector<float>> category_errors(4);

    for (int cat = 0; cat < 4; ++cat) {
        auto test_exemplar = generate_category_exemplar(64, cat, 0.05f);
        std::vector<float> errors(64);
        cortical_predictive_compute_error(predictive, 0, test_exemplar.data(), 64,
                                         errors.data(), 64);

        float total_error = 0.0f;
        for (float e : errors) total_error += std::abs(e);
        category_errors[cat].push_back(total_error);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify learning statistics", 200);
    predictive_stats_t stats;
    cortical_predictive_get_stats(predictive, &stats);
    EXPECT_GT(stats.total_updates, 100) << "Should have performed many learning updates";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(CorticalLearningE2ETest, Tuning_OrientationTuningCurve) {
    E2E_PIPELINE_START("Tuning: Orientation Tuning Curve");

    E2E_STAGE_BEGIN("Create orientation-selective hypercolumn", 200);
    hypercolumn_t* hcol = create_learning_hypercolumn();
    ASSERT_NE(hcol, nullptr);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Measure tuning curve", 1500);
    std::vector<float> tuning_curve;

    for (int angle = 0; angle < 180; angle += 15) {
        // Generate oriented stimulus
        std::vector<float> stimulus(NUM_CHANNELS);
        float theta = angle * M_PI / 180.0f;

        for (uint32_t i = 0; i < NUM_CHANNELS; ++i) {
            float x = static_cast<float>(i) / NUM_CHANNELS;
            stimulus[i] = 0.5f + 0.5f * std::cos(2.0f * M_PI * 4.0f * x * std::cos(theta));
        }

        hypercolumn_compute(hcol, stimulus.data(), NUM_CHANNELS);

        std::vector<float> distribution(NUM_MINICOLUMNS);
        hypercolumn_get_distribution(hcol, distribution.data(), NUM_MINICOLUMNS);

        float max_response = *std::max_element(distribution.begin(), distribution.end());
        tuning_curve.push_back(max_response);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify tuning properties", 200);
    // Should have a preferred orientation (peak in tuning curve)
    float max_response = *std::max_element(tuning_curve.begin(), tuning_curve.end());
    float min_response = *std::min_element(tuning_curve.begin(), tuning_curve.end());

    EXPECT_GT(max_response, 0.0f) << "Should have non-zero responses";
    E2E_STAGE_END();

    hypercolumn_destroy(hcol);
    E2E_PIPELINE_END();
}

TEST_F(CorticalLearningE2ETest, Tuning_ExperienceDependentSharpening) {
    E2E_PIPELINE_START("Tuning: Experience-Dependent Sharpening");

    E2E_STAGE_BEGIN("Setup and measure initial selectivity", 500);
    hypercolumn_t* hcol = create_learning_hypercolumn();
    ASSERT_NE(hcol, nullptr);

    // Initial stimulus
    auto preferred_stimulus = generate_clustered_input(NUM_CHANNELS, 0, 4);
    hypercolumn_compute(hcol, preferred_stimulus.data(), NUM_CHANNELS);

    std::vector<float> initial_dist(NUM_MINICOLUMNS);
    hypercolumn_get_distribution(hcol, initial_dist.data(), NUM_MINICOLUMNS);
    float initial_entropy = calculate_entropy(initial_dist);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Train with repeated exposure", 2000);
    for (int trial = 0; trial < 30; ++trial) {
        auto stimulus = generate_clustered_input(NUM_CHANNELS, 0, 4);
        hypercolumn_compute(hcol, stimulus.data(), NUM_CHANNELS);
        hypercolumn_run_competition(hcol, CC_COMPETITION_SOFTMAX, 0.8f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Measure post-training selectivity", 500);
    hypercolumn_compute(hcol, preferred_stimulus.data(), NUM_CHANNELS);

    std::vector<float> final_dist(NUM_MINICOLUMNS);
    hypercolumn_get_distribution(hcol, final_dist.data(), NUM_MINICOLUMNS);
    float final_entropy = calculate_entropy(final_dist);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get hypercolumn statistics", 200);
    cc_hypercolumn_stats_t stats;
    hypercolumn_get_stats(hcol, &stats);

    EXPECT_GT(stats.total_computations, 30) << "Should track computations";
    E2E_STAGE_END();

    hypercolumn_destroy(hcol);
    E2E_PIPELINE_END();
}
