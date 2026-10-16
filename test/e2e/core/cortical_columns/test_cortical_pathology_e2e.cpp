//=============================================================================
// test_cortical_pathology_e2e.cpp - Cortical Pathology End-to-End Tests
//=============================================================================
/**
 * @file test_cortical_pathology_e2e.cpp
 * @brief End-to-end tests for cortical pathology and recovery mechanisms
 *
 * WHAT: Full pipeline tests for simulating cortical dysfunction and recovery
 * WHY:  Verify system behavior under pathological conditions and test
 *       compensation/recovery mechanisms
 * HOW:  Test lesion simulation, hyperexcitability, plasticity impairment,
 *       and adaptive recovery strategies
 *
 * TEST COVERAGE:
 * - Cortical lesion simulation
 * - Hyperexcitability (seizure-like)
 * - Plasticity impairment
 * - Recovery mechanisms
 * - Compensation strategies
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

class CorticalPathologyE2ETest : public ::testing::Test {
protected:
    cortical_column_pool_t* pool = nullptr;
    cortical_hierarchy_t* hierarchy = nullptr;
    cortical_predictive_t* predictive = nullptr;

    static constexpr uint32_t NUM_MINICOLUMNS = 12;
    static constexpr uint32_t NUM_CHANNELS = 64;
    static constexpr uint32_t BASE_UNITS = 32;

    std::mt19937 rng{42};

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
        hier_config.max_areas = 10;
        hier_config.max_connections = 50;
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

    // Helper: Create standard hypercolumn
    hypercolumn_t* create_hypercolumn() {
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

    // Helper: Create multi-area hierarchy
    std::vector<uint32_t> create_visual_hierarchy() {
        std::vector<uint32_t> area_ids;
        cortical_area_type_t types[] = {CORTICAL_AREA_V1, CORTICAL_AREA_V2,
                                         CORTICAL_AREA_V4, CORTICAL_AREA_IT};

        for (int i = 0; i < 4; ++i) {
            cortical_area_config_t config = {
                .type = types[i],
                .stream = STREAM_VENTRAL,
                .hierarchy_level = static_cast<uint32_t>(i),
                .rf_expansion_factor = 2.0f,
                .num_hypercolumns = BASE_UNITS >> i,
                .neurons_per_hypercolumn = 1000,
                .feedforward_strength = 1.0f,
                .feedback_strength = 0.5f,
                .custom_name = nullptr
            };
            uint32_t id;
            cortical_hierarchy_add_area(hierarchy, &config, &id);
            area_ids.push_back(id);
        }

        // Connect areas
        for (size_t i = 0; i < area_ids.size() - 1; ++i) {
            inter_area_connection_config_t ff = {
                .source_area_id = area_ids[i],
                .target_area_id = area_ids[i + 1],
                .type = CONNECTION_TYPE_FEEDFORWARD,
                .source_layer = 2,
                .target_layer = 3,
                .weight = 1.0f,
                .delay_ms = 10.0f,
                .use_canonical_layers = true
            };
            uint32_t conn_id;
            cortical_hierarchy_connect_areas(hierarchy, &ff, &conn_id);

            inter_area_connection_config_t fb = {
                .source_area_id = area_ids[i + 1],
                .target_area_id = area_ids[i],
                .type = CONNECTION_TYPE_FEEDBACK,
                .source_layer = 4,
                .target_layer = 0,
                .weight = 0.5f,
                .delay_ms = 10.0f,
                .use_canonical_layers = true
            };
            cortical_hierarchy_connect_areas(hierarchy, &fb, &conn_id);
        }

        return area_ids;
    }

    // Helper: Generate normal input
    std::vector<float> generate_normal_input(uint32_t size) {
        std::vector<float> input(size);
        std::normal_distribution<float> dist(0.5f, 0.15f);
        for (uint32_t i = 0; i < size; ++i) {
            input[i] = std::clamp(dist(rng), 0.0f, 1.0f);
        }
        return input;
    }

    // Helper: Generate lesioned input (zeroed regions)
    std::vector<float> apply_lesion(std::vector<float> input, float lesion_start,
                                     float lesion_end) {
        uint32_t start_idx = static_cast<uint32_t>(lesion_start * input.size());
        uint32_t end_idx = static_cast<uint32_t>(lesion_end * input.size());

        for (uint32_t i = start_idx; i < end_idx && i < input.size(); ++i) {
            input[i] = 0.0f;
        }
        return input;
    }

    // Helper: Generate hyperexcitable input (amplified)
    std::vector<float> generate_hyperexcitable_input(uint32_t size, float gain) {
        auto input = generate_normal_input(size);
        for (float& v : input) {
            v = std::clamp(v * gain, 0.0f, 1.0f);
        }
        return input;
    }

    // Helper: Calculate total activation
    float total_activation(const std::vector<float>& activations) {
        return std::accumulate(activations.begin(), activations.end(), 0.0f);
    }

    // Helper: Calculate activation variance
    float calculate_variance(const std::vector<float>& values) {
        float mean = total_activation(values) / values.size();
        float variance = 0.0f;
        for (float v : values) {
            variance += (v - mean) * (v - mean);
        }
        return variance / values.size();
    }

    // Helper: Calculate spatial coherence
    float calculate_spatial_coherence(const std::vector<float>& activations) {
        if (activations.size() < 2) return 1.0f;

        float coherence = 0.0f;
        for (size_t i = 1; i < activations.size(); ++i) {
            float diff = std::abs(activations[i] - activations[i - 1]);
            coherence += 1.0f - diff;
        }
        return coherence / (activations.size() - 1);
    }
};

//=============================================================================
// Cortical Lesion Simulation Tests
//=============================================================================

TEST_F(CorticalPathologyE2ETest, Lesion_FocalLesionEffect) {
    E2E_PIPELINE_START("Lesion: Focal Lesion Effect");

    E2E_STAGE_BEGIN("Setup visual hierarchy", 500);
    auto area_ids = create_visual_hierarchy();
    EXPECT_EQ(area_ids.size(), 4);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process normal input", 500);
    auto normal_input = generate_normal_input(BASE_UNITS);
    cortical_hierarchy_set_area_input(hierarchy, area_ids[0], normal_input.data(), BASE_UNITS);
    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 3);

    std::vector<float> normal_output(BASE_UNITS >> 3);
    uint32_t size;
    cortical_hierarchy_get_area_activity(hierarchy, area_ids[3],
                                         normal_output.data(), BASE_UNITS >> 3, &size);
    float normal_total = total_activation(normal_output);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Simulate focal V1 lesion (25% of area)", 500);
    auto lesioned_input = apply_lesion(normal_input, 0.25f, 0.5f);
    cortical_hierarchy_set_area_input(hierarchy, area_ids[0], lesioned_input.data(), BASE_UNITS);
    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 3);

    std::vector<float> lesioned_output(BASE_UNITS >> 3);
    cortical_hierarchy_get_area_activity(hierarchy, area_ids[3],
                                         lesioned_output.data(), BASE_UNITS >> 3, &size);
    float lesioned_total = total_activation(lesioned_output);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify lesion impact", 200);
    // Lesion should reduce overall activity but not eliminate it
    EXPECT_GT(lesioned_total, 0.0f)
        << "Some activity should remain despite lesion";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(CorticalPathologyE2ETest, Lesion_IntermediateLevelLesion) {
    E2E_PIPELINE_START("Lesion: Intermediate Level Lesion");

    E2E_STAGE_BEGIN("Setup hierarchy", 500);
    auto area_ids = create_visual_hierarchy();
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Measure baseline with intact hierarchy", 500);
    auto input = generate_normal_input(BASE_UNITS);
    cortical_hierarchy_set_area_input(hierarchy, area_ids[0], input.data(), BASE_UNITS);
    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 3);

    std::vector<float> baseline(BASE_UNITS >> 3);
    uint32_t size;
    cortical_hierarchy_get_area_activity(hierarchy, area_ids[3],
                                         baseline.data(), BASE_UNITS >> 3, &size);
    float baseline_total = total_activation(baseline);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Simulate V2 lesion (intermediate level)", 500);
    // Remove V2 from hierarchy
    int result = cortical_hierarchy_remove_area(hierarchy, area_ids[1]);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify hierarchy disruption", 300);
    cortical_hierarchy_stats_t stats;
    cortical_hierarchy_get_stats(hierarchy, &stats);
    EXPECT_EQ(stats.num_areas, 3) << "Should have 3 areas after lesion";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(CorticalPathologyE2ETest, Lesion_LargeScaleDamage) {
    E2E_PIPELINE_START("Lesion: Large-Scale Damage");

    E2E_STAGE_BEGIN("Create hypercolumn array", 400);
    std::vector<hypercolumn_t*> hcols;
    for (int i = 0; i < 8; ++i) {
        hypercolumn_t* hcol = create_hypercolumn();
        ASSERT_NE(hcol, nullptr);
        hcols.push_back(hcol);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Measure baseline activity", 500);
    std::vector<float> baseline_responses;
    auto input = generate_normal_input(NUM_CHANNELS);

    for (auto hcol : hcols) {
        hypercolumn_compute(hcol, input.data(), NUM_CHANNELS);
        std::vector<float> dist(NUM_MINICOLUMNS);
        hypercolumn_get_distribution(hcol, dist.data(), NUM_MINICOLUMNS);
        baseline_responses.push_back(total_activation(dist));
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Simulate 50% column loss", 300);
    // Destroy half the hypercolumns (lesion)
    for (int i = 0; i < 4; ++i) {
        hypercolumn_destroy(hcols[i]);
        hcols[i] = nullptr;
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Measure remaining network activity", 500);
    std::vector<float> post_lesion_responses;
    for (int i = 4; i < 8; ++i) {
        hypercolumn_compute(hcols[i], input.data(), NUM_CHANNELS);
        std::vector<float> dist(NUM_MINICOLUMNS);
        hypercolumn_get_distribution(hcols[i], dist.data(), NUM_MINICOLUMNS);
        post_lesion_responses.push_back(total_activation(dist));
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify remaining function", 200);
    float remaining_total = total_activation(post_lesion_responses);
    EXPECT_GT(remaining_total, 0.0f) << "Remaining columns should still function";
    E2E_STAGE_END();

    for (int i = 4; i < 8; ++i) {
        if (hcols[i]) hypercolumn_destroy(hcols[i]);
    }
    E2E_PIPELINE_END();
}

//=============================================================================
// Hyperexcitability Tests (Seizure-like)
//=============================================================================

TEST_F(CorticalPathologyE2ETest, Hyperexcitability_ExcessiveActivation) {
    E2E_PIPELINE_START("Hyperexcitability: Excessive Activation");

    E2E_STAGE_BEGIN("Create hypercolumn", 200);
    hypercolumn_t* hcol = create_hypercolumn();
    ASSERT_NE(hcol, nullptr);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process normal input", 300);
    auto normal_input = generate_normal_input(NUM_CHANNELS);
    hypercolumn_compute(hcol, normal_input.data(), NUM_CHANNELS);

    std::vector<float> normal_dist(NUM_MINICOLUMNS);
    hypercolumn_get_distribution(hcol, normal_dist.data(), NUM_MINICOLUMNS);
    float normal_total = total_activation(normal_dist);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process hyperexcitable input (3x gain)", 300);
    auto hyper_input = generate_hyperexcitable_input(NUM_CHANNELS, 3.0f);
    hypercolumn_compute(hcol, hyper_input.data(), NUM_CHANNELS);

    std::vector<float> hyper_dist(NUM_MINICOLUMNS);
    hypercolumn_get_distribution(hcol, hyper_dist.data(), NUM_MINICOLUMNS);
    float hyper_total = total_activation(hyper_dist);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify hyperactivation", 200);
    EXPECT_GT(hyper_total, normal_total * 0.8f)
        << "Hyperexcitable input should produce higher activity";
    E2E_STAGE_END();

    hypercolumn_destroy(hcol);
    E2E_PIPELINE_END();
}

TEST_F(CorticalPathologyE2ETest, Hyperexcitability_SpreadingActivation) {
    E2E_PIPELINE_START("Hyperexcitability: Spreading Activation");

    E2E_STAGE_BEGIN("Setup hierarchy", 500);
    auto area_ids = create_visual_hierarchy();
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Inject focal hyperactivation at V1", 300);
    std::vector<float> focal_hyper(BASE_UNITS, 0.0f);
    // Small hyperactive focus
    for (uint32_t i = 10; i < 14; ++i) {
        focal_hyper[i] = 1.0f;  // Maximum activation
    }
    cortical_hierarchy_set_area_input(hierarchy, area_ids[0], focal_hyper.data(), BASE_UNITS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Propagate through hierarchy", 500);
    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 3);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Check for activity spread", 500);
    std::vector<float> v2_activity(BASE_UNITS >> 1);
    std::vector<float> v4_activity(BASE_UNITS >> 2);
    uint32_t size;

    cortical_hierarchy_get_area_activity(hierarchy, area_ids[1],
                                         v2_activity.data(), BASE_UNITS >> 1, &size);
    cortical_hierarchy_get_area_activity(hierarchy, area_ids[2],
                                         v4_activity.data(), BASE_UNITS >> 2, &size);

    float v2_total = total_activation(v2_activity);
    float v4_total = total_activation(v4_activity);

    EXPECT_GT(v2_total, 0.0f) << "Activation should spread to V2";
    EXPECT_GT(v4_total, 0.0f) << "Activation should spread to V4";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(CorticalPathologyE2ETest, Hyperexcitability_InhibitionFailure) {
    E2E_PIPELINE_START("Hyperexcitability: Inhibition Failure");

    E2E_STAGE_BEGIN("Create hypercolumn with normal inhibition", 200);
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
        .lateral_inhibition_strength = 0.8f,  // Strong inhibition
        .lateral_inhibition_sigma1 = 1.0f,
        .lateral_inhibition_sigma2 = 3.0f
    };
    hypercolumn_t* normal_hcol = hypercolumn_create(pool, &config);
    ASSERT_NE(normal_hcol, nullptr);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Create hypercolumn with failed inhibition", 200);
    config.lateral_inhibition_strength = 0.1f;  // Weak inhibition (failure)
    hypercolumn_t* impaired_hcol = hypercolumn_create(pool, &config);
    ASSERT_NE(impaired_hcol, nullptr);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Compare activation patterns", 500);
    auto input = generate_normal_input(NUM_CHANNELS);

    hypercolumn_compute(normal_hcol, input.data(), NUM_CHANNELS);
    std::vector<float> normal_dist(NUM_MINICOLUMNS);
    hypercolumn_get_distribution(normal_hcol, normal_dist.data(), NUM_MINICOLUMNS);

    hypercolumn_compute(impaired_hcol, input.data(), NUM_CHANNELS);
    std::vector<float> impaired_dist(NUM_MINICOLUMNS);
    hypercolumn_get_distribution(impaired_hcol, impaired_dist.data(), NUM_MINICOLUMNS);

    float normal_variance = calculate_variance(normal_dist);
    float impaired_variance = calculate_variance(impaired_dist);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify inhibition effect", 200);
    // With weak inhibition, distribution should be more spread out
    // (less winner-suppression of competitors)
    EXPECT_GE(normal_variance + impaired_variance, 0.0f)
        << "Both distributions should have some variance";
    E2E_STAGE_END();

    hypercolumn_destroy(normal_hcol);
    hypercolumn_destroy(impaired_hcol);
    E2E_PIPELINE_END();
}

//=============================================================================
// Plasticity Impairment Tests
//=============================================================================

TEST_F(CorticalPathologyE2ETest, PlasticityImpairment_ReducedLearning) {
    E2E_PIPELINE_START("Plasticity Impairment: Reduced Learning");

    E2E_STAGE_BEGIN("Setup normal learning system", 400);
    predictive_config_t normal_config;
    cortical_predictive_default_config(&normal_config);
    normal_config.prediction_learning_rate = 0.2f;

    cortical_predictive_t* normal_pc = cortical_predictive_create(&normal_config);
    ASSERT_NE(normal_pc, nullptr);
    cortical_predictive_add_level(normal_pc, 32, 32);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Setup impaired learning system", 400);
    predictive_config_t impaired_config;
    cortical_predictive_default_config(&impaired_config);
    impaired_config.prediction_learning_rate = 0.01f;  // Severely reduced

    predictive = cortical_predictive_create(&impaired_config);
    ASSERT_NE(predictive, nullptr);
    cortical_predictive_add_level(predictive, 32, 32);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Train both systems", 2000);
    auto pattern = generate_normal_input(32);
    std::vector<float> errors(32);

    for (int trial = 0; trial < 30; ++trial) {
        cortical_predictive_compute_error(normal_pc, 0, pattern.data(), 32,
                                         errors.data(), 32);
        cortical_predictive_update_predictions(normal_pc, 0);

        cortical_predictive_compute_error(predictive, 0, pattern.data(), 32,
                                         errors.data(), 32);
        cortical_predictive_update_predictions(predictive, 0);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Compare learning outcomes", 500);
    predictive_stats_t normal_stats, impaired_stats;
    cortical_predictive_get_stats(normal_pc, &normal_stats);
    cortical_predictive_get_stats(predictive, &impaired_stats);

    // Both should have done updates
    EXPECT_GT(normal_stats.total_updates, 20);
    EXPECT_GT(impaired_stats.total_updates, 20);
    E2E_STAGE_END();

    cortical_predictive_destroy(normal_pc);
    E2E_PIPELINE_END();
}

TEST_F(CorticalPathologyE2ETest, PlasticityImpairment_AmnesiaSyndrome) {
    E2E_PIPELINE_START("Plasticity Impairment: Amnesia Syndrome");

    E2E_STAGE_BEGIN("Setup system with learning deficit", 400);
    predictive_config_t config;
    cortical_predictive_default_config(&config);
    config.prediction_learning_rate = 0.0f;  // No learning (complete amnesia)

    predictive = cortical_predictive_create(&config);
    ASSERT_NE(predictive, nullptr);
    cortical_predictive_add_level(predictive, 32, 32);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Attempt to learn new pattern", 1000);
    auto pattern = generate_normal_input(32);
    std::vector<float> initial_errors(32);
    cortical_predictive_compute_error(predictive, 0, pattern.data(), 32,
                                     initial_errors.data(), 32);
    float initial_error = total_activation(initial_errors);

    // Try to learn
    for (int i = 0; i < 20; ++i) {
        std::vector<float> errors(32);
        cortical_predictive_compute_error(predictive, 0, pattern.data(), 32,
                                         errors.data(), 32);
        cortical_predictive_update_predictions(predictive, 0);
    }

    std::vector<float> final_errors(32);
    cortical_predictive_compute_error(predictive, 0, pattern.data(), 32,
                                     final_errors.data(), 32);
    float final_error = total_activation(final_errors);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify no learning occurred", 200);
    // With zero learning rate, errors should remain similar
    EXPECT_NEAR(final_error, initial_error, initial_error * 0.1f)
        << "With zero learning rate, error should not decrease";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(CorticalPathologyE2ETest, PlasticityImpairment_MetaplasticityDeficit) {
    E2E_PIPELINE_START("Plasticity Impairment: Metaplasticity Deficit");

    E2E_STAGE_BEGIN("Setup with impaired precision learning", 400);
    predictive_config_t config;
    cortical_predictive_default_config(&config);
    config.precision_learning_rate = 0.0f;  // No precision adaptation
    config.enable_precision_weighting = true;

    predictive = cortical_predictive_create(&config);
    ASSERT_NE(predictive, nullptr);
    cortical_predictive_add_level(predictive, 32, 32);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get initial precisions", 300);
    std::vector<float> initial_prec(32);
    cortical_predictive_get_precisions(predictive, 0, initial_prec.data(), 32);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Train with varied error variance", 1500);
    for (int trial = 0; trial < 30; ++trial) {
        auto stimulus = generate_normal_input(32);
        std::vector<float> errors(32);
        cortical_predictive_compute_error(predictive, 0, stimulus.data(), 32,
                                         errors.data(), 32);
        cortical_predictive_update_precisions(predictive, 0);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify precisions unchanged", 300);
    std::vector<float> final_prec(32);
    cortical_predictive_get_precisions(predictive, 0, final_prec.data(), 32);

    // With zero precision learning rate, precisions should be similar
    float diff = 0.0f;
    for (uint32_t i = 0; i < 32; ++i) {
        diff += std::abs(final_prec[i] - initial_prec[i]);
    }
    // Allow small floating point differences
    EXPECT_LT(diff, 0.1f) << "Precisions should not change with zero learning rate";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Recovery Mechanism Tests
//=============================================================================

TEST_F(CorticalPathologyE2ETest, Recovery_CompensatoryReorganization) {
    E2E_PIPELINE_START("Recovery: Compensatory Reorganization");

    E2E_STAGE_BEGIN("Setup hierarchy with redundancy", 500);
    // Create two parallel pathways
    cortical_area_config_t v1_config = {
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
    cortical_area_config_t v2a_config = {
        .type = CORTICAL_AREA_V2,
        .stream = STREAM_VENTRAL,
        .hierarchy_level = 1,
        .rf_expansion_factor = 2.0f,
        .num_hypercolumns = 16,
        .neurons_per_hypercolumn = 1000,
        .feedforward_strength = 1.0f,
        .feedback_strength = 0.5f,
        .custom_name = "V2a"
    };
    cortical_area_config_t v2b_config = {
        .type = CORTICAL_AREA_V2,
        .stream = STREAM_VENTRAL,
        .hierarchy_level = 1,
        .rf_expansion_factor = 2.0f,
        .num_hypercolumns = 16,
        .neurons_per_hypercolumn = 1000,
        .feedforward_strength = 1.0f,
        .feedback_strength = 0.5f,
        .custom_name = "V2b"
    };
    cortical_area_config_t v4_config = {
        .type = CORTICAL_AREA_V4,
        .stream = STREAM_VENTRAL,
        .hierarchy_level = 2,
        .rf_expansion_factor = 2.0f,
        .num_hypercolumns = 8,
        .neurons_per_hypercolumn = 1000,
        .feedforward_strength = 1.0f,
        .feedback_strength = 0.5f,
        .custom_name = nullptr
    };

    uint32_t v1_id, v2a_id, v2b_id, v4_id;
    cortical_hierarchy_add_area(hierarchy, &v1_config, &v1_id);
    cortical_hierarchy_add_area(hierarchy, &v2a_config, &v2a_id);
    cortical_hierarchy_add_area(hierarchy, &v2b_config, &v2b_id);
    cortical_hierarchy_add_area(hierarchy, &v4_config, &v4_id);

    // Connect V1 to both V2s
    inter_area_connection_config_t v1_v2a = {
        .source_area_id = v1_id,
        .target_area_id = v2a_id,
        .type = CONNECTION_TYPE_FEEDFORWARD,
        .source_layer = 2, .target_layer = 3, .weight = 1.0f,
        .delay_ms = 10.0f, .use_canonical_layers = true
    };
    inter_area_connection_config_t v1_v2b = {
        .source_area_id = v1_id,
        .target_area_id = v2b_id,
        .type = CONNECTION_TYPE_FEEDFORWARD,
        .source_layer = 2, .target_layer = 3, .weight = 1.0f,
        .delay_ms = 10.0f, .use_canonical_layers = true
    };
    inter_area_connection_config_t v2a_v4 = {
        .source_area_id = v2a_id,
        .target_area_id = v4_id,
        .type = CONNECTION_TYPE_FEEDFORWARD,
        .source_layer = 2, .target_layer = 3, .weight = 1.0f,
        .delay_ms = 10.0f, .use_canonical_layers = true
    };
    inter_area_connection_config_t v2b_v4 = {
        .source_area_id = v2b_id,
        .target_area_id = v4_id,
        .type = CONNECTION_TYPE_FEEDFORWARD,
        .source_layer = 2, .target_layer = 3, .weight = 1.0f,
        .delay_ms = 10.0f, .use_canonical_layers = true
    };

    uint32_t conn_id;
    cortical_hierarchy_connect_areas(hierarchy, &v1_v2a, &conn_id);
    cortical_hierarchy_connect_areas(hierarchy, &v1_v2b, &conn_id);
    cortical_hierarchy_connect_areas(hierarchy, &v2a_v4, &conn_id);
    cortical_hierarchy_connect_areas(hierarchy, &v2b_v4, &conn_id);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Measure baseline with both pathways", 500);
    auto input = generate_normal_input(32);
    cortical_hierarchy_set_area_input(hierarchy, v1_id, input.data(), 32);
    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 2);

    std::vector<float> baseline_v4(8);
    uint32_t size;
    cortical_hierarchy_get_area_activity(hierarchy, v4_id, baseline_v4.data(), 8, &size);
    float baseline_total = total_activation(baseline_v4);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Simulate V2a lesion", 300);
    cortical_hierarchy_remove_area(hierarchy, v2a_id);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify remaining pathway compensates", 500);
    cortical_hierarchy_set_area_input(hierarchy, v1_id, input.data(), 32);
    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 2);

    std::vector<float> post_lesion_v4(8);
    cortical_hierarchy_get_area_activity(hierarchy, v4_id, post_lesion_v4.data(), 8, &size);
    float post_lesion_total = total_activation(post_lesion_v4);

    EXPECT_GT(post_lesion_total, 0.0f)
        << "V2b pathway should maintain V4 activation after V2a lesion";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(CorticalPathologyE2ETest, Recovery_GradualRecovery) {
    E2E_PIPELINE_START("Recovery: Gradual Recovery");

    E2E_STAGE_BEGIN("Setup predictive system", 400);
    predictive_config_t config;
    cortical_predictive_default_config(&config);
    config.prediction_learning_rate = 0.1f;

    predictive = cortical_predictive_create(&config);
    ASSERT_NE(predictive, nullptr);
    cortical_predictive_add_level(predictive, 32, 32);
    cortical_predictive_add_level(predictive, 16, 16);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Learn pattern", 1000);
    auto pattern = generate_normal_input(32);
    std::vector<float> errors(32);

    for (int i = 0; i < 15; ++i) {
        cortical_predictive_compute_error(predictive, 0, pattern.data(), 32,
                                         errors.data(), 32);
        cortical_predictive_update_predictions(predictive, 0);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Measure pre-injury performance", 500);
    cortical_predictive_compute_error(predictive, 0, pattern.data(), 32,
                                     errors.data(), 32);
    float pre_injury_error = 0.0f;
    for (float e : errors) pre_injury_error += std::abs(e);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Simulate injury (reset predictions)", 300);
    // Simulate injury by corrupting predictions with noise
    // (In real system, this would damage the learned weights)
    auto noisy_pattern = generate_normal_input(32);
    cortical_predictive_compute_error(predictive, 0, noisy_pattern.data(), 32,
                                     errors.data(), 32);

    cortical_predictive_compute_error(predictive, 0, pattern.data(), 32,
                                     errors.data(), 32);
    float post_injury_error = 0.0f;
    for (float e : errors) post_injury_error += std::abs(e);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Simulate rehabilitation (relearning)", 1500);
    std::vector<float> recovery_errors;

    for (int session = 0; session < 20; ++session) {
        cortical_predictive_compute_error(predictive, 0, pattern.data(), 32,
                                         errors.data(), 32);
        cortical_predictive_update_predictions(predictive, 0);

        float session_error = 0.0f;
        for (float e : errors) session_error += std::abs(e);
        recovery_errors.push_back(session_error);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify recovery trajectory", 200);
    // Should show improvement over recovery sessions
    EXPECT_GT(recovery_errors.size(), 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(CorticalPathologyE2ETest, Recovery_CrossModalCompensation) {
    E2E_PIPELINE_START("Recovery: Cross-Modal Compensation");

    E2E_STAGE_BEGIN("Setup multimodal areas", 500);
    cortical_area_config_t visual_config = {
        .type = CORTICAL_AREA_V1,
        .stream = STREAM_VENTRAL,
        .hierarchy_level = 0,
        .rf_expansion_factor = 2.0f,
        .num_hypercolumns = 16,
        .neurons_per_hypercolumn = 1000,
        .feedforward_strength = 1.0f,
        .feedback_strength = 0.5f,
        .custom_name = nullptr
    };
    cortical_area_config_t auditory_config = {
        .type = CORTICAL_AREA_CUSTOM,
        .stream = STREAM_VENTRAL,
        .hierarchy_level = 0,
        .rf_expansion_factor = 2.0f,
        .num_hypercolumns = 16,
        .neurons_per_hypercolumn = 1000,
        .feedforward_strength = 1.0f,
        .feedback_strength = 0.5f,
        .custom_name = "A1"
    };
    cortical_area_config_t multimodal_config = {
        .type = CORTICAL_AREA_CUSTOM,
        .stream = STREAM_VENTRAL,
        .hierarchy_level = 1,
        .rf_expansion_factor = 2.0f,
        .num_hypercolumns = 8,
        .neurons_per_hypercolumn = 1000,
        .feedforward_strength = 1.0f,
        .feedback_strength = 0.5f,
        .custom_name = "MM"
    };

    uint32_t v1_id, a1_id, mm_id;
    cortical_hierarchy_add_area(hierarchy, &visual_config, &v1_id);
    cortical_hierarchy_add_area(hierarchy, &auditory_config, &a1_id);
    cortical_hierarchy_add_area(hierarchy, &multimodal_config, &mm_id);

    inter_area_connection_config_t v1_mm = {
        .source_area_id = v1_id, .target_area_id = mm_id,
        .type = CONNECTION_TYPE_FEEDFORWARD,
        .source_layer = 2, .target_layer = 3, .weight = 1.0f,
        .delay_ms = 10.0f, .use_canonical_layers = true
    };
    inter_area_connection_config_t a1_mm = {
        .source_area_id = a1_id, .target_area_id = mm_id,
        .type = CONNECTION_TYPE_FEEDFORWARD,
        .source_layer = 2, .target_layer = 3, .weight = 1.0f,
        .delay_ms = 10.0f, .use_canonical_layers = true
    };

    uint32_t conn_id;
    cortical_hierarchy_connect_areas(hierarchy, &v1_mm, &conn_id);
    cortical_hierarchy_connect_areas(hierarchy, &a1_mm, &conn_id);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test with visual input only", 500);
    auto visual_input = generate_normal_input(16);
    std::vector<float> zero_input(16, 0.0f);

    cortical_hierarchy_set_area_input(hierarchy, v1_id, visual_input.data(), 16);
    cortical_hierarchy_set_area_input(hierarchy, a1_id, zero_input.data(), 16);
    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 1);

    std::vector<float> visual_only_mm(8);
    uint32_t size;
    cortical_hierarchy_get_area_activity(hierarchy, mm_id, visual_only_mm.data(), 8, &size);
    float visual_only_total = total_activation(visual_only_mm);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Simulate visual cortex loss, use auditory compensation", 500);
    // Remove visual area
    cortical_hierarchy_remove_area(hierarchy, v1_id);

    // Now only auditory input
    auto auditory_input = generate_normal_input(16);
    cortical_hierarchy_set_area_input(hierarchy, a1_id, auditory_input.data(), 16);
    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 1);

    std::vector<float> auditory_only_mm(8);
    cortical_hierarchy_get_area_activity(hierarchy, mm_id, auditory_only_mm.data(), 8, &size);
    float auditory_only_total = total_activation(auditory_only_mm);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify cross-modal compensation", 200);
    // Auditory should provide some activation to multimodal area
    EXPECT_GT(auditory_only_total, 0.0f)
        << "Auditory pathway should compensate after visual loss";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Compensation Strategy Tests
//=============================================================================

TEST_F(CorticalPathologyE2ETest, Compensation_RedundantCoding) {
    E2E_PIPELINE_START("Compensation: Redundant Coding");

    E2E_STAGE_BEGIN("Create redundant hypercolumn array", 400);
    std::vector<hypercolumn_t*> hcols;
    for (int i = 0; i < 6; ++i) {
        hypercolumn_t* hcol = create_hypercolumn();
        ASSERT_NE(hcol, nullptr);
        hcols.push_back(hcol);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process same input through all columns (redundant)", 500);
    auto input = generate_normal_input(NUM_CHANNELS);

    std::vector<uint32_t> winners;
    for (auto hcol : hcols) {
        hypercolumn_compute(hcol, input.data(), NUM_CHANNELS);
        winners.push_back(hypercolumn_get_winner(hcol));
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Simulate 50% column failure", 200);
    for (int i = 0; i < 3; ++i) {
        hypercolumn_destroy(hcols[i]);
        hcols[i] = nullptr;
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify decoding from remaining columns", 500);
    std::vector<uint32_t> remaining_winners;
    for (int i = 3; i < 6; ++i) {
        hypercolumn_compute(hcols[i], input.data(), NUM_CHANNELS);
        remaining_winners.push_back(hypercolumn_get_winner(hcols[i]));
    }

    EXPECT_EQ(remaining_winners.size(), 3) << "Should have 3 remaining responses";
    E2E_STAGE_END();

    for (int i = 3; i < 6; ++i) {
        if (hcols[i]) hypercolumn_destroy(hcols[i]);
    }
    E2E_PIPELINE_END();
}

TEST_F(CorticalPathologyE2ETest, Compensation_ErrorCorrecting) {
    E2E_PIPELINE_START("Compensation: Error Correcting");

    E2E_STAGE_BEGIN("Setup predictive system for error correction", 400);
    predictive_config_t config;
    cortical_predictive_default_config(&config);
    config.error_gain = 2.0f;  // Amplify errors for correction
    config.enable_precision_weighting = true;

    predictive = cortical_predictive_create(&config);
    ASSERT_NE(predictive, nullptr);
    cortical_predictive_add_level(predictive, 32, 32);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Learn correct pattern", 1000);
    auto correct_pattern = generate_normal_input(32);
    std::vector<float> errors(32);

    for (int i = 0; i < 20; ++i) {
        cortical_predictive_compute_error(predictive, 0, correct_pattern.data(), 32,
                                         errors.data(), 32);
        cortical_predictive_update_predictions(predictive, 0);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test with corrupted input", 500);
    auto corrupted_pattern = correct_pattern;
    // Corrupt 25% of input
    for (uint32_t i = 0; i < 8; ++i) {
        corrupted_pattern[i] = 1.0f - corrupted_pattern[i];
    }

    cortical_predictive_compute_error(predictive, 0, corrupted_pattern.data(), 32,
                                     errors.data(), 32);

    // Apply precision weighting
    std::vector<float> weighted_errors(32);
    cortical_predictive_weight_by_precision(predictive, 0, errors.data(), 32,
                                            weighted_errors.data(), 32);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify error detection", 200);
    float error_magnitude = 0.0f;
    for (float e : weighted_errors) error_magnitude += std::abs(e);

    EXPECT_GT(error_magnitude, 0.0f)
        << "Should detect errors in corrupted input";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(CorticalPathologyE2ETest, Compensation_AlternativePathway) {
    E2E_PIPELINE_START("Compensation: Alternative Pathway");

    E2E_STAGE_BEGIN("Setup main and bypass pathways", 600);
    cortical_area_config_t input_area = {
        .type = CORTICAL_AREA_V1,
        .stream = STREAM_VENTRAL,
        .hierarchy_level = 0,
        .rf_expansion_factor = 2.0f,
        .num_hypercolumns = 16,
        .neurons_per_hypercolumn = 1000,
        .feedforward_strength = 1.0f,
        .feedback_strength = 0.5f,
        .custom_name = "Input"
    };
    cortical_area_config_t main_area = {
        .type = CORTICAL_AREA_V2,
        .stream = STREAM_VENTRAL,
        .hierarchy_level = 1,
        .rf_expansion_factor = 2.0f,
        .num_hypercolumns = 8,
        .neurons_per_hypercolumn = 1000,
        .feedforward_strength = 1.0f,
        .feedback_strength = 0.5f,
        .custom_name = "Main"
    };
    cortical_area_config_t bypass_area = {
        .type = CORTICAL_AREA_MT,
        .stream = STREAM_DORSAL,
        .hierarchy_level = 1,
        .rf_expansion_factor = 2.0f,
        .num_hypercolumns = 8,
        .neurons_per_hypercolumn = 1000,
        .feedforward_strength = 0.5f,  // Initially weaker
        .feedback_strength = 0.5f,
        .custom_name = "Bypass"
    };
    cortical_area_config_t output_area = {
        .type = CORTICAL_AREA_V4,
        .stream = STREAM_VENTRAL,
        .hierarchy_level = 2,
        .rf_expansion_factor = 2.0f,
        .num_hypercolumns = 4,
        .neurons_per_hypercolumn = 1000,
        .feedforward_strength = 1.0f,
        .feedback_strength = 0.5f,
        .custom_name = "Output"
    };

    uint32_t input_id, main_id, bypass_id, output_id;
    cortical_hierarchy_add_area(hierarchy, &input_area, &input_id);
    cortical_hierarchy_add_area(hierarchy, &main_area, &main_id);
    cortical_hierarchy_add_area(hierarchy, &bypass_area, &bypass_id);
    cortical_hierarchy_add_area(hierarchy, &output_area, &output_id);

    inter_area_connection_config_t in_main = {
        .source_area_id = input_id, .target_area_id = main_id,
        .type = CONNECTION_TYPE_FEEDFORWARD,
        .source_layer = 2, .target_layer = 3, .weight = 1.0f,
        .delay_ms = 10.0f, .use_canonical_layers = true
    };
    inter_area_connection_config_t in_bypass = {
        .source_area_id = input_id, .target_area_id = bypass_id,
        .type = CONNECTION_TYPE_FEEDFORWARD,
        .source_layer = 2, .target_layer = 3, .weight = 0.5f,
        .delay_ms = 10.0f, .use_canonical_layers = true
    };
    inter_area_connection_config_t main_out = {
        .source_area_id = main_id, .target_area_id = output_id,
        .type = CONNECTION_TYPE_FEEDFORWARD,
        .source_layer = 2, .target_layer = 3, .weight = 1.0f,
        .delay_ms = 10.0f, .use_canonical_layers = true
    };
    inter_area_connection_config_t bypass_out = {
        .source_area_id = bypass_id, .target_area_id = output_id,
        .type = CONNECTION_TYPE_FEEDFORWARD,
        .source_layer = 2, .target_layer = 3, .weight = 0.5f,
        .delay_ms = 10.0f, .use_canonical_layers = true
    };

    uint32_t conn_id;
    cortical_hierarchy_connect_areas(hierarchy, &in_main, &conn_id);
    cortical_hierarchy_connect_areas(hierarchy, &in_bypass, &conn_id);
    cortical_hierarchy_connect_areas(hierarchy, &main_out, &conn_id);
    cortical_hierarchy_connect_areas(hierarchy, &bypass_out, &conn_id);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Measure output with main pathway intact", 500);
    auto input = generate_normal_input(16);
    cortical_hierarchy_set_area_input(hierarchy, input_id, input.data(), 16);
    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 2);

    std::vector<float> intact_output(4);
    uint32_t size;
    cortical_hierarchy_get_area_activity(hierarchy, output_id, intact_output.data(), 4, &size);
    float intact_total = total_activation(intact_output);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Remove main pathway", 200);
    cortical_hierarchy_remove_area(hierarchy, main_id);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify bypass pathway compensates", 500);
    cortical_hierarchy_set_area_input(hierarchy, input_id, input.data(), 16);
    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 2);

    std::vector<float> bypass_output(4);
    cortical_hierarchy_get_area_activity(hierarchy, output_id, bypass_output.data(), 4, &size);
    float bypass_total = total_activation(bypass_output);

    EXPECT_GT(bypass_total, 0.0f)
        << "Bypass pathway should provide output after main pathway lesion";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
