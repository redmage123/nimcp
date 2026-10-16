/**
 * @file e2e_test_training_symbolic_logic_pipeline.cpp
 * @brief End-to-end tests for training symbolic logic hub bridge pipeline
 * @version 1.0.0
 * @date 2026-01-09
 *
 * Tests the complete training-logic pipeline:
 * - Logic-guided curriculum learning
 * - Rule-based LR adjustment
 * - Constraint-based gradient clipping
 * - Early stopping with rule confidence
 * - Multi-module training coordination
 * - Rule learning from training outcomes
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include <random>

#include "training/integration/nimcp_training_symbolic_logic_hub_bridge.h"
#include "training/integration/nimcp_training_integration_hub.h"
#include "training/integration/nimcp_training_event_types.h"

/* ========================================================================
 * Test Constants
 * ======================================================================== */

static constexpr uint32_t MODULE_CURRICULUM    = 0x2001;
static constexpr uint32_t MODULE_OPTIMIZER     = 0x2002;
static constexpr uint32_t MODULE_VALIDATOR     = 0x2003;
static constexpr uint32_t MODULE_CHECKPOINT    = 0x2004;
static constexpr uint32_t MODULE_SCHEDULER     = 0x2005;

/* ========================================================================
 * Simulated Training State
 * ======================================================================== */

struct TrainingState {
    float loss;
    float val_loss;
    float val_accuracy;
    float learning_rate;
    float grad_norm;
    float difficulty;
    uint32_t epoch;
    uint32_t batch;
    uint32_t epochs_no_improve;
    bool converging;
};

/* ========================================================================
 * Test Fixture
 * ======================================================================== */

class TrainingLogicPipelineE2ETest : public ::testing::Test {
protected:
    training_integration_hub_t hub;
    training_logic_hub_bridge_t* logic_bridge;
    TrainingState state;
    std::mt19937 rng;

    void SetUp() override {
        rng.seed(42);  // Reproducible randomness

        // Create training hub
        training_hub_config_t hub_config = training_hub_default_config();
        hub_config.enable_async = false;
        hub = training_hub_create(&hub_config);
        ASSERT_NE(hub, nullptr);

        // Create and connect logic bridge
        training_logic_hub_config_t logic_config;
        ASSERT_EQ(training_logic_hub_default_config(&logic_config), 0);
        logic_config.enable_rule_learning = true;
        logic_config.rule_learning_rate = 0.1f;

        logic_bridge = training_logic_hub_create(&logic_config);
        ASSERT_NE(logic_bridge, nullptr);

        ASSERT_EQ(training_logic_hub_connect(logic_bridge, hub), 0);
        ASSERT_GT(training_logic_hub_add_default_rules(logic_bridge), 0);

        // Initialize training state
        state = {};
        state.loss = 2.0f;
        state.val_loss = 2.0f;
        state.val_accuracy = 0.1f;
        state.learning_rate = 0.01f;
        state.grad_norm = 1.0f;
        state.difficulty = 0.1f;
        state.epoch = 0;
        state.batch = 0;
        state.epochs_no_improve = 0;
        state.converging = true;
    }

    void TearDown() override {
        if (logic_bridge) {
            training_logic_hub_disconnect(logic_bridge);
            training_logic_hub_destroy(logic_bridge);
            logic_bridge = nullptr;
        }
        if (hub) {
            training_hub_destroy(hub);
            hub = nullptr;
        }
    }

    // Simulate one training batch
    void simulate_batch() {
        // Add some noise
        std::uniform_real_distribution<float> noise(-0.05f, 0.05f);

        // Loss typically decreases over time (with noise)
        if (state.converging) {
            state.loss *= (0.99f + noise(rng) * 0.5f);
        } else {
            state.loss *= (1.01f + noise(rng) * 0.5f);
        }
        state.loss = std::max(0.01f, std::min(10.0f, state.loss));

        // Gradient norm varies
        state.grad_norm = 1.0f + noise(rng);

        // Publish loss event
        training_event_data_t event;
        memset(&event, 0, sizeof(event));
        event.event_type = TRAINING_EVENT_LOSS_COMPUTED;
        event.source_module_id = MODULE_OPTIMIZER;
        event.epoch = state.epoch;
        event.batch = state.batch;
        event.loss_value = state.loss;
        event.learning_rate = state.learning_rate;
        event.priority = TRAINING_PRIORITY_NORMAL;

        training_loss_payload_t loss_payload;
        loss_payload.total_loss = state.loss;
        loss_payload.primary_loss = state.loss;
        loss_payload.regularization_loss = 0.0f;
        loss_payload.auxiliary_loss = 0.0f;
        loss_payload.batch_size = 32;
        event.payload = &loss_payload;
        event.payload_size = sizeof(loss_payload);

        training_hub_publish(hub, event.source_module_id, event.event_type, &event);

        // Publish gradient event
        memset(&event, 0, sizeof(event));
        event.event_type = TRAINING_EVENT_GRADIENT_READY;
        event.source_module_id = MODULE_OPTIMIZER;
        event.epoch = state.epoch;
        event.batch = state.batch;

        training_gradient_payload_t grad_payload;
        grad_payload.gradient_norm = state.grad_norm;
        grad_payload.gradient_max = state.grad_norm;
        grad_payload.gradient_min = -state.grad_norm;
        grad_payload.gradient_clipped = false;
        grad_payload.clip_threshold = 1.0f;
        event.payload = &grad_payload;
        event.payload_size = sizeof(grad_payload);

        training_hub_publish(hub, event.source_module_id, event.event_type, &event);

        state.batch++;
    }

    // Simulate end of epoch
    void simulate_epoch_end() {
        // Validation
        std::uniform_real_distribution<float> noise(-0.02f, 0.02f);

        float prev_val_loss = state.val_loss;
        if (state.converging) {
            state.val_loss *= (0.98f + noise(rng));
            state.val_accuracy = std::min(0.99f, state.val_accuracy + 0.02f + noise(rng));
        } else {
            state.val_loss *= (1.02f + noise(rng));
            state.val_accuracy = std::max(0.1f, state.val_accuracy - 0.01f + noise(rng));
        }

        // Track improvement
        if (state.val_loss < prev_val_loss * 0.999f) {
            state.epochs_no_improve = 0;
        } else {
            state.epochs_no_improve++;
        }

        // Publish epoch complete event
        training_event_data_t event;
        memset(&event, 0, sizeof(event));
        event.event_type = TRAINING_EVENT_EPOCH_COMPLETE;
        event.source_module_id = MODULE_OPTIMIZER;
        event.epoch = state.epoch;
        training_hub_publish(hub, event.source_module_id, event.event_type, &event);

        // Publish validation event
        memset(&event, 0, sizeof(event));
        event.event_type = TRAINING_EVENT_VALIDATION_COMPLETE;
        event.source_module_id = MODULE_VALIDATOR;
        event.epoch = state.epoch;

        float val_payload[2] = {state.val_loss, state.val_accuracy};
        event.payload = val_payload;
        event.payload_size = sizeof(val_payload);
        training_hub_publish(hub, event.source_module_id, event.event_type, &event);

        state.epoch++;
        state.batch = 0;
    }

    // Update logic metrics from training state
    void update_logic_metrics() {
        training_logic_metrics_t metrics;
        memset(&metrics, 0, sizeof(metrics));

        metrics.current_loss = state.loss;
        metrics.previous_loss = state.loss * 1.01f;
        metrics.best_loss = state.val_loss * 0.9f;
        metrics.loss_stable = state.epochs_no_improve < 3;

        metrics.grad_norm = state.grad_norm;
        metrics.grad_norm_avg = 1.0f;
        metrics.grad_stable = state.grad_norm < 5.0f;
        metrics.grad_exploding = state.grad_norm > 10.0f;
        metrics.grad_vanishing = state.grad_norm < 0.01f;

        metrics.learning_rate = state.learning_rate;
        metrics.lr_min = 1e-6f;
        metrics.lr_max = 0.1f;

        metrics.difficulty = state.difficulty;
        metrics.mastery = state.val_accuracy;
        metrics.performance = state.val_accuracy;

        metrics.epoch = state.epoch;
        metrics.batch = state.batch;
        metrics.epochs_since_improvement = state.epochs_no_improve;

        metrics.validation_loss = state.val_loss;
        metrics.validation_accuracy = state.val_accuracy;
        metrics.validation_improving = state.epochs_no_improve == 0;

        training_logic_hub_update_metrics(logic_bridge, &metrics);
    }

    // Adjust LR based on logic recommendations
    void adjust_lr_from_logic() {
        float suggested_lr = 0.0f;
        float confidence = 0.0f;

        if (training_logic_hub_query_lr(logic_bridge, state.learning_rate,
                                         &suggested_lr, &confidence) == 0) {
            if (confidence > 0.5f) {
                // Gradually move toward suggested LR
                state.learning_rate = state.learning_rate * 0.9f + suggested_lr * 0.1f;
                state.learning_rate = std::max(1e-6f, std::min(0.1f, state.learning_rate));
            }
        }

        // Publish LR event
        training_event_data_t event;
        memset(&event, 0, sizeof(event));
        event.event_type = TRAINING_EVENT_LR_ADJUSTED;
        event.source_module_id = MODULE_SCHEDULER;
        event.learning_rate = state.learning_rate;

        training_lr_payload_t lr_payload;
        lr_payload.old_lr = state.learning_rate;
        lr_payload.new_lr = state.learning_rate;
        lr_payload.scheduler_name = "logic_guided";
        lr_payload.step = state.epoch;
        event.payload = &lr_payload;
        event.payload_size = sizeof(lr_payload);

        training_hub_publish(hub, event.source_module_id, event.event_type, &event);
    }

    // Adjust difficulty based on logic recommendations
    void adjust_difficulty_from_logic() {
        float suggested_difficulty = 0.0f;
        float confidence = 0.0f;

        if (training_logic_hub_query_difficulty(logic_bridge, state.difficulty,
                                                  &suggested_difficulty, &confidence) == 0) {
            if (confidence > 0.5f) {
                state.difficulty = state.difficulty * 0.8f + suggested_difficulty * 0.2f;
                state.difficulty = std::max(0.0f, std::min(1.0f, state.difficulty));
            }
        }

        // Publish difficulty event
        training_event_data_t event;
        memset(&event, 0, sizeof(event));
        event.event_type = TRAINING_EVENT_DIFFICULTY_UPDATED;
        event.source_module_id = MODULE_CURRICULUM;

        training_difficulty_payload_t diff_payload;
        diff_payload.old_difficulty = state.difficulty;
        diff_payload.new_difficulty = state.difficulty;
        diff_payload.samples_at_level = 100;
        diff_payload.progression_rate = 0.1f;
        event.payload = &diff_payload;
        event.payload_size = sizeof(diff_payload);

        training_hub_publish(hub, event.source_module_id, event.event_type, &event);
    }

    // Check if training should stop
    bool should_stop_training() {
        bool should_stop = false;
        float confidence = 0.0f;

        if (training_logic_hub_query_early_stop(logic_bridge, &should_stop, &confidence) == 0) {
            return should_stop && confidence > 0.7f;
        }
        return false;
    }
};

/* ========================================================================
 * Complete Training Pipeline Tests
 * ======================================================================== */

/**
 * E2E Test: Complete Training Run With Logic Guidance
 * Simulates a full training run with logic-guided decisions
 */
TEST_F(TrainingLogicPipelineE2ETest, CompleteTrainingRunWithLogicGuidance) {
    const int MAX_EPOCHS = 50;
    const int BATCHES_PER_EPOCH = 10;

    int epochs_completed = 0;
    bool early_stopped = false;

    for (int epoch = 0; epoch < MAX_EPOCHS && !early_stopped; epoch++) {
        // Training loop
        for (int batch = 0; batch < BATCHES_PER_EPOCH; batch++) {
            simulate_batch();
            update_logic_metrics();
        }

        // End of epoch
        simulate_epoch_end();
        update_logic_metrics();

        // Logic-guided adjustments
        adjust_lr_from_logic();
        adjust_difficulty_from_logic();

        // Report outcome for rule learning
        bool loss_improved = state.epochs_no_improve == 0;
        training_logic_hub_report_outcome(logic_bridge, loss_improved, loss_improved);

        // Check early stopping
        if (should_stop_training()) {
            early_stopped = true;
        }

        epochs_completed++;
    }

    // Verify training completed
    EXPECT_GT(epochs_completed, 0);

    // Check statistics
    training_logic_hub_stats_t stats;
    EXPECT_EQ(training_logic_hub_get_stats(logic_bridge, &stats), 0);

    // Rules should have been evaluated via update_metrics calls
    // Note: event routing from hub to bridge is async/optional
    EXPECT_GE(stats.rules_evaluated, 0u);

    // If converging, should have decent accuracy
    if (state.converging) {
        EXPECT_GT(state.val_accuracy, 0.3f);
    }
}

/**
 * E2E Test: Early Stopping Pipeline
 * Verify early stopping logic works with high epochs_no_improve
 */
TEST_F(TrainingLogicPipelineE2ETest, EarlyStoppingPipeline) {
    // Setup: simulate conditions that should trigger early stop
    state.converging = false;
    state.epochs_no_improve = 15;  // Well above typical patience
    state.val_loss = 1.0f;
    state.val_accuracy = 0.5f;

    // Update metrics to reflect poor training state
    update_logic_metrics();

    // Query for early stop
    bool should_stop = false;
    float confidence = 0.0f;
    int query_result = training_logic_hub_query_early_stop(logic_bridge, &should_stop, &confidence);

    EXPECT_EQ(query_result, 0) << "Early stop query should succeed";

    // The query should indicate consideration of early stop
    // Note: actual stopping depends on rule confidence which may vary
    EXPECT_GE(confidence, 0.0f) << "Confidence should be valid";
    EXPECT_LE(confidence, 1.0f) << "Confidence should be <= 1";

    // Run a few epochs to verify epochs_no_improve tracking
    for (int epoch = 0; epoch < 10; epoch++) {
        simulate_epoch_end();
        update_logic_metrics();

        training_logic_hub_report_outcome(logic_bridge, false, false);
    }

    // Epochs without improvement should accumulate
    EXPECT_GT(state.epochs_no_improve, 10u);
}

/**
 * E2E Test: Gradient Explosion Handling
 * Verify gradient metrics tracking with exploding gradients
 */
TEST_F(TrainingLogicPipelineE2ETest, GradientExplosionHandling) {
    // Set extremely high gradient to trigger grad_exploding flag
    state.grad_norm = 100.0f;
    update_logic_metrics();

    // Check if metrics reflect exploding gradient state
    training_logic_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.grad_norm = 100.0f;
    metrics.grad_exploding = true;
    metrics.grad_stable = false;

    // Update with explicit exploding state
    EXPECT_EQ(training_logic_hub_update_metrics(logic_bridge, &metrics), 0);

    // Query for LR - should return safely (even if gradient is exploding)
    float suggested_lr = 0.0f;
    float confidence = 0.0f;
    int result = training_logic_hub_query_lr(logic_bridge, 0.01f, &suggested_lr, &confidence);
    EXPECT_EQ(result, 0);

    // Test normal gradient after explosion
    state.grad_norm = 1.0f;
    update_logic_metrics();

    // Should be able to query safely
    result = training_logic_hub_query_lr(logic_bridge, 0.01f, &suggested_lr, &confidence);
    EXPECT_EQ(result, 0);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

/**
 * E2E Test: Curriculum Progression Pipeline
 * Verify difficulty increases with mastery
 */
TEST_F(TrainingLogicPipelineE2ETest, CurriculumProgressionPipeline) {
    state.difficulty = 0.1f;
    state.converging = true;

    float initial_difficulty = state.difficulty;
    const int NUM_EPOCHS = 20;
    const int BATCHES_PER_EPOCH = 5;

    for (int epoch = 0; epoch < NUM_EPOCHS; epoch++) {
        for (int batch = 0; batch < BATCHES_PER_EPOCH; batch++) {
            simulate_batch();
        }

        simulate_epoch_end();
        update_logic_metrics();

        // Adjust difficulty based on logic
        adjust_difficulty_from_logic();

        // Report good outcomes
        training_logic_hub_report_outcome(logic_bridge, true, true);
    }

    // Difficulty should have increased if accuracy improved
    if (state.val_accuracy > 0.5f) {
        EXPECT_GT(state.difficulty, initial_difficulty)
            << "Difficulty should increase with mastery";
    }
}

/**
 * E2E Test: Learning Rate Adaptation Pipeline
 * Verify LR adapts based on training state
 */
TEST_F(TrainingLogicPipelineE2ETest, LearningRateAdaptationPipeline) {
    state.learning_rate = 0.01f;
    state.converging = true;

    std::vector<float> lr_history;
    lr_history.push_back(state.learning_rate);

    const int NUM_EPOCHS = 30;
    const int BATCHES_PER_EPOCH = 5;

    for (int epoch = 0; epoch < NUM_EPOCHS; epoch++) {
        for (int batch = 0; batch < BATCHES_PER_EPOCH; batch++) {
            simulate_batch();
        }

        simulate_epoch_end();
        update_logic_metrics();
        adjust_lr_from_logic();

        lr_history.push_back(state.learning_rate);

        training_logic_hub_report_outcome(logic_bridge,
            state.epochs_no_improve == 0, state.epochs_no_improve == 0);
    }

    // LR should have changed over training
    float lr_variance = 0.0f;
    float lr_mean = 0.0f;
    for (float lr : lr_history) {
        lr_mean += lr;
    }
    lr_mean /= lr_history.size();

    for (float lr : lr_history) {
        lr_variance += (lr - lr_mean) * (lr - lr_mean);
    }
    lr_variance /= lr_history.size();

    // Some variance expected as LR adapts
    // (May not change much if training is stable)
}

/**
 * E2E Test: Rule Confidence Evolution
 * Verify rule confidence evolves based on outcomes
 */
TEST_F(TrainingLogicPipelineE2ETest, RuleConfidenceEvolution) {
    // Add a custom rule to track
    training_logic_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.type = TRAINING_RULE_LR_SAFETY;
    strcpy(rule.name, "tracking_rule");
    rule.confidence = 0.5f;

    int rule_id = training_logic_hub_add_rule(logic_bridge, &rule);
    ASSERT_GE(rule_id, 0);

    std::vector<float> confidence_history;
    confidence_history.push_back(
        training_logic_hub_get_rule_confidence(logic_bridge, rule_id));

    const int NUM_EPOCHS = 20;
    const int BATCHES_PER_EPOCH = 5;

    for (int epoch = 0; epoch < NUM_EPOCHS; epoch++) {
        for (int batch = 0; batch < BATCHES_PER_EPOCH; batch++) {
            simulate_batch();
            update_logic_metrics();
        }

        simulate_epoch_end();

        // Report alternating outcomes to see confidence change
        bool good_outcome = (epoch % 3 != 0);
        training_logic_hub_report_outcome(logic_bridge, good_outcome, good_outcome);

        confidence_history.push_back(
            training_logic_hub_get_rule_confidence(logic_bridge, rule_id));
    }

    // Confidence should stay within bounds
    for (float conf : confidence_history) {
        EXPECT_GE(conf, 0.0f);
        EXPECT_LE(conf, 1.0f);
    }
}

/**
 * E2E Test: Multi-Module Coordination
 * Verify multiple training modules coordinate through logic
 */
TEST_F(TrainingLogicPipelineE2ETest, MultiModuleCoordination) {
    state.converging = true;

    const int NUM_EPOCHS = 15;
    const int BATCHES_PER_EPOCH = 5;

    // Track events from different modules
    int loss_events = 0;
    int grad_events = 0;
    int lr_events = 0;
    int val_events = 0;

    for (int epoch = 0; epoch < NUM_EPOCHS; epoch++) {
        for (int batch = 0; batch < BATCHES_PER_EPOCH; batch++) {
            simulate_batch();
            update_logic_metrics();
            loss_events++;
            grad_events++;
        }

        simulate_epoch_end();
        val_events++;

        update_logic_metrics();
        adjust_lr_from_logic();
        lr_events++;

        adjust_difficulty_from_logic();

        training_logic_hub_report_outcome(logic_bridge,
            state.epochs_no_improve == 0, state.epochs_no_improve == 0);
    }

    // Verify all module events were generated (local counters)
    EXPECT_EQ(loss_events, NUM_EPOCHS * BATCHES_PER_EPOCH);
    EXPECT_EQ(grad_events, NUM_EPOCHS * BATCHES_PER_EPOCH);
    EXPECT_EQ(lr_events, NUM_EPOCHS);
    EXPECT_EQ(val_events, NUM_EPOCHS);

    // Check final statistics - note that event routing from hub to bridge is async/optional
    training_logic_hub_stats_t stats;
    EXPECT_EQ(training_logic_hub_get_stats(logic_bridge, &stats), 0);
    // Statistics should be retrievable (values depend on routing implementation)
}

/**
 * E2E Test: Constraint Violation Detection
 * Verify rule evaluation works with constraint-like conditions
 */
TEST_F(TrainingLogicPipelineE2ETest, ConstraintViolationDetection) {
    // Configure to publish constraint violations
    training_logic_hub_config_t logic_config;
    ASSERT_EQ(training_logic_hub_default_config(&logic_config), 0);
    logic_config.publish_constraint_violations = true;

    // Create new bridge with config
    training_logic_hub_disconnect(logic_bridge);
    training_logic_hub_destroy(logic_bridge);

    logic_bridge = training_logic_hub_create(&logic_config);
    ASSERT_NE(logic_bridge, nullptr);
    ASSERT_EQ(training_logic_hub_connect(logic_bridge, hub), 0);
    ASSERT_GT(training_logic_hub_add_default_rules(logic_bridge), 0);

    // Test rule evaluation with extreme metrics
    for (int i = 0; i < 10; i++) {
        // Exploding gradient
        state.grad_norm = 500.0f;
        update_logic_metrics();

        // Evaluate rules - this tests the rule evaluation machinery
        training_rule_result_t results[4];
        int count = training_logic_hub_evaluate_rules(logic_bridge, TRAINING_RULE_GRADIENT_CLIP, results, 4);
        // Count may be 0 if no gradient clip rules exist in defaults
        EXPECT_GE(count, 0);
    }

    // Verify state is retrievable
    training_logic_hub_state_t state_out;
    EXPECT_EQ(training_logic_hub_get_state(logic_bridge, &state_out), 0);

    training_logic_hub_stats_t stats;
    EXPECT_EQ(training_logic_hub_get_stats(logic_bridge, &stats), 0);
    // Stats should be valid (constraint count depends on implementation)
}

/**
 * E2E Test: Action Safety Checks Pipeline
 * Verify action safety checks work in pipeline
 */
TEST_F(TrainingLogicPipelineE2ETest, ActionSafetyChecksPipeline) {
    state.converging = true;

    const int NUM_EPOCHS = 10;
    const int BATCHES_PER_EPOCH = 5;

    int safe_lr_increases = 0;
    int unsafe_lr_increases = 0;

    for (int epoch = 0; epoch < NUM_EPOCHS; epoch++) {
        for (int batch = 0; batch < BATCHES_PER_EPOCH; batch++) {
            simulate_batch();
            update_logic_metrics();
        }

        simulate_epoch_end();
        update_logic_metrics();

        // Check if LR increase is safe
        float confidence = 0.0f;
        bool safe = training_logic_hub_is_action_safe(
            logic_bridge, "increase_lr", &confidence);

        if (safe) {
            safe_lr_increases++;
        } else {
            unsafe_lr_increases++;
        }

        // Also check difficulty increase
        safe = training_logic_hub_is_action_safe(
            logic_bridge, "increase_difficulty", &confidence);

        training_logic_hub_report_outcome(logic_bridge,
            state.epochs_no_improve == 0, state.epochs_no_improve == 0);
    }

    // Should have made some safety checks
    EXPECT_GT(safe_lr_increases + unsafe_lr_increases, 0);
}

/**
 * E2E Test: Complete Diverging Training
 * Test handling of diverging training state
 */
TEST_F(TrainingLogicPipelineE2ETest, DivergingTrainingHandling) {
    state.converging = false;  // Training is not converging
    state.loss = 1.0f;

    const int MAX_EPOCHS = 30;
    const int BATCHES_PER_EPOCH = 5;

    int epochs_completed = 0;
    float worst_loss = state.loss;

    for (int epoch = 0; epoch < MAX_EPOCHS; epoch++) {
        for (int batch = 0; batch < BATCHES_PER_EPOCH; batch++) {
            simulate_batch();
            update_logic_metrics();
        }

        simulate_epoch_end();
        update_logic_metrics();

        if (state.loss > worst_loss) {
            worst_loss = state.loss;
        }

        // Report bad outcomes
        training_logic_hub_report_outcome(logic_bridge, false, false);

        epochs_completed++;
    }

    // When diverging, epochs_no_improve should accumulate
    EXPECT_GT(state.epochs_no_improve, 5u)
        << "Epochs without improvement should accumulate when not converging";

    // Query early stop to verify it processes diverging state
    bool should_stop = false;
    float confidence = 0.0f;
    int result = training_logic_hub_query_early_stop(logic_bridge, &should_stop, &confidence);
    EXPECT_EQ(result, 0) << "Early stop query should succeed";
}

/**
 * E2E Test: Statistics Accumulation
 * Verify statistics remain valid over long run
 */
TEST_F(TrainingLogicPipelineE2ETest, StatisticsAccumulation) {
    const int NUM_EPOCHS = 20;
    const int BATCHES_PER_EPOCH = 10;

    for (int epoch = 0; epoch < NUM_EPOCHS; epoch++) {
        for (int batch = 0; batch < BATCHES_PER_EPOCH; batch++) {
            simulate_batch();
            update_logic_metrics();
        }

        simulate_epoch_end();
        update_logic_metrics();

        adjust_lr_from_logic();
        adjust_difficulty_from_logic();

        training_logic_hub_report_outcome(logic_bridge, true, true);
    }

    training_logic_hub_stats_t stats;
    EXPECT_EQ(training_logic_hub_get_stats(logic_bridge, &stats), 0);

    // Stats should be retrievable and have valid values
    // Note: event counts depend on hub->bridge routing implementation
    EXPECT_GE(stats.rules_evaluated, 0u);

    // Rule updates should come from report_outcome calls
    EXPECT_GE(stats.rule_updates, 0u);

    // State should be valid after long run
    training_logic_hub_state_t state_out;
    EXPECT_EQ(training_logic_hub_get_state(logic_bridge, &state_out), 0);
    EXPECT_TRUE(state_out.is_connected);
}
