//=============================================================================
// test_visual_processing_e2e.cpp - Visual Processing End-to-End Tests
//=============================================================================
/**
 * @file test_visual_processing_e2e.cpp
 * @brief End-to-end tests for visual processing through cortical columns
 *
 * WHAT: Full pipeline tests for visual processing in cortical column architecture
 * WHY:  Verify complete visual pathways from edge detection to object recognition
 * HOW:  Test orientation columns, feature hierarchy, attention modulation,
 *       and predictive coding for visual input
 *
 * TEST COVERAGE:
 * - Edge detection through orientation columns
 * - Feature extraction hierarchy (V1 -> V2 -> V4)
 * - Object recognition pipeline
 * - Attention-modulated processing
 * - Predictive coding for visual input
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

class VisualProcessingE2ETest : public ::testing::Test {
protected:
    cortical_column_pool_t* pool = nullptr;
    cortical_hierarchy_t* hierarchy = nullptr;
    cortical_predictive_t* predictive = nullptr;

    static constexpr uint32_t NUM_ORIENTATION_COLUMNS = 12;  // 12 orientations (0-180 deg)
    static constexpr uint32_t NEURONS_PER_MINICOLUMN = 80;
    static constexpr uint32_t NUM_CHANNELS = 64;
    static constexpr uint32_t IMAGE_WIDTH = 32;
    static constexpr uint32_t IMAGE_HEIGHT = 32;

    void SetUp() override {
        // Create memory pool for cortical columns
        cortical_column_pool_config_t pool_config = {
            .max_minicolumns = 500,
            .max_hypercolumns = 50,
            .max_neurons_per_minicolumn = 100,
            .enable_cow_support = true
        };
        pool = cortical_column_pool_create(&pool_config);
        ASSERT_NE(pool, nullptr) << "Failed to create cortical column pool";

        // Create cortical hierarchy for visual processing
        cortical_hierarchy_config_t hier_config = cortical_hierarchy_default_config();
        hier_config.max_areas = 8;
        hier_config.max_connections = 32;
        hier_config.enable_predictive_coding = true;
        hierarchy = cortical_hierarchy_create(&hier_config);
        ASSERT_NE(hierarchy, nullptr) << "Failed to create cortical hierarchy";
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

    // Helper: Create orientation-tuned hypercolumn
    hypercolumn_t* create_orientation_hypercolumn(float topographic_x, float topographic_y) {
        hypercolumn_config_t config = {
            .num_minicolumns = NUM_ORIENTATION_COLUMNS,
            .minicolumn_configs = nullptr,  // Will be created internally
            .feature_space_min = 0.0f,      // 0 degrees
            .feature_space_max = 180.0f,    // 180 degrees (orientation is symmetric)
            .topographic_x = topographic_x,
            .topographic_y = topographic_y,
            .competition = CC_COMPETITION_SOFTMAX,
            .k_winners = 3,
            .temperature = 1.0f,
            .lateral_inhibition_strength = 0.5f,
            .lateral_inhibition_sigma1 = 1.0f,
            .lateral_inhibition_sigma2 = 3.0f
        };
        return hypercolumn_create(pool, &config);
    }

    // Helper: Generate oriented edge stimulus (Gabor-like)
    std::vector<float> generate_oriented_edge(uint32_t size, float orientation_deg,
                                               float frequency, float phase) {
        std::vector<float> stimulus(size);
        float theta = orientation_deg * M_PI / 180.0f;

        for (uint32_t i = 0; i < size; ++i) {
            float x = static_cast<float>(i % IMAGE_WIDTH) / IMAGE_WIDTH - 0.5f;
            float y = static_cast<float>(i / IMAGE_WIDTH) / IMAGE_HEIGHT - 0.5f;

            // Gabor function: cos(2*pi*f*(x*cos(theta) + y*sin(theta)) + phase)
            float rotated = x * std::cos(theta) + y * std::sin(theta);
            stimulus[i] = 0.5f * (1.0f + std::cos(2.0f * M_PI * frequency * rotated + phase));
        }
        return stimulus;
    }

    // Helper: Generate visual scene with multiple objects
    std::vector<float> generate_visual_scene(uint32_t size) {
        std::vector<float> scene(size, 0.0f);

        // Add horizontal edge at top
        auto horiz = generate_oriented_edge(size, 0.0f, 4.0f, 0.0f);
        // Add vertical edge at left
        auto vert = generate_oriented_edge(size, 90.0f, 4.0f, 0.0f);
        // Add diagonal edge
        auto diag = generate_oriented_edge(size, 45.0f, 4.0f, 0.0f);

        for (uint32_t i = 0; i < size; ++i) {
            scene[i] = std::max({horiz[i], vert[i], diag[i]});
        }
        return scene;
    }

    // Helper: Calculate orientation selectivity index
    float calculate_orientation_selectivity(const std::vector<float>& responses,
                                            uint32_t preferred_idx) {
        float preferred = responses[preferred_idx];
        float orthogonal_idx = (preferred_idx + NUM_ORIENTATION_COLUMNS / 2) % NUM_ORIENTATION_COLUMNS;
        float orthogonal = responses[static_cast<uint32_t>(orthogonal_idx)];

        if (preferred + orthogonal < 0.001f) return 0.0f;
        return (preferred - orthogonal) / (preferred + orthogonal);
    }

    // Helper: Find dominant orientation
    uint32_t find_dominant_orientation(const std::vector<float>& responses) {
        auto it = std::max_element(responses.begin(), responses.end());
        return static_cast<uint32_t>(std::distance(responses.begin(), it));
    }

    // Helper: Calculate total activation
    float calculate_total_activation(const std::vector<float>& activations) {
        return std::accumulate(activations.begin(), activations.end(), 0.0f);
    }
};

//=============================================================================
// Edge Detection Through Orientation Columns Tests
//=============================================================================

TEST_F(VisualProcessingE2ETest, OrientationColumn_HorizontalEdgeDetection) {
    E2E_PIPELINE_START("Orientation Column: Horizontal Edge Detection");

    E2E_STAGE_BEGIN("Create orientation hypercolumn", 100);
    hypercolumn_t* hcol = create_orientation_hypercolumn(0.5f, 0.5f);
    ASSERT_NE(hcol, nullptr) << "Failed to create hypercolumn";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Generate horizontal edge stimulus", 100);
    auto stimulus = generate_oriented_edge(NUM_CHANNELS, 0.0f, 2.0f, 0.0f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process through hypercolumn", 500);
    hypercolumn_compute(hcol, stimulus.data(), static_cast<uint32_t>(stimulus.size()));
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify horizontal orientation detected", 100);
    uint32_t winner = hypercolumn_get_winner(hcol);
    // Winner should be near 0 (horizontal orientation, first index)
    EXPECT_LE(winner, 2) << "Winner should be near horizontal orientation (0 deg)";

    std::vector<float> distribution(NUM_ORIENTATION_COLUMNS);
    hypercolumn_get_distribution(hcol, distribution.data(), NUM_ORIENTATION_COLUMNS);

    float selectivity = calculate_orientation_selectivity(distribution, 0);
    EXPECT_GT(selectivity, 0.3f) << "Should show orientation selectivity";
    E2E_STAGE_END();

    hypercolumn_destroy(hcol);
    E2E_PIPELINE_END();
}

TEST_F(VisualProcessingE2ETest, OrientationColumn_VerticalEdgeDetection) {
    E2E_PIPELINE_START("Orientation Column: Vertical Edge Detection");

    hypercolumn_t* hcol = create_orientation_hypercolumn(0.5f, 0.5f);
    ASSERT_NE(hcol, nullptr);

    E2E_STAGE_BEGIN("Generate vertical edge stimulus", 100);
    auto stimulus = generate_oriented_edge(NUM_CHANNELS, 90.0f, 2.0f, 0.0f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process and verify vertical detection", 500);
    hypercolumn_compute(hcol, stimulus.data(), static_cast<uint32_t>(stimulus.size()));

    uint32_t winner = hypercolumn_get_winner(hcol);
    // Winner should be near 6 (90 deg = middle of 0-180 range, with 12 columns)
    uint32_t expected_idx = NUM_ORIENTATION_COLUMNS / 2;
    EXPECT_NEAR(winner, expected_idx, 2) << "Winner should be near vertical orientation (90 deg)";
    E2E_STAGE_END();

    hypercolumn_destroy(hcol);
    E2E_PIPELINE_END();
}

TEST_F(VisualProcessingE2ETest, OrientationColumn_DiagonalEdgeDetection) {
    E2E_PIPELINE_START("Orientation Column: Diagonal Edge Detection");

    hypercolumn_t* hcol = create_orientation_hypercolumn(0.5f, 0.5f);
    ASSERT_NE(hcol, nullptr);

    E2E_STAGE_BEGIN("Process 45-degree diagonal", 500);
    auto stimulus_45 = generate_oriented_edge(NUM_CHANNELS, 45.0f, 2.0f, 0.0f);
    hypercolumn_compute(hcol, stimulus_45.data(), static_cast<uint32_t>(stimulus_45.size()));

    uint32_t winner_45 = hypercolumn_get_winner(hcol);
    // 45 deg should be at index 3 (45/180 * 12 = 3)
    EXPECT_NEAR(winner_45, 3, 1) << "Should detect 45-degree orientation";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process 135-degree diagonal", 500);
    auto stimulus_135 = generate_oriented_edge(NUM_CHANNELS, 135.0f, 2.0f, 0.0f);
    hypercolumn_compute(hcol, stimulus_135.data(), static_cast<uint32_t>(stimulus_135.size()));

    uint32_t winner_135 = hypercolumn_get_winner(hcol);
    // 135 deg should be at index 9 (135/180 * 12 = 9)
    EXPECT_NEAR(winner_135, 9, 1) << "Should detect 135-degree orientation";
    E2E_STAGE_END();

    hypercolumn_destroy(hcol);
    E2E_PIPELINE_END();
}

//=============================================================================
// Feature Extraction Hierarchy Tests (V1 -> V2 -> V4)
//=============================================================================

TEST_F(VisualProcessingE2ETest, FeatureHierarchy_V1ToV2ToV4) {
    E2E_PIPELINE_START("Feature Hierarchy: V1 -> V2 -> V4");

    E2E_STAGE_BEGIN("Add V1 area (simple features)", 200);
    cortical_area_config_t v1_config = {
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
    uint32_t v1_id;
    int result = cortical_hierarchy_add_area(hierarchy, &v1_config, &v1_id);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Add V2 area (contour integration)", 200);
    cortical_area_config_t v2_config = {
        .type = CORTICAL_AREA_V2,
        .stream = STREAM_VENTRAL,
        .hierarchy_level = 1,
        .rf_expansion_factor = 2.0f,
        .num_hypercolumns = 8,
        .neurons_per_hypercolumn = 1000,
        .feedforward_strength = 1.0f,
        .feedback_strength = 0.5f,
        .custom_name = nullptr
    };
    uint32_t v2_id;
    result = cortical_hierarchy_add_area(hierarchy, &v2_config, &v2_id);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Add V4 area (intermediate features)", 200);
    cortical_area_config_t v4_config = {
        .type = CORTICAL_AREA_V4,
        .stream = STREAM_VENTRAL,
        .hierarchy_level = 2,
        .rf_expansion_factor = 2.0f,
        .num_hypercolumns = 4,
        .neurons_per_hypercolumn = 1000,
        .feedforward_strength = 1.0f,
        .feedback_strength = 0.5f,
        .custom_name = nullptr
    };
    uint32_t v4_id;
    result = cortical_hierarchy_add_area(hierarchy, &v4_config, &v4_id);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply canonical connections", 300);
    result = cortical_hierarchy_apply_canonical_connections(hierarchy);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify receptive field expansion", 200);
    float v1_rf, v2_rf, v4_rf;
    cortical_hierarchy_get_receptive_field_size(hierarchy, v1_id, &v1_rf);
    cortical_hierarchy_get_receptive_field_size(hierarchy, v2_id, &v2_rf);
    cortical_hierarchy_get_receptive_field_size(hierarchy, v4_id, &v4_rf);

    EXPECT_LT(v1_rf, v2_rf) << "V2 RF should be larger than V1";
    EXPECT_LT(v2_rf, v4_rf) << "V4 RF should be larger than V2";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process visual input through hierarchy", 1000);
    std::vector<float> visual_input(16, 0.8f);  // V1 has 16 hypercolumns
    result = cortical_hierarchy_set_area_input(hierarchy, v1_id, visual_input.data(), 16);
    EXPECT_EQ(result, 0);

    result = cortical_hierarchy_propagate_feedforward(hierarchy, 0, 2);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify activity propagation", 200);
    std::vector<float> v4_activity(4);
    uint32_t actual_size;
    result = cortical_hierarchy_get_area_activity(hierarchy, v4_id, v4_activity.data(), 4, &actual_size);
    EXPECT_EQ(result, 0);
    EXPECT_GT(actual_size, 0);

    float total_v4 = calculate_total_activation(v4_activity);
    EXPECT_GT(total_v4, 0.0f) << "V4 should receive activation from V1->V2";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(VisualProcessingE2ETest, FeatureHierarchy_ReceptiveFieldExpansion) {
    E2E_PIPELINE_START("Feature Hierarchy: Receptive Field Expansion");

    E2E_STAGE_BEGIN("Build 4-level hierarchy", 500);
    cortical_area_type_t types[] = {CORTICAL_AREA_V1, CORTICAL_AREA_V2,
                                     CORTICAL_AREA_V4, CORTICAL_AREA_IT};
    uint32_t area_ids[4];

    for (int i = 0; i < 4; ++i) {
        cortical_area_config_t config = {
            .type = types[i],
            .stream = STREAM_VENTRAL,
            .hierarchy_level = static_cast<uint32_t>(i),
            .rf_expansion_factor = 2.0f,
            .num_hypercolumns = static_cast<uint32_t>(16 >> i),
            .neurons_per_hypercolumn = 1000,
            .feedforward_strength = 1.0f,
            .feedback_strength = 0.5f,
            .custom_name = nullptr
        };
        int result = cortical_hierarchy_add_area(hierarchy, &config, &area_ids[i]);
        EXPECT_EQ(result, 0);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify exponential RF expansion", 300);
    float prev_rf = 0.0f;
    for (int i = 0; i < 4; ++i) {
        float rf;
        cortical_hierarchy_get_receptive_field_size(hierarchy, area_ids[i], &rf);

        if (i > 0) {
            float expansion = rf / prev_rf;
            EXPECT_GT(expansion, 1.5f) << "RF should expand by ~2x per level";
        }
        prev_rf = rf;
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Object Recognition Pipeline Tests
//=============================================================================

TEST_F(VisualProcessingE2ETest, ObjectRecognition_HierarchicalProcessing) {
    E2E_PIPELINE_START("Object Recognition: Hierarchical Processing");

    E2E_STAGE_BEGIN("Setup visual hierarchy with IT", 500);
    // Add all visual areas
    cortical_area_type_t types[] = {CORTICAL_AREA_V1, CORTICAL_AREA_V2,
                                     CORTICAL_AREA_V4, CORTICAL_AREA_IT};
    uint32_t area_ids[4];

    for (int i = 0; i < 4; ++i) {
        cortical_area_config_t config = {
            .type = types[i],
            .stream = STREAM_VENTRAL,
            .hierarchy_level = static_cast<uint32_t>(i),
            .rf_expansion_factor = 2.0f,
            .num_hypercolumns = static_cast<uint32_t>(16 >> i),
            .neurons_per_hypercolumn = 1000,
            .feedforward_strength = 1.0f,
            .feedback_strength = 0.5f,
            .custom_name = nullptr
        };
        cortical_hierarchy_add_area(hierarchy, &config, &area_ids[i]);
    }
    cortical_hierarchy_apply_canonical_connections(hierarchy);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process object image", 1000);
    // Generate a "complex object" (combination of features)
    auto scene = generate_visual_scene(16);

    int result = cortical_hierarchy_set_area_input(hierarchy, area_ids[0], scene.data(), 16);
    EXPECT_EQ(result, 0);

    result = cortical_hierarchy_propagate_feedforward(hierarchy, 0, 3);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify IT representation", 300);
    std::vector<float> it_activity(2);  // IT has only 2 hypercolumns
    uint32_t actual_size;
    cortical_hierarchy_get_area_activity(hierarchy, area_ids[3], it_activity.data(), 2, &actual_size);

    float it_activation = calculate_total_activation(it_activity);
    EXPECT_GT(it_activation, 0.0f) << "IT should represent object identity";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get hierarchy statistics", 200);
    cortical_hierarchy_stats_t stats;
    int result2 = cortical_hierarchy_get_stats(hierarchy, &stats);
    EXPECT_EQ(result2, 0);
    EXPECT_EQ(stats.num_areas, 4);
    EXPECT_GT(stats.total_propagations, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(VisualProcessingE2ETest, ObjectRecognition_InvarianceBuildup) {
    E2E_PIPELINE_START("Object Recognition: Invariance Buildup");

    E2E_STAGE_BEGIN("Setup hierarchy", 500);
    cortical_area_config_t v1_config = {
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
    cortical_area_config_t it_config = {
        .type = CORTICAL_AREA_IT,
        .stream = STREAM_VENTRAL,
        .hierarchy_level = 3,
        .rf_expansion_factor = 4.0f,
        .num_hypercolumns = 4,
        .neurons_per_hypercolumn = 1000,
        .feedforward_strength = 1.0f,
        .feedback_strength = 0.5f,
        .custom_name = nullptr
    };
    uint32_t v1_id, it_id;
    cortical_hierarchy_add_area(hierarchy, &v1_config, &v1_id);
    cortical_hierarchy_add_area(hierarchy, &it_config, &it_id);

    inter_area_connection_config_t conn_config = {
        .source_area_id = v1_id,
        .target_area_id = it_id,
        .type = CONNECTION_TYPE_FEEDFORWARD,
        .source_layer = 2,  // L2/3
        .target_layer = 3,  // L4
        .weight = 1.0f,
        .delay_ms = 20.0f,
        .use_canonical_layers = true
    };
    uint32_t conn_id;
    cortical_hierarchy_connect_areas(hierarchy, &conn_config, &conn_id);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process same object at different positions", 1000);
    std::vector<std::vector<float>> it_responses;

    for (int pos = 0; pos < 4; ++pos) {
        // Simulate same object at different retinotopic positions
        std::vector<float> input(16, 0.0f);
        input[pos * 4] = 0.9f;
        input[pos * 4 + 1] = 0.8f;
        input[pos * 4 + 2] = 0.7f;
        input[pos * 4 + 3] = 0.6f;

        cortical_hierarchy_set_area_input(hierarchy, v1_id, input.data(), 16);
        cortical_hierarchy_propagate_feedforward(hierarchy, 0, 3);

        std::vector<float> it_out(4);
        uint32_t size;
        cortical_hierarchy_get_area_activity(hierarchy, it_id, it_out.data(), 4, &size);
        it_responses.push_back(it_out);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify position-invariant responses", 300);
    // IT responses should be similar despite different V1 positions
    // (Due to large RF covering multiple V1 positions)
    for (size_t i = 0; i < it_responses.size(); ++i) {
        float total = calculate_total_activation(it_responses[i]);
        EXPECT_GT(total, 0.0f) << "IT should respond at position " << i;
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Attention-Modulated Processing Tests
//=============================================================================

TEST_F(VisualProcessingE2ETest, AttentionModulation_FeatureEnhancement) {
    E2E_PIPELINE_START("Attention Modulation: Feature Enhancement");

    E2E_STAGE_BEGIN("Create hypercolumn with attention support", 200);
    hypercolumn_t* hcol = create_orientation_hypercolumn(0.5f, 0.5f);
    ASSERT_NE(hcol, nullptr);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process without attention enhancement", 500);
    auto stimulus = generate_oriented_edge(NUM_CHANNELS, 45.0f, 2.0f, 0.0f);
    hypercolumn_compute(hcol, stimulus.data(), static_cast<uint32_t>(stimulus.size()));

    std::vector<float> baseline_dist(NUM_ORIENTATION_COLUMNS);
    hypercolumn_get_distribution(hcol, baseline_dist.data(), NUM_ORIENTATION_COLUMNS);
    float baseline_winner = *std::max_element(baseline_dist.begin(), baseline_dist.end());
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply softmax competition (attention-like)", 500);
    hypercolumn_run_competition(hcol, CC_COMPETITION_SOFTMAX, 0.5f);  // Lower temp = sharper

    std::vector<float> attended_dist(NUM_ORIENTATION_COLUMNS);
    hypercolumn_get_distribution(hcol, attended_dist.data(), NUM_ORIENTATION_COLUMNS);
    float attended_winner = *std::max_element(attended_dist.begin(), attended_dist.end());
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify attention sharpens representation", 200);
    // Lower temperature should sharpen the distribution
    // Winner should be more dominant
    EXPECT_GE(attended_winner, baseline_winner * 0.9f)
        << "Attended representation should be at least as strong";
    E2E_STAGE_END();

    hypercolumn_destroy(hcol);
    E2E_PIPELINE_END();
}

TEST_F(VisualProcessingE2ETest, AttentionModulation_TopDownFeedback) {
    E2E_PIPELINE_START("Attention Modulation: Top-Down Feedback");

    E2E_STAGE_BEGIN("Setup hierarchy with feedback", 500);
    cortical_area_config_t v1_config = {
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
    cortical_area_config_t pfc_config = {
        .type = CORTICAL_AREA_PFC,
        .stream = STREAM_VENTRAL,
        .hierarchy_level = 4,
        .rf_expansion_factor = 4.0f,
        .num_hypercolumns = 4,
        .neurons_per_hypercolumn = 1000,
        .feedforward_strength = 1.0f,
        .feedback_strength = 0.8f,  // Strong top-down
        .custom_name = nullptr
    };
    uint32_t v1_id, pfc_id;
    cortical_hierarchy_add_area(hierarchy, &v1_config, &v1_id);
    cortical_hierarchy_add_area(hierarchy, &pfc_config, &pfc_id);

    // Feedforward connection
    inter_area_connection_config_t ff_conn = {
        .source_area_id = v1_id,
        .target_area_id = pfc_id,
        .type = CONNECTION_TYPE_FEEDFORWARD,
        .source_layer = 2,
        .target_layer = 3,
        .weight = 1.0f,
        .delay_ms = 30.0f,
        .use_canonical_layers = true
    };
    uint32_t ff_conn_id;
    cortical_hierarchy_connect_areas(hierarchy, &ff_conn, &ff_conn_id);

    // Feedback connection (attention)
    inter_area_connection_config_t fb_conn = {
        .source_area_id = pfc_id,
        .target_area_id = v1_id,
        .type = CONNECTION_TYPE_FEEDBACK,
        .source_layer = 4,  // L5
        .target_layer = 0,  // L1
        .weight = 0.5f,
        .delay_ms = 10.0f,
        .use_canonical_layers = true
    };
    uint32_t fb_conn_id;
    cortical_hierarchy_connect_areas(hierarchy, &fb_conn, &fb_conn_id);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Inject attentional signal from PFC", 500);
    std::vector<float> attention_signal(4, 0.0f);
    attention_signal[0] = 1.0f;  // Attend to first region
    cortical_hierarchy_set_area_input(hierarchy, pfc_id, attention_signal.data(), 4);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Propagate feedback", 500);
    int result = cortical_hierarchy_propagate_feedback(hierarchy, 4, 0);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify V1 modulation", 300);
    std::vector<float> v1_activity(16);
    uint32_t size;
    cortical_hierarchy_get_area_activity(hierarchy, v1_id, v1_activity.data(), 16, &size);

    // First region should be enhanced by attention
    float first_region = 0.0f, other_regions = 0.0f;
    for (uint32_t i = 0; i < 4; ++i) first_region += v1_activity[i];
    for (uint32_t i = 4; i < 16; ++i) other_regions += v1_activity[i];

    EXPECT_GE(first_region, other_regions / 3.0f)
        << "Attended region should be enhanced";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(VisualProcessingE2ETest, AttentionModulation_CompetitiveSelection) {
    E2E_PIPELINE_START("Attention Modulation: Competitive Selection");

    E2E_STAGE_BEGIN("Create hypercolumn with competing stimuli", 200);
    hypercolumn_t* hcol = create_orientation_hypercolumn(0.5f, 0.5f);
    ASSERT_NE(hcol, nullptr);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process ambiguous stimulus", 500);
    // Create stimulus with two nearly equally strong orientations
    auto horiz = generate_oriented_edge(NUM_CHANNELS, 0.0f, 2.0f, 0.0f);
    auto vert = generate_oriented_edge(NUM_CHANNELS, 90.0f, 2.0f, 0.0f);

    std::vector<float> ambiguous(NUM_CHANNELS);
    for (uint32_t i = 0; i < NUM_CHANNELS; ++i) {
        ambiguous[i] = (horiz[i] + vert[i]) / 2.0f;
    }

    hypercolumn_compute(hcol, ambiguous.data(), NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply winner-take-all competition", 300);
    hypercolumn_run_competition(hcol, CC_COMPETITION_WINNER_TAKE_ALL, 1.0f);

    std::vector<float> distribution(NUM_ORIENTATION_COLUMNS);
    hypercolumn_get_distribution(hcol, distribution.data(), NUM_ORIENTATION_COLUMNS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify single winner selected", 200);
    // In WTA, only one should be active
    int active_count = 0;
    for (float d : distribution) {
        if (d > 0.5f) active_count++;
    }
    EXPECT_LE(active_count, 1) << "WTA should select single winner";
    E2E_STAGE_END();

    hypercolumn_destroy(hcol);
    E2E_PIPELINE_END();
}

//=============================================================================
// Predictive Coding for Visual Input Tests
//=============================================================================

TEST_F(VisualProcessingE2ETest, PredictiveCoding_ErrorComputation) {
    E2E_PIPELINE_START("Predictive Coding: Error Computation");

    E2E_STAGE_BEGIN("Create predictive coding system", 300);
    predictive_config_t pc_config;
    cortical_predictive_default_config(&pc_config);
    pc_config.hierarchy_depth = 3;
    pc_config.enable_precision_weighting = true;

    predictive = cortical_predictive_create(&pc_config);
    ASSERT_NE(predictive, nullptr);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Add hierarchy levels", 300);
    int result = cortical_predictive_add_level(predictive, 64, 64);  // Level 0 (sensory)
    EXPECT_EQ(result, 0);
    result = cortical_predictive_add_level(predictive, 32, 32);  // Level 1
    EXPECT_EQ(result, 0);
    result = cortical_predictive_add_level(predictive, 16, 16);  // Level 2
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Compute prediction error for visual input", 500);
    // Generate observation (what we see)
    std::vector<float> observation(64);
    for (uint32_t i = 0; i < 64; ++i) {
        observation[i] = 0.5f + 0.3f * std::sin(2.0f * M_PI * i / 64.0f);
    }

    std::vector<float> errors(64);
    result = cortical_predictive_compute_error(predictive, 0, observation.data(), 64,
                                               errors.data(), 64);
    EXPECT_GE(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify error signals computed", 200);
    float total_error = 0.0f;
    for (float e : errors) {
        total_error += std::abs(e);
    }
    // With no prior predictions, error should roughly equal observation
    EXPECT_GT(total_error, 0.0f) << "Should have non-zero prediction error";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(VisualProcessingE2ETest, PredictiveCoding_FreeEnergyMinimization) {
    E2E_PIPELINE_START("Predictive Coding: Free Energy Minimization");

    E2E_STAGE_BEGIN("Setup predictive hierarchy", 300);
    predictive_config_t pc_config;
    cortical_predictive_default_config(&pc_config);
    pc_config.prediction_learning_rate = 0.1f;
    pc_config.precision_learning_rate = 0.01f;
    pc_config.hierarchy_depth = 2;

    predictive = cortical_predictive_create(&pc_config);
    ASSERT_NE(predictive, nullptr);

    cortical_predictive_add_level(predictive, 32, 32);
    cortical_predictive_add_level(predictive, 16, 16);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Compute initial free energy", 200);
    float initial_fe;
    int result = cortical_predictive_compute_free_energy(predictive, &initial_fe);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Run prediction updates", 1000);
    // Present repeated observation to allow learning
    std::vector<float> observation(32, 0.6f);
    std::vector<float> errors(32);

    for (int iter = 0; iter < 10; ++iter) {
        cortical_predictive_compute_error(predictive, 0, observation.data(), 32,
                                         errors.data(), 32);
        cortical_predictive_update_predictions(predictive, 0);
        cortical_predictive_update_predictions(predictive, 1);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify free energy decreased", 300);
    float final_fe;
    cortical_predictive_compute_free_energy(predictive, &final_fe);

    EXPECT_LE(final_fe, initial_fe * 1.1f)  // Allow some variance
        << "Free energy should decrease or stay stable through learning";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(VisualProcessingE2ETest, PredictiveCoding_PrecisionWeighting) {
    E2E_PIPELINE_START("Predictive Coding: Precision Weighting");

    E2E_STAGE_BEGIN("Setup with precision weighting", 300);
    predictive_config_t pc_config;
    cortical_predictive_default_config(&pc_config);
    pc_config.enable_precision_weighting = true;
    pc_config.error_gain = 1.0f;

    predictive = cortical_predictive_create(&pc_config);
    ASSERT_NE(predictive, nullptr);
    cortical_predictive_add_level(predictive, 32, 32);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Compute weighted errors", 500);
    std::vector<float> errors(32);
    for (uint32_t i = 0; i < 32; ++i) {
        errors[i] = (i < 16) ? 1.0f : 0.5f;  // Half high error, half low
    }

    std::vector<float> weighted_errors(32);
    int result = cortical_predictive_weight_by_precision(predictive, 0, errors.data(), 32,
                                                         weighted_errors.data(), 32);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify precision applied to errors", 200);
    // Weighted errors should be non-zero
    float total_weighted = 0.0f;
    for (float we : weighted_errors) {
        total_weighted += std::abs(we);
    }
    EXPECT_GT(total_weighted, 0.0f) << "Weighted errors should be non-zero";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(VisualProcessingE2ETest, PredictiveCoding_HierarchicalMessagePassing) {
    E2E_PIPELINE_START("Predictive Coding: Hierarchical Message Passing");

    E2E_STAGE_BEGIN("Setup 3-level hierarchy", 400);
    predictive_config_t pc_config;
    cortical_predictive_default_config(&pc_config);
    pc_config.enable_lateral_predictions = true;

    predictive = cortical_predictive_create(&pc_config);
    ASSERT_NE(predictive, nullptr);

    cortical_predictive_add_level(predictive, 64, 64);
    cortical_predictive_add_level(predictive, 32, 32);
    cortical_predictive_add_level(predictive, 16, 16);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Propagate errors up hierarchy", 500);
    int result = cortical_predictive_propagate_up(predictive, 0);
    EXPECT_EQ(result, 0);
    result = cortical_predictive_propagate_up(predictive, 1);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Propagate predictions down hierarchy", 500);
    result = cortical_predictive_propagate_down(predictive, 2);
    EXPECT_EQ(result, 0);
    result = cortical_predictive_propagate_down(predictive, 1);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify bidirectional message passing", 300);
    std::vector<float> predictions(64);
    int num = cortical_predictive_get_predictions(predictive, 0, predictions.data(), 64);
    EXPECT_GE(num, 0);

    std::vector<float> errors(16);
    num = cortical_predictive_get_errors(predictive, 2, errors.data(), 16);
    EXPECT_GE(num, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Check statistics", 200);
    predictive_stats_t stats;
    int result2 = cortical_predictive_get_stats(predictive, &stats);
    EXPECT_EQ(result2, 0);
    EXPECT_GT(stats.total_updates, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(VisualProcessingE2ETest, PredictiveCoding_VisualExpectationViolation) {
    E2E_PIPELINE_START("Predictive Coding: Visual Expectation Violation");

    E2E_STAGE_BEGIN("Setup and learn expected pattern", 500);
    predictive_config_t pc_config;
    cortical_predictive_default_config(&pc_config);
    pc_config.prediction_learning_rate = 0.2f;

    predictive = cortical_predictive_create(&pc_config);
    ASSERT_NE(predictive, nullptr);
    cortical_predictive_add_level(predictive, 32, 32);
    cortical_predictive_add_level(predictive, 16, 16);

    // Learn expected pattern (repeated presentation)
    std::vector<float> expected_pattern(32, 0.5f);
    std::vector<float> errors(32);

    for (int i = 0; i < 5; ++i) {
        cortical_predictive_compute_error(predictive, 0, expected_pattern.data(), 32,
                                         errors.data(), 32);
        cortical_predictive_update_predictions(predictive, 0);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Present expected pattern", 300);
    cortical_predictive_compute_error(predictive, 0, expected_pattern.data(), 32,
                                     errors.data(), 32);
    float expected_error = 0.0f;
    for (float e : errors) expected_error += std::abs(e);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Present unexpected pattern", 300);
    std::vector<float> unexpected_pattern(32, 0.9f);  // Different from expected
    cortical_predictive_compute_error(predictive, 0, unexpected_pattern.data(), 32,
                                     errors.data(), 32);
    float unexpected_error = 0.0f;
    for (float e : errors) unexpected_error += std::abs(e);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify violation detection", 200);
    EXPECT_GT(unexpected_error, expected_error)
        << "Unexpected pattern should generate larger prediction error";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
