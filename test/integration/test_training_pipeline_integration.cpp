/**
 * @file test_training_pipeline_integration.cpp
 * @brief Integration tests for the full training pipeline
 *
 * WHAT: Tests integration of training bridges (cognitive, cortical, perception)
 * WHY:  Verify complete training workflow with modulation and feedback
 * HOW:  Test bridge lifecycle, modulation effects, and inter-bridge communication
 *
 * Test Categories:
 * - Full training pipeline (data loading -> training -> evaluation)
 * - Cognitive-training bridge integration
 * - Cortical-training bridge integration
 * - Perception-training bridge integration
 * - Cross-bridge coordination
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <memory>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_cognitive_training_bridge.h"
#include "middleware/training/nimcp_cortical_training_bridge.h"
#include "middleware/training/nimcp_perception_training_bridge.h"
#include "middleware/training/nimcp_training_module.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture - Cognitive-Training Bridge
//=============================================================================

class CognitiveTrainingBridgeTest : public ::testing::Test {
protected:
    cognitive_training_bridge_t* bridge = nullptr;
    cognitive_training_config_t config;

    void SetUp() override {
        cognitive_training_default_config(&config);
        config.mode = COGNITIVE_TRAINING_MODE_AUTOMATIC;
        config.enable_bio_async = false;  // Disable for unit testing
        config.disable_auto_update = true;  // Manual updates for testing

        bridge = cognitive_training_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            cognitive_training_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Test Fixture - Cortical-Training Bridge
//=============================================================================

class CorticalTrainingBridgeTest : public ::testing::Test {
protected:
    cortical_training_bridge_t* bridge = nullptr;
    cortical_training_config_t config;

    void SetUp() override {
        cortical_training_default_config(&config);
        config.mode = CORTICAL_TRAINING_MODE_AUTOMATIC;
        config.enable_bio_async = false;
        config.disable_auto_update = true;

        bridge = cortical_training_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            cortical_training_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Test Fixture - Perception-Training Bridge
//=============================================================================

class PerceptionTrainingBridgeTest : public ::testing::Test {
protected:
    perception_training_bridge_t* bridge = nullptr;
    perception_training_config_t config;

    void SetUp() override {
        perception_training_default_config(&config);
        config.mode = PERCEPTION_TRAINING_MODE_AUTOMATIC;
        config.enable_bio_async = false;
        config.disable_auto_update = true;

        bridge = perception_training_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            perception_training_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Test Fixture - Full Training Pipeline
//=============================================================================

class TrainingPipelineIntegrationTest : public ::testing::Test {
protected:
    cognitive_training_bridge_t* cognitive_bridge = nullptr;
    cortical_training_bridge_t* cortical_bridge = nullptr;
    perception_training_bridge_t* perception_bridge = nullptr;

    void SetUp() override {
        // Create cognitive-training bridge
        cognitive_training_config_t cog_config;
        cognitive_training_default_config(&cog_config);
        cog_config.mode = COGNITIVE_TRAINING_MODE_AUTOMATIC;
        cog_config.enable_bio_async = false;
        cog_config.disable_auto_update = true;
        cognitive_bridge = cognitive_training_create(&cog_config);
        ASSERT_NE(cognitive_bridge, nullptr);

        // Create cortical-training bridge
        cortical_training_config_t cort_config;
        cortical_training_default_config(&cort_config);
        cort_config.mode = CORTICAL_TRAINING_MODE_AUTOMATIC;
        cort_config.enable_bio_async = false;
        cort_config.disable_auto_update = true;
        cortical_bridge = cortical_training_create(&cort_config);
        ASSERT_NE(cortical_bridge, nullptr);

        // Create perception-training bridge
        perception_training_config_t perc_config;
        perception_training_default_config(&perc_config);
        perc_config.mode = PERCEPTION_TRAINING_MODE_AUTOMATIC;
        perc_config.enable_bio_async = false;
        perc_config.disable_auto_update = true;
        perception_bridge = perception_training_create(&perc_config);
        ASSERT_NE(perception_bridge, nullptr);
    }

    void TearDown() override {
        if (cognitive_bridge) {
            cognitive_training_destroy(cognitive_bridge);
            cognitive_bridge = nullptr;
        }
        if (cortical_bridge) {
            cortical_training_destroy(cortical_bridge);
            cortical_bridge = nullptr;
        }
        if (perception_bridge) {
            perception_training_destroy(perception_bridge);
            perception_bridge = nullptr;
        }
    }
};

//=============================================================================
// Cognitive-Training Bridge Tests
//=============================================================================

TEST_F(CognitiveTrainingBridgeTest, CreateAndDestroy) {
    // Bridge is created in SetUp - verify it's valid
    EXPECT_NE(bridge, nullptr);
}

TEST_F(CognitiveTrainingBridgeTest, StartAndStop) {
    int result = cognitive_training_start(bridge);
    EXPECT_EQ(result, 0);

    result = cognitive_training_stop(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(CognitiveTrainingBridgeTest, GetEffectsWithoutModules) {
    // Get effects without connected modules - should return default values
    cognitive_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));

    int result = cognitive_training_get_effects(bridge, &effects);
    EXPECT_EQ(result, 0);

    // Default LR factor should be 1.0 (no modulation)
    EXPECT_NEAR(effects.lr_factor, 1.0f, 0.1f);
}

TEST_F(CognitiveTrainingBridgeTest, SetEffectsForTesting) {
    // Create test effects
    cognitive_training_effects_t test_effects;
    memset(&test_effects, 0, sizeof(test_effects));
    test_effects.cognitive_load = 0.8f;
    test_effects.epistemic_uncertainty = 0.5f;
    test_effects.attention_focus = 0.7f;
    test_effects.exploration_drive = 0.3f;
    test_effects.emotional_valence = 0.2f;
    test_effects.lr_factor = 0.7f;
    test_effects.batch_size_factor = 0.8f;
    test_effects.valid = true;

    int result = cognitive_training_set_effects_for_testing(bridge, &test_effects);
    EXPECT_EQ(result, 0);

    // Verify effects were set
    cognitive_training_effects_t retrieved_effects;
    memset(&retrieved_effects, 0, sizeof(retrieved_effects));
    result = cognitive_training_get_effects(bridge, &retrieved_effects);
    EXPECT_EQ(result, 0);

    EXPECT_NEAR(retrieved_effects.cognitive_load, 0.8f, 0.01f);
    EXPECT_NEAR(retrieved_effects.lr_factor, 0.7f, 0.01f);
}

TEST_F(CognitiveTrainingBridgeTest, ModulatedLearningRate) {
    // Set test effects with low LR factor
    cognitive_training_effects_t test_effects;
    memset(&test_effects, 0, sizeof(test_effects));
    test_effects.lr_factor = 0.5f;
    test_effects.valid = true;

    cognitive_training_set_effects_for_testing(bridge, &test_effects);

    // Get modulated LR
    float base_lr = 0.01f;
    float modulated_lr = cognitive_training_get_modulated_lr(bridge, base_lr);

    // Modulated LR should be approximately base * factor
    EXPECT_NEAR(modulated_lr, 0.005f, 0.001f);
}

TEST_F(CognitiveTrainingBridgeTest, ModulatedBatchSize) {
    // Set test effects with reduced batch size factor
    cognitive_training_effects_t test_effects;
    memset(&test_effects, 0, sizeof(test_effects));
    test_effects.batch_size_factor = 0.5f;
    test_effects.valid = true;

    cognitive_training_set_effects_for_testing(bridge, &test_effects);

    // Get modulated batch size
    uint32_t base_batch = 64;
    uint32_t modulated_batch = cognitive_training_get_modulated_batch_size(bridge, base_batch);

    // Modulated batch should be reduced
    EXPECT_LE(modulated_batch, base_batch);
    EXPECT_GE(modulated_batch, 16u);  // Should not go below minimum
}

TEST_F(CognitiveTrainingBridgeTest, UpdateMetrics) {
    int result = cognitive_training_start(bridge);
    EXPECT_EQ(result, 0);

    // Update with training metrics
    result = cognitive_training_update_metrics(bridge, 1.5f, 0.1f, 0.01f, 100);
    EXPECT_EQ(result, 0);

    // Update again with improved loss
    result = cognitive_training_update_metrics(bridge, 1.2f, 0.08f, 0.01f, 101);
    EXPECT_EQ(result, 0);

    cognitive_training_stop(bridge);
}

TEST_F(CognitiveTrainingBridgeTest, SignalFeedbackEvents) {
    cognitive_training_start(bridge);

    // Signal satisfaction (loss improved)
    int result = cognitive_training_signal_event(
        bridge,
        COGNITIVE_TRAINING_FEEDBACK_SATISFACTION,
        0.8f
    );
    EXPECT_EQ(result, 0);

    // Signal frustration (plateau)
    result = cognitive_training_signal_event(
        bridge,
        COGNITIVE_TRAINING_FEEDBACK_FRUSTRATION,
        0.5f
    );
    EXPECT_EQ(result, 0);

    // Signal alarm (divergence)
    result = cognitive_training_signal_event(
        bridge,
        COGNITIVE_TRAINING_FEEDBACK_ALARM,
        1.0f
    );
    EXPECT_EQ(result, 0);

    cognitive_training_stop(bridge);
}

TEST_F(CognitiveTrainingBridgeTest, GetStatistics) {
    cognitive_training_start(bridge);

    // Perform some operations
    cognitive_training_update_metrics(bridge, 1.5f, 0.1f, 0.01f, 100);
    cognitive_training_signal_event(bridge, COGNITIVE_TRAINING_FEEDBACK_SATISFACTION, 0.8f);

    // Get statistics
    cognitive_training_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    int result = cognitive_training_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);

    // Verify some stats were recorded
    EXPECT_GE(stats.total_feedback_events, 1u);

    cognitive_training_stop(bridge);
}

TEST_F(CognitiveTrainingBridgeTest, ResetStatistics) {
    cognitive_training_start(bridge);

    // Perform operations
    cognitive_training_update_metrics(bridge, 1.5f, 0.1f, 0.01f, 100);

    // Reset stats
    int result = cognitive_training_reset_stats(bridge);
    EXPECT_EQ(result, 0);

    // Verify stats were reset
    cognitive_training_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    cognitive_training_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_modulations, 0u);

    cognitive_training_stop(bridge);
}

//=============================================================================
// Cortical-Training Bridge Tests
//=============================================================================

TEST_F(CorticalTrainingBridgeTest, CreateAndDestroy) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(CorticalTrainingBridgeTest, StartAndStop) {
    int result = cortical_training_start(bridge);
    EXPECT_EQ(result, 0);

    result = cortical_training_stop(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalTrainingBridgeTest, GetEffectsWithoutModules) {
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));

    int result = cortical_training_get_effects(bridge, &effects);
    EXPECT_EQ(result, 0);

    // Default LR factor should be near 1.0
    EXPECT_NEAR(effects.lr_factor, 1.0f, 0.2f);
}

TEST_F(CorticalTrainingBridgeTest, SetEffectsForTesting) {
    cortical_training_effects_t test_effects;
    memset(&test_effects, 0, sizeof(test_effects));
    test_effects.free_energy = 5.0f;
    test_effects.burst_rate = 0.7f;
    test_effects.winner_confidence = 0.8f;
    test_effects.lr_factor = 0.8f;
    test_effects.gradient_confidence = 0.9f;
    test_effects.valid = true;

    int result = cortical_training_set_effects_for_testing(bridge, &test_effects);
    EXPECT_EQ(result, 0);

    cortical_training_effects_t retrieved_effects;
    memset(&retrieved_effects, 0, sizeof(retrieved_effects));
    result = cortical_training_get_effects(bridge, &retrieved_effects);
    EXPECT_EQ(result, 0);

    EXPECT_NEAR(retrieved_effects.free_energy, 5.0f, 0.01f);
    EXPECT_NEAR(retrieved_effects.lr_factor, 0.8f, 0.01f);
}

TEST_F(CorticalTrainingBridgeTest, ModulatedLearningRate) {
    cortical_training_effects_t test_effects;
    memset(&test_effects, 0, sizeof(test_effects));
    test_effects.lr_factor = 0.6f;
    test_effects.valid = true;

    cortical_training_set_effects_for_testing(bridge, &test_effects);

    float base_lr = 0.01f;
    float modulated_lr = cortical_training_get_modulated_lr(bridge, base_lr);

    EXPECT_NEAR(modulated_lr, 0.006f, 0.001f);
}

TEST_F(CorticalTrainingBridgeTest, GradientConfidence) {
    cortical_training_effects_t test_effects;
    memset(&test_effects, 0, sizeof(test_effects));
    test_effects.gradient_confidence = 0.85f;
    test_effects.valid = true;

    cortical_training_set_effects_for_testing(bridge, &test_effects);

    float confidence = cortical_training_get_gradient_confidence(bridge);
    EXPECT_NEAR(confidence, 0.85f, 0.01f);
}

TEST_F(CorticalTrainingBridgeTest, PredictionsStable) {
    cortical_training_effects_t test_effects;
    memset(&test_effects, 0, sizeof(test_effects));
    test_effects.predictions_stable = true;
    test_effects.valid = true;

    cortical_training_set_effects_for_testing(bridge, &test_effects);

    bool stable = cortical_training_are_predictions_stable(bridge);
    EXPECT_TRUE(stable);
}

TEST_F(CorticalTrainingBridgeTest, UpdateMetrics) {
    cortical_training_start(bridge);

    int result = cortical_training_update_metrics(bridge, 2.5f, 0.15f, 0.01f, 50);
    EXPECT_EQ(result, 0);

    cortical_training_stop(bridge);
}

TEST_F(CorticalTrainingBridgeTest, SignalFeedbackEvents) {
    cortical_training_start(bridge);

    int result = cortical_training_signal_event(
        bridge,
        CORTICAL_TRAINING_FEEDBACK_STRENGTHEN_PREDICTIONS,
        0.7f
    );
    EXPECT_EQ(result, 0);

    result = cortical_training_signal_event(
        bridge,
        CORTICAL_TRAINING_FEEDBACK_CONSOLIDATE,
        0.9f
    );
    EXPECT_EQ(result, 0);

    cortical_training_stop(bridge);
}

//=============================================================================
// Perception-Training Bridge Tests
//=============================================================================

TEST_F(PerceptionTrainingBridgeTest, CreateAndDestroy) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(PerceptionTrainingBridgeTest, StartAndStop) {
    int result = perception_training_start(bridge);
    EXPECT_EQ(result, 0);

    result = perception_training_stop(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(PerceptionTrainingBridgeTest, GetEffectsWithoutModules) {
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));

    int result = perception_training_get_effects(bridge, &effects);
    EXPECT_EQ(result, 0);

    EXPECT_NEAR(effects.lr_factor, 1.0f, 0.2f);
}

TEST_F(PerceptionTrainingBridgeTest, SetEffectsForTesting) {
    perception_training_effects_t test_effects;
    memset(&test_effects, 0, sizeof(test_effects));
    test_effects.visual_confidence = 0.9f;
    test_effects.audio_quality = 0.8f;
    test_effects.comprehension = 0.85f;
    test_effects.lr_factor = 1.2f;
    test_effects.sample_weight = 1.5f;
    test_effects.skip_sample = false;
    test_effects.valid = true;

    int result = perception_training_set_effects_for_testing(bridge, &test_effects);
    EXPECT_EQ(result, 0);

    perception_training_effects_t retrieved_effects;
    memset(&retrieved_effects, 0, sizeof(retrieved_effects));
    result = perception_training_get_effects(bridge, &retrieved_effects);
    EXPECT_EQ(result, 0);

    EXPECT_NEAR(retrieved_effects.visual_confidence, 0.9f, 0.01f);
    EXPECT_NEAR(retrieved_effects.lr_factor, 1.2f, 0.01f);
}

TEST_F(PerceptionTrainingBridgeTest, ModulatedLearningRate) {
    perception_training_effects_t test_effects;
    memset(&test_effects, 0, sizeof(test_effects));
    test_effects.lr_factor = 1.3f;
    test_effects.valid = true;

    perception_training_set_effects_for_testing(bridge, &test_effects);

    float base_lr = 0.01f;
    float modulated_lr = perception_training_get_modulated_lr(bridge, base_lr);

    EXPECT_NEAR(modulated_lr, 0.013f, 0.002f);
}

TEST_F(PerceptionTrainingBridgeTest, SampleWeight) {
    perception_training_effects_t test_effects;
    memset(&test_effects, 0, sizeof(test_effects));
    test_effects.sample_weight = 1.8f;
    test_effects.valid = true;

    perception_training_set_effects_for_testing(bridge, &test_effects);

    float weight = perception_training_get_sample_weight(bridge);
    EXPECT_NEAR(weight, 1.8f, 0.1f);
}

TEST_F(PerceptionTrainingBridgeTest, ShouldSkipSample) {
    // Set effects with skip_sample = true
    perception_training_effects_t test_effects;
    memset(&test_effects, 0, sizeof(test_effects));
    test_effects.skip_sample = true;
    test_effects.valid = true;

    perception_training_set_effects_for_testing(bridge, &test_effects);

    bool should_skip = perception_training_should_skip_sample(bridge);
    EXPECT_TRUE(should_skip);

    // Set skip_sample = false
    test_effects.skip_sample = false;
    perception_training_set_effects_for_testing(bridge, &test_effects);

    should_skip = perception_training_should_skip_sample(bridge);
    EXPECT_FALSE(should_skip);
}

TEST_F(PerceptionTrainingBridgeTest, UpdateMetrics) {
    perception_training_start(bridge);

    int result = perception_training_update_metrics(bridge, 1.8f, 0.12f);
    EXPECT_EQ(result, 0);

    perception_training_stop(bridge);
}

TEST_F(PerceptionTrainingBridgeTest, SignalFeedbackEvents) {
    perception_training_start(bridge);

    int result = perception_training_signal_event(
        bridge,
        PERCEPTION_TRAINING_FEEDBACK_SENSITIVITY_BOOST,
        0.6f
    );
    EXPECT_EQ(result, 0);

    result = perception_training_signal_event(
        bridge,
        PERCEPTION_TRAINING_FEEDBACK_NOVELTY_SEEK,
        0.8f
    );
    EXPECT_EQ(result, 0);

    perception_training_stop(bridge);
}

//=============================================================================
// Full Training Pipeline Integration Tests
//=============================================================================

TEST_F(TrainingPipelineIntegrationTest, AllBridgesCreated) {
    EXPECT_NE(cognitive_bridge, nullptr);
    EXPECT_NE(cortical_bridge, nullptr);
    EXPECT_NE(perception_bridge, nullptr);
}

TEST_F(TrainingPipelineIntegrationTest, AllBridgesStartAndStop) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    EXPECT_EQ(cognitive_training_stop(cognitive_bridge), 0);
    EXPECT_EQ(cortical_training_stop(cortical_bridge), 0);
    EXPECT_EQ(perception_training_stop(perception_bridge), 0);
}

TEST_F(TrainingPipelineIntegrationTest, CrossBridgeConnection) {
    // Connect cognitive bridge to perception bridge
    int result = cognitive_training_connect_perception_training(
        cognitive_bridge,
        perception_bridge
    );
    EXPECT_EQ(result, 0);

    // Connect cognitive bridge to cortical bridge
    result = cognitive_training_connect_cortical_training(
        cognitive_bridge,
        cortical_bridge
    );
    EXPECT_EQ(result, 0);

    // Verify connections via stats
    cognitive_training_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    cognitive_training_get_stats(cognitive_bridge, &stats);
    EXPECT_TRUE(stats.perception_training_connected);
    EXPECT_TRUE(stats.cortical_training_connected);
}

TEST_F(TrainingPipelineIntegrationTest, SimulatedTrainingLoop) {
    // Start all bridges
    cognitive_training_start(cognitive_bridge);
    cortical_training_start(cortical_bridge);
    perception_training_start(perception_bridge);

    // Connect bridges
    cognitive_training_connect_perception_training(cognitive_bridge, perception_bridge);
    cognitive_training_connect_cortical_training(cognitive_bridge, cortical_bridge);

    // Set initial effects for testing
    cognitive_training_effects_t cog_effects;
    memset(&cog_effects, 0, sizeof(cog_effects));
    cog_effects.cognitive_load = 0.5f;
    cog_effects.exploration_drive = 0.4f;
    cog_effects.lr_factor = 0.9f;
    cog_effects.valid = true;
    cognitive_training_set_effects_for_testing(cognitive_bridge, &cog_effects);

    cortical_training_effects_t cort_effects;
    memset(&cort_effects, 0, sizeof(cort_effects));
    cort_effects.free_energy = 3.0f;
    cort_effects.burst_rate = 0.6f;
    cort_effects.lr_factor = 0.95f;
    cort_effects.valid = true;
    cortical_training_set_effects_for_testing(cortical_bridge, &cort_effects);

    perception_training_effects_t perc_effects;
    memset(&perc_effects, 0, sizeof(perc_effects));
    perc_effects.visual_confidence = 0.85f;
    perc_effects.audio_quality = 0.75f;
    perc_effects.lr_factor = 1.05f;
    perc_effects.valid = true;
    perception_training_set_effects_for_testing(perception_bridge, &perc_effects);

    // Simulate training iterations
    const int NUM_ITERATIONS = 10;
    float base_lr = 0.01f;
    std::vector<float> effective_lrs;

    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        // Get modulated LR from each bridge
        float cog_lr = cognitive_training_get_modulated_lr(cognitive_bridge, base_lr);
        float cort_lr = cortical_training_get_modulated_lr(cortical_bridge, base_lr);
        float perc_lr = perception_training_get_modulated_lr(perception_bridge, base_lr);

        // Combine modulations (simple average for testing)
        float effective_lr = (cog_lr + cort_lr + perc_lr) / 3.0f;
        effective_lrs.push_back(effective_lr);

        // Simulate loss decrease
        float loss = 2.0f - (i * 0.15f);
        float grad_norm = 0.2f - (i * 0.01f);

        // Update metrics
        cognitive_training_update_metrics(cognitive_bridge, loss, grad_norm, effective_lr, i);
        cortical_training_update_metrics(cortical_bridge, loss, grad_norm, effective_lr, i);
        perception_training_update_metrics(perception_bridge, loss, grad_norm);

        // Trigger updates
        cognitive_training_update(cognitive_bridge, 100);
        cortical_training_update(cortical_bridge, 100);
        perception_training_update(perception_bridge, 100);
    }

    // Verify we got modulated LRs
    EXPECT_EQ(effective_lrs.size(), static_cast<size_t>(NUM_ITERATIONS));

    // All effective LRs should be in reasonable range
    for (float lr : effective_lrs) {
        EXPECT_GT(lr, 0.0f);
        EXPECT_LT(lr, 0.1f);
    }

    // Stop bridges
    cognitive_training_stop(cognitive_bridge);
    cortical_training_stop(cortical_bridge);
    perception_training_stop(perception_bridge);
}

TEST_F(TrainingPipelineIntegrationTest, FeedbackPropagation) {
    cognitive_training_start(cognitive_bridge);
    cortical_training_start(cortical_bridge);
    perception_training_start(perception_bridge);

    // Signal satisfaction (loss improved)
    cognitive_training_signal_event(
        cognitive_bridge,
        COGNITIVE_TRAINING_FEEDBACK_SATISFACTION,
        0.9f
    );

    // Signal predictions consolidated
    cortical_training_signal_event(
        cortical_bridge,
        CORTICAL_TRAINING_FEEDBACK_CONSOLIDATE,
        0.85f
    );

    // Signal sensitivity boost
    perception_training_signal_event(
        perception_bridge,
        PERCEPTION_TRAINING_FEEDBACK_SENSITIVITY_BOOST,
        0.7f
    );

    // Get stats to verify feedback was registered
    cognitive_training_stats_t cog_stats;
    memset(&cog_stats, 0, sizeof(cog_stats));
    cognitive_training_get_stats(cognitive_bridge, &cog_stats);
    EXPECT_GE(cog_stats.total_feedback_events, 1u);

    cortical_training_stats_t cort_stats;
    memset(&cort_stats, 0, sizeof(cort_stats));
    cortical_training_get_stats(cortical_bridge, &cort_stats);
    EXPECT_GE(cort_stats.total_feedback_events, 1u);

    perception_training_stats_t perc_stats;
    memset(&perc_stats, 0, sizeof(perc_stats));
    perception_training_get_stats(perception_bridge, &perc_stats);
    EXPECT_GE(perc_stats.total_feedback_events, 1u);

    cognitive_training_stop(cognitive_bridge);
    cortical_training_stop(cortical_bridge);
    perception_training_stop(perception_bridge);
}

TEST_F(TrainingPipelineIntegrationTest, HighCognitiveLoadReducesBatch) {
    cognitive_training_start(cognitive_bridge);

    // Set high cognitive load
    cognitive_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.cognitive_load = 0.95f;  // Very high load
    effects.batch_size_factor = 0.5f;  // Should reduce batch
    effects.valid = true;

    cognitive_training_set_effects_for_testing(cognitive_bridge, &effects);

    uint32_t base_batch = 64;
    uint32_t modulated_batch = cognitive_training_get_modulated_batch_size(
        cognitive_bridge,
        base_batch
    );

    // Batch should be reduced under high load
    EXPECT_LT(modulated_batch, base_batch);

    cognitive_training_stop(cognitive_bridge);
}

TEST_F(TrainingPipelineIntegrationTest, LowVisualConfidenceReducesLR) {
    perception_training_start(perception_bridge);

    // Set low visual confidence
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.2f;  // Low confidence
    effects.lr_factor = 0.5f;  // Should reduce LR
    effects.valid = true;

    perception_training_set_effects_for_testing(perception_bridge, &effects);

    float base_lr = 0.01f;
    float modulated_lr = perception_training_get_modulated_lr(perception_bridge, base_lr);

    // LR should be reduced with low confidence
    EXPECT_LT(modulated_lr, base_lr);

    perception_training_stop(perception_bridge);
}

TEST_F(TrainingPipelineIntegrationTest, HighFreeEnergyBoostsLearning) {
    cortical_training_start(cortical_bridge);

    // Set high free energy (learning opportunity)
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.free_energy = 15.0f;  // High free energy
    effects.lr_factor = 1.1f;  // Should boost LR
    effects.valid = true;

    cortical_training_set_effects_for_testing(cortical_bridge, &effects);

    float base_lr = 0.01f;
    float modulated_lr = cortical_training_get_modulated_lr(cortical_bridge, base_lr);

    // LR should be increased with high free energy
    EXPECT_GT(modulated_lr, base_lr * 0.99f);

    cortical_training_stop(cortical_bridge);
}

//=============================================================================
// String Conversion Tests
//=============================================================================

TEST(TrainingBridgeUtilityTest, CognitiveModulationToString) {
    EXPECT_STREQ(
        cognitive_training_modulation_to_string(COGNITIVE_TRAINING_MODULATION_LR),
        "LR"
    );
    EXPECT_STREQ(
        cognitive_training_modulation_to_string(COGNITIVE_TRAINING_MODULATION_BATCH_SIZE),
        "BATCH_SIZE"
    );
}

TEST(TrainingBridgeUtilityTest, CorticalModulationToString) {
    EXPECT_STREQ(
        cortical_training_modulation_to_string(CORTICAL_TRAINING_MODULATION_LR),
        "LR"
    );
    EXPECT_STREQ(
        cortical_training_modulation_to_string(CORTICAL_TRAINING_MODULATION_GRADIENT_CONFIDENCE),
        "GRADIENT_CONFIDENCE"
    );
}

TEST(TrainingBridgeUtilityTest, PerceptionModulationToString) {
    EXPECT_STREQ(
        perception_training_modulation_to_string(PERCEPTION_TRAINING_MODULATION_LR),
        "LR"
    );
    EXPECT_STREQ(
        perception_training_modulation_to_string(PERCEPTION_TRAINING_MODULATION_SKIP_SAMPLE),
        "SKIP_SAMPLE"
    );
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
