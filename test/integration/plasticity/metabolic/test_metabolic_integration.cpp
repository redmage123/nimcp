/**
 * @file test_metabolic_integration.cpp
 * @brief Integration tests for metabolic plasticity with sleep and immune bridges
 * @version 1.0.0
 * @date 2025-12-19
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "plasticity/metabolic/nimcp_metabolic_plasticity.h"
#include "plasticity/metabolic/nimcp_metabolic_sleep_bridge.h"
#include "plasticity/metabolic/nimcp_metabolic_immune_bridge.h"
#include "cognitive/nimcp_sleep_wake.h"
#include "cognitive/immune/nimcp_brain_immune.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class MetabolicSleepIntegrationTest : public ::testing::Test {
protected:
    metabolic_plasticity_t* metabolic;
    sleep_system_t sleep_system;
    metabolic_sleep_bridge_t bridge;

    void SetUp() override {
        // Create sleep system
        sleep_config_t sleep_config = sleep_default_config();
        sleep_system = sleep_system_create(&sleep_config);
        ASSERT_NE(sleep_system, nullptr);

        // Create metabolic system
        metabolic = metabolic_plasticity_create(nullptr);
        ASSERT_NE(metabolic, nullptr);

        // Create bridge
        bridge = metabolic_sleep_bridge_create(nullptr, sleep_system, metabolic);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) metabolic_sleep_bridge_destroy(bridge);
        if (metabolic) metabolic_plasticity_destroy(metabolic);
        if (sleep_system) sleep_system_destroy(sleep_system);
    }
};

class MetabolicImmuneIntegrationTest : public ::testing::Test {
protected:
    metabolic_plasticity_t* metabolic;
    brain_immune_system_t* immune;
    metabolic_immune_bridge_t* bridge;

    void SetUp() override {
        // Create immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr);

        // Create metabolic system
        metabolic = metabolic_plasticity_create(nullptr);
        ASSERT_NE(metabolic, nullptr);

        // Create bridge
        bridge = metabolic_immune_bridge_create(nullptr, immune, metabolic);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) metabolic_immune_bridge_destroy(bridge);
        if (metabolic) metabolic_plasticity_destroy(metabolic);
        if (immune) brain_immune_destroy(immune);
    }
};

/* ============================================================================
 * Sleep-Metabolic Integration Tests
 * ============================================================================ */

TEST_F(MetabolicSleepIntegrationTest, BridgeLifecycle) {
    // Tested in SetUp/TearDown
    SUCCEED();
}

TEST_F(MetabolicSleepIntegrationTest, InitialEffects) {
    metabolic_sleep_effects_t effects;
    int ret = metabolic_sleep_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);

    EXPECT_EQ(effects.current_state, SLEEP_STATE_AWAKE);
    EXPECT_FLOAT_EQ(effects.recovery_rate_factor, METABOLIC_SLEEP_RECOVERY_AWAKE);
}

TEST_F(MetabolicSleepIntegrationTest, DeepSleepEnhancesRecovery) {
    // Deplete ATP
    metabolic_plasticity_restore_atp(metabolic, 50.0f);

    // Transition to deep NREM
    sleep_enter_state(sleep_system, SLEEP_STATE_DEEP_NREM);
    metabolic_sleep_update(bridge);

    // Recovery should be enhanced
    metabolic_sleep_effects_t effects;
    metabolic_sleep_get_effects(bridge, &effects);

    EXPECT_FLOAT_EQ(effects.recovery_rate_factor, METABOLIC_SLEEP_RECOVERY_DEEP_NREM);
    EXPECT_GT(effects.recovery_rate_factor, 1.0f);
    EXPECT_TRUE(effects.deep_restoration_active);
}

TEST_F(MetabolicSleepIntegrationTest, REMStandardRecovery) {
    sleep_enter_state(sleep_system, SLEEP_STATE_REM);
    metabolic_sleep_update(bridge);

    metabolic_sleep_effects_t effects;
    metabolic_sleep_get_effects(bridge, &effects);

    EXPECT_FLOAT_EQ(effects.recovery_rate_factor, METABOLIC_SLEEP_RECOVERY_REM);
    EXPECT_FALSE(effects.deep_restoration_active);
}

TEST_F(MetabolicSleepIntegrationTest, SleepPressureFromATPDepletion) {
    // Full ATP: low sleep pressure
    metabolic_plasticity_restore_atp(metabolic, 100.0f);
    metabolic_sleep_update(bridge);
    float pressure1 = metabolic_sleep_get_sleep_pressure(bridge);
    EXPECT_LT(pressure1, 0.3f);

    // Depleted ATP: high sleep pressure
    metabolic_plasticity_restore_atp(metabolic, 30.0f);
    metabolic_sleep_update(bridge);
    float pressure2 = metabolic_sleep_get_sleep_pressure(bridge);
    EXPECT_GT(pressure2, 0.7f);
}

TEST_F(MetabolicSleepIntegrationTest, CriticalSleepNeed) {
    // Above critical threshold: no critical need
    metabolic_plasticity_restore_atp(metabolic, 50.0f);
    EXPECT_FALSE(metabolic_sleep_is_critical_need(bridge));

    // Below critical threshold: critical need
    metabolic_plasticity_restore_atp(metabolic, 15.0f);
    EXPECT_TRUE(metabolic_sleep_is_critical_need(bridge));
}

TEST_F(MetabolicSleepIntegrationTest, RecoveryRateModulation) {
    float base_rate = 3.0f;

    // Awake state
    sleep_enter_state(sleep_system, SLEEP_STATE_AWAKE);
    metabolic_sleep_update(bridge);
    float awake_rate = metabolic_sleep_get_recovery_rate(bridge, base_rate);
    EXPECT_FLOAT_EQ(awake_rate, base_rate * METABOLIC_SLEEP_RECOVERY_AWAKE);

    // Deep sleep state
    sleep_enter_state(sleep_system, SLEEP_STATE_DEEP_NREM);
    metabolic_sleep_update(bridge);
    float deep_rate = metabolic_sleep_get_recovery_rate(bridge, base_rate);
    EXPECT_FLOAT_EQ(deep_rate, base_rate * METABOLIC_SLEEP_RECOVERY_DEEP_NREM);
    EXPECT_GT(deep_rate, awake_rate);
}

TEST_F(MetabolicSleepIntegrationTest, CostFactorModulation) {
    // Awake: standard cost
    sleep_enter_state(sleep_system, SLEEP_STATE_AWAKE);
    metabolic_sleep_update(bridge);
    float awake_cost = metabolic_sleep_get_cost_factor(bridge);
    EXPECT_FLOAT_EQ(awake_cost, METABOLIC_SLEEP_COST_AWAKE);

    // Deep NREM: reduced cost
    sleep_enter_state(sleep_system, SLEEP_STATE_DEEP_NREM);
    metabolic_sleep_update(bridge);
    float deep_cost = metabolic_sleep_get_cost_factor(bridge);
    EXPECT_FLOAT_EQ(deep_cost, METABOLIC_SLEEP_COST_DEEP_NREM);
    EXPECT_LT(deep_cost, awake_cost);
}

/* ============================================================================
 * Immune-Metabolic Integration Tests
 * ============================================================================ */

TEST_F(MetabolicImmuneIntegrationTest, BridgeLifecycle) {
    // Tested in SetUp/TearDown
    SUCCEED();
}

TEST_F(MetabolicImmuneIntegrationTest, InitialState) {
    cytokine_metabolic_effects_t effects;
    int ret = metabolic_immune_get_cytokine_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);

    EXPECT_GT(effects.total_cost_multiplier, 0.0f);
}

TEST_F(MetabolicImmuneIntegrationTest, CytokineEffectsApplied) {
    int ret = metabolic_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(ret, 0);

    cytokine_metabolic_effects_t effects;
    metabolic_immune_get_cytokine_effects(bridge, &effects);

    EXPECT_FLOAT_EQ(effects.il1_burden, CYTOKINE_IL1_METABOLIC_BURDEN);
    EXPECT_FLOAT_EQ(effects.il6_burden, CYTOKINE_IL6_METABOLIC_BURDEN);
    EXPECT_FLOAT_EQ(effects.tnf_burden, CYTOKINE_TNF_METABOLIC_BURDEN);
}

TEST_F(MetabolicImmuneIntegrationTest, InflammationIncreasedCost) {
    float base_cost = 3.0f;

    // Apply inflammation effects (would normally query immune system)
    metabolic_immune_apply_inflammation_effects(bridge);

    // Get modulated cost
    float effective_cost = metabolic_immune_get_effective_cost(bridge, base_cost);

    // Cost should be at least base (or higher with inflammation)
    EXPECT_GE(effective_cost, base_cost);
}

TEST_F(MetabolicImmuneIntegrationTest, InflammationImpairedRecovery) {
    float base_rate = 3.0f;

    metabolic_immune_apply_inflammation_effects(bridge);

    float effective_rate = metabolic_immune_get_effective_recovery(bridge, base_rate);

    // Recovery should be base or lower (impaired by inflammation)
    EXPECT_LE(effective_rate, base_rate);
}

TEST_F(MetabolicImmuneIntegrationTest, ATPEffectsOnImmune) {
    // High ATP: full immune capacity
    metabolic_plasticity_restore_atp(metabolic, 80.0f);
    metabolic_immune_update_atp_effects(bridge);

    float capacity1 = metabolic_immune_get_immune_capacity(bridge);
    EXPECT_FLOAT_EQ(capacity1, 1.0f);

    // Low ATP: reduced immune capacity
    metabolic_plasticity_restore_atp(metabolic, 40.0f);
    metabolic_immune_update_atp_effects(bridge);

    float capacity2 = metabolic_immune_get_immune_capacity(bridge);
    EXPECT_LT(capacity2, 1.0f);
}

TEST_F(MetabolicImmuneIntegrationTest, ImmuneImpairedByATP) {
    // High ATP: not impaired
    metabolic_plasticity_restore_atp(metabolic, 70.0f);
    metabolic_immune_update_atp_effects(bridge);
    EXPECT_FALSE(metabolic_immune_is_impaired_by_atp(bridge));

    // Low ATP: impaired
    metabolic_plasticity_restore_atp(metabolic, 40.0f);
    metabolic_immune_update_atp_effects(bridge);
    EXPECT_TRUE(metabolic_immune_is_impaired_by_atp(bridge));
}

TEST_F(MetabolicImmuneIntegrationTest, BridgeUpdate) {
    int ret = metabolic_immune_bridge_update(bridge, 1000);
    EXPECT_EQ(ret, 0);

    // Should have updated all effects
    cytokine_metabolic_effects_t cytokine_effects;
    metabolic_immune_get_cytokine_effects(bridge, &cytokine_effects);
    EXPECT_GT(cytokine_effects.total_cost_multiplier, 0.0f);
}

TEST_F(MetabolicImmuneIntegrationTest, ATPEffectsQuery) {
    metabolic_plasticity_restore_atp(metabolic, 60.0f);
    metabolic_immune_update_atp_effects(bridge);

    atp_immune_effects_t effects;
    int ret = metabolic_immune_get_atp_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);

    EXPECT_FLOAT_EQ(effects.atp_level, 60.0f);
    EXPECT_GT(effects.immune_capacity, 0.0f);
}

/* ============================================================================
 * Cross-Bridge Integration Tests
 * ============================================================================ */

class MetabolicFullIntegrationTest : public ::testing::Test {
protected:
    metabolic_plasticity_t* metabolic;
    sleep_system_t sleep_system;
    brain_immune_system_t* immune;
    metabolic_sleep_bridge_t sleep_bridge;
    metabolic_immune_bridge_t* immune_bridge;

    void SetUp() override {
        // Create systems
        sleep_config_t sleep_config = sleep_default_config();
        sleep_system = sleep_system_create(&sleep_config);

        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);

        metabolic = metabolic_plasticity_create(nullptr);

        // Create bridges
        sleep_bridge = metabolic_sleep_bridge_create(nullptr, sleep_system, metabolic);
        immune_bridge = metabolic_immune_bridge_create(nullptr, immune, metabolic);

        ASSERT_NE(sleep_system, nullptr);
        ASSERT_NE(immune, nullptr);
        ASSERT_NE(metabolic, nullptr);
        ASSERT_NE(sleep_bridge, nullptr);
        ASSERT_NE(immune_bridge, nullptr);
    }

    void TearDown() override {
        if (sleep_bridge) metabolic_sleep_bridge_destroy(sleep_bridge);
        if (immune_bridge) metabolic_immune_bridge_destroy(immune_bridge);
        if (metabolic) metabolic_plasticity_destroy(metabolic);
        if (immune) brain_immune_destroy(immune);
        if (sleep_system) sleep_system_destroy(sleep_system);
    }
};

TEST_F(MetabolicFullIntegrationTest, SleepAndImmuneCoexist) {
    // Both bridges should work together
    metabolic_sleep_update(sleep_bridge);
    metabolic_immune_bridge_update(immune_bridge, 1000);

    SUCCEED();
}

TEST_F(MetabolicFullIntegrationTest, DeepSleepRestorationWithInflammation) {
    // Deplete ATP
    metabolic_plasticity_restore_atp(metabolic, 40.0f);

    // Apply inflammation (increases cost)
    metabolic_immune_apply_inflammation_effects(immune_bridge);

    // Enter deep sleep (enhances recovery)
    sleep_enter_state(sleep_system, SLEEP_STATE_DEEP_NREM);
    metabolic_sleep_update(sleep_bridge);

    // Recovery should be enhanced despite inflammation
    metabolic_sleep_effects_t sleep_effects;
    metabolic_sleep_get_effects(sleep_bridge, &sleep_effects);

    EXPECT_GT(sleep_effects.recovery_rate_factor, 1.0f);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
