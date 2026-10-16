//=============================================================================
// test_training_logic_bridge_integration.cpp - Integration Tests
//=============================================================================

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_training_logic_bridge.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TrainingLogicIntegrationTest : public ::testing::Test {
protected:
    training_logic_bridge_t* bridge;
    training_logic_config_t config;

    void SetUp() override {
        training_logic_default_config(&config);
        config.enable_bio_async = false;  // Disable for controlled integration tests
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            training_logic_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Brain Training Integration Tests (3 tests)
//=============================================================================

TEST_F(TrainingLogicIntegrationTest, ConnectBrainTraining) {
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Connect brain training (NULL for testing)
    EXPECT_EQ(training_logic_connect_brain_training(bridge, nullptr), 0);

    training_logic_start(bridge);

    // Make training decisions
    training_logic_decision_t decision;
    EXPECT_EQ(training_logic_get_decision(bridge, &decision), 0);
}

TEST_F(TrainingLogicIntegrationTest, TrainingStepWithLogic) {
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(training_logic_connect_brain_training(bridge, nullptr), 0);
    EXPECT_EQ(training_logic_start(bridge), 0);

    // Simulate training steps with metrics
    for (int step = 0; step < 10; step++) {
        float loss = 1.0f / (step + 1);  // Decreasing loss
        float grad_norm = 0.5f;
        float lr = 0.001f;

        EXPECT_EQ(training_logic_update_metrics(bridge, loss, grad_norm, lr, step), 0);

        // Check stability
        bool stable = training_logic_check_stability(bridge);
        EXPECT_TRUE(stable);  // Should be stable with reasonable metrics
    }
}

TEST_F(TrainingLogicIntegrationTest, BiologicalTrainingWithLogic) {
    config.mode = TRAINING_LOGIC_MODE_AUTOMATIC;
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(training_logic_connect_brain_training(bridge, nullptr), 0);
    EXPECT_EQ(training_logic_start(bridge), 0);

    // Simulate biologically-inspired training cycle
    float base_lr = 0.001f;

    for (int step = 0; step < 20; step++) {
        // Update metrics
        EXPECT_EQ(training_logic_update_metrics(bridge, 0.5f, 0.3f, base_lr, step), 0);

        // Get modulated LR
        float effective_lr = training_logic_get_lr_modulation(bridge, base_lr);
        EXPECT_GT(effective_lr, 0.0f);

        // Get comprehensive decision
        training_logic_decision_t decision;
        EXPECT_EQ(training_logic_get_decision(bridge, &decision), 0);
    }
}

//=============================================================================
// Training-Immune Integration Tests (3 tests)
//=============================================================================

TEST_F(TrainingLogicIntegrationTest, ConnectTrainingImmune) {
    config.enable_immune_integration = true;
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Connect immune system (NULL for testing)
    EXPECT_EQ(training_logic_connect_training_immune(bridge, nullptr), 0);

    training_logic_start(bridge);

    // Make decision with immune connected
    training_logic_decision_t decision;
    EXPECT_EQ(training_logic_get_decision(bridge, &decision), 0);
}

TEST_F(TrainingLogicIntegrationTest, InflammationAffectsDecisions) {
    config.enable_immune_integration = true;
    config.disable_auto_update = true;  // Allow manual condition control
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(training_logic_connect_training_immune(bridge, nullptr), 0);
    EXPECT_EQ(training_logic_start(bridge), 0);

    // Set up conditions for LR increase
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_STABLE_FOR_N_STEPS, true), 0);
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_IMMUNE_OK, true), 0);
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_RESOURCE_OK, true), 0);

    bool can_increase = training_logic_can_increase_lr(bridge);
    EXPECT_TRUE(can_increase);

    // Simulate severe inflammation
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_IMMUNE_OK, false), 0);

    can_increase = training_logic_can_increase_lr(bridge);
    EXPECT_FALSE(can_increase);  // Inflammation blocks LR increase
}

TEST_F(TrainingLogicIntegrationTest, InstabilityTriggersImmune) {
    config.enable_immune_integration = true;
    config.disable_auto_update = true;  // Prevent conditions from being reset
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(training_logic_connect_training_immune(bridge, nullptr), 0);
    EXPECT_EQ(training_logic_start(bridge), 0);

    // Signal various instabilities
    EXPECT_EQ(training_logic_signal_instability(bridge, LOGIC_INSTABILITY_GRAD_EXPLOSION, 8), 0);
    EXPECT_TRUE(training_logic_needs_intervention(bridge));

    EXPECT_EQ(training_logic_signal_instability(bridge, LOGIC_INSTABILITY_LOSS_NAN, 10), 0);
    EXPECT_TRUE(training_logic_needs_intervention(bridge));

    // Verify stats tracked
    training_logic_stats_t stats;
    EXPECT_EQ(training_logic_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.intervention_triggers, 0);
}

//=============================================================================
// Portia-Logic Integration Tests (3 tests)
//=============================================================================

TEST_F(TrainingLogicIntegrationTest, ConnectPortiaLogic) {
    config.enable_portia_integration = true;
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Connect Portia logic (NULL for testing)
    EXPECT_EQ(training_logic_connect_portia_logic(bridge, nullptr), 0);

    training_logic_start(bridge);

    // Make decision with Portia connected
    training_logic_decision_t decision;
    EXPECT_EQ(training_logic_get_decision(bridge, &decision), 0);
}

TEST_F(TrainingLogicIntegrationTest, ResourceAffectsDecisions) {
    config.enable_portia_integration = true;
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(training_logic_connect_portia_logic(bridge, nullptr), 0);
    EXPECT_EQ(training_logic_start(bridge), 0);

    // With resources OK, batch size can be adjusted
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_RESOURCE_OK, true), 0);
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_MEMORY_OK, true), 0);
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_THROUGHPUT_OK, true), 0);

    bool increase_batch;
    bool should_adjust = training_logic_should_adjust_batch(bridge, &increase_batch);
    EXPECT_TRUE(should_adjust);

    // With resources constrained, decisions change
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_RESOURCE_OK, false), 0);

    training_logic_decision_t decision;
    EXPECT_EQ(training_logic_get_decision(bridge, &decision), 0);
}

TEST_F(TrainingLogicIntegrationTest, TierChangeAffectsLogic) {
    config.enable_portia_integration = true;
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(training_logic_connect_portia_logic(bridge, nullptr), 0);
    EXPECT_EQ(training_logic_start(bridge), 0);

    // Simulate tier changes affecting training decisions
    for (int tier = 0; tier < 4; tier++) {
        // Update resource condition based on tier
        bool resources_ok = (tier >= 2);
        EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_RESOURCE_OK, resources_ok), 0);

        training_logic_decision_t decision;
        EXPECT_EQ(training_logic_get_decision(bridge, &decision), 0);

        // Higher tiers should allow more aggressive training
        if (tier >= 2) {
            EXPECT_GE(decision.confidence, 0.5f);
        }
    }
}

//=============================================================================
// Swarm-Logic Integration Tests (3 tests)
//=============================================================================

TEST_F(TrainingLogicIntegrationTest, ConnectSwarmLogic) {
    config.enable_swarm_integration = true;
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Connect swarm logic (NULL for testing)
    EXPECT_EQ(training_logic_connect_swarm_logic(bridge, nullptr), 0);

    training_logic_start(bridge);

    // Make decision with swarm connected
    training_logic_decision_t decision;
    EXPECT_EQ(training_logic_get_decision(bridge, &decision), 0);
}

TEST_F(TrainingLogicIntegrationTest, ConsensusRequiredDecisions) {
    config.mode = TRAINING_LOGIC_MODE_CONSENSUS_REQUIRED;
    config.enable_swarm_integration = true;
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(training_logic_connect_swarm_logic(bridge, nullptr), 0);
    EXPECT_EQ(training_logic_start(bridge), 0);

    // In consensus mode, swarm approval required
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_SWARM_CONSENSUS, true), 0);

    training_logic_decision_t decision;
    EXPECT_EQ(training_logic_get_decision(bridge, &decision), 0);

    // Without consensus, decisions should be blocked
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_SWARM_CONSENSUS, false), 0);

    EXPECT_EQ(training_logic_get_decision(bridge, &decision), 0);
}

TEST_F(TrainingLogicIntegrationTest, DistributedTrainingConsensus) {
    config.mode = TRAINING_LOGIC_MODE_CONSENSUS_REQUIRED;
    config.enable_swarm_integration = true;
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(training_logic_connect_swarm_logic(bridge, nullptr), 0);
    EXPECT_EQ(training_logic_start(bridge), 0);

    // Simulate distributed training scenario
    for (int worker = 0; worker < 5; worker++) {
        // Each worker updates metrics
        float loss = 0.5f + (worker * 0.01f);
        EXPECT_EQ(training_logic_update_metrics(bridge, loss, 0.3f, 0.001f, worker * 10), 0);

        // Swarm consensus determines collective action
        EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_SWARM_CONSENSUS, true), 0);

        training_logic_decision_t decision;
        EXPECT_EQ(training_logic_get_decision(bridge, &decision), 0);
    }

    training_logic_stats_t stats;
    EXPECT_EQ(training_logic_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.total_decisions, 0);
}

//=============================================================================
// Unified Bridge Integration Tests (3 tests)
//=============================================================================

TEST_F(TrainingLogicIntegrationTest, ConnectUnifiedBridge) {
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Connect unified bridge (NULL for testing)
    EXPECT_EQ(training_logic_connect_unified(bridge, nullptr), 0);

    training_logic_start(bridge);

    // Make decision with unified bridge connected
    training_logic_decision_t decision;
    EXPECT_EQ(training_logic_get_decision(bridge, &decision), 0);
}

TEST_F(TrainingLogicIntegrationTest, CoordinatedDecisions) {
    config.enable_immune_integration = true;
    config.enable_portia_integration = true;
    config.enable_swarm_integration = true;

    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Connect all systems via unified bridge
    EXPECT_EQ(training_logic_connect_unified(bridge, nullptr), 0);
    EXPECT_EQ(training_logic_start(bridge), 0);

    // Set up favorable conditions across all systems
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_IMMUNE_OK, true), 0);
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_RESOURCE_OK, true), 0);
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_SWARM_CONSENSUS, true), 0);

    // Should get coordinated approval
    training_logic_decision_t decision;
    EXPECT_EQ(training_logic_get_decision(bridge, &decision), 0);
    EXPECT_TRUE(decision.approved);
}

TEST_F(TrainingLogicIntegrationTest, MultiSystemConsensus) {
    config.mode = TRAINING_LOGIC_MODE_CONSENSUS_REQUIRED;
    config.enable_immune_integration = true;
    config.enable_portia_integration = true;
    config.enable_swarm_integration = true;

    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(training_logic_connect_unified(bridge, nullptr), 0);
    EXPECT_EQ(training_logic_start(bridge), 0);

    // Test decision requiring multi-system consensus
    EXPECT_EQ(training_logic_update_metrics(bridge, 0.5f, 0.3f, 0.001f, 100), 0);

    // All systems must agree in consensus mode
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_IMMUNE_OK, true), 0);
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_RESOURCE_OK, true), 0);
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_SWARM_CONSENSUS, true), 0);

    training_logic_decision_t decision;
    EXPECT_EQ(training_logic_get_decision(bridge, &decision), 0);

    // Break consensus
    EXPECT_EQ(training_logic_set_condition(bridge, TRAINING_COND_RESOURCE_OK, false), 0);

    EXPECT_EQ(training_logic_get_decision(bridge, &decision), 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
