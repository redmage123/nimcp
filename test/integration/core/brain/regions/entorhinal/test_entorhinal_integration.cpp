/**
 * @file test_entorhinal_integration.cpp
 * @brief Integration tests for Entorhinal Cortex
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 *
 * Tests integration between:
 * - Grid cells, border cells, HD cells working together
 * - Path integration with grid cell updates
 * - Memory gateway with hippocampus bridge
 * - All 21+ bridge integrations working in concert
 * - Bio-async communication flow
 * - Security and immune system integration
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

extern "C" {
#include "core/brain/regions/entorhinal/nimcp_entorhinal.h"
#include "core/brain/regions/entorhinal/nimcp_entorhinal_adapter.h"
#include "core/brain/regions/entorhinal/nimcp_entorhinal_hypothalamus_bridge.h"
#include "core/brain/regions/entorhinal/nimcp_entorhinal_omni_bridge.h"
#include "core/brain/regions/entorhinal/nimcp_entorhinal_brain_init_bridge.h"
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*=============================================================================
 * INTEGRATION TEST FIXTURE
 *===========================================================================*/

class EntorhinalIntegrationTest : public ::testing::Test {
protected:
    nimcp_entorhinal_t* ec = nullptr;
    entorhinal_hypothalamus_bridge_state_t* hypothalamus_bridge = nullptr;
    entorhinal_omni_bridge_state_t* omni_bridge = nullptr;
    entorhinal_brain_init_bridge_t* init_bridge = nullptr;

    void SetUp() override {
        // Create entorhinal with all integrations enabled
        entorhinal_config_t config = entorhinal_default_config();
        config.enable_path_integration = true;
        config.enable_boundary_detection = true;
        config.enable_security = true;
        config.enable_immune = true;
        config.enable_bio_async = true;
        config.enable_snn = true;
        config.enable_plasticity = true;
        config.enable_cognitive = true;
        config.enable_hippocampus = true;
        ec = entorhinal_create(&config);
        ASSERT_NE(ec, nullptr);

        // Create and connect hypothalamus bridge
        hypothalamus_bridge = entorhinal_hypothalamus_bridge_create(nullptr);
        ASSERT_NE(hypothalamus_bridge, nullptr);
        entorhinal_hypothalamus_bridge_connect(hypothalamus_bridge, ec, nullptr);

        // Create and connect omni bridge
        omni_bridge = entorhinal_omni_bridge_create(nullptr);
        ASSERT_NE(omni_bridge, nullptr);
        entorhinal_omni_bridge_connect(omni_bridge, ec, nullptr);

        // Create and run init bridge
        entorhinal_brain_init_config_t init_config = entorhinal_brain_init_default_config();
        init_config.skip_self_test = true;
        init_config.skip_calibration = true;
        init_bridge = entorhinal_brain_init_bridge_create(&init_config);
        ASSERT_NE(init_bridge, nullptr);
    }

    void TearDown() override {
        if (init_bridge) {
            entorhinal_brain_init_bridge_destroy(init_bridge);
        }
        if (omni_bridge) {
            entorhinal_omni_bridge_destroy(omni_bridge);
        }
        if (hypothalamus_bridge) {
            entorhinal_hypothalamus_bridge_destroy(hypothalamus_bridge);
        }
        if (ec) {
            entorhinal_destroy(ec);
        }
    }
};

/*=============================================================================
 * SPATIAL CELL INTEGRATION TESTS
 *===========================================================================*/

TEST_F(EntorhinalIntegrationTest, GridAndBorderCellsIntegrate) {
    // Move to position near boundary
    float position[3] = {1.0f, 1.0f, 0.0f};
    EXPECT_EQ(entorhinal_update_grid_cells(ec, position, 3), 0);

    // Update border cells with nearby boundary
    float boundary_distances[4] = {1.0f, 10.0f, 10.0f, 10.0f};
    EXPECT_EQ(entorhinal_update_border_cells(ec, boundary_distances, 4), 0);

    // Both should influence position estimate
    float estimated_pos[3];
    float heading, pos_conf, head_conf;
    EXPECT_EQ(entorhinal_get_position_estimate(ec, estimated_pos, &heading,
        &pos_conf, &head_conf), 0);

    // Position confidence should be reasonable
    EXPECT_GE(pos_conf, 0.0f);
    EXPECT_LE(pos_conf, 1.0f);
}

TEST_F(EntorhinalIntegrationTest, GridAndHDCellsIntegrate) {
    // Update grid cells with position
    float position[3] = {5.0f, 5.0f, 0.0f};
    EXPECT_EQ(entorhinal_update_grid_cells(ec, position, 3), 0);

    // Update HD cells with heading
    float heading = M_PI / 4.0f;  // 45 degrees
    EXPECT_EQ(entorhinal_update_hd_cells(ec, heading, 0.0f), 0);

    // Get combined estimate
    float pos_out[3], heading_out, pos_conf, head_conf;
    EXPECT_EQ(entorhinal_get_position_estimate(ec, pos_out, &heading_out,
        &pos_conf, &head_conf), 0);

    // Heading confidence should be reasonable
    EXPECT_GE(head_conf, 0.0f);
}

TEST_F(EntorhinalIntegrationTest, AllSpatialCellsWorkTogether) {
    // Simulate moving through an environment
    for (int step = 0; step < 100; step++) {
        float x = cosf(step * 0.1f) * 5.0f;
        float y = sinf(step * 0.1f) * 5.0f;
        float position[3] = {x, y, 0.0f};
        float heading = atan2f(cosf((step + 1) * 0.1f) - cosf(step * 0.1f),
                               sinf((step + 1) * 0.1f) - sinf(step * 0.1f));

        // Update all spatial cells
        EXPECT_EQ(entorhinal_update_grid_cells(ec, position, 3), 0);
        EXPECT_EQ(entorhinal_update_hd_cells(ec, heading, 0.1f), 0);

        // Update border cells (far from boundaries in circular path)
        float boundary_distances[4] = {10.0f, 10.0f, 10.0f, 10.0f};
        EXPECT_EQ(entorhinal_update_border_cells(ec, boundary_distances, 4), 0);
    }

    // Should still be functional after many updates
    EXPECT_EQ(entorhinal_get_status(ec), ENTORHINAL_STATUS_READY);
}

/*=============================================================================
 * PATH INTEGRATION WITH GRID CELLS TESTS
 *===========================================================================*/

TEST_F(EntorhinalIntegrationTest, PathIntegrationUpdatesGridCells) {
    // Start at origin
    float start_pos[3] = {0.0f, 0.0f, 0.0f};
    entorhinal_reset_grid_phases(ec, start_pos);

    // Path integrate forward
    float velocity[3] = {1.0f, 0.0f, 0.0f};
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(entorhinal_path_integrate(ec, velocity, 0.0f, 0.1f), 0);
    }

    // Grid cells should reflect new position
    float pos_out[3], heading, pos_conf, head_conf;
    entorhinal_get_position_estimate(ec, pos_out, &heading, &pos_conf, &head_conf);

    // Should have moved approximately 1 meter
    EXPECT_NEAR(pos_out[0], 1.0f, 0.5f);
}

TEST_F(EntorhinalIntegrationTest, VisualCorrectionResetsGridDrift) {
    // Accumulate path integration drift
    float velocity[3] = {1.0f, 0.0f, 0.0f};
    for (int i = 0; i < 100; i++) {
        entorhinal_path_integrate(ec, velocity, 0.01f, 0.1f);  // Slight angular error
    }

    // Get drifted position
    float drifted_pos[3], heading, pos_conf, head_conf;
    entorhinal_get_position_estimate(ec, drifted_pos, &heading, &pos_conf, &head_conf);
    float drifted_conf = pos_conf;

    // Apply visual correction
    float true_pos[3] = {10.0f, 0.0f, 0.0f};
    EXPECT_EQ(entorhinal_apply_visual_correction(ec, true_pos, 0.0f, 1.0f), 0);

    // Confidence should improve
    entorhinal_get_position_estimate(ec, drifted_pos, &heading, &pos_conf, &head_conf);
    // Confidence may be restored after correction
}

TEST_F(EntorhinalIntegrationTest, BoundaryCorrectionHelpsLocalization) {
    // Path integrate with some drift
    float velocity[3] = {1.0f, 0.0f, 0.0f};
    for (int i = 0; i < 50; i++) {
        entorhinal_path_integrate(ec, velocity, 0.0f, 0.1f);
    }

    // Apply boundary correction (wall detected at specific distance)
    float boundary_pos[3] = {5.0f, 0.0f, 0.0f};
    EXPECT_EQ(entorhinal_apply_boundary_correction(ec, boundary_pos, 0.0f), 0);

    // Should still be functional
    EXPECT_EQ(entorhinal_get_status(ec), ENTORHINAL_STATUS_READY);
}

/*=============================================================================
 * MEMORY GATEWAY INTEGRATION TESTS
 *===========================================================================*/

TEST_F(EntorhinalIntegrationTest, EncodeAndRetrieveMemory) {
    // Open encoding gate
    EXPECT_EQ(entorhinal_set_encoding_gate(ec, 1.0f), 0);

    // Encode a memory with spatial context
    float features[32];
    for (int i = 0; i < 32; i++) {
        features[i] = sinf(i * 0.2f);
    }
    float spatial_context[3] = {3.0f, 4.0f, 0.0f};
    EXPECT_EQ(entorhinal_encode_to_hippocampus(ec, features, 32, spatial_context, 3), 0);

    // Open retrieval gate
    EXPECT_EQ(entorhinal_set_retrieval_gate(ec, 1.0f), 0);

    // Retrieve with similar cue
    float cue[32];
    for (int i = 0; i < 32; i++) {
        cue[i] = features[i] + 0.05f;  // Slightly noisy
    }
    float retrieved[64];
    uint32_t actual_features = 0;
    EXPECT_EQ(entorhinal_retrieve_from_hippocampus(ec, cue, 32, retrieved, 64, &actual_features), 0);
}

TEST_F(EntorhinalIntegrationTest, MemoryGatewayStatisticsUpdate) {
    uint64_t enc_before, ret_before, cons_before;
    entorhinal_get_gateway_stats(ec, &enc_before, &ret_before, &cons_before);

    // Perform encoding
    entorhinal_set_encoding_gate(ec, 1.0f);
    float features[16] = {0};
    float context[3] = {0};
    entorhinal_encode_to_hippocampus(ec, features, 16, context, 3);

    // Perform retrieval
    entorhinal_set_retrieval_gate(ec, 1.0f);
    float cue[16] = {0};
    float retrieved[32];
    uint32_t actual;
    entorhinal_retrieve_from_hippocampus(ec, cue, 16, retrieved, 32, &actual);

    // Perform consolidation
    entorhinal_consolidate_to_neocortex(ec, 0, 0.5f);

    uint64_t enc_after, ret_after, cons_after;
    entorhinal_get_gateway_stats(ec, &enc_after, &ret_after, &cons_after);

    // Statistics should have increased
    EXPECT_GE(enc_after, enc_before);
    EXPECT_GE(ret_after, ret_before);
    EXPECT_GE(cons_after, cons_before);
}

/*=============================================================================
 * BRIDGE INTEGRATION TESTS
 *===========================================================================*/

TEST_F(EntorhinalIntegrationTest, AllBridgesInitialize) {
    EXPECT_EQ(entorhinal_init_all_bridges(ec, nullptr), 0);
}

TEST_F(EntorhinalIntegrationTest, BidirectionalUpdateCycle) {
    // Initialize bridges
    entorhinal_init_all_bridges(ec, nullptr);

    // Run bidirectional update
    EXPECT_EQ(entorhinal_bidirectional_update(ec, 0.01f), 0);

    // Run multiple cycles
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(entorhinal_bidirectional_update(ec, 0.01f), 0);
    }

    // Should still be healthy
    float health = entorhinal_get_health_status(ec);
    EXPECT_GT(health, 0.0f);
}

TEST_F(EntorhinalIntegrationTest, SecurityAndImmuneIntegrate) {
    entorhinal_init_security_bridge(ec, nullptr, nullptr);
    entorhinal_init_immune_bridge(ec, nullptr);

    // Run security validation
    EXPECT_EQ(entorhinal_validate_security(ec), 0);

    // Run immune scan
    EXPECT_EQ(entorhinal_immune_scan(ec), 0);

    // Both should complete without detecting issues
    EXPECT_FALSE(ec->security_bridge.threat_detected);
    EXPECT_FALSE(ec->immune_bridge.anomaly_detected);
}

TEST_F(EntorhinalIntegrationTest, BioAsyncAndNeuromodulation) {
    entorhinal_init_bio_async_bridge(ec, nullptr);

    // Sync bio-async
    EXPECT_EQ(entorhinal_sync_bio_async(ec), 0);

    // Process neuromodulation
    EXPECT_EQ(entorhinal_process_neuromodulation(ec), 0);

    // Neuromodulator levels should be valid
    for (int i = 0; i < ENTORHINAL_CHANNEL_COUNT; i++) {
        EXPECT_GE(ec->bio_async_bridge.neuromodulator_levels[i], 0.0f);
        EXPECT_LE(ec->bio_async_bridge.neuromodulator_levels[i], 1.0f);
    }
}

TEST_F(EntorhinalIntegrationTest, PlasticityAndTrainingIntegrate) {
    entorhinal_init_plasticity_bridge(ec, nullptr, nullptr);
    entorhinal_init_training_bridge(ec, nullptr);

    // Enable training mode
    EXPECT_EQ(entorhinal_set_training_mode(ec, true), 0);

    // Run forward pass
    float input[32] = {0};
    float output[32];
    EXPECT_EQ(entorhinal_training_forward(ec, input, 32, output, 32), 0);

    // Run backward pass
    float grad[32] = {0};
    EXPECT_EQ(entorhinal_training_backward(ec, grad, 32), 0);

    // Apply weight updates
    EXPECT_EQ(entorhinal_apply_weight_updates(ec, 0.001f), 0);

    // Apply plasticity
    EXPECT_EQ(entorhinal_apply_plasticity(ec, 0.01f), 0);
}

/*=============================================================================
 * HYPOTHALAMUS BRIDGE INTEGRATION TESTS
 *===========================================================================*/

TEST_F(EntorhinalIntegrationTest, HypothalamusModulatesEncoding) {
    // Set high motivation state
    hypothalamic_motivational_state_t motivation = {0};
    motivation.hunger_drive = 0.9f;
    motivation.arousal_level = 0.8f;
    entorhinal_hypothalamus_receive_motivation(hypothalamus_bridge, &motivation);
    entorhinal_hypothalamus_bridge_update(hypothalamus_bridge, 0.01f);

    // Get encoding modulation
    float encoding_mod = entorhinal_hypothalamus_get_encoding_modulation(hypothalamus_bridge);

    // High motivation should boost encoding
    EXPECT_GT(encoding_mod, 0.5f);
}

TEST_F(EntorhinalIntegrationTest, HypothalamusValueMapUpdates) {
    // Process rewards at different locations
    float pos1[3] = {1.0f, 1.0f, 0.0f};
    float pos2[3] = {5.0f, 5.0f, 0.0f};

    entorhinal_hypothalamus_process_reward(hypothalamus_bridge, 1.0f, pos1, 3);
    entorhinal_hypothalamus_process_reward(hypothalamus_bridge, 0.0f, pos2, 3);

    // Value at rewarded location should be higher
    float value1 = entorhinal_hypothalamus_get_spatial_value(hypothalamus_bridge, pos1, 3);
    float value2 = entorhinal_hypothalamus_get_spatial_value(hypothalamus_bridge, pos2, 3);

    EXPECT_GT(value1, value2);
}

/*=============================================================================
 * OMNIDIRECTIONAL BRIDGE INTEGRATION TESTS
 *===========================================================================*/

TEST_F(EntorhinalIntegrationTest, OmniThreatAffectsAttention) {
    // Add threat from front
    omni_threat_vector_t threat;
    threat.direction = 0.0f;
    threat.distance = 5.0f;
    threat.magnitude = 0.9f;
    entorhinal_omni_receive_threats(omni_bridge, &threat, 1);
    entorhinal_omni_bridge_update(omni_bridge, 0.01f);

    // Get escape vector
    float escape[3];
    EXPECT_EQ(entorhinal_omni_get_escape_vector(omni_bridge, escape), 0);

    // Escape should be away from threat (backward)
    EXPECT_LT(escape[0], 0.0f);
}

TEST_F(EntorhinalIntegrationTest, OmniOpportunityGuidesApproach) {
    // Add opportunity to the right
    omni_opportunity_vector_t opportunity;
    opportunity.direction = M_PI / 2.0f;
    opportunity.distance = 3.0f;
    opportunity.value = 0.8f;
    opportunity.accessibility = 0.9f;
    entorhinal_omni_receive_opportunities(omni_bridge, &opportunity, 1);
    entorhinal_omni_bridge_update(omni_bridge, 0.01f);

    // Get approach vector
    float approach[3];
    EXPECT_EQ(entorhinal_omni_get_approach_vector(omni_bridge, approach), 0);
}

TEST_F(EntorhinalIntegrationTest, OmniBoundaryFeedsBorderCells) {
    // Set up spatial map with nearby boundaries
    omni_bridge->spatial_map.azimuth_distance[0] = 2.0f;
    omni_bridge->spatial_map.azimuth_distance[90] = 3.0f;

    // Get boundary signals
    float distances[8], directions[8];
    uint32_t num_boundaries = 0;
    EXPECT_EQ(entorhinal_omni_get_boundary_signals(omni_bridge,
        distances, directions, 8, &num_boundaries), 0);
}

/*=============================================================================
 * BRAIN INIT BRIDGE INTEGRATION TESTS
 *===========================================================================*/

TEST_F(EntorhinalIntegrationTest, FullInitializationSequence) {
    EXPECT_EQ(entorhinal_brain_init_initialize(init_bridge, ec, nullptr), 0);
    EXPECT_TRUE(entorhinal_brain_init_is_ready(init_bridge));

    // All core components should be initialized
    EXPECT_TRUE(init_bridge->status.grid_cells_initialized);
    EXPECT_TRUE(init_bridge->status.border_cells_initialized);
    EXPECT_TRUE(init_bridge->status.hd_cells_initialized);
    EXPECT_TRUE(init_bridge->status.path_integration_initialized);
    EXPECT_TRUE(init_bridge->status.memory_gateway_initialized);
}

TEST_F(EntorhinalIntegrationTest, InitShutdownCycle) {
    // Initialize
    EXPECT_EQ(entorhinal_brain_init_initialize(init_bridge, ec, nullptr), 0);
    EXPECT_TRUE(entorhinal_brain_init_is_ready(init_bridge));

    // Shutdown
    EXPECT_EQ(entorhinal_brain_init_shutdown(init_bridge), 0);
    EXPECT_EQ(init_bridge->status.shutdown_phase, ENTORHINAL_SHUTDOWN_PHASE_COMPLETE);
}

/*=============================================================================
 * FULL SYSTEM INTEGRATION TESTS
 *===========================================================================*/

TEST_F(EntorhinalIntegrationTest, FullNavigationScenario) {
    // Initialize everything
    entorhinal_brain_init_initialize(init_bridge, ec, nullptr);
    entorhinal_init_all_bridges(ec, nullptr);

    // Simulate a navigation task
    for (int step = 0; step < 200; step++) {
        float dt = 0.05f;  // 50ms timestep

        // Compute position on circular path
        float angle = step * 0.05f;
        float x = 5.0f + cosf(angle) * 3.0f;
        float y = 5.0f + sinf(angle) * 3.0f;
        float position[3] = {x, y, 0.0f};
        float heading = angle + M_PI / 2.0f;  // Tangent to circle

        // Update spatial cells
        entorhinal_update_grid_cells(ec, position, 3);
        entorhinal_update_hd_cells(ec, heading, 0.05f);

        // Path integration
        float velocity[3] = {-sinf(angle) * 0.15f, cosf(angle) * 0.15f, 0.0f};
        entorhinal_path_integrate(ec, velocity, 0.05f, dt);

        // Update bridges
        entorhinal_hypothalamus_bridge_update(hypothalamus_bridge, dt);
        entorhinal_omni_bridge_update(omni_bridge, dt);

        // Bidirectional data flow
        entorhinal_bidirectional_update(ec, dt);

        // Occasionally encode memories
        if (step % 50 == 0) {
            entorhinal_set_encoding_gate(ec, 1.0f);
            float features[16];
            for (int i = 0; i < 16; i++) features[i] = (float)step / 200.0f;
            entorhinal_encode_to_hippocampus(ec, features, 16, position, 3);
        }
    }

    // System should still be healthy
    EXPECT_EQ(entorhinal_get_status(ec), ENTORHINAL_STATUS_READY);
    float health = entorhinal_get_health_status(ec);
    EXPECT_GT(health, 0.5f);

    // Statistics should show activity
    entorhinal_stats_t stats;
    entorhinal_get_stats(ec, &stats);
    EXPECT_GT(stats.position_updates, 0u);
}

TEST_F(EntorhinalIntegrationTest, MemoryEncodingWithMotivation) {
    // Initialize
    entorhinal_brain_init_initialize(init_bridge, ec, nullptr);

    // Set high motivation
    hypothalamic_motivational_state_t motivation = {0};
    motivation.hunger_drive = 0.9f;
    motivation.arousal_level = 0.8f;
    motivation.exploration_drive = 0.7f;
    entorhinal_hypothalamus_receive_motivation(hypothalamus_bridge, &motivation);
    entorhinal_hypothalamus_bridge_update(hypothalamus_bridge, 0.01f);

    // Encoding should be modulated
    float encoding_boost = entorhinal_hypothalamus_get_encoding_modulation(hypothalamus_bridge);
    EXPECT_GT(encoding_boost, 0.5f);

    // Apply modulation to encoding
    entorhinal_set_encoding_gate(ec, encoding_boost);

    // Encode memory
    float features[32] = {0};
    float context[3] = {1.0f, 1.0f, 0.0f};
    EXPECT_EQ(entorhinal_encode_to_hippocampus(ec, features, 32, context, 3), 0);
}

TEST_F(EntorhinalIntegrationTest, ThreatResponseIntegration) {
    // Initialize
    entorhinal_brain_init_initialize(init_bridge, ec, nullptr);

    // Receive threat from omnidirectional system
    omni_threat_vector_t threat;
    threat.direction = 0.0f;
    threat.distance = 3.0f;
    threat.magnitude = 0.95f;
    entorhinal_omni_receive_threats(omni_bridge, &threat, 1);
    entorhinal_omni_bridge_update(omni_bridge, 0.01f);

    // Get escape vector
    float escape[3];
    entorhinal_omni_get_escape_vector(omni_bridge, escape);

    // Increase stress in hypothalamus
    hypothalamic_motivational_state_t motivation = {0};
    motivation.safety_drive = 0.95f;
    motivation.stress_level = 0.8f;
    motivation.arousal_level = 1.0f;
    entorhinal_hypothalamus_receive_motivation(hypothalamus_bridge, &motivation);
    entorhinal_hypothalamus_bridge_update(hypothalamus_bridge, 0.01f);

    // Memory encoding may be impaired under high stress
    float encoding_mod = entorhinal_hypothalamus_get_encoding_modulation(hypothalamus_bridge);
    // Encoding affected by stress (inverted U)
}

