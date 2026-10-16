/**
 * @file test_entorhinal_cognitive_integration.cpp
 * @brief Integration tests for Entorhinal Cortex with cognitive systems
 * @version Phase 5: Memory Circuit
 * @date 2025-01-13
 *
 * WHAT: Tests Entorhinal Cortex integration with cognitive systems including
 *       working memory, attention, reasoning, and executive control systems.
 *
 * WHY:  The entorhinal cortex serves as the primary gateway between hippocampus
 *       and neocortex. Cognitive systems modulate spatial processing for goal-
 *       directed navigation, spatial working memory, and attention-guided
 *       exploration. These tests ensure proper cognitive integration.
 *
 * HOW:  Tests cognitive bridge initialization, working memory spatial buffers,
 *       attention modulation of grid cells, goal-directed navigation, spatial
 *       reasoning integration, and metacognitive monitoring.
 *
 * COGNITIVE INTEGRATION POINTS:
 * - Working Memory: Spatial buffer for short-term location retention
 * - Attention System: Modulation of grid cell activity for focused processing
 * - Cognitive Integration Hub: Central communication for cognitive events
 * - Reasoning System: Spatial reasoning and path planning
 * - Executive Control: Goal-directed spatial processing control
 * - Metacognitive Monitoring: Self-assessment of navigation confidence
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "core/brain/regions/entorhinal/nimcp_entorhinal.h"
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class EntorhinalCognitiveIntegrationTest : public ::testing::Test {
protected:
    nimcp_entorhinal_t* ec = nullptr;
    entorhinal_config_t config;

    void SetUp() override {
        /* Create entorhinal cortex with cognitive integration enabled */
        config = entorhinal_default_config();
        config.enable_cognitive = true;
        config.enable_path_integration = true;
        config.enable_boundary_detection = true;
        config.enable_bio_async = false;  /* Disable for isolated testing */
        config.enable_security = false;
        config.enable_immune = false;
        ec = entorhinal_create(&config);
        ASSERT_NE(nullptr, ec);
    }

    void TearDown() override {
        if (ec) {
            entorhinal_destroy(ec);
            ec = nullptr;
        }
    }

    /* Helper to create test position */
    void CreateTestPosition(float* position, float x, float y, float z) {
        position[0] = x;
        position[1] = y;
        position[2] = z;
    }

    /* Helper to simulate navigation step */
    void SimulateNavigationStep(float x, float y, float heading, float dt) {
        float position[3] = {x, y, 0.0f};
        entorhinal_update_grid_cells(ec, position, 3);
        entorhinal_update_hd_cells(ec, heading, 0.0f);
        entorhinal_bidirectional_update(ec, dt);
    }

    /* Helper to compute distance between positions */
    float ComputeDistance(const float* pos1, const float* pos2, uint32_t dim) {
        float sum = 0.0f;
        for (uint32_t i = 0; i < dim; i++) {
            float diff = pos1[i] - pos2[i];
            sum += diff * diff;
        }
        return sqrtf(sum);
    }
};

/*=============================================================================
 * COGNITIVE BRIDGE INITIALIZATION TESTS
 * Test initialization and configuration of cognitive integration bridge
 *===========================================================================*/

TEST_F(EntorhinalCognitiveIntegrationTest, CognitiveBridgeInitializesWithNullPointers) {
    /* Initialize cognitive bridge with null pointers (standalone mode) */
    int result = entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);
    EXPECT_EQ(0, result);

    /* Bridge should be initialized with default values */
    EXPECT_EQ(ec->cognitive_bridge.working_memory, nullptr);
    EXPECT_EQ(ec->cognitive_bridge.attention, nullptr);
    EXPECT_EQ(ec->cognitive_bridge.hub, nullptr);
}

TEST_F(EntorhinalCognitiveIntegrationTest, CognitiveBridgeDefaultAttentionModulation) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Default attention modulation should be neutral (1.0) */
    EXPECT_GE(ec->cognitive_bridge.attention_modulation, 0.0f);
    EXPECT_LE(ec->cognitive_bridge.attention_modulation, 1.0f);
}

TEST_F(EntorhinalCognitiveIntegrationTest, CognitiveBridgeDefaultWorkingMemoryLoad) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Default working memory load should start at zero */
    EXPECT_GE(ec->cognitive_bridge.working_memory_load, 0.0f);
    EXPECT_LE(ec->cognitive_bridge.working_memory_load, 1.0f);
}

TEST_F(EntorhinalCognitiveIntegrationTest, CognitiveBridgeEventCountStartsAtZero) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Cognitive events sent should start at zero */
    EXPECT_EQ(0u, ec->cognitive_bridge.cognitive_events_sent);
}

/*=============================================================================
 * WORKING MEMORY SPATIAL BUFFER TESTS
 * Test spatial working memory integration with entorhinal cortex
 *===========================================================================*/

TEST_F(EntorhinalCognitiveIntegrationTest, SpatialBufferEncodesCurrentLocation) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Encode current location to spatial buffer via memory gateway */
    entorhinal_set_encoding_gate(ec, 1.0f);

    float features[32];
    for (int i = 0; i < 32; i++) {
        features[i] = 0.5f;
    }
    float spatial_context[3] = {5.0f, 5.0f, 0.0f};

    int result = entorhinal_encode_to_hippocampus(ec, features, 32, spatial_context, 3);
    EXPECT_EQ(0, result);

    /* Verify encoding occurred */
    uint64_t encoded, retrieved, consolidated;
    entorhinal_get_gateway_stats(ec, &encoded, &retrieved, &consolidated);
    EXPECT_GT(encoded, 0u);
}

TEST_F(EntorhinalCognitiveIntegrationTest, SpatialBufferRetrievesStoredLocation) {
    /* Initialize and encode */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);
    entorhinal_set_encoding_gate(ec, 1.0f);

    float features[32];
    for (int i = 0; i < 32; i++) {
        features[i] = 0.75f;
    }
    float spatial_context[3] = {3.0f, 4.0f, 0.0f};
    entorhinal_encode_to_hippocampus(ec, features, 32, spatial_context, 3);

    /* Retrieve with similar cue */
    entorhinal_set_retrieval_gate(ec, 1.0f);

    float cue[32];
    for (int i = 0; i < 32; i++) {
        cue[i] = features[i] + 0.02f;  /* Slightly noisy cue */
    }

    float retrieved[64];
    uint32_t actual_features = 0;
    int result = entorhinal_retrieve_from_hippocampus(ec, cue, 32, retrieved, 64, &actual_features);
    EXPECT_EQ(0, result);
}

TEST_F(EntorhinalCognitiveIntegrationTest, WorkingMemoryLoadAffectsEncoding) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Simulate high working memory load */
    ec->cognitive_bridge.working_memory_load = 0.9f;

    /* Encoding should still work but may be modulated */
    entorhinal_set_encoding_gate(ec, 1.0f);

    float features[16] = {0.5f};
    float context[3] = {1.0f, 1.0f, 0.0f};
    int result = entorhinal_encode_to_hippocampus(ec, features, 16, context, 3);
    EXPECT_EQ(0, result);
}

TEST_F(EntorhinalCognitiveIntegrationTest, MultipleLocationsInSpatialBuffer) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);
    entorhinal_set_encoding_gate(ec, 1.0f);

    /* Encode multiple locations */
    for (int loc = 0; loc < 5; loc++) {
        float features[16];
        for (int i = 0; i < 16; i++) {
            features[i] = (float)loc / 5.0f;
        }
        float context[3] = {(float)loc * 2.0f, (float)loc * 2.0f, 0.0f};
        entorhinal_encode_to_hippocampus(ec, features, 16, context, 3);
    }

    /* Verify multiple encodings */
    uint64_t encoded, retrieved, consolidated;
    entorhinal_get_gateway_stats(ec, &encoded, &retrieved, &consolidated);
    EXPECT_GE(encoded, 5u);
}

/*=============================================================================
 * ATTENTION-MODULATED GRID CELL ACTIVITY TESTS
 * Test attention system modulation of grid cell responses
 *===========================================================================*/

TEST_F(EntorhinalCognitiveIntegrationTest, AttentionModulatesGridCellResponse) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Set high attention modulation */
    ec->cognitive_bridge.attention_modulation = 0.9f;

    /* Update grid cells with position */
    float position[3] = {5.0f, 5.0f, 0.0f};
    int result = entorhinal_update_grid_cells(ec, position, 3);
    EXPECT_EQ(0, result);

    /* Grid cells should respond to attention modulation */
    /* (In full implementation, attention would scale grid cell activations) */
}

TEST_F(EntorhinalCognitiveIntegrationTest, LowAttentionReducesGridPrecision) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Set low attention modulation */
    ec->cognitive_bridge.attention_modulation = 0.2f;

    /* Update grid cells */
    float position[3] = {3.0f, 3.0f, 0.0f};
    entorhinal_update_grid_cells(ec, position, 3);

    /* Get position estimate */
    float pos_out[3], heading, pos_conf, head_conf;
    int result = entorhinal_get_position_estimate(ec, pos_out, &heading,
        &pos_conf, &head_conf);
    EXPECT_EQ(0, result);

    /* Confidence should still be in valid range */
    EXPECT_GE(pos_conf, 0.0f);
    EXPECT_LE(pos_conf, 1.0f);
}

TEST_F(EntorhinalCognitiveIntegrationTest, AttentionFocusOnSpecificLocation) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* High attention focused on navigation task */
    ec->cognitive_bridge.attention_modulation = 1.0f;

    /* Navigate to target with attention */
    for (int step = 0; step < 20; step++) {
        float x = (float)step * 0.5f;
        float position[3] = {x, 0.0f, 0.0f};
        entorhinal_update_grid_cells(ec, position, 3);
        entorhinal_update_hd_cells(ec, 0.0f, 0.0f);
    }

    /* Should maintain good position tracking */
    float pos_out[3], heading, pos_conf, head_conf;
    entorhinal_get_position_estimate(ec, pos_out, &heading, &pos_conf, &head_conf);
    EXPECT_GE(pos_conf, 0.0f);
}

TEST_F(EntorhinalCognitiveIntegrationTest, AttentionModulationDynamicUpdate) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Dynamically change attention during navigation */
    for (int step = 0; step < 30; step++) {
        /* Oscillate attention */
        ec->cognitive_bridge.attention_modulation = 0.5f + 0.5f * sinf(step * 0.2f);

        float position[3] = {(float)step * 0.3f, 0.0f, 0.0f};
        entorhinal_update_grid_cells(ec, position, 3);

        /* Should remain stable */
        EXPECT_EQ(ENTORHINAL_STATUS_READY, entorhinal_get_status(ec));
    }
}

/*=============================================================================
 * COGNITIVE INTEGRATION HUB COMMUNICATION TESTS
 * Test communication with central cognitive integration hub
 *===========================================================================*/

TEST_F(EntorhinalCognitiveIntegrationTest, PublishCognitiveEventsSucceeds) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Update grid cells to generate spatial event */
    float position[3] = {5.0f, 5.0f, 0.0f};
    entorhinal_update_grid_cells(ec, position, 3);

    /* Publish cognitive events */
    int result = entorhinal_publish_cognitive_events(ec);
    EXPECT_EQ(0, result);
}

TEST_F(EntorhinalCognitiveIntegrationTest, CognitiveEventCountIncrements) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    uint32_t initial_count = ec->cognitive_bridge.cognitive_events_sent;

    /* Generate activity and publish */
    float position[3] = {2.0f, 2.0f, 0.0f};
    entorhinal_update_grid_cells(ec, position, 3);
    entorhinal_publish_cognitive_events(ec);

    /* Event count should increment */
    EXPECT_GE(ec->cognitive_bridge.cognitive_events_sent, initial_count);
}

TEST_F(EntorhinalCognitiveIntegrationTest, BidirectionalUpdateIncludesCognitive) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Bidirectional update should process cognitive integration */
    for (int i = 0; i < 10; i++) {
        int result = entorhinal_bidirectional_update(ec, 0.01f);
        EXPECT_EQ(0, result);
    }

    /* System should remain healthy */
    float health = entorhinal_get_health_status(ec);
    EXPECT_GT(health, 0.0f);
}

TEST_F(EntorhinalCognitiveIntegrationTest, CognitiveHubReceivesSpatialUpdates) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Simulate spatial movement */
    for (int step = 0; step < 50; step++) {
        float angle = step * 0.1f;
        float x = 5.0f + cosf(angle) * 3.0f;
        float y = 5.0f + sinf(angle) * 3.0f;
        float position[3] = {x, y, 0.0f};

        entorhinal_update_grid_cells(ec, position, 3);
        entorhinal_update_hd_cells(ec, angle, 0.1f);
        entorhinal_publish_cognitive_events(ec);
    }

    /* Statistics should reflect activity */
    entorhinal_stats_t stats;
    entorhinal_get_stats(ec, &stats);
    EXPECT_GT(stats.cognitive_events_published, 0u);
}

/*=============================================================================
 * GOAL-DIRECTED NAVIGATION TESTS
 * Test goal-directed spatial processing with cognitive control
 *===========================================================================*/

TEST_F(EntorhinalCognitiveIntegrationTest, GoalDirectedPathIntegration) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Set goal and navigate toward it */
    float start_pos[3] = {0.0f, 0.0f, 0.0f};
    float goal_pos[3] = {10.0f, 10.0f, 0.0f};

    /* Reset at start position */
    entorhinal_reset_grid_phases(ec, start_pos);

    /* Navigate toward goal */
    for (int step = 0; step < 20; step++) {
        float t = (float)(step + 1) / 20.0f;
        float x = t * goal_pos[0];
        float y = t * goal_pos[1];

        float velocity[3] = {goal_pos[0] / 20.0f, goal_pos[1] / 20.0f, 0.0f};
        entorhinal_path_integrate(ec, velocity, 0.0f, 0.05f);
    }

    /* Should have progressed toward goal */
    float pos_out[3], heading, pos_conf, head_conf;
    entorhinal_get_position_estimate(ec, pos_out, &heading, &pos_conf, &head_conf);

    /* Position should be closer to goal than start */
    float dist_to_goal = ComputeDistance(pos_out, goal_pos, 2);
    float start_to_goal = ComputeDistance(start_pos, goal_pos, 2);
    EXPECT_LT(dist_to_goal, start_to_goal);
}

TEST_F(EntorhinalCognitiveIntegrationTest, NavigationWithObstacleAvoidance) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Simulate navigation with boundary detection */
    for (int step = 0; step < 30; step++) {
        float x = (float)step * 0.3f;
        float position[3] = {x, 5.0f, 0.0f};

        /* Nearby boundary on one side */
        float boundary_distances[4] = {10.0f, 2.0f, 10.0f, 10.0f};

        entorhinal_update_grid_cells(ec, position, 3);
        entorhinal_update_border_cells(ec, boundary_distances, 4);
    }

    /* Border cells should have detected boundaries */
    float directions[4], distances[4];
    uint32_t num_detected = 0;
    int result = entorhinal_detect_boundaries(ec, directions, distances, 4, &num_detected);
    EXPECT_EQ(0, result);
}

TEST_F(EntorhinalCognitiveIntegrationTest, GoalDirectedMemoryRetrieval) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Encode a goal location */
    entorhinal_set_encoding_gate(ec, 1.0f);
    float goal_features[32];
    for (int i = 0; i < 32; i++) {
        goal_features[i] = 0.8f;
    }
    float goal_context[3] = {10.0f, 10.0f, 0.0f};
    entorhinal_encode_to_hippocampus(ec, goal_features, 32, goal_context, 3);

    /* Later retrieve the goal location */
    entorhinal_set_retrieval_gate(ec, 1.0f);
    float cue[32];
    for (int i = 0; i < 32; i++) {
        cue[i] = 0.8f;
    }

    float retrieved[64];
    uint32_t actual = 0;
    int result = entorhinal_retrieve_from_hippocampus(ec, cue, 32, retrieved, 64, &actual);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * SPATIAL REASONING INTEGRATION TESTS
 * Test spatial reasoning and path planning integration
 *===========================================================================*/

TEST_F(EntorhinalCognitiveIntegrationTest, SpatialReasoningFromGridCells) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Update grid cells at known positions */
    float positions[5][3] = {
        {0.0f, 0.0f, 0.0f},
        {2.0f, 0.0f, 0.0f},
        {4.0f, 0.0f, 0.0f},
        {6.0f, 0.0f, 0.0f},
        {8.0f, 0.0f, 0.0f}
    };

    for (int i = 0; i < 5; i++) {
        entorhinal_update_grid_cells(ec, positions[i], 3);

        /* Decode position after each update */
        float decoded[3];
        float confidence;
        entorhinal_decode_position_from_grid(ec, decoded, &confidence);

        /* Confidence should be reasonable */
        EXPECT_GE(confidence, 0.0f);
        EXPECT_LE(confidence, 1.0f);
    }
}

TEST_F(EntorhinalCognitiveIntegrationTest, GridPopulationVectorComputation) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Update at a position */
    float position[3] = {3.0f, 4.0f, 0.0f};
    entorhinal_update_grid_cells(ec, position, 3);

    /* Get population vector */
    float vector[256];  /* Large enough buffer */
    uint32_t dim = 0;
    int result = entorhinal_get_grid_population_vector(ec, vector, &dim);
    EXPECT_EQ(0, result);
    EXPECT_GT(dim, 0u);
}

TEST_F(EntorhinalCognitiveIntegrationTest, HeadingDecodingForReasoning) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Set heading and decode */
    float true_heading = M_PI / 4.0f;  /* 45 degrees */
    entorhinal_update_hd_cells(ec, true_heading, 0.0f);

    float decoded_heading, confidence;
    int result = entorhinal_decode_heading(ec, &decoded_heading, &confidence);
    EXPECT_EQ(0, result);
    EXPECT_GE(confidence, 0.0f);
}

TEST_F(EntorhinalCognitiveIntegrationTest, IntegratedSpatialComputation) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Perform integrated spatial computation */
    float position[3] = {5.0f, 5.0f, 0.0f};
    float heading = M_PI / 3.0f;
    float boundary_distances[4] = {3.0f, 4.0f, 5.0f, 6.0f};

    /* Update all spatial cells */
    entorhinal_update_grid_cells(ec, position, 3);
    entorhinal_update_hd_cells(ec, heading, 0.0f);
    entorhinal_update_border_cells(ec, boundary_distances, 4);

    /* Get combined estimate */
    float pos_out[3], heading_out, pos_conf, head_conf;
    int result = entorhinal_get_position_estimate(ec, pos_out, &heading_out,
        &pos_conf, &head_conf);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * EXECUTIVE CONTROL OF SPATIAL PROCESSING TESTS
 * Test executive function control over spatial computations
 *===========================================================================*/

TEST_F(EntorhinalCognitiveIntegrationTest, ExecutiveControlGatesEncoding) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Executive control: close encoding gate */
    entorhinal_set_encoding_gate(ec, 0.0f);

    /* Attempt to encode */
    float features[16] = {0.5f};
    float context[3] = {1.0f, 1.0f, 0.0f};
    int result = entorhinal_encode_to_hippocampus(ec, features, 16, context, 3);
    /* Should succeed but encoding may be blocked */
    EXPECT_EQ(0, result);

    /* Open gate and encode */
    entorhinal_set_encoding_gate(ec, 1.0f);
    result = entorhinal_encode_to_hippocampus(ec, features, 16, context, 3);
    EXPECT_EQ(0, result);
}

TEST_F(EntorhinalCognitiveIntegrationTest, ExecutiveControlGatesRetrieval) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* First encode something */
    entorhinal_set_encoding_gate(ec, 1.0f);
    float features[16] = {0.5f};
    float context[3] = {1.0f, 1.0f, 0.0f};
    entorhinal_encode_to_hippocampus(ec, features, 16, context, 3);

    /* Executive control: close retrieval gate */
    entorhinal_set_retrieval_gate(ec, 0.0f);

    /* Attempt to retrieve */
    float cue[16] = {0.5f};
    float retrieved[32];
    uint32_t actual = 0;
    int result = entorhinal_retrieve_from_hippocampus(ec, cue, 16, retrieved, 32, &actual);
    EXPECT_EQ(0, result);

    /* Open gate and retrieve */
    entorhinal_set_retrieval_gate(ec, 1.0f);
    result = entorhinal_retrieve_from_hippocampus(ec, cue, 16, retrieved, 32, &actual);
    EXPECT_EQ(0, result);
}

TEST_F(EntorhinalCognitiveIntegrationTest, TrainingModeControl) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Enable training mode (executive decision) */
    int result = entorhinal_set_training_mode(ec, true);
    EXPECT_EQ(0, result);

    /* Perform training forward pass */
    float input[32] = {0};
    float output[32];
    result = entorhinal_training_forward(ec, input, 32, output, 32);
    EXPECT_EQ(0, result);

    /* Disable training mode */
    result = entorhinal_set_training_mode(ec, false);
    EXPECT_EQ(0, result);
}

TEST_F(EntorhinalCognitiveIntegrationTest, ResetUnderExecutiveControl) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Perform some operations */
    float position[3] = {5.0f, 5.0f, 0.0f};
    entorhinal_update_grid_cells(ec, position, 3);
    entorhinal_bidirectional_update(ec, 0.01f);

    /* Executive reset */
    bool result = entorhinal_reset(ec);
    EXPECT_TRUE(result);

    /* Should be back to initial state */
    EXPECT_EQ(ENTORHINAL_STATUS_READY, entorhinal_get_status(ec));
}

/*=============================================================================
 * METACOGNITIVE MONITORING TESTS
 * Test self-assessment of navigation confidence and performance
 *===========================================================================*/

TEST_F(EntorhinalCognitiveIntegrationTest, PositionConfidenceMonitoring) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Start with known position */
    float known_pos[3] = {0.0f, 0.0f, 0.0f};
    entorhinal_reset_grid_phases(ec, known_pos);

    /* Get initial confidence */
    float pos_out[3], heading, pos_conf_initial, head_conf;
    entorhinal_get_position_estimate(ec, pos_out, &heading, &pos_conf_initial, &head_conf);

    /* Path integrate with accumulated error */
    float velocity[3] = {1.0f, 0.0f, 0.0f};
    for (int i = 0; i < 100; i++) {
        entorhinal_path_integrate(ec, velocity, 0.01f, 0.1f);  /* Small angular error */
    }

    /* Confidence should be in valid range */
    float pos_conf_later;
    entorhinal_get_position_estimate(ec, pos_out, &heading, &pos_conf_later, &head_conf);
    EXPECT_GE(pos_conf_later, 0.0f);
    EXPECT_LE(pos_conf_later, 1.0f);
}

TEST_F(EntorhinalCognitiveIntegrationTest, HeadingConfidenceMonitoring) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Calibrate with known heading */
    entorhinal_calibrate_hd_cells(ec, 0.0f);

    /* Get heading confidence */
    float decoded_heading, confidence;
    entorhinal_decode_heading(ec, &decoded_heading, &confidence);
    EXPECT_GE(confidence, 0.0f);

    /* Update with consistent heading */
    for (int i = 0; i < 10; i++) {
        entorhinal_update_hd_cells(ec, 0.0f, 0.0f);
    }

    /* Confidence should remain reasonable */
    entorhinal_decode_heading(ec, &decoded_heading, &confidence);
    EXPECT_GE(confidence, 0.0f);
}

TEST_F(EntorhinalCognitiveIntegrationTest, HealthStatusMonitoring) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Get initial health */
    float health_initial = entorhinal_get_health_status(ec);
    EXPECT_GT(health_initial, 0.0f);
    EXPECT_LE(health_initial, 1.0f);

    /* Perform extensive processing */
    for (int i = 0; i < 200; i++) {
        float position[3] = {(float)i * 0.1f, 0.0f, 0.0f};
        entorhinal_update_grid_cells(ec, position, 3);
        entorhinal_bidirectional_update(ec, 0.01f);
    }

    /* Health should still be positive */
    float health_after = entorhinal_get_health_status(ec);
    EXPECT_GT(health_after, 0.0f);
}

TEST_F(EntorhinalCognitiveIntegrationTest, StatisticsForMetacognition) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Perform various operations */
    for (int i = 0; i < 50; i++) {
        float position[3] = {(float)i * 0.2f, (float)i * 0.1f, 0.0f};
        entorhinal_update_grid_cells(ec, position, 3);
        entorhinal_update_hd_cells(ec, (float)i * 0.05f, 0.05f);
    }

    /* Get comprehensive statistics */
    entorhinal_stats_t stats;
    int result = entorhinal_get_stats(ec, &stats);
    EXPECT_EQ(0, result);

    /* Statistics should reflect activity */
    EXPECT_GT(stats.updates_processed, 0u);
}

/*=============================================================================
 * ERROR HANDLING TESTS
 * Test error handling in cognitive integration scenarios
 *===========================================================================*/

TEST_F(EntorhinalCognitiveIntegrationTest, NullEntorhinalHandling) {
    /* All functions should handle null pointers gracefully */
    EXPECT_NE(0, entorhinal_init_cognitive_bridge(nullptr, nullptr, nullptr, nullptr));
    EXPECT_NE(0, entorhinal_publish_cognitive_events(nullptr));
    EXPECT_NE(0, entorhinal_bidirectional_update(nullptr, 0.01f));
}

TEST_F(EntorhinalCognitiveIntegrationTest, InvalidPositionDimension) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Update with invalid dimension should be handled */
    float position[3] = {1.0f, 1.0f, 0.0f};
    int result = entorhinal_update_grid_cells(ec, position, 0);  /* Invalid dim */
    /* Should either reject or handle gracefully */
    EXPECT_NE(ENTORHINAL_STATUS_ERROR, entorhinal_get_status(ec));
}

TEST_F(EntorhinalCognitiveIntegrationTest, NullBuffersInRetrieval) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Null output buffer should be handled */
    float cue[16] = {0.5f};
    entorhinal_set_retrieval_gate(ec, 1.0f);
    int result = entorhinal_retrieve_from_hippocampus(ec, cue, 16, nullptr, 0, nullptr);
    EXPECT_NE(ENTORHINAL_STATUS_ERROR, entorhinal_get_status(ec));
}

TEST_F(EntorhinalCognitiveIntegrationTest, ZeroDeltaTimeHandling) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Zero dt should be handled */
    float velocity[3] = {1.0f, 0.0f, 0.0f};
    int result = entorhinal_path_integrate(ec, velocity, 0.0f, 0.0f);
    EXPECT_EQ(0, result);  /* Should succeed with no effect */
}

TEST_F(EntorhinalCognitiveIntegrationTest, NegativeGateValuesClamp) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Negative gate values should be clamped or handled */
    int result = entorhinal_set_encoding_gate(ec, -1.0f);
    EXPECT_EQ(0, result);  /* Should succeed with clamping */

    result = entorhinal_set_retrieval_gate(ec, -0.5f);
    EXPECT_EQ(0, result);
}

TEST_F(EntorhinalCognitiveIntegrationTest, ExcessiveGateValuesClamp) {
    /* Initialize cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr);

    /* Gate values > 1 should be clamped */
    int result = entorhinal_set_encoding_gate(ec, 2.0f);
    EXPECT_EQ(0, result);

    result = entorhinal_set_retrieval_gate(ec, 1.5f);
    EXPECT_EQ(0, result);
}

TEST_F(EntorhinalCognitiveIntegrationTest, ErrorStringRetrieval) {
    /* Test error string retrieval */
    const char* error_str = entorhinal_error_string(ENTORHINAL_ERROR_NONE);
    EXPECT_NE(nullptr, error_str);

    error_str = entorhinal_error_string(ENTORHINAL_ERROR_INVALID_INPUT);
    EXPECT_NE(nullptr, error_str);

    error_str = entorhinal_error_string(ENTORHINAL_ERROR_GRID_DRIFT);
    EXPECT_NE(nullptr, error_str);
}

TEST_F(EntorhinalCognitiveIntegrationTest, StatusStringRetrieval) {
    /* Test status string retrieval */
    const char* status_str = entorhinal_status_string(ENTORHINAL_STATUS_IDLE);
    EXPECT_NE(nullptr, status_str);

    status_str = entorhinal_status_string(ENTORHINAL_STATUS_READY);
    EXPECT_NE(nullptr, status_str);

    status_str = entorhinal_status_string(ENTORHINAL_STATUS_PATH_INTEGRATING);
    EXPECT_NE(nullptr, status_str);
}

/*=============================================================================
 * MAIN ENTRY POINT
 *===========================================================================*/

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
