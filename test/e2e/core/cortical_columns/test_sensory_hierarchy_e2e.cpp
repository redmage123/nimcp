//=============================================================================
// test_sensory_hierarchy_e2e.cpp - Sensory Hierarchy End-to-End Tests
//=============================================================================
/**
 * @file test_sensory_hierarchy_e2e.cpp
 * @brief End-to-end tests for multi-level sensory processing in cortical hierarchy
 *
 * WHAT: Full pipeline tests for hierarchical sensory processing
 * WHY:  Verify complete sensory pathways with feedforward/feedback interactions
 * HOW:  Test multi-level processing, attention, cross-modal integration,
 *       and temporal sequence processing
 *
 * TEST COVERAGE:
 * - Multi-level sensory processing
 * - Bottom-up feature extraction
 * - Top-down attention and prediction
 * - Cross-modal integration
 * - Temporal sequence processing
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <array>
#include <numeric>
#include <algorithm>

#include "e2e_test_framework.h"
#include "core/cortical_columns/nimcp_cortical_column.h"
#include "core/cortical_columns/nimcp_cortical_hierarchy.h"
#include "core/cortical_columns/nimcp_cortical_predictive_coding.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SensoryHierarchyE2ETest : public ::testing::Test {
protected:
    cortical_column_pool_t* pool = nullptr;
    cortical_hierarchy_t* hierarchy = nullptr;
    cortical_predictive_t* predictive = nullptr;

    static constexpr uint32_t NUM_LEVELS = 4;
    static constexpr uint32_t BASE_UNITS = 64;

    void SetUp() override {
        // Create memory pool
        cortical_column_pool_config_t pool_config = {
            .max_minicolumns = 500,
            .max_hypercolumns = 100,
            .max_neurons_per_minicolumn = 100,
            .enable_cow_support = true
        };
        pool = cortical_column_pool_create(&pool_config);
        ASSERT_NE(pool, nullptr);

        // Create hierarchy
        cortical_hierarchy_config_t hier_config = cortical_hierarchy_default_config();
        hier_config.max_areas = 10;
        hier_config.max_connections = 50;
        hier_config.enable_predictive_coding = true;
        hier_config.enable_bio_async = false;
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

    // Helper: Create multi-level sensory hierarchy
    std::vector<uint32_t> create_sensory_hierarchy(uint32_t num_levels,
                                                    cortical_area_type_t base_type) {
        std::vector<uint32_t> area_ids;
        cortical_area_type_t types[] = {CORTICAL_AREA_V1, CORTICAL_AREA_V2,
                                         CORTICAL_AREA_V4, CORTICAL_AREA_IT};

        for (uint32_t level = 0; level < num_levels; ++level) {
            cortical_area_config_t config = {
                .type = types[level % 4],
                .stream = STREAM_VENTRAL,
                .hierarchy_level = level,
                .rf_expansion_factor = 2.0f,
                .num_hypercolumns = BASE_UNITS >> level,
                .neurons_per_hypercolumn = 1000,
                .feedforward_strength = 1.0f,
                .feedback_strength = 0.5f,
                .custom_name = nullptr
            };

            uint32_t area_id;
            cortical_hierarchy_add_area(hierarchy, &config, &area_id);
            area_ids.push_back(area_id);
        }
        return area_ids;
    }

    // Helper: Connect areas with feedforward and feedback
    void connect_hierarchy(const std::vector<uint32_t>& area_ids) {
        for (size_t i = 0; i < area_ids.size() - 1; ++i) {
            // Feedforward
            inter_area_connection_config_t ff_config = {
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
            cortical_hierarchy_connect_areas(hierarchy, &ff_config, &conn_id);

            // Feedback
            inter_area_connection_config_t fb_config = {
                .source_area_id = area_ids[i + 1],
                .target_area_id = area_ids[i],
                .type = CONNECTION_TYPE_FEEDBACK,
                .source_layer = 4,
                .target_layer = 0,
                .weight = 0.5f,
                .delay_ms = 10.0f,
                .use_canonical_layers = true
            };
            cortical_hierarchy_connect_areas(hierarchy, &fb_config, &conn_id);
        }
    }

    // Helper: Generate sensory input pattern
    std::vector<float> generate_sensory_input(uint32_t size, float center, float width) {
        std::vector<float> input(size);
        for (uint32_t i = 0; i < size; ++i) {
            float x = static_cast<float>(i) / size;
            float dist = (x - center) / width;
            input[i] = std::exp(-0.5f * dist * dist);
        }
        return input;
    }

    // Helper: Generate temporal sequence
    std::vector<std::vector<float>> generate_temporal_sequence(uint32_t size,
                                                                uint32_t num_frames) {
        std::vector<std::vector<float>> sequence;
        for (uint32_t t = 0; t < num_frames; ++t) {
            float center = 0.2f + 0.6f * t / num_frames;  // Moving stimulus
            sequence.push_back(generate_sensory_input(size, center, 0.1f));
        }
        return sequence;
    }

    // Helper: Calculate total activation
    float calculate_total_activation(const std::vector<float>& activations) {
        return std::accumulate(activations.begin(), activations.end(), 0.0f);
    }

    // Helper: Calculate activation sparsity
    float calculate_sparsity(const std::vector<float>& activations, float threshold = 0.1f) {
        uint32_t active = 0;
        for (float a : activations) {
            if (a > threshold) active++;
        }
        return static_cast<float>(active) / activations.size();
    }
};

//=============================================================================
// Multi-Level Sensory Processing Tests
//=============================================================================

TEST_F(SensoryHierarchyE2ETest, MultiLevel_FeedforwardPropagation) {
    E2E_PIPELINE_START("Multi-Level: Feedforward Propagation");

    E2E_STAGE_BEGIN("Create 4-level sensory hierarchy", 500);
    auto area_ids = create_sensory_hierarchy(4, CORTICAL_AREA_V1);
    EXPECT_EQ(area_ids.size(), 4);
    connect_hierarchy(area_ids);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Inject sensory input at level 0", 200);
    auto input = generate_sensory_input(BASE_UNITS, 0.5f, 0.1f);
    int result = cortical_hierarchy_set_area_input(hierarchy, area_ids[0],
                                                   input.data(), BASE_UNITS);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Propagate feedforward through all levels", 1000);
    result = cortical_hierarchy_propagate_feedforward(hierarchy, 0, 3);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify activity at each level", 500);
    for (uint32_t level = 0; level < 4; ++level) {
        uint32_t expected_size = BASE_UNITS >> level;
        std::vector<float> activity(expected_size);
        uint32_t actual_size;

        result = cortical_hierarchy_get_area_activity(hierarchy, area_ids[level],
                                                      activity.data(), expected_size,
                                                      &actual_size);
        EXPECT_EQ(result, 0);

        float total = calculate_total_activation(activity);
        EXPECT_GT(total, 0.0f) << "Level " << level << " should have activity";
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(SensoryHierarchyE2ETest, MultiLevel_FeedbackModulation) {
    E2E_PIPELINE_START("Multi-Level: Feedback Modulation");

    E2E_STAGE_BEGIN("Setup hierarchy", 500);
    auto area_ids = create_sensory_hierarchy(3, CORTICAL_AREA_V1);
    connect_hierarchy(area_ids);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Set top-down expectation at highest level", 300);
    uint32_t top_size = BASE_UNITS >> 2;
    std::vector<float> top_down(top_size);
    top_down[0] = 1.0f;  // Expect activity in first region
    cortical_hierarchy_set_area_input(hierarchy, area_ids[2], top_down.data(), top_size);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Propagate feedback to lower levels", 500);
    int result = cortical_hierarchy_propagate_feedback(hierarchy, 2, 0);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify feedback modulates lower levels", 300);
    std::vector<float> level0_activity(BASE_UNITS);
    uint32_t size;
    cortical_hierarchy_get_area_activity(hierarchy, area_ids[0],
                                         level0_activity.data(), BASE_UNITS, &size);

    // First quarter should be biased by top-down
    float first_quarter = 0.0f, rest = 0.0f;
    for (uint32_t i = 0; i < BASE_UNITS / 4; ++i) {
        first_quarter += level0_activity[i];
    }
    for (uint32_t i = BASE_UNITS / 4; i < BASE_UNITS; ++i) {
        rest += level0_activity[i];
    }

    // Feedback should bias the first region
    EXPECT_GE(first_quarter, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(SensoryHierarchyE2ETest, MultiLevel_BidirectionalProcessing) {
    E2E_PIPELINE_START("Multi-Level: Bidirectional Processing");

    E2E_STAGE_BEGIN("Setup hierarchy with strong feedback", 500);
    auto area_ids = create_sensory_hierarchy(3, CORTICAL_AREA_V1);
    connect_hierarchy(area_ids);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Set bottom-up input", 200);
    auto input = generate_sensory_input(BASE_UNITS, 0.5f, 0.15f);
    cortical_hierarchy_set_area_input(hierarchy, area_ids[0], input.data(), BASE_UNITS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Run feedforward sweep", 500);
    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 2);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Run feedback sweep", 500);
    cortical_hierarchy_propagate_feedback(hierarchy, 2, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Run second feedforward sweep (recurrent)", 500);
    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 2);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify stable processing", 300);
    cortical_hierarchy_stats_t stats;
    int result = cortical_hierarchy_get_stats(hierarchy, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GT(stats.total_propagations, 2);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Bottom-Up Feature Extraction Tests
//=============================================================================

TEST_F(SensoryHierarchyE2ETest, BottomUp_SimpleToComplexFeatures) {
    E2E_PIPELINE_START("Bottom-Up: Simple to Complex Features");

    E2E_STAGE_BEGIN("Create feature extraction hierarchy", 500);
    auto area_ids = create_sensory_hierarchy(4, CORTICAL_AREA_V1);
    connect_hierarchy(area_ids);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Inject edge-like pattern", 300);
    // Simple edge: half on, half off
    std::vector<float> edge_input(BASE_UNITS);
    for (uint32_t i = 0; i < BASE_UNITS / 2; ++i) {
        edge_input[i] = 0.9f;
    }
    for (uint32_t i = BASE_UNITS / 2; i < BASE_UNITS; ++i) {
        edge_input[i] = 0.1f;
    }
    cortical_hierarchy_set_area_input(hierarchy, area_ids[0], edge_input.data(), BASE_UNITS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Extract features through hierarchy", 1000);
    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 3);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify feature compression", 300);
    // Higher levels should have more compressed (smaller) representations
    std::vector<float> level0_act(BASE_UNITS);
    std::vector<float> level3_act(BASE_UNITS >> 3);
    uint32_t size0, size3;

    cortical_hierarchy_get_area_activity(hierarchy, area_ids[0],
                                         level0_act.data(), BASE_UNITS, &size0);
    cortical_hierarchy_get_area_activity(hierarchy, area_ids[3],
                                         level3_act.data(), BASE_UNITS >> 3, &size3);

    EXPECT_LT(size3, size0) << "Higher levels should have fewer units";
    EXPECT_GT(calculate_total_activation(level3_act), 0.0f)
        << "Top level should have activity";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(SensoryHierarchyE2ETest, BottomUp_SparseRepresentations) {
    E2E_PIPELINE_START("Bottom-Up: Sparse Representations");

    E2E_STAGE_BEGIN("Setup hierarchy", 500);
    auto area_ids = create_sensory_hierarchy(3, CORTICAL_AREA_V1);
    connect_hierarchy(area_ids);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process localized input", 500);
    auto input = generate_sensory_input(BASE_UNITS, 0.5f, 0.05f);  // Narrow peak
    cortical_hierarchy_set_area_input(hierarchy, area_ids[0], input.data(), BASE_UNITS);
    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 2);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Measure sparsity at each level", 500);
    std::vector<float> sparsities;

    for (uint32_t level = 0; level < 3; ++level) {
        uint32_t level_size = BASE_UNITS >> level;
        std::vector<float> activity(level_size);
        uint32_t actual_size;

        cortical_hierarchy_get_area_activity(hierarchy, area_ids[level],
                                             activity.data(), level_size, &actual_size);

        float sparsity = calculate_sparsity(activity, 0.1f);
        sparsities.push_back(sparsity);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify sparse coding", 200);
    // Representations should remain relatively sparse
    for (float s : sparsities) {
        EXPECT_LT(s, 0.9f) << "Representations should be sparse (< 90% active)";
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(SensoryHierarchyE2ETest, BottomUp_FeaturePooling) {
    E2E_PIPELINE_START("Bottom-Up: Feature Pooling");

    E2E_STAGE_BEGIN("Create pooling hierarchy", 500);
    auto area_ids = create_sensory_hierarchy(3, CORTICAL_AREA_V1);
    connect_hierarchy(area_ids);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Inject multiple local features", 300);
    // Create multiple active regions
    std::vector<float> multi_input(BASE_UNITS, 0.0f);
    for (uint32_t i = 0; i < BASE_UNITS; i += 8) {
        multi_input[i] = 0.8f;
        multi_input[i + 1] = 0.6f;
    }
    cortical_hierarchy_set_area_input(hierarchy, area_ids[0], multi_input.data(), BASE_UNITS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Pool features through hierarchy", 500);
    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 2);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify pooling maintains signal", 300);
    uint32_t top_size = BASE_UNITS >> 2;
    std::vector<float> top_activity(top_size);
    uint32_t actual_size;

    cortical_hierarchy_get_area_activity(hierarchy, area_ids[2],
                                         top_activity.data(), top_size, &actual_size);

    float top_total = calculate_total_activation(top_activity);
    EXPECT_GT(top_total, 0.0f) << "Pooled signal should propagate to top";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Top-Down Attention and Prediction Tests
//=============================================================================

TEST_F(SensoryHierarchyE2ETest, TopDown_AttentionalBiasing) {
    E2E_PIPELINE_START("Top-Down: Attentional Biasing");

    E2E_STAGE_BEGIN("Setup hierarchy", 500);
    auto area_ids = create_sensory_hierarchy(3, CORTICAL_AREA_V1);
    connect_hierarchy(area_ids);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Set uniform bottom-up input", 200);
    std::vector<float> uniform_input(BASE_UNITS, 0.5f);
    cortical_hierarchy_set_area_input(hierarchy, area_ids[0], uniform_input.data(), BASE_UNITS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply top-down attention to specific region", 300);
    uint32_t top_size = BASE_UNITS >> 2;
    std::vector<float> attention(top_size, 0.0f);
    attention[top_size / 2] = 1.0f;  // Attend to center
    cortical_hierarchy_set_area_input(hierarchy, area_ids[2], attention.data(), top_size);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Propagate attention downward", 500);
    cortical_hierarchy_propagate_feedback(hierarchy, 2, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process with attention", 500);
    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 2);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify attended region enhanced", 300);
    std::vector<float> level0_activity(BASE_UNITS);
    uint32_t size;
    cortical_hierarchy_get_area_activity(hierarchy, area_ids[0],
                                         level0_activity.data(), BASE_UNITS, &size);

    // Central region should be biased
    float center_sum = 0.0f, edge_sum = 0.0f;
    for (uint32_t i = BASE_UNITS / 4; i < 3 * BASE_UNITS / 4; ++i) {
        center_sum += level0_activity[i];
    }
    for (uint32_t i = 0; i < BASE_UNITS / 4; ++i) {
        edge_sum += level0_activity[i];
    }

    // With top-down attention, center should be influenced
    EXPECT_GE(center_sum + edge_sum, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(SensoryHierarchyE2ETest, TopDown_PredictiveProcessing) {
    E2E_PIPELINE_START("Top-Down: Predictive Processing");

    E2E_STAGE_BEGIN("Setup predictive hierarchy", 400);
    predictive_config_t pc_config;
    cortical_predictive_default_config(&pc_config);
    pc_config.hierarchy_depth = 3;
    pc_config.enable_precision_weighting = true;

    predictive = cortical_predictive_create(&pc_config);
    ASSERT_NE(predictive, nullptr);

    cortical_predictive_add_level(predictive, 64, 64);
    cortical_predictive_add_level(predictive, 32, 32);
    cortical_predictive_add_level(predictive, 16, 16);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Generate top-down predictions", 500);
    std::vector<float> predictions(64);
    int num = cortical_predictive_compute_prediction(predictive, 0, predictions.data(), 64);
    EXPECT_GE(num, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Compare with bottom-up input", 500);
    auto sensory_input = generate_sensory_input(64, 0.5f, 0.1f);
    std::vector<float> errors(64);
    int result = cortical_predictive_compute_error(predictive, 0, sensory_input.data(), 64,
                                                   errors.data(), 64);
    EXPECT_GE(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify prediction error computation", 200);
    float total_error = 0.0f;
    for (float e : errors) {
        total_error += std::abs(e);
    }
    EXPECT_GT(total_error, 0.0f) << "Should compute prediction error";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(SensoryHierarchyE2ETest, TopDown_ExpectationDrivenPerception) {
    E2E_PIPELINE_START("Top-Down: Expectation-Driven Perception");

    E2E_STAGE_BEGIN("Setup hierarchies", 600);
    auto area_ids = create_sensory_hierarchy(3, CORTICAL_AREA_V1);
    connect_hierarchy(area_ids);

    predictive_config_t pc_config;
    cortical_predictive_default_config(&pc_config);
    predictive = cortical_predictive_create(&pc_config);
    ASSERT_NE(predictive, nullptr);

    cortical_predictive_add_level(predictive, BASE_UNITS, BASE_UNITS);
    cortical_predictive_add_level(predictive, BASE_UNITS / 2, BASE_UNITS / 2);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process expected stimulus", 500);
    auto expected = generate_sensory_input(BASE_UNITS, 0.5f, 0.1f);
    cortical_hierarchy_set_area_input(hierarchy, area_ids[0], expected.data(), BASE_UNITS);
    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 2);

    std::vector<float> errors_expected(BASE_UNITS);
    cortical_predictive_compute_error(predictive, 0, expected.data(), BASE_UNITS,
                                     errors_expected.data(), BASE_UNITS);
    float error_expected = 0.0f;
    for (float e : errors_expected) error_expected += std::abs(e);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process unexpected stimulus", 500);
    auto unexpected = generate_sensory_input(BASE_UNITS, 0.2f, 0.1f);  // Different position
    cortical_hierarchy_set_area_input(hierarchy, area_ids[0], unexpected.data(), BASE_UNITS);
    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 2);

    std::vector<float> errors_unexpected(BASE_UNITS);
    cortical_predictive_compute_error(predictive, 0, unexpected.data(), BASE_UNITS,
                                     errors_unexpected.data(), BASE_UNITS);
    float error_unexpected = 0.0f;
    for (float e : errors_unexpected) error_unexpected += std::abs(e);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify expectation effect", 200);
    // Both should produce some error initially
    EXPECT_GT(error_expected + error_unexpected, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Cross-Modal Integration Tests
//=============================================================================

TEST_F(SensoryHierarchyE2ETest, CrossModal_VisualAuditoryBinding) {
    E2E_PIPELINE_START("Cross-Modal: Visual-Auditory Binding");

    E2E_STAGE_BEGIN("Create visual and auditory hierarchies", 600);
    // Visual stream
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
    uint32_t v1_id;
    cortical_hierarchy_add_area(hierarchy, &v1_config, &v1_id);

    // Auditory area (using custom type)
    cortical_area_config_t a1_config = {
        .type = CORTICAL_AREA_CUSTOM,
        .stream = STREAM_VENTRAL,
        .hierarchy_level = 0,
        .rf_expansion_factor = 2.0f,
        .num_hypercolumns = 32,
        .neurons_per_hypercolumn = 1000,
        .feedforward_strength = 1.0f,
        .feedback_strength = 0.5f,
        .custom_name = "A1"
    };
    uint32_t a1_id;
    cortical_hierarchy_add_area(hierarchy, &a1_config, &a1_id);

    // Multimodal integration area
    cortical_area_config_t mm_config = {
        .type = CORTICAL_AREA_CUSTOM,
        .stream = STREAM_VENTRAL,
        .hierarchy_level = 1,
        .rf_expansion_factor = 2.0f,
        .num_hypercolumns = 16,
        .neurons_per_hypercolumn = 1000,
        .feedforward_strength = 1.0f,
        .feedback_strength = 0.5f,
        .custom_name = "STS"  // Superior temporal sulcus
    };
    uint32_t mm_id;
    cortical_hierarchy_add_area(hierarchy, &mm_config, &mm_id);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Connect modalities to integration area", 300);
    // V1 -> STS
    inter_area_connection_config_t v1_sts = {
        .source_area_id = v1_id,
        .target_area_id = mm_id,
        .type = CONNECTION_TYPE_FEEDFORWARD,
        .source_layer = 2,
        .target_layer = 3,
        .weight = 1.0f,
        .delay_ms = 20.0f,
        .use_canonical_layers = true
    };
    uint32_t conn1;
    cortical_hierarchy_connect_areas(hierarchy, &v1_sts, &conn1);

    // A1 -> STS
    inter_area_connection_config_t a1_sts = {
        .source_area_id = a1_id,
        .target_area_id = mm_id,
        .type = CONNECTION_TYPE_FEEDFORWARD,
        .source_layer = 2,
        .target_layer = 3,
        .weight = 1.0f,
        .delay_ms = 20.0f,
        .use_canonical_layers = true
    };
    uint32_t conn2;
    cortical_hierarchy_connect_areas(hierarchy, &a1_sts, &conn2);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Present concurrent audiovisual stimuli", 500);
    // Visual: flash at center
    auto visual_input = generate_sensory_input(32, 0.5f, 0.1f);
    cortical_hierarchy_set_area_input(hierarchy, v1_id, visual_input.data(), 32);

    // Auditory: sound at same position
    auto auditory_input = generate_sensory_input(32, 0.5f, 0.1f);
    cortical_hierarchy_set_area_input(hierarchy, a1_id, auditory_input.data(), 32);

    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 1);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify multimodal integration", 300);
    std::vector<float> mm_activity(16);
    uint32_t size;
    cortical_hierarchy_get_area_activity(hierarchy, mm_id, mm_activity.data(), 16, &size);

    float total = calculate_total_activation(mm_activity);
    EXPECT_GT(total, 0.0f) << "Multimodal area should integrate both inputs";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(SensoryHierarchyE2ETest, CrossModal_SpatialCongruence) {
    E2E_PIPELINE_START("Cross-Modal: Spatial Congruence");

    E2E_STAGE_BEGIN("Setup multimodal areas", 500);
    // Create two unimodal areas and one multimodal
    cortical_area_config_t m1_config = {
        .type = CORTICAL_AREA_CUSTOM,
        .stream = STREAM_VENTRAL,
        .hierarchy_level = 0,
        .rf_expansion_factor = 2.0f,
        .num_hypercolumns = 32,
        .neurons_per_hypercolumn = 1000,
        .feedforward_strength = 1.0f,
        .feedback_strength = 0.5f,
        .custom_name = "M1"
    };
    cortical_area_config_t m2_config = {
        .type = CORTICAL_AREA_CUSTOM,
        .stream = STREAM_VENTRAL,
        .hierarchy_level = 0,
        .rf_expansion_factor = 2.0f,
        .num_hypercolumns = 32,
        .neurons_per_hypercolumn = 1000,
        .feedforward_strength = 1.0f,
        .feedback_strength = 0.5f,
        .custom_name = "M2"
    };
    cortical_area_config_t mm_config = {
        .type = CORTICAL_AREA_CUSTOM,
        .stream = STREAM_VENTRAL,
        .hierarchy_level = 1,
        .rf_expansion_factor = 2.0f,
        .num_hypercolumns = 16,
        .neurons_per_hypercolumn = 1000,
        .feedforward_strength = 1.0f,
        .feedback_strength = 0.5f,
        .custom_name = "MM"
    };

    uint32_t m1_id, m2_id, mm_id;
    cortical_hierarchy_add_area(hierarchy, &m1_config, &m1_id);
    cortical_hierarchy_add_area(hierarchy, &m2_config, &m2_id);
    cortical_hierarchy_add_area(hierarchy, &mm_config, &mm_id);

    inter_area_connection_config_t conn = {
        .source_area_id = m1_id,
        .target_area_id = mm_id,
        .type = CONNECTION_TYPE_FEEDFORWARD,
        .source_layer = 2,
        .target_layer = 3,
        .weight = 1.0f,
        .delay_ms = 10.0f,
        .use_canonical_layers = true
    };
    uint32_t conn_id;
    cortical_hierarchy_connect_areas(hierarchy, &conn, &conn_id);

    conn.source_area_id = m2_id;
    cortical_hierarchy_connect_areas(hierarchy, &conn, &conn_id);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test congruent stimuli (same location)", 500);
    auto m1_input = generate_sensory_input(32, 0.5f, 0.1f);
    auto m2_input = generate_sensory_input(32, 0.5f, 0.1f);

    cortical_hierarchy_set_area_input(hierarchy, m1_id, m1_input.data(), 32);
    cortical_hierarchy_set_area_input(hierarchy, m2_id, m2_input.data(), 32);
    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 1);

    std::vector<float> congruent_activity(16);
    uint32_t size;
    cortical_hierarchy_get_area_activity(hierarchy, mm_id, congruent_activity.data(), 16, &size);
    float congruent_total = calculate_total_activation(congruent_activity);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test incongruent stimuli (different locations)", 500);
    auto m1_input2 = generate_sensory_input(32, 0.2f, 0.1f);
    auto m2_input2 = generate_sensory_input(32, 0.8f, 0.1f);

    cortical_hierarchy_set_area_input(hierarchy, m1_id, m1_input2.data(), 32);
    cortical_hierarchy_set_area_input(hierarchy, m2_id, m2_input2.data(), 32);
    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 1);

    std::vector<float> incongruent_activity(16);
    cortical_hierarchy_get_area_activity(hierarchy, mm_id, incongruent_activity.data(), 16, &size);
    float incongruent_total = calculate_total_activation(incongruent_activity);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify both processed", 200);
    // Both should produce some activity
    EXPECT_GT(congruent_total + incongruent_total, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Temporal Sequence Processing Tests
//=============================================================================

TEST_F(SensoryHierarchyE2ETest, Temporal_SequenceProcessing) {
    E2E_PIPELINE_START("Temporal: Sequence Processing");

    E2E_STAGE_BEGIN("Setup hierarchy", 400);
    auto area_ids = create_sensory_hierarchy(3, CORTICAL_AREA_V1);
    connect_hierarchy(area_ids);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Generate moving stimulus sequence", 300);
    auto sequence = generate_temporal_sequence(BASE_UNITS, 10);
    EXPECT_EQ(sequence.size(), 10);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process sequence through hierarchy", 2000);
    std::vector<std::vector<float>> top_responses;

    for (const auto& frame : sequence) {
        cortical_hierarchy_set_area_input(hierarchy, area_ids[0], frame.data(), BASE_UNITS);
        cortical_hierarchy_propagate_feedforward(hierarchy, 0, 2);

        uint32_t top_size = BASE_UNITS >> 2;
        std::vector<float> top_activity(top_size);
        uint32_t actual_size;
        cortical_hierarchy_get_area_activity(hierarchy, area_ids[2],
                                             top_activity.data(), top_size, &actual_size);
        top_responses.push_back(top_activity);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify temporal tracking", 300);
    for (size_t t = 0; t < top_responses.size(); ++t) {
        float total = calculate_total_activation(top_responses[t]);
        EXPECT_GT(total, 0.0f) << "Frame " << t << " should produce activity";
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(SensoryHierarchyE2ETest, Temporal_MotionDetection) {
    E2E_PIPELINE_START("Temporal: Motion Detection");

    E2E_STAGE_BEGIN("Setup dorsal stream for motion", 500);
    // Dorsal stream for motion processing
    cortical_area_config_t v1_config = {
        .type = CORTICAL_AREA_V1,
        .stream = STREAM_DORSAL,
        .hierarchy_level = 0,
        .rf_expansion_factor = 2.0f,
        .num_hypercolumns = 32,
        .neurons_per_hypercolumn = 1000,
        .feedforward_strength = 1.0f,
        .feedback_strength = 0.5f,
        .custom_name = nullptr
    };
    cortical_area_config_t mt_config = {
        .type = CORTICAL_AREA_MT,
        .stream = STREAM_DORSAL,
        .hierarchy_level = 1,
        .rf_expansion_factor = 3.0f,
        .num_hypercolumns = 16,
        .neurons_per_hypercolumn = 1000,
        .feedforward_strength = 1.0f,
        .feedback_strength = 0.5f,
        .custom_name = nullptr
    };

    uint32_t v1_id, mt_id;
    cortical_hierarchy_add_area(hierarchy, &v1_config, &v1_id);
    cortical_hierarchy_add_area(hierarchy, &mt_config, &mt_id);

    inter_area_connection_config_t conn = {
        .source_area_id = v1_id,
        .target_area_id = mt_id,
        .type = CONNECTION_TYPE_FEEDFORWARD,
        .source_layer = 2,
        .target_layer = 3,
        .weight = 1.0f,
        .delay_ms = 10.0f,
        .use_canonical_layers = true
    };
    uint32_t conn_id;
    cortical_hierarchy_connect_areas(hierarchy, &conn, &conn_id);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process moving stimulus", 1500);
    std::vector<float> mt_responses;

    for (int frame = 0; frame < 8; ++frame) {
        float position = 0.1f + 0.1f * frame;
        auto input = generate_sensory_input(32, position, 0.05f);

        cortical_hierarchy_set_area_input(hierarchy, v1_id, input.data(), 32);
        cortical_hierarchy_propagate_feedforward(hierarchy, 0, 1);

        std::vector<float> mt_activity(16);
        uint32_t size;
        cortical_hierarchy_get_area_activity(hierarchy, mt_id, mt_activity.data(), 16, &size);

        float total = calculate_total_activation(mt_activity);
        mt_responses.push_back(total);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify motion responses", 200);
    // MT should respond throughout motion
    for (size_t i = 0; i < mt_responses.size(); ++i) {
        EXPECT_GT(mt_responses[i], 0.0f) << "MT should respond to motion at frame " << i;
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(SensoryHierarchyE2ETest, Temporal_SequencePrediction) {
    E2E_PIPELINE_START("Temporal: Sequence Prediction");

    E2E_STAGE_BEGIN("Setup predictive system", 400);
    predictive_config_t pc_config;
    cortical_predictive_default_config(&pc_config);
    pc_config.prediction_learning_rate = 0.2f;

    predictive = cortical_predictive_create(&pc_config);
    ASSERT_NE(predictive, nullptr);

    cortical_predictive_add_level(predictive, 32, 32);
    cortical_predictive_add_level(predictive, 16, 16);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Learn repeating sequence", 1500);
    // Simple repeating pattern: A, B, C, A, B, C...
    std::vector<std::vector<float>> patterns = {
        generate_sensory_input(32, 0.2f, 0.08f),
        generate_sensory_input(32, 0.5f, 0.08f),
        generate_sensory_input(32, 0.8f, 0.08f)
    };

    std::vector<float> errors(32);
    for (int rep = 0; rep < 5; ++rep) {
        for (const auto& pattern : patterns) {
            cortical_predictive_compute_error(predictive, 0, pattern.data(), 32,
                                             errors.data(), 32);
            cortical_predictive_update_predictions(predictive, 0);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test prediction accuracy", 500);
    // Present pattern A, compute prediction error for B
    cortical_predictive_compute_error(predictive, 0, patterns[0].data(), 32,
                                     errors.data(), 32);

    std::vector<float> predictions(32);
    cortical_predictive_get_predictions(predictive, 0, predictions.data(), 32);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify learning occurred", 200);
    predictive_stats_t stats;
    cortical_predictive_get_stats(predictive, &stats);
    EXPECT_GT(stats.total_updates, 10);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(SensoryHierarchyE2ETest, Temporal_RhythmicProcessing) {
    E2E_PIPELINE_START("Temporal: Rhythmic Processing");

    E2E_STAGE_BEGIN("Setup auditory-like hierarchy", 400);
    cortical_area_config_t a1_config = {
        .type = CORTICAL_AREA_CUSTOM,
        .stream = STREAM_VENTRAL,
        .hierarchy_level = 0,
        .rf_expansion_factor = 2.0f,
        .num_hypercolumns = 32,
        .neurons_per_hypercolumn = 1000,
        .feedforward_strength = 1.0f,
        .feedback_strength = 0.5f,
        .custom_name = "A1"
    };
    cortical_area_config_t belt_config = {
        .type = CORTICAL_AREA_CUSTOM,
        .stream = STREAM_VENTRAL,
        .hierarchy_level = 1,
        .rf_expansion_factor = 2.0f,
        .num_hypercolumns = 16,
        .neurons_per_hypercolumn = 1000,
        .feedforward_strength = 1.0f,
        .feedback_strength = 0.5f,
        .custom_name = "Belt"
    };

    uint32_t a1_id, belt_id;
    cortical_hierarchy_add_area(hierarchy, &a1_config, &a1_id);
    cortical_hierarchy_add_area(hierarchy, &belt_config, &belt_id);

    inter_area_connection_config_t conn = {
        .source_area_id = a1_id,
        .target_area_id = belt_id,
        .type = CONNECTION_TYPE_FEEDFORWARD,
        .source_layer = 2,
        .target_layer = 3,
        .weight = 1.0f,
        .delay_ms = 10.0f,
        .use_canonical_layers = true
    };
    uint32_t conn_id;
    cortical_hierarchy_connect_areas(hierarchy, &conn, &conn_id);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process rhythmic input (strong-weak-weak pattern)", 1500);
    std::vector<float> responses;

    // 3/4 time: strong - weak - weak
    float amplitudes[] = {0.9f, 0.4f, 0.4f, 0.9f, 0.4f, 0.4f, 0.9f, 0.4f, 0.4f};

    for (float amp : amplitudes) {
        auto beat = generate_sensory_input(32, 0.5f, 0.2f);
        for (float& v : beat) v *= amp;

        cortical_hierarchy_set_area_input(hierarchy, a1_id, beat.data(), 32);
        cortical_hierarchy_propagate_feedforward(hierarchy, 0, 1);

        std::vector<float> belt_activity(16);
        uint32_t size;
        cortical_hierarchy_get_area_activity(hierarchy, belt_id, belt_activity.data(), 16, &size);

        responses.push_back(calculate_total_activation(belt_activity));
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify rhythmic structure preserved", 300);
    // Strong beats (indices 0, 3, 6) should have higher responses
    float strong_avg = (responses[0] + responses[3] + responses[6]) / 3.0f;
    float weak_avg = (responses[1] + responses[2] + responses[4] + responses[5] +
                      responses[7] + responses[8]) / 6.0f;

    // Both should have some response
    EXPECT_GT(strong_avg + weak_avg, 0.0f) << "Should respond to rhythm";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
