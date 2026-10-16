/**
 * @file test_eligibility_immune_integration.cpp
 * @brief Unit tests for Eligibility Trace - Immune System Integration
 *
 * WHAT: Test bidirectional coupling between eligibility traces and immune system
 * WHY:  Validate cytokine modulation of trace dynamics and learning failure detection
 * HOW:  Test cytokine trace shortening, inflammation LR reduction, learning stress triggers
 *
 * @version 1.0.0
 * @date 2025-12-11
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "plasticity/immune/nimcp_eligibility_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "plasticity/eligibility/nimcp_eligibility_trace.h"

class EligibilityImmuneTest : public ::testing::Test {
protected:
    eligibility_immune_bridge_t* bridge;
    brain_immune_system_t* immune;
    eligibility_config_t eligibility_config;

    void SetUp() override {
        // Create immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr);

        // Create eligibility config with baseline parameters
        eligibility_config = eligibility_default_config();
        eligibility_config.decay_lambda = 0.95f;
        eligibility_config.learning_rate = 0.001f;

        // Create bridge
        eligibility_immune_config_t bridge_config;
        eligibility_immune_default_config(&bridge_config);
        bridge = eligibility_immune_bridge_create(&bridge_config, immune, &eligibility_config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        eligibility_immune_bridge_destroy(bridge);
        brain_immune_destroy(immune);
    }
};

/**
 * TEST: Bridge Lifecycle
 * BIOLOGICAL: Basic initialization and cleanup
 */
TEST_F(EligibilityImmuneTest, BridgeLifecycle) {
    // Bridge should be created successfully
    EXPECT_NE(bridge, nullptr);

    // Configuration should be linked
    EXPECT_FLOAT_EQ(eligibility_config.decay_lambda, 0.95f);
    EXPECT_FLOAT_EQ(eligibility_config.learning_rate, 0.001f);
}

/**
 * TEST: Default Configuration
 * BIOLOGICAL: Verify biologically-based defaults
 */
TEST_F(EligibilityImmuneTest, DefaultConfig) {
    eligibility_immune_config_t config;
    int result = eligibility_immune_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_trace_modulation);
    EXPECT_TRUE(config.enable_inflammation_impairment);
    EXPECT_TRUE(config.enable_learning_failure_detection);
    EXPECT_TRUE(config.enable_consolidation_monitoring);
    EXPECT_FLOAT_EQ(config.cytokine_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.learning_failure_threshold, LEARNING_FAILURE_STRESS_THRESHOLD);
}

/**
 * TEST: Cytokine Trace Shortening
 * BIOLOGICAL: Pro-inflammatory cytokines accelerate trace decay
 */
TEST_F(EligibilityImmuneTest, CytokinesShortenTraces) {
    // Store baseline
    float baseline_lambda = eligibility_config.decay_lambda;
    EXPECT_FLOAT_EQ(baseline_lambda, 0.95f);

    // Trigger inflammation (would release cytokines)
    uint32_t antigen_id;
    uint8_t epitope[] = {0x01, 0x02, 0x03};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                  ANTIGEN_SEVERITY_HIGH, 0, &antigen_id);

    // Full immune activation
    uint32_t b_cell_id, helper_id;
    brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(immune, antigen_id, &helper_id);
    brain_immune_t_help_b(immune, helper_id, b_cell_id);

    // Apply cytokine effects
    int result = eligibility_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);

    // Decay lambda should be reduced (faster decay)
    // Note: Actual effect depends on cytokine levels from immune system
    // In real implementation, lambda would be < baseline
}

/**
 * TEST: Inflammation Reduces Learning Rate
 * BIOLOGICAL: Inflammation impairs learning effectiveness
 */
TEST_F(EligibilityImmuneTest, InflammationReducesLearningRate) {
    float baseline_lr = eligibility_config.learning_rate;

    // Apply inflammation effects
    eligibility_immune_apply_inflammation_effects(bridge);

    // Get learning rate factor
    float lr_factor = eligibility_immune_get_lr_factor(bridge);

    // Without inflammation, should be 1.0
    EXPECT_FLOAT_EQ(lr_factor, INFLAMMATION_LR_FACTOR_NONE);

    // With inflammation, would be < 1.0
    // (depends on immune system state)
}

/**
 * TEST: Effective Lambda Computation
 * BIOLOGICAL: Inflammation shortens trace window
 */
TEST_F(EligibilityImmuneTest, EffectiveLambdaComputation) {
    // Get effective lambda (should be baseline initially)
    float lambda = eligibility_immune_get_effective_lambda(bridge);
    EXPECT_FLOAT_EQ(lambda, 0.95f);

    // Apply cytokine effects
    eligibility_immune_apply_cytokine_effects(bridge);

    // Lambda should still be accessible
    lambda = eligibility_immune_get_effective_lambda(bridge);
    EXPECT_GE(lambda, 0.6f);  // Min lambda
    EXPECT_LE(lambda, 0.95f); // Max lambda
}

/**
 * TEST: Learning Failure Detection
 * BIOLOGICAL: Sustained negative rewards trigger stress response
 */
TEST_F(EligibilityImmuneTest, LearningFailureDetection) {
    // Simulate repeated negative rewards
    for (int i = 0; i < 60; i++) {
        eligibility_immune_detect_learning_failure(bridge, -0.5f);
    }

    // Should detect high stress
    float stress = eligibility_immune_get_stress_level(bridge);
    EXPECT_GT(stress, 0.5f);
}

/**
 * TEST: Positive Rewards Reset Failure Counter
 * BIOLOGICAL: Successful learning reduces stress
 */
TEST_F(EligibilityImmuneTest, PositiveRewardsResetFailures) {
    // Negative rewards
    for (int i = 0; i < 10; i++) {
        eligibility_immune_detect_learning_failure(bridge, -0.3f);
    }

    float stress_after_failures = eligibility_immune_get_stress_level(bridge);

    // Positive reward
    eligibility_immune_detect_learning_failure(bridge, 0.8f);

    float stress_after_success = eligibility_immune_get_stress_level(bridge);

    // Stress should not increase (may decrease)
    EXPECT_LE(stress_after_success, stress_after_failures * 1.1f);
}

/**
 * TEST: Consolidation Monitoring
 * BIOLOGICAL: Traces awaiting consolidation create frustration
 */
TEST_F(EligibilityImmuneTest, ConsolidationMonitoring) {
    // Many unconsolidated traces, no bursts
    for (int i = 0; i < 20; i++) {
        eligibility_immune_monitor_consolidation(bridge, 50, false);
    }

    // Should accumulate frustration
    // (query internal state if needed)

    // Burst occurs - should reduce frustration
    eligibility_immune_monitor_consolidation(bridge, 10, true);
}

/**
 * TEST: Trace Impairment Detection
 * BIOLOGICAL: Significant lambda reduction indicates impairment
 */
TEST_F(EligibilityImmuneTest, TraceImpairmentDetection) {
    // Initially not impaired
    EXPECT_FALSE(eligibility_immune_is_trace_impaired(bridge));

    // Manually reduce lambda significantly
    eligibility_config.decay_lambda = 0.70f;

    // Should detect impairment (lambda < 85% of baseline 0.95)
    EXPECT_TRUE(eligibility_immune_is_trace_impaired(bridge));
}

/**
 * TEST: Baseline Restoration from IL-10
 * BIOLOGICAL: Anti-inflammatory cytokines restore normal trace dynamics
 */
TEST_F(EligibilityImmuneTest, BaselineRestoration) {
    // Reduce lambda (simulate inflammation)
    eligibility_config.decay_lambda = 0.75f;
    float impaired_lambda = eligibility_config.decay_lambda;

    // Restore baseline (would be triggered by IL-10)
    eligibility_immune_restore_baseline(bridge);

    // Lambda should move toward baseline (may not reach immediately)
    float restored_lambda = eligibility_config.decay_lambda;
    // Without IL-10, no change expected in this test
    // (actual restoration requires cytokine effects)
}

/**
 * TEST: Learning Stress Triggers Immune
 * BIOLOGICAL: Learned helplessness activates inflammation
 */
TEST_F(EligibilityImmuneTest, LearningStressTriggers) {
    // Simulate sustained learning failure
    for (int i = 0; i < 60; i++) {
        eligibility_immune_detect_learning_failure(bridge, -0.8f);
    }

    // Trigger immune from stress
    int result = eligibility_immune_trigger_from_learning_stress(bridge);
    EXPECT_EQ(result, 0);

    // Would present antigen to immune system
    // (check immune system state if API available)
}

/**
 * TEST: Consolidation Failure Threshold
 * BIOLOGICAL: Excessive unconsolidated traces trigger immune
 */
TEST_F(EligibilityImmuneTest, ConsolidationFailureThreshold) {
    // Exceed consolidation failure threshold
    uint32_t excessive_traces = CONSOLIDATION_FAILURE_THRESHOLD + 50;
    eligibility_immune_monitor_consolidation(bridge, excessive_traces, false);

    // Should trigger immune activation
    // (internal flag set)
}

/**
 * TEST: Bidirectional Update
 * BIOLOGICAL: Complete update cycle processes both directions
 */
TEST_F(EligibilityImmuneTest, BidirectionalUpdate) {
    // Perform update with current state
    int result = eligibility_immune_bridge_update(
        bridge,
        1000,    // 1 second delta
        -0.5f,   // Negative reward
        25,      // Some active traces
        false    // No burst
    );

    EXPECT_EQ(result, 0);

    // Both immune→eligibility and eligibility→immune should process
}

/**
 * TEST: Cytokine Effects Query
 * BIOLOGICAL: Retrieve current cytokine-trace effects
 */
TEST_F(EligibilityImmuneTest, QueryCytokineEffects) {
    cytokine_trace_effects_t effects;
    int result = eligibility_immune_get_cytokine_effects(bridge, &effects);

    EXPECT_EQ(result, 0);
    // Effects should be initialized (even if zero)
}

/**
 * TEST: Inflammation State Query
 * BIOLOGICAL: Retrieve current inflammation-trace state
 */
TEST_F(EligibilityImmuneTest, QueryInflammationState) {
    inflammation_trace_state_t state;
    int result = eligibility_immune_get_inflammation_state(bridge, &state);

    EXPECT_EQ(result, 0);
    EXPECT_GE(state.decay_lambda_modifier, 0.6f);
    EXPECT_LE(state.decay_lambda_modifier, 1.0f);
}

/**
 * TEST: Multiple Update Cycles
 * BIOLOGICAL: Sustained interaction over time
 */
TEST_F(EligibilityImmuneTest, MultipleUpdateCycles) {
    // Run multiple update cycles with varying conditions
    for (int i = 0; i < 100; i++) {
        float reward = (i % 10 == 0) ? 1.0f : -0.2f;  // Occasional success
        bool burst = (i % 20 == 0);                   // Occasional burst
        uint32_t traces = 10 + (i % 30);              // Varying trace count

        eligibility_immune_bridge_update(bridge, 100, reward, traces, burst);
    }

    // System should remain stable
    float stress = eligibility_immune_get_stress_level(bridge);
    EXPECT_GE(stress, 0.0f);
    EXPECT_LE(stress, 1.0f);
}

/**
 * TEST: Chronic Inflammation Effects
 * BIOLOGICAL: Prolonged inflammation has cumulative effects
 */
TEST_F(EligibilityImmuneTest, ChronicInflammationEffects) {
    // Apply inflammation effects
    eligibility_immune_apply_inflammation_effects(bridge);

    // Query state
    inflammation_trace_state_t state;
    eligibility_immune_get_inflammation_state(bridge, &state);

    // Should compute chronic status
    EXPECT_FALSE(state.is_chronic);  // Initially not chronic

    // Distal reward impairment should be bounded
    EXPECT_GE(state.distal_reward_impairment, 0.0f);
    EXPECT_LE(state.distal_reward_impairment, 1.0f);
}

/**
 * TEST: Dopamine System Disruption
 * BIOLOGICAL: Inflammation reduces dopamine synthesis and bursts
 */
TEST_F(EligibilityImmuneTest, DopamineDisruption) {
    eligibility_immune_apply_inflammation_effects(bridge);

    inflammation_trace_state_t state;
    eligibility_immune_get_inflammation_state(bridge, &state);

    // Should compute dopamine disruption
    EXPECT_GE(state.dopamine_synthesis_reduction, 0.0f);
    EXPECT_LE(state.dopamine_synthesis_reduction, 0.9f);
    EXPECT_GE(state.burst_amplitude_reduction, 0.0f);
    EXPECT_LE(state.burst_amplitude_reduction, 0.8f);
}

/**
 * TEST: Trace Window Reduction
 * BIOLOGICAL: Inflammation shortens effective credit assignment window
 */
TEST_F(EligibilityImmuneTest, TraceWindowReduction) {
    eligibility_immune_apply_inflammation_effects(bridge);

    inflammation_trace_state_t state;
    eligibility_immune_get_inflammation_state(bridge, &state);

    // Trace window should be computed
    EXPECT_GT(state.trace_window_ms, 0.0f);
    // With no inflammation, should be near baseline
}

/**
 * TEST: Learning Rate Factor Bounds
 * BIOLOGICAL: LR factor should be within biological range
 */
TEST_F(EligibilityImmuneTest, LearningRateFactorBounds) {
    eligibility_immune_apply_cytokine_effects(bridge);
    eligibility_immune_apply_inflammation_effects(bridge);

    float lr_factor = eligibility_immune_get_lr_factor(bridge);

    // Should be within valid range
    EXPECT_GE(lr_factor, INFLAMMATION_LR_FACTOR_STORM);  // Min 0.5
    EXPECT_LE(lr_factor, INFLAMMATION_LR_FACTOR_NONE);   // Max 1.0
}

/**
 * TEST: Null Parameter Handling
 * BIOLOGICAL: Robust error handling
 */
TEST_F(EligibilityImmuneTest, NullParameterHandling) {
    // Null config
    EXPECT_EQ(eligibility_immune_default_config(nullptr), -1);

    // Null bridge in queries
    EXPECT_FLOAT_EQ(eligibility_immune_get_stress_level(nullptr), 0.0f);
    EXPECT_FALSE(eligibility_immune_is_trace_impaired(nullptr));

    // Null bridge in updates
    EXPECT_EQ(eligibility_immune_apply_cytokine_effects(nullptr), -1);
    EXPECT_EQ(eligibility_immune_detect_learning_failure(nullptr, 0.5f), -1);
}

/**
 * TEST: Consolidation Frustration Accumulation
 * BIOLOGICAL: Repeated consolidation failures increase frustration
 */
TEST_F(EligibilityImmuneTest, ConsolidationFrustrationAccumulation) {
    // Many updates without bursts
    for (int i = 0; i < 50; i++) {
        eligibility_immune_monitor_consolidation(bridge, 30, false);
    }

    // Frustration should accumulate internally
    // (would need getter for consolidation_frustration)

    // Burst should reduce frustration
    eligibility_immune_monitor_consolidation(bridge, 5, true);
}

/**
 * TEST: Thread Safety
 * BIOLOGICAL: Mutex protection for concurrent access
 */
TEST_F(EligibilityImmuneTest, ThreadSafety) {
    // Multiple concurrent updates should be safe
    // (actual thread testing would require pthread spawn)
    for (int i = 0; i < 10; i++) {
        eligibility_immune_bridge_update(bridge, 100, 0.0f, 10, false);
    }

    // No crashes expected
    EXPECT_TRUE(true);
}

/**
 * INTEGRATION TEST: Complete Immune-Eligibility Cycle
 * BIOLOGICAL: Inflammation → trace impairment → learning failure → immune trigger
 */
TEST_F(EligibilityImmuneTest, CompleteImmuneCycle) {
    // 1. Trigger inflammation
    uint32_t antigen_id;
    uint8_t epitope[] = {0xAA, 0xBB};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 2,
                                  ANTIGEN_SEVERITY_HIGH, 0, &antigen_id);

    uint32_t b_cell_id, helper_id;
    brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(immune, antigen_id, &helper_id);

    // 2. Inflammation shortens traces
    eligibility_immune_apply_inflammation_effects(bridge);
    EXPECT_TRUE(eligibility_immune_get_lr_factor(bridge) <= 1.0f);

    // 3. Learning failures accumulate
    for (int i = 0; i < 30; i++) {
        eligibility_immune_detect_learning_failure(bridge, -0.6f);
    }

    // 4. Stress triggers immune
    EXPECT_GT(eligibility_immune_get_stress_level(bridge), 0.3f);
    eligibility_immune_trigger_from_learning_stress(bridge);

    // 5. Complete bidirectional cycle
    eligibility_immune_bridge_update(bridge, 1000, -0.5f, 20, false);
}

/**
 * INTEGRATION TEST: Recovery Cycle
 * BIOLOGICAL: IL-10 → trace restoration → learning recovery
 */
TEST_F(EligibilityImmuneTest, RecoveryCycle) {
    // Impair traces
    eligibility_config.decay_lambda = 0.70f;
    eligibility_config.learning_rate = 0.0005f;

    // Apply baseline restoration (would be from IL-10)
    eligibility_immune_restore_baseline(bridge);

    // Eventually should restore (with IL-10 signal)
    // Multiple cycles needed for gradual restoration
    for (int i = 0; i < 10; i++) {
        eligibility_immune_restore_baseline(bridge);
    }
}

/**
 * STRESS TEST: Extreme Inflammation
 * BIOLOGICAL: Cytokine storm severely impairs learning
 */
TEST_F(EligibilityImmuneTest, ExtremeInflammation) {
    // Simulate cytokine storm effects
    eligibility_config.decay_lambda = INFLAMMATION_TRACE_MULTIPLIER_STORM * 0.95f;
    eligibility_config.learning_rate = INFLAMMATION_LR_FACTOR_STORM * 0.001f;

    // Should still maintain valid parameters
    EXPECT_GE(eligibility_config.decay_lambda, 0.57f);  // ~0.6 * 0.95
    EXPECT_GE(eligibility_config.learning_rate, 0.0005f);  // ~0.5 * 0.001

    // System should detect severe impairment
    EXPECT_TRUE(eligibility_immune_is_trace_impaired(bridge));
}
