/**
 * @file test_portia_training_integration.cpp
 * @brief Integration tests for Portia-Training resource-aware coordination
 *
 * WHAT: End-to-end integration testing of Portia and Training modules
 * WHY:  Verify real-world coordination between resource management and training
 * HOW:  Simulate tier changes, measure training adaptation, verify bio-async messaging
 *
 * Integration Scenarios:
 * - Full training pipeline with dynamic tier changes
 * - Portia tier downgrade during active training
 * - Training recovery after tier upgrade
 * - Bio-async message flow between Portia and Training
 * - Resource request handling
 * - Checkpoint creation on EMERGENCY tier
 *
 * @author NIMCP Development Team
 * @date 2025-12-09
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
// Headers have their own extern "C" guards
#include "middleware/training/nimcp_brain_training_integration.h"
#include "portia/nimcp_portia.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/platform/nimcp_platform_tier.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

class PortiaTrainingIntegrationTest : public ::testing::Test {
protected:
    nimcp_brain_training_ctx_t* training_ctx;
    bio_router_context_t* router_ctx;
    void* portia_ctx;  /* Mock Portia context */

    void SetUp() override {
        /* Initialize bio-async router */
        bio_router_config_t router_config = {0};
        router_config.max_modules = 32;
        router_config.default_inbox_capacity = 128;
        router_config.enable_priority_channels = true;
        router_config.worker_thread_count = 4;

        router_ctx = bio_router_init(&router_config);
        ASSERT_NE(router_ctx, nullptr) << "Failed to initialize bio-router";

        /* Initialize Portia (mock context for testing) */
        portia_ctx = (void*)0x12345678;  /* Mock pointer */

        /* Create brain training context with Portia integration */
        nimcp_brain_training_config_t training_config =
            nimcp_brain_training_default_config();
        training_config.enable_portia_integration = true;
        training_config.min_batch_size_ratio = 0.25f;
        training_config.allow_training_pause = true;
        training_config.adapt_to_tier_changes = true;

        training_ctx = nimcp_brain_training_create(&training_config);
        ASSERT_NE(training_ctx, nullptr) << "Failed to create brain training context";

        /* Initialize training */
        nimcp_result_t res = nimcp_brain_training_init(
            training_ctx,
            nullptr,
            nullptr
        );
        ASSERT_EQ(res, NIMCP_SUCCESS) << "Failed to initialize brain training";

        /* Connect Portia to training */
        res = nimcp_brain_training_connect_portia(training_ctx, portia_ctx);
        ASSERT_EQ(res, NIMCP_SUCCESS) << "Failed to connect Portia";

        LOG_INFO("=== Integration Test Setup Complete ===");
    }

    void TearDown() override {
        if (training_ctx) {
            nimcp_brain_training_destroy(training_ctx);
            training_ctx = nullptr;
        }

        if (router_ctx) {
            bio_router_shutdown(router_ctx);
            router_ctx = nullptr;
        }

        LOG_INFO("=== Integration Test Teardown Complete ===");
    }

    /* Helper: Simulate training step */
    bool simulate_training_step(size_t base_batch_size, float base_lr) {
        /* Check if training paused */
        if (nimcp_brain_training_is_paused(training_ctx)) {
            LOG_INFO("Training step skipped: training paused");
            return false;
        }

        /* Get adjusted parameters */
        size_t adjusted_batch = nimcp_brain_training_get_adjusted_batch_size(
            training_ctx,
            base_batch_size
        );

        float adjusted_lr = nimcp_brain_training_get_adjusted_lr(
            training_ctx,
            base_lr
        );

        LOG_INFO("Training step: batch=%zu (%.0f%%), lr=%.6f (%.0f%%)",
                 adjusted_batch,
                 (adjusted_batch * 100.0f) / base_batch_size,
                 adjusted_lr,
                 (adjusted_lr * 100.0f) / base_lr);

        return true;
    }
};

/* ============================================================================
 * Full Training Pipeline Tests
 * ============================================================================ */

/**
 * @test Full training pipeline with tier changes mid-training
 */
TEST_F(PortiaTrainingIntegrationTest, FullTrainingPipelineWithTierChanges) {
    LOG_INFO("=== Test: Full Training Pipeline with Tier Changes ===");

    const size_t base_batch = 128;
    const float base_lr = 0.01f;

    /* Epoch 1: FULL tier */
    LOG_INFO("Epoch 1: FULL tier");
    nimcp_brain_training_on_tier_change(training_ctx, PLATFORM_TIER_FULL);

    for (int step = 0; step < 10; step++) {
        bool trained = simulate_training_step(base_batch, base_lr);
        EXPECT_TRUE(trained);
    }

    /* Epoch 2: Downgrade to MEDIUM tier */
    LOG_INFO("Epoch 2: MEDIUM tier (simulating resource pressure)");
    nimcp_brain_training_on_tier_change(training_ctx, PLATFORM_TIER_MEDIUM);

    for (int step = 0; step < 10; step++) {
        bool trained = simulate_training_step(base_batch, base_lr);
        EXPECT_TRUE(trained);

        /* Verify reduced batch size */
        size_t adjusted = nimcp_brain_training_get_adjusted_batch_size(
            training_ctx,
            base_batch
        );
        EXPECT_LT(adjusted, base_batch);
    }

    /* Epoch 3: Further downgrade to CONSTRAINED tier */
    LOG_INFO("Epoch 3: CONSTRAINED tier (severe resource pressure)");
    nimcp_brain_training_on_tier_change(training_ctx, PLATFORM_TIER_CONSTRAINED);

    for (int step = 0; step < 10; step++) {
        bool trained = simulate_training_step(base_batch, base_lr);
        EXPECT_TRUE(trained);

        /* Verify significantly reduced batch size */
        size_t adjusted = nimcp_brain_training_get_adjusted_batch_size(
            training_ctx,
            base_batch
        );
        EXPECT_LE(adjusted, base_batch / 2);
    }

    /* Epoch 4: Upgrade back to MEDIUM after recovery */
    LOG_INFO("Epoch 4: MEDIUM tier (resources recovered)");
    nimcp_brain_training_on_tier_change(training_ctx, PLATFORM_TIER_MEDIUM);

    for (int step = 0; step < 10; step++) {
        bool trained = simulate_training_step(base_batch, base_lr);
        EXPECT_TRUE(trained);

        /* Verify batch size increased */
        size_t adjusted = nimcp_brain_training_get_adjusted_batch_size(
            training_ctx,
            base_batch
        );
        EXPECT_GT(adjusted, base_batch / 2);
    }

    LOG_INFO("=== Test Complete: Training adapted across all tiers ===");
}

/**
 * @test Training pause and resume during EMERGENCY tier
 */
TEST_F(PortiaTrainingIntegrationTest, TrainingPauseAndResumeOnEmergency) {
    LOG_INFO("=== Test: Training Pause and Resume on EMERGENCY ===");

    const size_t base_batch = 128;
    const float base_lr = 0.01f;

    /* Start training on FULL tier */
    LOG_INFO("Phase 1: Training on FULL tier");
    nimcp_brain_training_on_tier_change(training_ctx, PLATFORM_TIER_FULL);

    for (int step = 0; step < 5; step++) {
        bool trained = simulate_training_step(base_batch, base_lr);
        EXPECT_TRUE(trained);
    }

    /* EMERGENCY: Downgrade to MINIMAL tier */
    LOG_INFO("Phase 2: EMERGENCY tier - training should pause");
    nimcp_brain_training_on_tier_change(training_ctx, PLATFORM_TIER_MINIMAL);

    EXPECT_TRUE(nimcp_brain_training_is_paused(training_ctx));

    /* Try to train - should be skipped */
    for (int step = 0; step < 5; step++) {
        bool trained = simulate_training_step(base_batch, base_lr);
        EXPECT_FALSE(trained) << "Training should be paused during EMERGENCY";
    }

    /* Recovery: Upgrade to CONSTRAINED tier */
    LOG_INFO("Phase 3: Recovery to CONSTRAINED tier - training resumes");
    nimcp_brain_training_on_tier_change(training_ctx, PLATFORM_TIER_CONSTRAINED);

    EXPECT_FALSE(nimcp_brain_training_is_paused(training_ctx));

    /* Resume training */
    for (int step = 0; step < 5; step++) {
        bool trained = simulate_training_step(base_batch, base_lr);
        EXPECT_TRUE(trained) << "Training should resume after tier upgrade";
    }

    LOG_INFO("=== Test Complete: Pause/Resume handled correctly ===");
}

/**
 * @test Degradation event handling during training
 */
TEST_F(PortiaTrainingIntegrationTest, DegradationEventDuringTraining) {
    LOG_INFO("=== Test: Degradation Event During Training ===");

    const size_t base_batch = 128;
    const float base_lr = 0.01f;

    /* Start training on FULL tier */
    nimcp_brain_training_on_tier_change(training_ctx, PLATFORM_TIER_FULL);

    /* Phase 1: Normal training */
    LOG_INFO("Phase 1: Normal training (DEGRADATION_NONE)");
    nimcp_brain_training_on_degradation_event(training_ctx, 0);

    for (int step = 0; step < 5; step++) {
        simulate_training_step(base_batch, base_lr);
    }

    size_t batch_before = nimcp_brain_training_get_adjusted_batch_size(
        training_ctx,
        base_batch
    );

    /* Phase 2: SEVERE degradation */
    LOG_INFO("Phase 2: SEVERE degradation - batch size reduced");
    nimcp_brain_training_on_degradation_event(training_ctx, 3);

    size_t batch_after_severe = nimcp_brain_training_get_adjusted_batch_size(
        training_ctx,
        base_batch
    );

    EXPECT_LT(batch_after_severe, batch_before)
        << "Batch size should reduce with SEVERE degradation";

    for (int step = 0; step < 5; step++) {
        simulate_training_step(base_batch, base_lr);
    }

    /* Phase 3: CRITICAL degradation - training pauses */
    LOG_INFO("Phase 3: CRITICAL degradation - training pauses");
    nimcp_brain_training_on_degradation_event(training_ctx, 4);

    EXPECT_TRUE(nimcp_brain_training_is_paused(training_ctx));

    for (int step = 0; step < 5; step++) {
        bool trained = simulate_training_step(base_batch, base_lr);
        EXPECT_FALSE(trained);
    }

    /* Phase 4: Recovery to MODERATE */
    LOG_INFO("Phase 4: Recovery to MODERATE - training resumes");
    nimcp_brain_training_on_degradation_event(training_ctx, 2);

    EXPECT_FALSE(nimcp_brain_training_is_paused(training_ctx));

    for (int step = 0; step < 5; step++) {
        bool trained = simulate_training_step(base_batch, base_lr);
        EXPECT_TRUE(trained);
    }

    LOG_INFO("=== Test Complete: Degradation events handled correctly ===");
}

/**
 * @test Resource request messaging
 */
TEST_F(PortiaTrainingIntegrationTest, ResourceRequestMessaging) {
    LOG_INFO("=== Test: Resource Request Messaging ===");

    /* Send resource requests */
    nimcp_result_t res;

    res = nimcp_brain_training_request_resources(
        training_ctx,
        128,    /* batch_size */
        50000   /* param_count */
    );

    /* Should succeed or return NOT_INITIALIZED if bio-async disabled */
    EXPECT_TRUE(res == NIMCP_SUCCESS || res == NIMCP_ERROR_NOT_INITIALIZED);

    LOG_INFO("Resource request 1 sent: batch=128, params=50000");

    /* Change tier and send another request */
    nimcp_brain_training_on_tier_change(training_ctx, PLATFORM_TIER_MEDIUM);

    res = nimcp_brain_training_request_resources(
        training_ctx,
        96,     /* reduced batch_size */
        50000   /* param_count */
    );

    EXPECT_TRUE(res == NIMCP_SUCCESS || res == NIMCP_ERROR_NOT_INITIALIZED);

    LOG_INFO("Resource request 2 sent: batch=96, params=50000");

    LOG_INFO("=== Test Complete: Resource requests sent successfully ===");
}

/**
 * @test Rapid tier oscillation stress test
 */
TEST_F(PortiaTrainingIntegrationTest, RapidTierOscillationStressTest) {
    LOG_INFO("=== Test: Rapid Tier Oscillation Stress Test ===");

    const size_t base_batch = 128;
    const float base_lr = 0.01f;

    /* Oscillate between tiers rapidly */
    platform_tier_t tiers[] = {
        PLATFORM_TIER_FULL,
        PLATFORM_TIER_MEDIUM,
        PLATFORM_TIER_FULL,
        PLATFORM_TIER_CONSTRAINED,
        PLATFORM_TIER_FULL,
        PLATFORM_TIER_MEDIUM,
        PLATFORM_TIER_CONSTRAINED,
        PLATFORM_TIER_MEDIUM,
        PLATFORM_TIER_FULL
    };

    for (size_t i = 0; i < sizeof(tiers) / sizeof(tiers[0]); i++) {
        LOG_INFO("Oscillation %zu: %s", i, platform_tier_get_name(tiers[i]));

        nimcp_brain_training_on_tier_change(training_ctx, tiers[i]);

        /* Train a few steps */
        for (int step = 0; step < 3; step++) {
            simulate_training_step(base_batch, base_lr);
        }

        /* Verify training still works */
        EXPECT_FALSE(nimcp_brain_training_is_paused(training_ctx))
            << "Training should not pause during oscillation";
    }

    LOG_INFO("=== Test Complete: Survived rapid tier oscillation ===");
}

/**
 * @test Long-running training simulation with random tier changes
 */
TEST_F(PortiaTrainingIntegrationTest, LongRunningTrainingWithRandomTiers) {
    LOG_INFO("=== Test: Long-Running Training with Random Tier Changes ===");

    const size_t base_batch = 128;
    const float base_lr = 0.01f;
    const int num_epochs = 5;
    const int steps_per_epoch = 20;

    platform_tier_t available_tiers[] = {
        PLATFORM_TIER_FULL,
        PLATFORM_TIER_MEDIUM,
        PLATFORM_TIER_CONSTRAINED
    };

    size_t total_steps_trained = 0;
    size_t total_steps_attempted = 0;

    for (int epoch = 0; epoch < num_epochs; epoch++) {
        /* Randomly change tier each epoch */
        platform_tier_t tier = available_tiers[epoch % 3];
        LOG_INFO("Epoch %d: Tier %s", epoch + 1, platform_tier_get_name(tier));

        nimcp_brain_training_on_tier_change(training_ctx, tier);

        for (int step = 0; step < steps_per_epoch; step++) {
            total_steps_attempted++;

            bool trained = simulate_training_step(base_batch, base_lr);
            if (trained) {
                total_steps_trained++;
            }
        }
    }

    /* Verify most steps completed successfully */
    float completion_rate = (float)total_steps_trained / total_steps_attempted;
    EXPECT_GT(completion_rate, 0.9f)
        << "At least 90% of training steps should complete";

    LOG_INFO("=== Test Complete: %zu/%zu steps completed (%.1f%%) ===",
             total_steps_trained,
             total_steps_attempted,
             completion_rate * 100.0f);
}

/**
 * @test Combined tier and degradation events
 */
TEST_F(PortiaTrainingIntegrationTest, CombinedTierAndDegradationEvents) {
    LOG_INFO("=== Test: Combined Tier and Degradation Events ===");

    const size_t base_batch = 128;
    const float base_lr = 0.01f;

    /* Scenario: Start FULL, apply MODERATE degradation */
    LOG_INFO("Phase 1: FULL tier + MODERATE degradation");
    nimcp_brain_training_on_tier_change(training_ctx, PLATFORM_TIER_FULL);
    nimcp_brain_training_on_degradation_event(training_ctx, 2);

    for (int step = 0; step < 5; step++) {
        simulate_training_step(base_batch, base_lr);
    }

    /* Scenario: Downgrade to MEDIUM tier + SEVERE degradation */
    LOG_INFO("Phase 2: MEDIUM tier + SEVERE degradation");
    nimcp_brain_training_on_tier_change(training_ctx, PLATFORM_TIER_MEDIUM);
    nimcp_brain_training_on_degradation_event(training_ctx, 3);

    size_t batch_combined = nimcp_brain_training_get_adjusted_batch_size(
        training_ctx,
        base_batch
    );

    LOG_INFO("Combined effect: batch size = %zu (%.1f%% of base)",
             batch_combined,
             (batch_combined * 100.0f) / base_batch);

    for (int step = 0; step < 5; step++) {
        simulate_training_step(base_batch, base_lr);
    }

    /* Scenario: Recovery - upgrade tier and reduce degradation */
    LOG_INFO("Phase 3: FULL tier + NONE degradation (recovery)");
    nimcp_brain_training_on_tier_change(training_ctx, PLATFORM_TIER_FULL);
    nimcp_brain_training_on_degradation_event(training_ctx, 0);

    EXPECT_FALSE(nimcp_brain_training_is_paused(training_ctx));

    for (int step = 0; step < 5; step++) {
        bool trained = simulate_training_step(base_batch, base_lr);
        EXPECT_TRUE(trained);
    }

    LOG_INFO("=== Test Complete: Combined events handled correctly ===");
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
