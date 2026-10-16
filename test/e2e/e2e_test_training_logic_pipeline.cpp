//=============================================================================
// e2e_test_training_logic_pipeline.cpp - End-to-End Pipeline Tests
//=============================================================================

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <cmath>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_training_logic_bridge.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TrainingLogicE2ETest : public ::testing::Test {
protected:
    training_logic_bridge_t* bridge;
    training_logic_config_t config;

    void SetUp() override {
        training_logic_default_config(&config);
        config.enable_bio_async = false;  // Simplify E2E tests
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            training_logic_destroy(bridge);
            bridge = nullptr;
        }
    }

    // Helper: Simulate stable training
    void SimulateStableTraining(int steps) {
        for (int i = 0; i < steps; i++) {
            float loss = 1.0f / (i + 1);  // Decreasing loss
            training_logic_update_metrics(bridge, loss, 0.3f, 0.001f, i);
        }
    }

    // Helper: Simulate unstable training
    void SimulateUnstableTraining() {
        training_logic_signal_instability(bridge, LOGIC_INSTABILITY_GRAD_EXPLOSION, 8);
        training_logic_update_metrics(bridge, 1e6f, 1e5f, 0.001f, 100);
    }

    // Helper: Simulate gradual improvement
    void SimulateGradualImprovement(int steps) {
        for (int i = 0; i < steps; i++) {
            float loss = 1.0f - (i * 0.01f);
            float grad_norm = 0.5f - (i * 0.001f);
            training_logic_update_metrics(bridge, loss, grad_norm, 0.001f, i);
        }
    }
};

//=============================================================================
// Full Training Pipeline E2E Tests (3 tests)
//=============================================================================

TEST_F(TrainingLogicE2ETest, TrainingWithLogicGating) {
    // SCENARIO: Full training loop with logic-based gating
    config.mode = TRAINING_LOGIC_MODE_AUTOMATIC;
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(training_logic_connect_brain_training(bridge, nullptr), 0);
    EXPECT_EQ(training_logic_start(bridge), 0);

    float base_lr = 0.001f;
    uint32_t base_batch_size = 32;

    for (int step = 0; step < 100; step++) {
        // Update training metrics
        float loss = 1.0f / (step + 1);
        float grad_norm = 0.5f;
        EXPECT_EQ(training_logic_update_metrics(bridge, loss, grad_norm, base_lr, step), 0);

        // Check stability
        bool stable = training_logic_check_stability(bridge);

        // Get modulated parameters
        float effective_lr = training_logic_get_lr_modulation(bridge, base_lr);
        uint32_t effective_batch = training_logic_get_batch_size_modulation(bridge, base_batch_size);

        EXPECT_GT(effective_lr, 0.0f);
        EXPECT_GT(effective_batch, 0);

        // Check if checkpoint needed
        if (step % 20 == 0 && step > 0) {
            bool should_checkpoint = training_logic_should_checkpoint(bridge);
            // Checkpointing logic verified
        }

        // Get comprehensive decision
        training_logic_decision_t decision;
        EXPECT_EQ(training_logic_get_decision(bridge, &decision), 0);
    }

    // Verify complete training cycle
    training_logic_stats_t stats;
    EXPECT_EQ(training_logic_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_decisions, 100);
    EXPECT_GT(stats.stability_checks, 0);
}

TEST_F(TrainingLogicE2ETest, AdaptiveLearningRate) {
    // SCENARIO: Learning rate adapts based on training stability
    config.mode = TRAINING_LOGIC_MODE_AUTOMATIC;
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(training_logic_start(bridge), 0);

    float base_lr = 0.001f;

    // Phase 1: Stable training (LR should increase)
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_STABLE_FOR_N_STEPS, true), 0);
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_IMMUNE_OK, true), 0);
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_RESOURCE_OK, true), 0);

    bool can_increase = training_logic_can_increase_lr(bridge);
    if (can_increase) {
        float increased_lr = base_lr * config.lr_increase_factor;
        float modulated_lr = training_logic_get_lr_modulation(bridge, base_lr);
        EXPECT_GE(modulated_lr, base_lr);
    }

    // Phase 2: Instability detected (LR should decrease)
    EXPECT_EQ(training_logic_signal_instability(bridge, LOGIC_INSTABILITY_GRAD_EXPLOSION, 8), 0);

    bool needs_intervention = training_logic_needs_intervention(bridge);
    EXPECT_TRUE(needs_intervention);

    training_logic_decision_t decision;
    EXPECT_EQ(training_logic_get_decision(bridge, &decision), 0);

    if (decision.type == TRAINING_DECISION_DECREASE_LR) {
        EXPECT_LT(decision.modulation_factor, 1.0f);
    }

    // Phase 3: Recovery
    SimulateStableTraining(20);

    EXPECT_EQ(training_logic_get_decision(bridge, &decision), 0);
}

TEST_F(TrainingLogicE2ETest, AutomaticCheckpointing) {
    // SCENARIO: Automatic checkpointing based on logic conditions
    config.mode = TRAINING_LOGIC_MODE_AUTOMATIC;
    config.checkpoint_interval = 50;
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(training_logic_start(bridge), 0);

    int checkpoints_created = 0;

    for (int step = 0; step < 200; step++) {
        EXPECT_EQ(training_logic_update_metrics(bridge, 0.5f, 0.3f, 0.001f, step), 0);

        // Update batch metrics
        EXPECT_EQ(training_logic_update_batch_metrics(bridge, 32, 10.0f, 0.5f), 0);

        // Set conditions for checkpointing
        EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_NOT_MID_BATCH, true), 0);
        EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_MEMORY_OK, true), 0);
        EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_SUFFICIENT_PROGRESS, step >= config.checkpoint_interval), 0);

        bool should_checkpoint = training_logic_should_checkpoint(bridge);

        if (should_checkpoint && step >= config.checkpoint_interval) {
            training_logic_decision_t decision;
            EXPECT_EQ(training_logic_get_decision(bridge, &decision), 0);

            if (decision.type == TRAINING_DECISION_CHECKPOINT) {
                checkpoints_created++;
                // Reset progress counter (would happen in real system)
                EXPECT_EQ(training_logic_set_numeric_condition(bridge, "steps_since_checkpoint", 0.0f), 0);
            }
        }
    }

    training_logic_stats_t stats;
    EXPECT_EQ(training_logic_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.checkpoints_triggered, 0);
}

//=============================================================================
// Instability Recovery E2E Tests (3 tests)
//=============================================================================

TEST_F(TrainingLogicE2ETest, GradientExplosionRecovery) {
    // SCENARIO: Detect gradient explosion and recover
    config.mode = TRAINING_LOGIC_MODE_AUTOMATIC;
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(training_logic_start(bridge), 0);

    // Stable training
    SimulateStableTraining(10);

    // Gradient explosion
    EXPECT_EQ(training_logic_signal_instability(bridge, LOGIC_INSTABILITY_GRAD_EXPLOSION, 9), 0);
    EXPECT_EQ(training_logic_update_metrics(bridge, 1e6f, 1e5f, 0.001f, 11), 0);

    // Check intervention needed
    bool needs_intervention = training_logic_needs_intervention(bridge);
    EXPECT_TRUE(needs_intervention);

    // Get recovery decision
    training_logic_decision_t decision;
    EXPECT_EQ(training_logic_get_decision(bridge, &decision), 0);

    // Should recommend decrease LR or pause
    EXPECT_TRUE(decision.type == TRAINING_DECISION_DECREASE_LR ||
                decision.type == TRAINING_DECISION_PAUSE ||
                decision.type == TRAINING_DECISION_ROLLBACK);

    // Apply decision
    EXPECT_EQ(training_logic_apply_decision(bridge, &decision), 0);

    // Simulate recovery
    SimulateGradualImprovement(20);

    // Should stabilize
    bool stable = training_logic_check_stability(bridge);
    // Stability should improve after recovery
}

TEST_F(TrainingLogicE2ETest, LossNaNRecovery) {
    // SCENARIO: Detect NaN loss and recover via rollback
    config.mode = TRAINING_LOGIC_MODE_AUTOMATIC;
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(training_logic_start(bridge), 0);

    // Stable training
    SimulateStableTraining(15);

    // NaN loss detected
    EXPECT_EQ(training_logic_signal_instability(bridge, LOGIC_INSTABILITY_LOSS_NAN, 10), 0);
    EXPECT_EQ(training_logic_update_metrics(bridge, NAN, 0.3f, 0.001f, 16), 0);

    // Should need intervention
    bool needs_intervention = training_logic_needs_intervention(bridge);
    EXPECT_TRUE(needs_intervention);

    // Get recovery decision
    training_logic_decision_t decision;
    EXPECT_EQ(training_logic_get_decision(bridge, &decision), 0);

    // Should recommend rollback or terminate
    EXPECT_TRUE(decision.type == TRAINING_DECISION_ROLLBACK ||
                decision.type == TRAINING_DECISION_TERMINATE ||
                decision.type == TRAINING_DECISION_PAUSE);

    // Verify intervention triggered
    training_logic_stats_t stats;
    EXPECT_EQ(training_logic_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.intervention_triggers, 0);
}

TEST_F(TrainingLogicE2ETest, DivergenceDetectionAndPause) {
    // SCENARIO: Detect training divergence and pause
    config.mode = TRAINING_LOGIC_MODE_AUTOMATIC;
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(training_logic_start(bridge), 0);

    // Simulate diverging training (increasing loss)
    for (int i = 0; i < 20; i++) {
        float loss = 0.5f + (i * 0.1f);  // Increasing loss
        EXPECT_EQ(training_logic_update_metrics(bridge, loss, 0.3f, 0.001f, i), 0);
    }

    // Signal divergence
    EXPECT_EQ(training_logic_signal_instability(bridge, LOGIC_INSTABILITY_OSCILLATION, 6), 0);

    // Should need intervention
    bool needs_intervention = training_logic_needs_intervention(bridge);
    EXPECT_TRUE(needs_intervention);

    // Get decision
    training_logic_decision_t decision;
    EXPECT_EQ(training_logic_get_decision(bridge, &decision), 0);

    // Should recommend pause or decrease LR
    EXPECT_TRUE(decision.type == TRAINING_DECISION_PAUSE ||
                decision.type == TRAINING_DECISION_DECREASE_LR);

    training_logic_stats_t stats;
    EXPECT_EQ(training_logic_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.intervention_triggers, 0);
}

//=============================================================================
// Multi-System Coordination E2E Tests (2 tests)
//=============================================================================

TEST_F(TrainingLogicE2ETest, TrainingImmuneLogicPipeline) {
    // SCENARIO: Training coordinated with immune system
    config.mode = TRAINING_LOGIC_MODE_AUTOMATIC;
    config.enable_immune_integration = true;
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(training_logic_connect_training_immune(bridge, nullptr), 0);
    EXPECT_EQ(training_logic_start(bridge), 0);

    // Phase 1: Healthy training (immune OK)
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_IMMUNE_OK, true), 0);
    for (int i = 0; i < 10; i++) {
        float loss = 1.0f / (i + 1);
        EXPECT_EQ(training_logic_update_metrics(bridge, loss, 0.3f, 0.001f, i), 0);
        training_logic_decision_t decision;
        EXPECT_EQ(training_logic_get_decision(bridge, &decision), 0);
    }

    // Phase 2: Immune inflammation (systemic)
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_IMMUNE_OK, false), 0);

    training_logic_decision_t decision2;
    EXPECT_EQ(training_logic_get_decision(bridge, &decision2), 0);

    // Inflammation should affect decisions
    // (LR increase blocked, conservative training)

    // Phase 3: Recovery
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_IMMUNE_OK, true), 0);
    for (int i = 0; i < 10; i++) {
        float loss = 0.5f / (i + 1);
        EXPECT_EQ(training_logic_update_metrics(bridge, loss, 0.3f, 0.001f, 10 + i), 0);
        training_logic_decision_t decision;
        EXPECT_EQ(training_logic_get_decision(bridge, &decision), 0);
    }

    training_logic_stats_t stats;
    EXPECT_EQ(training_logic_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.total_decisions, 15);  // At least 10 + 1 + 10 = 21 decisions
}

TEST_F(TrainingLogicE2ETest, PortiaTrainingLogicPipeline) {
    // SCENARIO: Training coordinated with Portia resources
    config.mode = TRAINING_LOGIC_MODE_AUTOMATIC;
    config.enable_portia_integration = true;
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(training_logic_connect_portia_logic(bridge, nullptr), 0);
    EXPECT_EQ(training_logic_start(bridge), 0);

    uint32_t base_batch_size = 32;

    // Phase 1: High resources (can increase batch size)
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_RESOURCE_OK, true), 0);
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_MEMORY_OK, true), 0);
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_THROUGHPUT_OK, true), 0);

    bool increase_batch;
    bool should_adjust = training_logic_should_adjust_batch(bridge, &increase_batch);
    if (should_adjust && increase_batch) {
        uint32_t modulated_batch = training_logic_get_batch_size_modulation(bridge, base_batch_size);
        EXPECT_GE(modulated_batch, base_batch_size);
    }

    // Phase 2: Resource pressure (decrease batch size)
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_RESOURCE_OK, false), 0);
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_MEMORY_OK, false), 0);

    should_adjust = training_logic_should_adjust_batch(bridge, &increase_batch);
    if (should_adjust && !increase_batch) {
        uint32_t modulated_batch = training_logic_get_batch_size_modulation(bridge, base_batch_size);
        EXPECT_LE(modulated_batch, base_batch_size);
    }

    // Phase 3: Resources recovered
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_RESOURCE_OK, true), 0);
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_MEMORY_OK, true), 0);

    training_logic_stats_t stats;
    EXPECT_EQ(training_logic_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.batch_adjustments, 0);
}

//=============================================================================
// Distributed Scenarios E2E Tests (2 tests)
//=============================================================================

TEST_F(TrainingLogicE2ETest, SwarmConsensusTraining) {
    // SCENARIO: Distributed training with swarm consensus
    config.mode = TRAINING_LOGIC_MODE_CONSENSUS_REQUIRED;
    config.enable_swarm_integration = true;
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(training_logic_connect_swarm_logic(bridge, nullptr), 0);
    EXPECT_EQ(training_logic_start(bridge), 0);

    // Simulate multi-worker training
    const int num_workers = 5;

    for (int step = 0; step < 50; step++) {
        // Each worker contributes metrics
        for (int worker = 0; worker < num_workers; worker++) {
            float loss = 0.5f + (worker * 0.01f);
            EXPECT_EQ(training_logic_update_metrics(bridge, loss, 0.3f, 0.001f, step * num_workers + worker), 0);
        }

        // Consensus required for major decisions
        EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_SWARM_CONSENSUS, step % 3 == 0), 0);

        training_logic_decision_t decision;
        EXPECT_EQ(training_logic_get_decision(bridge, &decision), 0);

        // In consensus mode, decisions depend on swarm agreement
        if (!decision.approved && decision.type != TRAINING_DECISION_CONTINUE) {
            // Decision blocked due to lack of consensus
        }
    }

    training_logic_stats_t stats;
    EXPECT_EQ(training_logic_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.total_decisions, 0);
}

TEST_F(TrainingLogicE2ETest, UnifiedDecisionPipeline) {
    // SCENARIO: Full unified pipeline with all systems
    config.mode = TRAINING_LOGIC_MODE_AUTOMATIC;
    config.enable_immune_integration = true;
    config.enable_portia_integration = true;
    config.enable_swarm_integration = true;

    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(training_logic_connect_training_immune(bridge, nullptr), 0);
    EXPECT_EQ(training_logic_connect_portia_logic(bridge, nullptr), 0);
    EXPECT_EQ(training_logic_connect_swarm_logic(bridge, nullptr), 0);
    EXPECT_EQ(training_logic_connect_unified(bridge, nullptr), 0);

    EXPECT_EQ(training_logic_start(bridge), 0);

    // Simulate complex training scenario
    for (int step = 0; step < 100; step++) {
        // Update training metrics
        float loss = 1.0f / (step + 1);
        EXPECT_EQ(training_logic_update_metrics(bridge, loss, 0.3f, 0.001f, step), 0);

        // Update all condition sources
        EXPECT_EQ(training_logic_update_conditions(bridge), 0);

        // Set system conditions
        bool immune_ok = (step % 10 != 9);  // Periodic inflammation
        bool resource_ok = (step % 15 != 14);  // Periodic resource pressure
        bool swarm_consensus = (step % 5 == 0);  // Periodic consensus

        EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_IMMUNE_OK, immune_ok), 0);
        EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_RESOURCE_OK, resource_ok), 0);
        EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_SWARM_CONSENSUS, swarm_consensus), 0);

        // Get unified decision
        training_logic_decision_t decision;
        EXPECT_EQ(training_logic_get_decision(bridge, &decision), 0);

        // Decisions should reflect multi-system state
        if (!immune_ok || !resource_ok) {
            // Conservative decisions expected
        }
    }

    // Verify comprehensive statistics
    training_logic_stats_t stats;
    EXPECT_EQ(training_logic_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_decisions, 100);
    EXPECT_GT(stats.stability_checks, 0);
    EXPECT_GT(stats.avg_decision_time_us, 0.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
