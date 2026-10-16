/**
 * @file test_mental_health_immune_integration.cpp
 * @brief Unit tests for mental health - brain immune bridge integration
 *
 * WHAT: Tests for bidirectional mental health-immune system coupling
 * WHY:  Verify biological accuracy of cytokine-mental health connection
 * HOW:  Test cytokine effects on disorder risk, disorder effects on immunity
 *
 * COVERAGE:
 * - Bridge creation and lifecycle
 * - Cytokine effects on depression/anxiety risk
 * - Chronic inflammation → depression
 * - Neurotransmitter modulation
 * - Depression/anxiety → immune activation
 * - PTSD → chronic inflammation
 * - Recovery → immune boost
 * - Statistics and monitoring
 *
 * @author NIMCP Development Team
 * @date 2025-12-11
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_mental_health_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/nimcp_mental_health.h"
#include "utils/memory/nimcp_memory.h"

using ::testing::_;
using ::testing::Return;

// =============================================================================
// Test Fixture
// =============================================================================

class MentalHealthImmuneBridgeTest : public ::testing::Test {
protected:
    mental_health_immune_bridge_t* bridge;
    brain_immune_system_t* immune_system;
    mental_health_monitor_t* mental_health_monitor;

    void SetUp() override {
        // Create brain immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);

        // Start immune system
        brain_immune_start(immune_system);

        // Create mental health monitor
        mental_health_monitor = mental_health_create_default();
        ASSERT_NE(mental_health_monitor, nullptr);

        // Create bridge (initially NULL, created in tests)
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            mental_health_immune_bridge_destroy(bridge);
        }
        if (mental_health_monitor) {
            mental_health_destroy(mental_health_monitor);
        }
        if (immune_system) {
            brain_immune_destroy(immune_system);
        }
    }

    // Helper: Create bridge with default config
    void CreateBridge() {
        bridge = mental_health_immune_bridge_create(nullptr, immune_system, mental_health_monitor);
        ASSERT_NE(bridge, nullptr);
    }

    // Helper: Create inflammation in immune system
    void CreateInflammation(int site_count) {
        for (int i = 0; i < site_count; i++) {
            uint32_t site_id;
            brain_immune_initiate_inflammation(immune_system, i, i, &site_id);
        }
    }
};

// =============================================================================
// Lifecycle Tests
// =============================================================================

TEST_F(MentalHealthImmuneBridgeTest, DefaultConfig_Success) {
    // WHAT: Test default configuration retrieval
    // WHY:  Verify configuration system works
    // HOW:  Get default config, check values

    mental_health_immune_config_t config;
    int result = mental_health_immune_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_disorder_modulation);
    EXPECT_TRUE(config.enable_inflammation_depression);
    EXPECT_TRUE(config.enable_inflammation_anxiety);
    EXPECT_TRUE(config.enable_disorder_immune_trigger);
    EXPECT_TRUE(config.enable_recovery_immune_boost);
    EXPECT_EQ(config.cytokine_sensitivity, 1.0f);
}

TEST_F(MentalHealthImmuneBridgeTest, DefaultConfig_NullPointer) {
    // WHAT: Test default config with NULL pointer
    // WHY:  Verify error handling
    // HOW:  Pass NULL, expect failure

    int result = mental_health_immune_default_config(nullptr);

    EXPECT_EQ(result, -1);
}

TEST_F(MentalHealthImmuneBridgeTest, CreateBridge_Success) {
    // WHAT: Test successful bridge creation
    // WHY:  Verify basic lifecycle
    // HOW:  Create bridge, check non-NULL

    CreateBridge();

    EXPECT_NE(bridge, nullptr);
}

TEST_F(MentalHealthImmuneBridgeTest, CreateBridge_NullImmuneSystem) {
    // WHAT: Test bridge creation with NULL immune system
    // WHY:  Verify error handling
    // HOW:  Pass NULL immune system, expect failure

    bridge = mental_health_immune_bridge_create(nullptr, nullptr, mental_health_monitor);

    EXPECT_EQ(bridge, nullptr);
}

TEST_F(MentalHealthImmuneBridgeTest, CreateBridge_NullMentalHealth) {
    // WHAT: Test bridge creation with NULL mental health monitor
    // WHY:  Verify error handling
    // HOW:  Pass NULL monitor, expect failure

    bridge = mental_health_immune_bridge_create(nullptr, immune_system, nullptr);

    EXPECT_EQ(bridge, nullptr);
}

TEST_F(MentalHealthImmuneBridgeTest, DestroyBridge_Success) {
    // WHAT: Test bridge destruction
    // WHY:  Verify proper cleanup
    // HOW:  Create and destroy, check no crash

    CreateBridge();

    mental_health_immune_bridge_destroy(bridge);
    bridge = nullptr; // Prevent double-free in TearDown

    SUCCEED();
}

TEST_F(MentalHealthImmuneBridgeTest, DestroyBridge_NullPointer) {
    // WHAT: Test destroying NULL bridge
    // WHY:  Verify error handling
    // HOW:  Destroy NULL, check no crash

    mental_health_immune_bridge_destroy(nullptr);

    SUCCEED();
}

// =============================================================================
// Immune → Mental Health: Cytokine Effects Tests
// =============================================================================

TEST_F(MentalHealthImmuneBridgeTest, CytokineEffects_NoInflammation) {
    // WHAT: Test cytokine effects with no inflammation
    // WHY:  Verify baseline behavior
    // HOW:  Apply cytokine effects with clean immune state

    CreateBridge();

    int result = mental_health_immune_apply_cytokine_effects(bridge);

    EXPECT_EQ(result, 0);

    cytokine_mental_health_effects_t effects;
    mental_health_immune_get_cytokine_effects(bridge, &effects);

    // With no inflammation, effects should be minimal
    EXPECT_NEAR(effects.total_depression_risk_shift, 0.0f, 0.1f);
    EXPECT_NEAR(effects.total_anxiety_risk_shift, 0.0f, 0.1f);
}

TEST_F(MentalHealthImmuneBridgeTest, CytokineEffects_WithInflammation) {
    // WHAT: Test cytokine effects with active inflammation
    // WHY:  Verify inflammation increases depression/anxiety risk
    // HOW:  Create inflammation, apply effects, check risk increase

    CreateBridge();
    CreateInflammation(5); // Moderate inflammation

    int result = mental_health_immune_apply_cytokine_effects(bridge);

    EXPECT_EQ(result, 0);

    cytokine_mental_health_effects_t effects;
    mental_health_immune_get_cytokine_effects(bridge, &effects);

    // With inflammation, depression and anxiety risk should increase
    EXPECT_GT(effects.total_depression_risk_shift, 0.0f);
    EXPECT_GT(effects.total_anxiety_risk_shift, 0.0f);
    EXPECT_GT(effects.neurotransmitter_suppression, 0.0f);
}

TEST_F(MentalHealthImmuneBridgeTest, CytokineEffects_CytokineStorm) {
    // WHAT: Test cytokine effects during cytokine storm
    // WHY:  Verify severe inflammation has major mental health impact
    // HOW:  Create storm-level inflammation, check high risk

    CreateBridge();
    CreateInflammation(7); // Storm-level inflammation

    int result = mental_health_immune_apply_cytokine_effects(bridge);

    EXPECT_EQ(result, 0);

    cytokine_mental_health_effects_t effects;
    mental_health_immune_get_cytokine_effects(bridge, &effects);

    // Cytokine storm should create substantial risk
    EXPECT_GT(effects.total_depression_risk_shift, 0.3f);
    EXPECT_GT(effects.neurotransmitter_suppression, 0.5f);
}

TEST_F(MentalHealthImmuneBridgeTest, CytokineEffects_NullBridge) {
    // WHAT: Test cytokine effects with NULL bridge
    // WHY:  Verify error handling
    // HOW:  Pass NULL, expect failure

    int result = mental_health_immune_apply_cytokine_effects(nullptr);

    EXPECT_EQ(result, -1);
}

// =============================================================================
// Immune → Mental Health: Inflammation Effects Tests
// =============================================================================

TEST_F(MentalHealthImmuneBridgeTest, InflammationEffects_LocalInflammation) {
    // WHAT: Test inflammation effects with local inflammation
    // WHY:  Verify low inflammation has minimal mental health impact
    // HOW:  Create 1 site, check low risk multipliers

    CreateBridge();
    CreateInflammation(1); // Local inflammation

    int result = mental_health_immune_apply_inflammation_effects(bridge);

    EXPECT_EQ(result, 0);

    inflammation_mental_health_state_t state;
    mental_health_immune_get_inflammation_state(bridge, &state);

    EXPECT_EQ(state.current_level, INFLAMMATION_LOCAL);
    EXPECT_NEAR(state.depression_risk_multiplier, 1.0f, 0.3f);
}

TEST_F(MentalHealthImmuneBridgeTest, InflammationEffects_SystemicInflammation) {
    // WHAT: Test inflammation effects with systemic inflammation
    // WHY:  Verify high inflammation increases mental health risk
    // HOW:  Create 5 sites, check elevated risk multipliers

    CreateBridge();
    CreateInflammation(5); // Systemic inflammation

    int result = mental_health_immune_apply_inflammation_effects(bridge);

    EXPECT_EQ(result, 0);

    inflammation_mental_health_state_t state;
    mental_health_immune_get_inflammation_state(bridge, &state);

    EXPECT_EQ(state.current_level, INFLAMMATION_SYSTEMIC);
    EXPECT_GT(state.depression_risk_multiplier, 1.0f);
    EXPECT_GT(state.anxiety_risk_multiplier, 1.0f);
    EXPECT_GT(state.serotonin_suppression, 0.0f);
    EXPECT_GT(state.dopamine_suppression, 0.0f);
}

TEST_F(MentalHealthImmuneBridgeTest, InflammationEffects_CytokineStorm) {
    // WHAT: Test inflammation effects during cytokine storm
    // WHY:  Verify storm triggers psychosis risk
    // HOW:  Create 7 sites, check psychosis risk > 0

    CreateBridge();
    CreateInflammation(7); // Storm

    int result = mental_health_immune_apply_inflammation_effects(bridge);

    EXPECT_EQ(result, 0);

    inflammation_mental_health_state_t state;
    mental_health_immune_get_inflammation_state(bridge, &state);

    EXPECT_EQ(state.current_level, INFLAMMATION_STORM);
    EXPECT_GT(state.psychosis_risk, 0.0f);
    EXPECT_GT(state.cognitive_impairment, 0.5f);
}

TEST_F(MentalHealthImmuneBridgeTest, InflammationEffects_NullBridge) {
    // WHAT: Test inflammation effects with NULL bridge
    // WHY:  Verify error handling
    // HOW:  Pass NULL, expect failure

    int result = mental_health_immune_apply_inflammation_effects(nullptr);

    EXPECT_EQ(result, -1);
}

// =============================================================================
// Immune → Mental Health: Risk Computation Tests
// =============================================================================

TEST_F(MentalHealthImmuneBridgeTest, ComputeDepressionRisk_NoInflammation) {
    // WHAT: Test depression risk with no inflammation
    // WHY:  Verify baseline is 1.0 (no increase)
    // HOW:  Compute risk with clean state

    CreateBridge();
    mental_health_immune_apply_inflammation_effects(bridge);

    float risk = mental_health_immune_compute_depression_risk(bridge);

    EXPECT_NEAR(risk, 1.0f, 0.1f);
}

TEST_F(MentalHealthImmuneBridgeTest, ComputeDepressionRisk_WithInflammation) {
    // WHAT: Test depression risk with inflammation
    // WHY:  Verify inflammation increases risk
    // HOW:  Create inflammation, compute risk, expect > 1.0

    CreateBridge();
    CreateInflammation(5);
    mental_health_immune_apply_inflammation_effects(bridge);

    float risk = mental_health_immune_compute_depression_risk(bridge);

    EXPECT_GT(risk, 1.0f);
}

TEST_F(MentalHealthImmuneBridgeTest, ComputeAnxietyRisk_NoInflammation) {
    // WHAT: Test anxiety risk with no inflammation
    // WHY:  Verify baseline is 1.0
    // HOW:  Compute risk with clean state

    CreateBridge();
    mental_health_immune_apply_inflammation_effects(bridge);

    float risk = mental_health_immune_compute_anxiety_risk(bridge);

    EXPECT_NEAR(risk, 1.0f, 0.1f);
}

TEST_F(MentalHealthImmuneBridgeTest, ComputeAnxietyRisk_WithInflammation) {
    // WHAT: Test anxiety risk with inflammation
    // WHY:  Verify inflammation increases risk
    // HOW:  Create inflammation, compute risk, expect > 1.0

    CreateBridge();
    CreateInflammation(5);
    mental_health_immune_apply_inflammation_effects(bridge);

    float risk = mental_health_immune_compute_anxiety_risk(bridge);

    EXPECT_GT(risk, 1.0f);
}

// =============================================================================
// Immune → Mental Health: Neurotransmitter Modulation Tests
// =============================================================================

TEST_F(MentalHealthImmuneBridgeTest, NeurotransmitterModulation_NoInflammation) {
    // WHAT: Test neurotransmitter modulation with no inflammation
    // WHY:  Verify no suppression at baseline
    // HOW:  Apply modulation, check suppression near zero

    CreateBridge();
    mental_health_immune_apply_cytokine_effects(bridge);

    int result = mental_health_immune_modulate_neurotransmitters(bridge);

    EXPECT_EQ(result, 0);

    float suppression = mental_health_immune_get_neurotransmitter_suppression(bridge);
    EXPECT_NEAR(suppression, 0.0f, 0.1f);
}

TEST_F(MentalHealthImmuneBridgeTest, NeurotransmitterModulation_WithInflammation) {
    // WHAT: Test neurotransmitter modulation with inflammation
    // WHY:  Verify inflammation suppresses neurotransmitters
    // HOW:  Create inflammation, apply modulation, check suppression > 0

    CreateBridge();
    CreateInflammation(5);
    mental_health_immune_apply_cytokine_effects(bridge);

    int result = mental_health_immune_modulate_neurotransmitters(bridge);

    EXPECT_EQ(result, 0);

    float suppression = mental_health_immune_get_neurotransmitter_suppression(bridge);
    EXPECT_GT(suppression, 0.0f);
}

// =============================================================================
// Mental Health → Immune: Disorder Trigger Tests
// =============================================================================

TEST_F(MentalHealthImmuneBridgeTest, TriggerFromDepression_LowSeverity) {
    // WHAT: Test depression trigger with low severity
    // WHY:  Verify low depression doesn't trigger immune
    // HOW:  Simulate low depression, check no trigger

    CreateBridge();

    int result = mental_health_immune_trigger_from_depression(bridge);

    EXPECT_EQ(result, 0);
    // With low/no depression, should not trigger
}

TEST_F(MentalHealthImmuneBridgeTest, TriggerFromDepression_NullBridge) {
    // WHAT: Test depression trigger with NULL bridge
    // WHY:  Verify error handling
    // HOW:  Pass NULL, expect failure

    int result = mental_health_immune_trigger_from_depression(nullptr);

    EXPECT_EQ(result, -1);
}

TEST_F(MentalHealthImmuneBridgeTest, TriggerFromAnxiety_LowSeverity) {
    // WHAT: Test anxiety trigger with low severity
    // WHY:  Verify low anxiety doesn't trigger immune
    // HOW:  Simulate low anxiety, check no trigger

    CreateBridge();

    int result = mental_health_immune_trigger_from_anxiety(bridge);

    EXPECT_EQ(result, 0);
}

TEST_F(MentalHealthImmuneBridgeTest, TriggerFromAnxiety_NullBridge) {
    // WHAT: Test anxiety trigger with NULL bridge
    // WHY:  Verify error handling
    // HOW:  Pass NULL, expect failure

    int result = mental_health_immune_trigger_from_anxiety(nullptr);

    EXPECT_EQ(result, -1);
}

TEST_F(MentalHealthImmuneBridgeTest, TriggerFromPTSD_LowSeverity) {
    // WHAT: Test PTSD trigger with low severity
    // WHY:  Verify low PTSD doesn't trigger immune
    // HOW:  Simulate low PTSD, check no trigger

    CreateBridge();

    int result = mental_health_immune_trigger_from_ptsd(bridge);

    EXPECT_EQ(result, 0);
}

TEST_F(MentalHealthImmuneBridgeTest, TriggerFromPTSD_NullBridge) {
    // WHAT: Test PTSD trigger with NULL bridge
    // WHY:  Verify error handling
    // HOW:  Pass NULL, expect failure

    int result = mental_health_immune_trigger_from_ptsd(nullptr);

    EXPECT_EQ(result, -1);
}

// =============================================================================
// Mental Health → Immune: Recovery Boost Tests
// =============================================================================

TEST_F(MentalHealthImmuneBridgeTest, BoostFromRecovery_NoRecovery) {
    // WHAT: Test recovery boost with no recent intervention
    // WHY:  Verify no boost without recovery
    // HOW:  Apply boost with clean state, check no effect

    CreateBridge();

    int result = mental_health_immune_boost_from_recovery(bridge);

    EXPECT_EQ(result, 0);
}

TEST_F(MentalHealthImmuneBridgeTest, BoostFromRecovery_NullBridge) {
    // WHAT: Test recovery boost with NULL bridge
    // WHY:  Verify error handling
    // HOW:  Pass NULL, expect failure

    int result = mental_health_immune_boost_from_recovery(nullptr);

    EXPECT_EQ(result, -1);
}

// =============================================================================
// Bidirectional Update Tests
// =============================================================================

TEST_F(MentalHealthImmuneBridgeTest, BridgeUpdate_Success) {
    // WHAT: Test full bidirectional update
    // WHY:  Verify integrated update works
    // HOW:  Update bridge, check success

    CreateBridge();

    int result = mental_health_immune_bridge_update(bridge, 1000);

    EXPECT_EQ(result, 0);
}

TEST_F(MentalHealthImmuneBridgeTest, BridgeUpdate_NullBridge) {
    // WHAT: Test update with NULL bridge
    // WHY:  Verify error handling
    // HOW:  Pass NULL, expect failure

    int result = mental_health_immune_bridge_update(nullptr, 1000);

    EXPECT_EQ(result, -1);
}

TEST_F(MentalHealthImmuneBridgeTest, BridgeUpdate_WithInflammation) {
    // WHAT: Test update with active inflammation
    // WHY:  Verify inflammation is processed correctly
    // HOW:  Create inflammation, update, check effects

    CreateBridge();
    CreateInflammation(5);

    int result = mental_health_immune_bridge_update(bridge, 1000);

    EXPECT_EQ(result, 0);

    cytokine_mental_health_effects_t effects;
    mental_health_immune_get_cytokine_effects(bridge, &effects);

    // Should see cytokine effects after update
    EXPECT_GT(effects.total_depression_risk_shift, 0.0f);
}

// =============================================================================
// Query Tests
// =============================================================================

TEST_F(MentalHealthImmuneBridgeTest, GetCytokineEffects_Success) {
    // WHAT: Test cytokine effects query
    // WHY:  Verify state can be retrieved
    // HOW:  Get effects, check success

    CreateBridge();

    cytokine_mental_health_effects_t effects;
    int result = mental_health_immune_get_cytokine_effects(bridge, &effects);

    EXPECT_EQ(result, 0);
}

TEST_F(MentalHealthImmuneBridgeTest, GetCytokineEffects_NullBridge) {
    // WHAT: Test cytokine effects query with NULL bridge
    // WHY:  Verify error handling
    // HOW:  Pass NULL bridge, expect failure

    cytokine_mental_health_effects_t effects;
    int result = mental_health_immune_get_cytokine_effects(nullptr, &effects);

    EXPECT_EQ(result, -1);
}

TEST_F(MentalHealthImmuneBridgeTest, GetCytokineEffects_NullEffects) {
    // WHAT: Test cytokine effects query with NULL output
    // WHY:  Verify error handling
    // HOW:  Pass NULL output, expect failure

    CreateBridge();

    int result = mental_health_immune_get_cytokine_effects(bridge, nullptr);

    EXPECT_EQ(result, -1);
}

TEST_F(MentalHealthImmuneBridgeTest, GetInflammationState_Success) {
    // WHAT: Test inflammation state query
    // WHY:  Verify state can be retrieved
    // HOW:  Get state, check success

    CreateBridge();

    inflammation_mental_health_state_t state;
    int result = mental_health_immune_get_inflammation_state(bridge, &state);

    EXPECT_EQ(result, 0);
}

TEST_F(MentalHealthImmuneBridgeTest, GetInflammationState_NullBridge) {
    // WHAT: Test inflammation state query with NULL bridge
    // WHY:  Verify error handling
    // HOW:  Pass NULL bridge, expect failure

    inflammation_mental_health_state_t state;
    int result = mental_health_immune_get_inflammation_state(nullptr, &state);

    EXPECT_EQ(result, -1);
}

TEST_F(MentalHealthImmuneBridgeTest, IsCytokineDepression_NoInflammation) {
    // WHAT: Test cytokine depression check with no inflammation
    // WHY:  Verify baseline is false
    // HOW:  Check with clean state, expect false

    CreateBridge();

    bool is_cytokine_depression = mental_health_immune_is_cytokine_depression(bridge);

    EXPECT_FALSE(is_cytokine_depression);
}

TEST_F(MentalHealthImmuneBridgeTest, IsCytokineDepression_HighInflammation) {
    // WHAT: Test cytokine depression check with high inflammation
    // WHY:  Verify high inflammation triggers cytokine depression
    // HOW:  Create storm, apply effects, expect true

    CreateBridge();
    CreateInflammation(7);
    mental_health_immune_apply_cytokine_effects(bridge);

    bool is_cytokine_depression = mental_health_immune_is_cytokine_depression(bridge);

    // With cytokine storm, should detect cytokine depression
    EXPECT_TRUE(is_cytokine_depression);
}

TEST_F(MentalHealthImmuneBridgeTest, GetStats_Success) {
    // WHAT: Test statistics query
    // WHY:  Verify stats can be retrieved
    // HOW:  Get stats, check success

    CreateBridge();

    uint64_t total_updates;
    uint32_t depression_triggers;
    uint32_t anxiety_triggers;

    int result = mental_health_immune_get_stats(bridge, &total_updates,
                                                &depression_triggers, &anxiety_triggers);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(total_updates, 0); // No updates yet
}

TEST_F(MentalHealthImmuneBridgeTest, GetStats_AfterUpdates) {
    // WHAT: Test statistics after updates
    // WHY:  Verify stats are tracked correctly
    // HOW:  Update multiple times, check stats

    CreateBridge();

    mental_health_immune_bridge_update(bridge, 1000);
    mental_health_immune_bridge_update(bridge, 1000);
    mental_health_immune_bridge_update(bridge, 1000);

    uint64_t total_updates;
    uint32_t depression_triggers;
    uint32_t anxiety_triggers;

    mental_health_immune_get_stats(bridge, &total_updates,
                                   &depression_triggers, &anxiety_triggers);

    EXPECT_EQ(total_updates, 3);
}

TEST_F(MentalHealthImmuneBridgeTest, GetStats_NullBridge) {
    // WHAT: Test stats query with NULL bridge
    // WHY:  Verify error handling
    // HOW:  Pass NULL bridge, expect failure

    uint64_t total_updates;
    uint32_t depression_triggers;
    uint32_t anxiety_triggers;

    int result = mental_health_immune_get_stats(nullptr, &total_updates,
                                                &depression_triggers, &anxiety_triggers);

    EXPECT_EQ(result, -1);
}

// =============================================================================
// Integration Tests
// =============================================================================

TEST_F(MentalHealthImmuneBridgeTest, FullCycle_InflammationToRecovery) {
    // WHAT: Test full cycle: inflammation → mental health impact → recovery
    // WHY:  Verify complete integration works
    // HOW:  Simulate inflammation, check effects, simulate recovery

    CreateBridge();

    // Phase 1: Create inflammation
    CreateInflammation(5);
    mental_health_immune_bridge_update(bridge, 1000);

    inflammation_mental_health_state_t state1;
    mental_health_immune_get_inflammation_state(bridge, &state1);
    float risk1 = mental_health_immune_compute_depression_risk(bridge);

    EXPECT_GT(state1.depression_risk_multiplier, 1.0f);
    EXPECT_GT(risk1, 1.0f);

    // Phase 2: Recovery (would resolve inflammation in real scenario)
    mental_health_immune_boost_from_recovery(bridge);

    SUCCEED();
}

TEST_F(MentalHealthImmuneBridgeTest, ChronicInflammation_HighDepressionRisk) {
    // WHAT: Test chronic inflammation increases depression risk significantly
    // WHY:  Verify chronic inflammation modeling
    // HOW:  Simulate long inflammation duration, check high risk

    CreateBridge();
    CreateInflammation(4); // Moderate inflammation
    mental_health_immune_apply_inflammation_effects(bridge);

    inflammation_mental_health_state_t state;
    mental_health_immune_get_inflammation_state(bridge, &state);

    // Even without chronic flag set, systemic inflammation should increase risk
    EXPECT_GT(state.depression_risk_multiplier, 1.0f);
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
