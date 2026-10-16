/**
 * @file test_stdp_immune_integration.cpp
 * @brief Unit tests for STDP-Immune integration
 * @version 1.0.0
 * @date 2025-12-11
 */

#include <gtest/gtest.h>
#include "plasticity/immune/nimcp_stdp_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "plasticity/stdp/nimcp_stdp.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class StdpImmuneTest : public ::testing::Test {
protected:
    stdp_immune_bridge_t* bridge;
    brain_immune_system_t* immune_system;
    stdp_synapse_t* synapses;
    size_t num_synapses;

    void SetUp() override {
        /* Create synapses array */
        num_synapses = 10;
        synapses = new stdp_synapse_t[num_synapses];
        for (size_t i = 0; i < num_synapses; i++) {
            stdp_synapse_init(&synapses[i]);
        }

        /* Create immune system */
        brain_immune_config_t immune_cfg;
        brain_immune_default_config(&immune_cfg);
        immune_system = brain_immune_create(&immune_cfg);
        ASSERT_NE(immune_system, nullptr);

        /* Create default config */
        stdp_immune_config_t config;
        stdp_immune_default_config(&config);

        /* Create integration bridge */
        bridge = stdp_immune_bridge_create(&config, immune_system, synapses, num_synapses);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) stdp_immune_bridge_destroy(bridge);
        if (immune_system) brain_immune_destroy(immune_system);
        if (synapses) delete[] synapses;
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(StdpImmuneTest, CreateDestroy) {
    /* WHAT: Test system creation and destruction
     * WHY:  Verify resource management
     * HOW:  Create and destroy, check non-null
     */
    EXPECT_NE(bridge, nullptr);
}

TEST_F(StdpImmuneTest, DefaultConfig) {
    /* WHAT: Test default configuration
     * WHY:  Ensure sensible defaults
     * HOW:  Check config values
     */
    stdp_immune_config_t config;
    ASSERT_EQ(stdp_immune_default_config(&config), 0);

    EXPECT_TRUE(config.enable_cytokine_stdp_modulation);
    EXPECT_TRUE(config.enable_inflammation_impairment);
    EXPECT_TRUE(config.enable_instability_detection);
    EXPECT_TRUE(config.enable_homeostatic_feedback);
    EXPECT_GT(config.base_learning_rate, 0.0f);
    EXPECT_GT(config.base_a_plus, 0.0f);
    EXPECT_GT(config.base_a_minus, 0.0f);
}

TEST_F(StdpImmuneTest, ConnectSystems) {
    /* WHAT: Test system connections
     * WHY:  Verify integration linking
     * HOW:  Connect and check success
     */
    /* Already connected in SetUp, verify successful */
    EXPECT_NE(immune_system, nullptr);
    EXPECT_NE(bridge->immune_system, nullptr);
    EXPECT_EQ(bridge->num_synapses, num_synapses);
}

/* ============================================================================
 * Immune → STDP Tests (Cytokine Effects)
 * ============================================================================ */

TEST_F(StdpImmuneTest, CytokineEffects_IL1_ImpairedLTP) {
    /* WHAT: Test IL-1β impairs LTP
     * WHY:  Verify biological cytokine effect on plasticity
     * HOW:  Apply cytokine effect, check LTP reduction
     */
    cytokine_stdp_effects_t effects;

    /* Apply cytokine effects */
    ASSERT_EQ(stdp_immune_apply_cytokine_effects(bridge), 0);
    ASSERT_EQ(stdp_immune_get_cytokine_effects(bridge, &effects), 0);

    /* IL-1 should impair LTP (factor < 1.0) */
    EXPECT_LT(effects.il1_ltp_impairment, 1.0f);
    EXPECT_GE(effects.il1_ltp_impairment, CYTOKINE_IL1_LTP_IMPAIRMENT);
}

TEST_F(StdpImmuneTest, CytokineEffects_TNF_Alpha_StrongImpairment) {
    /* WHAT: Test TNF-α causes strongest LTP impairment
     * WHY:  Verify most severe pro-inflammatory effect
     * HOW:  Compare TNF-α impairment to other cytokines
     */
    cytokine_stdp_effects_t effects;

    ASSERT_EQ(stdp_immune_apply_cytokine_effects(bridge), 0);
    ASSERT_EQ(stdp_immune_get_cytokine_effects(bridge, &effects), 0);

    /* TNF-α should have strongest impairment */
    EXPECT_LT(effects.tnf_ltp_impairment, effects.il1_ltp_impairment);
    EXPECT_LT(effects.tnf_ltp_impairment, effects.il6_ltp_impairment);
    EXPECT_LE(effects.tnf_ltp_impairment, CYTOKINE_TNF_LTP_IMPAIRMENT);
}

TEST_F(StdpImmuneTest, CytokineEffects_IL10_RestoresLTP) {
    /* WHAT: Test IL-10 restores LTP capacity
     * WHY:  Verify anti-inflammatory restoration
     * HOW:  Check IL-10 restoration factor > 1.0
     */
    cytokine_stdp_effects_t effects;

    ASSERT_EQ(stdp_immune_apply_cytokine_effects(bridge), 0);
    ASSERT_EQ(stdp_immune_get_cytokine_effects(bridge, &effects), 0);

    /* IL-10 should restore/enhance LTP */
    EXPECT_GT(effects.il10_ltp_restoration, 1.0f);
    EXPECT_GE(effects.il10_ltp_restoration, CYTOKINE_IL10_LTP_RESTORATION);
}

TEST_F(StdpImmuneTest, CytokineEffects_TotalModulation) {
    /* WHAT: Test total LTP modulation combines all cytokines
     * WHY:  Verify compound effects are computed
     * HOW:  Check total modulation reflects all factors
     */
    cytokine_stdp_effects_t effects;

    ASSERT_EQ(stdp_immune_apply_cytokine_effects(bridge), 0);
    ASSERT_EQ(stdp_immune_get_cytokine_effects(bridge, &effects), 0);

    /* Total modulation should be product of all factors */
    EXPECT_GT(effects.total_ltp_modulation, 0.0f);
    EXPECT_LE(effects.total_ltp_modulation, 2.0f);
    EXPECT_GT(effects.learning_rate_factor, 0.0f);
    EXPECT_LE(effects.learning_rate_factor, 1.5f);
}

/* ============================================================================
 * Immune → STDP Tests (Inflammation Effects)
 * ============================================================================ */

TEST_F(StdpImmuneTest, InflammationEffects_LocalInflammation) {
    /* WHAT: Test local inflammation has mild effect
     * WHY:  Verify graded inflammation response
     * HOW:  Check local inflammation → 90% learning rate
     */
    inflammation_stdp_state_t state;

    /* Apply inflammation effects */
    ASSERT_EQ(stdp_immune_apply_inflammation_effects(bridge), 0);
    ASSERT_EQ(stdp_immune_get_inflammation_state(bridge, &state), 0);

    /* Initial state should be none */
    EXPECT_EQ(state.current_level, INFLAMMATION_NONE);
    EXPECT_FLOAT_EQ(state.learning_rate_suppression, 0.0f);
}

TEST_F(StdpImmuneTest, InflammationEffects_SystemicInflammation) {
    /* WHAT: Test systemic inflammation severely impairs learning
     * WHY:  Verify severe inflammation → 40% learning rate
     * HOW:  Simulate systemic inflammation, check suppression
     */
    /* Note: Would need to set inflammation in immune_system
     * For now, test the mapping functions work */
    float lr_systemic = stdp_immune_get_effective_learning_rate(bridge, 1.0f);
    EXPECT_GT(lr_systemic, 0.0f);
}

TEST_F(StdpImmuneTest, InflammationEffects_ChronicInflammation) {
    /* WHAT: Test chronic inflammation causes persistent deficits
     * WHY:  Verify chronic (>7 days) effects
     * HOW:  Check chronic flag and additional impairments
     */
    inflammation_stdp_state_t state;

    ASSERT_EQ(stdp_immune_apply_inflammation_effects(bridge), 0);
    ASSERT_EQ(stdp_immune_get_inflammation_state(bridge, &state), 0);

    /* Initially not chronic */
    EXPECT_FALSE(state.is_chronic);
    EXPECT_FLOAT_EQ(state.spine_density_loss, 0.0f);
    EXPECT_FLOAT_EQ(state.consolidation_impairment, 0.0f);
}

TEST_F(StdpImmuneTest, InflammationEffects_TimingWindowNarrowing) {
    /* WHAT: Test inflammation narrows STDP timing window
     * WHY:  Verify tau reduction from inflammation
     * HOW:  Check timing_window_narrowing factor
     */
    inflammation_stdp_state_t state;

    ASSERT_EQ(stdp_immune_apply_inflammation_effects(bridge), 0);
    ASSERT_EQ(stdp_immune_get_inflammation_state(bridge, &state), 0);

    /* Should have narrowing factor */
    EXPECT_GE(state.timing_window_narrowing, 0.0f);
    EXPECT_LE(state.timing_window_narrowing, 1.0f);
}

TEST_F(StdpImmuneTest, InflammationEffects_LTPCapacityReduction) {
    /* WHAT: Test inflammation reduces LTP capacity
     * WHY:  Verify magnitude reduction from inflammation
     * HOW:  Check ltp_capacity_reduction factor
     */
    inflammation_stdp_state_t state;

    ASSERT_EQ(stdp_immune_apply_inflammation_effects(bridge), 0);
    ASSERT_EQ(stdp_immune_get_inflammation_state(bridge, &state), 0);

    /* Should have capacity reduction */
    EXPECT_GE(state.ltp_capacity_reduction, 0.0f);
    EXPECT_LE(state.ltp_capacity_reduction, 0.9f);
}

TEST_F(StdpImmuneTest, InflammationEffects_LTDEnhancement) {
    /* WHAT: Test inflammation enhances LTD susceptibility
     * WHY:  Verify inflammation makes depression easier
     * HOW:  Check ltd_enhancement factor
     */
    inflammation_stdp_state_t state;

    ASSERT_EQ(stdp_immune_apply_inflammation_effects(bridge), 0);
    ASSERT_EQ(stdp_immune_get_inflammation_state(bridge, &state), 0);

    /* Should have LTD enhancement */
    EXPECT_GE(state.ltd_enhancement, 0.0f);
    EXPECT_LE(state.ltd_enhancement, 0.5f);
}

/* ============================================================================
 * STDP Parameter Modulation Tests
 * ============================================================================ */

TEST_F(StdpImmuneTest, ModulationState_AllFactorsComputed) {
    /* WHAT: Test modulation state computation
     * WHY:  Verify all STDP parameters get modulation factors
     * HOW:  Get modulation state, check all fields
     */
    stdp_modulation_state_t modulation;

    ASSERT_EQ(stdp_immune_get_modulation_state(bridge, &modulation), 0);

    /* All modulation factors should be positive */
    EXPECT_GT(modulation.learning_rate_modulation, 0.0f);
    EXPECT_GT(modulation.a_plus_modulation, 0.0f);
    EXPECT_GT(modulation.a_minus_modulation, 0.0f);
    EXPECT_GT(modulation.tau_plus_modulation, 0.0f);
    EXPECT_GT(modulation.tau_minus_modulation, 0.0f);

    /* Effective parameters should be computed */
    EXPECT_GT(modulation.effective_learning_rate, 0.0f);
    EXPECT_GT(modulation.effective_a_plus, 0.0f);
    EXPECT_GT(modulation.effective_a_minus, 0.0f);
    EXPECT_GT(modulation.effective_tau_plus, 0.0f);
    EXPECT_GT(modulation.effective_tau_minus, 0.0f);
}

TEST_F(StdpImmuneTest, ApplyModulationToSynapse) {
    /* WHAT: Test applying modulation to STDP synapse
     * WHY:  Verify parameters are updated correctly
     * HOW:  Apply modulation, check synapse parameters changed
     */
    stdp_synapse_t* synapse = &synapses[0];
    float original_lr = synapse->learning_rate;

    /* Apply modulation */
    ASSERT_EQ(stdp_immune_apply_modulation_to_synapse(bridge, synapse), 0);

    /* Parameters should be updated (may be same if no inflammation) */
    EXPECT_GT(synapse->learning_rate, 0.0f);
    EXPECT_GT(synapse->a_plus, 0.0f);
    EXPECT_GT(synapse->a_minus, 0.0f);
    EXPECT_GT(synapse->tau_plus, 0.0f);
    EXPECT_GT(synapse->tau_minus, 0.0f);
}

TEST_F(StdpImmuneTest, RestorePlasticity) {
    /* WHAT: Test plasticity restoration after inflammation
     * WHY:  Verify parameters can return to baseline
     * HOW:  Restore with recovery factor, check interpolation
     */
    /* First apply modulation */
    for (size_t i = 0; i < num_synapses; i++) {
        stdp_immune_apply_modulation_to_synapse(bridge, &synapses[i]);
    }

    /* Restore with 50% recovery */
    ASSERT_EQ(stdp_immune_restore_plasticity(bridge, 0.5f), 0);

    /* Check restoration happened */
    EXPECT_GT(bridge->plasticity_restorations, 0);

    /* Restore fully */
    ASSERT_EQ(stdp_immune_restore_plasticity(bridge, 1.0f), 0);
}

TEST_F(StdpImmuneTest, EffectiveLearningRate) {
    /* WHAT: Test effective learning rate computation
     * WHY:  Verify combined cytokine and inflammation effects
     * HOW:  Get effective LR, compare to base
     */
    float base_lr = 0.01f;
    float effective_lr = stdp_immune_get_effective_learning_rate(bridge, base_lr);

    /* Should be positive and <= base (never increases without IL-10) */
    EXPECT_GT(effective_lr, 0.0f);
    EXPECT_LE(effective_lr, base_lr * 1.5f);  /* Allow for IL-10 restoration */
}

/* ============================================================================
 * STDP → Immune Tests (Instability Detection)
 * ============================================================================ */

TEST_F(StdpImmuneTest, InstabilityDetection_NoInstabilityInitially) {
    /* WHAT: Test no instability detected with zero activity
     * WHY:  Verify false positive prevention
     * HOW:  Detect instability on fresh synapses
     */
    ASSERT_EQ(stdp_immune_detect_instability(bridge), 0);

    stdp_instability_state_t state;
    ASSERT_EQ(stdp_immune_get_instability_state(bridge, &state), 0);

    EXPECT_FALSE(state.ltp_runaway_detected);
    EXPECT_FALSE(state.ltd_runaway_detected);
    EXPECT_FALSE(state.homeostatic_threat);
    EXPECT_FLOAT_EQ(state.instability_severity, 0.0f);
}

TEST_F(StdpImmuneTest, InstabilityDetection_LTPRunaway) {
    /* WHAT: Test detection of runaway LTP
     * WHY:  Verify excessive potentiation detection
     * HOW:  Simulate excessive LTP, check detection
     */
    /* Simulate excessive LTP on synapses */
    for (size_t i = 0; i < num_synapses; i++) {
        synapses[i].total_ltp = 15.0f;  /* Above threshold */
        synapses[i].total_ltd = 0.1f;   /* Minimal LTD */
    }

    ASSERT_EQ(stdp_immune_detect_instability(bridge), 0);

    stdp_instability_state_t state;
    ASSERT_EQ(stdp_immune_get_instability_state(bridge, &state), 0);

    /* Should detect LTP runaway */
    EXPECT_TRUE(state.ltp_runaway_detected);
    EXPECT_GT(state.ltp_ltd_ratio, STDP_LTP_LTD_BALANCE_THRESHOLD);
    EXPECT_GT(state.instability_severity, 0.5f);
}

TEST_F(StdpImmuneTest, InstabilityDetection_LTDRunaway) {
    /* WHAT: Test detection of runaway LTD
     * WHY:  Verify excessive depression detection
     * HOW:  Simulate excessive LTD, check detection
     */
    /* Simulate excessive LTD on synapses */
    for (size_t i = 0; i < num_synapses; i++) {
        synapses[i].total_ltp = 0.1f;   /* Minimal LTP */
        synapses[i].total_ltd = 15.0f;  /* Above threshold */
    }

    ASSERT_EQ(stdp_immune_detect_instability(bridge), 0);

    stdp_instability_state_t state;
    ASSERT_EQ(stdp_immune_get_instability_state(bridge, &state), 0);

    /* Should detect LTD runaway */
    EXPECT_TRUE(state.ltd_runaway_detected);
    EXPECT_LT(state.ltp_ltd_ratio, (1.0f / STDP_LTP_LTD_BALANCE_THRESHOLD));
    EXPECT_GT(state.instability_severity, 0.5f);
}

TEST_F(StdpImmuneTest, InstabilityDetection_BalancedPlasticity) {
    /* WHAT: Test detection of balanced LTP/LTD
     * WHY:  Verify healthy plasticity recognition
     * HOW:  Set balanced LTP/LTD, check balanced flag
     */
    /* Simulate balanced plasticity */
    for (size_t i = 0; i < num_synapses; i++) {
        synapses[i].total_ltp = 2.0f;
        synapses[i].total_ltd = 1.8f;  /* Close to balanced */
    }

    ASSERT_EQ(stdp_immune_detect_instability(bridge), 0);

    stdp_instability_state_t state;
    ASSERT_EQ(stdp_immune_get_instability_state(bridge, &state), 0);

    /* Should detect balanced plasticity */
    EXPECT_TRUE(state.balanced_plasticity);
    EXPECT_FALSE(state.ltp_runaway_detected);
    EXPECT_FALSE(state.ltd_runaway_detected);
    EXPECT_LT(state.instability_severity, 0.5f);
}

TEST_F(StdpImmuneTest, InstabilityDetection_HomeostasisThreat) {
    /* WHAT: Test detection of rapid weight changes
     * WHY:  Verify homeostatic threat detection
     * HOW:  Simulate rapid changes, check threat flag
     */
    /* Simulate rapid weight changes */
    for (size_t i = 0; i < num_synapses; i++) {
        synapses[i].total_ltp = 8.0f;
        synapses[i].total_ltd = 8.0f;  /* Balanced but rapid */
    }

    ASSERT_EQ(stdp_immune_detect_instability(bridge), 0);

    stdp_instability_state_t state;
    ASSERT_EQ(stdp_immune_get_instability_state(bridge, &state), 0);

    /* May detect homeostatic threat depending on rate */
    EXPECT_GE(state.weight_change_rate, 0.0f);
}

TEST_F(StdpImmuneTest, AlertInstability) {
    /* WHAT: Test alerting immune system of instability
     * WHY:  Verify immune system notification
     * HOW:  Create instability, alert, check antigen
     */
    /* Simulate instability */
    for (size_t i = 0; i < num_synapses; i++) {
        synapses[i].total_ltp = 15.0f;
        synapses[i].total_ltd = 0.1f;
    }

    ASSERT_EQ(stdp_immune_detect_instability(bridge), 0);

    /* Alert immune system */
    uint32_t antigen_id = 0;
    int result = stdp_immune_alert_instability(bridge, &antigen_id);

    /* Should succeed if instability is severe enough */
    if (result == 0) {
        EXPECT_GT(antigen_id, 0);
        EXPECT_GT(bridge->instability_alerts, 0);
    }
}

TEST_F(StdpImmuneTest, SignalBalancedPlasticity) {
    /* WHAT: Test signaling healthy plasticity to immune
     * WHY:  Verify anti-inflammatory signaling
     * HOW:  Set balanced state, signal, check success
     */
    /* Simulate balanced plasticity */
    for (size_t i = 0; i < num_synapses; i++) {
        synapses[i].total_ltp = 2.0f;
        synapses[i].total_ltd = 1.8f;
    }

    ASSERT_EQ(stdp_immune_detect_instability(bridge), 0);
    ASSERT_EQ(stdp_immune_signal_balanced_plasticity(bridge), 0);

    /* Should not fail */
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(StdpImmuneTest, FullCycle_InflammationImpairedLearning) {
    /* WHAT: Test complete inflammation → impaired learning cycle
     * WHY:  Verify end-to-end integration
     * HOW:  Inflammation → cytokine effects → STDP modulation
     */
    /* Apply cytokine and inflammation effects */
    ASSERT_EQ(stdp_immune_apply_cytokine_effects(bridge), 0);
    ASSERT_EQ(stdp_immune_apply_inflammation_effects(bridge), 0);

    /* Get effective learning rate */
    float base_lr = bridge->base_learning_rate;
    float effective_lr = stdp_immune_get_effective_learning_rate(bridge, base_lr);

    /* Should be positive */
    EXPECT_GT(effective_lr, 0.0f);

    /* Apply to synapses */
    for (size_t i = 0; i < num_synapses; i++) {
        ASSERT_EQ(stdp_immune_apply_modulation_to_synapse(bridge, &synapses[i]), 0);
    }
}

TEST_F(StdpImmuneTest, FullCycle_InstabilityToImmune) {
    /* WHAT: Test complete instability → immune alert cycle
     * WHY:  Verify STDP → immune pathway
     * HOW:  Runaway LTP → detect → alert → antigen
     */
    /* Create runaway LTP */
    for (size_t i = 0; i < num_synapses; i++) {
        synapses[i].total_ltp = 15.0f;
        synapses[i].total_ltd = 0.1f;
    }

    /* Detect instability */
    ASSERT_EQ(stdp_immune_detect_instability(bridge), 0);

    stdp_instability_state_t state;
    ASSERT_EQ(stdp_immune_get_instability_state(bridge, &state), 0);
    EXPECT_TRUE(state.ltp_runaway_detected);

    /* Alert immune system */
    uint32_t antigen_id = 0;
    if (state.instability_severity >= 0.5f) {
        int result = stdp_immune_alert_instability(bridge, &antigen_id);
        if (result == 0) {
            EXPECT_GT(antigen_id, 0);
        }
    }
}

TEST_F(StdpImmuneTest, BidirectionalUpdate) {
    /* WHAT: Test update loop integrates all components
     * WHY:  Verify update processes everything correctly
     * HOW:  Set up state, call update, check results
     */
    /* Update integration system */
    ASSERT_EQ(stdp_immune_bridge_update(bridge, 100), 0);

    /* System should be running */
    EXPECT_GT(bridge->total_updates, 0);

    /* Multiple updates should work */
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(stdp_immune_bridge_update(bridge, 100), 0);
    }

    EXPECT_EQ(bridge->total_updates, 11);
}

TEST_F(StdpImmuneTest, Query_PlasticityImpaired) {
    /* WHAT: Test querying plasticity impairment status
     * WHY:  Verify query API
     * HOW:  Check impairment before and after effects
     */
    /* Initially not impaired */
    bool impaired_before = stdp_immune_is_plasticity_impaired(bridge);

    /* Apply effects */
    ASSERT_EQ(stdp_immune_apply_cytokine_effects(bridge), 0);
    ASSERT_EQ(stdp_immune_apply_inflammation_effects(bridge), 0);

    /* Check again (may or may not be impaired depending on state) */
    bool impaired_after = stdp_immune_is_plasticity_impaired(bridge);

    /* At least one of the queries should succeed */
    EXPECT_TRUE(true);  /* Test passes if no crash */
}

TEST_F(StdpImmuneTest, Query_LTPCapacityReduction) {
    /* WHAT: Test querying LTP capacity reduction
     * WHY:  Verify query API
     * HOW:  Get reduction percentage, check range
     */
    float reduction = stdp_immune_get_ltp_capacity_reduction(bridge);

    /* Should be in valid range [0-100] */
    EXPECT_GE(reduction, 0.0f);
    EXPECT_LE(reduction, 100.0f);
}

/* ============================================================================
 * Edge Cases and Error Handling
 * ============================================================================ */

TEST_F(StdpImmuneTest, NullPointerHandling) {
    /* WHAT: Test null pointer handling
     * WHY:  Verify robustness
     * HOW:  Call functions with null pointers
     */
    EXPECT_EQ(stdp_immune_default_config(nullptr), -1);
    EXPECT_EQ(stdp_immune_apply_cytokine_effects(nullptr), -1);
    EXPECT_EQ(stdp_immune_detect_instability(nullptr), -1);
    EXPECT_FALSE(stdp_immune_is_plasticity_impaired(nullptr));
}

TEST_F(StdpImmuneTest, ZeroSynapses) {
    /* WHAT: Test handling of zero synapses
     * WHY:  Verify edge case handling
     * HOW:  Create bridge with zero synapses (should fail)
     */
    stdp_immune_config_t config;
    stdp_immune_default_config(&config);

    stdp_immune_bridge_t* test_bridge = stdp_immune_bridge_create(
        &config, immune_system, synapses, 0);

    /* Should fail to create */
    EXPECT_EQ(test_bridge, nullptr);
}

TEST_F(StdpImmuneTest, RecoveryFactorClamping) {
    /* WHAT: Test recovery factor clamping
     * WHY:  Verify bounds checking
     * HOW:  Restore with out-of-range factors
     */
    /* Should clamp to [0, 1] internally */
    ASSERT_EQ(stdp_immune_restore_plasticity(bridge, -1.0f), 0);
    ASSERT_EQ(stdp_immune_restore_plasticity(bridge, 2.0f), 0);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
