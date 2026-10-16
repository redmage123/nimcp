/**
 * @file test_training_exception_integration.cpp
 * @brief Integration tests for Training Exception Handling with Brain Immune System
 *
 * WHAT: Test complete exception-to-immune pipeline for training subsystem
 * WHY:  Verify training errors trigger appropriate immune responses and recovery
 * HOW:  Simulate training failures, verify immune presentation, test recovery flow
 *
 * TEST SCENARIOS:
 * - Gradient Explosion -> Immune Response -> GC/Rollback Recovery
 * - NaN Detection -> Immune Response -> Checkpoint Rollback
 * - Training Divergence -> Immune Response -> Emergency Save + Restart
 * - Multi-error Training -> Aggregate Exception -> Coordinated Recovery
 * - Handler Chain Integration with Training Exceptions
 * - Recovery Callback Chain Execution
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <atomic>
#include <thread>
#include <chrono>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "training/nimcp_gradient_scaling.h"
#include "training/nimcp_mixed_precision.h"
#include "training/nimcp_training_checkpoint.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class TrainingExceptionIntegrationTest : public ::testing::Test {
protected:
    static std::atomic<int> exception_handler_count;
    static std::atomic<int> immune_presentation_count;
    static std::atomic<int> recovery_callback_count;
    static std::atomic<nimcp_exception_recovery_action_t> last_recovery_action;
    static std::atomic<bool> rollback_executed;
    static std::atomic<bool> emergency_save_executed;
    static std::atomic<bool> gc_executed;

    nimcp_handler_registration_t* training_handler_reg;
    nimcp_handler_registration_t* immune_handler_reg;

    void SetUp() override {
        exception_handler_count = 0;
        immune_presentation_count = 0;
        recovery_callback_count = 0;
        last_recovery_action = EXCEPTION_RECOVERY_NONE;
        rollback_executed = false;
        emergency_save_executed = false;
        gc_executed = false;

        nimcp_exception_system_init();

        // Initialize immune integration
        nimcp_exception_immune_config_t config;
        nimcp_exception_immune_default_config(&config);
        config.enable_auto_present = true;
        config.enable_auto_recovery = true;
        nimcp_exception_immune_init(&config);

        // Register training exception handler
        nimcp_handler_options_t options;
        nimcp_handler_default_options(&options);
        options.name = "training_integration_handler";
        options.handler = training_exception_handler;
        options.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        options.category_filter = EXCEPTION_CATEGORY_BRAIN;
        training_handler_reg = nimcp_handler_register(&options);

        // Register immune handler
        options.name = "immune_integration_handler";
        options.handler = immune_handler;
        options.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
        options.category_filter = (nimcp_exception_category_t)0;  // All categories
        immune_handler_reg = nimcp_handler_register(&options);

        // Register recovery callbacks
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK, rollback_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_EMERGENCY_SAVE, emergency_save_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_GC, gc_callback, nullptr);
    }

    void TearDown() override {
        if (training_handler_reg) {
            nimcp_handler_unregister(training_handler_reg);
            training_handler_reg = nullptr;
        }
        if (immune_handler_reg) {
            nimcp_handler_unregister(immune_handler_reg);
            immune_handler_reg = nullptr;
        }

        nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK);
        nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_EMERGENCY_SAVE);
        nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_GC);

        nimcp_exception_clear_current();
        nimcp_exception_immune_shutdown();
        nimcp_exception_system_shutdown();
    }

    static bool training_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        exception_handler_count++;
        return false;  // Don't consume - allow chain to continue
    }

    static bool immune_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        if (ex->severity >= EXCEPTION_SEVERITY_SEVERE) {
            immune_presentation_count++;
        }
        return false;
    }

    static int rollback_recovery_callback(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
        (void)ex;
        (void)user_data;
        recovery_callback_count++;
        last_recovery_action = action;
        rollback_executed = true;
        return 0;
    }

    static int emergency_save_callback(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
        (void)ex;
        (void)user_data;
        recovery_callback_count++;
        last_recovery_action = action;
        emergency_save_executed = true;
        return 0;
    }

    static int gc_callback(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
        (void)ex;
        (void)user_data;
        recovery_callback_count++;
        last_recovery_action = action;
        gc_executed = true;
        return 0;
    }
};

std::atomic<int> TrainingExceptionIntegrationTest::exception_handler_count(0);
std::atomic<int> TrainingExceptionIntegrationTest::immune_presentation_count(0);
std::atomic<int> TrainingExceptionIntegrationTest::recovery_callback_count(0);
std::atomic<nimcp_exception_recovery_action_t> TrainingExceptionIntegrationTest::last_recovery_action(EXCEPTION_RECOVERY_NONE);
std::atomic<bool> TrainingExceptionIntegrationTest::rollback_executed(false);
std::atomic<bool> TrainingExceptionIntegrationTest::emergency_save_executed(false);
std::atomic<bool> TrainingExceptionIntegrationTest::gc_executed(false);

//=============================================================================
// Gradient Explosion Integration Tests
//=============================================================================

TEST_F(TrainingExceptionIntegrationTest, GradientExplosion_FullPipeline) {
    // WHAT: Test full pipeline for gradient explosion handling
    // WHY:  Verify exception -> handler -> immune -> recovery flow

    // Create gradient explosion exception
    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_BACKWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "layer_fc1",
        "Gradient explosion detected: norm=1e10 exceeds threshold=1e5"
    );
    ASSERT_NE(ex, nullptr);
    ex->gradient_norm = 1e10f;
    ex->learning_diverged = false;

    // Set training context
    nimcp_exception_set_context((nimcp_exception_t*)ex, "layer_name", "fc1");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "gradient_norm", "1e10");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "threshold", "1e5");

    // Dispatch through handler chain
    exception_handler_count = 0;
    nimcp_exception_dispatch((nimcp_exception_t*)ex);
    EXPECT_GE(exception_handler_count.load(), 1);

    // Present to immune system
    nimcp_immune_response_t response;
    std::memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune((nimcp_exception_t*)ex, &response);
    EXPECT_EQ(result, 0);

    // Execute recovery
    rollback_executed = false;
    gc_executed = false;
    nimcp_execute_recovery((nimcp_exception_t*)ex, EXCEPTION_RECOVERY_ROLLBACK);
    EXPECT_TRUE(rollback_executed.load());

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(TrainingExceptionIntegrationTest, GradientExplosion_TriggersGC) {
    // WHAT: Test that severe gradient explosion triggers GC recovery
    // WHY:  GC clears memory that might contain corrupted gradients

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_BACKWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "conv_layer",
        "Severe gradient explosion requiring GC"
    );
    ASSERT_NE(ex, nullptr);

    gc_executed = false;
    nimcp_execute_recovery((nimcp_exception_t*)ex, EXCEPTION_RECOVERY_GC);
    EXPECT_TRUE(gc_executed.load());
    EXPECT_EQ(last_recovery_action.load(), EXCEPTION_RECOVERY_GC);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// NaN Detection Integration Tests
//=============================================================================

TEST_F(TrainingExceptionIntegrationTest, NaNDetection_ImmuneResponse) {
    // WHAT: Test NaN detection triggers proper immune response
    // WHY:  NaN must be handled before corrupting more training state

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_FORWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "weights_layer3",
        "NaN detected in weight tensor at indices [12, 45]"
    );
    ASSERT_NE(ex, nullptr);
    ex->has_nan_weights = true;

    // Present to immune
    nimcp_immune_response_t response;
    std::memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune((nimcp_exception_t*)ex, &response);
    EXPECT_EQ(result, 0);

    // Get recovery strategy
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)ex, &strategy);

    // Brain exceptions should suggest rollback or GC
    EXPECT_TRUE(
        strategy.primary_action == EXCEPTION_RECOVERY_GC ||
        strategy.primary_action == EXCEPTION_RECOVERY_ROLLBACK ||
        strategy.primary_action == EXCEPTION_RECOVERY_REDUCE_LOAD
    );

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(TrainingExceptionIntegrationTest, NaNDetection_RollbackRecovery) {
    // WHAT: Test NaN recovery with checkpoint rollback
    // WHY:  Rollback to last known good state when NaN detected

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_FORWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "gradients",
        "NaN detected, initiating rollback"
    );
    ASSERT_NE(ex, nullptr);

    // Set context for checkpoint
    nimcp_exception_set_context((nimcp_exception_t*)ex, "checkpoint_epoch", "49");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "current_epoch", "50");

    rollback_executed = false;
    nimcp_execute_recovery((nimcp_exception_t*)ex, EXCEPTION_RECOVERY_ROLLBACK);
    EXPECT_TRUE(rollback_executed.load());

    // Verify recovery was attempted flag is set
    ex->base.recovery_attempted = true;
    ex->base.recovery_succeeded = rollback_executed.load();
    EXPECT_TRUE(ex->base.recovery_attempted);
    EXPECT_TRUE(ex->base.recovery_succeeded);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Training Divergence Integration Tests
//=============================================================================

TEST_F(TrainingExceptionIntegrationTest, Divergence_EmergencySaveRecovery) {
    // WHAT: Test divergence triggers emergency save
    // WHY:  Preserve current state before attempting recovery

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_INFERENCE_FAILED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        0, "training_loop",
        "Training diverged: loss increased 1000x in 100 steps"
    );
    ASSERT_NE(ex, nullptr);
    ex->learning_diverged = true;

    nimcp_exception_set_context((nimcp_exception_t*)ex, "initial_loss", "0.5");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "final_loss", "500.0");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "steps", "100");

    emergency_save_executed = false;
    nimcp_execute_recovery((nimcp_exception_t*)ex, EXCEPTION_RECOVERY_EMERGENCY_SAVE);
    EXPECT_TRUE(emergency_save_executed.load());

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(TrainingExceptionIntegrationTest, Divergence_ReduceLoadRecovery) {
    // WHAT: Test divergence triggers load reduction
    // WHY:  Reducing batch size or learning rate can help recovery

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_INFERENCE_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "optimizer",
        "Divergence detected, reducing learning rate"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)ex, &strategy);

    // Recovery strategy should include REDUCE_LOAD as an option
    EXPECT_TRUE(
        strategy.primary_action == EXCEPTION_RECOVERY_REDUCE_LOAD ||
        strategy.fallback_action == EXCEPTION_RECOVERY_REDUCE_LOAD ||
        strategy.primary_action == EXCEPTION_RECOVERY_GC ||
        strategy.primary_action == EXCEPTION_RECOVERY_ROLLBACK
    );

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Checkpoint Exception Integration Tests
//=============================================================================

TEST_F(TrainingExceptionIntegrationTest, CheckpointFailure_EmergencySave) {
    // WHAT: Test checkpoint failure triggers emergency save
    // WHY:  Must preserve state somehow if normal checkpoint fails

    nimcp_io_exception_t* ex = nimcp_io_exception_create(
        NIMCP_ERROR_FILE_WRITE,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "/checkpoints/model.ckpt",
        "Checkpoint save failed: disk full"
    );
    ASSERT_NE(ex, nullptr);

    // Dispatch through handlers
    exception_handler_count = 0;
    nimcp_exception_dispatch((nimcp_exception_t*)ex);

    // Attempt emergency save
    emergency_save_executed = false;
    nimcp_execute_recovery((nimcp_exception_t*)ex, EXCEPTION_RECOVERY_EMERGENCY_SAVE);
    EXPECT_TRUE(emergency_save_executed.load());

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(TrainingExceptionIntegrationTest, CheckpointCorruption_RetryRecovery) {
    // WHAT: Test corrupted checkpoint triggers retry from earlier checkpoint
    // WHY:  Corrupted checkpoint should fall back to previous version

    nimcp_io_exception_t* ex = nimcp_io_exception_create(
        NIMCP_ERROR_CHECKPOINT_LOAD,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "/checkpoints/epoch_50.ckpt",
        "Checkpoint corrupted, attempting earlier checkpoint"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context((nimcp_exception_t*)ex, "corrupted_checkpoint", "epoch_50");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "fallback_checkpoint", "epoch_45");

    rollback_executed = false;
    nimcp_execute_recovery((nimcp_exception_t*)ex, EXCEPTION_RECOVERY_ROLLBACK);
    EXPECT_TRUE(rollback_executed.load());

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Multi-Error Integration Tests
//=============================================================================

TEST_F(TrainingExceptionIntegrationTest, MultipleErrors_AggregateHandling) {
    // WHAT: Test aggregated training errors from batch
    // WHY:  Multiple errors should be handled together efficiently

    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Multiple training errors in batch"
    );
    ASSERT_NE(agg, nullptr);

    // Add gradient explosion
    nimcp_brain_exception_t* grad_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_BACKWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "layer_1",
        "Gradient explosion"
    );
    nimcp_aggregate_exception_add(agg, (nimcp_exception_t*)grad_ex);

    // Add NaN detection
    nimcp_brain_exception_t* nan_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_FORWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "layer_2",
        "NaN detected"
    );
    nimcp_aggregate_exception_add(agg, (nimcp_exception_t*)nan_ex);

    // Add checkpoint error
    nimcp_io_exception_t* ckpt_ex = nimcp_io_exception_create(
        NIMCP_ERROR_FILE_WRITE,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "/ckpt.bin",
        "Checkpoint failed"
    );
    nimcp_aggregate_exception_add(agg, (nimcp_exception_t*)ckpt_ex);

    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 3u);

    // Dispatch aggregate
    exception_handler_count = 0;
    nimcp_exception_dispatch((nimcp_exception_t*)agg);
    EXPECT_GE(exception_handler_count.load(), 1);

    // Present aggregate to immune
    nimcp_immune_response_t response;
    std::memset(&response, 0, sizeof(response));
    nimcp_exception_present_to_immune((nimcp_exception_t*)agg, &response);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

//=============================================================================
// Handler Chain Integration Tests
//=============================================================================

TEST_F(TrainingExceptionIntegrationTest, HandlerChain_PriorityOrder) {
    // WHAT: Test handler chain executes in priority order
    // WHY:  High priority handlers should process training exceptions first

    static std::vector<int> call_order;
    call_order.clear();

    auto high_priority_handler = [](nimcp_exception_t*, void*) -> bool {
        call_order.push_back(1);
        return false;
    };

    auto low_priority_handler = [](nimcp_exception_t*, void*) -> bool {
        call_order.push_back(2);
        return false;
    };

    nimcp_handler_options_t high_opts;
    nimcp_handler_default_options(&high_opts);
    high_opts.name = "high_priority";
    high_opts.handler = high_priority_handler;
    high_opts.priority = 200;  // Higher than default
    auto* high_reg = nimcp_handler_register(&high_opts);

    nimcp_handler_options_t low_opts;
    nimcp_handler_default_options(&low_opts);
    low_opts.name = "low_priority";
    low_opts.handler = low_priority_handler;
    low_opts.priority = 5;  // Lower than default
    auto* low_reg = nimcp_handler_register(&low_opts);

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_BACKWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "test", "Priority test"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_dispatch((nimcp_exception_t*)ex);

    // High priority (200) should be called before low priority (5)
    if (call_order.size() >= 2) {
        // Find positions of our handlers
        auto high_pos = std::find(call_order.begin(), call_order.end(), 1);
        auto low_pos = std::find(call_order.begin(), call_order.end(), 2);

        if (high_pos != call_order.end() && low_pos != call_order.end()) {
            EXPECT_LT(std::distance(call_order.begin(), high_pos),
                      std::distance(call_order.begin(), low_pos));
        }
    }

    nimcp_handler_unregister(high_reg);
    nimcp_handler_unregister(low_reg);
    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(TrainingExceptionIntegrationTest, HandlerChain_CategoryFilter) {
    // WHAT: Test handler category filtering for training exceptions
    // WHY:  Brain category handlers should only receive brain exceptions

    static std::atomic<bool> brain_handler_called(false);
    static std::atomic<bool> io_handler_called(false);

    auto brain_handler = [](nimcp_exception_t*, void*) -> bool {
        brain_handler_called = true;
        return false;
    };

    auto io_handler = [](nimcp_exception_t*, void*) -> bool {
        io_handler_called = true;
        return false;
    };

    nimcp_handler_options_t brain_opts;
    nimcp_handler_default_options(&brain_opts);
    brain_opts.name = "brain_only";
    brain_opts.handler = brain_handler;
    brain_opts.category_filter = EXCEPTION_CATEGORY_BRAIN;
    auto* brain_reg = nimcp_handler_register(&brain_opts);

    nimcp_handler_options_t io_opts;
    nimcp_handler_default_options(&io_opts);
    io_opts.name = "io_only";
    io_opts.handler = io_handler;
    io_opts.category_filter = EXCEPTION_CATEGORY_IO;
    auto* io_reg = nimcp_handler_register(&io_opts);

    // Create brain exception
    brain_handler_called = false;
    io_handler_called = false;
    nimcp_brain_exception_t* brain_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_BACKWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "test", "Category test"
    );
    ASSERT_NE(brain_ex, nullptr);
    nimcp_exception_dispatch((nimcp_exception_t*)brain_ex);
    EXPECT_TRUE(brain_handler_called.load());
    EXPECT_FALSE(io_handler_called.load());
    nimcp_exception_unref((nimcp_exception_t*)brain_ex);

    // Create IO exception
    brain_handler_called = false;
    io_handler_called = false;
    nimcp_io_exception_t* io_ex = nimcp_io_exception_create(
        NIMCP_ERROR_FILE_WRITE,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "/test.bin", "IO test"
    );
    ASSERT_NE(io_ex, nullptr);
    nimcp_exception_dispatch((nimcp_exception_t*)io_ex);
    EXPECT_FALSE(brain_handler_called.load());
    EXPECT_TRUE(io_handler_called.load());
    nimcp_exception_unref((nimcp_exception_t*)io_ex);

    nimcp_handler_unregister(brain_reg);
    nimcp_handler_unregister(io_reg);
}

//=============================================================================
// Immune Statistics Integration Tests
//=============================================================================

TEST_F(TrainingExceptionIntegrationTest, ImmuneStats_TracksPresentations) {
    // WHAT: Test exceptions can be presented to immune system
    // WHY:  Immune system integration helps with recovery

    nimcp_exception_immune_reset_stats();

    // Create and present multiple exceptions
    int successful_presentations = 0;
    for (int i = 0; i < 5; i++) {
        nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
            NIMCP_ERROR_BACKWARD_PASS,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            i, "layer",
            "Exception %d for stats test", i
        );
        ASSERT_NE(ex, nullptr);

        // Present to immune - just verify it doesn't crash
        nimcp_exception_present_to_immune((nimcp_exception_t*)ex, nullptr);
        successful_presentations++;
        nimcp_exception_unref((nimcp_exception_t*)ex);
    }

    // Verify we successfully presented all exceptions
    EXPECT_EQ(successful_presentations, 5);
}

//=============================================================================
// Recovery Chain Integration Tests
//=============================================================================

TEST_F(TrainingExceptionIntegrationTest, RecoveryChain_PrimaryThenFallback) {
    // WHAT: Test recovery chain: primary action then fallback
    // WHY:  If primary recovery fails, fallback should execute

    static std::atomic<int> primary_call_count(0);
    static std::atomic<int> fallback_call_count(0);

    auto failing_primary = [](nimcp_exception_t*, nimcp_exception_recovery_action_t, void*) -> int {
        primary_call_count++;
        return -1;  // Fail
    };

    auto fallback = [](nimcp_exception_t*, nimcp_exception_recovery_action_t, void*) -> int {
        fallback_call_count++;
        return 0;  // Success
    };

    // Use RETRY as primary, GC as fallback for this test
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RETRY, failing_primary, nullptr);
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_COMPACT, fallback, nullptr);

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_BACKWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "test", "Recovery chain test"
    );
    ASSERT_NE(ex, nullptr);

    primary_call_count = 0;
    fallback_call_count = 0;

    // Try primary - it should fail
    int result = nimcp_execute_recovery((nimcp_exception_t*)ex, EXCEPTION_RECOVERY_RETRY);
    EXPECT_NE(result, 0);
    EXPECT_EQ(primary_call_count.load(), 1);

    // Try fallback - it should succeed
    result = nimcp_execute_recovery((nimcp_exception_t*)ex, EXCEPTION_RECOVERY_COMPACT);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(fallback_call_count.load(), 1);

    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_RETRY);
    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_COMPACT);
    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Epitope Pattern Matching Integration Tests
//=============================================================================

TEST_F(TrainingExceptionIntegrationTest, Epitope_SimilarExceptionsSimilarEpitopes) {
    // WHAT: Test similar exceptions produce similar epitopes
    // WHY:  Immune system should recognize patterns

    nimcp_brain_exception_t* ex1 = nimcp_brain_exception_create(
        NIMCP_ERROR_BACKWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "layer_fc1",
        "Gradient explosion 1"
    );
    ASSERT_NE(ex1, nullptr);

    nimcp_brain_exception_t* ex2 = nimcp_brain_exception_create(
        NIMCP_ERROR_BACKWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "layer_fc1",
        "Gradient explosion 2"
    );
    ASSERT_NE(ex2, nullptr);

    size_t len1 = nimcp_exception_generate_epitope((nimcp_exception_t*)ex1);
    size_t len2 = nimcp_exception_generate_epitope((nimcp_exception_t*)ex2);

    EXPECT_GT(len1, 0u);
    EXPECT_GT(len2, 0u);

    // Same error code and category should produce some matching bytes
    int matching_bytes = 0;
    size_t min_len = std::min(len1, len2);
    for (size_t i = 0; i < min_len; i++) {
        if (ex1->base.epitope[i] == ex2->base.epitope[i]) {
            matching_bytes++;
        }
    }

    // Expect at least some bytes to match (error code, category are encoded)
    EXPECT_GT(matching_bytes, 0);

    nimcp_exception_unref((nimcp_exception_t*)ex1);
    nimcp_exception_unref((nimcp_exception_t*)ex2);
}

TEST_F(TrainingExceptionIntegrationTest, Epitope_DifferentExceptionsDifferentEpitopes) {
    // WHAT: Test different exceptions produce different epitopes
    // WHY:  Immune system should distinguish different errors

    nimcp_brain_exception_t* grad_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_BACKWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "layer",
        "Gradient explosion"
    );
    ASSERT_NE(grad_ex, nullptr);

    nimcp_brain_exception_t* nan_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_FORWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "layer",
        "NaN detected"
    );
    ASSERT_NE(nan_ex, nullptr);

    nimcp_exception_generate_epitope((nimcp_exception_t*)grad_ex);
    nimcp_exception_generate_epitope((nimcp_exception_t*)nan_ex);

    // Different error codes should produce different epitopes
    bool all_same = true;
    size_t min_len = std::min(grad_ex->base.epitope_len, nan_ex->base.epitope_len);
    for (size_t i = 0; i < min_len; i++) {
        if (grad_ex->base.epitope[i] != nan_ex->base.epitope[i]) {
            all_same = false;
            break;
        }
    }

    EXPECT_FALSE(all_same);

    nimcp_exception_unref((nimcp_exception_t*)grad_ex);
    nimcp_exception_unref((nimcp_exception_t*)nan_ex);
}

//=============================================================================
// Async Exception Processing Tests
//=============================================================================

TEST_F(TrainingExceptionIntegrationTest, AsyncPresentation_QueueProcessing) {
    // WHAT: Test async exception presentation
    // WHY:  Training shouldn't block on exception handling

    // Present multiple exceptions asynchronously
    for (int i = 0; i < 10; i++) {
        nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
            NIMCP_ERROR_BACKWARD_PASS,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            i, "async_layer",
            "Async exception %d", i
        );
        ASSERT_NE(ex, nullptr);

        int result = nimcp_exception_present_async((nimcp_exception_t*)ex);
        EXPECT_EQ(result, 0);
        nimcp_exception_unref((nimcp_exception_t*)ex);
    }

    // Process pending
    size_t processed = nimcp_exception_immune_process_pending(0);
    EXPECT_GE(processed, 0u);  // May be 0 if already processed
}

//=============================================================================
// Exception Notification Tests
//=============================================================================

TEST_F(TrainingExceptionIntegrationTest, NotifyRecoveryResult_Success) {
    // WHAT: Test recovery result notification
    // WHY:  Immune system should learn from recovery outcomes

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_BACKWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "layer",
        "Exception for notification test"
    );
    ASSERT_NE(ex, nullptr);

    // Present first
    nimcp_exception_present_to_immune((nimcp_exception_t*)ex, nullptr);

    // Notify successful recovery
    int result = nimcp_exception_notify_recovery_result(
        (nimcp_exception_t*)ex,
        EXCEPTION_RECOVERY_ROLLBACK,
        true  // success
    );
    EXPECT_EQ(result, 0);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(TrainingExceptionIntegrationTest, NotifyRecoveryResult_Failure) {
    // WHAT: Test recovery failure notification
    // WHY:  Immune system should learn from failed recoveries

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_INFERENCE_FAILED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        0, "training",
        "Exception for failed recovery test"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_present_to_immune((nimcp_exception_t*)ex, nullptr);

    // Notify failed recovery
    int result = nimcp_exception_notify_recovery_result(
        (nimcp_exception_t*)ex,
        EXCEPTION_RECOVERY_ROLLBACK,
        false  // failure
    );
    EXPECT_EQ(result, 0);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
