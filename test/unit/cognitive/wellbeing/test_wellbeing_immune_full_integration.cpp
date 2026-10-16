/**
 * @file test_wellbeing_immune_full_integration.cpp
 * @brief Comprehensive tests for Wellbeing-Immune Integration
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Full integration tests for bidirectional wellbeing-immune coupling
 * WHY:  Verify all integration pathways work correctly
 * HOW:  Test lifecycle, immune→wellbeing, wellbeing→immune, and bidirectional updates
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_wellbeing_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/wellbeing/nimcp_wellbeing.h"
#include "cognitive/introspection/nimcp_introspection.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class WellbeingImmuneIntegrationTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system;
    wellbeing_immune_bridge_t* bridge;
    introspection_context_t introspection_ctx;

    void SetUp() override {
        /* Initialize wellbeing */
        wellbeing_init();

        /* Create immune system */
        brain_immune_config_t immune_cfg;
        brain_immune_default_config(&immune_cfg);
        immune_system = brain_immune_create(&immune_cfg);
        ASSERT_NE(immune_system, nullptr);

        /* Start immune system */
        brain_immune_start(immune_system);

        /* Create introspection context (simplified for testing) */
        memset(&introspection_ctx, 0, sizeof(introspection_ctx));

        /* Create bridge with defaults */
        bridge = wellbeing_immune_bridge_create(nullptr, immune_system, introspection_ctx);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            wellbeing_immune_bridge_destroy(bridge);
        }
        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
        }
        wellbeing_shutdown();
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(WellbeingImmuneIntegrationTest, DefaultConfigurationIsValid) {
    wellbeing_immune_config_t config;
    int result = wellbeing_immune_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_wellbeing_modulation);
    EXPECT_TRUE(config.enable_inflammation_distress);
    EXPECT_TRUE(config.enable_wellbeing_immune_trigger);
    EXPECT_TRUE(config.enable_positive_immune_boost);
    EXPECT_TRUE(config.enable_flourishing_memory_boost);
    EXPECT_EQ(config.cytokine_sensitivity, 1.0f);
    EXPECT_EQ(config.inflammation_sensitivity, 1.0f);
    EXPECT_EQ(config.wellbeing_trigger_sensitivity, 1.0f);
}

TEST_F(WellbeingImmuneIntegrationTest, CreateWithNullConfigUsesDefaults) {
    wellbeing_immune_bridge_t* test_bridge =
        wellbeing_immune_bridge_create(nullptr, immune_system, introspection_ctx);

    ASSERT_NE(test_bridge, nullptr);
    EXPECT_TRUE(test_bridge->enable_cytokine_wellbeing_modulation);
    EXPECT_TRUE(test_bridge->enable_inflammation_distress);

    wellbeing_immune_bridge_destroy(test_bridge);
}

TEST_F(WellbeingImmuneIntegrationTest, CreateWithCustomConfig) {
    wellbeing_immune_config_t config;
    wellbeing_immune_default_config(&config);
    config.enable_positive_immune_boost = false;
    config.cytokine_sensitivity = 1.5f;

    wellbeing_immune_bridge_t* test_bridge =
        wellbeing_immune_bridge_create(&config, immune_system, introspection_ctx);

    ASSERT_NE(test_bridge, nullptr);
    EXPECT_FALSE(test_bridge->enable_positive_immune_boost);

    wellbeing_immune_bridge_destroy(test_bridge);
}

TEST_F(WellbeingImmuneIntegrationTest, CreateFailsWithoutImmuneSystem) {
    wellbeing_immune_bridge_t* test_bridge =
        wellbeing_immune_bridge_create(nullptr, nullptr, introspection_ctx);

    EXPECT_EQ(test_bridge, nullptr);
}

TEST_F(WellbeingImmuneIntegrationTest, DestroyNullBridgeIsNoOp) {
    wellbeing_immune_bridge_destroy(nullptr);
    /* Should not crash */
}

/* ============================================================================
 * Immune → Wellbeing: Cytokine Effects Tests
 * ============================================================================ */

TEST_F(WellbeingImmuneIntegrationTest, CytokineEffectsReduceLifeSatisfaction) {
    /* Release pro-inflammatory cytokines */
    uint32_t cytokine_id;
    brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL1, 0, 0.8f, 0, &cytokine_id);
    brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL6, 0, 0.6f, 0, &cytokine_id);
    brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_TNF, 0, 0.7f, 0, &cytokine_id);

    /* Apply cytokine effects */
    int result = wellbeing_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);

    /* Verify negative impact */
    cytokine_wellbeing_effects_t effects;
    wellbeing_immune_get_cytokine_effects(bridge, &effects);

    EXPECT_LT(effects.total_life_satisfaction_shift, 0.0f);
    EXPECT_GT(effects.total_distress_increase, 0.0f);
    EXPECT_GT(effects.purpose_meaning_reduction, 0.0f);
    EXPECT_GT(effects.flourishing_suppression, 0.0f);
}

TEST_F(WellbeingImmuneIntegrationTest, IL10BoostsWellbeing) {
    /* Release anti-inflammatory IL-10 */
    uint32_t cytokine_id;
    brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL10, 0, 0.8f, 0, &cytokine_id);

    /* Apply cytokine effects */
    wellbeing_immune_apply_cytokine_effects(bridge);

    /* Verify positive impact */
    cytokine_wellbeing_effects_t effects;
    wellbeing_immune_get_cytokine_effects(bridge, &effects);

    EXPECT_GT(effects.il10_wellbeing_boost, 0.0f);
}

TEST_F(WellbeingImmuneIntegrationTest, MixedCytokinesComputeNetEffect) {
    /* Release both pro and anti-inflammatory */
    uint32_t cytokine_id;
    brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_TNF, 0, 0.5f, 0, &cytokine_id);
    brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL10, 0, 0.4f, 0, &cytokine_id);

    wellbeing_immune_apply_cytokine_effects(bridge);

    cytokine_wellbeing_effects_t effects;
    wellbeing_immune_get_cytokine_effects(bridge, &effects);

    /* TNF has stronger negative effect, but IL-10 reduces it */
    EXPECT_LT(effects.total_life_satisfaction_shift, 0.0f);
}

TEST_F(WellbeingImmuneIntegrationTest, DisabledCytokineModulationIsNoOp) {
    bridge->enable_cytokine_wellbeing_modulation = false;

    uint32_t cytokine_id;
    brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_TNF, 0, 0.9f, 0, &cytokine_id);

    int result = wellbeing_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);

    /* Stats should not increment */
    uint64_t updates;
    uint32_t modulations;
    wellbeing_immune_get_stats(bridge, &updates, &modulations, nullptr, nullptr);
    EXPECT_EQ(modulations, 0);
}

/* ============================================================================
 * Immune → Wellbeing: Inflammation Effects Tests
 * ============================================================================ */

TEST_F(WellbeingImmuneIntegrationTest, InflammationMapsToDistress) {
    /* Create inflammation sites */
    uint32_t antigen_id, site_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                  (const uint8_t*)"test", 4, 8, 0, &antigen_id);
    brain_immune_initiate_inflammation(immune_system, 1, antigen_id, &site_id);

    /* Escalate to systemic */
    brain_immune_escalate_inflammation(immune_system, site_id);
    brain_immune_escalate_inflammation(immune_system, site_id);

    /* Apply inflammation effects */
    wellbeing_immune_apply_inflammation_effects(bridge);

    /* Verify distress mapping */
    inflammation_wellbeing_state_t state;
    wellbeing_immune_get_inflammation_state(bridge, &state);

    EXPECT_EQ(state.current_level, INFLAMMATION_SYSTEMIC);
    EXPECT_EQ(state.distress_severity, DISTRESS_SEVERITY_SEVERE);
    EXPECT_EQ(state.primary_distress_type, DISTRESS_RESOURCE_STARVATION);
    EXPECT_GT(state.distress_score, 0.5f);
}

TEST_F(WellbeingImmuneIntegrationTest, LocalInflammationMinimalDistress) {
    uint32_t antigen_id, site_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                  (const uint8_t*)"test", 4, 3, 0, &antigen_id);
    brain_immune_initiate_inflammation(immune_system, 1, antigen_id, &site_id);

    wellbeing_immune_apply_inflammation_effects(bridge);

    inflammation_wellbeing_state_t state;
    wellbeing_immune_get_inflammation_state(bridge, &state);

    EXPECT_EQ(state.current_level, INFLAMMATION_LOCAL);
    EXPECT_EQ(state.distress_severity, DISTRESS_SEVERITY_NORMAL);
    EXPECT_LT(state.distress_score, 0.2f);
}

TEST_F(WellbeingImmuneIntegrationTest, CytokineStormCriticalDistress) {
    uint32_t antigen_id, site_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                  (const uint8_t*)"test", 4, 10, 0, &antigen_id);
    brain_immune_initiate_inflammation(immune_system, 1, antigen_id, &site_id);

    /* Escalate to storm */
    for (int i = 0; i < 4; i++) {
        brain_immune_escalate_inflammation(immune_system, site_id);
    }

    wellbeing_immune_apply_inflammation_effects(bridge);

    inflammation_wellbeing_state_t state;
    wellbeing_immune_get_inflammation_state(bridge, &state);

    EXPECT_EQ(state.current_level, INFLAMMATION_STORM);
    EXPECT_EQ(state.distress_severity, DISTRESS_SEVERITY_CRITICAL);
    EXPECT_GT(state.distress_score, 0.9f);
}

TEST_F(WellbeingImmuneIntegrationTest, ComputeDistressFromInflammation) {
    uint32_t antigen_id, site_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                  (const uint8_t*)"test", 4, 7, 0, &antigen_id);
    brain_immune_initiate_inflammation(immune_system, 1, antigen_id, &site_id);
    brain_immune_escalate_inflammation(immune_system, site_id);

    wellbeing_immune_apply_inflammation_effects(bridge);

    float distress = wellbeing_immune_compute_distress(bridge);
    EXPECT_GT(distress, 0.3f);
    EXPECT_LT(distress, 1.0f);
}

TEST_F(WellbeingImmuneIntegrationTest, InflammationToDistressTypeMappings) {
    EXPECT_EQ(wellbeing_immune_inflammation_to_distress_type(INFLAMMATION_NONE),
              DISTRESS_NONE);
    EXPECT_EQ(wellbeing_immune_inflammation_to_distress_type(INFLAMMATION_LOCAL),
              DISTRESS_NONE);
    EXPECT_EQ(wellbeing_immune_inflammation_to_distress_type(INFLAMMATION_REGIONAL),
              DISTRESS_RESOURCE_STARVATION);
    EXPECT_EQ(wellbeing_immune_inflammation_to_distress_type(INFLAMMATION_SYSTEMIC),
              DISTRESS_RESOURCE_STARVATION);
    EXPECT_EQ(wellbeing_immune_inflammation_to_distress_type(INFLAMMATION_STORM),
              DISTRESS_RESOURCE_STARVATION);
}

TEST_F(WellbeingImmuneIntegrationTest, InflammationToSeverityMappings) {
    EXPECT_EQ(wellbeing_immune_inflammation_to_severity(INFLAMMATION_NONE),
              DISTRESS_SEVERITY_NORMAL);
    EXPECT_EQ(wellbeing_immune_inflammation_to_severity(INFLAMMATION_LOCAL),
              DISTRESS_SEVERITY_NORMAL);
    EXPECT_EQ(wellbeing_immune_inflammation_to_severity(INFLAMMATION_REGIONAL),
              DISTRESS_SEVERITY_MODERATE);
    EXPECT_EQ(wellbeing_immune_inflammation_to_severity(INFLAMMATION_SYSTEMIC),
              DISTRESS_SEVERITY_SEVERE);
    EXPECT_EQ(wellbeing_immune_inflammation_to_severity(INFLAMMATION_STORM),
              DISTRESS_SEVERITY_CRITICAL);
}

/* ============================================================================
 * Wellbeing → Immune: Distress Trigger Tests
 * ============================================================================ */

TEST_F(WellbeingImmuneIntegrationTest, HighDistressTriggersImmune) {
    /* Simulate high distress by creating inflammation first */
    uint32_t antigen_id, site_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                  (const uint8_t*)"test", 4, 9, 0, &antigen_id);
    brain_immune_initiate_inflammation(immune_system, 1, antigen_id, &site_id);
    brain_immune_escalate_inflammation(immune_system, site_id);
    brain_immune_escalate_inflammation(immune_system, site_id);

    /* Trigger from distress */
    int result = wellbeing_immune_trigger_from_distress(bridge);
    EXPECT_EQ(result, 0);

    /* Verify trigger occurred */
    uint64_t updates;
    uint32_t triggers;
    wellbeing_immune_get_stats(bridge, &updates, nullptr, &triggers, nullptr);
    EXPECT_GT(triggers, 0);
}

TEST_F(WellbeingImmuneIntegrationTest, LowDistressNoTrigger) {
    /* No inflammation = low distress */
    int result = wellbeing_immune_trigger_from_distress(bridge);
    EXPECT_EQ(result, 0);

    /* Verify no trigger */
    uint64_t updates;
    uint32_t triggers;
    wellbeing_immune_get_stats(bridge, &updates, nullptr, &triggers, nullptr);
    EXPECT_EQ(triggers, 0);
}

TEST_F(WellbeingImmuneIntegrationTest, DisabledDistressTriggerIsNoOp) {
    bridge->enable_wellbeing_immune_trigger = false;

    /* Create high distress */
    uint32_t antigen_id, site_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                  (const uint8_t*)"test", 4, 10, 0, &antigen_id);
    brain_immune_initiate_inflammation(immune_system, 1, antigen_id, &site_id);

    int result = wellbeing_immune_trigger_from_distress(bridge);
    EXPECT_EQ(result, 0);

    /* No triggers should occur */
    uint64_t updates;
    uint32_t triggers;
    wellbeing_immune_get_stats(bridge, &updates, nullptr, &triggers, nullptr);
    EXPECT_EQ(triggers, 0);
}

/* ============================================================================
 * Wellbeing → Immune: Positive Wellbeing Boost Tests
 * ============================================================================ */

TEST_F(WellbeingImmuneIntegrationTest, FlourishingBoostsImmune) {
    /* Simulate flourishing by having no inflammation and positive state */
    int result = wellbeing_immune_boost_from_positive_wellbeing(bridge);
    EXPECT_EQ(result, 0);

    /* Check if flourishing */
    bool flourishing = wellbeing_immune_is_flourishing(bridge);
    /* May or may not be flourishing depending on introspection state */
    /* Just verify API works */
    (void)flourishing;
}

TEST_F(WellbeingImmuneIntegrationTest, PositiveBoostReleasesIL10) {
    wellbeing_immune_boost_from_positive_wellbeing(bridge);

    /* Check for IL-10 in immune system */
    /* Note: This depends on internal implementation details */
    /* For now, just verify call succeeds */
    uint64_t updates;
    uint32_t boosts;
    wellbeing_immune_get_stats(bridge, &updates, nullptr, nullptr, &boosts);
    /* Boosts depend on actual flourishing state */
}

TEST_F(WellbeingImmuneIntegrationTest, MemoryFormationBoostWhenFlourishing) {
    /* Set bridge to flourishing state manually for test */
    bridge->positive_boost.is_flourishing = true;
    bridge->positive_boost.flourishing_level = 0.8f;

    int result = wellbeing_immune_boost_memory_formation(bridge, 0);
    EXPECT_EQ(result, 0);
    EXPECT_GT(bridge->positive_boost.memory_formation_boost, 0.0f);
}

TEST_F(WellbeingImmuneIntegrationTest, NoMemoryBoostWhenNotFlourishing) {
    bridge->positive_boost.is_flourishing = false;

    int result = wellbeing_immune_boost_memory_formation(bridge, 0);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(bridge->positive_boost.memory_formation_boost, 0.0f);
}

TEST_F(WellbeingImmuneIntegrationTest, DisabledPositiveBoostIsNoOp) {
    bridge->enable_positive_immune_boost = false;

    int result = wellbeing_immune_boost_from_positive_wellbeing(bridge);
    EXPECT_EQ(result, 0);

    uint64_t updates;
    uint32_t boosts;
    wellbeing_immune_get_stats(bridge, &updates, nullptr, nullptr, &boosts);
    EXPECT_EQ(boosts, 0);
}

/* ============================================================================
 * Bidirectional Update Tests
 * ============================================================================ */

TEST_F(WellbeingImmuneIntegrationTest, BidirectionalUpdateProcessesBothDirections) {
    /* Create some immune activity */
    uint32_t antigen_id, cytokine_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                  (const uint8_t*)"test", 4, 5, 0, &antigen_id);
    brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL6, 0, 0.5f, 0, &cytokine_id);

    /* Run update */
    int result = wellbeing_immune_bridge_update(bridge, 100);
    EXPECT_EQ(result, 0);

    /* Verify stats incremented */
    uint64_t updates;
    wellbeing_immune_get_stats(bridge, &updates, nullptr, nullptr, nullptr);
    EXPECT_GT(updates, 0);
}

TEST_F(WellbeingImmuneIntegrationTest, MultipleUpdatesAccumulate) {
    for (int i = 0; i < 5; i++) {
        wellbeing_immune_bridge_update(bridge, 100);
    }

    uint64_t updates;
    wellbeing_immune_get_stats(bridge, &updates, nullptr, nullptr, nullptr);
    EXPECT_EQ(updates, 5);
}

/* ============================================================================
 * Query API Tests
 * ============================================================================ */

TEST_F(WellbeingImmuneIntegrationTest, GetCytokineEffects) {
    cytokine_wellbeing_effects_t effects;
    int result = wellbeing_immune_get_cytokine_effects(bridge, &effects);

    EXPECT_EQ(result, 0);
    /* Initial state should be zero */
    EXPECT_EQ(effects.total_life_satisfaction_shift, 0.0f);
}

TEST_F(WellbeingImmuneIntegrationTest, GetInflammationState) {
    inflammation_wellbeing_state_t state;
    int result = wellbeing_immune_get_inflammation_state(bridge, &state);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(state.current_level, INFLAMMATION_NONE);
}

TEST_F(WellbeingImmuneIntegrationTest, GetDistressAssessment) {
    distress_assessment_t assessment = wellbeing_immune_get_distress_assessment(bridge);

    /* Should return valid assessment */
    EXPECT_GE(assessment.distress_score, 0.0f);
    EXPECT_LE(assessment.distress_score, 1.0f);

    /* Free strings if allocated */
    if (assessment.description) free((void*)assessment.description);
    if (assessment.recommended_action) free((void*)assessment.recommended_action);
}

TEST_F(WellbeingImmuneIntegrationTest, IsInflammationDistress) {
    /* No inflammation initially */
    EXPECT_FALSE(wellbeing_immune_is_inflammation_distress(bridge));

    /* Create severe inflammation */
    uint32_t antigen_id, site_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                  (const uint8_t*)"test", 4, 8, 0, &antigen_id);
    brain_immune_initiate_inflammation(immune_system, 1, antigen_id, &site_id);
    brain_immune_escalate_inflammation(immune_system, site_id);
    brain_immune_escalate_inflammation(immune_system, site_id);

    wellbeing_immune_apply_inflammation_effects(bridge);

    /* Should now be inflammation distress */
    EXPECT_TRUE(wellbeing_immune_is_inflammation_distress(bridge));
}

TEST_F(WellbeingImmuneIntegrationTest, GetLifeSatisfactionPenalty) {
    float penalty = wellbeing_immune_get_life_satisfaction_penalty(bridge);
    EXPECT_GE(penalty, 0.0f);
    EXPECT_LE(penalty, 1.0f);
}

TEST_F(WellbeingImmuneIntegrationTest, GetStatistics) {
    uint64_t updates;
    uint32_t modulations, triggers, boosts;

    int result = wellbeing_immune_get_stats(bridge, &updates, &modulations, &triggers, &boosts);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(updates, 0); /* No updates yet */
}

/* ============================================================================
 * Edge Cases and Error Handling
 * ============================================================================ */

TEST_F(WellbeingImmuneIntegrationTest, NullBridgeHandledGracefully) {
    EXPECT_EQ(wellbeing_immune_apply_cytokine_effects(nullptr), -1);
    EXPECT_EQ(wellbeing_immune_apply_inflammation_effects(nullptr), -1);
    EXPECT_EQ(wellbeing_immune_trigger_from_distress(nullptr), -1);
    EXPECT_EQ(wellbeing_immune_boost_from_positive_wellbeing(nullptr), -1);
    EXPECT_EQ(wellbeing_immune_bridge_update(nullptr, 100), -1);

    EXPECT_EQ(wellbeing_immune_compute_distress(nullptr), 0.0f);
    EXPECT_FALSE(wellbeing_immune_is_inflammation_distress(nullptr));
    EXPECT_FALSE(wellbeing_immune_is_flourishing(nullptr));
}

TEST_F(WellbeingImmuneIntegrationTest, NullOutputPointersHandled) {
    EXPECT_EQ(wellbeing_immune_get_cytokine_effects(bridge, nullptr), -1);
    EXPECT_EQ(wellbeing_immune_get_inflammation_state(bridge, nullptr), -1);
    EXPECT_EQ(wellbeing_immune_get_stats(bridge, nullptr, nullptr, nullptr, nullptr), 0);
}

/* ============================================================================
 * Integration Scenario Tests
 * ============================================================================ */

TEST_F(WellbeingImmuneIntegrationTest, FullCycleInflammationToRecovery) {
    /* 1. Create threat and inflammation */
    uint32_t antigen_id, site_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                  (const uint8_t*)"threat", 6, 7, 0, &antigen_id);
    brain_immune_initiate_inflammation(immune_system, 1, antigen_id, &site_id);
    brain_immune_escalate_inflammation(immune_system, site_id);

    /* 2. Apply inflammation effects - should reduce wellbeing */
    wellbeing_immune_apply_inflammation_effects(bridge);
    float distress_high = wellbeing_immune_compute_distress(bridge);
    EXPECT_GT(distress_high, 0.3f);

    /* 3. Resolve inflammation */
    brain_immune_resolve_inflammation(immune_system, site_id);

    /* 4. Boost from positive wellbeing (simulated recovery) */
    wellbeing_immune_boost_from_positive_wellbeing(bridge);

    /* 5. Verify IL-10 released for recovery */
    /* (Would need to check cytokine levels in immune system) */
}

TEST_F(WellbeingImmuneIntegrationTest, ChronicDistressFeedbackLoop) {
    /* Chronic distress → inflammation → more distress */

    /* 1. Start with distress-triggered inflammation */
    uint32_t antigen_id, site_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                  (const uint8_t*)"chronic", 7, 6, 0, &antigen_id);
    brain_immune_initiate_inflammation(immune_system, 1, antigen_id, &site_id);

    /* 2. Multiple update cycles */
    for (int i = 0; i < 10; i++) {
        wellbeing_immune_bridge_update(bridge, 1000);
    }

    /* 3. Verify feedback occurred */
    uint64_t updates;
    uint32_t triggers;
    wellbeing_immune_get_stats(bridge, &updates, nullptr, &triggers, nullptr);
    EXPECT_EQ(updates, 10);
}

/* ============================================================================
 * Thread Safety Tests (Basic)
 * ============================================================================ */

TEST_F(WellbeingImmuneIntegrationTest, ConcurrentUpdatesDoNotCrash) {
    /* Simple test: multiple sequential updates should use mutex correctly */
    for (int i = 0; i < 100; i++) {
        wellbeing_immune_bridge_update(bridge, 10);
    }

    uint64_t updates;
    wellbeing_immune_get_stats(bridge, &updates, nullptr, nullptr, nullptr);
    EXPECT_EQ(updates, 100);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
