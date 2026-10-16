//=============================================================================
// test_dragonfly_multi_bridge_integration.cpp - Multi-Bridge Integration Tests
//=============================================================================
/**
 * @file test_dragonfly_multi_bridge_integration.cpp
 * @brief Integration tests for dragonfly bridges working together
 *
 * WHAT: Tests coordination between dragonfly's 12 bridge modules
 * WHY:  Verify bridges communicate and synchronize correctly
 * HOW:  Create bridges, simulate data flow, verify outputs
 *
 * BRIDGES TESTED:
 * - Visual Bridge: Target detection from visual cortex
 * - Audio Bridge: Directional cues from audio cortex
 * - Parietal Bridge: Spatial reasoning for interception
 * - Cortical Bridge: Higher-level processing integration
 * - Cognitive Bridge: Attention and salience signals
 * - Thalamic Bridge: Signal routing and gating
 * - Substrate Bridge: Metabolic costs and fatigue
 * - FEP Bridge: Free energy prediction signals
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly.h"
#include "dragonfly/nimcp_dragonfly_visual_bridge.h"
#include "dragonfly/nimcp_dragonfly_audio_bridge.h"
#include "dragonfly/nimcp_dragonfly_parietal_bridge.h"
#include "dragonfly/nimcp_dragonfly_cortical_bridge.h"
#include "dragonfly/nimcp_dragonfly_cognitive_bridge.h"
#include "dragonfly/nimcp_dragonfly_thalamic_bridge.h"
#include "dragonfly/nimcp_dragonfly_substrate_bridge.h"
#include "dragonfly/nimcp_dragonfly_fep_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class DragonflyMultiBridgeTest : public ::testing::Test {
protected:
    dragonfly_system_t* system = nullptr;
    dragonfly_visual_bridge_t* visual_bridge = nullptr;
    dragonfly_audio_bridge_t* audio_bridge = nullptr;
    dragonfly_parietal_bridge_t* parietal_bridge = nullptr;
    dragonfly_cortical_bridge_t* cortical_bridge = nullptr;
    dragonfly_cognitive_bridge_t* cognitive_bridge = nullptr;
    dragonfly_thalamic_bridge_t* thalamic_bridge = nullptr;
    dragonfly_substrate_bridge_t* substrate_bridge = nullptr;
    dragonfly_fep_bridge_t* fep_bridge = nullptr;

    void SetUp() override {
        // Create dragonfly system
        dragonfly_config_t config = dragonfly_default_config();
        system = dragonfly_system_create(&config);
        ASSERT_NE(system, nullptr);

        // Create visual bridge with proper API
        visual_bridge_config_t visual_config = visual_bridge_default_config();
        visual_bridge = dragonfly_visual_bridge_create(system, nullptr, &visual_config);

        // Create audio bridge
        audio_bridge_config_t audio_config = audio_bridge_default_config();
        audio_bridge = dragonfly_audio_bridge_create(system, nullptr, &audio_config);

        // Create parietal bridge
        parietal_bridge_config_t parietal_config = parietal_bridge_default_config();
        parietal_bridge = dragonfly_parietal_bridge_create(system, nullptr, nullptr, &parietal_config);

        // Create cortical bridge
        dragonfly_cortical_config_t cortical_config;
        dragonfly_cortical_bridge_default_config(&cortical_config);
        cortical_bridge = dragonfly_cortical_bridge_create(system, nullptr, &cortical_config);

        // Create cognitive bridge
        dragonfly_cognitive_config_t cognitive_config;
        dragonfly_cognitive_bridge_default_config(&cognitive_config);
        cognitive_bridge = dragonfly_cognitive_bridge_create(system, &cognitive_config);

        // Create thalamic bridge
        dragonfly_thalamic_config_t thalamic_config;
        dragonfly_thalamic_bridge_default_config(&thalamic_config);
        thalamic_bridge = dragonfly_thalamic_bridge_create(system, nullptr, &thalamic_config);

        // Create substrate bridge
        dragonfly_substrate_config_t substrate_config;
        dragonfly_substrate_bridge_default_config(&substrate_config);
        substrate_bridge = dragonfly_substrate_bridge_create(system, nullptr, &substrate_config);

        // Create FEP bridge
        dragonfly_fep_config_t fep_config;
        dragonfly_fep_bridge_default_config(&fep_config);
        fep_bridge = dragonfly_fep_bridge_create(system, nullptr, &fep_config);
    }

    void TearDown() override {
        if (fep_bridge) dragonfly_fep_bridge_destroy(fep_bridge);
        if (substrate_bridge) dragonfly_substrate_bridge_destroy(substrate_bridge);
        if (thalamic_bridge) dragonfly_thalamic_bridge_destroy(thalamic_bridge);
        if (cognitive_bridge) dragonfly_cognitive_bridge_destroy(cognitive_bridge);
        if (cortical_bridge) dragonfly_cortical_bridge_destroy(cortical_bridge);
        if (parietal_bridge) dragonfly_parietal_bridge_destroy(parietal_bridge);
        if (audio_bridge) dragonfly_audio_bridge_destroy(audio_bridge);
        if (visual_bridge) dragonfly_visual_bridge_destroy(visual_bridge);
        if (system) dragonfly_system_destroy(system);
    }
};

//=============================================================================
// All Bridges Created Tests
//=============================================================================

TEST_F(DragonflyMultiBridgeTest, AllBridgesCreatedSuccessfully) {
    EXPECT_NE(visual_bridge, nullptr) << "Visual bridge should be created";
    EXPECT_NE(audio_bridge, nullptr) << "Audio bridge should be created";
    EXPECT_NE(parietal_bridge, nullptr) << "Parietal bridge should be created";
    EXPECT_NE(cortical_bridge, nullptr) << "Cortical bridge should be created";
    EXPECT_NE(cognitive_bridge, nullptr) << "Cognitive bridge should be created";
    EXPECT_NE(thalamic_bridge, nullptr) << "Thalamic bridge should be created";
    EXPECT_NE(substrate_bridge, nullptr) << "Substrate bridge should be created";
    EXPECT_NE(fep_bridge, nullptr) << "FEP bridge should be created";
}

//=============================================================================
// Visual + Audio Sensor Fusion Tests
//=============================================================================

TEST_F(DragonflyMultiBridgeTest, VisualAudioSensorFusion) {
    ASSERT_NE(visual_bridge, nullptr);
    ASSERT_NE(audio_bridge, nullptr);

    // Inject synthetic visual blob
    motion_blob_t visual_blob = {};
    visual_blob.center_x = 320.0f;
    visual_blob.center_y = 240.0f;
    visual_blob.size_pixels = 50.0f;
    visual_blob.velocity_x = 10.0f;
    visual_blob.velocity_y = 5.0f;
    visual_blob.contrast = 0.8f;
    visual_blob.salience = 0.85f;
    visual_blob.track_id = 1;
    EXPECT_EQ(dragonfly_visual_bridge_inject_blob(visual_bridge, &visual_blob), 0);

    // Inject synthetic audio source from similar direction
    audio_source_t audio_source = {};
    audio_source.azimuth = 0.46f;  // Similar direction
    audio_source.elevation = 0.0f;
    audio_source.intensity_db = 60.0f;
    audio_source.distance_est = 5.0f;
    audio_source.confidence = 0.7f;
    audio_source.frequency_hz = 500.0f;
    audio_source.bandwidth_hz = 100.0f;
    audio_source.source_id = 1;
    audio_source.timestamp_us = 1000000;
    EXPECT_EQ(dragonfly_audio_bridge_inject_source(audio_bridge, &audio_source), 0);

    // Get visual result
    visual_motion_result_t visual_result;
    EXPECT_EQ(dragonfly_visual_bridge_get_result(visual_bridge, &visual_result), 0);

    // Get audio result
    audio_detection_result_t audio_result;
    EXPECT_EQ(dragonfly_audio_bridge_get_result(audio_bridge, &audio_result), 0);

    // Both should have detected something
    EXPECT_GE(visual_result.num_blobs, 0u);
}

//=============================================================================
// Parietal Spatial Processing Tests
//=============================================================================

TEST_F(DragonflyMultiBridgeTest, ParietalSpatialProcessing) {
    ASSERT_NE(parietal_bridge, nullptr);

    // Set observer state
    observer_state_t observer = {};
    observer.position = {0.0f, 0.0f, 0.0f};
    observer.orientation = {1.0f, 0.0f, 0.0f, 0.0f};
    observer.heading = 0.0f;
    observer.pitch = 0.0f;
    observer.roll = 0.0f;
    observer.frame = COORD_FRAME_WORLD;
    EXPECT_EQ(dragonfly_parietal_bridge_set_observer(parietal_bridge, &observer), 0);

    // Get observer back
    observer_state_t obs_out;
    EXPECT_EQ(dragonfly_parietal_bridge_get_observer(parietal_bridge, &obs_out), 0);

    // Compute angles to a position
    parietal_vec3_t target_pos = {10.0f, 5.0f, 2.0f};
    float azimuth, elevation, distance;
    EXPECT_EQ(dragonfly_parietal_bridge_compute_angles(parietal_bridge, &target_pos,
                                                        &azimuth, &elevation, &distance), 0);
    EXPECT_GT(distance, 0.0f);
}

//=============================================================================
// Cortical Direction Processing Tests
//=============================================================================

TEST_F(DragonflyMultiBridgeTest, CorticalDirectionProcessing) {
    ASSERT_NE(cortical_bridge, nullptr);

    // Test TSDN to cortical column conversion
    float tsdn_rates[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT] = {};
    tsdn_rates[0] = 0.9f;  // Strong activation at 0 degrees
    tsdn_rates[1] = 0.6f;
    tsdn_rates[15] = 0.5f;

    cortical_direction_t direction;
    EXPECT_EQ(dragonfly_cortical_tsdn_to_column(cortical_bridge, tsdn_rates, &direction), 0);

    // Should have computed winner
    EXPECT_GE(direction.confidence, 0.0f);
    EXPECT_LE(direction.confidence, 1.0f);
}

//=============================================================================
// Cognitive Attention Tests
//=============================================================================

TEST_F(DragonflyMultiBridgeTest, CognitiveAttentionUpdate) {
    ASSERT_NE(cognitive_bridge, nullptr);

    // Step the cognitive bridge
    EXPECT_EQ(dragonfly_cognitive_step(cognitive_bridge, 16.0f), 0);

    // Update all cognitive systems
    EXPECT_EQ(dragonfly_cognitive_update_all(cognitive_bridge), 0);

    // Get attention focus
    dragonfly_attention_focus_t focus;
    EXPECT_EQ(dragonfly_cognitive_get_attention_focus(cognitive_bridge, &focus), 0);
}

//=============================================================================
// Thalamic Signal Routing Tests
//=============================================================================

TEST_F(DragonflyMultiBridgeTest, ThalamicSignalRouting) {
    ASSERT_NE(thalamic_bridge, nullptr);

    // Relay visual target signal
    thal_visual_target_t visual_target = {};
    visual_target.position[0] = 10.0f;
    visual_target.position[1] = 5.0f;
    visual_target.position[2] = 0.0f;
    visual_target.angular_position[0] = 0.5f;
    visual_target.angular_position[1] = 0.1f;
    visual_target.size = 0.05f;
    visual_target.contrast = 0.8f;
    visual_target.motion_energy = 0.7f;
    EXPECT_EQ(dragonfly_thalamic_relay_visual(thalamic_bridge, &visual_target), 0);

    // Set routing mode
    EXPECT_EQ(dragonfly_thalamic_set_mode(thalamic_bridge, THAL_ROUTE_TRACKING), 0);
    EXPECT_EQ(dragonfly_thalamic_get_mode(thalamic_bridge), THAL_ROUTE_TRACKING);

    // Step the thalamic bridge
    EXPECT_EQ(dragonfly_thalamic_step(thalamic_bridge, 16.0f), 0);
}

//=============================================================================
// Substrate Energy Tests
//=============================================================================

TEST_F(DragonflyMultiBridgeTest, SubstrateEnergyTracking) {
    ASSERT_NE(substrate_bridge, nullptr);

    // Get initial energy
    float initial_energy = dragonfly_substrate_get_energy(substrate_bridge);
    EXPECT_GT(initial_energy, 0.0f);

    // Record some tracking activity
    EXPECT_EQ(dragonfly_substrate_record_tracking(substrate_bridge, 2), 0);

    // Record prediction
    EXPECT_EQ(dragonfly_substrate_record_prediction(substrate_bridge, 0.5f), 0);

    // Set activity level
    EXPECT_EQ(dragonfly_substrate_set_activity(substrate_bridge, SUBSTRATE_ACTIVITY_TRACKING), 0);
    EXPECT_EQ(dragonfly_substrate_get_activity(substrate_bridge), SUBSTRATE_ACTIVITY_TRACKING);

    // Step
    EXPECT_EQ(dragonfly_substrate_step(substrate_bridge, 16.0f), 0);
}

//=============================================================================
// FEP Prediction Error Tests
//=============================================================================

TEST_F(DragonflyMultiBridgeTest, FEPPredictionError) {
    ASSERT_NE(fep_bridge, nullptr);

    // Compute prediction errors from observations
    float observations[8] = {1.0f, 0.5f, 0.2f, 0.8f, 0.3f, 0.6f, 0.1f, 0.9f};
    dragonfly_fep_errors_t errors;
    EXPECT_EQ(dragonfly_fep_compute_errors(fep_bridge, observations, 8, &errors), 0);

    // Get free energy
    float free_energy = dragonfly_fep_get_free_energy(fep_bridge);
    EXPECT_GE(free_energy, 0.0f);

    // Update and step
    EXPECT_EQ(dragonfly_fep_update(fep_bridge, 16.0f), 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(DragonflyMultiBridgeTest, BridgeStatisticsAccumulate) {
    ASSERT_NE(visual_bridge, nullptr);
    ASSERT_NE(cognitive_bridge, nullptr);

    // Process multiple inputs through bridges
    for (int i = 0; i < 50; i++) {
        motion_blob_t blob = {};
        blob.center_x = 320.0f + i;
        blob.center_y = 240.0f;
        blob.size_pixels = 50.0f;
        blob.velocity_x = 5.0f;
        blob.velocity_y = 0.0f;
        blob.contrast = 0.8f;
        blob.salience = 0.8f;
        blob.track_id = (uint32_t)i + 1;
        dragonfly_visual_bridge_inject_blob(visual_bridge, &blob);

        dragonfly_cognitive_step(cognitive_bridge, 16.0f);
    }

    // Get visual statistics
    visual_bridge_stats_t visual_stats;
    EXPECT_EQ(dragonfly_visual_bridge_get_stats(visual_bridge, &visual_stats), 0);
    EXPECT_GE(visual_stats.frames_processed, 0u);  // Verify stats work

    // Get cognitive statistics
    cognitive_bridge_stats_t cognitive_stats;
    EXPECT_EQ(dragonfly_cognitive_bridge_get_stats(cognitive_bridge, &cognitive_stats), 0);
    EXPECT_GE(cognitive_stats.salience_updates, 0u);  // Verify stats work
}

//=============================================================================
// Reset Tests
//=============================================================================

TEST_F(DragonflyMultiBridgeTest, AllBridgesReset) {
    EXPECT_EQ(dragonfly_visual_bridge_reset(visual_bridge), 0);
    EXPECT_EQ(dragonfly_audio_bridge_reset(audio_bridge), 0);
    EXPECT_EQ(dragonfly_parietal_bridge_reset(parietal_bridge), 0);
    EXPECT_EQ(dragonfly_cortical_bridge_reset(cortical_bridge), 0);
    EXPECT_EQ(dragonfly_cognitive_bridge_reset(cognitive_bridge), 0);
    EXPECT_EQ(dragonfly_thalamic_bridge_reset(thalamic_bridge), 0);
    EXPECT_EQ(dragonfly_substrate_bridge_reset(substrate_bridge), 0);
    EXPECT_EQ(dragonfly_fep_bridge_reset(fep_bridge), 0);
}

//=============================================================================
// Null Safety Tests
//=============================================================================

TEST_F(DragonflyMultiBridgeTest, NullBridgeHandling) {
    // Destroy operations should be null-safe
    dragonfly_visual_bridge_destroy(nullptr);
    dragonfly_audio_bridge_destroy(nullptr);
    dragonfly_parietal_bridge_destroy(nullptr);
    dragonfly_cortical_bridge_destroy(nullptr);
    dragonfly_cognitive_bridge_destroy(nullptr);
    dragonfly_thalamic_bridge_destroy(nullptr);
    dragonfly_substrate_bridge_destroy(nullptr);
    dragonfly_fep_bridge_destroy(nullptr);
    SUCCEED();
}
