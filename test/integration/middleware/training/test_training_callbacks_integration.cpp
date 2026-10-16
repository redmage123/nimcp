/**
 * @file test_training_callbacks_integration.cpp
 * @brief Integration tests for Training Callbacks Module (Phase TCB-1)
 *
 * Tests integration with:
 * - Security module integration
 * - Memory pool integration (unified memory)
 * - Multiple callbacks across training flow
 * - Simulated training pipeline
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

// Global tracking for integration tests
struct IntegrationTracker {
    std::atomic<int> step_complete_count{0};
    std::atomic<int> loss_computed_count{0};
    std::atomic<int> weights_updated_count{0};
    std::atomic<int> epoch_complete_count{0};
    std::atomic<float> last_loss{0.0f};
    std::atomic<float> last_lr{0.0f};
    std::atomic<bool> early_stop_triggered{false};
    std::vector<float> loss_history;

    void reset() {
        step_complete_count.store(0);
        loss_computed_count.store(0);
        weights_updated_count.store(0);
        epoch_complete_count.store(0);
        last_loss.store(0.0f);
        last_lr.store(0.0f);
        early_stop_triggered.store(false);
        loss_history.clear();
    }
};

IntegrationTracker g_tracker;

/**
 * @brief Test fixture for callback integration tests
 */
class TrainingCallbacksIntegrationTest : public ::testing::Test {
protected:
    tcb_context_t* cb_ctx = nullptr;
    nimcp_sec_integration_t* security_ctx = nullptr;

    void SetUp() override {
        g_tracker.reset();

        // Initialize security context
        security_ctx = nimcp_sec_integration_create();
        if (security_ctx) {
            nimcp_sec_integration_config_t sec_cfg = nimcp_sec_integration_default_config();
            nimcp_sec_integration_init(security_ctx, &sec_cfg);
        }
    }

    void TearDown() override {
        if (cb_ctx) {
            tcb_destroy(cb_ctx);
            cb_ctx = nullptr;
        }
        if (security_ctx) {
            nimcp_sec_integration_destroy(security_ctx);
            security_ctx = nullptr;
        }
    }

    void CreateCallbackContext() {
        tcb_config_t config = tcb_config_default();
        config.security_ctx = security_ctx;
        cb_ctx = tcb_create(&config);
        ASSERT_NE(cb_ctx, nullptr);
    }
};

// =============================================================================
// Callback Functions for Integration Testing
// =============================================================================

static tcb_action_t integration_step_complete(const tcb_event_t* event) {
    g_tracker.step_complete_count++;
    if (event) {
        g_tracker.last_loss.store(event->metrics.loss);
        g_tracker.last_lr.store(event->metrics.learning_rate);
    }
    return TCB_ACTION_CONTINUE;
}

static tcb_action_t integration_loss_computed(const tcb_event_t* event) {
    g_tracker.loss_computed_count++;
    if (event) {
        g_tracker.loss_history.push_back(event->metrics.loss);
    }
    return TCB_ACTION_CONTINUE;
}

static tcb_action_t integration_weights_updated(const tcb_event_t* event) {
    (void)event;
    g_tracker.weights_updated_count++;
    return TCB_ACTION_CONTINUE;
}

static tcb_action_t integration_epoch_complete(const tcb_event_t* event) {
    (void)event;
    g_tracker.epoch_complete_count++;
    return TCB_ACTION_CONTINUE;
}

static tcb_action_t integration_early_stopper(const tcb_event_t* event) {
    if (!event) return TCB_ACTION_CONTINUE;

    // Stop if loss is very low (converged)
    if (event->metrics.loss < 0.001f) {
        g_tracker.early_stop_triggered.store(true);
        return TCB_ACTION_STOP_TRAINING;
    }
    return TCB_ACTION_CONTINUE;
}

static tcb_action_t integration_lr_reducer(const tcb_event_t* event) {
    if (!event) return TCB_ACTION_CONTINUE;

    // Reduce LR if gradient norm too high
    if (event->metrics.gradient_norm > 10.0f) {
        return TCB_ACTION_REDUCE_LR;
    }
    return TCB_ACTION_CONTINUE;
}

static tcb_action_t integration_divergence_detector(const tcb_event_t* event) {
    if (!event) return TCB_ACTION_CONTINUE;

    if (std::isnan(event->metrics.loss) || std::isinf(event->metrics.loss)) {
        return TCB_ACTION_STOP_TRAINING;
    }
    if (event->metrics.is_diverging) {
        return TCB_ACTION_REDUCE_LR;
    }
    return TCB_ACTION_CONTINUE;
}

// =============================================================================
// Callback Context Standalone Tests
// =============================================================================

TEST_F(TrainingCallbacksIntegrationTest, CallbackContext_WithSecurity) {
    CreateCallbackContext();

    // Register multiple callbacks
    uint32_t id1 = tcb_register_simple(cb_ctx, TCB_EVENT_STEP_COMPLETE,
                                        integration_step_complete, nullptr, "step");
    uint32_t id2 = tcb_register_simple(cb_ctx, TCB_EVENT_LOSS_COMPUTED,
                                        integration_loss_computed, nullptr, "loss");

    EXPECT_NE(id1, 0u);
    EXPECT_NE(id2, 0u);

    // Simulate training metrics updates
    for (int i = 0; i < 10; i++) {
        float loss = 1.0f / (i + 1);
        tcb_update_metrics(cb_ctx, loss, 0.01f, i, 0.5f);
        tcb_fire_event(cb_ctx, TCB_EVENT_LOSS_COMPUTED, nullptr);
        tcb_fire_event(cb_ctx, TCB_EVENT_STEP_COMPLETE, nullptr);
    }

    EXPECT_EQ(g_tracker.step_complete_count.load(), 10);
    EXPECT_EQ(g_tracker.loss_computed_count.load(), 10);
}

TEST_F(TrainingCallbacksIntegrationTest, CallbackContext_EarlyStopping) {
    tcb_config_t config = tcb_config_default();
    config.enable_early_stopping = true;
    config.patience = 5;
    config.min_delta = 0.01f;
    cb_ctx = tcb_create(&config);
    ASSERT_NE(cb_ctx, nullptr);

    tcb_register_simple(cb_ctx, TCB_EVENT_STEP_COMPLETE,
                        integration_early_stopper, nullptr, "stopper");

    // Simulate decreasing loss that reaches < 0.001 (threshold in integration_early_stopper)
    // Using exponential decay: 1.0 * exp(-0.5 * i) reaches 0.0005 at i ~ 15
    bool stopped = false;
    for (int i = 0; i < 20 && !stopped; i++) {
        float loss = 1.0f * expf(-0.5f * i);  // Exponential decay to trigger early stopping
        tcb_update_metrics(cb_ctx, loss, 0.01f, i, 0.5f);

        tcb_action_t action = tcb_fire_event(cb_ctx, TCB_EVENT_STEP_COMPLETE, nullptr);
        if (action == TCB_ACTION_STOP_TRAINING) {
            stopped = true;
        }
    }

    EXPECT_TRUE(stopped || g_tracker.early_stop_triggered.load());
}

// =============================================================================
// Simulated Training Loop Integration Tests
// =============================================================================

TEST_F(TrainingCallbacksIntegrationTest, SimulatedTraining_MultipleCallbacks) {
    CreateCallbackContext();

    // Register all callback types
    tcb_register_simple(cb_ctx, TCB_EVENT_STEP_COMPLETE,
                        integration_step_complete, nullptr, "step");
    tcb_register_simple(cb_ctx, TCB_EVENT_LOSS_COMPUTED,
                        integration_loss_computed, nullptr, "loss");
    tcb_register_simple(cb_ctx, TCB_EVENT_WEIGHTS_UPDATED,
                        integration_weights_updated, nullptr, "weights");
    tcb_register_simple(cb_ctx, TCB_EVENT_STEP_COMPLETE,
                        integration_lr_reducer, nullptr, "lr_reducer");
    tcb_register_simple(cb_ctx, TCB_EVENT_STEP_COMPLETE,
                        integration_divergence_detector, nullptr, "divergence");

    // Simulate training loop
    float learning_rate = 0.01f;
    bool training = true;

    for (int step = 0; step < 100 && training; step++) {
        // Simulate forward pass and loss computation
        float loss = 1.0f / (step + 1) + 0.001f * (step % 10);
        float gradient_norm = 0.5f + 0.1f * (step % 5);

        // Update metrics
        tcb_update_metrics(cb_ctx, loss, learning_rate, step, gradient_norm);

        // Fire loss computed event
        tcb_action_t action = tcb_fire_event(cb_ctx, TCB_EVENT_LOSS_COMPUTED, nullptr);
        if (action == TCB_ACTION_STOP_TRAINING) {
            training = false;
            continue;
        }

        // Fire weights updated event
        action = tcb_fire_event(cb_ctx, TCB_EVENT_WEIGHTS_UPDATED, nullptr);
        if (action == TCB_ACTION_STOP_TRAINING) {
            training = false;
            continue;
        }

        // Fire step complete event
        action = tcb_fire_event(cb_ctx, TCB_EVENT_STEP_COMPLETE, nullptr);
        if (action == TCB_ACTION_STOP_TRAINING) {
            training = false;
        } else if (action == TCB_ACTION_REDUCE_LR) {
            learning_rate *= 0.5f;
        } else if (action == TCB_ACTION_INCREASE_LR) {
            learning_rate *= 1.5f;
        }
    }

    // Verify callbacks were invoked
    EXPECT_GT(g_tracker.step_complete_count.load(), 50);
    EXPECT_GT(g_tracker.loss_computed_count.load(), 50);
    EXPECT_GT(g_tracker.weights_updated_count.load(), 50);

    // Verify loss history was recorded
    EXPECT_GT(g_tracker.loss_history.size(), 50u);

    // Get statistics
    tcb_stats_t stats;
    tcb_get_stats(cb_ctx, &stats);
    EXPECT_GT(stats.total_callbacks_fired, 150u);
}

TEST_F(TrainingCallbacksIntegrationTest, SimulatedTraining_EpochsAndCheckpoints) {
    CreateCallbackContext();

    tcb_register_simple(cb_ctx, TCB_EVENT_STEP_COMPLETE,
                        integration_step_complete, nullptr, "step");
    tcb_register_simple(cb_ctx, TCB_EVENT_EPOCH_COMPLETE,
                        integration_epoch_complete, nullptr, "epoch");

    int steps_per_epoch = 10;
    int num_epochs = 5;

    for (int epoch = 0; epoch < num_epochs; epoch++) {
        for (int step = 0; step < steps_per_epoch; step++) {
            int global_step = epoch * steps_per_epoch + step;
            float loss = 1.0f / (global_step + 1);

            tcb_update_metrics(cb_ctx, loss, 0.01f, global_step, 0.5f);
            tcb_fire_event(cb_ctx, TCB_EVENT_STEP_COMPLETE, nullptr);
        }

        // Fire epoch complete
        tcb_fire_event(cb_ctx, TCB_EVENT_EPOCH_COMPLETE, nullptr);

        // Create checkpoint at end of each epoch
        char path[64];
        snprintf(path, sizeof(path), "/tmp/checkpoint_epoch_%d.nimcp", epoch);
        tcb_checkpoint(cb_ctx, path);
    }

    EXPECT_EQ(g_tracker.step_complete_count.load(), steps_per_epoch * num_epochs);
    EXPECT_EQ(g_tracker.epoch_complete_count.load(), num_epochs);

    // Verify last checkpoint was recorded
    const char* last_checkpoint = tcb_get_last_checkpoint(cb_ctx);
    ASSERT_NE(last_checkpoint, nullptr);
    EXPECT_TRUE(strstr(last_checkpoint, "epoch_4") != nullptr);
}

// =============================================================================
// Memory Pool Integration Tests
// =============================================================================

TEST_F(TrainingCallbacksIntegrationTest, UnifiedMemory_CallbackContext) {
    // Create callback context with memory pool enabled
    tcb_config_t config = tcb_config_default();
    config.use_memory_pool = true;
    cb_ctx = tcb_create(&config);
    ASSERT_NE(cb_ctx, nullptr);

    // Register callbacks (max 16 per event type per TCB_MAX_CALLBACKS_PER_EVENT)
    const int num_callbacks = 16;
    for (int i = 0; i < num_callbacks; i++) {
        char name[32];
        snprintf(name, sizeof(name), "callback_%d", i);
        uint32_t id = tcb_register_simple(cb_ctx, TCB_EVENT_STEP_COMPLETE,
                                           integration_step_complete, nullptr, name);
        EXPECT_NE(id, 0u);
    }

    // Fire events
    for (int i = 0; i < 100; i++) {
        tcb_update_metrics(cb_ctx, 1.0f / (i + 1), 0.01f, i, 0.5f);
        tcb_fire_event(cb_ctx, TCB_EVENT_STEP_COMPLETE, nullptr);
    }

    // Each callback fires once per event
    EXPECT_EQ(g_tracker.step_complete_count.load(), 100 * num_callbacks);
}

// =============================================================================
// Security Integration Tests
// =============================================================================

TEST_F(TrainingCallbacksIntegrationTest, Security_ModuleRegistration) {
    // Create callback context with security
    tcb_config_t config = tcb_config_default();
    config.security_ctx = security_ctx;
    cb_ctx = tcb_create(&config);
    ASSERT_NE(cb_ctx, nullptr);

    // The callback context should have registered with security module
    // Verify security context has the module registered
    nimcp_sec_integration_stats_t stats;
    if (nimcp_sec_get_stats(security_ctx, &stats) == NIMCP_SUCCESS) {
        EXPECT_GE(stats.registered_modules, 1u);
    }

    // Register and fire callbacks normally
    tcb_register_simple(cb_ctx, TCB_EVENT_STEP_COMPLETE,
                        integration_step_complete, nullptr, "secure_cb");

    tcb_fire_event(cb_ctx, TCB_EVENT_STEP_COMPLETE, nullptr);
    EXPECT_EQ(g_tracker.step_complete_count.load(), 1);
}

TEST_F(TrainingCallbacksIntegrationTest, Security_TrustTracking) {
    // Create multiple contexts with security
    std::vector<tcb_context_t*> contexts;

    for (int i = 0; i < 5; i++) {
        tcb_config_t config = tcb_config_default();
        config.security_ctx = security_ctx;
        tcb_context_t* ctx = tcb_create(&config);
        ASSERT_NE(ctx, nullptr);
        contexts.push_back(ctx);

        // Register callback
        tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                            integration_step_complete, nullptr, "test");

        // Fire events
        for (int j = 0; j < 10; j++) {
            tcb_fire_event(ctx, TCB_EVENT_STEP_COMPLETE, nullptr);
        }
    }

    // Check security stats
    nimcp_sec_integration_stats_t stats;
    if (nimcp_sec_get_stats(security_ctx, &stats) == NIMCP_SUCCESS) {
        EXPECT_GE(stats.registered_modules, 5u);
    }

    // Cleanup
    for (tcb_context_t* ctx : contexts) {
        tcb_destroy(ctx);
    }

    EXPECT_EQ(g_tracker.step_complete_count.load(), 50);
}

// =============================================================================
// Stress Tests
// =============================================================================

TEST_F(TrainingCallbacksIntegrationTest, Stress_ManyCallbacks) {
    CreateCallbackContext();

    // Register max callbacks per event type
    for (int i = 0; i < TCB_MAX_CALLBACKS_PER_EVENT; i++) {
        char name[32];
        snprintf(name, sizeof(name), "cb_%d", i);
        tcb_register_simple(cb_ctx, TCB_EVENT_STEP_COMPLETE,
                            integration_step_complete, nullptr, name);
    }

    // Fire many events
    for (int i = 0; i < 1000; i++) {
        tcb_update_metrics(cb_ctx, 1.0f / (i + 1), 0.01f, i, 0.5f);
        tcb_fire_event(cb_ctx, TCB_EVENT_STEP_COMPLETE, nullptr);
    }

    EXPECT_EQ(g_tracker.step_complete_count.load(), 1000 * TCB_MAX_CALLBACKS_PER_EVENT);

    // Get statistics
    tcb_stats_t stats;
    nimcp_result_t result = tcb_get_stats(cb_ctx, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_callbacks_fired, (uint64_t)(1000 * TCB_MAX_CALLBACKS_PER_EVENT));
}

TEST_F(TrainingCallbacksIntegrationTest, Stress_RapidRegistrationUnregistration) {
    CreateCallbackContext();

    std::vector<uint32_t> ids;

    for (int round = 0; round < 100; round++) {
        // Register 10 callbacks
        for (int i = 0; i < 10; i++) {
            char name[32];
            snprintf(name, sizeof(name), "cb_%d_%d", round, i);
            uint32_t id = tcb_register_simple(cb_ctx, TCB_EVENT_STEP_COMPLETE,
                                               integration_step_complete, nullptr, name);
            if (id != 0) {
                ids.push_back(id);
            }
        }

        // Fire some events
        tcb_fire_event(cb_ctx, TCB_EVENT_STEP_COMPLETE, nullptr);

        // Unregister half
        for (size_t i = 0; i < ids.size() / 2 && !ids.empty(); i++) {
            tcb_unregister(cb_ctx, ids.back());
            ids.pop_back();
        }
    }

    // Clean up remaining
    for (uint32_t id : ids) {
        tcb_unregister(cb_ctx, id);
    }

    EXPECT_GT(g_tracker.step_complete_count.load(), 0);
}

// =============================================================================
// Concurrent Access Tests
// =============================================================================

TEST_F(TrainingCallbacksIntegrationTest, Concurrent_FiringAndMetricsUpdate) {
    CreateCallbackContext();

    tcb_register_simple(cb_ctx, TCB_EVENT_STEP_COMPLETE,
                        integration_step_complete, nullptr, "test");

    std::atomic<bool> running{true};
    std::atomic<int> total_fires{0};

    // Thread that updates metrics
    std::thread updater([this, &running]() {
        uint64_t step = 0;
        while (running.load()) {
            tcb_update_metrics(cb_ctx, 1.0f / (step + 1), 0.01f, step, 0.5f);
            step++;
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    });

    // Thread that fires events
    std::thread firer([this, &running, &total_fires]() {
        while (running.load()) {
            tcb_fire_event(cb_ctx, TCB_EVENT_STEP_COMPLETE, nullptr);
            total_fires++;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    // Let them run
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    running.store(false);

    updater.join();
    firer.join();

    EXPECT_EQ(g_tracker.step_complete_count.load(), total_fires.load());
}

// =============================================================================
// Action Handling Tests
// =============================================================================

TEST_F(TrainingCallbacksIntegrationTest, Action_LearningRateAdjustment) {
    CreateCallbackContext();

    // Callback that detects high gradient and requests LR reduction
    tcb_register_simple(cb_ctx, TCB_EVENT_STEP_COMPLETE,
                        integration_lr_reducer, nullptr, "lr_reducer");

    float learning_rate = 0.1f;
    int lr_reductions = 0;

    for (int step = 0; step < 50; step++) {
        // Alternate between normal and high gradient norm
        float gradient_norm = (step % 10 == 0) ? 15.0f : 0.5f;

        tcb_update_metrics(cb_ctx, 1.0f / (step + 1), learning_rate, step, gradient_norm);
        tcb_action_t action = tcb_fire_event(cb_ctx, TCB_EVENT_STEP_COMPLETE, nullptr);

        if (action == TCB_ACTION_REDUCE_LR) {
            learning_rate *= 0.5f;
            lr_reductions++;
        }
    }

    // Should have had some LR reductions at steps 0, 10, 20, 30, 40
    EXPECT_EQ(lr_reductions, 5);
}

TEST_F(TrainingCallbacksIntegrationTest, Action_DivergenceHandling) {
    CreateCallbackContext();

    tcb_register_simple(cb_ctx, TCB_EVENT_STEP_COMPLETE,
                        integration_divergence_detector, nullptr, "divergence");

    // Test NaN detection
    tcb_update_metrics(cb_ctx, NAN, 0.01f, 0, 0.5f);
    tcb_action_t action = tcb_fire_event(cb_ctx, TCB_EVENT_STEP_COMPLETE, nullptr);
    EXPECT_EQ(action, TCB_ACTION_STOP_TRAINING);

    // Test Inf detection
    tcb_update_metrics(cb_ctx, INFINITY, 0.01f, 1, 0.5f);
    action = tcb_fire_event(cb_ctx, TCB_EVENT_STEP_COMPLETE, nullptr);
    EXPECT_EQ(action, TCB_ACTION_STOP_TRAINING);
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
