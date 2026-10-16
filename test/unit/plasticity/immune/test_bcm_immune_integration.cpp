/**
 * @file test_bcm_immune_integration.cpp
 * @brief Unit tests for BCM-Immune integration bridge
 *
 * WHAT: Comprehensive tests for immune modulation of BCM learning
 * WHY:  Verify correct integration of immune state with BCM plasticity
 * HOW:  Google Test framework with biological validation
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_brain_immune.h"
#include "plasticity/immune/nimcp_bcm_immune_bridge.h"
#include "plasticity/bcm/nimcp_bcm.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class BCMImmuneTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system;
    bcm_params_t bcm_params;
    bcm_immune_bridge_t* bridge;
    bcm_immune_config_t config;

    void SetUp() override {
        /* Create immune system with defaults */
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);

        /* Create BCM parameters */
        bcm_params = bcm_params_cortical();

        /* Get default bridge config */
        int result = bcm_immune_default_config(&config);
        ASSERT_EQ(result, 0);

        /* Create bridge */
        bridge = bcm_immune_bridge_create(&config, immune_system, &bcm_params);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            bcm_immune_bridge_destroy(bridge);
        }
        if (immune_system) {
            brain_immune_destroy(immune_system);
        }
    }

    /* Helper: Create test cytokine */
    void add_cytokine(brain_cytokine_type_t type, float concentration) {
        uint32_t cytokine_id;
        brain_immune_release_cytokine(
            immune_system, type, 0, concentration, 0, &cytokine_id);
    }

    /* Helper: Create inflammation site */
    void create_inflammation(brain_inflammation_level_t level) {
        uint32_t site_id;
        brain_immune_initiate_inflammation(immune_system, 1, 0, &site_id);

        /* Escalate to desired level */
        while (immune_system->inflammation_sites[0].level < level) {
            brain_immune_escalate_inflammation(immune_system, site_id);
        }
    }

    /* Helper: Create test synapses */
    bcm_synapse_t* create_synapses(uint32_t count, float threshold) {
        bcm_synapse_t* synapses = (bcm_synapse_t*)nimcp_malloc(sizeof(bcm_synapse_t) * count);
        for (uint32_t i = 0; i < count; i++) {
            synapses[i] = bcm_synapse_init(0.5f, threshold);
        }
        return synapses;
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(BCMImmuneTest, DefaultConfigValid) {
    /* WHAT: Verify default configuration has reasonable values
     * WHY:  Ensure biological plausibility
     */
    EXPECT_TRUE(config.enable_cytokine_modulation);
    EXPECT_TRUE(config.enable_inflammation_disruption);
    EXPECT_TRUE(config.enable_bcm_immune_trigger);
    EXPECT_TRUE(config.enable_baseline_tracking);
    EXPECT_TRUE(config.enable_recovery_assistance);

    EXPECT_FLOAT_EQ(config.cytokine_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.inflammation_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.abnormality_sensitivity, 1.0f);

    EXPECT_GT(config.threshold_instability_factor, 1.0f);
    EXPECT_LT(config.learning_collapse_factor, 0.5f);
    EXPECT_LT(config.metaplasticity_stuck_factor, 0.1f);
}

TEST_F(BCMImmuneTest, ConfigNullPointerHandling) {
    /* WHAT: Verify NULL pointer safety
     * WHY:  Prevent crashes on invalid input
     */
    int result = bcm_immune_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(BCMImmuneTest, CreateBridgeNullSystems) {
    /* WHAT: Verify creation fails with NULL systems
     * WHY:  Require both immune and BCM to be valid
     */
    bcm_immune_bridge_t* null_bridge = bcm_immune_bridge_create(
        &config, nullptr, &bcm_params);
    EXPECT_EQ(null_bridge, nullptr);

    null_bridge = bcm_immune_bridge_create(&config, immune_system, nullptr);
    EXPECT_EQ(null_bridge, nullptr);
}

/* ============================================================================
 * Cytokine Modulation Tests
 * ============================================================================ */

TEST_F(BCMImmuneTest, CytokineEffectsNoCytokines) {
    /* WHAT: Check effects when no cytokines present
     * WHY:  Baseline should be 1.0 (no modulation)
     */
    int result = bcm_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);

    cytokine_bcm_effects_t effects;
    bcm_immune_get_cytokine_effects(bridge, &effects);

    /* With no cytokines, multipliers should be ~1.0 */
    EXPECT_NEAR(effects.theta_m_multiplier, 1.0f, 0.1f);
    EXPECT_NEAR(effects.learning_rate_multiplier, 1.0f, 0.1f);
    EXPECT_NEAR(effects.tau_multiplier, 1.0f, 0.1f);
}

TEST_F(BCMImmuneTest, IL1ElevatesThreshold) {
    /* WHAT: Verify IL-1β increases theta_m
     * WHY:  Pro-inflammatory cytokines reduce LTP
     */
    add_cytokine(BRAIN_CYTOKINE_IL1, 0.5f);

    int result = bcm_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);

    cytokine_bcm_effects_t effects;
    bcm_immune_get_cytokine_effects(bridge, &effects);

    /* IL-1β should elevate threshold */
    EXPECT_GT(effects.theta_m_multiplier, 1.0f);
    EXPECT_LE(effects.theta_m_multiplier, INFLAMMATION_THETA_MAX_FACTOR);
    EXPECT_GT(effects.il1_threshold_elevation, 0.0f);
}

TEST_F(BCMImmuneTest, IL6ReducesLearningRate) {
    /* WHAT: Verify IL-6 decreases learning rate
     * WHY:  IL-6 slows plasticity
     */
    add_cytokine(BRAIN_CYTOKINE_IL6, 0.6f);

    int result = bcm_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);

    cytokine_bcm_effects_t effects;
    bcm_immune_get_cytokine_effects(bridge, &effects);

    /* IL-6 should reduce learning rate */
    EXPECT_LT(effects.learning_rate_multiplier, 1.0f);
    EXPECT_GE(effects.learning_rate_multiplier, INFLAMMATION_LR_MIN_FACTOR);
    EXPECT_GT(effects.il6_learning_reduction, 0.0f);
}

TEST_F(BCMImmuneTest, TNFAcceleratesSliding) {
    /* WHAT: Verify TNF-α reduces tau (faster sliding)
     * WHY:  TNF-α accelerates threshold adaptation
     */
    add_cytokine(BRAIN_CYTOKINE_TNF, 0.4f);

    int result = bcm_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);

    cytokine_bcm_effects_t effects;
    bcm_immune_get_cytokine_effects(bridge, &effects);

    /* TNF-α should reduce tau (faster sliding) */
    EXPECT_LT(effects.tau_multiplier, 1.0f);
    EXPECT_GE(effects.tau_multiplier, 0.5f);
    EXPECT_GT(effects.tnf_sliding_acceleration, 0.0f);
}

TEST_F(BCMImmuneTest, IL10PromotesRecovery) {
    /* WHAT: Verify IL-10 moves parameters toward baseline
     * WHY:  Anti-inflammatory cytokines restore normal function
     */
    /* First add pro-inflammatory */
    add_cytokine(BRAIN_CYTOKINE_IL1, 0.8f);
    bcm_immune_apply_cytokine_effects(bridge);

    cytokine_bcm_effects_t effects_before;
    bcm_immune_get_cytokine_effects(bridge, &effects_before);
    float theta_before = effects_before.theta_m_multiplier;

    /* Now add IL-10 */
    add_cytokine(BRAIN_CYTOKINE_IL10, 0.5f);
    bcm_immune_apply_cytokine_effects(bridge);

    cytokine_bcm_effects_t effects_after;
    bcm_immune_get_cytokine_effects(bridge, &effects_after);

    /* IL-10 should move theta closer to 1.0 */
    EXPECT_LT(fabsf(effects_after.theta_m_multiplier - 1.0f),
              fabsf(theta_before - 1.0f));
    EXPECT_GT(effects_after.il10_recovery_factor, 0.0f);
}

TEST_F(BCMImmuneTest, CombinedCytokineEffects) {
    /* WHAT: Verify multiple cytokines combine correctly
     * WHY:  Multiple cytokines should have cumulative effects
     */
    add_cytokine(BRAIN_CYTOKINE_IL1, 0.3f);
    add_cytokine(BRAIN_CYTOKINE_IL6, 0.4f);
    add_cytokine(BRAIN_CYTOKINE_TNF, 0.2f);

    int result = bcm_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);

    cytokine_bcm_effects_t effects;
    bcm_immune_get_cytokine_effects(bridge, &effects);

    /* All three pro-inflammatory cytokines should have effects */
    EXPECT_GT(effects.theta_m_multiplier, 1.0f);
    EXPECT_LT(effects.learning_rate_multiplier, 1.0f);
    EXPECT_LT(effects.tau_multiplier, 1.0f);
}

/* ============================================================================
 * Inflammation Disruption Tests
 * ============================================================================ */

TEST_F(BCMImmuneTest, InflammationDisruptionNone) {
    /* WHAT: Check disruption when no inflammation
     * WHY:  No inflammation should mean no disruption
     */
    int result = bcm_immune_apply_inflammation_effects(bridge);
    EXPECT_EQ(result, 0);

    inflammation_bcm_state_t state;
    bcm_immune_get_inflammation_state(bridge, &state);

    EXPECT_EQ(state.current_level, INFLAMMATION_NONE);
    EXPECT_FLOAT_EQ(state.threshold_instability, 0.0f);
    EXPECT_FLOAT_EQ(state.metaplasticity_impairment, 0.0f);
    EXPECT_FLOAT_EQ(state.learning_suppression, 0.0f);
    EXPECT_FALSE(state.is_chronic);
}

TEST_F(BCMImmuneTest, InflammationDisruptionLocal) {
    /* WHAT: Verify local inflammation causes mild disruption
     * WHY:  Low inflammation should have moderate effects
     */
    create_inflammation(INFLAMMATION_LOCAL);

    int result = bcm_immune_apply_inflammation_effects(bridge);
    EXPECT_EQ(result, 0);

    inflammation_bcm_state_t state;
    bcm_immune_get_inflammation_state(bridge, &state);

    EXPECT_EQ(state.current_level, INFLAMMATION_LOCAL);
    EXPECT_GT(state.threshold_instability, 0.0f);
    EXPECT_LT(state.threshold_instability, 0.5f);
}

TEST_F(BCMImmuneTest, InflammationDisruptionSystemic) {
    /* WHAT: Verify systemic inflammation causes severe disruption
     * WHY:  High inflammation should strongly impair BCM
     */
    create_inflammation(INFLAMMATION_SYSTEMIC);

    int result = bcm_immune_apply_inflammation_effects(bridge);
    EXPECT_EQ(result, 0);

    inflammation_bcm_state_t state;
    bcm_immune_get_inflammation_state(bridge, &state);

    EXPECT_EQ(state.current_level, INFLAMMATION_SYSTEMIC);
    EXPECT_GT(state.threshold_instability, 0.5f);
    EXPECT_GT(state.metaplasticity_impairment, 0.5f);
    EXPECT_GT(state.learning_suppression, 0.5f);
}

TEST_F(BCMImmuneTest, InflammationDisruptionStorm) {
    /* WHAT: Verify cytokine storm causes maximum disruption
     * WHY:  Storm should maximally impair plasticity
     */
    create_inflammation(INFLAMMATION_STORM);

    int result = bcm_immune_apply_inflammation_effects(bridge);
    EXPECT_EQ(result, 0);

    inflammation_bcm_state_t state;
    bcm_immune_get_inflammation_state(bridge, &state);

    EXPECT_EQ(state.current_level, INFLAMMATION_STORM);
    EXPECT_GT(state.threshold_instability, 0.7f);
    EXPECT_GT(state.metaplasticity_impairment, 0.6f);
    EXPECT_GT(state.learning_suppression, 0.8f);
}

/* ============================================================================
 * Baseline Tracking Tests
 * ============================================================================ */

TEST_F(BCMImmuneTest, BaselineInitiallyNotEstablished) {
    /* WHAT: Verify baseline not established at start
     * WHY:  Need to collect samples before baseline valid
     */
    EXPECT_FALSE(bridge->baseline_metrics.baseline_established);
    EXPECT_EQ(bridge->baseline_metrics.samples_collected, 0);
}

TEST_F(BCMImmuneTest, BaselineUpdateCollectsSamples) {
    /* WHAT: Verify baseline updates collect samples
     * WHY:  Need to track progress toward baseline
     */
    bcm_synapse_t* synapses = create_synapses(100, 0.5f);
    bcm_stats_t stats = {0};
    stats.avg_threshold = 0.5f;
    stats.total_updates = 100;
    stats.ltp_events = 50;
    stats.ltd_events = 50;

    for (int i = 0; i < 50; i++) {
        bcm_immune_update_baseline(bridge, synapses, 100, &stats);
    }

    EXPECT_EQ(bridge->baseline_metrics.samples_collected, 50);
    EXPECT_FALSE(bridge->baseline_metrics.baseline_established);

    nimcp_free(synapses);
}

TEST_F(BCMImmuneTest, BaselineEstablishedAfterSufficientSamples) {
    /* WHAT: Verify baseline established after enough samples
     * WHY:  Need 100 samples for stable baseline
     */
    bcm_synapse_t* synapses = create_synapses(100, 0.5f);
    bcm_stats_t stats = {0};
    stats.avg_threshold = 0.5f;
    stats.total_updates = 100;
    stats.ltp_events = 50;
    stats.ltd_events = 50;

    for (int i = 0; i < 100; i++) {
        bcm_immune_update_baseline(bridge, synapses, 100, &stats);
    }

    EXPECT_EQ(bridge->baseline_metrics.samples_collected, 100);
    EXPECT_TRUE(bridge->baseline_metrics.baseline_established);

    nimcp_free(synapses);
}

/* ============================================================================
 * Abnormality Detection Tests
 * ============================================================================ */

TEST_F(BCMImmuneTest, AbnormalityDetectionHealthy) {
    /* WHAT: Verify healthy BCM not flagged as abnormal
     * WHY:  Normal dynamics should not trigger immune
     */
    /* Establish baseline first */
    bcm_synapse_t* synapses = create_synapses(100, 0.5f);
    bcm_stats_t stats = {0};
    stats.avg_threshold = 0.5f;
    stats.total_updates = 100;
    stats.ltp_events = 50;
    stats.ltd_events = 50;

    for (int i = 0; i < 100; i++) {
        bcm_immune_update_baseline(bridge, synapses, 100, &stats);
    }

    /* Now detect abnormalities with same healthy metrics */
    int result = bcm_immune_detect_abnormalities(bridge, synapses, 100, &stats);
    EXPECT_EQ(result, 0);

    EXPECT_FALSE(bridge->abnormality_state.threshold_unstable);
    EXPECT_FALSE(bridge->abnormality_state.learning_collapsed);
    EXPECT_FALSE(bridge->abnormality_state.metaplasticity_stuck);
    EXPECT_LT(bridge->abnormality_state.immune_trigger_severity, 3);

    nimcp_free(synapses);
}

TEST_F(BCMImmuneTest, AbnormalityDetectionThresholdInstability) {
    /* WHAT: Verify high threshold variance detected as unstable
     * WHY:  Excessive oscillations are pathological
     */
    /* Establish baseline */
    bcm_synapse_t* synapses = create_synapses(100, 0.5f);
    bcm_stats_t stats = {0};
    stats.avg_threshold = 0.5f;
    stats.total_updates = 100;
    stats.ltp_events = 50;
    stats.ltd_events = 50;

    for (int i = 0; i < 100; i++) {
        bcm_immune_update_baseline(bridge, synapses, 100, &stats);
    }

    /* Create unstable synapses (high variance) */
    for (uint32_t i = 0; i < 100; i++) {
        synapses[i].threshold = 0.5f + ((float)(i % 10) - 5.0f) * 0.2f;
    }
    stats.avg_threshold = 0.5f;

    int result = bcm_immune_detect_abnormalities(bridge, synapses, 100, &stats);
    EXPECT_EQ(result, 0);

    EXPECT_TRUE(bridge->abnormality_state.threshold_unstable);
    EXPECT_GT(bridge->abnormality_state.immune_trigger_severity, 0);

    nimcp_free(synapses);
}

TEST_F(BCMImmuneTest, AbnormalityDetectionLearningCollapse) {
    /* WHAT: Verify low LTP/LTD detected as collapsed
     * WHY:  Minimal plasticity indicates dysfunction
     */
    /* Establish baseline */
    bcm_synapse_t* synapses = create_synapses(100, 0.5f);
    bcm_stats_t stats = {0};
    stats.avg_threshold = 0.5f;
    stats.total_updates = 1000;
    stats.ltp_events = 500;
    stats.ltd_events = 500;

    for (int i = 0; i < 100; i++) {
        bcm_immune_update_baseline(bridge, synapses, 100, &stats);
    }

    /* Create collapsed learning (very few events) */
    stats.total_updates = 1000;
    stats.ltp_events = 10; /* Only 10 events vs 500 baseline */
    stats.ltd_events = 10;

    int result = bcm_immune_detect_abnormalities(bridge, synapses, 100, &stats);
    EXPECT_EQ(result, 0);

    EXPECT_TRUE(bridge->abnormality_state.learning_collapsed);
    EXPECT_GT(bridge->abnormality_state.immune_trigger_severity, 0);

    nimcp_free(synapses);
}

/* ============================================================================
 * Immune Triggering Tests
 * ============================================================================ */

TEST_F(BCMImmuneTest, ImmuneTriggerLowSeverityIgnored) {
    /* WHAT: Verify low severity abnormalities don't trigger immune
     * WHY:  Only significant dysfunction should trigger response
     */
    bridge->abnormality_state.immune_trigger_severity = 2;

    int result = bcm_immune_trigger_from_abnormality(bridge);
    EXPECT_EQ(result, 0);

    /* Should not create antigen */
    EXPECT_EQ(immune_system->antigen_count, 0);
}

TEST_F(BCMImmuneTest, ImmuneTriggerHighSeverityTriggersResponse) {
    /* WHAT: Verify high severity abnormalities trigger immune
     * WHY:  Severe dysfunction requires immune intervention
     */
    bridge->abnormality_state.immune_trigger_severity = 7;
    bridge->abnormality_state.threshold_unstable = true;
    bridge->abnormality_state.learning_collapsed = true;

    int result = bcm_immune_trigger_from_abnormality(bridge);
    EXPECT_EQ(result, 0);

    /* Should create antigen */
    EXPECT_GT(immune_system->antigen_count, 0);
    EXPECT_GT(bridge->immune_triggered_responses, 0);
}

/* ============================================================================
 * Query API Tests
 * ============================================================================ */

TEST_F(BCMImmuneTest, IsHealthyReturnsTrue) {
    /* WHAT: Verify healthy check works
     * WHY:  Quick health check for monitoring
     */
    bridge->abnormality_state.threshold_unstable = false;
    bridge->abnormality_state.learning_collapsed = false;
    bridge->abnormality_state.metaplasticity_stuck = false;

    EXPECT_TRUE(bcm_immune_is_healthy(bridge));
}

TEST_F(BCMImmuneTest, IsHealthyReturnsFalse) {
    /* WHAT: Verify unhealthy detection works
     * WHY:  Any abnormality should flag unhealthy
     */
    bridge->abnormality_state.threshold_unstable = true;

    EXPECT_FALSE(bcm_immune_is_healthy(bridge));
}

TEST_F(BCMImmuneTest, GetThresholdInstability) {
    /* WHAT: Verify instability score retrieval
     * WHY:  Need to query severity
     */
    bridge->abnormality_state.threshold_instability_score = 0.75f;

    float score = bcm_immune_get_threshold_instability(bridge);
    EXPECT_FLOAT_EQ(score, 0.75f);
}

TEST_F(BCMImmuneTest, GetLearningActivity) {
    /* WHAT: Verify learning activity retrieval
     * WHY:  Need to query activity level
     */
    bridge->abnormality_state.learning_activity_score = 0.4f;

    float score = bcm_immune_get_learning_activity(bridge);
    EXPECT_FLOAT_EQ(score, 0.4f);
}

TEST_F(BCMImmuneTest, GetMetaplasticityHealth) {
    /* WHAT: Verify metaplasticity health retrieval
     * WHY:  Need to query sliding function health
     */
    bridge->abnormality_state.metaplasticity_health = 0.9f;

    float health = bcm_immune_get_metaplasticity_health(bridge);
    EXPECT_FLOAT_EQ(health, 0.9f);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(BCMImmuneTest, BidirectionalUpdateFullCycle) {
    /* WHAT: Verify full bidirectional update cycle
     * WHY:  Integration test for complete functionality
     */
    /* Create test synapses and stats */
    bcm_synapse_t* synapses = create_synapses(100, 0.5f);
    bcm_stats_t stats = {0};
    stats.avg_threshold = 0.5f;
    stats.total_updates = 100;
    stats.ltp_events = 50;
    stats.ltd_events = 50;

    /* Establish baseline */
    for (int i = 0; i < 100; i++) {
        bcm_immune_bridge_update(bridge, synapses, 100, &stats, 10);
    }

    EXPECT_TRUE(bridge->baseline_metrics.baseline_established);

    /* Add cytokines */
    add_cytokine(BRAIN_CYTOKINE_IL1, 0.6f);
    add_cytokine(BRAIN_CYTOKINE_IL6, 0.4f);

    /* Create inflammation */
    create_inflammation(INFLAMMATION_REGIONAL);

    /* Update should apply cytokine effects and detect abnormalities */
    int result = bcm_immune_bridge_update(bridge, synapses, 100, &stats, 10);
    EXPECT_EQ(result, 0);

    /* Verify cytokine effects applied */
    cytokine_bcm_effects_t effects;
    bcm_immune_get_cytokine_effects(bridge, &effects);
    EXPECT_GT(effects.theta_m_multiplier, 1.0f);

    /* Verify inflammation effects applied */
    inflammation_bcm_state_t inflam_state;
    bcm_immune_get_inflammation_state(bridge, &inflam_state);
    EXPECT_GT(inflam_state.threshold_instability, 0.0f);

    EXPECT_GT(bridge->total_updates, 0);

    nimcp_free(synapses);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
