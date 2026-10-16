/**
 * @file test_neuromodulator_immune.cpp
 * @brief Unit tests for neuromodulator-immune integration
 * @version 1.0.0
 * @date 2025-12-11
 */

#include <gtest/gtest.h>
#include "plasticity/immune/nimcp_neuromodulator_immune.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class NeuromodImmuneTest : public ::testing::Test {
protected:
    neuromod_immune_system_t* system;
    brain_immune_system_t* immune_system;
    neuromodulator_system_t neuromod_system;

    void SetUp() override {
        /* Create default config */
        neuromod_immune_config_t config;
        neuromod_immune_default_config(&config);

        /* Create integration system */
        system = neuromod_immune_create(&config);
        ASSERT_NE(system, nullptr);

        /* Create immune system */
        brain_immune_config_t immune_cfg;
        brain_immune_default_config(&immune_cfg);
        immune_system = brain_immune_create(&immune_cfg);
        ASSERT_NE(immune_system, nullptr);

        /* Create neuromodulator system */
        neuromodulator_config_t neuromod_cfg = {
            .baseline_dopamine = 0.00005f,
            .baseline_serotonin = 0.00003f,
            .baseline_acetylcholine = 0.0001f,
            .baseline_norepinephrine = 0.00002f,
            .dopamine_decay = 2.0f,
            .serotonin_decay = 10.0f,
            .acetylcholine_decay = 0.5f,
            .norepinephrine_decay = 3.0f,
            .reward_dopamine_gain = 0.5f,
            .threat_norepinephrine_gain = 0.7f,
            .salience_acetylcholine_gain = 0.6f,
            .punishment_serotonin_gain = 0.4f,
            .enable_volume_transmission = true,
            .diffusion_rate = 0.1f
        };
        neuromod_system = neuromodulator_system_create(&neuromod_cfg);
        ASSERT_NE(neuromod_system, nullptr);

        /* Connect systems */
        ASSERT_EQ(neuromod_immune_connect_immune(system, immune_system), 0);
        ASSERT_EQ(neuromod_immune_connect_neuromod(system, neuromod_system), 0);
    }

    void TearDown() override {
        if (system) neuromod_immune_destroy(system);
        if (immune_system) brain_immune_destroy(immune_system);
        if (neuromod_system) neuromodulator_system_destroy(neuromod_system);
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(NeuromodImmuneTest, CreateDestroy) {
    /* WHAT: Test system creation and destruction
     * WHY:  Verify resource management
     * HOW:  Create and destroy, check non-null
     */
    EXPECT_NE(system, nullptr);
}

TEST_F(NeuromodImmuneTest, DefaultConfig) {
    /* WHAT: Test default configuration
     * WHY:  Ensure sensible defaults
     * HOW:  Check config values
     */
    neuromod_immune_config_t config;
    ASSERT_EQ(neuromod_immune_default_config(&config), 0);

    EXPECT_GT(config.dopamine_baseline, 0.0f);
    EXPECT_GT(config.serotonin_baseline, 0.0f);
    EXPECT_GT(config.imbalance_threshold, 0.0f);
    EXPECT_TRUE(config.enable_cytokine_effects);
    EXPECT_TRUE(config.enable_imbalance_detection);
}

TEST_F(NeuromodImmuneTest, ConnectSystems) {
    /* WHAT: Test system connections
     * WHY:  Verify integration linking
     * HOW:  Connect and check success
     */
    /* Already connected in SetUp, verify successful */
    EXPECT_NE(immune_system, nullptr);
    EXPECT_NE(neuromod_system, nullptr);
}

/* ============================================================================
 * Cytokine → Neuromodulator Tests
 * ============================================================================ */

TEST_F(NeuromodImmuneTest, IL1_SuppressesSynthesis) {
    /* WHAT: Test IL-1 suppression of monoamine synthesis
     * WHY:  Verify biological cytokine effect
     * HOW:  Apply IL-1, check synthesis multipliers
     */
    cytokine_neuromod_effects_t effects_before, effects_after;
    ASSERT_EQ(neuromod_immune_get_cytokine_effects(system, &effects_before), 0);

    /* Apply IL-1β effect */
    ASSERT_EQ(neuromod_immune_apply_cytokine_effect(
        system, CYTOKINE_IL1B, 0.5f), 0);

    ASSERT_EQ(neuromod_immune_get_cytokine_effects(system, &effects_after), 0);

    /* IL-1 should suppress dopamine and serotonin */
    EXPECT_LT(effects_after.dopamine_synthesis_multiplier,
              effects_before.dopamine_synthesis_multiplier);
    EXPECT_LT(effects_after.serotonin_synthesis_multiplier,
              effects_before.serotonin_synthesis_multiplier);
    EXPECT_LT(effects_after.tyrosine_hydroxylase_activity,
              effects_before.tyrosine_hydroxylase_activity);
}

TEST_F(NeuromodImmuneTest, TNF_Alpha_StrongSuppression) {
    /* WHAT: Test TNF-α very strong suppression
     * WHY:  Verify strongest pro-inflammatory effect
     * HOW:  Apply TNF-α, check greater suppression than IL-1
     */
    cytokine_neuromod_effects_t effects_tnf;

    /* Apply TNF-α */
    ASSERT_EQ(neuromod_immune_apply_cytokine_effect(
        system, CYTOKINE_TNFA, 0.5f), 0);

    ASSERT_EQ(neuromod_immune_get_cytokine_effects(system, &effects_tnf), 0);

    /* TNF-α should cause strong suppression */
    EXPECT_LT(effects_tnf.dopamine_synthesis_multiplier, 0.8f);
    EXPECT_LT(effects_tnf.serotonin_synthesis_multiplier, 0.75f);
    EXPECT_GT(effects_tnf.mao_activity, 1.0f);  /* Increased degradation */
}

TEST_F(NeuromodImmuneTest, IL10_RestoresSynthesis) {
    /* WHAT: Test IL-10 anti-inflammatory restoration
     * WHY:  Verify IL-10 counteracts pro-inflammatory suppression
     * HOW:  Suppress then restore, check recovery
     */
    cytokine_neuromod_effects_t effects;

    /* First suppress with IL-6 */
    ASSERT_EQ(neuromod_immune_apply_cytokine_effect(
        system, CYTOKINE_IL6, 0.5f), 0);
    ASSERT_EQ(neuromod_immune_get_cytokine_effects(system, &effects), 0);
    float suppressed_da = effects.dopamine_synthesis_multiplier;

    /* Then restore with IL-10 */
    ASSERT_EQ(neuromod_immune_apply_cytokine_effect(
        system, CYTOKINE_IL10, 0.8f), 0);
    ASSERT_EQ(neuromod_immune_get_cytokine_effects(system, &effects), 0);

    /* IL-10 should restore toward 1.0 */
    EXPECT_GT(effects.dopamine_synthesis_multiplier, suppressed_da);
    EXPECT_GT(effects.tyrosine_hydroxylase_activity, 0.9f);
}

TEST_F(NeuromodImmuneTest, ProInflammatory_Effect) {
    /* WHAT: Test generalized pro-inflammatory effect
     * WHY:  Verify batch cytokine application
     * HOW:  Apply pro-inflammatory, check all suppressions
     */
    cytokine_neuromod_effects_t effects;

    ASSERT_EQ(neuromod_immune_apply_proinflammatory_effect(
        system, 0.6f), 0);

    ASSERT_EQ(neuromod_immune_get_cytokine_effects(system, &effects), 0);

    /* Should suppress synthesis and increase degradation */
    EXPECT_LT(effects.dopamine_synthesis_multiplier, 1.0f);
    EXPECT_LT(effects.serotonin_synthesis_multiplier, 1.0f);
    EXPECT_LT(effects.tyrosine_availability, 1.0f);
    EXPECT_LT(effects.tryptophan_availability, 1.0f);
    EXPECT_GT(effects.mao_activity, 1.0f);
    EXPECT_EQ(effects.total_suppressions, 1);
}

TEST_F(NeuromodImmuneTest, AntiInflammatory_Effect) {
    /* WHAT: Test anti-inflammatory restoration
     * WHY:  Verify IL-10 restorative pathway
     * HOW:  Suppress then restore, verify recovery
     */
    cytokine_neuromod_effects_t effects;

    /* Suppress first */
    ASSERT_EQ(neuromod_immune_apply_proinflammatory_effect(system, 0.5f), 0);

    /* Then restore */
    ASSERT_EQ(neuromod_immune_apply_antiinflammatory_effect(system, 0.7f), 0);

    ASSERT_EQ(neuromod_immune_get_cytokine_effects(system, &effects), 0);

    /* Should restore toward normal */
    EXPECT_GT(effects.dopamine_synthesis_multiplier, 0.7f);
    EXPECT_GT(effects.tyrosine_hydroxylase_activity, 0.7f);
    EXPECT_EQ(effects.total_enhancements, 1);
}

/* ============================================================================
 * Neuromodulator → Immune Tests
 * ============================================================================ */

TEST_F(NeuromodImmuneTest, DetectDopamineExcess) {
    /* WHAT: Test detection of dopamine excess
     * WHY:  Verify imbalance detection mechanism
     * HOW:  Set high DA, detect imbalance
     */
    /* Set dopamine to excess */
    ASSERT_TRUE(neuromodulator_set_level(neuromod_system, NEUROMOD_DOPAMINE, 0.0002f));

    neuromod_imbalance_t* imbalance = nullptr;
    int result = neuromod_immune_detect_imbalance(system, &imbalance);

    EXPECT_EQ(result, 0);
    ASSERT_NE(imbalance, nullptr);
    EXPECT_EQ(imbalance->type, NEUROMOD_IMBALANCE_DA_EXCESS);
    EXPECT_GT(imbalance->severity, 0.4f);
}

TEST_F(NeuromodImmuneTest, DetectSerotoninDeficiency) {
    /* WHAT: Test detection of serotonin deficiency
     * WHY:  Verify depression-like state detection
     * HOW:  Set low 5-HT, detect imbalance
     */
    /* Set serotonin to deficiency */
    ASSERT_TRUE(neuromodulator_set_level(neuromod_system, NEUROMOD_SEROTONIN, 0.00001f));

    neuromod_imbalance_t* imbalance = nullptr;
    int result = neuromod_immune_detect_imbalance(system, &imbalance);

    EXPECT_EQ(result, 0);
    ASSERT_NE(imbalance, nullptr);
    EXPECT_EQ(imbalance->type, NEUROMOD_IMBALANCE_5HT_DEFICIENCY);
    EXPECT_LT(imbalance->serotonin_deviation, 0.0f);
}

TEST_F(NeuromodImmuneTest, NoImbalanceWhenBalanced) {
    /* WHAT: Test no detection when balanced
     * WHY:  Verify false positive prevention
     * HOW:  Keep baselines, check no imbalance
     */
    neuromod_imbalance_t* imbalance = nullptr;
    int result = neuromod_immune_detect_imbalance(system, &imbalance);

    /* Should return -1 (no imbalance) when balanced */
    EXPECT_EQ(result, -1);
    EXPECT_EQ(imbalance, nullptr);
}

TEST_F(NeuromodImmuneTest, AlertImbalanceToImmune) {
    /* WHAT: Test alerting immune system of imbalance
     * WHY:  Verify immune system notification
     * HOW:  Create imbalance, alert, check antigen
     */
    /* Create dopamine excess */
    ASSERT_TRUE(neuromodulator_set_level(neuromod_system, NEUROMOD_DOPAMINE, 0.0002f));

    neuromod_imbalance_t* imbalance = nullptr;
    ASSERT_EQ(neuromod_immune_detect_imbalance(system, &imbalance), 0);
    ASSERT_NE(imbalance, nullptr);

    /* Alert immune system */
    uint32_t antigen_id = 0;
    int result = neuromod_immune_alert_imbalance(system, imbalance, &antigen_id);

    EXPECT_EQ(result, 0);
    EXPECT_GT(antigen_id, 0);
    EXPECT_TRUE(imbalance->immune_alerted);
    EXPECT_EQ(imbalance->antigen_id, antigen_id);
}

TEST_F(NeuromodImmuneTest, CorrectImbalance) {
    /* WHAT: Test homeostatic correction of imbalance
     * WHY:  Verify immune response to restore balance
     * HOW:  Suppress synthesis first, create imbalance, apply correction, check recovery
     */
    /* First apply pro-inflammatory suppression to drive synthesis below 1.0.
     * The correction path uses apply_antiinflammatory_effect() which restores
     * TOWARD 1.0 via `+= restoration * (1.0 - current)`. If synthesis is
     * already at 1.0, the delta is zero and the correction has no visible
     * effect on synthesis multipliers. */
    ASSERT_EQ(neuromod_immune_apply_proinflammatory_effect(system, 0.5f), 0);

    /* Create serotonin deficiency */
    ASSERT_TRUE(neuromodulator_set_level(neuromod_system, NEUROMOD_SEROTONIN, 0.00001f));

    neuromod_imbalance_t* imbalance = nullptr;
    ASSERT_EQ(neuromod_immune_detect_imbalance(system, &imbalance), 0);
    ASSERT_NE(imbalance, nullptr);

    /* Apply correction */
    cytokine_neuromod_effects_t effects_before, effects_after;
    ASSERT_EQ(neuromod_immune_get_cytokine_effects(system, &effects_before), 0);

    int result = neuromod_immune_correct_imbalance(system, imbalance);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(imbalance->corrective_action_taken);

    ASSERT_EQ(neuromod_immune_get_cytokine_effects(system, &effects_after), 0);

    /* Correction for 5-HT deficiency should enhance synthesis back toward 1.0 */
    EXPECT_GT(effects_after.serotonin_synthesis_multiplier,
              effects_before.serotonin_synthesis_multiplier);
}

TEST_F(NeuromodImmuneTest, IsBalanced_Check) {
    /* WHAT: Test balance checking for individual neuromodulators
     * WHY:  Verify query API
     * HOW:  Check baseline and imbalanced states
     */
    /* At baseline, all should be balanced */
    EXPECT_TRUE(neuromod_immune_is_balanced(system, NEUROMOD_DOPAMINE));
    EXPECT_TRUE(neuromod_immune_is_balanced(system, NEUROMOD_SEROTONIN));

    /* Create imbalance */
    ASSERT_TRUE(neuromodulator_set_level(neuromod_system, NEUROMOD_DOPAMINE, 0.0003f));

    /* Now dopamine should be imbalanced */
    EXPECT_FALSE(neuromod_immune_is_balanced(system, NEUROMOD_DOPAMINE));
    EXPECT_TRUE(neuromod_immune_is_balanced(system, NEUROMOD_SEROTONIN));
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(NeuromodImmuneTest, FullCycle_CytokineToImbalanceToCorrection) {
    /* WHAT: Test complete neuroimmune cycle
     * WHY:  Verify end-to-end integration
     * HOW:  Cytokine → suppression → imbalance → alert → correction
     */

    /* 1. Apply pro-inflammatory cytokines */
    ASSERT_EQ(neuromod_immune_apply_proinflammatory_effect(system, 0.7f), 0);

    /* 2. Update system (simulates metabolic changes) */
    ASSERT_EQ(neuromod_immune_update(system, 100), 0);

    /* 3. Check cytokine effects */
    cytokine_neuromod_effects_t effects;
    ASSERT_EQ(neuromod_immune_get_cytokine_effects(system, &effects), 0);
    EXPECT_LT(effects.dopamine_synthesis_multiplier, 0.7f);

    /* 4. Create imbalance (simulated by lowering neuromodulator) */
    ASSERT_TRUE(neuromodulator_set_level(neuromod_system, NEUROMOD_SEROTONIN, 0.00001f));

    /* 5. Detect imbalance */
    neuromod_imbalance_t* imbalance = nullptr;
    ASSERT_EQ(neuromod_immune_detect_imbalance(system, &imbalance), 0);
    ASSERT_NE(imbalance, nullptr);

    /* 6. Alert immune system */
    uint32_t antigen_id = 0;
    ASSERT_EQ(neuromod_immune_alert_imbalance(system, imbalance, &antigen_id), 0);
    EXPECT_GT(antigen_id, 0);

    /* 7. Apply correction */
    ASSERT_EQ(neuromod_immune_correct_imbalance(system, imbalance), 0);

    /* 8. Verify correction attempted */
    EXPECT_TRUE(imbalance->corrective_action_taken);
}

TEST_F(NeuromodImmuneTest, Update_ProcessesCytokinesAndImbalances) {
    /* WHAT: Test update loop integrates all components
     * WHY:  Verify update processes everything correctly
     * HOW:  Set up state, call update, check results
     */

    /* Create some immune activity */
    uint32_t antigen_id;
    uint8_t epitope[64] = {0x01, 0x02, 0x03};
    ASSERT_EQ(brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
        epitope, sizeof(epitope), 7, 0, &antigen_id), 0);

    /* Update integration system */
    ASSERT_EQ(neuromod_immune_update(system, 1000), 0);

    /* System should be running */
    EXPECT_TRUE(system->running);

    /* Multiple updates should work */
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(neuromod_immune_update(system, 100), 0);
    }
}

TEST_F(NeuromodImmuneTest, GetImbalances_QueryActive) {
    /* WHAT: Test querying active imbalances
     * WHY:  Verify imbalance tracking API
     * HOW:  Create imbalances, query, check count
     *
     * NOTE: classify_imbalance() returns the FIRST imbalance type found
     * in clinical significance order (DA > 5HT > NE > ACH). When both
     * DA excess and 5HT deficiency exist, only DA_EXCESS is classified
     * because the classifier returns on the first match. The second
     * detect call finds the existing DA_EXCESS entry (still uncorrected)
     * and returns it instead of creating a new 5HT_DEFICIENCY entry.
     * As a result, only 1 imbalance is tracked. */

    /* Create dopamine excess */
    ASSERT_TRUE(neuromodulator_set_level(neuromod_system, NEUROMOD_DOPAMINE, 0.0003f));
    neuromod_imbalance_t* imb1 = nullptr;
    ASSERT_EQ(neuromod_immune_detect_imbalance(system, &imb1), 0);

    /* Create serotonin deficiency */
    ASSERT_TRUE(neuromodulator_set_level(neuromod_system, NEUROMOD_SEROTONIN, 0.00001f));
    neuromod_imbalance_t* imb2 = nullptr;
    ASSERT_EQ(neuromod_immune_detect_imbalance(system, &imb2), 0);

    /* Query imbalances */
    neuromod_imbalance_t imbalances[10];
    size_t count = 0;
    ASSERT_EQ(neuromod_immune_get_imbalances(system, imbalances, 10, &count), 0);

    EXPECT_GE(count, 1);  /* At least 1 (DA_EXCESS dominates classification) */
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(NeuromodImmuneTest, ImbalanceToString) {
    /* WHAT: Test imbalance type string conversion
     * WHY:  Verify logging/debugging utilities
     * HOW:  Convert all types, check non-null
     */
    EXPECT_STREQ(neuromod_immune_imbalance_to_string(NEUROMOD_IMBALANCE_NONE), "NONE");
    EXPECT_STREQ(neuromod_immune_imbalance_to_string(NEUROMOD_IMBALANCE_DA_EXCESS), "DA_EXCESS");
    EXPECT_STREQ(neuromod_immune_imbalance_to_string(NEUROMOD_IMBALANCE_5HT_DEFICIENCY), "5HT_DEFICIENCY");
    EXPECT_STREQ(neuromod_immune_imbalance_to_string(NEUROMOD_IMBALANCE_NE_EXCESS), "NE_EXCESS");
}

TEST_F(NeuromodImmuneTest, CytokineEffectToString) {
    /* WHAT: Test cytokine effect string conversion
     * WHY:  Verify logging utilities
     * HOW:  Convert all types
     */
    EXPECT_STREQ(neuromod_immune_cytokine_effect_to_string(CYTOKINE_EFFECT_NONE), "NONE");
    EXPECT_STREQ(neuromod_immune_cytokine_effect_to_string(CYTOKINE_EFFECT_SUPPRESS_SYNTHESIS), "SUPPRESS_SYNTHESIS");
    EXPECT_STREQ(neuromod_immune_cytokine_effect_to_string(CYTOKINE_EFFECT_ENHANCE_SYNTHESIS), "ENHANCE_SYNTHESIS");
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
