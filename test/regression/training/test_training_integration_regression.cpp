/**
 * @file test_training_integration_regression.cpp
 * @brief Regression tests for training integration module
 *
 * WHAT: Regression tests guarding against training integration edge-case bugs
 * WHY:  Prevent regressions in hub lifecycle, event delivery, module registration,
 *       brain training context operations, LR scheduling, gradient management,
 *       and statistics tracking under repeated/stress usage
 * HOW:  GTest executable linked against nimcp library
 *
 * @author NIMCP Development Team
 * @date 2026-02-25
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <atomic>

extern "C" {
#include "training/integration/nimcp_training_integration_hub.h"
#include "training/integration/nimcp_training_event_types.h"
#include "middleware/training/nimcp_brain_training_integration.h"
}

// =============================================================================
// Hub Lifecycle Regression Fixture
// =============================================================================

class TrainingHubRegression : public ::testing::Test {
protected:
    training_integration_hub_t hub = nullptr;

    void SetUp() override {
        training_hub_config_t config = training_hub_default_config();
        config.enable_async = false;  // Synchronous for deterministic regression tests
        config.enable_metrics = true;
        hub = training_hub_create(&config);
        ASSERT_NE(hub, nullptr) << "Failed to create training integration hub";
    }

    void TearDown() override {
        if (hub) {
            training_hub_destroy(hub);
            hub = nullptr;
        }
    }
};

// =============================================================================
// Brain Training Context Regression Fixture
// =============================================================================

class BrainTrainingRegression : public ::testing::Test {
protected:
    nimcp_brain_training_ctx_t* ctx = nullptr;

    void SetUp() override {
        nimcp_brain_training_config_t config = nimcp_brain_training_default_config();
        config.enable_security = false;
        config.enable_plasticity_bridge = false;
        config.enable_training_callbacks = false;
        config.enable_second_messengers = false;
        config.enable_portia_integration = false;
        ctx = nimcp_brain_training_create(&config);
        ASSERT_NE(ctx, nullptr) << "Failed to create brain training context";
    }

    void TearDown() override {
        if (ctx) {
            nimcp_brain_training_destroy(ctx);
            ctx = nullptr;
        }
    }
};

// =============================================================================
// Test 1: RepeatedModuleRegistrationStable
// Register and unregister modules 100 times — hub stays consistent, no crash
// =============================================================================

TEST_F(TrainingHubRegression, RepeatedModuleRegistrationStable) {
    for (int i = 0; i < 100; i++) {
        uint32_t mod_id = 0x5000 + (uint32_t)i;
        int rc = training_hub_register_module(
            hub, mod_id, TRAINING_CATEGORY_OPTIMIZATION,
            "repeated_module", nullptr);
        ASSERT_EQ(rc, 0) << "Registration failed at iteration " << i;

        rc = training_hub_unregister_module(hub, mod_id);
        ASSERT_EQ(rc, 0) << "Unregistration failed at iteration " << i;
    }

    // Hub should have 0 registered modules after all unregistrations
    training_hub_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    int rc = training_hub_get_stats(hub, &stats);
    ASSERT_EQ(rc, 0);
    EXPECT_EQ(stats.registered_modules, 0u);
}

// =============================================================================
// Test 2: EventPublishWithNoSubscribersNoCrash
// Publishing events with no subscribers should succeed gracefully
// =============================================================================

TEST_F(TrainingHubRegression, EventPublishWithNoSubscribersNoCrash) {
    // Register a publisher module
    int rc = training_hub_register_module(
        hub, 0x3001, TRAINING_CATEGORY_DATA, "publisher", nullptr);
    ASSERT_EQ(rc, 0);

    // Publish 50 events with no subscribers — should not crash
    for (int i = 0; i < 50; i++) {
        training_event_data_t event_data;
        memset(&event_data, 0, sizeof(event_data));
        event_data.event_type = TRAINING_EVENT_LOSS_COMPUTED;
        event_data.source_module_id = 0x3001;
        event_data.loss_value = 1.0f / (float)(i + 1);
        event_data.epoch = (uint32_t)i;

        rc = training_hub_publish(hub, 0x3001, TRAINING_EVENT_LOSS_COMPUTED, &event_data);
        EXPECT_EQ(rc, 0) << "Publish failed at iteration " << i;
    }

    training_hub_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    rc = training_hub_get_stats(hub, &stats);
    ASSERT_EQ(rc, 0);
    EXPECT_GE(stats.events_published, 50u);
}

// =============================================================================
// Test 3: SubscribeUnsubscribeRepeatedNoCrash
// Subscribe and unsubscribe the same callback 100 times — no crash or leak
// =============================================================================

static int dummy_callback(const training_event_data_t* event, void* user_data) {
    (void)event;
    (void)user_data;
    return 0;
}

TEST_F(TrainingHubRegression, SubscribeUnsubscribeRepeatedNoCrash) {
    int rc = training_hub_register_module(
        hub, 0x3010, TRAINING_CATEGORY_CURRICULUM, "sub_module", nullptr);
    ASSERT_EQ(rc, 0);

    for (int i = 0; i < 100; i++) {
        rc = training_hub_subscribe(
            hub, 0x3010, TRAINING_EVENT_DIFFICULTY_UPDATED,
            dummy_callback, nullptr);
        EXPECT_EQ(rc, 0) << "Subscribe failed at iteration " << i;

        rc = training_hub_unsubscribe(
            hub, 0x3010, TRAINING_EVENT_DIFFICULTY_UPDATED);
        EXPECT_EQ(rc, 0) << "Unsubscribe failed at iteration " << i;
    }

    training_hub_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    rc = training_hub_get_stats(hub, &stats);
    ASSERT_EQ(rc, 0);
    EXPECT_EQ(stats.active_subscriptions, 0u);
}

// =============================================================================
// Test 4: StatsResetClearsAllCounters
// After resetting stats, all counters should be zero
// =============================================================================

TEST_F(TrainingHubRegression, StatsResetClearsAllCounters) {
    // Generate some activity first
    int rc = training_hub_register_module(
        hub, 0x3020, TRAINING_CATEGORY_OPTIMIZATION, "stats_mod", nullptr);
    ASSERT_EQ(rc, 0);

    training_event_data_t event_data;
    memset(&event_data, 0, sizeof(event_data));
    event_data.event_type = TRAINING_EVENT_EPOCH_COMPLETE;
    event_data.source_module_id = 0x3020;

    for (int i = 0; i < 10; i++) {
        training_hub_publish(hub, 0x3020, TRAINING_EVENT_EPOCH_COMPLETE, &event_data);
    }

    // Reset and verify
    rc = training_hub_reset_stats(hub);
    ASSERT_EQ(rc, 0);

    training_hub_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));  // Fill with garbage to detect non-zeroing
    rc = training_hub_get_stats(hub, &stats);
    ASSERT_EQ(rc, 0);
    EXPECT_EQ(stats.events_published, 0u);
    EXPECT_EQ(stats.events_delivered, 0u);
    EXPECT_EQ(stats.events_dropped, 0u);
    EXPECT_EQ(stats.queries_processed, 0u);
    EXPECT_EQ(stats.queries_failed, 0u);
}

// =============================================================================
// Test 5: DifficultyUpdateHelperStable
// Publish difficulty updates 50 times with varying values — no NaN, no crash
// =============================================================================

TEST_F(TrainingHubRegression, DifficultyUpdateHelperStable) {
    int rc = training_hub_register_module(
        hub, 0x3030, TRAINING_CATEGORY_CURRICULUM, "difficulty_mod", nullptr);
    ASSERT_EQ(rc, 0);

    for (int i = 0; i < 50; i++) {
        float old_diff = (float)i / 50.0f;
        float new_diff = (float)(i + 1) / 50.0f;

        rc = training_hub_publish_difficulty_update(hub, 0x3030, old_diff, new_diff);
        EXPECT_EQ(rc, 0) << "Difficulty update failed at iteration " << i;
    }

    training_hub_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    rc = training_hub_get_stats(hub, &stats);
    ASSERT_EQ(rc, 0);
    EXPECT_GE(stats.difficulty_updates, 50u);
}

// =============================================================================
// Test 6: LossAndLrHelpersMultipleRounds
// Publish loss and LR adjustment events in alternating pattern — stable stats
// =============================================================================

TEST_F(TrainingHubRegression, LossAndLrHelpersMultipleRounds) {
    int rc = training_hub_register_module(
        hub, 0x3040, TRAINING_CATEGORY_OPTIMIZATION, "loss_lr_mod", nullptr);
    ASSERT_EQ(rc, 0);

    for (int i = 0; i < 30; i++) {
        float loss = 1.0f - (float)i * 0.03f;
        rc = training_hub_publish_loss(hub, 0x3040, (uint32_t)i, 0, loss);
        EXPECT_EQ(rc, 0) << "Loss publish failed at iteration " << i;

        float old_lr = 0.01f * (1.0f - (float)i * 0.01f);
        float new_lr = 0.01f * (1.0f - (float)(i + 1) * 0.01f);
        rc = training_hub_publish_lr_adjustment(hub, 0x3040, old_lr, new_lr);
        EXPECT_EQ(rc, 0) << "LR adjustment failed at iteration " << i;
    }

    training_hub_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    rc = training_hub_get_stats(hub, &stats);
    ASSERT_EQ(rc, 0);
    EXPECT_GE(stats.lr_adjustments, 30u);
}

// =============================================================================
// Test 7: BrainTrainingModeSetGetRoundTrip
// Set training mode and get it back — verify round-trip consistency
// =============================================================================

TEST_F(BrainTrainingRegression, ModeSetGetRoundTrip) {
    nimcp_training_mode_t modes[] = {
        NIMCP_TRAINING_MODE_TRAIN,
        NIMCP_TRAINING_MODE_EVAL,
        NIMCP_TRAINING_MODE_INFERENCE,
        NIMCP_TRAINING_MODE_TRAIN
    };

    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
        nimcp_result_t rc = nimcp_brain_training_set_mode(ctx, modes[i]);
        EXPECT_EQ(rc, NIMCP_SUCCESS) << "Set mode failed for mode " << (int)modes[i];

        nimcp_training_mode_t got = nimcp_brain_training_get_mode(ctx);
        EXPECT_EQ(got, modes[i]) << "Round-trip mode mismatch at iteration " << i;
    }
}

// =============================================================================
// Test 8: BrainTrainingStatsUpdateNeverNaN
// Update stats 100 times with varying loss values — no NaN in aggregates
// =============================================================================

TEST_F(BrainTrainingRegression, StatsUpdateNeverNaN) {
    for (int i = 0; i < 100; i++) {
        float loss = 1.0f / (float)(i + 1);  // Decreasing loss: 1.0, 0.5, 0.33, ...
        nimcp_brain_training_update_stats(ctx, 32, loss);
    }

    nimcp_training_session_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    nimcp_result_t rc = nimcp_brain_training_get_stats(ctx, &stats);
    EXPECT_EQ(rc, NIMCP_SUCCESS);
    EXPECT_FALSE(std::isnan(stats.avg_loss)) << "avg_loss is NaN after 100 updates";
    EXPECT_FALSE(std::isnan(stats.min_loss)) << "min_loss is NaN after 100 updates";
    EXPECT_FALSE(std::isnan(stats.max_loss)) << "max_loss is NaN after 100 updates";
    EXPECT_FALSE(std::isnan(stats.total_loss)) << "total_loss is NaN after 100 updates";
    EXPECT_FALSE(std::isnan(stats.loss_variance)) << "loss_variance is NaN after 100 updates";
    EXPECT_GE(stats.total_batches, 100u);
    EXPECT_GE(stats.total_samples, 3200u);  // 100 * 32
}

// =============================================================================
// Test 9: EarlyStopResetIdempotent
// Reset early stopping state 50 times — should never crash
// =============================================================================

TEST_F(BrainTrainingRegression, EarlyStopResetIdempotent) {
    for (int i = 0; i < 50; i++) {
        nimcp_brain_training_reset_early_stop(ctx);
    }

    // After reset, early stop should not be triggered
    bool should_stop = nimcp_brain_training_check_early_stop(ctx, 0.5f);
    EXPECT_FALSE(should_stop) << "Early stop triggered immediately after reset";
}

// =============================================================================
// Test 10: GradientClipNeverProducesNaN
// Clip gradients with various thresholds — result should never be NaN
// =============================================================================

TEST_F(BrainTrainingRegression, GradientClipNeverProducesNaN) {
    const size_t count = 64;
    std::vector<float> gradients(count);

    // Fill with large gradients that need clipping
    for (size_t i = 0; i < count; i++) {
        gradients[i] = (float)(i + 1) * 10.0f;  // 10, 20, 30, ...
    }

    float thresholds[] = {0.001f, 0.1f, 1.0f, 10.0f, 100.0f, 1000.0f};
    for (float threshold : thresholds) {
        std::vector<float> grads_copy = gradients;
        float clip_ratio = nimcp_brain_training_clip_gradients(
            ctx, grads_copy.data(), count, NIMCP_CLIP_BY_NORM, threshold);

        EXPECT_FALSE(std::isnan(clip_ratio))
            << "Clip ratio is NaN for threshold " << threshold;

        for (size_t i = 0; i < count; i++) {
            EXPECT_FALSE(std::isnan(grads_copy[i]))
                << "Gradient[" << i << "] is NaN after clipping with threshold " << threshold;
            EXPECT_FALSE(std::isinf(grads_copy[i]))
                << "Gradient[" << i << "] is Inf after clipping with threshold " << threshold;
        }
    }
}

// =============================================================================
// Test 11: DropoutRateZeroPassesThrough
// Dropout with rate 0.0 should pass all values through unchanged
// =============================================================================

TEST_F(BrainTrainingRegression, DropoutRateZeroPassesThrough) {
    const size_t count = 128;
    std::vector<float> input(count);
    std::vector<float> output(count, 0.0f);

    for (size_t i = 0; i < count; i++) {
        input[i] = (float)(i + 1);
    }

    nimcp_brain_training_set_mode(ctx, NIMCP_TRAINING_MODE_TRAIN);
    uint64_t dropped = nimcp_brain_training_apply_dropout(
        ctx, input.data(), output.data(), nullptr, count, 0.0f);

    EXPECT_EQ(dropped, 0u) << "Elements were dropped with rate 0.0";

    for (size_t i = 0; i < count; i++) {
        EXPECT_FLOAT_EQ(output[i], input[i])
            << "Dropout with rate 0.0 modified value at index " << i;
    }
}

// =============================================================================
// Test 12: MultiEventTypeDeliveryToSubscriber
// Subscribe to multiple event types, publish each — verify all delivered
// =============================================================================

static std::atomic<int> g_event_count{0};

static int counting_callback(const training_event_data_t* event, void* user_data) {
    (void)event;
    (void)user_data;
    g_event_count.fetch_add(1);
    return 0;
}

TEST_F(TrainingHubRegression, MultiEventTypeDeliveryToSubscriber) {
    g_event_count.store(0);

    int rc = training_hub_register_module(
        hub, 0x3050, TRAINING_CATEGORY_DATA, "multi_sub", nullptr);
    ASSERT_EQ(rc, 0);

    // Subscribe to 5 different event types
    training_event_type_t types[] = {
        TRAINING_EVENT_LOSS_COMPUTED,
        TRAINING_EVENT_EPOCH_COMPLETE,
        TRAINING_EVENT_BATCH_COMPLETE,
        TRAINING_EVENT_WEIGHTS_UPDATED,
        TRAINING_EVENT_LR_ADJUSTED
    };

    for (auto type : types) {
        rc = training_hub_subscribe(hub, 0x3050, type, counting_callback, nullptr);
        ASSERT_EQ(rc, 0) << "Subscribe failed for event type " << (int)type;
    }

    // Publish one event of each type
    for (auto type : types) {
        training_event_data_t data;
        memset(&data, 0, sizeof(data));
        data.event_type = type;
        data.source_module_id = 0x3050;

        rc = training_hub_publish(hub, 0x3050, type, &data);
        EXPECT_EQ(rc, 0) << "Publish failed for type " << (int)type;
    }

    EXPECT_EQ(g_event_count.load(), 5) << "Not all events were delivered";
}
