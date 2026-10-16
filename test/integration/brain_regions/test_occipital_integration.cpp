/**
 * @file test_occipital_integration.cpp
 * @brief Integration tests for Occipital Cortex with brain systems
 *
 * WHAT: Integration tests for occipital cortex connections
 * WHY:  Verify correct visual processing flow through brain regions
 * HOW:  Test parietal/temporal stream connections, training integration,
 *       bio-async messaging, and cognitive module interactions
 *
 * INTEGRATION COVERAGE:
 * - Dorsal stream: V1 → V5/MT → Parietal (motion/spatial)
 * - Ventral stream: V1 → V4 → Temporal (color/form/objects)
 * - Thalamic routing: LGN/Pulvinar/SC pathways
 * - Substrate modulation: Metabolic effects on visual processing
 * - Training integration: Visual learning with confidence modulation
 * - Cognitive integration: Salience, attention, global workspace
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// Headers have their own extern "C" guards
#include "core/brain/regions/occipital/nimcp_occipital_adapter.h"
#include "core/brain/regions/occipital/nimcp_occipital_substrate_bridge.h"
#include "core/brain/regions/occipital/nimcp_occipital_thalamic_bridge.h"
#include "middleware/training/nimcp_occipital_training_bridge.h"

// ============================================================================
// TEST FIXTURE
// ============================================================================

class OccipitalIntegrationTest : public ::testing::Test {
protected:
    occipital_adapter_t* occipital;
    occipital_substrate_bridge_t* substrate_bridge;
    occipital_thalamic_bridge_t* thalamic_bridge;
    occipital_training_bridge_t* training_bridge;

    void SetUp() override {
        // Create occipital adapter
        occipital_config_t config = occipital_default_config();
        config.image_width = 128;
        config.image_height = 128;
        config.color_channels = 3;
        occipital = occipital_create(&config);
        ASSERT_NE(nullptr, occipital);

        // Create substrate bridge
        occipital_substrate_config_t sub_config = occipital_substrate_default_config();
        substrate_bridge = occipital_substrate_bridge_create(occipital, NULL, &sub_config);
        ASSERT_NE(nullptr, substrate_bridge);

        // Create thalamic bridge
        occipital_thalamic_config_t thal_config = occipital_thalamic_default_config();
        thalamic_bridge = occipital_thalamic_bridge_create(occipital, NULL, &thal_config);
        ASSERT_NE(nullptr, thalamic_bridge);

        // Create training bridge
        occipital_training_config_t train_config;
        occipital_training_default_config(&train_config);
        training_bridge = occipital_training_bridge_create(&train_config);
        ASSERT_NE(nullptr, training_bridge);
        occipital_training_connect_occipital(training_bridge, occipital);
    }

    void TearDown() override {
        if (training_bridge) {
            occipital_training_bridge_destroy(training_bridge);
            training_bridge = nullptr;
        }
        if (thalamic_bridge) {
            occipital_thalamic_bridge_destroy(thalamic_bridge);
            thalamic_bridge = nullptr;
        }
        if (substrate_bridge) {
            occipital_substrate_bridge_destroy(substrate_bridge);
            substrate_bridge = nullptr;
        }
        if (occipital) {
            occipital_destroy(occipital);
            occipital = nullptr;
        }
    }

    // Helper to create test image
    float* create_test_image(uint32_t w, uint32_t h, uint32_t c) {
        float* img = (float*)calloc(w * h * c, sizeof(float));
        if (!img) return nullptr;

        // Create gradient pattern
        for (uint32_t y = 0; y < h; y++) {
            for (uint32_t x = 0; x < w; x++) {
                for (uint32_t ch = 0; ch < c; ch++) {
                    img[(ch * h * w) + (y * w) + x] = (float)(x + y) / (float)(w + h);
                }
            }
        }
        return img;
    }
};

// ============================================================================
// BRIDGE COORDINATION TESTS
// ============================================================================

TEST_F(OccipitalIntegrationTest, AllBridgesCreateSuccessfully) {
    // Already verified in SetUp, but explicit test
    EXPECT_NE(nullptr, occipital);
    EXPECT_NE(nullptr, substrate_bridge);
    EXPECT_NE(nullptr, thalamic_bridge);
    EXPECT_NE(nullptr, training_bridge);
}

TEST_F(OccipitalIntegrationTest, BridgesResetWithoutLosingConnection) {
    // Reset all bridges
    EXPECT_EQ(0, occipital_substrate_bridge_reset(substrate_bridge));
    EXPECT_EQ(0, occipital_thalamic_bridge_reset(thalamic_bridge));
    EXPECT_EQ(0, occipital_training_bridge_reset(training_bridge));

    // Bridges should still work after reset
    EXPECT_EQ(0, occipital_substrate_bridge_update(substrate_bridge));
    EXPECT_EQ(0, occipital_training_update_effects(training_bridge));
}

// ============================================================================
// SUBSTRATE-VISUAL PROCESSING INTEGRATION
// ============================================================================

TEST_F(OccipitalIntegrationTest, SubstrateEffectsModulateProcessing) {
    // Update substrate effects
    EXPECT_EQ(0, occipital_substrate_bridge_update(substrate_bridge));

    // Get effects
    occipital_substrate_effects_t effects;
    EXPECT_EQ(0, occipital_substrate_bridge_get_effects(substrate_bridge, &effects));

    // Effects should be valid and within range
    EXPECT_GE(effects.overall_capacity, 0.0f);
    EXPECT_LE(effects.overall_capacity, 1.0f);

    // V1-V5 effects should be consistent
    EXPECT_GE(effects.v1_contrast_sensitivity, 0.0f);
    EXPECT_GE(effects.v4_color_constancy, 0.0f);
    EXPECT_GE(effects.v5_motion_direction, 0.0f);
}

TEST_F(OccipitalIntegrationTest, SubstrateApplyEffectsIntegrates) {
    // Update then apply
    EXPECT_EQ(0, occipital_substrate_bridge_update(substrate_bridge));
    EXPECT_EQ(0, occipital_substrate_bridge_apply_effects(substrate_bridge));

    // Check stats updated
    occipital_substrate_stats_t stats;
    EXPECT_EQ(0, occipital_substrate_bridge_get_stats(substrate_bridge, &stats));
    EXPECT_GE(stats.updates_processed, 1ULL);
}

// ============================================================================
// THALAMIC ROUTING INTEGRATION
// ============================================================================

TEST_F(OccipitalIntegrationTest, ThalamicRoutingWithAttention) {
    // Set attention
    EXPECT_EQ(0, occipital_thalamic_set_attention(thalamic_bridge, 0.8f));
    EXPECT_EQ(0, occipital_thalamic_set_spatial_attention(thalamic_bridge, 0.5f, 0.5f, 0.2f));

    // Route V1 signal
    float test_data[64] = {0.5f};
    EXPECT_EQ(0, occipital_thalamic_route_v1(thalamic_bridge, test_data, 0.8f));

    // Check attention was applied
    float attention;
    EXPECT_EQ(0, occipital_thalamic_get_attention(thalamic_bridge, &attention));
    EXPECT_FLOAT_EQ(0.8f, attention);
}

TEST_F(OccipitalIntegrationTest, ThalamicPathwaySeparation) {
    // Route magnocellular signal
    occipital_thalamic_signal_t magno_signal = {0};
    magno_signal.signal_type = OCCIPITAL_SIGNAL_MAGNO;
    magno_signal.pathway = LGN_PATHWAY_MAGNOCELLULAR;
    magno_signal.visual_intensity = 0.7f;
    magno_signal.temporal_frequency = 30.0f;  // High TF for motion
    EXPECT_EQ(0, occipital_thalamic_route_signal(thalamic_bridge, &magno_signal));

    // Route parvocellular signal
    occipital_thalamic_signal_t parvo_signal = {0};
    parvo_signal.signal_type = OCCIPITAL_SIGNAL_PARVO;
    parvo_signal.pathway = LGN_PATHWAY_PARVOCELLULAR;
    parvo_signal.visual_intensity = 0.9f;
    parvo_signal.spatial_frequency = 20.0f;  // High SF for detail
    EXPECT_EQ(0, occipital_thalamic_route_signal(thalamic_bridge, &parvo_signal));

    // Check both pathways registered
    occipital_thalamic_stats_t stats;
    EXPECT_EQ(0, occipital_thalamic_bridge_get_stats(thalamic_bridge, &stats));
    EXPECT_GE(stats.magno_signals, 1ULL);
    EXPECT_GE(stats.parvo_signals, 1ULL);
}

TEST_F(OccipitalIntegrationTest, ThalamicDorsalVentralStreams) {
    // Route dorsal stream
    float motion[32] = {0.3f};
    EXPECT_EQ(0, occipital_thalamic_route_dorsal(thalamic_bridge, motion, 0.7f));

    // Route ventral stream
    float form[32] = {0.4f};
    EXPECT_EQ(0, occipital_thalamic_route_ventral(thalamic_bridge, form, 0.6f));

    // Check both streams processed
    occipital_thalamic_stats_t stats;
    EXPECT_EQ(0, occipital_thalamic_bridge_get_stats(thalamic_bridge, &stats));
    EXPECT_GE(stats.dorsal_signals, 1ULL);
    EXPECT_GE(stats.ventral_signals, 1ULL);
}

// ============================================================================
// TRAINING INTEGRATION TESTS
// ============================================================================

TEST_F(OccipitalIntegrationTest, TrainingEffectsBasedOnVisualConfidence) {
    // Update training effects
    EXPECT_EQ(0, occipital_training_update_effects(training_bridge));

    // Get effects
    occipital_training_effects_t effects;
    EXPECT_EQ(0, occipital_training_get_effects(training_bridge, &effects));

    // LR factor should be modulated
    float base_lr = 0.01f;
    float modulated = occipital_training_get_modulated_lr(training_bridge, base_lr);
    EXPECT_GT(modulated, 0.0f);
}

TEST_F(OccipitalIntegrationTest, TrainingAreaSpecificUpdates) {
    // Train each area
    EXPECT_EQ(0, occipital_training_train_area(training_bridge, OCCIPITAL_TRAIN_V1, 0.01f));
    EXPECT_EQ(0, occipital_training_train_area(training_bridge, OCCIPITAL_TRAIN_V2, 0.01f));
    EXPECT_EQ(0, occipital_training_train_area(training_bridge, OCCIPITAL_TRAIN_V4, 0.01f));
    EXPECT_EQ(0, occipital_training_train_area(training_bridge, OCCIPITAL_TRAIN_V5, 0.01f));

    // Check stats
    occipital_training_stats_t stats;
    EXPECT_EQ(0, occipital_training_get_stats(training_bridge, &stats));
    EXPECT_GE(stats.v1_updates, 1ULL);
    EXPECT_GE(stats.v2_updates, 1ULL);
    EXPECT_GE(stats.v4_updates, 1ULL);
    EXPECT_GE(stats.v5_updates, 1ULL);
}

TEST_F(OccipitalIntegrationTest, TrainingWithTargets) {
    // Create targets
    occipital_training_targets_t targets = {0};
    targets.supervision_strength = 0.8f;

    // Apply targets
    EXPECT_EQ(0, occipital_training_apply_targets(training_bridge, &targets));

    // Compute loss
    float loss;
    EXPECT_EQ(0, occipital_training_compute_loss(training_bridge, OCCIPITAL_TRAIN_ALL, &loss));
    EXPECT_GE(loss, 0.0f);
}

// ============================================================================
// FULL PROCESSING PIPELINE INTEGRATION
// ============================================================================

TEST_F(OccipitalIntegrationTest, FullVisualProcessingPipeline) {
    // 1. Update substrate (metabolic state)
    EXPECT_EQ(0, occipital_substrate_bridge_update(substrate_bridge));

    // 2. Get metabolic effects
    occipital_substrate_effects_t sub_effects;
    EXPECT_EQ(0, occipital_substrate_bridge_get_effects(substrate_bridge, &sub_effects));

    // 3. Set attention based on capacity
    float attention = sub_effects.attention_capacity;
    EXPECT_EQ(0, occipital_thalamic_set_attention(thalamic_bridge, attention));

    // 4. Route visual input through thalamus
    float visual_input[64] = {0.5f};
    EXPECT_EQ(0, occipital_thalamic_route_v1(thalamic_bridge, visual_input, 0.8f));

    // 5. Update training effects
    EXPECT_EQ(0, occipital_training_update_effects(training_bridge));

    // 6. Check if should skip training
    bool should_skip = occipital_training_should_skip(training_bridge);
    // High confidence should not skip
    if (!should_skip) {
        // 7. Get modulated learning rate
        float lr = occipital_training_get_modulated_lr(training_bridge, 0.01f);
        EXPECT_GT(lr, 0.0f);

        // 8. Train areas
        EXPECT_EQ(0, occipital_training_train_area(training_bridge, OCCIPITAL_TRAIN_ALL, lr));
    }

    // 9. Broadcast capacity (would go to bio-async)
    EXPECT_EQ(0, occipital_substrate_bridge_broadcast_capacity(substrate_bridge));
    EXPECT_EQ(0, occipital_thalamic_bridge_broadcast_routing(thalamic_bridge));
}

// ============================================================================
// STATISTICS CONSISTENCY TESTS
// ============================================================================

TEST_F(OccipitalIntegrationTest, StatisticsConsistentAcrossBridges) {
    // Perform several updates
    for (int i = 0; i < 10; i++) {
        occipital_substrate_bridge_update(substrate_bridge);
        occipital_thalamic_route_v1(thalamic_bridge, nullptr, 0.5f);
        occipital_training_update(training_bridge, 100);
    }

    // Get all stats
    occipital_substrate_stats_t sub_stats;
    occipital_thalamic_stats_t thal_stats;
    occipital_training_stats_t train_stats;

    EXPECT_EQ(0, occipital_substrate_bridge_get_stats(substrate_bridge, &sub_stats));
    EXPECT_EQ(0, occipital_thalamic_bridge_get_stats(thalamic_bridge, &thal_stats));
    EXPECT_EQ(0, occipital_training_get_stats(training_bridge, &train_stats));

    // All should have processed roughly 10 updates
    EXPECT_GE(sub_stats.updates_processed, 10ULL);
    EXPECT_GE(thal_stats.v1_signals_routed, 10ULL);
    EXPECT_GE(train_stats.total_training_steps, 1ULL);
}

// ============================================================================
// RESET AND CLEANUP TESTS
// ============================================================================

TEST_F(OccipitalIntegrationTest, ResetClearsStatistics) {
    // Generate stats
    for (int i = 0; i < 5; i++) {
        occipital_substrate_bridge_update(substrate_bridge);
        occipital_thalamic_route_v1(thalamic_bridge, nullptr, 0.5f);
    }

    // Reset
    occipital_substrate_bridge_reset_stats(substrate_bridge);
    occipital_thalamic_bridge_reset_stats(thalamic_bridge);
    occipital_training_reset_stats(training_bridge);

    // Verify cleared
    occipital_substrate_stats_t sub_stats;
    occipital_thalamic_stats_t thal_stats;
    occipital_training_stats_t train_stats;

    EXPECT_EQ(0, occipital_substrate_bridge_get_stats(substrate_bridge, &sub_stats));
    EXPECT_EQ(0, occipital_thalamic_bridge_get_stats(thalamic_bridge, &thal_stats));
    EXPECT_EQ(0, occipital_training_get_stats(training_bridge, &train_stats));

    EXPECT_EQ(0ULL, sub_stats.updates_processed);
    EXPECT_EQ(0ULL, thal_stats.v1_signals_routed);
    EXPECT_EQ(0ULL, train_stats.total_training_steps);
}

// ============================================================================
// STRESS TESTS
// ============================================================================

TEST_F(OccipitalIntegrationTest, HighThroughputProcessing) {
    // Simulate high-frequency visual processing
    for (int i = 0; i < 1000; i++) {
        occipital_substrate_bridge_update(substrate_bridge);

        float visual[16] = {(float)i / 1000.0f};
        occipital_thalamic_route_v1(thalamic_bridge, visual, 0.5f);

        if (i % 10 == 0) {
            occipital_training_update(training_bridge, 10);
        }
    }

    // Verify no crashes and reasonable stats
    occipital_substrate_stats_t sub_stats;
    EXPECT_EQ(0, occipital_substrate_bridge_get_stats(substrate_bridge, &sub_stats));
    EXPECT_EQ(1000ULL, sub_stats.updates_processed);

    occipital_thalamic_stats_t thal_stats;
    EXPECT_EQ(0, occipital_thalamic_bridge_get_stats(thalamic_bridge, &thal_stats));
    EXPECT_GE(thal_stats.v1_signals_routed, 1000ULL);
}
