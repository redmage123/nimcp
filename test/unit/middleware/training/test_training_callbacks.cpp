/**
 * @file test_training_callbacks.cpp
 * @brief Unit tests for Training Callbacks Module (Phase TCB-1)
 *
 * Tests cover:
 * - Callback lifecycle (create, register, unregister, destroy)
 * - Event firing and callback execution
 * - Callback actions (CONTINUE, STOP, SKIP, ROLLBACK, LR adjustments)
 * - Priority ordering
 * - Metrics tracking and updates
 * - Early stopping detection
 * - Checkpoint management
 * - Statistics collection
 * - Security integration
 * - Memory pool integration
 * - Thread safety
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <atomic>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_training_callbacks.h"
#include "security/nimcp_security_integration.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/validation/nimcp_common.h"

namespace {

constexpr float EPSILON = 1e-5f;

// Global counters for callback verification
std::atomic<int> g_callback_count{0};
std::atomic<int> g_step_complete_count{0};
std::atomic<int> g_loss_computed_count{0};
std::atomic<float> g_last_loss{0.0f};

/**
 * @brief Test fixture for callback unit tests
 */
class TrainingCallbacksTest : public ::testing::Test {
protected:
    tcb_context_t* ctx = nullptr;
    nimcp_sec_integration_t* security_ctx = nullptr;

    void SetUp() override {
        // Reset global counters
        g_callback_count.store(0);
        g_step_complete_count.store(0);
        g_loss_computed_count.store(0);
        g_last_loss.store(0.0f);

        // Initialize security context (optional)
        security_ctx = nimcp_sec_integration_create();
        if (security_ctx) {
            nimcp_sec_integration_config_t sec_cfg = nimcp_sec_integration_default_config();
            nimcp_sec_integration_init(security_ctx, &sec_cfg);
        }
    }

    void TearDown() override {
        if (ctx) {
            tcb_destroy(ctx);
            ctx = nullptr;
        }
        if (security_ctx) {
            nimcp_sec_integration_destroy(security_ctx);
            security_ctx = nullptr;
        }
    }

    void CreateDefaultContext() {
        tcb_config_t config = tcb_config_default();
        ctx = tcb_create(&config);
        ASSERT_NE(ctx, nullptr);
    }

    void CreateContextWithSecurity() {
        tcb_config_t config = tcb_config_default();
        config.security_ctx = security_ctx;
        ctx = tcb_create(&config);
        ASSERT_NE(ctx, nullptr);
    }
};

// =============================================================================
// Basic Callback Functions for Testing
// =============================================================================

static tcb_action_t simple_callback(const tcb_event_t* event) {
    (void)event;
    g_callback_count++;
    return TCB_ACTION_CONTINUE;
}

static tcb_action_t step_complete_callback(const tcb_event_t* event) {
    g_step_complete_count++;
    if (event) {
        g_last_loss.store(event->metrics.loss);
    }
    return TCB_ACTION_CONTINUE;
}

static tcb_action_t loss_computed_callback(const tcb_event_t* event) {
    g_loss_computed_count++;
    if (event) {
        g_last_loss.store(event->metrics.loss);
    }
    return TCB_ACTION_CONTINUE;
}

static tcb_action_t stop_callback(const tcb_event_t* event) {
    (void)event;
    g_callback_count++;
    return TCB_ACTION_STOP_TRAINING;
}

static tcb_action_t skip_callback(const tcb_event_t* event) {
    (void)event;
    g_callback_count++;
    return TCB_ACTION_SKIP_STEP;
}

static tcb_action_t reduce_lr_callback(const tcb_event_t* event) {
    (void)event;
    g_callback_count++;
    return TCB_ACTION_REDUCE_LR;
}

static tcb_action_t increase_lr_callback(const tcb_event_t* event) {
    (void)event;
    g_callback_count++;
    return TCB_ACTION_INCREASE_LR;
}

static tcb_action_t rollback_callback(const tcb_event_t* event) {
    (void)event;
    g_callback_count++;
    return TCB_ACTION_ROLLBACK;
}

// =============================================================================
// Lifecycle Tests
// =============================================================================

TEST_F(TrainingCallbacksTest, Create_DefaultConfig) {
    ctx = tcb_create(nullptr);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(TrainingCallbacksTest, Create_CustomConfig) {
    tcb_config_t config = tcb_config_default();
    config.enable_early_stopping = false;
    config.patience = 20;
    config.checkpoint_interval = 500;

    ctx = tcb_create(&config);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(TrainingCallbacksTest, Create_WithSecurityContext) {
    CreateContextWithSecurity();
    // Context should be created with security registration
}

TEST_F(TrainingCallbacksTest, Destroy_NullPointer) {
    // Should not crash
    tcb_destroy(nullptr);
}

TEST_F(TrainingCallbacksTest, DefaultConfig_Values) {
    tcb_config_t config = tcb_config_default();

    EXPECT_FALSE(config.enable_auto_checkpoint);
    EXPECT_EQ(config.checkpoint_interval, TCB_DEFAULT_CHECKPOINT_INTERVAL);
    EXPECT_TRUE(config.enable_early_stopping);
    EXPECT_EQ(config.patience, 10u);
    EXPECT_NEAR(config.min_delta, 1e-6f, EPSILON);
    EXPECT_FALSE(config.enable_async_callbacks);
    EXPECT_TRUE(config.use_memory_pool);
    EXPECT_EQ(config.security_ctx, nullptr);
}

// =============================================================================
// Callback Registration Tests
// =============================================================================

TEST_F(TrainingCallbacksTest, Register_SimpleCallback) {
    CreateDefaultContext();

    uint32_t id = tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                                       simple_callback, nullptr, "test");
    EXPECT_NE(id, 0u);
}

TEST_F(TrainingCallbacksTest, Register_MultipleCallbacks) {
    CreateDefaultContext();

    uint32_t id1 = tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                                        simple_callback, nullptr, "cb1");
    uint32_t id2 = tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                                        simple_callback, nullptr, "cb2");
    uint32_t id3 = tcb_register_simple(ctx, TCB_EVENT_LOSS_COMPUTED,
                                        simple_callback, nullptr, "cb3");

    EXPECT_NE(id1, 0u);
    EXPECT_NE(id2, 0u);
    EXPECT_NE(id3, 0u);
    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
}

TEST_F(TrainingCallbacksTest, Register_FullCallback) {
    CreateDefaultContext();

    tcb_callback_info_t info = {
        .callback = simple_callback,
        .user_data = nullptr,
        .event_type = TCB_EVENT_EPOCH_COMPLETE,
        .mode = TCB_MODE_SYNC,
        .priority = TCB_PRIORITY_HIGH,
        .name = "full_callback",
        .enabled = true
    };

    uint32_t id = tcb_register(ctx, &info);
    EXPECT_NE(id, 0u);
}

TEST_F(TrainingCallbacksTest, Register_NullContext) {
    uint32_t id = tcb_register_simple(nullptr, TCB_EVENT_STEP_COMPLETE,
                                       simple_callback, nullptr, "test");
    EXPECT_EQ(id, 0u);
}

TEST_F(TrainingCallbacksTest, Register_NullCallback) {
    CreateDefaultContext();

    uint32_t id = tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                                       nullptr, nullptr, "test");
    EXPECT_EQ(id, 0u);
}

TEST_F(TrainingCallbacksTest, Register_InvalidEventType) {
    CreateDefaultContext();

    uint32_t id = tcb_register_simple(ctx, (tcb_event_type_t)999,
                                       simple_callback, nullptr, "test");
    EXPECT_EQ(id, 0u);
}

TEST_F(TrainingCallbacksTest, Unregister_Success) {
    CreateDefaultContext();

    uint32_t id = tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                                       simple_callback, nullptr, "test");
    ASSERT_NE(id, 0u);

    nimcp_result_t result = tcb_unregister(ctx, id);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(TrainingCallbacksTest, Unregister_InvalidId) {
    CreateDefaultContext();

    nimcp_result_t result = tcb_unregister(ctx, 999);
    EXPECT_EQ(result, NIMCP_NOT_FOUND);
}

TEST_F(TrainingCallbacksTest, Unregister_Zero) {
    CreateDefaultContext();

    nimcp_result_t result = tcb_unregister(ctx, 0);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(TrainingCallbacksTest, UnregisterAll_ByEventType) {
    CreateDefaultContext();

    tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE, simple_callback, nullptr, "cb1");
    tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE, simple_callback, nullptr, "cb2");
    tcb_register_simple(ctx, TCB_EVENT_LOSS_COMPUTED, simple_callback, nullptr, "cb3");

    uint32_t removed = tcb_unregister_all(ctx, TCB_EVENT_STEP_COMPLETE);
    EXPECT_EQ(removed, 2u);

    // Only loss computed callback should remain
    EXPECT_EQ(tcb_get_callback_count(ctx, TCB_EVENT_STEP_COMPLETE), 0u);
    EXPECT_EQ(tcb_get_callback_count(ctx, TCB_EVENT_LOSS_COMPUTED), 1u);
}

TEST_F(TrainingCallbacksTest, UnregisterAll_AllTypes) {
    CreateDefaultContext();

    tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE, simple_callback, nullptr, "cb1");
    tcb_register_simple(ctx, TCB_EVENT_LOSS_COMPUTED, simple_callback, nullptr, "cb2");
    tcb_register_simple(ctx, TCB_EVENT_EPOCH_COMPLETE, simple_callback, nullptr, "cb3");

    uint32_t removed = tcb_unregister_all(ctx, TCB_EVENT_COUNT);
    EXPECT_EQ(removed, 3u);
    EXPECT_EQ(tcb_get_callback_count(ctx, TCB_EVENT_COUNT), 0u);
}

TEST_F(TrainingCallbacksTest, SetEnabled) {
    CreateDefaultContext();

    uint32_t id = tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                                       simple_callback, nullptr, "test");
    ASSERT_NE(id, 0u);

    nimcp_result_t result = tcb_set_enabled(ctx, id, false);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    result = tcb_set_enabled(ctx, id, true);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

// =============================================================================
// Event Firing Tests
// =============================================================================

TEST_F(TrainingCallbacksTest, Fire_SingleCallback) {
    CreateDefaultContext();

    tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                        step_complete_callback, nullptr, "test");

    tcb_metrics_t metrics = {};
    metrics.step = 1;
    metrics.loss = 0.5f;

    tcb_action_t action = tcb_fire_event(ctx, TCB_EVENT_STEP_COMPLETE, &metrics);

    EXPECT_EQ(action, TCB_ACTION_CONTINUE);
    EXPECT_EQ(g_step_complete_count.load(), 1);
    EXPECT_NEAR(g_last_loss.load(), 0.5f, EPSILON);
}

TEST_F(TrainingCallbacksTest, Fire_MultipleCallbacks) {
    CreateDefaultContext();

    tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                        simple_callback, nullptr, "cb1");
    tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                        simple_callback, nullptr, "cb2");
    tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                        simple_callback, nullptr, "cb3");

    tcb_fire_event(ctx, TCB_EVENT_STEP_COMPLETE, nullptr);

    EXPECT_EQ(g_callback_count.load(), 3);
}

TEST_F(TrainingCallbacksTest, Fire_NoMatchingCallbacks) {
    CreateDefaultContext();

    tcb_register_simple(ctx, TCB_EVENT_LOSS_COMPUTED,
                        simple_callback, nullptr, "test");

    tcb_action_t action = tcb_fire_event(ctx, TCB_EVENT_STEP_COMPLETE, nullptr);

    EXPECT_EQ(action, TCB_ACTION_CONTINUE);
    EXPECT_EQ(g_callback_count.load(), 0);
}

TEST_F(TrainingCallbacksTest, Fire_DisabledCallback) {
    CreateDefaultContext();

    uint32_t id = tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                                       simple_callback, nullptr, "test");
    tcb_set_enabled(ctx, id, false);

    tcb_fire_event(ctx, TCB_EVENT_STEP_COMPLETE, nullptr);

    EXPECT_EQ(g_callback_count.load(), 0);
}

TEST_F(TrainingCallbacksTest, Fire_StopAction) {
    CreateDefaultContext();

    tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                        stop_callback, nullptr, "stopper");

    tcb_action_t action = tcb_fire_event(ctx, TCB_EVENT_STEP_COMPLETE, nullptr);

    EXPECT_EQ(action, TCB_ACTION_STOP_TRAINING);
}

TEST_F(TrainingCallbacksTest, Fire_SkipAction) {
    CreateDefaultContext();

    tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                        skip_callback, nullptr, "skipper");

    tcb_action_t action = tcb_fire_event(ctx, TCB_EVENT_STEP_COMPLETE, nullptr);

    EXPECT_EQ(action, TCB_ACTION_SKIP_STEP);
}

TEST_F(TrainingCallbacksTest, Fire_ReduceLRAction) {
    CreateDefaultContext();

    tcb_register_simple(ctx, TCB_EVENT_DIVERGENCE,
                        reduce_lr_callback, nullptr, "reducer");

    tcb_action_t action = tcb_fire_event(ctx, TCB_EVENT_DIVERGENCE, nullptr);

    EXPECT_EQ(action, TCB_ACTION_REDUCE_LR);
}

TEST_F(TrainingCallbacksTest, Fire_ActionPriority_StopOverOther) {
    CreateDefaultContext();

    // Register skip first, then stop - stop should win
    tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                        skip_callback, nullptr, "skipper");
    tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                        stop_callback, nullptr, "stopper");

    tcb_action_t action = tcb_fire_event(ctx, TCB_EVENT_STEP_COMPLETE, nullptr);

    // STOP has highest priority
    EXPECT_EQ(action, TCB_ACTION_STOP_TRAINING);
}

TEST_F(TrainingCallbacksTest, Fire_ActionPriority_RollbackOverLR) {
    CreateDefaultContext();

    tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                        reduce_lr_callback, nullptr, "reducer");
    tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                        rollback_callback, nullptr, "rollback");

    tcb_action_t action = tcb_fire_event(ctx, TCB_EVENT_STEP_COMPLETE, nullptr);

    // ROLLBACK > REDUCE_LR
    EXPECT_EQ(action, TCB_ACTION_ROLLBACK);
}

// =============================================================================
// Priority Tests
// =============================================================================

std::vector<int> g_priority_order;

static tcb_action_t priority_callback_critical(const tcb_event_t* event) {
    (void)event;
    g_priority_order.push_back(3);  // Critical = 3
    return TCB_ACTION_CONTINUE;
}

static tcb_action_t priority_callback_high(const tcb_event_t* event) {
    (void)event;
    g_priority_order.push_back(2);  // High = 2
    return TCB_ACTION_CONTINUE;
}

static tcb_action_t priority_callback_normal(const tcb_event_t* event) {
    (void)event;
    g_priority_order.push_back(1);  // Normal = 1
    return TCB_ACTION_CONTINUE;
}

static tcb_action_t priority_callback_low(const tcb_event_t* event) {
    (void)event;
    g_priority_order.push_back(0);  // Low = 0
    return TCB_ACTION_CONTINUE;
}

TEST_F(TrainingCallbacksTest, Priority_Ordering) {
    CreateDefaultContext();
    g_priority_order.clear();

    // Register in mixed order
    tcb_callback_info_t info_low = {
        .callback = priority_callback_low,
        .event_type = TCB_EVENT_STEP_COMPLETE,
        .priority = TCB_PRIORITY_LOW,
        .enabled = true
    };
    tcb_callback_info_t info_normal = {
        .callback = priority_callback_normal,
        .event_type = TCB_EVENT_STEP_COMPLETE,
        .priority = TCB_PRIORITY_NORMAL,
        .enabled = true
    };
    tcb_callback_info_t info_high = {
        .callback = priority_callback_high,
        .event_type = TCB_EVENT_STEP_COMPLETE,
        .priority = TCB_PRIORITY_HIGH,
        .enabled = true
    };
    tcb_callback_info_t info_critical = {
        .callback = priority_callback_critical,
        .event_type = TCB_EVENT_STEP_COMPLETE,
        .priority = TCB_PRIORITY_CRITICAL,
        .enabled = true
    };

    // Register in reverse priority order
    tcb_register(ctx, &info_low);
    tcb_register(ctx, &info_normal);
    tcb_register(ctx, &info_high);
    tcb_register(ctx, &info_critical);

    tcb_fire_event(ctx, TCB_EVENT_STEP_COMPLETE, nullptr);

    // Should execute in priority order: critical, high, normal, low
    ASSERT_EQ(g_priority_order.size(), 4u);
    EXPECT_EQ(g_priority_order[0], 3);  // Critical first
    EXPECT_EQ(g_priority_order[1], 2);  // High second
    EXPECT_EQ(g_priority_order[2], 1);  // Normal third
    EXPECT_EQ(g_priority_order[3], 0);  // Low last
}

// =============================================================================
// Metrics Tests
// =============================================================================

TEST_F(TrainingCallbacksTest, Metrics_Update) {
    CreateDefaultContext();

    tcb_update_metrics(ctx, 1.0f, 0.01f, 1, 0.5f);

    tcb_metrics_t metrics;
    nimcp_result_t result = tcb_get_metrics(ctx, &metrics);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_NEAR(metrics.loss, 1.0f, EPSILON);
    EXPECT_NEAR(metrics.learning_rate, 0.01f, EPSILON);
    EXPECT_EQ(metrics.step, 1u);
    EXPECT_NEAR(metrics.gradient_norm, 0.5f, EPSILON);
}

TEST_F(TrainingCallbacksTest, Metrics_EMA) {
    CreateDefaultContext();

    // First update establishes baseline
    tcb_update_metrics(ctx, 1.0f, 0.01f, 0, 0.5f);

    tcb_metrics_t metrics;
    tcb_get_metrics(ctx, &metrics);
    EXPECT_NEAR(metrics.loss_ema, 1.0f, EPSILON);

    // Second update should use EMA
    tcb_update_metrics(ctx, 0.5f, 0.01f, 1, 0.5f);
    tcb_get_metrics(ctx, &metrics);

    // EMA = 0.1 * 0.5 + 0.9 * 1.0 = 0.95
    EXPECT_NEAR(metrics.loss_ema, 0.95f, 0.01f);
}

TEST_F(TrainingCallbacksTest, Metrics_MinMax) {
    CreateDefaultContext();

    tcb_update_metrics(ctx, 1.0f, 0.01f, 0, 0.5f);
    tcb_update_metrics(ctx, 0.5f, 0.01f, 1, 0.5f);
    tcb_update_metrics(ctx, 2.0f, 0.01f, 2, 0.5f);
    tcb_update_metrics(ctx, 0.3f, 0.01f, 3, 0.5f);

    tcb_metrics_t metrics;
    tcb_get_metrics(ctx, &metrics);

    EXPECT_NEAR(metrics.min_loss, 0.3f, EPSILON);
    EXPECT_NEAR(metrics.max_loss, 2.0f, EPSILON);
}

TEST_F(TrainingCallbacksTest, Metrics_LossHistory) {
    CreateDefaultContext();

    // Update loss multiple times
    for (int i = 0; i < 15; i++) {
        tcb_update_metrics(ctx, 1.0f - i * 0.05f, 0.01f, i, 0.5f);
    }

    tcb_metrics_t metrics;
    tcb_get_metrics(ctx, &metrics);

    // With consistently decreasing loss, should detect convergence eventually
    // (though might not trigger in just 15 steps due to TCB_MIN_STEPS_FOR_CONVERGENCE)
    EXPECT_TRUE(metrics.step == 14);
}

// =============================================================================
// Early Stopping Tests
// =============================================================================

TEST_F(TrainingCallbacksTest, EarlyStopping_PatientExhausted) {
    tcb_config_t config = tcb_config_default();
    config.enable_early_stopping = true;
    config.patience = 3;
    config.min_delta = 0.01f;
    ctx = tcb_create(&config);
    ASSERT_NE(ctx, nullptr);

    // First loss establishes best
    tcb_update_metrics(ctx, 1.0f, 0.01f, 0, 0.5f);
    EXPECT_FALSE(tcb_should_stop(ctx, 1.0f));

    // Loss doesn't improve enough
    tcb_update_metrics(ctx, 1.0f, 0.01f, 1, 0.5f);
    EXPECT_FALSE(tcb_should_stop(ctx, 1.0f));

    tcb_update_metrics(ctx, 1.0f, 0.01f, 2, 0.5f);
    EXPECT_FALSE(tcb_should_stop(ctx, 1.0f));

    tcb_update_metrics(ctx, 1.0f, 0.01f, 3, 0.5f);
    EXPECT_TRUE(tcb_should_stop(ctx, 1.0f));  // Patience exhausted
}

TEST_F(TrainingCallbacksTest, EarlyStopping_Improvement) {
    tcb_config_t config = tcb_config_default();
    config.enable_early_stopping = true;
    config.patience = 3;
    config.min_delta = 0.01f;
    ctx = tcb_create(&config);

    tcb_update_metrics(ctx, 1.0f, 0.01f, 0, 0.5f);
    EXPECT_FALSE(tcb_should_stop(ctx, 1.0f));

    tcb_update_metrics(ctx, 0.9f, 0.01f, 1, 0.5f);  // Improved
    EXPECT_FALSE(tcb_should_stop(ctx, 0.9f));

    EXPECT_EQ(tcb_get_steps_without_improvement(ctx), 0u);
}

TEST_F(TrainingCallbacksTest, EarlyStopping_NaN) {
    tcb_config_t config = tcb_config_default();
    config.enable_early_stopping = true;
    ctx = tcb_create(&config);

    tcb_update_metrics(ctx, 1.0f, 0.01f, 0, 0.5f);
    EXPECT_TRUE(tcb_should_stop(ctx, NAN));
}

TEST_F(TrainingCallbacksTest, EarlyStopping_Infinity) {
    tcb_config_t config = tcb_config_default();
    config.enable_early_stopping = true;
    ctx = tcb_create(&config);

    tcb_update_metrics(ctx, 1.0f, 0.01f, 0, 0.5f);
    EXPECT_TRUE(tcb_should_stop(ctx, INFINITY));
}

TEST_F(TrainingCallbacksTest, EarlyStopping_Reset) {
    tcb_config_t config = tcb_config_default();
    config.enable_early_stopping = true;
    config.patience = 3;
    ctx = tcb_create(&config);

    // Exhaust patience
    for (int i = 0; i < 5; i++) {
        tcb_update_metrics(ctx, 1.0f, 0.01f, i, 0.5f);
    }
    EXPECT_TRUE(tcb_get_steps_without_improvement(ctx) > 0);

    // Reset
    tcb_reset_early_stopping(ctx);
    EXPECT_EQ(tcb_get_steps_without_improvement(ctx), 0u);
}

// =============================================================================
// Checkpoint Tests
// =============================================================================

TEST_F(TrainingCallbacksTest, Checkpoint_CreatePath) {
    CreateDefaultContext();

    tcb_update_metrics(ctx, 1.0f, 0.01f, 100, 0.5f);
    nimcp_result_t result = tcb_checkpoint(ctx, nullptr);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    const char* path = tcb_get_last_checkpoint(ctx);
    ASSERT_NE(path, nullptr);
    EXPECT_TRUE(strstr(path, "100") != nullptr);  // Should contain step number
}

TEST_F(TrainingCallbacksTest, Checkpoint_CustomPath) {
    CreateDefaultContext();

    nimcp_result_t result = tcb_checkpoint(ctx, "/tmp/custom_checkpoint.nimcp");

    EXPECT_EQ(result, NIMCP_SUCCESS);
    const char* path = tcb_get_last_checkpoint(ctx);
    EXPECT_STREQ(path, "/tmp/custom_checkpoint.nimcp");
}

std::atomic<bool> g_checkpoint_handler_called{false};

static tcb_action_t checkpoint_handler(const tcb_event_t* event) {
    (void)event;
    g_checkpoint_handler_called.store(true);
    return TCB_ACTION_CONTINUE;
}

TEST_F(TrainingCallbacksTest, Checkpoint_Handler) {
    CreateDefaultContext();
    g_checkpoint_handler_called.store(false);

    tcb_set_checkpoint_handler(ctx, checkpoint_handler, nullptr);
    tcb_checkpoint(ctx, nullptr);

    EXPECT_TRUE(g_checkpoint_handler_called.load());
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_F(TrainingCallbacksTest, Stats_CallbacksFired) {
    CreateDefaultContext();

    tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                        simple_callback, nullptr, "test");

    for (int i = 0; i < 10; i++) {
        tcb_fire_event(ctx, TCB_EVENT_STEP_COMPLETE, nullptr);
    }

    tcb_stats_t stats;
    nimcp_result_t result = tcb_get_stats(ctx, &stats);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_callbacks_fired, 10u);
    EXPECT_EQ(stats.callbacks_by_event[TCB_EVENT_STEP_COMPLETE], 10u);
}

TEST_F(TrainingCallbacksTest, Stats_Reset) {
    CreateDefaultContext();

    tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                        simple_callback, nullptr, "test");

    tcb_fire_event(ctx, TCB_EVENT_STEP_COMPLETE, nullptr);

    tcb_stats_t stats;
    tcb_get_stats(ctx, &stats);
    EXPECT_EQ(stats.total_callbacks_fired, 1u);

    tcb_reset_stats(ctx);

    tcb_get_stats(ctx, &stats);
    EXPECT_EQ(stats.total_callbacks_fired, 0u);
}

TEST_F(TrainingCallbacksTest, Stats_EarlyStopTracked) {
    CreateDefaultContext();

    tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                        stop_callback, nullptr, "stopper");

    tcb_fire_event(ctx, TCB_EVENT_STEP_COMPLETE, nullptr);

    tcb_stats_t stats;
    tcb_get_stats(ctx, &stats);
    EXPECT_EQ(stats.early_stops_triggered, 1u);
}

TEST_F(TrainingCallbacksTest, CallbackCount) {
    CreateDefaultContext();

    tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                        simple_callback, nullptr, "cb1");
    tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                        simple_callback, nullptr, "cb2");
    tcb_register_simple(ctx, TCB_EVENT_LOSS_COMPUTED,
                        simple_callback, nullptr, "cb3");

    EXPECT_EQ(tcb_get_callback_count(ctx, TCB_EVENT_STEP_COMPLETE), 2u);
    EXPECT_EQ(tcb_get_callback_count(ctx, TCB_EVENT_LOSS_COMPUTED), 1u);
    EXPECT_EQ(tcb_get_callback_count(ctx, TCB_EVENT_EPOCH_COMPLETE), 0u);
    EXPECT_EQ(tcb_get_callback_count(ctx, TCB_EVENT_COUNT), 3u);  // Total
}

// =============================================================================
// Built-in Callbacks Tests
// =============================================================================

TEST_F(TrainingCallbacksTest, Builtin_Logger) {
    tcb_event_t event = {};
    event.metrics.step = 100;
    event.metrics.loss = 0.5f;
    event.metrics.learning_rate = 0.001f;
    event.metrics.gradient_norm = 1.5f;

    tcb_action_t action = tcb_builtin_logger(&event);
    EXPECT_EQ(action, TCB_ACTION_CONTINUE);
}

TEST_F(TrainingCallbacksTest, Builtin_DivergenceDetector_NaN) {
    tcb_event_t event = {};
    event.metrics.loss = NAN;

    tcb_action_t action = tcb_builtin_divergence_detector(&event);
    EXPECT_EQ(action, TCB_ACTION_STOP_TRAINING);
}

TEST_F(TrainingCallbacksTest, Builtin_DivergenceDetector_Inf) {
    tcb_event_t event = {};
    event.metrics.loss = INFINITY;

    tcb_action_t action = tcb_builtin_divergence_detector(&event);
    EXPECT_EQ(action, TCB_ACTION_STOP_TRAINING);
}

TEST_F(TrainingCallbacksTest, Builtin_GradientMonitor_Exploding) {
    tcb_event_t event = {};
    event.metrics.gradient_norm = 150.0f;
    event.metrics.step = 1;

    tcb_action_t action = tcb_builtin_gradient_monitor(&event);
    EXPECT_EQ(action, TCB_ACTION_REDUCE_LR);
}

TEST_F(TrainingCallbacksTest, Builtin_GradientMonitor_Vanishing) {
    tcb_event_t event = {};
    event.metrics.gradient_norm = 1e-8f;
    event.metrics.step = 200;

    tcb_action_t action = tcb_builtin_gradient_monitor(&event);
    EXPECT_EQ(action, TCB_ACTION_INCREASE_LR);
}

// =============================================================================
// Utility Function Tests
// =============================================================================

TEST_F(TrainingCallbacksTest, EventTypeName) {
    EXPECT_STREQ(tcb_event_type_name(TCB_EVENT_STEP_COMPLETE), "STEP_COMPLETE");
    EXPECT_STREQ(tcb_event_type_name(TCB_EVENT_LOSS_COMPUTED), "LOSS_COMPUTED");
    EXPECT_STREQ(tcb_event_type_name(TCB_EVENT_CHECKPOINT), "CHECKPOINT");
}

TEST_F(TrainingCallbacksTest, ActionName) {
    EXPECT_STREQ(tcb_action_name(TCB_ACTION_CONTINUE), "CONTINUE");
    EXPECT_STREQ(tcb_action_name(TCB_ACTION_STOP_TRAINING), "STOP_TRAINING");
    EXPECT_STREQ(tcb_action_name(TCB_ACTION_REDUCE_LR), "REDUCE_LR");
}

TEST_F(TrainingCallbacksTest, ValidateConfig_Valid) {
    tcb_config_t config = tcb_config_default();
    nimcp_result_t result = tcb_validate_config(&config);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(TrainingCallbacksTest, ValidateConfig_InvalidPatience) {
    tcb_config_t config = tcb_config_default();
    config.enable_early_stopping = true;
    config.patience = 0;

    nimcp_result_t result = tcb_validate_config(&config);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(TrainingCallbacksTest, ValidateConfig_InvalidDivergence) {
    tcb_config_t config = tcb_config_default();
    config.divergence_threshold = 0.5f;  // Must be > 1.0

    nimcp_result_t result = tcb_validate_config(&config);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

TEST_F(TrainingCallbacksTest, ThreadSafety_Registration) {
    CreateDefaultContext();

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    // Multiple threads registering callbacks
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &success_count, t]() {
            for (int i = 0; i < 10; i++) {
                char name[32];
                snprintf(name, sizeof(name), "cb_%d_%d", t, i);
                uint32_t id = tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                                                   simple_callback, nullptr, name);
                if (id != 0) {
                    success_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // All registrations should succeed (or hit capacity limit)
    EXPECT_GT(success_count.load(), 0);
}

TEST_F(TrainingCallbacksTest, ThreadSafety_FiringWhileRegistering) {
    CreateDefaultContext();

    std::atomic<bool> running{true};
    std::atomic<int> fire_count{0};

    // Thread firing events
    std::thread fire_thread([this, &running, &fire_count]() {
        while (running.load()) {
            tcb_fire_event(ctx, TCB_EVENT_STEP_COMPLETE, nullptr);
            fire_count++;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    // Main thread registering/unregistering
    for (int i = 0; i < 50; i++) {
        uint32_t id = tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                                           simple_callback, nullptr, "test");
        if (id != 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
            tcb_unregister(ctx, id);
        }
    }

    running.store(false);
    fire_thread.join();

    EXPECT_GT(fire_count.load(), 0);
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_F(TrainingCallbacksTest, EdgeCase_NullEvent) {
    CreateDefaultContext();

    tcb_action_t action = tcb_fire(ctx, nullptr);
    EXPECT_EQ(action, TCB_ACTION_CONTINUE);
}

TEST_F(TrainingCallbacksTest, EdgeCase_NullContext) {
    tcb_metrics_t metrics;
    nimcp_result_t result = tcb_get_metrics(nullptr, &metrics);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(TrainingCallbacksTest, EdgeCase_NullMetricsOutput) {
    CreateDefaultContext();
    nimcp_result_t result = tcb_get_metrics(ctx, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(TrainingCallbacksTest, EdgeCase_LargeStepCount) {
    CreateDefaultContext();

    tcb_update_metrics(ctx, 1.0f, 0.01f, UINT64_MAX, 0.5f);

    tcb_metrics_t metrics;
    tcb_get_metrics(ctx, &metrics);
    EXPECT_EQ(metrics.step, UINT64_MAX);
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
