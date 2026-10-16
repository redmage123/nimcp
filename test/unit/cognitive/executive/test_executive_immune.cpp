/**
 * @file test_executive_immune.cpp
 * @brief Unit tests for Executive-Immune System Integration
 *
 * WHAT: Test inflammation-induced cognitive impairment in executive functions
 * WHY:  Verify cytokine-induced cognitive fog affects task switching, inhibition, planning
 * HOW:  Create immune system with varying inflammation, test executive responses
 *
 * @date 2025-12-11
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/nimcp_executive.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/time/nimcp_time.h"

class ExecutiveImmuneTest : public ::testing::Test {
protected:
    executive_controller_t* exec;
    brain_immune_system_t* immune_system;

    void SetUp() override {
        // Create executive controller with immune integration enabled
        executive_config_t config = {};
        config.max_tasks = 16;
        config.task_switch_cost_ms = 200.0F;
        config.inhibition_threshold = 0.7F;
        config.max_plan_depth = 10;
        config.enable_task_prioritization = true;
        config.enable_deadline_checking = true;
        config.enable_portia_integration = false;
        config.enable_tom_integration = false;
        config.enable_immune_integration = true;  // ENABLE IMMUNE INTEGRATION
        config.immune_impairment_threshold = 0.6F;
        config.immune_critical_threshold = 0.85F;

        exec = executive_create_custom(&config);
        ASSERT_NE(exec, nullptr);

        // Create brain immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);

        // Associate immune system with executive controller
        executive_set_immune_system(exec, immune_system);
    }

    void TearDown() override {
        if (exec) {
            executive_destroy(exec);
        }
        if (immune_system) {
            brain_immune_destroy(immune_system);
        }
    }

    // Helper: Simulate inflammation by creating inflammation sites
    void simulate_inflammation(float level) {
        // level 0.0-1.0 maps to 0-64 sites (BRAIN_IMMUNE_MAX_INFLAMMATION=64)
        uint32_t num_sites = (uint32_t)(level * 64.0F);
        for (uint32_t i = 0; i < num_sites; i++) {
            uint32_t site_id = 0;
            // Create antigen first
            uint32_t antigen_id = 0;
            uint8_t epitope[64] = {(uint8_t)i};
            brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                          epitope, sizeof(epitope), 5, 0, &antigen_id);
            // Trigger inflammation
            brain_immune_initiate_inflammation(immune_system, i, antigen_id, &site_id);
        }
    }
};

//=============================================================================
// Cognitive Capacity Tests
//=============================================================================

TEST_F(ExecutiveImmuneTest, NormalCapacityWithNoInflammation) {
    // WHAT: Verify full capacity when no inflammation
    // WHY:  Baseline check for normal operation
    // HOW:  Query capacity, expect 1.0

    float capacity = executive_get_immune_adjusted_capacity(exec);
    EXPECT_FLOAT_EQ(capacity, 1.0F);
}

TEST_F(ExecutiveImmuneTest, ReducedCapacityWithModerateInflammation) {
    // WHAT: Verify reduced capacity with moderate inflammation
    // WHY:  Cytokines impair cognitive function
    // HOW:  Simulate 50% inflammation, expect ~55% capacity

    simulate_inflammation(0.5F);

    float capacity = executive_get_immune_adjusted_capacity(exec);

    // Expect capacity around 0.55 (1.0 - 0.5*0.9)
    EXPECT_GT(capacity, 0.5F);
    EXPECT_LT(capacity, 0.7F);
}

TEST_F(ExecutiveImmuneTest, SeverelyReducedCapacityWithHighInflammation) {
    // WHAT: Verify severe capacity reduction with high inflammation
    // WHY:  High cytokines cause cognitive fog
    // HOW:  Simulate 100% inflammation, expect ~10% capacity (floor)

    simulate_inflammation(1.0F);

    float capacity = executive_get_immune_adjusted_capacity(exec);

    // Expect capacity around 0.1 (floor value)
    EXPECT_FLOAT_EQ(capacity, 0.1F);
}

//=============================================================================
// Impairment Detection Tests
//=============================================================================

TEST_F(ExecutiveImmuneTest, NoImpairmentBelowThreshold) {
    // WHAT: Verify impairment not detected below threshold
    // WHY:  Threshold at 0.6, below should be non-impaired
    // HOW:  Simulate 0.5 inflammation, check impairment

    simulate_inflammation(0.5F);

    bool impaired = executive_is_immune_impaired(exec);
    EXPECT_FALSE(impaired);
}

TEST_F(ExecutiveImmuneTest, ImpairmentDetectedAboveThreshold) {
    // WHAT: Verify impairment detected above threshold
    // WHY:  Threshold at 0.6, above should trigger impairment
    // HOW:  Simulate 0.7 inflammation, check impairment

    simulate_inflammation(0.7F);

    bool impaired = executive_is_immune_impaired(exec);
    EXPECT_TRUE(impaired);
}

//=============================================================================
// Task Switch Cost Tests
//=============================================================================

TEST_F(ExecutiveImmuneTest, NormalSwitchCostWithoutInflammation) {
    // WHAT: Verify normal switch cost when no inflammation
    // WHY:  Baseline check for normal switching
    // HOW:  Query switch cost, expect base value (200ms)

    float cost = executive_get_immune_adjusted_switch_cost(exec);
    EXPECT_FLOAT_EQ(cost, 200.0F);
}

TEST_F(ExecutiveImmuneTest, IncreasedSwitchCostWithInflammation) {
    // WHAT: Verify increased switch cost with inflammation
    // WHY:  Cytokines increase cognitive rigidity
    // HOW:  Simulate 50% inflammation, expect 2x cost (400ms)

    simulate_inflammation(0.5F);

    float cost = executive_get_immune_adjusted_switch_cost(exec);

    // Expect cost around 400ms (200 * (1 + 0.5*2.0))
    EXPECT_GT(cost, 350.0F);
    EXPECT_LT(cost, 450.0F);
}

TEST_F(ExecutiveImmuneTest, SeverelyIncreasedSwitchCostWithHighInflammation) {
    // WHAT: Verify severe switch cost increase with high inflammation
    // WHY:  High cytokines cause perseveration
    // HOW:  Simulate 100% inflammation, expect 3x cost (600ms)

    simulate_inflammation(1.0F);

    float cost = executive_get_immune_adjusted_switch_cost(exec);

    // Expect cost around 600ms (200 * (1 + 1.0*2.0))
    EXPECT_FLOAT_EQ(cost, 600.0F);
}

//=============================================================================
// Inhibition Threshold Tests
//=============================================================================

TEST_F(ExecutiveImmuneTest, NormalInhibitionThresholdWithoutInflammation) {
    // WHAT: Verify normal inhibition threshold when no inflammation
    // WHY:  Baseline check for normal impulse control
    // HOW:  Query threshold, expect base value (0.7)

    float threshold = executive_get_immune_adjusted_inhibition(exec);
    EXPECT_FLOAT_EQ(threshold, 0.7F);
}

TEST_F(ExecutiveImmuneTest, ImpairedInhibitionWithInflammation) {
    // WHAT: Verify impaired inhibition (higher threshold) with inflammation
    // WHY:  Cytokines impair prefrontal inhibitory control
    // HOW:  Simulate 50% inflammation, expect threshold ~0.825

    simulate_inflammation(0.5F);

    float threshold = executive_get_immune_adjusted_inhibition(exec);

    // Expect threshold around 0.825 (0.7 + 0.5*0.25)
    EXPECT_GT(threshold, 0.8F);
    EXPECT_LT(threshold, 0.85F);
}

TEST_F(ExecutiveImmuneTest, SeverelyImpairedInhibitionWithHighInflammation) {
    // WHAT: Verify severely impaired inhibition with high inflammation
    // WHY:  High cytokines severely impair impulse control
    // HOW:  Simulate 100% inflammation, expect threshold 0.95

    simulate_inflammation(1.0F);

    float threshold = executive_get_immune_adjusted_inhibition(exec);

    // Expect threshold around 0.95 (0.7 + 1.0*0.25)
    EXPECT_FLOAT_EQ(threshold, 0.95F);
}

//=============================================================================
// Integration Disabled Tests
//=============================================================================

TEST_F(ExecutiveImmuneTest, NoEffectWhenImmuneIntegrationDisabled) {
    // WHAT: Verify no immune effects when integration disabled
    // WHY:  Ensure integration is optional
    // HOW:  Set immune system to NULL, check normal values

    executive_set_immune_system(exec, nullptr);
    simulate_inflammation(1.0F);  // Create inflammation but it won't affect exec

    float capacity = executive_get_immune_adjusted_capacity(exec);
    float cost = executive_get_immune_adjusted_switch_cost(exec);
    float threshold = executive_get_immune_adjusted_inhibition(exec);
    bool impaired = executive_is_immune_impaired(exec);

    EXPECT_FLOAT_EQ(capacity, 1.0F);
    EXPECT_FLOAT_EQ(cost, 200.0F);
    EXPECT_FLOAT_EQ(threshold, 0.7F);
    EXPECT_FALSE(impaired);
}

//=============================================================================
// Caching Tests
//=============================================================================

TEST_F(ExecutiveImmuneTest, InflammationLevelCached) {
    // WHAT: Verify inflammation level is cached for performance
    // WHY:  Avoid excessive immune system queries
    // HOW:  Query multiple times rapidly, check consistency

    simulate_inflammation(0.5F);

    float capacity1 = executive_get_immune_adjusted_capacity(exec);
    float capacity2 = executive_get_immune_adjusted_capacity(exec);
    float capacity3 = executive_get_immune_adjusted_capacity(exec);

    // All queries should return same value (cached)
    EXPECT_FLOAT_EQ(capacity1, capacity2);
    EXPECT_FLOAT_EQ(capacity2, capacity3);
}

//=============================================================================
// Boundary Condition Tests
//=============================================================================

TEST_F(ExecutiveImmuneTest, NullExecutiveController) {
    // WHAT: Verify graceful handling of NULL executive controller
    // WHY:  Guard against invalid inputs
    // HOW:  Pass NULL, expect safe defaults

    float capacity = executive_get_immune_adjusted_capacity(nullptr);
    float cost = executive_get_immune_adjusted_switch_cost(nullptr);
    float threshold = executive_get_immune_adjusted_inhibition(nullptr);
    bool impaired = executive_is_immune_impaired(nullptr);

    EXPECT_FLOAT_EQ(capacity, 1.0F);
    EXPECT_FLOAT_EQ(cost, 200.0F);  // DEFAULT_SWITCH_COST_MS
    EXPECT_FLOAT_EQ(threshold, 0.7F);  // DEFAULT_INHIBITION_THRESHOLD
    EXPECT_FALSE(impaired);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
